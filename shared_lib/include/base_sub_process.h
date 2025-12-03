#ifndef BASE_SUB_PROCESS_H
#define BASE_SUB_PROCESS_H

#include "vta_subprocess_base_global.h"

#include <QObject>
#include <QPointer>
#include <QJsonObject>
#include <memory> // For std::unique_ptr
#include "process_state.h"
#include "message.h"
#include "i_sub_process_ipc_communication.h"
#include "sub_process_config_manager.h"

/**
 * @brief 子进程抽象基类，提供统一生命周期与状态管理
 *
 * 模板方法：
 * - Initialize() -> OnInitialize()
 * - Start() -> OnStart()
 * - Stop() -> OnStop()
 * - HandleMessage() -> OnHandleMessage()
 */
class VTA_SUBPROCESS_BASE_EXPORT BaseSubProcess : public QObject {
    Q_OBJECT

public:
    explicit BaseSubProcess(QObject* parent = nullptr);
    virtual ~BaseSubProcess();

    // 生命周期（对外）
    VTA_SUBPROCESS_BASE_EXPORT bool Initialize(const QJsonObject& config);
    VTA_SUBPROCESS_BASE_EXPORT bool Start();
    VTA_SUBPROCESS_BASE_EXPORT void Stop();

    // 消息处理（对外）
    VTA_SUBPROCESS_BASE_EXPORT void HandleMessage(const IpcMessage& message);

    // 依赖注入
    void SetIpc(ISubProcessIpcCommunication* ipc);

    // 访问器
    ProcessState GetState() const;
    ISubProcessIpcCommunication* GetIpc() const;
    SubProcessConfigManager* GetConfigManager() const;

signals:
    void StateChanged(ProcessState state);
    void Started();
    void Stopped();
    void ErrorOccurred(const QString& error_message);

protected:
    // 子类实现
    virtual bool OnInitialize(const QJsonObject& config) = 0;
    virtual bool OnStart() = 0;
    virtual void OnStop() = 0;
    virtual void OnHandleMessage(const IpcMessage& message) = 0;

    // 状态变更
    void SetState(ProcessState state);


private:
    QPointer<ISubProcessIpcCommunication> ipc_;
    ProcessState state_;

    std::unique_ptr<SubProcessConfigManager> config_manager_;
};

#endif // BASE_SUB_PROCESS_H
