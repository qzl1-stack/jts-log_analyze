#ifndef APP_MANAGER_H
#define APP_MANAGER_H

#include <QObject>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QScopedPointer>
#include <memory>
#include "log_analyzer_subprocess.h"

// 前向声明
class TextFileHandler;
class UpdateChecker;
class TcpClient;
class LogAnalyzerSubProcess;
class SqliteTextHandler;
class SshFileManager;
class MapDataManager; // 新增

class AppManager : public QObject
{
    Q_OBJECT

public:
    explicit AppManager(QObject *parent = nullptr);
    ~AppManager();
    void initialize(QQmlApplicationEngine* engine);

    Q_INVOKABLE void CheckForUpdates(); 
    
    /**
     * @brief 触发黑盒子功能
     */
    Q_INVOKABLE void triggerBlackBox();

public slots:
    void onApplicationReady();
    void onApplicationExit();

private slots:
    void onIpAddressSelected(const QString& ip);
    void onUpdateCheckFailed(const QString& errorMessage);

private:
    void setupQmlContext(QQmlApplicationEngine* engine);

private:
    QQmlApplicationEngine* m_engine; ///< QML 引擎指针

    std::unique_ptr<TextFileHandler> m_textFileHandler; ///< 文本文件处理器
    std::unique_ptr<SqliteTextHandler> m_sqliteTextHandler; ///< 数据库文本处理器
    std::unique_ptr<UpdateChecker> m_updateChecker;     ///< 更新检查器
    std::unique_ptr<TcpClient> m_tcpClient;             ///< TCP客户端
    std::unique_ptr<LogAnalyzerSubProcess> m_logAnalyzerSubProcess; ///< 子进程业务逻辑
    std::unique_ptr<SshFileManager> m_sshFileManager;   ///< SSH文件管理器
    std::unique_ptr<MapDataManager> m_mapDataManager;   ///< 地图数据管理器（对QML暴露）

    QString m_selectedIpAddress; ///< 由子进程选择的IP地址
};

#endif // APP_MANAGER_H