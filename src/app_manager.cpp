#include "app_manager.h"
#include "textfilehandler.h"
#include "update_checker.h"
#include "tcpclient.h"
#include "log_analyzer_subprocess.h"
#include "sqlite_text_handler.h"
#include "ssh_file_manager.h"
#include "map_data_manager.h"

#include <QDebug>
#include <QGuiApplication>
#include <QQmlContext>
#include <QCoreApplication>
#include <QSysInfo>
#include <QFile>
#include <QFileInfo>
#include <QMessageBox>
#include <QTimer>
#include <QApplication>

#ifdef Q_OS_WIN
#include <windows.h> // Required for WinAPI functions
#include <Lmcons.h>  // Required for MAX_COMPUTERNAME_LENGTH
#include <TlHelp32.h> // Required for PROCESS_INFORMATION
#include <Psapi.h>   // Required for GetModuleFileNameExW
#include <ShlObj.h>  // Required for SHGetFolderPathW
#include <KnownFolders.h> // Required for FOLDERID_ProgramFilesX86
#endif

AppManager::AppManager(QObject *parent)
    : QObject(parent)
    , m_engine(nullptr)
    , m_textFileHandler(nullptr)
    , m_sqliteTextHandler(nullptr)
    , m_updateChecker(nullptr)
    , m_tcpClient(nullptr)
{
    qDebug() << "AppManager: 初始化";
}

AppManager::~AppManager()
{
    qDebug() << "AppManager: 析构";
}

void AppManager::initialize(QQmlApplicationEngine* engine)
{
    qDebug() << "AppManager: 开始初始化应用程序";
    
    m_engine = engine;
    
    // 创建各个组件
    m_textFileHandler = std::make_unique<TextFileHandler>();
    m_sqliteTextHandler = std::make_unique<SqliteTextHandler>();
    m_updateChecker = std::make_unique<UpdateChecker>();
    m_tcpClient = std::make_unique<TcpClient>();
    m_logAnalyzerSubProcess = std::make_unique<LogAnalyzerSubProcess>(this);
    m_sshFileManager = std::make_unique<SshFileManager>(this);
    m_mapDataManager = std::make_unique<MapDataManager>();
    
    // // 在暴露到QML之前完成依赖注入
    if (m_sqliteTextHandler) {
        m_mapDataManager->setDatabaseManager(m_sqliteTextHandler->dbManager());
    }
    
    connect(m_updateChecker.get(), &UpdateChecker::UpdateCheckFailed, 
            this, &AppManager::onUpdateCheckFailed);
    
    // 连接子进程的IP选择信号到AppManager的槽
    connect(m_logAnalyzerSubProcess.get(), &LogAnalyzerSubProcess::ipAddressSelected,
            this, &AppManager::onIpAddressSelected);

    // 当子进程配置更新时，同步工作目录到 SshFileManager
    connect(m_logAnalyzerSubProcess.get(), &LogAnalyzerSubProcess::workDirectoryUpdated,
            this, [this](const QString& dir) {
                if (m_sshFileManager) {
                    m_sshFileManager->setWorkDirectory(dir);
                    qDebug() << "AppManager: 已同步工作目录到 SshFileManager:" << dir;
                }
            });

    // 连接TCP客户端信号
    // 这个连接只建立一次。当连接成功时，它会自动尝试发送命令。
    connect(m_tcpClient.get(), &TcpClient::connectionStateChanged,
            this, [this](bool connected) {
                if (connected) {
                    qDebug() << "AppManager: TCP连接成功。";
                } else {
                    qDebug() << "AppManager: TCP连接已断开。";
                }
            });

    connect(m_tcpClient.get(), &TcpClient::dataSent,
            [](bool success, const QString& message) {
                qDebug() << "AppManager: TCP数据发送" << (success ? "成功" : "失败") << ":" << message;
            });

    connect(m_tcpClient.get(), &TcpClient::errorOccurred,
            [](const QString& error) {
                qWarning() << "AppManager: TCP错误:" << error;
            });
    
    // 设置 QML 上下文属性
    setupQmlContext(engine);
    
    qDebug() << "AppManager: 应用程序初始化完成";
}


void AppManager::triggerBlackBox()
{
    qDebug() << "AppManager: 触发黑盒子功能";
    
    if (!m_tcpClient) {
        qWarning() << "AppManager: TCP客户端未初始化";
        return;
    }

    if (m_selectedIpAddress.isEmpty()) {
        qWarning() << "AppManager: 尚未从子进程选择任何IP地址。";
        return;
    }

    m_tcpClient->setServerAddress(m_selectedIpAddress, 8002);

    // 检查当前连接状态
    if (m_tcpClient->isConnected()) {
        qDebug() << "AppManager: TCP已连接，直接发送命令。";
        m_tcpClient->sendTriggerBlackBoxCommand();
    } else {
        qDebug() << "AppManager: TCP未连接，尝试连接并设置单次发送任务。";
        // 如果未连接，设置一个一次性的信号槽，在连接成功后发送命令
        auto connection = std::make_shared<QMetaObject::Connection>();
        *connection = connect(m_tcpClient.get(), &TcpClient::connectionStateChanged, this,
            [this, connection](bool connected) {
                if (connected) {
                    m_tcpClient->sendTriggerBlackBoxCommand();
                }
                // 命令发送（或失败）后，断开这个一次性连接
                QObject::disconnect(*connection);
            });

        // 尝试连接服务器
        if (!m_tcpClient->connectToServer()) {
            qWarning() << "AppManager: 连接到服务器失败。";
            QObject::disconnect(*connection); // 连接失败，清理一次性信号槽
        }
    }
}

void AppManager::CheckForUpdates()
{
    qDebug() << "=== CheckForUpdates 开始执行 === (PID:" << QCoreApplication::applicationPid() << ")";

    QString updater_dir = QCoreApplication::applicationDirPath();
    QString updater_path = updater_dir + "/updater.exe";

    qDebug() << "尝试启动更新程序，路径: " << updater_path;

    if (!QFile::exists(updater_path)) {
        qWarning() << "更新程序不存在: " << updater_path;
        QMessageBox::critical(nullptr, tr("错误"), tr("未找到更新程序 (updater.exe)，请确保它与主程序在同一目录下。")); // 使用 nullptr 作为父对象
        return;
    }

    // 在启动更新程序之前，记录当前的系统和应用状态
    qDebug() << "系统信息:";
    qDebug() << "  操作系统:" << QSysInfo::productType() << QSysInfo::productVersion();
    qDebug() << "  CPU架构:" << QSysInfo::currentCpuArchitecture();
    qDebug() << "  应用程序目录:" << QCoreApplication::applicationDirPath();
    qDebug() << "  应用程序文件:" << QCoreApplication::applicationFilePath();

    // 检查更新程序的权限和属性
    QFileInfo updaterFileInfo(updater_path);
    qDebug() << "更新程序文件信息:";
    qDebug() << "  文件大小:" << updaterFileInfo.size() << "字节";
    qDebug() << "  是否可执行:" << updaterFileInfo.isExecutable();
    qDebug() << "  文件权限:"
             << (updaterFileInfo.permissions() & QFile::ReadOwner ? "可读 " : "")
             << (updaterFileInfo.permissions() & QFile::WriteOwner ? "可写 " : "")
             << (updaterFileInfo.permissions() & QFile::ExeOwner ? "可执行" : "");

    qDebug() << "即将以完全独立模式启动 updater.exe...";
    
    // 使用 WinAPI 创建一个完全独立的进程，脱离父进程的控制台
    STARTUPINFOW si;
    PROCESS_INFORMATION pi;
    ZeroMemory(&si, sizeof(si));
    si.cb = sizeof(si);
    ZeroMemory(&pi, sizeof(pi));

    // 准备命令行参数
    QString command = "\"" + updater_path + "\"";
    
    // 设置安全属性，允许子进程继承句柄
    SECURITY_ATTRIBUTES sa;
    sa.nLength = sizeof(SECURITY_ATTRIBUTES);
    sa.lpSecurityDescriptor = NULL;
    sa.bInheritHandle = TRUE;

    bool started = CreateProcessW(
        NULL,                           // 使用 lpCommandLine 中的程序名
        (LPWSTR)command.utf16(),        // 命令行
        &sa,                            // 进程安全属性
        NULL,                           // 线程安全属性
        TRUE,                           // 继承句柄
        CREATE_NEW_CONSOLE |
        NORMAL_PRIORITY_CLASS,          // 普通优先级
        NULL,                           // 使用父进程的环境变量
        (LPWSTR)updater_dir.utf16(),    // 工作目录
        &si,                            // STARTUPINFO
        &pi                             // PROCESS_INFORMATION
    );
    
    if (started) {
        qDebug() << "更新程序已成功启动，进程ID:" << pi.dwProcessId;
        
        // 关闭进程和线程句柄
        CloseHandle(pi.hProcess);
        CloseHandle(pi.hThread);
        
        qDebug() << "更新程序已成功启动，主程序即将退出";
        
        // 在退出前记录一些诊断信息
        qDebug() << "退出前系统状态:";
        qDebug() << "  活动窗口:" << QApplication::activeWindow();
        qDebug() << "  活动弹出窗口:" << QApplication::activePopupWidget();
        
        // QMessageBox::information(this, tr("更新"), tr("更新程序已启动，主程序将退出以完成更新过程。"));
        
        // 尝试优雅地退出应用程序
        QTimer::singleShot(100, this, [this]() {
            qDebug() << "正在尝试优雅退出...";
            QCoreApplication::quit();
        });
    } else {
        DWORD error = GetLastError();
        qWarning() << "启动更新程序失败，错误代码:" << error;
        
        // 获取详细的错误信息
        LPVOID lpMsgBuf;
        FormatMessage(
            FORMAT_MESSAGE_ALLOCATE_BUFFER |
            FORMAT_MESSAGE_FROM_SYSTEM |
            FORMAT_MESSAGE_IGNORE_INSERTS,
            NULL,
            error,
            MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
            (LPTSTR) &lpMsgBuf,
            0, NULL
        );

        QString errorMsg = QString::fromWCharArray((LPCWSTR)lpMsgBuf);
        LocalFree(lpMsgBuf);

        qWarning() << "详细错误信息:" << errorMsg;
        
        QMessageBox::critical(nullptr, tr("错误"), // 使用 nullptr 作为父对象
            tr("启动更新程序失败。\n错误代码: %1\n错误信息: %2")
            .arg(error)
            .arg(errorMsg)
        );
    }

    qDebug() << "=== CheckForUpdates 执行完毕 ===";
}

void AppManager::onApplicationReady()
{
    qDebug() << "AppManager: 应用程序启动完成";
    
    // 启动自动更新检查
    // QMetaObject::invokeMethod(m_updateChecker.get(), "startAutoUpdateCheck", Qt::QueuedConnection);

    // 在GUI加载完成后，再初始化和启动子进程
    if (m_logAnalyzerSubProcess) {
        qDebug() << "AppManager: 准备启动子进程服务...";

        if (m_logAnalyzerSubProcess->Initialize()) {
            m_logAnalyzerSubProcess->Start();
        } else {
            qWarning() << "AppManager: LogAnalyzerSubProcess 初始化失败。";
        }
    }
}

void AppManager::onApplicationExit()
{
    qDebug() << "AppManager: 应用程序即将退出";

    // 停止子进程服务
    if (m_logAnalyzerSubProcess) {
        qDebug() << "AppManager: 正在停止子进程服务...";
        m_logAnalyzerSubProcess->Stop();
    }
 
    // 执行清理工作
    if (m_textFileHandler) {
        m_textFileHandler->cleanupSearchThread();
    }

    if (m_sqliteTextHandler) {
        m_sqliteTextHandler->cleanupSearchThread();
    }
}


void AppManager::onIpAddressSelected(const QString &ip)
{
    m_selectedIpAddress = ip;
    qDebug() << "AppManager: 已将黑盒目标IP更新为" << m_selectedIpAddress;
    
    // 同时更新SSH文件管理器的连接参数
    if (m_sshFileManager) {
        m_sshFileManager->setConnectionParams(ip);
        qDebug() << "AppManager: 已更新SSH文件管理器连接参数";
    }
}

void AppManager::onUpdateCheckFailed(const QString& errorMessage)
{
    qWarning() << "AppManager: 更新检查失败: " << errorMessage;
}



void AppManager::setupQmlContext(QQmlApplicationEngine* engine)
{
    if (!engine) {
        qWarning() << "AppManager: QML 引擎为空，无法设置上下文属性";
        return;
    }
    
    // 设置 QML 上下文属性
    engine->rootContext()->setContextProperty("appManager", this); 
    engine->rootContext()->setContextProperty("fileHandler", m_textFileHandler.get());
    engine->rootContext()->setContextProperty("sqliteTextHandler", m_sqliteTextHandler.get());
    engine->rootContext()->setContextProperty("updateChecker", m_updateChecker.get());
    engine->rootContext()->setContextProperty("sshFileManager", m_sshFileManager.get());
    engine->rootContext()->setContextProperty("mapDataManager", m_mapDataManager.get()); // 暴露MapDataManager
}