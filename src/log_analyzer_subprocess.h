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
#include "sub_process_status_reporter.h" // 包含状态上报类

class LogAnalyzerStatusReporter; // 前向声明

/**
 * @brief 日志分析器子进程类
 * 
 * 主要功能：
 * 1. 监控指定目录的日志文件变化
 * 2. 解析和分析日志内容
 * 3. 向主进程报告分析结果
 * 4. 处理主进程下发的分析任务
 */
class LogAnalyzerSubProcess : public BaseSubProcess
{
    Q_OBJECT

public:
    explicit LogAnalyzerSubProcess(QObject* parent = nullptr);
    virtual ~LogAnalyzerSubProcess();

    // 业务相关的公共接口
    void SetWatchDirectories(const QStringList& directories);
    void AddAnalysisPattern(const QString& pattern_name, const QString& regex_pattern);
    void RemoveAnalysisPattern(const QString& pattern_name);
    
    // 获取当前状态信息
    QJsonObject GetCurrentStatus() const;

signals:
    void ipAddressSelected(const QString& ip);
    void workDirectoryUpdated(const QString& workDir);

protected:
    // 继承自BaseSubProcess的虚函数实现
    bool OnInitialize(const QJsonObject& config) override;
    bool OnStart() override;
    void OnStop() override;
    void OnHandleMessage(const IpcMessage& message) override;


private slots:
    // 文件系统监控槽函数
    void OnFileChanged(const QString& file_path);
    void OnDirectoryChanged(const QString& directory_path);
    
    // 分析任务完成槽函数
    void OnAnalysisCompleted(const QString& file_path, const QJsonObject& results);

private:
    // 初始化相关方法
    bool InitializeFileWatcher();
    void SetupConfiguration();
    
    // 文件处理方法
    void ProcessLogFile(const QString& file_path);
    void AnalyzeLogContent(const QString& file_path, const QString& content);
    QJsonObject ExtractLogMetrics(const QString& content);
    
    // 消息处理方法
    void HandleConfigUpdateMessage(const IpcMessage& message);
    void HandleCommandMessage(const IpcMessage& message);
    void HandleShutdownMessage(const IpcMessage& message);
    
    // 状态报告方法
    void SendAnalysisReport(const QString& file_path, const QJsonObject& analysis_results);
    void SendErrorReport(const QString& error_message, const QString& context = QString());
    
    // 工具方法
    bool IsValidLogFile(const QString& file_path) const;
    QString GenerateMessageId() const;
    qint64 GetCurrentTimestamp() const;

private:
    // 文件监控
    std::unique_ptr<QFileSystemWatcher> file_watcher_;
    QStringList watch_directories_;
    QStringList supported_extensions_;
    
    // 分析模式
    QMap<QString, QString> analysis_patterns_;  // pattern_name -> regex
    
    // 配置参数
    int heartbeat_interval_ms_;
    int max_file_size_mb_;
    bool auto_analyze_enabled_;
    
    // 统计信息
    mutable QMutex stats_mutex_;
    qint64 total_files_processed_;
    qint64 total_lines_analyzed_;
    qint64 total_errors_found_;
    QDateTime last_analysis_time_;
    
    // 进程标识
    QString process_id_;

    // IPC通信
    std::unique_ptr<LocalIpcCommunication> ipc_communication_;
};


/**
 * @brief 日志分析器状态报告器
 * 
 * 继承自SubProcessStatusReporter，提供特定于日志分析器的状态信息
 */
class LogAnalyzerStatusReporter : public SubProcessStatusReporter
{
    Q_OBJECT

public:
    explicit LogAnalyzerStatusReporter(LogAnalyzerSubProcess* process, QObject* parent = nullptr);

protected:
    QJsonObject CollectStatus() override;

private:
    LogAnalyzerSubProcess* log_analyzer_process_;
};

#endif // LOG_ANALYZER_SUBPROCESS_H
