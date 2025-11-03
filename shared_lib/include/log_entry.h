#ifndef LOG_ENTRY_H
#define LOG_ENTRY_H

#include <QObject>
#include <QString>
#include <QDateTime>
#include <QJsonObject>
#include <QStringList>
#include <QMap>
#include <QList>
#include "vta_subprocess_base_global.h"

/**
 * @brief 日志级别（与主进程一致）
 */
enum class LogLevel {
    kTrace = 0,
    kDebug,
    kInfo,
    kWarning,
    kError,
    kFatal
};

/**
 * @brief 日志分类（与主进程一致）
 */
enum class LogCategory {
    kSystem = 0,
    kBusiness,
    kPerformance,
    kSecurity,
    kNetwork,
    kDatabase,
    kUser
};

/**
 * @brief 日志条目（与主进程一致）
 */
struct VTA_SUBPROCESS_BASE_EXPORT LogEntry {
    QString log_id;
    QDateTime timestamp;
    LogLevel level;
    LogCategory category;
    QString source_process;
    QString module_name;
    QString function_name;
    int line_number;
    QString message;
    QJsonObject context;
    QString thread_id;
    QString session_id;

    VTA_SUBPROCESS_BASE_EXPORT QJsonObject ToJson() const;
    VTA_SUBPROCESS_BASE_EXPORT static LogEntry FromJson(const QJsonObject& json);

    VTA_SUBPROCESS_BASE_EXPORT QString ToString() const;

    VTA_SUBPROCESS_BASE_EXPORT static LogEntry Create(LogLevel level,
                           LogCategory category,
                           const QString& source_process,
                           const QString& message,
                           const QString& module_name = QString(),
                           const QString& function_name = QString(),
                           int line_number = 0);
};

/**
 * @brief 日志查询条件（与主进程一致）
 */
struct VTA_SUBPROCESS_BASE_EXPORT LogQueryCondition {
    QDateTime start_time;
    QDateTime end_time;
    QList<LogLevel> levels;
    QList<LogCategory> categories;
    QList<QString> process_ids;
    QStringList source_processes;
    QStringList module_names;
    QString keyword;
    int limit;
    int offset;

    VTA_SUBPROCESS_BASE_EXPORT LogQueryCondition();

    VTA_SUBPROCESS_BASE_EXPORT void Reset();
    VTA_SUBPROCESS_BASE_EXPORT bool IsValid() const;
};

/**
 * @brief 日志统计信息（与主进程一致）
 */
struct VTA_SUBPROCESS_BASE_EXPORT LogStatistics {
    int total_count;
    QMap<LogLevel, int> level_counts;
    QMap<LogCategory, int> category_counts;
    QMap<QString, int> process_counts;
    QDateTime earliest_time;
    QDateTime latest_time;
    qint64 total_size_bytes;

    VTA_SUBPROCESS_BASE_EXPORT QJsonObject ToJson() const;
};

// 工具函数（与主进程一致）
VTA_SUBPROCESS_BASE_EXPORT QString LogLevelToString(LogLevel level);
VTA_SUBPROCESS_BASE_EXPORT QString LogCategoryToString(LogCategory category);
VTA_SUBPROCESS_BASE_EXPORT LogLevel LogLevelFromString(const QString& level_str);
VTA_SUBPROCESS_BASE_EXPORT LogCategory LogCategoryFromString(const QString& category_str);

// 便捷日志宏（与主进程一致）
#define LOG_TRACE(process, message) \
LogEntry::Create(LogLevel::kTrace, LogCategory::kSystem, process, message, \
                 __FILE__, __FUNCTION__, __LINE__)

#define LOG_DEBUG(process, message) \
    LogEntry::Create(LogLevel::kDebug, LogCategory::kSystem, process, message, \
                     __FILE__, __FUNCTION__, __LINE__)

#define LOG_INFO(process, message) \
    LogEntry::Create(LogLevel::kInfo, LogCategory::kSystem, process, message, \
                     __FILE__, __FUNCTION__, __LINE__)

#define LOG_WARNING(process, message) \
    LogEntry::Create(LogLevel::kWarning, LogCategory::kSystem, process, \
                     message, __FILE__, __FUNCTION__, __LINE__)

#define LOG_ERROR(process, message) \
    LogEntry::Create(LogLevel::kError, LogCategory::kSystem, process, message, \
                     __FILE__, __FUNCTION__, __LINE__)

#define LOG_FATAL(process, message) \
    LogEntry::Create(LogLevel::kFatal, LogCategory::kSystem, process, message, \
                     __FILE__, __FUNCTION__, __LINE__)
#endif // LOG_ENTRY_H
