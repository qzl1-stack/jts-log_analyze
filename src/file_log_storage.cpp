#include "file_log_storage.h"
#include <QDebug>
#include <QJsonObject>
#include <QJsonArray>
#include <QStandardPaths>
#include <QCoreApplication>
#include <QFileInfo>
#include <QDirIterator>
#include <QDateTime>
#include <QMutexLocker>
#include <QUuid>

FileLogStorage::FileLogStorage(QObject* parent)
    : ISubProcessLogStorage(parent)
    , log_directory_()
    , log_file_prefix_("subprocess_log")
    , current_log_file_(nullptr)
    , log_stream_(nullptr)
    , max_file_size_(10 * 1024 * 1024)      // 10MB per file
    , max_total_size_(100 * 1024 * 1024)    // 100MB total
    , retention_days_(7)                     // 7 days retention
    , flush_interval_ms_(5000)               // 5 seconds flush interval
    , cleanup_interval_hours_(24)            // 24 hours cleanup interval
    , max_buffer_size_(100)                  // 100 entries buffer
    , flush_timer_(nullptr)
    , cleanup_timer_(nullptr)
    , total_entries_written_(0)
    , current_file_size_(0)
{
    // 生成进程标识
    process_id_ = QString("subprocess_%1").arg(QUuid::createUuid().toString(QUuid::WithoutBraces).left(8));
    
    // 设置默认日志目录
    QString default_log_dir = QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation);
    default_log_dir += "/logs";
    log_directory_ = default_log_dir;
    
    qDebug() << "FileLogStorage created with process ID:" << process_id_;
}

FileLogStorage::~FileLogStorage()
{
    // 刷新缓冲的日志条目
    FlushBufferedEntries();
    
    // 关闭文件
    if (log_stream_) {
        log_stream_->flush();
        log_stream_.reset();
    }
    if (current_log_file_) {
        current_log_file_->close();
        current_log_file_.reset();
    }
    
    qDebug() << "FileLogStorage destroyed:" << process_id_;
}

bool FileLogStorage::Initialize(const QJsonObject& config)
{
    qDebug() << "Initializing FileLogStorage with config:" << config;
    
    // 加载配置
    if (config.contains("log_directory")) {
        log_directory_ = config["log_directory"].toString();
    }
    if (config.contains("log_file_prefix")) {
        log_file_prefix_ = config["log_file_prefix"].toString();
    }
    
    max_file_size_ = config.value("max_file_size").toInt(static_cast<int>(max_file_size_));
    max_total_size_ = config.value("max_total_size").toInt(static_cast<int>(max_total_size_));
    retention_days_ = config.value("retention_days").toInt(retention_days_);
    flush_interval_ms_ = config.value("flush_interval_ms").toInt(flush_interval_ms_);
    max_buffer_size_ = config.value("max_buffer_size").toInt(max_buffer_size_);
    
    // 初始化日志目录
    if (!InitializeLogDirectory()) {
        qWarning() << "Failed to initialize log directory";
        return false;
    }
    
    // 初始化日志文件
    if (!InitializeLogFile()) {
        qWarning() << "Failed to initialize log file";
        return false;
    }
    
    // 初始化定时器
    if (!InitializeTimers()) {
        qWarning() << "Failed to initialize timers";
        return false;
    }
    
    qDebug() << "FileLogStorage initialized successfully";
    qDebug() << "Log directory:" << log_directory_;
    qDebug() << "Current log file:" << current_log_file_path_;
    
    return true;
}

bool FileLogStorage::Start()
{
    // 启动定时器
    if (flush_timer_) {
        flush_timer_->start();
    }
    if (cleanup_timer_) {
        cleanup_timer_->start();
    }
    return true;
}

void FileLogStorage::Stop()
{
    // 停止定时器
    if (flush_timer_) {
        flush_timer_->stop();
    }
    if (cleanup_timer_) {
        cleanup_timer_->stop();
    }
    
    // 刷新剩余的日志
    FlushBufferedEntries();
}

bool FileLogStorage::WriteLog(const LogEntry& entry)
{
    return StoreLogEntry(entry);
}

int FileLogStorage::WriteLogs(const QList<LogEntry>& entries)
{
    int success_count = 0;
    for (const LogEntry& entry : entries) {
        if (WriteLog(entry)) {
            success_count++;
        }
    }
    return success_count;
}

bool FileLogStorage::WriteLogAsync(const LogEntry& entry)
{
    return StoreLogEntry(entry);
}

QList<LogEntry> FileLogStorage::QueryLogs(const LogQueryCondition& condition)
{
    QDateTime start_time = condition.start_time.isValid() ? condition.start_time : QDateTime::currentDateTime().addDays(-7);
    QDateTime end_time = condition.end_time.isValid() ? condition.end_time : QDateTime::currentDateTime();
    LogLevel min_level = condition.levels.isEmpty() ? LogLevel::kTrace : condition.levels.first();
    
    return RetrieveLogEntries(start_time, end_time, min_level);
}

QList<LogEntry> FileLogStorage::GetLatestLogs(int count, const QList<LogLevel>& level_filter)
{
    QDateTime end_time = QDateTime::currentDateTime();
    QDateTime start_time = end_time.addDays(-1); // 查找最近一天的日志
    LogLevel min_level = level_filter.isEmpty() ? LogLevel::kTrace : level_filter.first();
    
    QList<LogEntry> all_logs = RetrieveLogEntries(start_time, end_time, min_level);
    
    // 按时间戳倒序排序，返回最新的count条
    std::sort(all_logs.begin(), all_logs.end(), [](const LogEntry& a, const LogEntry& b) {
        return a.timestamp > b.timestamp;
    });
    
    return all_logs.mid(0, qMin(count, all_logs.size()));
}

QList<LogEntry> FileLogStorage::GetProcessLogs(const QString& process_id, int count)
{
    QDateTime end_time = QDateTime::currentDateTime();
    QDateTime start_time = end_time.addDays(-1);
    
    QList<LogEntry> all_logs = RetrieveLogEntries(start_time, end_time, LogLevel::kTrace);
    QList<LogEntry> process_logs;
    
    for (const LogEntry& entry : all_logs) {
        if (entry.source_process == process_id) {
            process_logs.append(entry);
            if (process_logs.size() >= count) {
                break;
            }
        }
    }
    
    return process_logs;
}

int FileLogStorage::CleanupOldLogs(int days_to_keep)
{
    QDateTime cutoff_time = QDateTime::currentDateTime().addDays(-days_to_keep);
    return ClearLogs(cutoff_time) ? 1 : 0;
}

bool FileLogStorage::ArchiveLogs(const QDateTime& start_time, const QDateTime& end_time, const QString& archive_path)
{
    Q_UNUSED(start_time)
    Q_UNUSED(end_time)
    Q_UNUSED(archive_path)
    // 简单实现：暂不支持归档功能
    return false;
}

LogStatistics FileLogStorage::GetStatistics(const LogQueryCondition& condition)
{
    LogStatistics stats;
    stats.total_count = 0;
    stats.total_size_bytes = GetTotalLogSize();
    stats.earliest_time = QDateTime::currentDateTime();
    stats.latest_time = QDateTime::currentDateTime().addDays(-30);
    
    // 简单统计实现
    QList<LogEntry> logs = QueryLogs(condition);
    stats.total_count = logs.size();
    
    for (const LogEntry& entry : logs) {
        stats.level_counts[entry.level]++;
        stats.category_counts[entry.category]++;
        stats.process_counts[entry.source_process]++;
        
        if (entry.timestamp < stats.earliest_time) {
            stats.earliest_time = entry.timestamp;
        }
        if (entry.timestamp > stats.latest_time) {
            stats.latest_time = entry.timestamp;
        }
    }
    
    return stats;
}

bool FileLogStorage::IsHealthy() const
{
    return current_log_file_ && current_log_file_->isOpen();
}

QJsonObject FileLogStorage::GetStorageInfo() const
{
    QJsonObject info;
    info["log_directory"] = log_directory_;
    info["current_file"] = current_log_file_path_;
    info["total_size"] = static_cast<qint64>(GetTotalLogSize());
    info["total_entries"] = static_cast<qint64>(total_entries_written_);
    info["is_healthy"] = IsHealthy();
    return info;
}

bool FileLogStorage::Flush()
{
    FlushBufferedEntries();
    return true;
}

bool FileLogStorage::CreateIndex(const QString& field_name)
{
    Q_UNUSED(field_name)
    // 文件存储不支持索引
    return false;
}

QString FileLogStorage::GetLastError() const
{
    if (current_log_file_) {
        return current_log_file_->errorString();
    }
    return QString();
}

bool FileLogStorage::StoreLogEntry(const LogEntry& entry)
{
    QMutexLocker locker(&log_mutex_);
    
    // 添加到缓冲区
    buffered_entries_.append(entry);
    
    // 如果缓冲区满了，立即刷新
    if (buffered_entries_.size() >= max_buffer_size_) {
        locker.unlock();
        FlushBufferedEntries();
        return true;
    }
    
    return true;
}

QList<LogEntry> FileLogStorage::RetrieveLogEntries(const QDateTime& start_time, 
                                                   const QDateTime& end_time, 
                                                   LogLevel min_level)
{
    QList<LogEntry> result;
    
    // 获取所有日志文件
    QStringList log_files = GetLogFileList();
    
    for (const QString& file_path : log_files) {
        // 检查文件时间范围
        QDateTime file_time = ExtractTimestampFromFileName(QFileInfo(file_path).baseName());
        if (file_time.isValid() && file_time < start_time.addDays(-1)) {
            continue; // 跳过太旧的文件
        }
        
        // 读取文件中的日志条目
        QList<LogEntry> file_entries = ReadLogEntriesFromFile(file_path, start_time, end_time, min_level);
        result.append(file_entries);
    }
    
    // 按时间戳排序
    std::sort(result.begin(), result.end(), [](const LogEntry& a, const LogEntry& b) {
        return a.timestamp < b.timestamp;
    });
    
    qDebug() << "Retrieved" << result.size() << "log entries from" << log_files.size() << "files";
    return result;
}

bool FileLogStorage::ClearLogs(const QDateTime& before_time)
{
    QMutexLocker locker(&log_mutex_);
    
    QStringList log_files = GetLogFileList();
    int deleted_count = 0;
    
    for (const QString& file_path : log_files) {
        QFileInfo file_info(file_path);
        
        // 检查文件修改时间
        if (file_info.lastModified() < before_time) {
            if (QFile::remove(file_path)) {
                deleted_count++;
                qDebug() << "Deleted old log file:" << file_path;
            } else {
                qWarning() << "Failed to delete log file:" << file_path;
            }
        }
    }
    
    qDebug() << "Cleared" << deleted_count << "log files before" << before_time.toString();
    return deleted_count > 0;
}

qint64 FileLogStorage::GetTotalLogSize() const
{
    return CalculateTotalLogSize();
}

void FileLogStorage::SetLogDirectory(const QString& directory)
{
    log_directory_ = directory;
}

void FileLogStorage::SetMaxFileSize(qint64 size_bytes)
{
    max_file_size_ = size_bytes;
}

void FileLogStorage::SetMaxTotalSize(qint64 size_bytes)
{
    max_total_size_ = size_bytes;
}

void FileLogStorage::SetRetentionDays(int days)
{
    retention_days_ = days;
}

void FileLogStorage::SetFlushInterval(int interval_ms)
{
    flush_interval_ms_ = interval_ms;
    if (flush_timer_) {
        flush_timer_->setInterval(interval_ms);
    }
}

void FileLogStorage::OnFlushTimer()
{
    FlushBufferedEntries();
}

void FileLogStorage::OnCleanupTimer()
{
    CleanupOldLogs();
}

bool FileLogStorage::InitializeLogDirectory()
{
    QDir dir;
    if (!dir.exists(log_directory_)) {
        if (!dir.mkpath(log_directory_)) {
            qWarning() << "Failed to create log directory:" << log_directory_;
            return false;
        }
        qDebug() << "Created log directory:" << log_directory_;
    }
    
    return true;
}

bool FileLogStorage::InitializeLogFile()
{
    current_log_file_path_ = GetCurrentLogFilePath();
    current_log_file_ = std::make_unique<QFile>(current_log_file_path_);
    
    if (!current_log_file_->open(QIODevice::WriteOnly | QIODevice::Append)) {
        qWarning() << "Failed to open log file:" << current_log_file_path_;
        return false;
    }
    
    log_stream_ = std::make_unique<QTextStream>(current_log_file_.get());
    log_stream_->setEncoding(QStringConverter::Utf8);
    
    // 获取当前文件大小
    current_file_size_ = current_log_file_->size();
    last_rotation_time_ = QDateTime::currentDateTime();
    
    qDebug() << "Initialized log file:" << current_log_file_path_ << "size:" << current_file_size_;
    return true;
}

bool FileLogStorage::InitializeTimers()
{
    // 刷新定时器
    flush_timer_ = std::make_unique<QTimer>(this);
    flush_timer_->setInterval(flush_interval_ms_);
    connect(flush_timer_.get(), &QTimer::timeout,
            this, &FileLogStorage::OnFlushTimer);
    flush_timer_->start();
    
    // 清理定时器
    cleanup_timer_ = std::make_unique<QTimer>(this);
    cleanup_timer_->setInterval(cleanup_interval_hours_ * 60 * 60 * 1000);
    connect(cleanup_timer_.get(), &QTimer::timeout,
            this, &FileLogStorage::OnCleanupTimer);
    cleanup_timer_->start();
    
    return true;
}

void FileLogStorage::RotateLogFile()
{
    qDebug() << "Rotating log file, current size:" << current_file_size_;
    
    // 刷新并关闭当前文件
    if (log_stream_) {
        log_stream_->flush();
        log_stream_.reset();
    }
    if (current_log_file_) {
        current_log_file_->close();
        current_log_file_.reset();
    }
    
    // 创建新的日志文件
    InitializeLogFile();
}

void FileLogStorage::CleanupOldLogs()
{
    qDebug() << "Cleaning up old logs";
    
    QStringList log_files = GetLogFileList();
    qint64 total_size = 0;
    QList<QPair<QDateTime, QString>> file_times;
    
    // 计算总大小并收集文件时间信息
    for (const QString& file_path : log_files) {
        QFileInfo file_info(file_path);
        total_size += file_info.size();
        file_times.append(qMakePair(file_info.lastModified(), file_path));
    }
    
    // 按时间排序（最旧的在前）
    std::sort(file_times.begin(), file_times.end());
    
    int deleted_count = 0;
    
    // 删除过期文件
    QDateTime cutoff_time = QDateTime::currentDateTime().addDays(-retention_days_);
    for (const auto& file_time : file_times) {
        if (file_time.first < cutoff_time || total_size > max_total_size_) {
            QFileInfo file_info(file_time.second);
            if (QFile::remove(file_time.second)) {
                total_size -= file_info.size();
                deleted_count++;
                qDebug() << "Deleted old log file:" << file_time.second;
            }
        }
    }
    
    qDebug() << "Cleanup completed, deleted" << deleted_count << "files";
}

QString FileLogStorage::GenerateLogFileName(const QDateTime& timestamp) const
{
    QString date_str = timestamp.toString("yyyy-MM-dd_hh-mm-ss");
    return QString("%1_%2_%3.log").arg(log_file_prefix_, process_id_, date_str);
}

QString FileLogStorage::GetCurrentLogFilePath() const
{
    QString file_name = GenerateLogFileName();
    return QDir(log_directory_).absoluteFilePath(file_name);
}

bool FileLogStorage::WriteLogEntryToFile(const LogEntry& entry)
{
    if (!log_stream_) {
        return false;
    }
    
    QString formatted_entry = FormatLogEntry(entry);
    *log_stream_ << formatted_entry << Qt::endl;
    
    current_file_size_ += formatted_entry.toUtf8().size() + 1; // +1 for newline
    total_entries_written_++;
    
    // 检查是否需要轮转文件
    if (current_file_size_ >= max_file_size_) {
        RotateLogFile();
    }
    
    return true;
}

void FileLogStorage::FlushBufferedEntries()
{
    QMutexLocker locker(&log_mutex_);
    
    if (buffered_entries_.isEmpty()) {
        return;
    }
    
    qDebug() << "Flushing" << buffered_entries_.size() << "buffered log entries";
    
    for (const LogEntry& entry : buffered_entries_) {
        WriteLogEntryToFile(entry);
    }
    
    buffered_entries_.clear();
    
    // 刷新文件流
    if (log_stream_) {
        log_stream_->flush();
    }
}

QString FileLogStorage::FormatLogEntry(const LogEntry& entry) const
{
    QString level_str = LogLevelToString(entry.level);
    QString category_str = LogCategoryToString(entry.category);
    
    return QString("[%1] [%2] [%3:%4] [%5] [%6] %7")
           .arg(entry.timestamp.toString(Qt::ISODate))
           .arg(level_str)
           .arg(entry.module_name.isEmpty() ? "unknown" : entry.module_name)
           .arg(entry.line_number)
           .arg(category_str)
           .arg(entry.source_process)
           .arg(entry.message);
}

QList<LogEntry> FileLogStorage::ReadLogEntriesFromFile(const QString& file_path,
                                                       const QDateTime& start_time,
                                                       const QDateTime& end_time,
                                                       LogLevel min_level) const
{
    QList<LogEntry> entries;
    
    QFile file(file_path);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        qWarning() << "Cannot open log file for reading:" << file_path;
        return entries;
    }
    
    QTextStream stream(&file);
    stream.setEncoding(QStringConverter::Utf8);
    
    while (!stream.atEnd()) {
        QString line = stream.readLine();
        if (line.isEmpty()) {
            continue;
        }
        
        LogEntry entry = ParseLogLine(line);
        if (entry.timestamp.isValid() &&
            entry.timestamp >= start_time &&
            entry.timestamp <= end_time &&
            entry.level >= min_level) {
            entries.append(entry);
        }
    }
    
    return entries;
}

LogEntry FileLogStorage::ParseLogLine(const QString& line) const
{
    LogEntry entry;
    
    // 简单的日志行解析
    // 格式: [timestamp] [level] [module:line] [category] [process] message
    QRegularExpression regex(R"(\[([^\]]+)\] \[([^\]]+)\] \[([^\]]+):(\d+)\] \[([^\]]+)\] \[([^\]]+)\] (.*))");
    QRegularExpressionMatch match = regex.match(line);
    
    if (match.hasMatch()) {
        entry.timestamp = QDateTime::fromString(match.captured(1), Qt::ISODate);
        entry.level = LogLevelFromString(match.captured(2));
        entry.module_name = match.captured(3);
        entry.line_number = match.captured(4).toInt();
        entry.category = LogCategoryFromString(match.captured(5));
        entry.source_process = match.captured(6);
        entry.message = match.captured(7);
    }
    
    return entry;
}

QStringList FileLogStorage::GetLogFileList() const
{
    QDir dir(log_directory_);
    QStringList filters;
    filters << QString("%1_*.log").arg(log_file_prefix_);
    
    QStringList file_names = dir.entryList(filters, QDir::Files, QDir::Time);
    QStringList full_paths;
    
    for (const QString& file_name : file_names) {
        full_paths.append(dir.absoluteFilePath(file_name));
    }
    
    return full_paths;
}

qint64 FileLogStorage::CalculateTotalLogSize() const
{
    qint64 total_size = 0;
    QStringList log_files = GetLogFileList();
    
    for (const QString& file_path : log_files) {
        QFileInfo file_info(file_path);
        total_size += file_info.size();
    }
    
    return total_size;
}

bool FileLogStorage::IsLogFileExpired(const QString& file_path, int retention_days) const
{
    QFileInfo file_info(file_path);
    QDateTime cutoff_time = QDateTime::currentDateTime().addDays(-retention_days);
    return file_info.lastModified() < cutoff_time;
}

QDateTime FileLogStorage::ExtractTimestampFromFileName(const QString& file_name) const
{
    // 从文件名中提取时间戳
    // 格式: prefix_processid_yyyy-MM-dd_hh-mm-ss
    QRegularExpression regex(R"(\d{4}-\d{2}-\d{2}_\d{2}-\d{2}-\d{2})");
    QRegularExpressionMatch match = regex.match(file_name);
    
    if (match.hasMatch()) {
        return QDateTime::fromString(match.captured(0), "yyyy-MM-dd_hh-mm-ss");
    }
    
    return QDateTime();
}
