#include "local_ipc_communication.h"
#include <QDebug>
#include <QJsonDocument>
#include <QJsonObject>
#include <QUuid>
#include <QDateTime>
#include <QMutexLocker>
#include <QHostAddress>

LocalIpcCommunication::LocalIpcCommunication(QObject* parent)
    : ISubProcessIpcCommunication(parent)
    , socket_(nullptr)
    , server_name_("log_analyzer_socket")
    , connection_state_(ConnectionState::kDisconnected)
    , max_queue_size_(1000)
    , connection_timeout_ms_(10000)     // 10秒连接超时
    , auto_reconnect_enabled_(true)
    , messages_sent_(0)
    , messages_received_(0)
    , connection_attempts_(0)
{
    qDebug() << "LocalIpcCommunication created";
}

LocalIpcCommunication::~LocalIpcCommunication()
{
    qDebug() << "LocalIpcCommunication destroyed";
}

bool LocalIpcCommunication::Initialize(const QJsonObject& config)
{
    qDebug() << "Initializing LocalIpcCommunication with config:" << config;
    

    // 加载服务器配置
    if (config.contains("server_name")) {
        server_name_ = config["server_name"].toString();
    }
    
    // 加载定时器配置
    reconnect_interval_ms_ = config.value("reconnect_interval_ms").toInt(reconnect_interval_ms_);
    heartbeat_interval_ms_ = config.value("heartbeat_interval_ms").toInt(heartbeat_interval_ms_);
    
    // 加载其他配置
    max_queue_size_ = config.value("max_queue_size").toInt(max_queue_size_);
    connection_timeout_ms_ = config.value("connection_timeout_ms").toInt(connection_timeout_ms_);
    auto_reconnect_enabled_ = config.value("auto_reconnect_enabled").toBool(auto_reconnect_enabled_);
    
    // 初始化定时器
    if (!InitializeTimers()) {
        qWarning() << "Failed to initialize socket";
        return false;
    }

    // 其他初始化逻辑
    if (!InitializeSocket()) {
        return false;
    }
    
    qDebug() << "LocalIpcCommunication initialized successfully";
    return true;
}

bool LocalIpcCommunication::Start()
{
    // 启动重连和心跳定时器
    StartReconnectTimer();
    // StartHeartbeatTimer();

    // 尝试连接
    return Connect();
}

void LocalIpcCommunication::Stop()
{
    Disconnect();
}

bool LocalIpcCommunication::PublishToTopic(const QString& topic, const IpcMessage& message)
{
    // For local IPC, we'll just send the message directly to the main process
    // as if it were a direct message. The main process will handle topic routing.
    IpcMessage topic_message = message;
    topic_message.topic = topic; // Ensure topic is set in the message
    // return SendMessage(topic_message);
}

bool LocalIpcCommunication::SubscribeToTopic(const QString& topic)
{
    QMutexLocker locker(&subscription_mutex_);
    if (!subscribed_topics_.contains(topic)) {
        subscribed_topics_.append(topic);
        qDebug() << "Subscribed to topic:" << topic;
        emit TopicSubscriptionChanged(topic, true);
        return true;
    }
    return false;
}

bool LocalIpcCommunication::UnsubscribeFromTopic(const QString& topic)
{
    QMutexLocker locker(&subscription_mutex_);
    if (subscribed_topics_.removeAll(topic) > 0) {
        qDebug() << "Unsubscribed from topic:" << topic;
        emit TopicSubscriptionChanged(topic, false);
        return true;
    }
    return false;
}

QStringList LocalIpcCommunication::GetSubscribedTopics() const
{
    // QMutexLocker locker(&subscription_mutex_);
    return subscribed_topics_;
}

bool LocalIpcCommunication::Connect()
{
    // QMutexLocker locker(&state_mutex_);
    
    if (connection_state_ == ConnectionState::kConnected) {
        qDebug() << "Already connected";
        return true;
    }
    
    if (connection_state_ == ConnectionState::kConnecting) {
        qDebug() << "Connection already in progress";
        return false;
    }
    
    // UpdateConnectionState(ConnectionState::kConnecting);
    connection_attempts_++;
    
    qDebug() << "Connecting to server:" << server_name_;
    
    socket_->connectToServer(server_name_);
    
    return true;
}

void LocalIpcCommunication::Disconnect()
{
    // 移除外层对 state_mutex_ 的加锁，避免与 UpdateConnectionState 内部加锁产生死锁
    qDebug() << "Disconnecting from server";
    
    // 停止定时器
    StopReconnectTimer();
    StopHeartbeatTimer();
    
    // 关闭Socket连接
    if (socket_ && socket_->state() != QLocalSocket::UnconnectedState) {
        socket_->disconnectFromServer();
    }
    
    UpdateConnectionState(ConnectionState::kDisconnected);
    
    // 清空消息队列
    QMutexLocker queue_locker(&message_queue_mutex_);
    outgoing_message_queue_.clear();
}

bool LocalIpcCommunication::SendMessage(const IpcMessage& message)
{
    QMutexLocker queue_locker(&message_queue_mutex_);
    // 检查队列大小
    if (outgoing_message_queue_.size() >= max_queue_size_) {
        qWarning() << "Message queue full, dropping oldest message";
        outgoing_message_queue_.dequeue();
    }
    
    // 添加消息到队列
    outgoing_message_queue_.enqueue(message);
    qDebug() << "Message queued:" << MessageTypeToString(message.type);
    // qDebug() << "body:" << message.body;
    qDebug() << "sender_id:" << message.sender_id;

    // 如果已连接，立即发送
    if (IsConnected()) {
        // 移除立即发送逻辑，只负责入队
        queue_locker.unlock();
        SendQueuedMessages();
        return true;
    }

    qDebug() << "Message queued, will send when connected";
    return false;
}

bool LocalIpcCommunication::IsConnected() const
{
    QMutexLocker locker(&state_mutex_);
    return connection_state_ == ConnectionState::kConnected;
}

ConnectionState LocalIpcCommunication::GetConnectionState() const
{
    QMutexLocker locker(&state_mutex_);
    return connection_state_;
}



void LocalIpcCommunication::OnSocketConnected()
{
    qDebug() << "Socket connected to server";
    
    UpdateConnectionState(ConnectionState::kConnected);
     
    // 停止重连定时器
    StopReconnectTimer();
    
    // 发送HELLO消息进行握手
    SendHelloMessage();

    // 发送队列中的消息
    SendQueuedMessages();
    
    // 开始心跳
    StartHeartbeatTimer();
    
    // 发出连接成功信号
    emit ConnectionEstablished();
}

void LocalIpcCommunication::OnSocketDisconnected()
{
    qDebug() << "Socket disconnected from server";
    
    // 移除外层加锁，避免重复加锁
    UpdateConnectionState(ConnectionState::kDisconnected);
    
    // 如果启用自动重连，开始重连定时器
    if (auto_reconnect_enabled_) {
        StartReconnectTimer();
    }
    
    // 发出连接断开信号
    emit ConnectionLost();
}

void LocalIpcCommunication::OnSocketError(QLocalSocket::LocalSocketError error)
{
    qWarning() << "Socket error:" << error << socket_->errorString();
    
    UpdateConnectionState(ConnectionState::kError);
    
    // // 发出错误信号
    // emit ErrorOccurred(socket_->errorString());
    
    // 如果启用自动重连，开始重连定时器
    if (auto_reconnect_enabled_) {
        StartReconnectTimer();
    }
}

void LocalIpcCommunication::OnSocketReadyRead()
{
    // 读取所有可用数据
    QByteArray data = socket_->readAll();
    qDebug() << "OnSocketReadyRead bytes:" << data.size();
    receive_buffer_.append(data);
    
    // 处理接收到的数据
    ProcessIncomingData();
}


bool LocalIpcCommunication::InitializeSocket()
{
    socket_ = std::make_unique<QLocalSocket>(this);
    
    // 连接信号
    connect(socket_.get(), &QLocalSocket::connected,
            this, &LocalIpcCommunication::OnSocketConnected);
    connect(socket_.get(), &QLocalSocket::disconnected,
            this, &LocalIpcCommunication::OnSocketDisconnected);
    connect(socket_.get(), &QLocalSocket::errorOccurred,
            this, &LocalIpcCommunication::OnSocketError);
    connect(socket_.get(), &QLocalSocket::readyRead,
            this, &LocalIpcCommunication::OnSocketReadyRead);
    
    return true;
}

void LocalIpcCommunication::ProcessIncomingData()
{
    // 简单的消息分割协议：每个消息以换行符结束
    bool processed_any = false;
    while (receive_buffer_.contains('\n')) {
        int newline_pos = receive_buffer_.indexOf('\n');
        QByteArray message_data = receive_buffer_.left(newline_pos);
        receive_buffer_.remove(0, newline_pos + 1);
        
        if (!message_data.isEmpty()) {
            processed_any = true;
            ProcessCompleteMessage(message_data);
        }
    }

}

void LocalIpcCommunication::ProcessCompleteMessage(const QByteArray& message_data)
{
    try {
        IpcMessage message = ParseReceivedMessage(message_data);
        messages_received_++;
        
        qDebug() << "Message received:" << MessageTypeToString(message.type)
                 << "from" << message.sender_id;
        
        // 处理心跳回应
        if (message.type == MessageType::kHeartbeatAck) {
            last_heartbeat_time_ = QDateTime::currentDateTime();
            return;
        }
        
        // 发出消息接收信号
        emit MessageReceived(message);
        
    } catch (const std::exception& e) {
        qWarning() << "Failed to parse received message:" << e.what();
    }
}

void LocalIpcCommunication::SendQueuedMessages()
{
    // (1) 先获取 state_mutex_ 来检查连接状态
    bool is_connected_state;
    {
        QMutexLocker state_locker(&state_mutex_);
        is_connected_state = (connection_state_ == ConnectionState::kConnected);
    } // state_locker 在这里释放

    if (!is_connected_state) {
        qDebug() << "Not connected, stopping SendQueuedMessages.";
        return;
    }

    // (2) 然后获取 message_queue_mutex_ 来处理消息队列
    QMutexLocker queue_locker(&message_queue_mutex_);

    if (outgoing_message_queue_.isEmpty()) {
        return; // 队列为空，无需发送
    }

    IpcMessage message = outgoing_message_queue_.head(); // 获取头部消息，不立即移除

    queue_locker.unlock(); // 暂时解锁队列

    QByteArray message_data = PrepareMessageForTransmission(message);

    qint64 bytes_written = socket_->write(message_data);
    if (bytes_written != -1 && bytes_written == message_data.size()) {
        socket_->flush();
        messages_sent_++;
        qDebug() << "Message sent:" << MessageTypeToString(message.type)
                 << "to" << message.receiver_id;

        queue_locker.relock(); // 重新锁定，移除已发送消息
        outgoing_message_queue_.dequeue();

        if (!outgoing_message_queue_.isEmpty()) {
            QMetaObject::invokeMethod(this, "SendQueuedMessages", Qt::QueuedConnection);
        }
    } else {
        qWarning() << "Failed to send message: " << socket_->errorString();
    }
}


void LocalIpcCommunication::UpdateConnectionState(ConnectionState new_state)
{
    QMutexLocker locker(&state_mutex_); // 新增：在函数内部加锁
    if (connection_state_ != new_state) {
        connection_state_ = new_state;
        emit ConnectionStateChanged(new_state);
        qDebug() << "Connection state changed to:" << ConnectionStateToString(new_state);
    }
}


void LocalIpcCommunication::SendHeartbeatMessage()
{
    if (!IsConnected()) {
        return;
    }

    IpcMessage heartbeat;
    heartbeat.type = MessageType::kHeartbeat;
    heartbeat.topic = "heartbeat";
    heartbeat.msg_id = GenerateMessageId();
    heartbeat.timestamp = QDateTime::currentMSecsSinceEpoch();
    heartbeat.sender_id = "AGV分析";           // 必须与 MainController 启动的 process_id 一致
    heartbeat.receiver_id = "main_process";     

    QJsonObject body;
    body["process_state"] = "running";
    body["process_name"] = "AGV分析";
    body["timestamp"] = heartbeat.timestamp;
    heartbeat.body = body;

    // 使用统一的序列化方法
    QByteArray payload = PrepareMessageForTransmission(heartbeat);
    
    // 直接发送序列化后的数据（已包含换行符）
    const qint64 n = socket_->write(payload);
    socket_->flush();
    qDebug() << "heartbeat message:" << payload;
    qDebug() << "Heartbeat sent, bytes:" << n;
}

QByteArray LocalIpcCommunication::PrepareMessageForTransmission(const IpcMessage& message)
{
    QJsonDocument doc(message.ToJson());
    QByteArray data = doc.toJson(QJsonDocument::Compact);
    data.append('\n'); // 消息分隔符
    return data;
}

IpcMessage LocalIpcCommunication::ParseReceivedMessage(const QByteArray& data)
{
    QJsonParseError error;
    QJsonDocument doc = QJsonDocument::fromJson(data, &error);
    
    if (error.error != QJsonParseError::NoError) {
        throw std::runtime_error(QString("JSON parse error: %1").arg(error.errorString()).toStdString());
    }
    
    return IpcMessage::FromJson(doc.object());
}

QString LocalIpcCommunication::GenerateMessageId() const
{
    return QUuid::createUuid().toString(QUuid::WithoutBraces);
}

void LocalIpcCommunication::OnReconnectTimer()
{
    // 如果未连接，尝试重新连接
    if (GetConnectionState() != ConnectionState::kConnected) {
        qDebug() << "Attempting to reconnect to server:" << server_name_;
        Connect();
    }
}

void LocalIpcCommunication::OnHeartbeatTimer()
{
    // 如果已连接，发送心跳消息
    if (GetConnectionState() == ConnectionState::kConnected) {
        SendHeartbeatMessage(); // 这会调用 LocalIpcCommunication 自己的 SendHeartbeatMessage
    }
}
