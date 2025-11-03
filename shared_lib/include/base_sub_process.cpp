#include "base_sub_process.h"
#include <QUuid>
#include <QDateTime>
#include <QDebug>
#include "sub_process_config_manager.h"
#include "sub_process_status_reporter.h"

BaseSubProcess::BaseSubProcess(QObject* parent)
    : QObject(parent)
    , ipc_(nullptr)
    , log_storage_(nullptr)
    , state_(ProcessState::kNotInitialized) // 修正为 kNotInitialized
    , process_id_() // 初始化为空
    , config_manager_(std::make_unique<SubProcessConfigManager>(this))
    , status_reporter_(std::make_unique<SubProcessStatusReporter>(this))
{
    
}

BaseSubProcess::~BaseSubProcess()
{
}

bool BaseSubProcess::Initialize(const QJsonObject& config)
{
    if (state_ != ProcessState::kNotInitialized) {
        ErrorOccurred("Initialize called on an already initialized process.");
        return false;
    }
    SetState(ProcessState::kInitializing);

    // Load configuration
    if (!config_manager_->LoadFromJsonObject(config)) {
        ErrorOccurred("Failed to load configuration.");
        SetState(ProcessState::kError);
        return false;
    }

    // 从配置中获取子进程ID并设置
    process_id_ = config_manager_->GetValue("process_id").toString();
    if (process_id_.isEmpty()) {
        qWarning("BaseSubProcess: Process ID not found in configuration. Using default.");
        process_id_ = QUuid::createUuid().toString(QUuid::WithoutBraces); // 生成一个UUID作为默认ID
    }
    status_reporter_->SetProcessId(process_id_);

    // Pass IPC to StatusReporter if already set
    if(ipc_) {
        status_reporter_->SetIpc(ipc_);
    }

    if (OnInitialize(config)) {
        SetState(ProcessState::kInitialized);
        return true;
    } else {
        ErrorOccurred("OnInitialize returned false.");
        SetState(ProcessState::kError);
        return false;
    }
}

bool BaseSubProcess::Start()
{
    if (state_ != ProcessState::kInitialized) {
        ErrorOccurred("Start called on a process that is not initialized.");
        return false;
    }

    if (OnStart()) {
        SetState(ProcessState::kRunning);
        // Start status reporting with config value or default
        int report_interval = GetConfigManager()->GetValue("reporting.interval_ms", 5000).toInt();
        qDebug() << "Start reporting with interval:" << report_interval;
        status_reporter_->StartReporting(report_interval); // 取消注释
        emit Started(); // 取消注释
        return true;
    } else {
        ErrorOccurred("OnStart returned false.");
        SetState(ProcessState::kError);
        return false;
    }
}

void BaseSubProcess::Stop()
{
    if (state_ != ProcessState::kRunning && state_ != ProcessState::kInitialized) {
        return;
    }
    SetState(ProcessState::kStopping);
    status_reporter_->StopReporting(); // Stop reporting on stop
    OnStop();
    SetState(ProcessState::kStopped);
    emit Stopped();
}

void BaseSubProcess::HandleMessage(const IpcMessage& message)
{
    // Intercept config updates before passing to subclass
    if (message.type == MessageType::kConfigUpdate) {
        if (message.body.contains("config") && message.body["config"].isObject()) {
            config_manager_->LoadFromJsonObject(message.body["config"].toObject());
        }
    }
    OnHandleMessage(message);
}

void BaseSubProcess::SetIpc(ISubProcessIpcCommunication* ipc)
{
    ipc_ = ipc;
    if (ipc_) {
        // 在IPC模块上连接消息接收信号
        connect(ipc_, &ISubProcessIpcCommunication::MessageReceived,
                this, &BaseSubProcess::HandleMessage);
    }

    if (status_reporter_) {
        status_reporter_->SetIpc(ipc);
    }
}

void BaseSubProcess::SetLogStorage(ISubProcessLogStorage* storage)
{
    if (storage) storage->setParent(this);
    log_storage_ = storage;
}

ProcessState BaseSubProcess::GetState() const
{
    return state_;
}

ISubProcessIpcCommunication* BaseSubProcess::GetIpc() const
{
    return ipc_;
}

ISubProcessLogStorage* BaseSubProcess::GetLogStorage() const
{
    return log_storage_.data();
}

SubProcessConfigManager* BaseSubProcess::GetConfigManager() const
{
    return config_manager_.get();
}

SubProcessStatusReporter* BaseSubProcess::GetStatusReporter() const
{
    return status_reporter_.get();
}

void BaseSubProcess::SetState(ProcessState state)
{
    if (state_ == state) return;
    state_ = state;
    emit StateChanged(state_);
}


