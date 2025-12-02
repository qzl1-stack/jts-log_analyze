#ifndef SUB_PROCESS_CONFIG_MANAGER_H
#define SUB_PROCESS_CONFIG_MANAGER_H

#include "vta_subprocess_base_global.h"
#include <QObject>
#include <QJsonObject>
#include <QJsonValue>
#include <QMutex>

/**
 * @brief 子进程配置管理器
 *
 * 负责加载、解析和提供对子进程配置的访问。
 * 支持从文件或从主进程传递的QJsonObject加载配置。
 */
class VTA_SUBPROCESS_BASE_EXPORT SubProcessConfigManager : public QObject {
    Q_OBJECT

public:
    VTA_SUBPROCESS_BASE_EXPORT explicit SubProcessConfigManager(QObject* parent = nullptr);
    VTA_SUBPROCESS_BASE_EXPORT virtual ~SubProcessConfigManager();

    VTA_SUBPROCESS_BASE_EXPORT void InitializeDefaultConfig();
    VTA_SUBPROCESS_BASE_EXPORT bool LoadFromJsonObject(const QJsonObject& config_data);
    VTA_SUBPROCESS_BASE_EXPORT QJsonObject GetConfig() const;
    VTA_SUBPROCESS_BASE_EXPORT QJsonValue GetValue(const QString& key, const QJsonValue& default_value = QJsonValue()) const;
    VTA_SUBPROCESS_BASE_EXPORT int GetConfigVersion() const;
    VTA_SUBPROCESS_BASE_EXPORT QString GetProcessDescription() const;

signals:
    /**
     * @brief 当配置更新时发出此信号
     * @param new_config 新的配置
     */
    void ConfigUpdated(const QJsonObject& new_config);

private:
    void MergeJsonObjects(QJsonObject& dest, const QJsonObject& src);
    QJsonObject config_;
    mutable QMutex mutex_;
};

#endif // SUB_PROCESS_CONFIG_MANAGER_H
