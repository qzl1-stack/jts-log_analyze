#include "sub_process_status_reporter.h"
#include <QCoreApplication>
#include <QDebug>
#include <QDateTime>

// Windows specific headers
#ifdef Q_OS_WIN
#include <windows.h>
#include <psapi.h> // For PROCESS_MEMORY_COUNTERS_EX
#endif

SubProcessStatusReporter::SubProcessStatusReporter(QObject* parent)
    : QObject(parent),
    report_timer_(new QTimer(this)),
    last_kernel_time_({0, 0}), // Initialize FILETIME structs
    last_user_time_({0, 0}),
    last_system_time_(0),
    is_first_cpu_check_(true)
{
    connect(report_timer_, &QTimer::timeout, this, &SubProcessStatusReporter::OnReportTimerTimeout);
}

SubProcessStatusReporter::~SubProcessStatusReporter()
{
}

void SubProcessStatusReporter::SetIpc(ISubProcessIpcCommunication* ipc)
{
    QMutexLocker locker(&mutex_);
    ipc_ = ipc;
}

VTA_SUBPROCESS_BASE_EXPORT void SubProcessStatusReporter::SetProcessId(const QString& id)
{
    QMutexLocker locker(&mutex_);
    process_id_ = id;
}

void SubProcessStatusReporter::StartReporting(int interval_ms)
{
    if (interval_ms <= 0) {
        qWarning("StatusReporter: Invalid reporting interval %d ms. Must be > 0.", interval_ms);
        return;
    }
    report_timer_->start(interval_ms);
}

void SubProcessStatusReporter::StopReporting()
{
    report_timer_->stop();
}

QJsonObject SubProcessStatusReporter::CollectStatus()
{
    QJsonObject status;
    status["timestamp"] = QDateTime::currentMSecsSinceEpoch();
    status["status"] = "running";

#ifdef Q_OS_WIN
    // Get CPU Usage
    FILETIME create_time, exit_time, kernel_time, user_time;
    if (GetProcessTimes(GetCurrentProcess(), &create_time, &exit_time, &kernel_time, &user_time)) {
        ULONGLONG current_kernel_time = (static_cast<ULONGLONG>(kernel_time.dwHighDateTime) << 32) | kernel_time.dwLowDateTime;
        ULONGLONG current_user_time = (static_cast<ULONGLONG>(user_time.dwHighDateTime) << 32) | user_time.dwLowDateTime;

        ULONGLONG current_system_time;
        FILETIME ft_idle, ft_kernel, ft_user;
        if (GetSystemTimes(&ft_idle, &ft_kernel, &ft_user)) {
            current_system_time = (static_cast<ULONGLONG>(ft_kernel.dwHighDateTime) << 32 | ft_kernel.dwLowDateTime) +
                                  (static_cast<ULONGLONG>(ft_user.dwHighDateTime) << 32 | ft_user.dwLowDateTime);
        }

        if (is_first_cpu_check_) {
            last_kernel_time_ = kernel_time;
            last_user_time_ = user_time;
            last_system_time_ = current_system_time;
            is_first_cpu_check_ = false;
            status["cpu_usage"] = 0.0; // Initial value
        } else {
            ULONGLONG kernel_diff = current_kernel_time - ((static_cast<ULONGLONG>(last_kernel_time_.dwHighDateTime) << 32) | last_kernel_time_.dwLowDateTime);
            ULONGLONG user_diff = current_user_time - ((static_cast<ULONGLONG>(last_user_time_.dwHighDateTime) << 32) | last_user_time_.dwLowDateTime);
            ULONGLONG system_diff = current_system_time - last_system_time_;

            if (system_diff > 0) {
                double cpu_usage = (static_cast<double>(kernel_diff + user_diff) / system_diff) * 100.0;
                status["cpu_usage"] = cpu_usage;
            } else {
                status["cpu_usage"] = 0.0;
            }
            last_kernel_time_ = kernel_time;
            last_user_time_ = user_time;
            last_system_time_ = current_system_time;
        }
    } else {
        qWarning("SubProcessStatusReporter: Failed to get process times.");
        status["cpu_usage"] = 0.0;
    }

    // Get Memory Usage
    PROCESS_MEMORY_COUNTERS_EX pmc;
    if (GetProcessMemoryInfo(GetCurrentProcess(), (PROCESS_MEMORY_COUNTERS*)&pmc, sizeof(pmc))) {
        status["memory_usage_mb"] = static_cast<double>(pmc.PrivateUsage) / (1024 * 1024); // Private bytes in MB
    } else {
        qWarning("SubProcessStatusReporter: Failed to get process memory info.");
        status["memory_usage_mb"] = 0.0;
    }
#else
    // Placeholder for non-Windows systems (e.g., Linux/macOS)
    status["cpu_usage"] = 0.0;
    status["memory_usage_mb"] = 0.0;
    qWarning("SubProcessStatusReporter: CPU/Memory usage collection not implemented for this OS.");
#endif

    return status;
}

void SubProcessStatusReporter::OnReportTimerTimeout()
{
    ReportStatusNow();
}

void SubProcessStatusReporter::ReportStatusNow()
{
    // 在持有锁的情况下检查必要条件
    {
        QMutexLocker locker(&mutex_);
        if (!ipc_ || ipc_->GetConnectionState() != ConnectionState::kConnected) {
            // qWarning("StatusReporter: IPC channel is not set or not connected. Cannot report status.");
            return;
        }
    } // 锁在这里自动释放，CollectStatus()可以在无锁状态下执行

    QJsonObject status_payload = CollectStatus(); // 收集状态（可能耗时）

    // 重新获取锁，构建并发送消息
    QMutexLocker locker(&mutex_); // 再次获取锁
    if (!ipc_ || ipc_->GetConnectionState() != ConnectionState::kConnected) {
        // 在收集状态期间，IPC状态可能发生变化，再次检查
        return;
    }

    IpcMessage status_message;
    status_message.type = MessageType::kStatusReport;
    status_message.timestamp = QDateTime::currentMSecsSinceEpoch();
    status_message.topic = "status";
    status_message.msg_id = QUuid::createUuid().toString(QUuid::WithoutBraces);
    status_message.sender_id = "log_agent";
    status_message.receiver_id = "main_process";
    status_message.body = status_payload;

    ipc_->SendMessage(status_message);
}
