#include <QApplication>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QUrl>
#include <QObject>
#include <QTimer>
#include <QQmlComponent>
#include <QtQml>
#include <QQuickItem>
#include <QFileInfo>
#include <QDateTime>
#include "app_manager.h"
#include <QDebug>
#include "textfilehandler.h"
#include "ssh_file_manager.h"
#include "map_data_manager.h"

// 声明 VehicleReviewPage 类
class VehicleReviewPage : public QQuickItem {
    Q_OBJECT
public:
    explicit VehicleReviewPage(QQuickItem *parent = nullptr) : QQuickItem(parent) {
        setFlag(ItemHasContents, true);  // 关键设置
        setVisible(true);  // 显式设置可见
    }
};

// 清理日志文件：如果文件超过指定大小限制，则清空文件
void cleanupLogFile(const QString &logFilePath, qint64 maxSizeBytes = 10 * 1024 * 1024) {
    QFileInfo fileInfo(logFilePath);
    if (fileInfo.exists() && fileInfo.size() > maxSizeBytes) {
        QFile logFile(logFilePath);
        if (logFile.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text)) {
            QTextStream stream(&logFile);
            stream << QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss.zzz") 
                   << " [Info] 日志文件已清理（超过大小限制 " 
                   << (maxSizeBytes / 1024 / 1024) << "MB）" << Qt::endl;
            logFile.close();
        }
    }
}

void customMessageOutput(QtMsgType type, const QMessageLogContext &context, const QString &msg)
{
    // 确保日志目录存在
    QDir logDir(QCoreApplication::applicationDirPath() + "/logs");
    if (!logDir.exists()) {
        logDir.mkpath(".");
    }

    // 创建日志文件路径
    QString logFilePath = logDir.filePath("logagent_log.txt");
    
    // 定期清理日志文件（如果超过10MB则清空）
    cleanupLogFile(logFilePath, 10 * 1024 * 1024);  // 10MB
    
    QFile logFile(logFilePath);
    
    if (logFile.open(QIODevice::WriteOnly | QIODevice::Append | QIODevice::Text)) {
        QTextStream stream(&logFile);
        
        QString logString;
        QTextStream logStream(&logString);
        logStream << QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss.zzz") << " ";
        logStream << msg ;
        
        stream << logString << Qt::endl; // 写入到文件
        logFile.close();
        
        // 同时输出到标准错误流
        fprintf(stderr, "%s\n", logString.toLocal8Bit().constData());
        fflush(stderr);
    }

}   

void signalHandler(int signum) {
    qDebug() << "捕获到信号" << signum << "，正在准备退出...";
    
    // 通知应用程序退出
    QCoreApplication::quit();
}

int main(int argc, char *argv[])
{
    qInstallMessageHandler(customMessageOutput);
    QApplication app(argc, argv);
    app.setApplicationName("车辆分析器");
    app.setApplicationVersion("1.0.2");

    qmlRegisterType<VehicleReviewPage>("Log_analyzer", 1, 0, "VehicleReviewPage");

    // 注册 FileListModel 到 QML 和元类型系统
    qmlRegisterType<FileListModel>("Log_analyzer", 1, 0, "FileListModel");
    qRegisterMetaType<FileListModel*>("FileListModel*");

    qmlRegisterType<SshFileListModel>("Log_analyzer", 1, 0, "SshFileListModel");
    qRegisterMetaType<SshFileListModel*>("SshFileListModel*");
    qRegisterMetaType<SshFileInfo>("SshFileInfo");

    qmlRegisterType<MapDataManager>("Log_analyzer", 1, 0, "MapDataManager");
    qRegisterMetaType<MapDataManager*>("MapDataManager*");

    auto appManager = std::make_unique<AppManager>();

    QQmlApplicationEngine engine;

    appManager->initialize(&engine);
    
    const QUrl url(QStringLiteral("qrc:/qml/Main.qml"));
    QObject::connect(&engine, &QQmlApplicationEngine::objectCreated,
                     &app, [url, &appManager](QObject *obj, const QUrl &objUrl) {
        if (!obj && url == objUrl) {
            QCoreApplication::exit(-1);
        } else if (obj && url == objUrl) {
            // 应用程序启动完成
            QTimer::singleShot(100, appManager.get(), &AppManager::onApplicationReady);
        }
    }, Qt::QueuedConnection);
    
    engine.load(url);

    signal(SIGTERM, signalHandler); // terminate() 发送的信号
    signal(SIGINT, signalHandler);  // Ctrl+C 发送的信号
    
    // 连接应用程序退出信号
    QObject::connect(&app, &QApplication::aboutToQuit, 
                     appManager.get(), &AppManager::onApplicationExit);

    return app.exec();
}

#include "main.moc"


