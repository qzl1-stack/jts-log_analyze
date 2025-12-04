#ifndef LOCAL_IPC_COMMUNICATION_H
#define LOCAL_IPC_COMMUNICATION_H

#include "i_sub_process_ipc_communication.h"
#include <QLocalSocket>
#include <QTimer>
#include <QMutex>
#include <QQueue>
#include <memory>
#include <QMetaObject> 

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
    bool Initialize(const QJsonObject& config) override;
    bool Start() override;
    void Stop() override;
    
    ConnectionState GetConnectionState() const override;
    
    // LocalSocket特有的方法
    bool Connect();
    void Disconnect();
    bool IsConnected() const;

protected:
    QString GetSenderId() override
    {
        return "AGV分析";
    }

    QString GetProcessName() override
    {
        return "AGV分析";
    }

    QString GetProcessVersion() override
    {
        return "1.0.0";
    }

signals:
    void ConnectionEstablished();
    void ConnectionLost();

protected slots:
    void OnSocketConnected();
    void OnSocketDisconnected();
    void OnSocketError(QLocalSocket::LocalSocketError error);
    void OnSocketReadyRead();
    
    // 默认实现的重连
    void OnReconnectTimer() override; 
    
protected:
    // 实现基类的纯虚函数
    qint64 WriteData(const QByteArray& data) override{
        qint64 bytes_written = socket_->write(data);
        return bytes_written;
    }

private:
    // 初始化方法
    bool InitializeSocket();
    
    // 消息处理方法
    void ProcessIncomingData();
    void ProcessCompleteMessage(const QByteArray& message_data);
    
    void UpdateConnectionState(ConnectionState new_state);
    
    // 工具方法
    IpcMessage ParseReceivedMessage(const QByteArray& data);
    QString GenerateMessageId() const;

private:
    // 网络连接
    std::unique_ptr<QLocalSocket> socket_;
    QString server_name_;
    
    // 连接状态
    ConnectionState connection_state_;
    mutable QMutex state_mutex_;
    
    // 数据接收缓冲区
    QByteArray receive_buffer_;
    
    // 配置参数
    int connection_timeout_ms_;
    bool auto_reconnect_enabled_;
    
    // 统计信息
    qint64 messages_sent_;
    qint64 messages_received_;
    qint64 connection_attempts_;
    QDateTime last_heartbeat_time_;
};

#endif // LOCAL_IPC_COMMUNICATION_H
