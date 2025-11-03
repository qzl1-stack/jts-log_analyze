#ifndef I_SUB_PROCESS_LOG_STORAGE_H
#define I_SUB_PROCESS_LOG_STORAGE_H
#include <QObject>
#include <QJsonObject>
#include "log_entry.h"
#include "vta_subprocess_base_global.h"

/**
 * @brief 子进程侧日志存储接口（与主进程一致）
 */
class VTA_SUBPROCESS_BASE_EXPORT ISubProcessLogStorage : public QObject {
    Q_OBJECT

public:
    explicit ISubProcessLogStorage(QObject* parent = nullptr)
        : QObject(parent) {}
    virtual ~ISubProcessLogStorage() = default;

    virtual bool Initialize(const QJsonObject& config) = 0;
    virtual bool Start() = 0;
    virtual void Stop() = 0;

    virtual bool WriteLog(const LogEntry& entry) = 0;
    virtual int WriteLogs(const QList<LogEntry>& entries) = 0;
    virtual bool WriteLogAsync(const LogEntry& entry) = 0;

    virtual QList<LogEntry> QueryLogs(
        const LogQueryCondition& condition) = 0;

    virtual QList<LogEntry> GetLatestLogs(
        int count, const QList<LogLevel>& level_filter = {}) = 0;

    virtual QList<LogEntry> GetProcessLogs(
        const QString& process_id, int count = 100) = 0;

    virtual int CleanupOldLogs(int days_to_keep) = 0;

    virtual bool ArchiveLogs(const QDateTime& start_time,
                             const QDateTime& end_time,
                             const QString& archive_path) = 0;

    virtual LogStatistics GetStatistics(
        const LogQueryCondition& condition = LogQueryCondition()) = 0;

    virtual bool IsHealthy() const = 0;
    virtual QJsonObject GetStorageInfo() const = 0;
    virtual bool Flush() = 0;
    virtual bool CreateIndex(const QString& field_name) = 0;
    virtual QString GetLastError() const = 0;

signals:
    void LogWritten(const LogEntry& entry, bool success);
    void BatchLogWritten(int count, int total);
    void StorageStateChanged(bool is_healthy,
                             const QString& error_message = QString());
    void StorageCapacityWarning(double used_percentage, qint64 free_bytes);
    void ArchiveCompleted(const QString& archive_path,
                          int log_count, bool success);
};

#endif // I_SUB_PROCESS_LOG_STORAGE_H
