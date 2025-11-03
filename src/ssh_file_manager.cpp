#include "ssh_file_manager.h"
#include <QDebug>
#include <QRegularExpression>
#include <QStandardPaths>
#include <QDir>
#include <QFileInfo>
#include <QSaveFile>
#include <QUrl>
#include <QCoreApplication>

// ==================== SshFileListModel 实现 ====================

SshFileListModel::SshFileListModel(QObject* parent) : QAbstractListModel(parent) {
}

int SshFileListModel::rowCount(const QModelIndex& parent) const {
    Q_UNUSED(parent)
    return m_files.size();
}

QVariant SshFileListModel::data(const QModelIndex& index, int role) const {
    if (!index.isValid() || index.row() >= m_files.size()) {
        return QVariant();
    }
    
    const SshFileInfo& file = m_files[index.row()];
    
    switch (role) {
        case NameRole:
            return file.name;
        case SizeRole:
            return file.size;
        case ModifiedTimeRole:
            return file.modifiedTime;
        case PermissionsRole:
            return file.permissions;
        case IsDirectoryRole:
            return file.isDirectory;
        case SelectedRole:
            return index.row() < m_selected.size() ? m_selected[index.row()] : false;
        case Qt::DisplayRole:
            return file.name;
        default:
            return QVariant();
    }
}

bool SshFileListModel::setData(const QModelIndex& index, const QVariant& value, int role) {
    if (!index.isValid() || index.row() >= m_files.size()) {
        return false;
    }
    
    if (role == SelectedRole) {
        if (index.row() >= m_selected.size()) {
            m_selected.resize(m_files.size());
        }
        m_selected[index.row()] = value.toBool();
        emit dataChanged(index, index, {SelectedRole});
        return true;
    }
    
    return false;
}

Qt::ItemFlags SshFileListModel::flags(const QModelIndex& index) const {
    if (!index.isValid()) {
        return Qt::NoItemFlags;
    }
    return Qt::ItemIsEnabled | Qt::ItemIsSelectable;
}

QHash<int, QByteArray> SshFileListModel::roleNames() const {
    QHash<int, QByteArray> roles;
    roles[NameRole] = "name";
    roles[SizeRole] = "size";
    roles[ModifiedTimeRole] = "modifiedTime";
    roles[PermissionsRole] = "permissions";
    roles[IsDirectoryRole] = "isDirectory";
    roles[SelectedRole] = "selected";
    return roles;
}

void SshFileListModel::setFiles(const QList<SshFileInfo>& files) {
    beginResetModel();
    m_files = files;
    m_selected.clear();
    m_selected.resize(files.size());
    endResetModel();
    emit selectedCountChanged();
}

void SshFileListModel::clear() {
    beginResetModel();
    m_files.clear();
    m_selected.clear();
    endResetModel();
    emit selectedCountChanged();
}

void SshFileListModel::toggleSelection(int index) {
    if (index >= 0 && index < m_files.size()) {
        if (index >= m_selected.size()) {
            m_selected.resize(m_files.size());
        }
        m_selected[index] = !m_selected[index];
        QModelIndex modelIndex = this->index(index);
        emit dataChanged(modelIndex, modelIndex, {SelectedRole});
        emit selectedCountChanged();
    }
}

void SshFileListModel::selectAll() {
    if (m_files.isEmpty()) return;
    
    m_selected.resize(m_files.size());
    for (int i = 0; i < m_selected.size(); ++i) {
        m_selected[i] = !m_files[i].isDirectory; // 只选择文件，不选择目录
    }
    emit dataChanged(index(0), index(m_files.size() - 1), {SelectedRole});
    emit selectedCountChanged();
}

void SshFileListModel::clearSelection() {
    if (m_files.isEmpty()) return;
    
    m_selected.fill(false);
    emit dataChanged(index(0), index(m_files.size() - 1), {SelectedRole});
    emit selectedCountChanged();
}

QStringList SshFileListModel::getSelectedFiles() const {
    QStringList selected;
    for (int i = 0; i < m_files.size() && i < m_selected.size(); ++i) {
        if (m_selected[i] && !m_files[i].isDirectory) {
            selected.append(m_files[i].name);
        }
    }
    return selected;
}

int SshFileListModel::getSelectedCount() const {
    int count = 0;
    for (int i = 0; i < m_files.size() && i < m_selected.size(); ++i) {
        if (m_selected[i] && !m_files[i].isDirectory) {
            count++;
        }
    }
    return count;
}

// ==================== SshFileManager 实现 ====================

SshFileManager::SshFileManager(QObject* parent)
    : QObject(parent)
    , m_port(22)
    , m_connected(false)
    , m_busy(false)
    , m_fileListModel(new SshFileListModel(this))
    , m_progressTimer(new QTimer(this))
{
    // 设置默认连接参数
    m_username = "root";
    m_password = "789521";
    
    // 默认工作目录为用户下载目录
    m_workDirectory = QStandardPaths::writableLocation(QStandardPaths::DownloadLocation);
    if (m_workDirectory.isEmpty()) {
        m_workDirectory = QDir::homePath();
    }
    
    // 设置进度更新定时器
    m_progressTimer->setInterval(500); // 每500ms更新一次进度
    connect(m_progressTimer, &QTimer::timeout, this, &SshFileManager::updateOverallProgress);
    
    setStatusMessage("就绪");
}

SshFileManager::~SshFileManager() {
    cancelAllDownloads();
}

void SshFileManager::setConnectionParams(const QString& host, const QString& username, 
                                       const QString& password, int port) {
    m_host = host;
    m_username = username;
    m_password = password;
    m_port = port;
    
    setConnected(false);
    setStatusMessage("连接参数已更新");
}

void SshFileManager::testConnection() {
    if (m_busy) {
        qWarning() << "SshFileManager: 正在忙碌中，无法测试连接";
        return;
    }
    
    if (m_host.isEmpty()) {
        emit fileListError("主机地址为空");
        return;
    }
    
    setBusy(true);
    setStatusMessage("正在测试连接...");
    
    // 创建测试连接进程
    m_testProcess = new QProcess(this);
    connect(m_testProcess, &QProcess::errorOccurred, this, [this](QProcess::ProcessError error) {
        if (error == QProcess::FailedToStart) {
            setBusy(false);
            setStatusMessage("进程启动失败");
            QString errorMsg = "无法启动sshpass进程。请确保sshpass已安装或已正确部署。";
            qCritical() << "SshFileManager:" << errorMsg;
            emit fileListError(errorMsg);
            if(m_testProcess) {
                m_testProcess->deleteLater();
                m_testProcess = nullptr;
            }
        }
    });
    connect(m_testProcess, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
            this, &SshFileManager::onTestProcessFinished);
    
    // 使用简单的echo命令测试连接
    QString sshExe = locateSshExe();
    qDebug() << "SshFileManager: sshExe:" << sshExe;
    QString sshpassPath = locateSshpassExe();
    qDebug() << "SshFileManager: sshpassPath:" << sshpassPath;
    if (sshpassPath.isEmpty()) {
        setBusy(false);
        emit fileListError("未找到sshpass，请部署到 app/ssh_tools 或安装在MSYS2");
        return;
    }
    QStringList args = buildSshCommand("echo connection_test");
    
    qDebug() << "SshFileManager: 测试连接命令:" << args.join(" ");
    
    // 环境：注入 PATH，避免 -1073741515(缺少DLL)
    m_testProcess->setProcessEnvironment(buildSshProcessEnv(sshpassPath, sshExe, QString()));

    connect(m_testProcess, &QProcess::readyReadStandardError, this, [this]() {
        if (m_testProcess) {
            QByteArray err = m_testProcess->readAllStandardError();
            if (!err.isEmpty()) qDebug() << "ssh stderr:" << err;
        }
    });
    connect(m_testProcess, &QProcess::readyReadStandardOutput, this, [this]() {
        if (m_testProcess) {
            QByteArray out = m_testProcess->readAllStandardOutput();
            if (!out.isEmpty()) qDebug() << "ssh stdout:" << out;
        }
    });

    m_testProcess->start(sshpassPath, args);
    
    // 设置超时
    QTimer::singleShot(10000, this, [this]() {
        if (m_testProcess && m_testProcess->state() == QProcess::Running) {
            m_testProcess->kill();
            setBusy(false);
            setStatusMessage("连接超时");
            emit fileListError("连接超时");
        }
    });
}

void SshFileManager::refreshFileList() {
    if (m_busy) {
        qWarning() << "SshFileManager: 正在忙碌中，无法刷新文件列表";
        return;
    }
    
    if (m_host.isEmpty()) {
        emit fileListError("主机地址为空");
        return;
    }
    
    setBusy(true);
    setStatusMessage("正在获取文件列表...");
    
    // 创建列表进程
    m_listProcess = new QProcess(this);
    connect(m_listProcess, &QProcess::errorOccurred, this, [this](QProcess::ProcessError error) {
        if (error == QProcess::FailedToStart) {
            setBusy(false);
            setStatusMessage("进程启动失败");
            QString errorMsg = "无法启动sshpass进程。请确保sshpass已安装或已正确部署。";
            qCritical() << "SshFileManager:" << errorMsg;
            emit fileListError(errorMsg);
            if(m_listProcess) {
                m_listProcess->deleteLater();
                m_listProcess = nullptr;
            }
        }
    });
    connect(m_listProcess, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
            this, &SshFileManager::onListProcessFinished);
    
    // 使用ls命令获取文件列表，包含详细信息
    QString sshExe = locateSshExe();
    QString sshpassPath = locateSshpassExe();
    if (sshpassPath.isEmpty()) {
        setBusy(false);
        emit fileListError("未找到sshpass，请部署到 app/ssh_tools 或安装在MSYS2");
        return;
    }
    QString command = "ls -la --time-style=+%s /home/blackbox/ 2>/dev/null || echo 'ERROR: Cannot access directory'";
    QStringList args = buildSshCommand(command);
    
    qDebug() << "SshFileManager: 文件列表命令:" << args.join(" ");

    m_listProcess->setProcessEnvironment(buildSshProcessEnv(sshpassPath, sshExe, QString()));

    m_listProcess->start(sshpassPath, args);
    
    // 设置超时
    QTimer::singleShot(15000, this, [this]() {
        if (m_listProcess && m_listProcess->state() == QProcess::Running) {
            m_listProcess->kill();
            setBusy(false);
            setStatusMessage("获取文件列表超时");
            emit fileListError("获取文件列表超时");
        }
    });
}

void SshFileManager::downloadSelectedFiles(const QString& localDirectory) {
    Q_UNUSED(localDirectory)
    if (m_busy) {
        qWarning() << "SshFileManager: 正在忙碌中，无法开始下载";
        return;
    }
    
    QStringList selectedFiles = m_fileListModel->getSelectedFiles();
    if (selectedFiles.isEmpty()) {
        emit fileListError("没有选择要下载的文件");
        return;
    }
    
    // 始终使用配置透传的工作目录
    const QString targetDir = m_workDirectory.trimmed();
    if (targetDir.isEmpty()) {
        emit fileListError("工作目录未设置，请在配置中指定 work_directory");
        return;
    }
    
    // 检查本地目录
    QDir localDir(targetDir);
    if (!localDir.exists()) {
        if (!localDir.mkpath(".")) {
            emit fileListError("无法创建本地目录: " + targetDir);
            return;
        }
    }
    
    setBusy(true);
    setStatusMessage(QString("准备下载 %1 个文件到 %2...").arg(selectedFiles.size()).arg(targetDir));
    
    // 清理之前的下载任务
    cleanupDownloadTasks();
    
    // 创建下载任务
    for (const QString& fileName : selectedFiles) {
        DownloadTask task;
        task.remoteFile = fileName;
        task.localPath = QDir(targetDir).absoluteFilePath(fileName);
        
        // 从文件列表中获取文件大小
        for (int i = 0; i < m_fileListModel->rowCount(); ++i) {
            QModelIndex index = m_fileListModel->index(i);
            if (m_fileListModel->data(index, SshFileListModel::NameRole).toString() == fileName) {
                task.totalSize = m_fileListModel->data(index, SshFileListModel::SizeRole).toLongLong();
                break;
            }
        }
        
        m_downloadTasks.append(task);
    }
    
    // 开始下载
    m_progressTimer->start();
    startNextDownload();
}

void SshFileManager::cancelAllDownloads() {
    if (!m_busy) return;
    
    // 终止所有下载进程
    for (auto& task : m_downloadTasks) {
        if (task.process && task.process->state() == QProcess::Running) {
            task.process->kill();
        }
    }
    
    cleanupDownloadTasks();
    m_progressTimer->stop();
    setBusy(false);
    setStatusMessage("下载已取消");
}

void SshFileManager::disconnect() {
    cancelAllDownloads();
    
    if (m_listProcess && m_listProcess->state() == QProcess::Running) {
        m_listProcess->kill();
    }
    
    if (m_testProcess && m_testProcess->state() == QProcess::Running) {
        m_testProcess->kill();
    }
    
    setConnected(false);
    setStatusMessage("已断开连接");
}

void SshFileManager::onTestProcessFinished(int exitCode, QProcess::ExitStatus exitStatus) {
    setBusy(false);
    
    if (exitStatus == QProcess::NormalExit && exitCode == 0) {
        setConnected(true);
        setStatusMessage("连接成功");
        
        // 自动刷新文件列表
        refreshFileList();
    } else {
        setConnected(false);
        QString error = "连接失败";
        if (m_testProcess) {
            const QString err = QString::fromUtf8(m_testProcess->readAllStandardError());
            const QString out = QString::fromUtf8(m_testProcess->readAllStandardOutput());
            error += QString(" (code=%1)").arg(exitCode);
            if (!err.isEmpty()) {
                error += ": " + err.trimmed();
            } else if (!out.isEmpty()) {
                error += ": " + out.trimmed();
            }
        }
        setStatusMessage(error);
        emit fileListError(error);
    }
    
    if (m_testProcess) {
        m_testProcess->deleteLater();
        m_testProcess = nullptr;
    }
}

void SshFileManager::onListProcessFinished(int exitCode, QProcess::ExitStatus exitStatus) {
    setBusy(false);
    
    if (exitStatus == QProcess::NormalExit && exitCode == 0) {
        QString output = m_listProcess->readAllStandardOutput();
        parseFileListOutput(output);
        setStatusMessage(QString("找到 %1 个文件").arg(m_fileListModel->rowCount()));
        emit fileListReady(m_fileListModel);
    } else {
        QString error = "获取文件列表失败";
        if (m_listProcess) {
            QString errorOutput = m_listProcess->readAllStandardError();
            if (!errorOutput.isEmpty()) {
                error += ": " + errorOutput.trimmed();
            }
        }
        setStatusMessage(error);
        emit fileListError(error);
    }
    
    if (m_listProcess) {
        m_listProcess->deleteLater();
        m_listProcess = nullptr;
    }
}

void SshFileManager::onDownloadProcessFinished(int exitCode, QProcess::ExitStatus exitStatus) {
    QProcess* process = qobject_cast<QProcess*>(sender());
    if (!process) return;
    
    // 找到对应的下载任务
    DownloadTask* task = nullptr;
    for (auto& t : m_downloadTasks) {
        if (t.process == process) {
            task = &t;
            break;
        }
    }
    
    if (!task) {
        process->deleteLater();
        return;
    }
    
    if (exitStatus == QProcess::NormalExit && exitCode == 0) {
        task->completed = true;
        task->downloadedSize = task->totalSize;
        emit downloadFinished(task->remoteFile, task->localPath);
        setStatusMessage(QString("已完成: %1").arg(task->remoteFile));
    } else {
        task->failed = true;
        QString error = process->readAllStandardError();
        task->errorMessage = error.isEmpty() ? "下载失败" : error.trimmed();
        emit downloadFailed(task->remoteFile, task->errorMessage);
        setStatusMessage(QString("失败: %1").arg(task->remoteFile));
    }
    
    process->deleteLater();
    task->process = nullptr;
    
    // 检查是否所有任务都完成了
    bool allCompleted = true;
    for (const auto& t : m_downloadTasks) {
        if (!t.completed && !t.failed) {
            allCompleted = false;
            break;
        }
    }
    
    if (allCompleted) {
        m_progressTimer->stop();
        setBusy(false);
        
        int successCount = 0;
        for (const auto& t : m_downloadTasks) {
            if (t.completed) successCount++;
        }
        
        setStatusMessage(QString("下载完成: %1/%2 个文件成功").arg(successCount).arg(m_downloadTasks.size()));
        emit allDownloadsCompleted();
        
        cleanupDownloadTasks();
    } else {
        // 开始下一个下载
        startNextDownload();
    }
}

void SshFileManager::onDownloadProcessReadyRead() {
    // 这里可以实现更精确的进度监控，但scp命令的输出比较难解析
    // 目前使用定时器定期检查文件大小来估算进度
}

void SshFileManager::updateOverallProgress() {
    if (m_downloadTasks.isEmpty()) return;
    
    int completedFiles = 0;
    qint64 totalBytesReceived = 0;
    qint64 totalBytesExpected = 0;
    
    for (auto& task : m_downloadTasks) {
        if (task.completed) {
            completedFiles++;
            totalBytesReceived += task.totalSize;
        } else if (!task.failed && !task.localPath.isEmpty()) {
            // 检查本地文件大小来估算进度
            QFileInfo localFile(task.localPath);
            if (localFile.exists()) {
                task.downloadedSize = localFile.size();
                totalBytesReceived += task.downloadedSize;
                
                // 发送单个文件的进度
                if (task.totalSize > 0) {
                    emit downloadProgress(task.remoteFile, task.downloadedSize, task.totalSize);
                }
            }
        }
        totalBytesExpected += task.totalSize;
    }
    
    emit overallProgress(completedFiles, m_downloadTasks.size(), totalBytesReceived, totalBytesExpected);
}

QString SshFileManager::locateSshExe() const
{
    QStringList candidates;
    // 明确优先 MSYS2（支持D盘/C盘两种常见安装）
    // candidates << "D:/MSYS2/usr/bin/ssh.exe";
    // candidates << "D:/MSYS2/mingw64/bin/ssh.exe";
    // candidates << "C:/MSYS2/usr/bin/ssh.exe";
    // candidates << "C:/MSYS2/mingw64/bin/ssh.exe";
    candidates << QCoreApplication::applicationDirPath() + "/ssh_tools/ssh.exe";

    for (const QString& p : candidates) {
        QFileInfo fi(p);
        if (fi.exists() && fi.isFile()) {
            return QDir::toNativeSeparators(fi.absoluteFilePath());
        }
    }

    // 兜底：查 PATH，允许使用 Windows System32 OpenSSH
    const QString found = QStandardPaths::findExecutable("ssh");
    if (!found.isEmpty()) {
        return QDir::toNativeSeparators(found);
    }
    return QString();
}

QString SshFileManager::locateScpExe() const
{
    QStringList candidates;
    // candidates << "D:/MSYS2/usr/bin/scp.exe";
    // candidates << "D:/MSYS2/mingw64/bin/scp.exe";
    // candidates << "C:/MSYS2/usr/bin/scp.exe";
    // candidates << "C:/MSYS2/mingw64/bin/scp.exe";
    candidates << QCoreApplication::applicationDirPath() + "/ssh_tools/scp.exe";

    for (const QString& p : candidates) {
        QFileInfo fi(p);
        if (fi.exists() && fi.isFile()) {
            return QDir::toNativeSeparators(fi.absoluteFilePath());
        }
    }

    const QString found = QStandardPaths::findExecutable("scp");
    if (!found.isEmpty()) {
        return QDir::toNativeSeparators(found);
    }
    return QString();
}

QString SshFileManager::locateSshpassExe() const
{
    QStringList candidates;
    candidates << QCoreApplication::applicationDirPath() + "/ssh_tools/sshpass.exe";
    // candidates << "D:/MSYS2/usr/bin/sshpass.exe";
    // candidates << "C:/MSYS2/usr/bin/sshpass.exe";

    for (const QString& p : candidates) {
        QFileInfo fi(p);
        if (fi.exists() && fi.isFile()) {
            return QDir::toNativeSeparators(fi.absoluteFilePath());
        }
    }
    // 作为兜底尝试 PATH
    const QString found = QStandardPaths::findExecutable("sshpass");
    return found;
}

QProcessEnvironment SshFileManager::buildSshProcessEnv(const QString& sshpassPath,
                                                       const QString& sshExePath,
                                                       const QString& scpExePath) const
{
    QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
    QString path = env.value("PATH");
    QStringList extra;

    auto addDir = [&](const QString& exePath) {
        if (exePath.isEmpty()) return;
        QFileInfo fi(exePath);
        const QString dir = fi.absolutePath();
        if (!dir.isEmpty() && !path.contains(dir, Qt::CaseInsensitive)) {
            extra << dir;
        }
    };

    addDir(sshpassPath);
    addDir(sshExePath);
    addDir(scpExePath);
    // 不再硬编码 MSYS2 路径，部署机无 MSYS2 时避免无效 PATH 污染

    if (!extra.isEmpty()) {
        path = extra.join(";") + ";" + path;
        env.insert("PATH", path);
    }

    // 禁止使用SSH代理/公钥
    env.insert("SSH_ASKPASS", "");
    env.insert("DISPLAY", "");

    return env;
}

QStringList SshFileManager::buildSshCommand(const QString& command) const {
    QStringList args;

    // sshpass 参数
    args << "-p" << m_password;

    // 优先使用绝对路径 ssh.exe，避免依赖目标机 PATH
    {
        const QString sshExe = locateSshExe();
        args << (sshExe.isEmpty() ? QStringLiteral("ssh") : sshExe);
    }
    // args << "ssh";
    args << "-p" << QString::number(m_port);
    args << "-o" << "StrictHostKeyChecking=no";
    args << "-o" << "UserKnownHostsFile=/dev/null";
    args << "-o" << "ConnectTimeout=8";
    args << "-o" << "PreferredAuthentications=password";
    args << "-o" << "NumberOfPasswordPrompts=1";
    args << "-o" << "BatchMode=no";
    args << QString("%1@%2").arg(m_username).arg(m_host);
    args << command;

    return args;
}

void SshFileManager::parseFileListOutput(const QString& output) {
    QList<SshFileInfo> files;
    
    // 解析ls -la输出
    QStringList lines = output.split('\n', Qt::SkipEmptyParts);
    
    // 跳过第一行（总计）和当前目录/父目录条目
    for (const QString& line : lines) {
        if (line.startsWith("total") || line.contains(" . ") || line.contains(" .. ")) {
            continue;
        }
        
        // 解析ls -la格式: permissions links owner group size timestamp name
        QRegularExpression regex(R"(^([drwx-]+)\s+\d+\s+\w+\s+\w+\s+(\d+)\s+(\d+)\s+(.+)$)");
        QRegularExpressionMatch match = regex.match(line.trimmed());
        
        if (match.hasMatch()) {
            QString permissions = match.captured(1);
            qint64 size = match.captured(2).toLongLong();
            QString timestamp = match.captured(3);
            QString name = match.captured(4);
            
            // 跳过隐藏文件和特殊文件
            if (name.startsWith('.') || name.isEmpty()) {
                continue;
            }
            
            bool isDirectory = permissions.startsWith('d');
            QDateTime modTime = parseUnixTimestamp(timestamp);
            
            files.append(SshFileInfo(name, size, modTime, permissions, isDirectory));
        }
    }
    
    // 按名称排序，目录在前
    std::sort(files.begin(), files.end(), [](const SshFileInfo& a, const SshFileInfo& b) {
        if (a.isDirectory != b.isDirectory) {
            return a.isDirectory > b.isDirectory;
        }
        return a.name < b.name;
    });
    
    m_fileListModel->setFiles(files);
}

void SshFileManager::setBusy(bool busy) {
    if (m_busy != busy) {
        m_busy = busy;
        emit busyStateChanged(busy);
    }
}

void SshFileManager::setStatusMessage(const QString& message) {
    if (m_statusMessage != message) {
        m_statusMessage = message;
        emit statusMessageChanged(message);
        qDebug() << "SshFileManager:" << message;
    }
}

void SshFileManager::setConnected(bool connected) {
    if (m_connected != connected) {
        m_connected = connected;
        emit connectionStateChanged(connected);
    }
}

void SshFileManager::startNextDownload() {
    // 找到下一个未开始的下载任务
    for (auto& task : m_downloadTasks) {
        if (!task.process && !task.completed && !task.failed) {
            // 创建下载进程
            task.process = new QProcess(this);
            connect(task.process, &QProcess::errorOccurred, this, [this, &task](QProcess::ProcessError error) {
                if (error == QProcess::FailedToStart) {
                    task.failed = true;
                    task.errorMessage = "无法启动sshpass进程。请确保sshpass已安装或已正确部署。";
                    qCritical() << "SshFileManager:" << task.errorMessage;
                    emit downloadFailed(task.remoteFile, task.errorMessage);
                    onDownloadProcessFinished(-1, QProcess::CrashExit);
                }
            });
            connect(task.process, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
                    this, &SshFileManager::onDownloadProcessFinished);
            connect(task.process, &QProcess::readyReadStandardOutput,
                    this, &SshFileManager::onDownloadProcessReadyRead);
            
            // 构建scp命令
            QString sshpassPath = locateSshpassExe();
            QString scpExe = locateScpExe();
            QString sshExeForScp = locateSshExe();
            if (sshpassPath.isEmpty()) {
                task.failed = true;
                task.errorMessage = "未找到sshpass，请部署到 app/ssh_tools 或安装在MSYS2";
                emit downloadFailed(task.remoteFile, task.errorMessage);
                onDownloadProcessFinished(-1, QProcess::CrashExit);
                return;
            }
            QStringList commandArgs;
            commandArgs << "-p" << m_password;
            commandArgs << (scpExe.isEmpty() ? "scp" : scpExe);
            // 显式指定 ssh 程序，避免 MSYS scp 寻找 /usr/bin/ssh
            if (!sshExeForScp.isEmpty()) {
                commandArgs << "-S" << sshExeForScp;
            }
            commandArgs << "-P" << QString::number(m_port);
            commandArgs << "-o" << "StrictHostKeyChecking=no";
            commandArgs << "-o" << "UserKnownHostsFile=/dev/null";
            commandArgs << "-o" << "PreferredAuthentications=password";
            commandArgs << "-o" << "NumberOfPasswordPrompts=1";
            commandArgs << "-o" << "BatchMode=no";
            commandArgs << QString("%1@%2:/home/blackbox/%3").arg(m_username).arg(m_host).arg(task.remoteFile);
            commandArgs << task.localPath;

            qDebug() << "SshFileManager: 开始下载:" << task.remoteFile;
            emit downloadStarted(task.remoteFile);

            task.process->setProcessEnvironment(buildSshProcessEnv(sshpassPath, sshExeForScp, scpExe));
            task.process->start(sshpassPath, commandArgs);
            return; // 一次只启动一个下载
        }
    }
}

void SshFileManager::cleanupDownloadTasks() {
    for (auto& task : m_downloadTasks) {
        if (task.process) {
            task.process->deleteLater();
            task.process = nullptr;
        }
    }
    m_downloadTasks.clear();
}

QString SshFileManager::formatFileSize(qint64 bytes) const {
    if (bytes < 1024) {
        return QString("%1 B").arg(bytes);
    } else if (bytes < 1024 * 1024) {
        return QString("%1 KB").arg(bytes / 1024.0, 0, 'f', 1);
    } else if (bytes < 1024 * 1024 * 1024) {
        return QString("%1 MB").arg(bytes / (1024.0 * 1024.0), 0, 'f', 1);
    } else {
        return QString("%1 GB").arg(bytes / (1024.0 * 1024.0 * 1024.0), 0, 'f', 1);
    }
}

QDateTime SshFileManager::parseUnixTimestamp(const QString& timestamp) const {
    bool ok;
    qint64 unixTime = timestamp.toLongLong(&ok);
    if (ok) {
        return QDateTime::fromSecsSinceEpoch(unixTime);
    }
    return QDateTime::currentDateTime();
}
