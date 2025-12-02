#include "log_analyzer_subprocess.h"
#include <QDebug>
#include <QFile>
#include <QTextStream>
#include <QDir>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonArray>
#include <QRegularExpression>
#include <QUuid>
#include <QDateTime>
#include <QMutexLocker>
#include <QCoreApplication>
#include "sub_process_config_manager.h"

LogAnalyzerSubProcess::LogAnalyzerSubProcess(QObject* parent)
    : BaseSubProcess(parent)
    , heartbeat_interval_ms_(5000)      // 5秒心跳
{   
    // 创建并设置IPC通信
    ipc_communication_ = std::make_unique<LocalIpcCommunication>(this);
    SetIpc(ipc_communication_.get());
}

LogAnalyzerSubProcess::~LogAnalyzerSubProcess()
{
}

bool LogAnalyzerSubProcess::OnInitialize(const QJsonObject& config)
{
    Q_UNUSED(config)
    qDebug() << "Initializing LogAnalyzerSubProcess...";
    
    // 初始化IPC
    QJsonValue ipc_config_value = GetConfigManager()->GetValue("ipc");
    if (!ipc_config_value.isObject()) {
        qWarning() << "IPC configuration is missing or not an object.";
        return false;
    }
    
    if (!ipc_communication_->Initialize(ipc_config_value.toObject())) {
        qCritical() << "[LogAnalyzerSubProcess] Failed to initialize IPC communication.";
        return false;
    }

    // 从ConfigManager加载配置
    SetupConfiguration();
    return true;
}

bool LogAnalyzerSubProcess::OnStart()
{
    qDebug() << "Starting LogAnalyzerSubProcess";
    
    // 启动IPC连接
    if (!ipc_communication_->Start()) {
        qWarning() << "IPC connection start failed, will automatically retry.";
    }
    
    // 基类会启动状态报告
    
    qDebug() << "LogAnalyzerSubProcess started successfully";
    return true;
}

void LogAnalyzerSubProcess::OnStop()
{
    // 停止IPC
    if(ipc_communication_) {
        ipc_communication_->Stop();
    }
    
    qDebug() << "LogAnalyzerSubProcess stopped";
}

void LogAnalyzerSubProcess::OnHandleMessage(const IpcMessage& message)
{
    qDebug() << "Handling message:" << MessageTypeToString(message.type) 
             << "from" << message.sender_id;
    
    switch (message.type) {
    case MessageType::kConfigUpdate:
        HandleConfigUpdateMessage(message);
        break;
        
    case MessageType::kCommand:
        HandleCommandMessage(message);
        break;
        
    case MessageType::kShutdown:
        HandleShutdownMessage(message);
        break;
        
    default:
        qDebug() << "Unhandled message type:" << MessageTypeToString(message.type);
        break;
    }
}

void LogAnalyzerSubProcess::SetupConfiguration()
{
    auto* config_manager = GetConfigManager();
    if (!config_manager) return;

    // 加载定时器间隔
    heartbeat_interval_ms_ = config_manager->GetValue("heartbeat_interval_ms", heartbeat_interval_ms_).toInt();

    qDebug() << "Configuration loaded successfully from ConfigManager";
}

void LogAnalyzerSubProcess::HandleConfigUpdateMessage(const IpcMessage& message)
{
    qDebug() << "Handling config update message";
    const QString workDir = GetConfigManager()->GetValue("work_directory").toString();
    if (!workDir.isEmpty())
    {
        emit workDirectoryUpdated(workDir);
    }
    qDebug() << "Configuration updated successfully";
}

void LogAnalyzerSubProcess::HandleCommandMessage(const IpcMessage& message)
{
    QString command = message.body.value("command").toString();
    qDebug() << "Handling command:" << command;
    
    if (command == "select_ip") {
        QString ip = message.body.value("selected_ip").toString();
        qDebug() << "Selected ip:" << ip;
        emit ipAddressSelected(ip);
    } else {
        qWarning() << "Unknown command:" << command;
    }
}

void LogAnalyzerSubProcess::HandleShutdownMessage(const IpcMessage& message)
{
    Q_UNUSED(message)
    qDebug() << "Received shutdown message, stopping process";
    Stop();
    QCoreApplication::quit();
}

void LogAnalyzerSubProcess::SendErrorReport(const QString& error_message, const QString& context)
{
    if (auto* ipc = GetIpc()) {
        IpcMessage error_report;
        error_report.type = MessageType::kErrorReport;
        error_report.topic = "error";
        error_report.sender_id = "AGV分析";
        error_report.receiver_id = "main_process";
        
        QJsonObject error_body;
        error_body["error_message"] = error_message;
        error_body["context"] = context;
        error_body["process_state"] = ProcessStateToString(GetState());
        error_report.body = error_body;
        
        ipc->SendMessage(error_report);
        qWarning() << "Error report sent:" << error_message;
    }
}



