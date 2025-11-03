#ifndef SUB_PROCESS_STATUS_REPORTER_H
#define SUB_PROCESS_STATUS_REPORTER_H

#include "vta_subprocess_base_global.h"
#include <QObject>
#include <QJsonObject>
#include <QPointer>
#include <QTimer>
#include <QMutex>
#include "i_sub_process_ipc_communication.h"
#include "message.h"

#ifdef Q_OS_WIN 
#define NO_USER_MACROS // Prevent Windows API macros like SendMessage from conflicting with class members
#include <windows.h>
#undef SendMessage // Explicitly undefine SendMessage macro after windows.h
#endif

/**
 * @brief 子进程状态监控与报告器
 *
 * 负责定期收集子进程的运行状态、资源使用情况等，并通过IPC通道上报给主进程。
 */
class VTA_SUBPROCESS_BASE_EXPORT SubProcessStatusReporter : public QObject {
    Q_OBJECT

public:
    VTA_SUBPROCESS_BASE_EXPORT explicit SubProcessStatusReporter(QObject* parent = nullptr);
    VTA_SUBPROCESS_BASE_EXPORT virtual ~SubProcessStatusReporter();

    VTA_SUBPROCESS_BASE_EXPORT void SetIpc(ISubProcessIpcCommunication* ipc);
    VTA_SUBPROCESS_BASE_EXPORT void SetProcessId(const QString& id);
    VTA_SUBPROCESS_BASE_EXPORT void StartReporting(int interval_ms = 5000);
    VTA_SUBPROCESS_BASE_EXPORT void StopReporting();
    VTA_SUBPROCESS_BASE_EXPORT void ReportStatusNow();

protected:
    /**
     * @brief 收集状态信息，子类可以重写此函数以添加自定义的业务状态
     * @return QJsonObject 包含状态信息的JSON对象
     */
    virtual QJsonObject CollectStatus();

private slots:
    void OnReportTimerTimeout();

private:
    QPointer<ISubProcessIpcCommunication> ipc_;
    QTimer* report_timer_;
    QMutex mutex_;
    QString process_id_;

    // For CPU usage calculation (Windows specific)
    FILETIME last_kernel_time_;
    FILETIME last_user_time_;
    ULONGLONG last_system_time_;
    bool is_first_cpu_check_;
};

#endif // SUB_PROCESS_STATUS_REPORTER_H
