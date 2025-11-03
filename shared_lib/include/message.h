#ifndef MESSAGE_H
#define MESSAGE_H

#include <QString>
#include <QJsonObject>
#include <QByteArray>
#include "vta_subprocess_base_global.h"  

/**
 * @brief IPC通信消息类型（与主进程一致）
 */
enum class MessageType {
    kHello = 0,
    kHelloAck,
    kHeartbeat,
    kHeartbeatAck,
    kConfigUpdate,
    kCommand,
    kCommandResponse,
    kStatusReport,
    kLogMessage,
    kErrorReport,
    kShutdown
};

/**
 * @brief IPC连接状态（与主进程一致）
 */
enum class ConnectionState {
    kDisconnected = 0,
    kConnecting,
    kConnected,
    kInitialized,
    kAuthenticated,
    kError
};

// 结构体声明添加导出宏
struct VTA_SUBPROCESS_BASE_EXPORT IpcMessage {
    MessageType type;
    QString topic;
    QString msg_id;
    qint64 timestamp;
    QString sender_id;
    QString receiver_id;
    QJsonObject body;

    VTA_SUBPROCESS_BASE_EXPORT QJsonObject ToJson() const;
    VTA_SUBPROCESS_BASE_EXPORT static IpcMessage FromJson(const QJsonObject& json);

    VTA_SUBPROCESS_BASE_EXPORT QByteArray ToByteArray() const;
    VTA_SUBPROCESS_BASE_EXPORT static IpcMessage FromByteArray(const QByteArray& data);
};

// 全局函数添加导出宏
VTA_SUBPROCESS_BASE_EXPORT QString MessageTypeToString(MessageType type);
VTA_SUBPROCESS_BASE_EXPORT QString ConnectionStateToString(ConnectionState state);

// 兼容命名（架构文档中的 Message）
using Message = IpcMessage;

#endif // MESSAGE_H
