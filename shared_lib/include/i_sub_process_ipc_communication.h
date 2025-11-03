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
        : QObject(parent) {}
    virtual ~ISubProcessIpcCommunication() = default;

    virtual bool Initialize(const QJsonObject& config, const QString& sub_process_id) = 0;
    virtual bool Start() = 0;
    virtual void Stop() = 0;

    virtual ConnectionState GetConnectionState() const = 0;

    virtual bool SendMessage(const IpcMessage& message) = 0;
    virtual bool PublishToTopic(const QString& topic,
                                const IpcMessage& message) = 0;

    virtual bool SubscribeToTopic(const QString& topic) = 0;
    virtual bool UnsubscribeFromTopic(const QString& topic) = 0;
    virtual QStringList GetSubscribedTopics() const = 0;

signals:
    void MessageReceived(const IpcMessage& message);
    void ConnectionStateChanged(ConnectionState state);
    void ErrorOccurred(const QString& error_message);
    void TopicSubscriptionChanged(const QString& topic, bool subscribed);

protected:
    /**
     * @brief 发送 HELLO 消息
     * 
     * 子类应该在连接成功后调用此函数
     */
    virtual void SendHelloMessage()
    {
        IpcMessage hello_message;
        hello_message.type = MessageType::kHello;
        hello_message.topic = "registration";
        hello_message.msg_id = QUuid::createUuid().toString(QUuid::WithoutBraces);
        hello_message.timestamp = QDateTime::currentMSecsSinceEpoch();
        hello_message.sender_id = "log_agent";
        hello_message.receiver_id = "main_process";

        QJsonObject body;
        body["version"] = "1.0.0";
        body["process_name"] = "log_agent";
        hello_message.body = body;
        // 调用由子类实现的SendMessage方法
        SendMessage(hello_message);
    }
    
    /**
     * @brief 初始化通用定时器（心跳和重连）
     * 
     * 子类应该在 Initialize 方法中调用此函数
     * @return true 如果成功
     */
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

    /**
     * @brief 启动重连定时器
     */
    void StartReconnectTimer()
    {
        if (reconnect_timer_) {
            reconnect_timer_->start();
        }
    }

    /**
     * @brief 停止重连定时器
     */
    void StopReconnectTimer()
    {
        if (reconnect_timer_) {
            reconnect_timer_->stop();
        }
    }

    /**
     * @brief 启动心跳定时器
     */
    void StartHeartbeatTimer()
    {
        if (heartbeat_timer_) {
            heartbeat_timer_->start();
        }
    }

    /**
     * @brief 停止心跳定时器
     */
    void StopHeartbeatTimer()
    {
        if (heartbeat_timer_) {
            heartbeat_timer_->stop();
        }
    }

protected slots:
    // 默认实现的重连和心跳处理
    virtual void OnReconnectTimer() = 0; 
    virtual void OnHeartbeatTimer() = 0;  

protected:
    // 定时器
    std::unique_ptr<QTimer> reconnect_timer_;
    std::unique_ptr<QTimer> heartbeat_timer_;
    int reconnect_interval_ms_{5000};   // 默认5秒重连
    int heartbeat_interval_ms_{10000};  // 默认10秒心跳

    // 子进程信息
    QString sub_process_id_;
};
#endif // I_SUB_PROCESS_IPC_COMMUNICATION_H
