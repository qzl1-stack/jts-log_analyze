#ifndef SSH_FILE_MANAGER_H
#define SSH_FILE_MANAGER_H

#include <QObject>
#include <QProcess>
#include <QTimer>
#include <QFileInfo>
#include <QDir>
#include <QStandardPaths>
#include <QPointer>
#include <QAbstractListModel>
#include <QDateTime>

// SSH文件信息结构
struct SshFileInfo {
    QString name;
    qint64 size;
    QDateTime modifiedTime;
    QString permissions;
    bool isDirectory;
    
    SshFileInfo() : size(0), isDirectory(false) {}
    SshFileInfo(const QString& n, qint64 s, const QDateTime& t, const QString& p, bool dir)
        : name(n), size(s), modifiedTime(t), permissions(p), isDirectory(dir) {}
};
Q_DECLARE_METATYPE(SshFileInfo)

// 文件列表模型
class SshFileListModel : public QAbstractListModel {
    Q_OBJECT
    
public:
    enum Roles {
        NameRole = Qt::UserRole + 1,
        SizeRole,
        ModifiedTimeRole,
        PermissionsRole,
        IsDirectoryRole,
        SelectedRole
    };
    
    Q_PROPERTY(int selectedCount READ getSelectedCount NOTIFY selectedCountChanged)
    Q_PROPERTY(bool hasSelection READ hasSelection NOTIFY selectedCountChanged)
    
    explicit SshFileListModel(QObject* parent = nullptr);
    
    // QAbstractListModel 接口
    int rowCount(const QModelIndex& parent = QModelIndex()) const override;
    QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;
    bool setData(const QModelIndex& index, const QVariant& value, int role = Qt::EditRole) override;
    Qt::ItemFlags flags(const QModelIndex& index) const override;
    QHash<int, QByteArray> roleNames() const override;
    
    // 自定义方法
    void setFiles(const QList<SshFileInfo>& files);
    void clear();
    Q_INVOKABLE void toggleSelection(int index);
    Q_INVOKABLE void selectAll();
    Q_INVOKABLE void clearSelection();
    Q_INVOKABLE QStringList getSelectedFiles() const;
    Q_INVOKABLE int getSelectedCount() const;
    bool hasSelection() const { return getSelectedCount() > 0; }
    
signals:
    void selectedCountChanged();
    
private:
    QList<SshFileInfo> m_files;
    QList<bool> m_selected;
};

// 下载任务信息
struct DownloadTask {
    QString remoteFile;
    QString localPath;
    qint64 totalSize;
    qint64 downloadedSize;
    QPointer<QProcess> process;
    bool completed;
    bool failed;
    QString errorMessage;
    
    DownloadTask() : totalSize(0), downloadedSize(0), completed(false), failed(false) {}
};

// SSH文件管理器主类
class SshFileManager : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool connected READ isConnected NOTIFY connectionStateChanged)
    Q_PROPERTY(bool busy READ isBusy NOTIFY busyStateChanged)
    Q_PROPERTY(QString statusMessage READ statusMessage NOTIFY statusMessageChanged)
    Q_PROPERTY(QString workDirectory READ workDirectory WRITE setWorkDirectory NOTIFY workDirectoryChanged)
    
public:
    explicit SshFileManager(QObject* parent = nullptr);
    ~SshFileManager();
    
    // 属性访问器
    bool isConnected() const { return m_connected; }
    bool isBusy() const { return m_busy; }
    QString statusMessage() const { return m_statusMessage; }
    QString workDirectory() const { return m_workDirectory; }
    
    // 设置连接参数
    void setConnectionParams(const QString& host, const QString& username = "root", 
                           const QString& password = "789521", int port = 22);
    Q_INVOKABLE void setWorkDirectory(const QString& dir) { m_workDirectory = dir; emit workDirectoryChanged(m_workDirectory); }
    
public slots:
    // 文件操作
    Q_INVOKABLE void refreshFileList();
    Q_INVOKABLE void downloadSelectedFiles(const QString& localDirectory);
    Q_INVOKABLE void cancelAllDownloads();
    
    // 连接管理
    Q_INVOKABLE void testConnection();
    Q_INVOKABLE void disconnect();

signals:
    // 连接状态信号
    void connectionStateChanged(bool connected);
    void busyStateChanged(bool busy);
    void statusMessageChanged(const QString& message);
    void workDirectoryChanged(const QString& dir);
    
    // 文件列表信号
    void fileListReady(SshFileListModel* model);
    void fileListError(const QString& error);
    
    // 下载进度信号
    void downloadStarted(const QString& fileName);
    void downloadProgress(const QString& fileName, qint64 bytesReceived, qint64 bytesTotal);
    void downloadFinished(const QString& fileName, const QString& localPath);
    void downloadFailed(const QString& fileName, const QString& error);
    void allDownloadsCompleted();
    
    // 总体进度信号
    void overallProgress(int completedFiles, int totalFiles, qint64 totalBytesReceived, qint64 totalBytesExpected);

private slots:
    void onListProcessFinished(int exitCode, QProcess::ExitStatus exitStatus);
    void onTestProcessFinished(int exitCode, QProcess::ExitStatus exitStatus);
    void onDownloadProcessFinished(int exitCode, QProcess::ExitStatus exitStatus);
    void onDownloadProcessReadyRead();
    void updateOverallProgress();

private:
    // 连接参数
    QString m_host;
    QString m_username;
    QString m_password;
    int m_port;
    
    // 状态
    bool m_connected;
    bool m_busy;
    QString m_statusMessage;
    QString m_workDirectory;
    
    // 文件列表
    SshFileListModel* m_fileListModel;
    QPointer<QProcess> m_listProcess;
    QPointer<QProcess> m_testProcess;
    
    // 下载管理
    QList<DownloadTask> m_downloadTasks;
    QTimer* m_progressTimer;
    
    // 辅助方法
    QStringList buildSshCommand(const QString& command) const;
    void parseFileListOutput(const QString& output);
    void setBusy(bool busy);
    void setStatusMessage(const QString& message);
    void setConnected(bool connected);
    void startNextDownload();
    void cleanupDownloadTasks();
    QString formatFileSize(qint64 bytes) const;
    QDateTime parseUnixTimestamp(const QString& timestamp) const;

    // 可执行文件定位
    QString locateSshExe() const;
    QString locateScpExe() const;
    QString locateSshpassExe() const;
    QProcessEnvironment buildSshProcessEnv(const QString& sshpassPath,
                                           const QString& sshExePath,
                                           const QString& scpExePath) const;
};

#endif // SSH_FILE_MANAGER_H
