#ifndef PROCESS_STATE_H
#define PROCESS_STATE_H
#include <QString>

/**
 * @brief 子进程运行状态
 */
enum class ProcessState {
    kNotInitialized = 0,  // 未初始化
    kInitializing,        // 初始化中
    kInitialized,         // 已初始化
    kRunning,             // 运行中
    kStopping,            // 停止中
    kStopped,             // 已停止
    kError                // 错误
};

inline QString ProcessStateToString(ProcessState state)
{
    switch (state) {
    case ProcessState::kNotInitialized: return "NOT_INITIALIZED";
    case ProcessState::kInitializing: return "INITIALIZING";
    case ProcessState::kInitialized: return "INITIALIZED";
    case ProcessState::kRunning: return "RUNNING";
    case ProcessState::kStopping: return "STOPPING";
    case ProcessState::kStopped: return "STOPPED";
    case ProcessState::kError: return "ERROR";
    default: return "UNKNOWN";
    }
}

#endif // PROCESS_STATE_H
