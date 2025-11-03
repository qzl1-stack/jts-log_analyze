#include "tcpclient.h"
#include <QDataStream>
#include <QNetworkInterface>

TcpClient::TcpClient(QObject *parent)
    : QObject(parent)
    , m_socket(nullptr)
    , m_serverHost("127.0.0.1")  // 默认本地地址
    , m_serverPort(8002)         // 默认端口8002
    , m_reconnectTimer(nullptr)
    , m_autoReconnect(false)
    , m_expectedBytes(0)
{
    // 创建TCP套接字
    m_socket = new QTcpSocket(this);
    
    // 连接信号和槽
    connect(m_socket, &QTcpSocket::connected, this, &TcpClient::onConnected);
    connect(m_socket, &QTcpSocket::disconnected, this, &TcpClient::onDisconnected);
    connect(m_socket, QOverload<QAbstractSocket::SocketError>::of(&QAbstractSocket::errorOccurred),
            this, &TcpClient::onSocketError);
    connect(m_socket, &QTcpSocket::bytesWritten, this, &TcpClient::onBytesWritten);
    
    // 创建重连定时器
    m_reconnectTimer = new QTimer(this);
    m_reconnectTimer->setSingleShot(true);
    m_reconnectTimer->setInterval(3000); // 3秒后重连
    connect(m_reconnectTimer, &QTimer::timeout, this, &TcpClient::connectToServer);
    
    qDebug() << "TcpClient: TCP客户端已初始化";
}

TcpClient::~TcpClient()
{
    if (m_socket && m_socket->state() != QAbstractSocket::UnconnectedState) {
        m_socket->disconnectFromHost();
        if (m_socket->state() != QAbstractSocket::UnconnectedState) {
            m_socket->waitForDisconnected(3000);
        }
    }
    qDebug() << "TcpClient: TCP客户端已销毁";
}

void TcpClient::setServerAddress(const QString& host, quint16 port)
{
    m_serverHost = host;
    m_serverPort = port;
    qDebug() << "TcpClient: 设置服务器地址" << host << ":" << port;
}

bool TcpClient::connectToServer()
{
    if (m_socket->state() == QAbstractSocket::ConnectedState) {
        qDebug() << "TcpClient: 已经连接到服务器";
        return true;
    }
    
    if (m_socket->state() == QAbstractSocket::ConnectingState) {
        qDebug() << "TcpClient: 正在连接中...";
        return true;
    }
    
    qDebug() << "TcpClient: 正在连接到服务器" << m_serverHost << ":" << m_serverPort;
    m_socket->connectToHost(m_serverHost, m_serverPort);
    
    return true;
}

void TcpClient::disconnectFromServer()
{
    m_autoReconnect = false;
    m_reconnectTimer->stop();
    
    if (m_socket->state() != QAbstractSocket::UnconnectedState) {
        qDebug() << "TcpClient: 断开与服务器的连接";
        m_socket->disconnectFromHost();
    }
}

bool TcpClient::sendTriggerBlackBoxCommand()
{
    if (m_socket->state() != QAbstractSocket::ConnectedState) {
        QString error = "TcpClient: 未连接到服务器，无法发送数据";
        qWarning() << error;
        emit errorOccurred(error);
        
        // 尝试重新连接
        connectToServer();
        return false;
    }
    
    // 构建数据帧
    QByteArray frame = buildTriggerBlackBoxFrame();
    
    // 发送数据
    qint64 bytesWritten = m_socket->write(frame);
    if (bytesWritten == -1) {
        QString error = "TcpClient: 数据发送失败: " + m_socket->errorString();
        qWarning() << error;
        emit errorOccurred(error);
        emit dataSent(false, error);
        return false;
    }
    
    m_expectedBytes = frame.size();
    qDebug() << "TcpClient: 发送触发黑盒子指令，数据长度:" << bytesWritten << "字节";
    qDebug() << "TcpClient: 发送的数据帧(十六进制):" << frame.toHex(' ').toUpper();
    
    return true;
}

bool TcpClient::isConnected() const
{
    qDebug() << "TcpClient: 检查连接状态" << m_socket->state();
    return m_socket && m_socket->state() == QAbstractSocket::ConnectedState;
}

void TcpClient::onConnected()
{
    qDebug() << "TcpClient: 成功连接到服务器" << m_serverHost << ":" << m_serverPort;
    m_reconnectTimer->stop();
    emit connectionStateChanged(true);
}

void TcpClient::onDisconnected()
{
    qDebug() << "TcpClient: 与服务器断开连接";
    emit connectionStateChanged(false);
    
    if (m_autoReconnect) {
        qDebug() << "TcpClient: 3秒后尝试重新连接...";
        m_reconnectTimer->start();
    }
}

void TcpClient::onSocketError(QAbstractSocket::SocketError error)
{
    QString errorString = m_socket->errorString();
    QString errorMsg = QString("TcpClient: 套接字错误 (%1): %2").arg(error).arg(errorString);
    
    qWarning() << errorMsg;
    emit errorOccurred(errorMsg);
    
    // 某些错误情况下尝试重连
    if (error == QAbstractSocket::RemoteHostClosedError ||
        error == QAbstractSocket::NetworkError ||
        error == QAbstractSocket::ConnectionRefusedError) {
        
        if (m_autoReconnect) {
            qDebug() << "TcpClient: 3秒后尝试重新连接...";
            m_reconnectTimer->start();
        }
    }
}

void TcpClient::onBytesWritten(qint64 bytes)
{
    qDebug() << "TcpClient: 成功发送" << bytes << "字节数据";
    
    if (bytes == m_expectedBytes) {
        emit dataSent(true, "触发黑盒子指令发送成功");
        m_expectedBytes = 0;
    }
}

quint16 TcpClient::calculateCRC(const QByteArray& data)
{
    // 简单的CRC16校验算法（可根据实际需求调整）
    quint16 crc = 0xFFFF;
    
    for (int i = 0; i < data.size(); ++i) {
        crc ^= static_cast<quint8>(data[i]);
        for (int j = 0; j < 8; ++j) {
            if (crc & 0x0001) {
                crc = (crc >> 1) ^ 0xA001;
            } else {
                crc >>= 1;
            }
        }
    }
    
    return crc;
}

QByteArray TcpClient::buildTriggerBlackBoxFrame()
{
    QByteArray frame;
    QDataStream stream(&frame, QIODevice::WriteOnly);
    stream.setByteOrder(QDataStream::LittleEndian); // 设置字节序
    
    // 构建有效数据部分
    QByteArray payload;
    QDataStream payloadStream(&payload, QIODevice::WriteOnly);
    payloadStream.setByteOrder(QDataStream::LittleEndian);
    
    // 有效数据：指令码(0x38) + 保留(0) + AGV地址(0) + 功能码(1)
    payloadStream << static_cast<quint8>(0x38);  // 指令码
    payloadStream << static_cast<quint8>(0);     // 保留
    payloadStream << static_cast<quint8>(0);     // AGV地址
    payloadStream << static_cast<quint16>(1);    // 功能码：触发黑盒子
    
    // 计算数据长度
    quint16 dataLength = static_cast<quint16>(payload.size());
    
    // 计算校验码：数据长度 + 有效数据的CRC校验值
    QByteArray crcData;
    QDataStream crcStream(&crcData, QIODevice::WriteOnly);
    crcStream.setByteOrder(QDataStream::LittleEndian);
    crcStream << dataLength;
    crcData.append(payload);

    quint16 checksum = calculateCRC(crcData);

    // 构建完整数据帧
    // 起始码：0x4A53 ("JS") - 大端存储
    stream.setByteOrder(QDataStream::BigEndian); // 临时切换为大端
    stream << static_cast<quint16>(0x4A53);
    stream.setByteOrder(QDataStream::LittleEndian); // 恢复为小端

    // 数据长度
    stream << dataLength;

    // 有效数据
    stream.writeRawData(payload.data(), payload.size());
    
    // 校验码
    stream << checksum;
    
    qDebug() << "TcpClient: 构建数据帧完成";
    qDebug() << "  - 起始码: 0x4A53";
    qDebug() << "  - 数据长度:" << dataLength;
    qDebug() << "  - 有效数据:" << payload.toHex(' ').toUpper();
    qDebug() << "  - 校验码:" << QString("0x%1").arg(checksum, 4, 16, QChar('0')).toUpper();
    qDebug() << "  - 完整帧:" << frame.toHex(' ').toUpper();
    
    return frame;
}