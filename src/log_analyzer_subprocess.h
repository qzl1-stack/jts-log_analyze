#ifndef LOG_ANALYZER_SUBPROCESS_H
#define LOG_ANALYZER_SUBPROCESS_H

#include "base_sub_process.h"
#include <QTimer>
#include <QFileSystemWatcher>
#include <QStringList>
#include <QMutex>
#include <QThread>
#include <memory>
#include "local_ipc_communication.h"

/**
 * @brief 日志分析器子进程类
 */
class LogAnalyzerSubProcess : public BaseSubProcess
{
    Q_OBJECT

public:
    explicit LogAnalyzerSubProcess(QObject* parent = nullptr);
    virtual ~LogAnalyzerSubProcess();

    

signals:
    void ipAddressSelected(const QString& ip);
    void workDirectoryUpdated(const QString& workDir);

protected:
    // 继承自BaseSubProcess的虚函数实现
    bool OnInitialize(const QJsonObject& config) override;
    bool OnStart() override;
    void OnStop() override;
    void OnHandleMessage(const IpcMessage& message) override;


private:
    void SetupConfiguration();
    // 消息处理方法
    void HandleConfigUpdateMessage(const IpcMessage& message);
    void HandleCommandMessage(const IpcMessage& message);
    void HandleShutdownMessage(const IpcMessage& message);
    
    void SendErrorReport(const QString& error_message, const QString& context = QString());

private:
    // 配置参数
    int heartbeat_interval_ms_;
    // IPC通信
    std::unique_ptr<LocalIpcCommunication> ipc_communication_;
};

#endif // LOG_ANALYZER_SUBPROCESS_H