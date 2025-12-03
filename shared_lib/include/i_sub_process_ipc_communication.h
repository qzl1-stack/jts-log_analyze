#ifndef I_SUB_PROCESS_IPC_COMMUNICATION_H
#define I_SUB_PROCESS_IPC_COMMUNICATION_H
#include <QObject>
#include <QString>
#include <QStringList>
#include <QJsonObject>
#include <memory>
#include <QTimer>
#include <QUuid>
#include <QDateTime>
#include <QQueue>
#include <QMutex>
#include <QMutexLocker>
#include <QJsonDocument>
#include "message.h"
#include "vta_subprocess_base_global.h"

/**
 * @brief 子进程侧 IPC 通信接口（与主进程兼容）
 *
 * 子进程通常作为客户端连接主进程，但接口保持与主进程
 * 兼容的核心能力：初始化、启动、停止、Topic 发布/订阅、
 * 状态查询与信号回调。
 */
class VTA_SUBPROCESS_BASE_EXPORT ISubProcessIpcCommunication : public QObject {
    Q_OBJECT

public:
    explicit ISubProcessIpcCommunication(QObject* parent = nullptr)
        : QObject(parent) 
    {
        max_queue_size_ = 1000; // 默认队列大小
    }
    virtual ~ISubProcessIpcCommunication() = default;

    virtual bool Initialize(const QJsonObject& config) = 0;
    virtual bool Start() = 0;
    virtual void Stop() = 0;

    virtual ConnectionState GetConnectionState() const = 0;

    /**
     * @brief 发送消息（通用实现）
     * 
     * 将消息加入队列，如果已连接则触发发送
     */
    virtual bool SendMessage(const IpcMessage& message)
    {
        QMutexLocker queue_locker(&message_queue_mutex_);
        // 检查队列大小
        if (outgoing_message_queue_.size() >= max_queue_size_) {
            // qWarning() << "Message queue full, dropping oldest message"; // 避免基类依赖过多日志
            outgoing_message_queue_.dequeue();
        }
        
        // 添加消息到队列
        outgoing_message_queue_.enqueue(message);

        // 如果已连接，立即发送
        if (GetConnectionState() == ConnectionState::kConnected) {
            queue_locker.unlock();
            // 使用 invokeMethod 异步调用以避免直接递归锁死风险
            QMetaObject::invokeMethod(this, "SendQueuedMessages", Qt::QueuedConnection);
            return true;
        }

        return false;
    }

    virtual bool PublishToTopic(const QString& topic,
                                const IpcMessage& message) = 0;

    virtual bool SubscribeToTopic(const QString& topic) = 0;
    virtual bool UnsubscribeFromTopic(const QString& topic) = 0;
    virtual QStringList GetSubscribedTopics() const = 0;

public slots:
    /**
     * @brief 发送队列中的消息
     * 
     * 这是一个通用流程：取出消息 -> 序列化 -> 调用纯虚函数 WriteData
     */
    virtual void SendQueuedMessages()
    {
        // (1) 检查连接状态
        if (GetConnectionState() != ConnectionState::kConnected) {
            return;
        }

        // (2) 处理消息队列
        QMutexLocker queue_locker(&message_queue_mutex_);

        if (outgoing_message_queue_.isEmpty()) {
            return; // 队列为空，无需发送
        }

        IpcMessage message = outgoing_message_queue_.head(); // 获取头部消息，不立即移除
        queue_locker.unlock(); // 暂时解锁队列以进行序列化和IO操作

        QByteArray message_data = PrepareMessageForTransmission(message);
        
        // 调用派生类实现的具体发送逻辑
        qint64 bytes_written = WriteData(message_data);

        if (bytes_written != -1 && bytes_written == message_data.size()) {
            queue_locker.relock(); // 重新锁定，移除已发送消息
            if (!outgoing_message_queue_.isEmpty()) {
                outgoing_message_queue_.dequeue();
            }

            if (!outgoing_message_queue_.isEmpty()) {
                queue_locker.unlock();
                // 继续发送下一条
                QMetaObject::invokeMethod(this, "SendQueuedMessages", Qt::QueuedConnection);
            }
        } 
        // 如果发送失败，消息保留在队列头部，等待下一次尝试
    }

signals:
    void MessageReceived(const IpcMessage& message);
    void ConnectionStateChanged(ConnectionState state);
    void ErrorOccurred(const QString& error_message);
    void TopicSubscriptionChanged(const QString& topic, bool subscribed);

protected:
    virtual QString GetSenderId() = 0;
    virtual QString GetProcessName() = 0;
    virtual QString GetProcessVersion() = 0;

    virtual IpcMessage CreateHelloMessage()
    {
        IpcMessage hello_message;
        hello_message.type = MessageType::kHello;
        hello_message.topic = "registration";
        hello_message.msg_id = QUuid::createUuid().toString(QUuid::WithoutBraces);
        hello_message.timestamp = QDateTime::currentMSecsSinceEpoch();
        hello_message.sender_id = GetSenderId();
        hello_message.receiver_id = "main_process";

        QJsonObject body;
        body["version"] = GetProcessVersion();
        body["process_name"] = GetProcessName();
        hello_message.body = body;

        return hello_message;
    }

    virtual void SendHelloMessage()
    {
        IpcMessage hello_message = CreateHelloMessage();
        // 调用由子类实现的SendMessage方法
        SendMessage(hello_message);
    }


    virtual IpcMessage CreateHeartbeatMessage()
    {
        IpcMessage heartbeat;
        heartbeat.type = MessageType::kHeartbeat;
        heartbeat.topic = "heartbeat";
        heartbeat.msg_id = QUuid::createUuid().toString(QUuid::WithoutBraces);
        heartbeat.timestamp = QDateTime::currentMSecsSinceEpoch();
        heartbeat.sender_id = GetSenderId();
        heartbeat.receiver_id = "main_process";

        QJsonObject body;
        body["process_state"] = "running";
        body["process_name"] = GetProcessName();
        body["timestamp"] = heartbeat.timestamp;
        heartbeat.body = body;

        return heartbeat;
    }

    virtual void SendHeartbeatMessage()
    {
        if (GetConnectionState() != ConnectionState::kConnected) {
            return;
        }

        IpcMessage heartbeat = CreateHeartbeatMessage();
        SendMessage(heartbeat);
    }
    
    bool InitializeTimers()
    {
        // 重连定时器
        reconnect_timer_ = std::make_unique<QTimer>(this);
        reconnect_timer_->setInterval(reconnect_interval_ms_);
        connect(reconnect_timer_.get(), &QTimer::timeout, this,
                &ISubProcessIpcCommunication::OnReconnectTimer);

        // 心跳定时器
        heartbeat_timer_ = std::make_unique<QTimer>(this);
        heartbeat_timer_->setInterval(heartbeat_interval_ms_);
        connect(heartbeat_timer_.get(), &QTimer::timeout, this,
                &ISubProcessIpcCommunication::OnHeartbeatTimer);

        return true;
    }

    void StartReconnectTimer()
    {
        if (reconnect_timer_) {
            reconnect_timer_->start();
        }
    }
    void StopReconnectTimer()
    {
        if (reconnect_timer_) {
            reconnect_timer_->stop();
        }
    }

    void StartHeartbeatTimer()
    {
        if (heartbeat_timer_) {
            heartbeat_timer_->start();
        }
    }

    void StopHeartbeatTimer()
    {
        if (heartbeat_timer_) {
            heartbeat_timer_->stop();
        }
    }

protected slots:
    // 默认实现的重连和心跳处理
    virtual void OnReconnectTimer() = 0; 
    
    virtual void OnHeartbeatTimer()
    {
        SendHeartbeatMessage();
    } 

    /**
     * @brief 序列化消息
     */
    virtual QByteArray PrepareMessageForTransmission(const IpcMessage& message)
    {
        QJsonDocument doc(message.ToJson());
        QByteArray data = doc.toJson(QJsonDocument::Compact);
        data.append('\n'); // 消息分隔符
        return data;
    }

    /**
     * @brief 写入数据到传输介质
     * 
     * 纯虚函数，由派生类实现具体的写入逻辑（如 socket write）
     * @param data 要发送的二进制数据
     * @return qint64 实际写入的字节数，-1 表示错误
     */
    virtual qint64 WriteData(const QByteArray& data) = 0;

protected:
    // 定时器
    std::unique_ptr<QTimer> reconnect_timer_;
    std::unique_ptr<QTimer> heartbeat_timer_;
    int reconnect_interval_ms_{5000};   // 默认5秒重连
    int heartbeat_interval_ms_{10000};  // 默认10秒心跳

    // 消息队列管理
    QMutex message_queue_mutex_;
    QQueue<IpcMessage> outgoing_message_queue_;
    int max_queue_size_;
};
#endif // I_SUB_PROCESS_IPC_COMMUNICATION_H
