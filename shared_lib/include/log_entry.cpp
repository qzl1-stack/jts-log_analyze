#include "log_entry.h"
#include <QJsonDocument>
#include <QUuid>
#include <QThread>

QJsonObject LogEntry::ToJson() const
{
    QJsonObject json;
    json["log_id"] = log_id;
    json["timestamp"] = timestamp.toString(Qt::ISODate);
    json["level"] = static_cast<int>(level);
    json["category"] = static_cast<int>(category);
    json["source_process"] = source_process;
    json["module_name"] = module_name;
    json["function_name"] = function_name;
    json["line_number"] = line_number;
    json["message"] = message;
    json["context"] = context;
    json["thread_id"] = thread_id;
    json["session_id"] = session_id;
    return json;
}

LogEntry LogEntry::FromJson(const QJsonObject& json)
{
    LogEntry entry;
    entry.log_id = json["log_id"].toString();
    entry.timestamp = QDateTime::fromString(json["timestamp"].toString(),
                                            Qt::ISODate);
    entry.level = static_cast<LogLevel>(json["level"].toInt());
    entry.category = static_cast<LogCategory>(json["category"].toInt());
    entry.source_process = json["source_process"].toString();
    entry.module_name = json["module_name"].toString();
    entry.function_name = json["function_name"].toString();
    entry.line_number = json["line_number"].toInt();
    entry.message = json["message"].toString();
    entry.context = json["context"].toObject();
    entry.thread_id = json["thread_id"].toString();
    entry.session_id = json["session_id"].toString();
    return entry;
}

QString LogEntry::ToString() const
{
    QString level_str = LogLevelToString(level);
    QString category_str = LogCategoryToString(category);
    QString time_str = timestamp.toString("yyyy-MM-dd hh:mm:ss.zzz");

    QString result = QString("[%1] [%2] [%3] [%4]")
                         .arg(time_str)
                         .arg(level_str)
                         .arg(category_str)
                         .arg(source_process);

    if (!module_name.isEmpty()) {
        result += QString(" [%1]").arg(module_name);
    }
    if (!function_name.isEmpty() && line_number > 0) {
        result += QString(" [%1:%2]").arg(function_name).arg(line_number);
    }
    result += QString(" %1").arg(message);

    if (!context.isEmpty()) {
        QJsonDocument doc(context);
        result += QString(" Context: %1")
                      .arg(QString::fromUtf8(
                          doc.toJson(QJsonDocument::Compact)));
    }
    return result;
}

LogEntry LogEntry::Create(LogLevel level,
                          LogCategory category,
                          const QString& source_process,
                          const QString& message,
                          const QString& module_name,
                          const QString& function_name,
                          int line_number)
{
    LogEntry entry;
    entry.log_id = QUuid::createUuid()
                       .toString(QUuid::WithoutBraces);
    entry.timestamp = QDateTime::currentDateTime();
    entry.level = level;
    entry.category = category;
    entry.source_process = source_process;
    entry.module_name = module_name;
    entry.function_name = function_name;
    entry.line_number = line_number;
    entry.message = message;
    entry.thread_id = QString::number(
        reinterpret_cast<quintptr>(QThread::currentThreadId()));
    return entry;
}

LogQueryCondition::LogQueryCondition()
{
    Reset();
}

void LogQueryCondition::Reset()
{
    start_time = QDateTime();
    end_time = QDateTime();
    levels.clear();
    categories.clear();
    process_ids.clear();
    source_processes.clear();
    module_names.clear();
    keyword.clear();
    limit = 1000;
    offset = 0;
}

bool LogQueryCondition::IsValid() const
{
    if (start_time.isValid() && end_time.isValid() &&
        start_time > end_time) {
        return false;
    }
    if (limit < 0 || offset < 0) {
        return false;
    }
    return true;
}

QJsonObject LogStatistics::ToJson() const
{
    QJsonObject json;
    json["total_count"] = total_count;
    json["earliest_time"] = earliest_time.toString(Qt::ISODate);
    json["latest_time"] = latest_time.toString(Qt::ISODate);
    json["total_size_bytes"] = total_size_bytes;

    QJsonObject level_json;
    for (auto it = level_counts.begin();
         it != level_counts.end(); ++it) {
        level_json[LogLevelToString(it.key())] = it.value();
    }
    json["level_counts"] = level_json;

    QJsonObject category_json;
    for (auto it = category_counts.begin();
         it != category_counts.end(); ++it) {
        category_json[LogCategoryToString(it.key())] = it.value();
    }
    json["category_counts"] = category_json;

    QJsonObject process_json;
    for (auto it = process_counts.begin();
         it != process_counts.end(); ++it) {
        process_json[it.key()] = it.value();
    }
    json["process_counts"] = process_json;

    return json;
}

QString LogLevelToString(LogLevel level)
{
    switch (level) {
    case LogLevel::kTrace: return "TRACE";
    case LogLevel::kDebug: return "DEBUG";
    case LogLevel::kInfo: return "INFO";
    case LogLevel::kWarning: return "WARNING";
    case LogLevel::kError: return "ERROR";
    case LogLevel::kFatal: return "FATAL";
    default: return "UNKNOWN";
    }
}

QString LogCategoryToString(LogCategory category)
{
    switch (category) {
    case LogCategory::kSystem: return "SYSTEM";
    case LogCategory::kBusiness: return "BUSINESS";
    case LogCategory::kPerformance: return "PERFORMANCE";
    case LogCategory::kSecurity: return "SECURITY";
    case LogCategory::kNetwork: return "NETWORK";
    case LogCategory::kDatabase: return "DATABASE";
    case LogCategory::kUser: return "USER";
    default: return "UNKNOWN";
    }
}

LogLevel LogLevelFromString(const QString& level_str)
{
    QString upper = level_str.toUpper();
    if (upper == "TRACE") return LogLevel::kTrace;
    if (upper == "DEBUG") return LogLevel::kDebug;
    if (upper == "INFO") return LogLevel::kInfo;
    if (upper == "WARNING") return LogLevel::kWarning;
    if (upper == "ERROR") return LogLevel::kError;
    if (upper == "FATAL") return LogLevel::kFatal;
    return LogLevel::kInfo;
}

LogCategory LogCategoryFromString(const QString& category_str)
{
    QString upper = category_str.toUpper();
    if (upper == "SYSTEM") return LogCategory::kSystem;
    if (upper == "BUSINESS") return LogCategory::kBusiness;
    if (upper == "PERFORMANCE") return LogCategory::kPerformance;
    if (upper == "SECURITY") return LogCategory::kSecurity;
    if (upper == "NETWORK") return LogCategory::kNetwork;
    if (upper == "DATABASE") return LogCategory::kDatabase;
    if (upper == "USER") return LogCategory::kUser;
    return LogCategory::kSystem;
}
