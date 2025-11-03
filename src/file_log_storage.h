#ifndef FILE_LOG_STORAGE_H
#define FILE_LOG_STORAGE_H

#include "i_sub_process_log_storage.h"
#include <QFile>
#include <QTextStream>
#include <QMutex>
#include <QDir>
#include <QTimer>
#include <memory>

/**
 * @brief 文件日志存储实现类
 * 
 * 将子进程日志写入到本地文件系统
 * 支持日志轮转、自动清理、异步写入等功能
 */
class FileLogStorage : public ISubProcessLogStorage
{
    Q_OBJECT

public:
    explicit FileLogStorage(QObject* parent = nullptr);
    virtual ~FileLogStorage();

    // ISubProcessLogStorage接口实现
    bool Initialize(const QJsonObject& config) override;
    bool Start() override;
    void Stop() override;
    bool WriteLog(const LogEntry& entry) override;
    int WriteLogs(const QList<LogEntry>& entries) override;
    bool WriteLogAsync(const LogEntry& entry) override;
    QList<LogEntry> QueryLogs(const LogQueryCondition& condition) override;
    QList<LogEntry> GetLatestLogs(int count, const QList<LogLevel>& level_filter = {}) override;
    QList<LogEntry> GetProcessLogs(const QString& process_id, int count = 100) override;
    int CleanupOldLogs(int days_to_keep) override;
    bool ArchiveLogs(const QDateTime& start_time, const QDateTime& end_time, const QString& archive_path) override;
    LogStatistics GetStatistics(const LogQueryCondition& condition = LogQueryCondition()) override;
    bool IsHealthy() const override;
    QJsonObject GetStorageInfo() const override;
    bool Flush() override;
    bool CreateIndex(const QString& field_name) override;
    QString GetLastError() const override;
    
    // 兼容性方法（用于向后兼容）
    bool StoreLogEntry(const LogEntry& entry);
    QList<LogEntry> RetrieveLogEntries(const QDateTime& start_time, 
                                       const QDateTime& end_time, 
                                       LogLevel min_level = LogLevel::kDebug);
    bool ClearLogs(const QDateTime& before_time);
    qint64 GetTotalLogSize() const;

    // 文件存储特有的方法
    void SetLogDirectory(const QString& directory);
    void SetMaxFileSize(qint64 size_bytes);
    void SetMaxTotalSize(qint64 size_bytes);
    void SetRetentionDays(int days);
    void SetFlushInterval(int interval_ms);

protected slots:
    void OnFlushTimer();
    void OnCleanupTimer();

private:
    // 初始化方法
    bool InitializeLogDirectory();
    bool InitializeLogFile();
    bool InitializeTimers();
    
    // 文件管理方法
    void RotateLogFile();
    void CleanupOldLogs();
    QString GenerateLogFileName(const QDateTime& timestamp = QDateTime::currentDateTime()) const;
    QString GetCurrentLogFilePath() const;
    
    // 写入方法
    bool WriteLogEntryToFile(const LogEntry& entry);
    void FlushBufferedEntries();
    QString FormatLogEntry(const LogEntry& entry) const;
    
    // 读取方法
    QList<LogEntry> ReadLogEntriesFromFile(const QString& file_path,
                                           const QDateTime& start_time,
                                           const QDateTime& end_time,
                                           LogLevel min_level) const;
    LogEntry ParseLogLine(const QString& line) const;
    
    // 工具方法
    QStringList GetLogFileList() const;
    qint64 CalculateTotalLogSize() const;
    bool IsLogFileExpired(const QString& file_path, int retention_days) const;
    QDateTime ExtractTimestampFromFileName(const QString& file_name) const;

private:
    // 文件系统相关
    QString log_directory_;
    QString log_file_prefix_;
    QString current_log_file_path_;
    std::unique_ptr<QFile> current_log_file_;
    std::unique_ptr<QTextStream> log_stream_;
    
    // 配置参数
    qint64 max_file_size_;          // 单个日志文件最大大小
    qint64 max_total_size_;         // 日志总大小限制
    int retention_days_;            // 日志保留天数
    int flush_interval_ms_;         // 刷新间隔
    int cleanup_interval_hours_;    // 清理间隔（小时）
    
    // 缓冲和同步
    mutable QMutex log_mutex_;
    QList<LogEntry> buffered_entries_;
    int max_buffer_size_;
    
    // 定时器
    std::unique_ptr<QTimer> flush_timer_;
    std::unique_ptr<QTimer> cleanup_timer_;
    
    // 统计信息
    qint64 total_entries_written_;
    qint64 current_file_size_;
    QDateTime last_rotation_time_;
    
    // 进程标识
    QString process_id_;
};

#endif // FILE_LOG_STORAGE_H
