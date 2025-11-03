#ifndef LOCAL_IPC_COMMUNICATION_H
#define LOCAL_IPC_COMMUNICATION_H

#include "i_sub_process_ipc_communication.h"
#include <QLocalSocket>
#include <QTimer>
#include <QMutex>
#include <QQueue>
#include <memory>
#include <QMetaObject> // 新增

/**
 * @brief TCP实现的IPC通信类
 * 
 * 使用TCP Socket与主进程进行通信
 * 支持自动重连、消息队列、心跳检测等功能
 */
class LocalIpcCommunication : public ISubProcessIpcCommunication
{
    Q_OBJECT

public:
    explicit LocalIpcCommunication(QObject* parent = nullptr);
    virtual ~LocalIpcCommunication();

    // ISubProcessIpcCommunication接口实现
    bool Initialize(const QJsonObject& config, const QString& sub_process_id) override;
    bool Start() override;
    void Stop() override;
    bool SendMessage(const IpcMessage& message) override;
    bool PublishToTopic(const QString& topic, const IpcMessage& message) override;
    bool SubscribeToTopic(const QString& topic) override;
    bool UnsubscribeFromTopic(const QString& topic) override;
    QStringList GetSubscribedTopics() const override;
    ConnectionState GetConnectionState() const override;
    
    // LocalSocket特有的方法
    bool Connect();
    void Disconnect();
    bool IsConnected() const;

    // LocalSocket特有的配置方法
    void SetServerName(const QString& name);

signals:
    void ConnectionEstablished();
    void ConnectionLost();

protected slots:
    void OnSocketConnected();
    void OnSocketDisconnected();
    void OnSocketError(QLocalSocket::LocalSocketError error);
    void OnSocketReadyRead();
    
    // 默认实现的重连和心跳处理
    void OnReconnectTimer() override; // 保持 override
    void OnHeartbeatTimer() override;   // 保持 override

private:
    // 初始化方法
    bool InitializeSocket();
    
    // 消息处理方法
    void ProcessIncomingData();
    void ProcessCompleteMessage(const QByteArray& message_data);
    void SendQueuedMessages();
    
    void UpdateConnectionState(ConnectionState new_state);
    
    void SendHeartbeatMessage();
    
    // 工具方法
    QByteArray PrepareMessageForTransmission(const IpcMessage& message);
    IpcMessage ParseReceivedMessage(const QByteArray& data);
    QString GenerateMessageId() const;

private:
    // 网络连接
    std::unique_ptr<QLocalSocket> socket_;
    QString server_name_;
    
    // 连接状态
    ConnectionState connection_state_;
    mutable QMutex state_mutex_;
    
    // 定时器 (已移至基类 ISubProcessIpcCommunication)
    
    // 消息队列
    QMutex message_queue_mutex_;
    QQueue<IpcMessage> outgoing_message_queue_;
    
    // Topic订阅管理
    QMutex subscription_mutex_;
    QStringList subscribed_topics_;
    
    // 数据接收缓冲区
    QByteArray receive_buffer_;
    
    // 配置参数
    int max_queue_size_;
    int connection_timeout_ms_;
    bool auto_reconnect_enabled_;
    
    // 统计信息
    qint64 messages_sent_;
    qint64 messages_received_;
    qint64 connection_attempts_;
    QDateTime last_heartbeat_time_;
};

#endif // LOCAL_IPC_COMMUNICATION_H
