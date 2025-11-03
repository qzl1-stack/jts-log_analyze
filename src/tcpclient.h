#ifndef TCPCLIENT_H
#define TCPCLIENT_H

#include <QObject>
#include <QTcpSocket>
#include <QHostAddress>
#include <QTimer>
#include <QByteArray>
#include <QDebug>

/**
 * @brief TCP客户端类，用于向黑盒子服务器发送触发指令
 * 
 * 协议格式：
 * - 起始码：0x4A53 ("JS")
 * - 数据长度：有效数据字节数
 * - 有效数据：指令码(0x38) + 保留(0) + AGV地址(0) + 功能码(1)
 * - 校验码：数据长度 + 有效数据的CRC校验值
 */
class TcpClient : public QObject
{
    Q_OBJECT

public:
    explicit TcpClient(QObject *parent = nullptr);
    ~TcpClient();

    /**
     * @brief 设置服务器地址和端口
     * @param host 服务器IP地址
     * @param port 服务器端口号（默认8002）
     */
    void setServerAddress(const QString& host, quint16 port = 8002);

    /**
     * @brief 连接到服务器
     * @return 是否成功开始连接
     */
    bool connectToServer();

    /**
     * @brief 断开与服务器的连接
     */
    void disconnectFromServer();

    /**
     * @brief 发送触发黑盒子指令
     * @return 是否成功发送
     */
    bool sendTriggerBlackBoxCommand();

    /**
     * @brief 获取连接状态
     * @return 是否已连接
     */
    bool isConnected() const;

signals:
    /**
     * @brief 连接状态改变信号
     * @param connected 是否已连接
     */
    void connectionStateChanged(bool connected);

    /**
     * @brief 数据发送完成信号
     * @param success 是否发送成功
     * @param message 状态消息
     */
    void dataSent(bool success, const QString& message);

    /**
     * @brief 错误信号
     * @param error 错误信息
     */
    void errorOccurred(const QString& error);

private slots:
    /**
     * @brief TCP连接成功槽函数
     */
    void onConnected();

    /**
     * @brief TCP连接断开槽函数
     */
    void onDisconnected();

    /**
     * @brief TCP错误处理槽函数
     * @param error 套接字错误
     */
    void onSocketError(QAbstractSocket::SocketError error);

    /**
     * @brief 数据写入完成槽函数
     * @param bytes 写入的字节数
     */
    void onBytesWritten(qint64 bytes);

private:
    /**
     * @brief 计算CRC校验码
     * @param data 需要校验的数据
     * @return CRC校验值
     */
    quint16 calculateCRC(const QByteArray& data);

    /**
     * @brief 构建触发黑盒子数据帧
     * @return 完整的数据帧
     */
    QByteArray buildTriggerBlackBoxFrame();

private:
    QTcpSocket* m_socket;           // TCP套接字
    QString m_serverHost;           // 服务器地址
    quint16 m_serverPort;           // 服务器端口
    QTimer* m_reconnectTimer;       // 重连定时器
    bool m_autoReconnect;           // 是否自动重连
    qint64 m_expectedBytes;         // 期望发送的字节数
};

#endif // TCPCLIENT_H