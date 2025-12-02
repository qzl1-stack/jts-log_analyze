#include "base_sub_process.h"
#include <QUuid>
#include <QDateTime>
#include <QDebug>
#include "sub_process_config_manager.h"

BaseSubProcess::BaseSubProcess(QObject* parent)
    : QObject(parent)
    , ipc_(nullptr)
    , log_storage_(nullptr)
    , state_(ProcessState::kNotInitialized) // 修正为 kNotInitialized
    , config_manager_(std::make_unique<SubProcessConfigManager>(this))
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
    OnStop();
    SetState(ProcessState::kStopped);
    emit Stopped();
}

void BaseSubProcess::HandleMessage(const IpcMessage& message)
{
    // Intercept config updates before passing to subclass
    qDebug() << "Hotconfig:" << message.body;
    if (message.type == MessageType::kConfigUpdate) {
        if (message.body.contains("updated_config") && message.body["updated_config"].isObject()) {
            config_manager_->LoadFromJsonObject(message.body);
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

void BaseSubProcess::SetState(ProcessState state)
{
    if (state_ == state) return;
    state_ = state;
    emit StateChanged(state_);
}


