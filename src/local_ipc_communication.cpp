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
    // max_queue_size_ initialized in base class
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
    // 尝试连接
    return Connect();
}

void LocalIpcCommunication::Stop()
{
    Disconnect();
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
    StopReconnectTimer();
    SendHelloMessage(); //调用基类实现
    SendQueuedMessages();
    StartHeartbeatTimer();
    emit ConnectionEstablished();
}

void LocalIpcCommunication::OnSocketDisconnected()
{
    qDebug() << "Socket disconnected from server";
    UpdateConnectionState(ConnectionState::kDisconnected);
    emit ConnectionLost();
}

void LocalIpcCommunication::OnSocketError(QLocalSocket::LocalSocketError error)
{
    qWarning() << "Socket error:" << error << socket_->errorString();
    UpdateConnectionState(ConnectionState::kError);
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


void LocalIpcCommunication::UpdateConnectionState(ConnectionState new_state)
{
    QMutexLocker locker(&state_mutex_); // 新增：在函数内部加锁
    if (connection_state_ != new_state) {
        connection_state_ = new_state;
        emit ConnectionStateChanged(new_state);
        qDebug() << "Connection state changed to:" << ConnectionStateToString(new_state);
    }
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


void LocalIpcCommunication::OnReconnectTimer()
{
    // 如果未连接，尝试重新连接
    if (GetConnectionState() != ConnectionState::kConnected) {
        qDebug() << "Attempting to reconnect to server:" << server_name_;
        Connect();
    }
}
