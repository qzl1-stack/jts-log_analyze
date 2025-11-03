#include "sub_process_config_manager.h"
#include <QtCore/QFile>
#include <QtCore/QJsonDocument>
#include <QtCore/QMutexLocker>
#include <QtCore/QJsonArray>
#include <QtCore/QDebug>

SubProcessConfigManager::SubProcessConfigManager(QObject* parent)
    : QObject(parent)
{
}

SubProcessConfigManager::~SubProcessConfigManager()
{
}

void SubProcessConfigManager::InitializeDefaultConfig()
{
    QMutexLocker locker(&mutex_);
    config_ = QJsonObject{
        {"ip_table", QJsonArray()},
        {"work_directory",QJsonObject()},
        {"watch_directories", QJsonArray()},
        {"analysis_patterns", QJsonObject()},
        {"ipc", QJsonObject{
            {"server_name", "master_ipc_server"},
            {"reconnect_interval_ms", 5000},
            {"heartbeat_interval_ms", 10000}
        }}
    };
}

bool SubProcessConfigManager::LoadFromFile(const QString& config_path)
{
    QFile file(config_path);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        qWarning("ConfigManager: Could not open config file: %s", qPrintable(config_path));
        return false;
    }

    QJsonParseError parse_error;
    QJsonDocument doc = QJsonDocument::fromJson(file.readAll(), &parse_error);
    file.close();

    if (parse_error.error != QJsonParseError::NoError) {
        qWarning("ConfigManager: Failed to parse config file: %s, error: %s",
                 qPrintable(config_path), qPrintable(parse_error.errorString()));
        return false;
    }

    if (!doc.isObject()) {
        qWarning("ConfigManager: Config file content is not a JSON object: %s", qPrintable(config_path));
        return false;
    }

    return LoadFromJsonObject(doc.object());
}

bool SubProcessConfigManager::LoadFromJsonObject(const QJsonObject& config_data)
{
    QMutexLocker locker(&mutex_);

    if (config_data.contains("updated_config") && config_data["updated_config"].isObject()) {
        // 这是热更新消息，只合并 "updated_config" 部分
        QJsonObject updated_part = config_data["updated_config"].toObject();
        MergeJsonObjects(config_, updated_part);
        qDebug() << "ConfigManager: Configuration hot-reloaded via merge.";
    } else {
        // 这是初始配置覆盖，合并整个对象
        MergeJsonObjects(config_, config_data);
        qDebug() << "ConfigManager: Initial configuration loaded via merge.";
    }

    locker.unlock(); // Emit signal without the lock

    emit ConfigUpdated(config_);
    return true;
}

QJsonObject SubProcessConfigManager::GetConfig() const
{
    QMutexLocker locker(&mutex_);
    return config_;
}

QJsonValue SubProcessConfigManager::GetValue(const QString& key, const QJsonValue& default_value) const
{
    QMutexLocker locker(&mutex_);
    if (key.isEmpty()) {
        return default_value;
    }

    QStringList keys = key.split('.');
    QJsonObject current_obj = config_;
    for (int i = 0; i < keys.size() - 1; ++i) {
        if (current_obj.contains(keys[i]) && current_obj[keys[i]].isObject()) {
            current_obj = current_obj[keys[i]].toObject();
        } else {
            return default_value;
        }
    }
    const QString last_key = keys.last();
    if (current_obj.contains(last_key)) {
        return current_obj.value(last_key);
    } else {
        return default_value;
    }
}

int SubProcessConfigManager::GetConfigVersion() const
{
    return GetValue("version", 1).toInt();
}

QString SubProcessConfigManager::GetProcessDescription() const
{
    return GetValue("description", "No description available.").toString();
}

void SubProcessConfigManager::MergeJsonObjects(QJsonObject &dest, const QJsonObject &src)
{
    for (auto it = src.constBegin(); it != src.constEnd(); ++it) {
        const QString& key = it.key();
        const QJsonValue& value = it.value();

        if (dest.contains(key) && dest[key].isObject() && value.isObject()) {
            // 如果两个值都是对象，则递归合并
            QJsonObject dest_obj = dest[key].toObject();
            MergeJsonObjects(dest_obj, value.toObject());
            dest[key] = dest_obj;
        } else {
            // 否则，直接用src的值覆盖dest的值
            dest[key] = value;
        }
    }
}
