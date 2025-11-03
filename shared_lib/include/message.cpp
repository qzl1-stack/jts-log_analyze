#include "message.h"
#include <QJsonDocument>
#include <QJsonParseError>
#include <QDebug>

QJsonObject IpcMessage::ToJson() const
{
    QJsonObject json;
    json["type"] = static_cast<int>(type);
    json["topic"] = topic;
    json["msg_id"] = msg_id;
    json["timestamp"] = timestamp;
    json["sender_id"] = sender_id;
    json["receiver_id"] = receiver_id;
    json["body"] = body;
    return json;
}

IpcMessage IpcMessage::FromJson(const QJsonObject& json)
{
    IpcMessage message;
    message.type = static_cast<MessageType>(json["type"].toInt());
    message.topic = json["topic"].toString();
    message.msg_id = json["msg_id"].toString();
    message.timestamp = static_cast<qint64>(json["timestamp"].toDouble());
    message.sender_id = json["sender_id"].toString();
    message.receiver_id = json["receiver_id"].toString();
    message.body = json["body"].toObject();
    return message;
}

QByteArray IpcMessage::ToByteArray() const
{
    QJsonDocument doc(ToJson());
    return doc.toJson(QJsonDocument::Compact);
}

IpcMessage IpcMessage::FromByteArray(const QByteArray& data)
{
    QJsonParseError error;
    QJsonDocument doc = QJsonDocument::fromJson(data, &error);
    if (error.error != QJsonParseError::NoError) {
        qWarning() << "Failed to parse IpcMessage:" << error.errorString();
        return IpcMessage{};
    }
    return FromJson(doc.object());
}

QString MessageTypeToString(MessageType type)
{
    switch (type) {
    case MessageType::kHello: return "HELLO";
    case MessageType::kHelloAck: return "HELLO_ACK";
    case MessageType::kHeartbeat: return "HEARTBEAT";
    case MessageType::kHeartbeatAck: return "HEARTBEAT_ACK";
    case MessageType::kConfigUpdate: return "CONFIG_UPDATE";
    case MessageType::kCommand: return "COMMAND";
    case MessageType::kCommandResponse: return "COMMAND_RESPONSE";
    case MessageType::kStatusReport: return "STATUS_REPORT";
    case MessageType::kLogMessage: return "LOG_MESSAGE";
    case MessageType::kErrorReport: return "ERROR_REPORT";
    case MessageType::kShutdown: return "SHUTDOWN";
    default: return "UNKNOWN";
    }
}

QString ConnectionStateToString(ConnectionState state)
{
    switch (state) {
    case ConnectionState::kDisconnected: return "DISCONNECTED";
    case ConnectionState::kConnecting: return "CONNECTING";
    case ConnectionState::kConnected: return "CONNECTED";
    case ConnectionState::kInitialized: return "INITIALIZED";
    case ConnectionState::kAuthenticated: return "AUTHENTICATED";
    case ConnectionState::kError: return "ERROR";
    default: return "UNKNOWN";
    }
}
