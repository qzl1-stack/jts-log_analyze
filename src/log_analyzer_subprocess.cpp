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
    , file_watcher_(nullptr)
    , heartbeat_interval_ms_(5000)      // 5秒心跳
    , max_file_size_mb_(100)            // 最大文件大小100MB
    , auto_analyze_enabled_(true)
    , total_files_processed_(0)
    , total_lines_analyzed_(0)
    , total_errors_found_(0)
{
    // 生成唯一进程ID
    process_id_ = QString("log_analyzer_%1").arg(QUuid::createUuid().toString(QUuid::WithoutBraces));
    
    // 设置支持的日志文件扩展名
    supported_extensions_ << "*.log" << "*.txt" << "*.out" << "*.err";
    
    // 添加默认分析模式
    analysis_patterns_["error"] = "\\b(ERROR|FATAL|CRITICAL)\\b";
    analysis_patterns_["warning"] = "\\b(WARN|WARNING)\\b";
    analysis_patterns_["info"] = "\\b(INFO|INFORMATION)\\b";
    analysis_patterns_["debug"] = "\\b(DEBUG|TRACE)\\b";
    
    // 创建并设置IPC通信
    ipc_communication_ = std::make_unique<LocalIpcCommunication>(this);
    SetIpc(ipc_communication_.get());

    qDebug() << "LogAnalyzerSubProcess created with ID:" << process_id_;
}

LogAnalyzerSubProcess::~LogAnalyzerSubProcess()
{
    qDebug() << "LogAnalyzerSubProcess destroyed:" << process_id_;
}

void LogAnalyzerSubProcess::SetWatchDirectories(const QStringList& directories)
{
    watch_directories_ = directories;
    qDebug() << "Watch directories set:" << directories;
}

void LogAnalyzerSubProcess::AddAnalysisPattern(const QString& pattern_name, const QString& regex_pattern)
{
    analysis_patterns_[pattern_name] = regex_pattern;
    qDebug() << "Analysis pattern added:" << pattern_name << "=>" << regex_pattern;
}

void LogAnalyzerSubProcess::RemoveAnalysisPattern(const QString& pattern_name)
{
    if (analysis_patterns_.remove(pattern_name) > 0) {
        qDebug() << "Analysis pattern removed:" << pattern_name;
    }
}

QJsonObject LogAnalyzerSubProcess::GetCurrentStatus() const
{
    QMutexLocker locker(&stats_mutex_);
    
    QJsonObject status;
    status["process_id"] = process_id_;
    status["state"] = ProcessStateToString(GetState());
    status["total_files_processed"] = total_files_processed_;
    status["total_lines_analyzed"] = total_lines_analyzed_;
    status["total_errors_found"] = total_errors_found_;
    status["last_analysis_time"] = last_analysis_time_.toString(Qt::ISODate);
    status["watch_directories"] = QJsonArray::fromStringList(watch_directories_);
    status["auto_analyze_enabled"] = auto_analyze_enabled_;
    
    return status;
}

bool LogAnalyzerSubProcess::OnInitialize(const QJsonObject& config)
{
    Q_UNUSED(config)
    qDebug() << "Initializing LogAnalyzerSubProcess...";
    
    // 初始化默认配置结构
    GetConfigManager()->InitializeDefaultConfig();
    
    // 初始化IPC
    QJsonValue ipc_config_value = GetConfigManager()->GetValue("ipc");
    if (!ipc_config_value.isObject()) {
        qWarning() << "IPC configuration is missing or not an object.";
        return false;
    }
    
    if (!ipc_communication_->Initialize(ipc_config_value.toObject(), GetSubProcessId())) {
        qCritical() << "[LogAnalyzerSubProcess] Failed to initialize IPC communication.";
        return false;
    }

    // 从ConfigManager加载配置
    SetupConfiguration();
    
    // 初始化文件监控器
    if (!InitializeFileWatcher()) {
        qWarning() << "Failed to initialize file watcher";
        return false;
    } 
    
    qDebug() << "LogAnalyzerSubProcess initialized successfully";
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
    qDebug() << "Stopping LogAnalyzerSubProcess";
    
    // 停止IPC
    if(ipc_communication_) {
        ipc_communication_->Stop();
    }
    
    // 基类会停止状态报告
    
    // 清理文件监控器
    if (file_watcher_) {
        file_watcher_->removePaths(file_watcher_->files());
        file_watcher_->removePaths(file_watcher_->directories());
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

void LogAnalyzerSubProcess::OnFileChanged(const QString& file_path)
{
    qDebug() << "File changed:" << file_path;
    
    if (auto_analyze_enabled_ && IsValidLogFile(file_path)) {
        ProcessLogFile(file_path);
    }
}

void LogAnalyzerSubProcess::OnDirectoryChanged(const QString& directory_path)
{
    qDebug() << "Directory changed:" << directory_path;
    
    // 扫描目录中的新文件
    QDir dir(directory_path);
    QStringList files = dir.entryList(supported_extensions_, QDir::Files);
    
    for (const QString& file : files) {
        QString full_path = dir.absoluteFilePath(file);
        if (!file_watcher_->files().contains(full_path)) {
            file_watcher_->addPath(full_path);
            qDebug() << "Added new file to watch:" << full_path;
        }
    }
}

void LogAnalyzerSubProcess::OnAnalysisCompleted(const QString& file_path, const QJsonObject& results)
{
    qDebug() << "Analysis completed for:" << file_path;
    SendAnalysisReport(file_path, results);
}

bool LogAnalyzerSubProcess::InitializeFileWatcher()
{
    file_watcher_ = std::make_unique<QFileSystemWatcher>(this);
    
    // 连接信号
    connect(file_watcher_.get(), &QFileSystemWatcher::fileChanged,
            this, &LogAnalyzerSubProcess::OnFileChanged);
    connect(file_watcher_.get(), &QFileSystemWatcher::directoryChanged,
            this, &LogAnalyzerSubProcess::OnDirectoryChanged);
    
    // 添加监控目录
    for (const QString& dir_path : watch_directories_) {
        QDir dir(dir_path);
        if (dir.exists()) {
            file_watcher_->addPath(dir_path);
            
            // 添加现有文件到监控
            QStringList files = dir.entryList(supported_extensions_, QDir::Files);
            for (const QString& file : files) {
                QString full_path = dir.absoluteFilePath(file);
                file_watcher_->addPath(full_path);
            }
            
            qDebug() << "Added directory to watch:" << dir_path << "with" << files.size() << "files";
        } else {
            qWarning() << "Watch directory does not exist:" << dir_path;
        }
    }
    
    return true;
}

void LogAnalyzerSubProcess::SetupConfiguration()
{
    auto* config_manager = GetConfigManager();
    if (!config_manager) return;

    // 加载监控目录
    QJsonValue watch_dirs_value = config_manager->GetValue("watch_directories");
    if (watch_dirs_value.isArray()) {
        QJsonArray watch_dirs = watch_dirs_value.toArray();
        watch_directories_.clear();
        for (const auto& value : watch_dirs) {
            watch_directories_.append(value.toString());
        }
    }

    // 加载定时器间隔
    heartbeat_interval_ms_ = config_manager->GetValue("heartbeat_interval_ms", heartbeat_interval_ms_).toInt();

    // 加载其他配置
    max_file_size_mb_ = config_manager->GetValue("max_file_size_mb", max_file_size_mb_).toInt();
    auto_analyze_enabled_ = config_manager->GetValue("auto_analyze_enabled", auto_analyze_enabled_).toBool();

    // 加载自定义分析模式
    QJsonValue patterns_value = config_manager->GetValue("analysis_patterns");
    if (patterns_value.isObject()) {
        QJsonObject patterns = patterns_value.toObject();
        for (auto it = patterns.begin(); it != patterns.end(); ++it) {
            analysis_patterns_[it.key()] = it.value().toString();
        }
    }

    qDebug() << "Configuration loaded successfully from ConfigManager";
}

void LogAnalyzerSubProcess::ProcessLogFile(const QString& file_path)
{
    QFileInfo file_info(file_path);
    
    // 检查文件大小
    if (file_info.size() > max_file_size_mb_ * 1024 * 1024) {
        qWarning() << "File too large, skipping:" << file_path;
        return;
    }
    
    // 读取文件内容
    QFile file(file_path);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        qWarning() << "Cannot open file:" << file_path;
        return;
    }
    
    QTextStream stream(&file);
    QString content = stream.readAll();
    file.close();
    
    // 分析内容
    AnalyzeLogContent(file_path, content);
    
    // 更新统计信息
    QMutexLocker locker(&stats_mutex_);
    total_files_processed_++;
    total_lines_analyzed_ += content.count('\n') + 1;
    last_analysis_time_ = QDateTime::currentDateTime();
}

void LogAnalyzerSubProcess::AnalyzeLogContent(const QString& file_path, const QString& content)
{
    QJsonObject analysis_results;
    analysis_results["file_path"] = file_path;
    analysis_results["timestamp"] = QDateTime::currentDateTime().toString(Qt::ISODate);
    
    // 基本统计
    QJsonObject basic_stats;
    basic_stats["total_lines"] = content.count('\n') + 1;
    basic_stats["file_size"] = content.size();
    analysis_results["basic_stats"] = basic_stats;
    
    // 模式匹配分析
    QJsonObject pattern_matches;
    for (auto it = analysis_patterns_.begin(); it != analysis_patterns_.end(); ++it) {
        QRegularExpression regex(it.value(), QRegularExpression::CaseInsensitiveOption);
        QRegularExpressionMatchIterator matches = regex.globalMatch(content);
        
        int match_count = 0;
        QJsonArray match_lines;
        
        QStringList lines = content.split('\n');
        for (int i = 0; i < lines.size(); ++i) {
            if (regex.match(lines[i]).hasMatch()) {
                match_count++;
                QJsonObject match_info;
                match_info["line_number"] = i + 1;
                match_info["content"] = lines[i].trimmed();
                match_lines.append(match_info);
                
                // 限制返回的匹配行数
                if (match_lines.size() >= 10) {
                    break;
                }
            }
        }
        
        QJsonObject pattern_result;
        pattern_result["count"] = match_count;
        pattern_result["matches"] = match_lines;
        pattern_matches[it.key()] = pattern_result;
        
        // 如果是错误模式，更新错误统计
        if (it.key() == "error" || it.key() == "fatal") {
            QMutexLocker locker(&stats_mutex_);
            total_errors_found_ += match_count;
        }
    }
    
    analysis_results["pattern_matches"] = pattern_matches;
    
    // 发送分析结果
    OnAnalysisCompleted(file_path, analysis_results);
}

void LogAnalyzerSubProcess::HandleConfigUpdateMessage(const IpcMessage& message)
{
    qDebug() << "Handling config update message";

    QJsonObject new_config = message.body;
    qDebug() << "New config:" << new_config;
    if (GetConfigManager()->LoadFromJsonObject(new_config)) {
        // 重新初始化文件监控器
        if (file_watcher_) {
            file_watcher_->removePaths(file_watcher_->files());
            file_watcher_->removePaths(file_watcher_->directories());
        }
        InitializeFileWatcher();
        qDebug() << "config_manager->GetConfig():" << GetConfigManager()->GetConfig();
        
        // 将新的工作目录透传给上层
        const QString workDir = GetConfigManager()->GetValue("work_directory").toString();
        if (!workDir.isEmpty()) {
            emit workDirectoryUpdated(workDir);
        }
        
        qDebug() << "Configuration updated successfully";
    } else {
        SendErrorReport("Failed to update configuration", "HandleConfigUpdateMessage");
    }
}

void LogAnalyzerSubProcess::HandleCommandMessage(const IpcMessage& message)
{
    QString command = message.body.value("command").toString();
    qDebug() << "Handling command:" << command;
    
    if (command == "analyze_file") {
        QString file_path = message.body.value("file_path").toString();
        if (!file_path.isEmpty() && QFile::exists(file_path)) {
            ProcessLogFile(file_path);
        }
    }else if (command == "select_ip") {
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

void LogAnalyzerSubProcess::SendAnalysisReport(const QString& file_path, const QJsonObject& analysis_results)
{
    if (auto* ipc = GetIpc()) {
        IpcMessage report_message;
        report_message.type = MessageType::kStatusReport;
        report_message.topic = "analysis_report";
        report_message.msg_id = GenerateMessageId();
        report_message.timestamp = GetCurrentTimestamp();
        report_message.sender_id = process_id_;
        report_message.receiver_id = "main_process";
        report_message.body = analysis_results;
        
        // ipc->SendMessage(report_message);
        qDebug() << "Analysis report sent for:" << file_path;
    }
}

void LogAnalyzerSubProcess::SendErrorReport(const QString& error_message, const QString& context)
{
    if (auto* ipc = GetIpc()) {
        IpcMessage error_report;
        error_report.type = MessageType::kErrorReport;
        error_report.topic = "error";
        error_report.msg_id = GenerateMessageId();
        error_report.timestamp = GetCurrentTimestamp();
        error_report.sender_id = process_id_;
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

bool LogAnalyzerSubProcess::IsValidLogFile(const QString& file_path) const
{
    QFileInfo file_info(file_path);
    
    // 检查文件是否存在且可读
    if (!file_info.exists() || !file_info.isReadable()) {
        return false;
    }
    
    // 检查文件扩展名
    QString suffix = file_info.suffix().toLower();
    QStringList valid_suffixes = {"log", "txt", "out", "err"};
    
    return valid_suffixes.contains(suffix);
}

QString LogAnalyzerSubProcess::GenerateMessageId() const
{
    return QUuid::createUuid().toString(QUuid::WithoutBraces);
}

qint64 LogAnalyzerSubProcess::GetCurrentTimestamp() const
{
    return QDateTime::currentMSecsSinceEpoch();
}


// LogAnalyzerStatusReporter implementation
LogAnalyzerStatusReporter::LogAnalyzerStatusReporter(LogAnalyzerSubProcess* process, QObject* parent)
    : SubProcessStatusReporter(parent), log_analyzer_process_(process)
{
}

QJsonObject LogAnalyzerStatusReporter::CollectStatus()
{
    if (log_analyzer_process_) {
        return log_analyzer_process_->GetCurrentStatus();
    }
    return SubProcessStatusReporter::CollectStatus(); // Fallback to base implementation
}
