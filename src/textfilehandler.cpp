#include "src/textfilehandler.h"
#include <QFileInfo>
#include <QUrl>
#include <QDataStream>
#include <QDebug>
#include <QProcess>
#include <QDir>
#include <QRegularExpression>
#include <QVariantList>
#include <QVariantMap>

// FileListModel 实现
FileListModel::FileListModel(QObject* parent) : QAbstractListModel(parent) {
}

int FileListModel::rowCount(const QModelIndex& parent) const {
    Q_UNUSED(parent)
    return m_files.size();
}

QVariant FileListModel::data(const QModelIndex& index, int role) const {
    if (!index.isValid() || index.row() >= m_files.size()) {
        return QVariant();
    }
    
    const FileMeta& file = m_files[index.row()];
    
    switch (role) {
        case PathRole:
            return file.path;
        case NameRole:
            return file.name;
        case SizeRole:
            return file.size;
        case KeywordRole:
            return file.keyword;
        case CategoryRole:
            return file.category;
        case Qt::DisplayRole:
            return QString("%1 (%2 KB)").arg(file.name).arg(file.size / 1024);
        default:
            return QVariant();
    }
}

QHash<int, QByteArray> FileListModel::roleNames() const {
    QHash<int, QByteArray> roles;
    roles[PathRole] = "path";
    roles[NameRole] = "name";
    roles[SizeRole] = "size";
    roles[KeywordRole] = "keyword";
    roles[CategoryRole] = "category";
    return roles;
}

void FileListModel::setFiles(const QList<FileMeta>& files) {
    beginResetModel();
    m_files = files;
    endResetModel();
}

void FileListModel::clear() {
    beginResetModel();
    m_files.clear();
    endResetModel();
}

QVariantMap FileListModel::getFile(int index) const {
    QVariantMap result;
    if (index >= 0 && index < m_files.size()) {
        const FileMeta& file = m_files[index];
        result["path"] = file.path;
        result["name"] = file.name;
        result["size"] = file.size;
        result["keyword"] = file.keyword;
        result["category"] = file.category;
    }
    return result;
}

// SearchWorker 实现
SearchWorker::SearchWorker(QObject *parent) 
    : QObject(parent), m_maxResults(100), m_cancelled(false) {
}

SearchWorker::~SearchWorker() {
    m_cancelled = true;
}

void SearchWorker::setSearchData(const QString &content, const QString &searchText, int maxResults) {
    QMutexLocker locker(&m_mutex);
    m_content = content;
    m_searchText = searchText;
    m_maxResults = maxResults;
    m_cancelled = false;
}

void SearchWorker::startSearch() {
    qDebug() << "SearchWorker::startSearch 被调用";
    QTimer::singleShot(0, this, &SearchWorker::performSearch);
}

void SearchWorker::cancelSearch() {
    qDebug() << "SearchWorker::cancelSearch 被调用";
    m_cancelled = true;
}

void SearchWorker::performSearch() {
    qDebug() << "SearchWorker::performSearch 开始执行";
    qDebug() << "内容长度:" << m_content.length() << "搜索词:" << m_searchText;
    
    if (m_content.isEmpty() || m_searchText.isEmpty()) {
        qDebug() << "内容或搜索词为空，结束搜索";
        emit searchFinished();
        return;
    }

    QList<SearchResult> results;
    QString highlightedContent;
    
    // 分割文本行
    QStringList lines = m_content.split('\n');
    int totalLines = lines.size();
    
    // 创建搜索正则表达式
    QRegularExpression searchRegex(QRegularExpression::escape(m_searchText), 
                                   QRegularExpression::CaseInsensitiveOption);
    
    int foundCount = 0;
    int processedLines = 0;
    
    // 分批处理，每批500行
    const int batchSize = 500;
    
    for (int i = 0; i < totalLines && !m_cancelled && foundCount < m_maxResults; i++) {
        const QString &line = lines[i];
        
        // 检查是否匹配
        QRegularExpressionMatch match = searchRegex.match(line);
        if (match.hasMatch()) {
            SearchResult result;
            result.lineNumber = i + 1;
            result.fullLine = line;
            result.preview = line.length() > 50 ? line.left(50) + "..." : line;
            results.append(result);
            foundCount++;
            
            // 创建高亮内容，保留原始搜索词
            QString highlightedLine = line;
            int offset = 0;
            while (true) {
                match = searchRegex.match(highlightedLine, offset);
                if (!match.hasMatch()) break;
                
                int start = match.capturedStart();
                int length = match.capturedLength();
                
                highlightedLine.replace(
                    start, length, 
                    QString("<span style=\"background-color: #DBEAFE; color: #1D4ED8; font-weight: bold;\">%1</span>")
                        .arg(highlightedLine.mid(start, length))
                );
                
                // 更新偏移量，避免重复高亮 (73 是高亮标签的近似长度)
                offset = start + length + 73; 
            }
            
            // 使用 <p> 标签包装高亮行
            highlightedContent += QString("<p style=\"margin: 0; padding: 4px 0; line-height: 1.5; border-bottom: 1px solid #F3F4F6;\">%1</p>")
                                .arg(highlightedLine.isEmpty() ? "&nbsp;" : highlightedLine);
        } else {
            // 普通行，进行HTML转义并用 <p> 包装
            QString escapedLine = line;
            escapedLine.replace("&", "&amp;")
                       .replace("<", "&lt;")
                       .replace(">", "&gt;");
            
            highlightedContent += QString("<p style=\"margin: 0; padding: 4px 0; line-height: 1.5; border-bottom: 1px solid #F3F4F6;\">%1</p>")
                                .arg(escapedLine.isEmpty() ? "&nbsp;" : escapedLine);
        }
        
        processedLines++;
        
        // 每处理一批后发送进度并检查取消
        if (processedLines % batchSize == 0) {
            int progress = (processedLines * 100) / totalLines;
            emit searchProgress(progress);
            
            if (m_cancelled) {
                emit searchCancelled();
                return;
            }
            
            // 让出CPU时间，避免长时间占用线程
            QThread::msleep(1);
        }
    }
    
    if (m_cancelled) {
        emit searchCancelled();
        return;
    }
    
    // 发送最终结果
    emit searchProgress(100);
    emit searchResultReady(results, highlightedContent);
    emit searchFinished();
}

// TextFileHandler 构造函数修改
TextFileHandler::TextFileHandler(QObject *parent) 
    : QObject(parent), m_cancelLoading(false), m_tempDir(nullptr) {
    
    qDebug() << "TextFileHandler 构造函数开始";
    
    // 初始化文件缓存 (最大50MB)
    m_fileCache.setMaxCost(50 * 1024 * 1024);
    
    // 初始化文件列表模型
    m_fileListModel = new FileListModel(this);
    
    // 初始化搜索线程和工作对象
    initializeSearchThread();
}

TextFileHandler::~TextFileHandler() {
    cleanupSearchThread();
    cleanupTempFiles();
    m_fileCache.clear();
}

void TextFileHandler::initializeSearchThread() {
    qDebug() << "初始化搜索线程";
    
    // 创建线程
    m_searchThread = std::make_unique<QThread>();
    
    // 创建工作对象
    m_searchWorker = new SearchWorker();
    m_searchWorker->moveToThread(m_searchThread.get());
    
    // 连接信号
    connect(m_searchWorker, &SearchWorker::searchProgress, 
            this, &TextFileHandler::searchProgress);
    connect(m_searchWorker, &SearchWorker::searchResultReady, 
            this, [this](const QList<SearchResult> &results, const QString &highlightedContent) {
        qDebug() << "收到搜索结果，结果数量:" << results.size();
        QVariantList variantResults;
        for (const SearchResult &result : results) {
            QVariantMap map;
            map["lineNumber"] = result.lineNumber;
            map["preview"] = result.preview;
            map["fullLine"] = result.fullLine;
            variantResults.append(map);
        }
        emit searchResultReady(variantResults, highlightedContent);
    });
    connect(m_searchWorker, &SearchWorker::searchFinished, 
            this, &TextFileHandler::searchFinished);
    connect(m_searchWorker, &SearchWorker::searchCancelled, 
            this, &TextFileHandler::searchCancelled);
    
    // 启动搜索线程
    m_searchThread->start();
    qDebug() << "搜索线程已启动";
}

void TextFileHandler::cleanupSearchThread() {
    qDebug() << "清理搜索线程";
    
    if (m_searchWorker) {
        m_searchWorker->cancelSearch();
    }
    
    if (m_searchThread && m_searchThread->isRunning()) {
        m_searchThread->quit();
        if (!m_searchThread->wait(5000)) {  // 等待5秒
            qWarning() << "搜索线程未能正常退出，强制终止";
            m_searchThread->terminate();
            m_searchThread->wait();
        }
    }
    
    // 清理工作对象
    if (m_searchWorker) {
        m_searchWorker->deleteLater();
        m_searchWorker = nullptr;
    }
    
    // 重置智能指针
    m_searchThread.reset();
    
    qDebug() << "搜索线程清理完成";
}

void TextFileHandler::startAsyncSearch(const QString &content, const QString &searchText, int maxResults) {
    qDebug() << "TextFileHandler::startAsyncSearch 被调用";
    qDebug() << "搜索词:" << searchText;
    qDebug() << "内容长度:" << content.length();
    qDebug() << "最大结果数:" << maxResults;
    
    if (m_searchWorker) {
        qDebug() << "设置搜索数据";
        m_searchWorker->setSearchData(content, searchText, maxResults);
        qDebug() << "调用 startSearch";
        QMetaObject::invokeMethod(m_searchWorker, "startSearch", Qt::QueuedConnection);//等价的信号-槽连接
                                                                                       //Qt::QueuedConnection 的意义
                                                                                       //异步执行：不会阻塞当前线程
                                                                                       //线程安全：通过Qt的事件循环机制
        qDebug() << "startSearch 调用完成";
    } else {
        qDebug() << "错误：m_searchWorker 为空";
        emit loadError("搜索工作对象未初始化");
    }
}

void TextFileHandler::cancelSearch() {
    if (m_searchWorker) {
        m_searchWorker->cancelSearch();
    }
}

void TextFileHandler::loadTextFileAsync(const QString &fileName) {
    // 重置取消标志
    m_cancelLoading = false;

    // 如果没有传入文件名，则弹出文件选择对话框
    QString selectedFileName = fileName;
    if (selectedFileName.isEmpty()) {
        selectedFileName = QFileDialog::getOpenFileName(
            nullptr,
            "选择文件",
            "",
            "支持的文件 (*.txt *.log *.md *.csv *.zip);;文本文件 (*.txt *.log *.md *.csv);;压缩文件 (*.zip);;所有文件 (*)"
        );
    } else {
        // 处理 QUrl 格式的文件路径
        QUrl url(selectedFileName);
        selectedFileName = url.toLocalFile();
    }
    
    if (selectedFileName.isEmpty()) {
        emit loadError("未选择文件");
        return;
    }

    // 检查是否为ZIP文件
    if (selectedFileName.toLower().endsWith(".zip")) {
        // 处理ZIP文件
        try {
            processZipFile(selectedFileName);
        } catch (const std::exception& e) {
            emit loadError(QString("ZIP文件处理错误：%1").arg(e.what()));
        }
        return;
    }

    // 原有的单个文本文件加载逻辑
    QThread* thread = new QThread();
    QObject* worker = new QObject();
    worker->moveToThread(thread);

    // 连接信号和槽
    connect(thread, &QThread::started, [this, selectedFileName, thread, worker]() {
        try {
            // 获取文件大小
            QFileInfo fileInfo(selectedFileName);
            qint64 fileSize = fileInfo.size();

            // 打开文件
            QFile file(selectedFileName);
            if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
                emit loadError("无法打开文件：" + selectedFileName);
                QMetaObject::invokeMethod(QThread::currentThread(), "quit", Qt::QueuedConnection);
                return;
            }
            
            // 分块读取文件
            QTextStream in(&file);
            in.setEncoding(QStringConverter::Utf8);

            QString content;
            qint64 bytesRead = 0;
            const int CHUNK_SIZE = 1024 * 1024; // 1MB 分块大小

            while (!in.atEnd() && !m_cancelLoading) {
                content += in.read(CHUNK_SIZE);
                bytesRead += CHUNK_SIZE;

                // 计算并发送进度
                int progress = qMin(100, static_cast<int>((bytesRead * 100) / fileSize));
                emit loadProgress(progress);
            }

            file.close();

            // 检查是否被取消
            if (m_cancelLoading) {
                emit loadError("文件加载已取消");
            } else {
                emit fileLoaded(content);
            }
        } catch (const std::exception& e) {
            emit loadError(QString("加载错误：%1").arg(e.what()));
        }

        // 结束线程
        QMetaObject::invokeMethod(QThread::currentThread(), "quit", Qt::QueuedConnection);
    });

    // 清理资源
    connect(thread, &QThread::finished, thread, &QThread::deleteLater);
    connect(thread, &QThread::finished, worker, &QObject::deleteLater);

    // 启动线程
    thread->start();
}

void TextFileHandler::processZipFile(const QString& zipPath) {
    emit loadProgress(10);  // 开始处理

    // 创建临时目录
    cleanupTempFiles();
    m_tempDir = std::make_unique<QTemporaryDir>();
    if (!m_tempDir->isValid()) {
        throw std::runtime_error("无法创建临时目录");
    }

    emit loadProgress(20);

    // 解压ZIP文件
    if (!extractZipFile(zipPath, m_tempDir->path())) {
        throw std::runtime_error("ZIP文件解压失败");
    }

    emit loadProgress(60);  // 解压完成

    // 扫描所有文本文件
    QList<FileMeta> allTextFiles = scanTextFiles(m_tempDir->path());
    if (allTextFiles.isEmpty()) {
        throw std::runtime_error("ZIP文件中未找到可识别的文本文件");
    }

    // 1. 文件分组
    QMap<QString, QList<FileMeta>> groupedFiles;
    for (const FileMeta& file : allTextFiles) {
        groupedFiles[file.keyword].append(file);
    }

    // 2. 内容合并、缓存并创建新的文件列表
    QList<FileMeta> modelFiles;
    for (auto it = groupedFiles.constBegin(); it != groupedFiles.constEnd(); ++it) {
        const QString& keyword = it.key();
        const QList<FileMeta>& group = it.value();

        // 2.1 内容合并
        QString mergedContent = mergeTextFiles(group);
        qint64 totalSize = mergedContent.toUtf8().size();

        // 2.2 存入缓存，使用 keyword 作为唯一的 key
        m_fileCache.insert(keyword, new QString(mergedContent), totalSize);
        qDebug() << "缓存文件组:" << keyword << "大小:" << totalSize;

        // 2.3 创建聚合后的 FileMeta
        QString displayName = group.size() > 1
            ? QString("%1 (%2 个文件)").arg(keyword).arg(group.size())
            : keyword;

        FileMeta aggregateMeta(
            keyword,                      // path -> 使用 keyword 作为唯一标识符
            displayName,                  // name -> 显示名称
            totalSize,                    // size -> 合并后的大小
            keyword,                      // keyword
            getFileCategory(keyword)      // category
        );
        modelFiles.append(aggregateMeta);
    }
    
    // 排序，让列表更美观
    std::sort(modelFiles.begin(), modelFiles.end(), [](const FileMeta& a, const FileMeta& b){
        return a.name < b.name;
    });

    emit loadProgress(80);  // 文件处理完成

    // 3. 更新文件列表模型
    m_fileListModel->setFiles(modelFiles);
    emit fileListReady(m_fileListModel);

    emit loadProgress(100);  // 全部处理完成
}

bool TextFileHandler::extractZipFile(const QString& zipPath, const QString& extractDir) {
    qDebug() << "开始解压ZIP文件:" << zipPath << "到目录:" << extractDir;
    
    // 方法1：尝试使用系统的unzip命令（跨平台）
    QProcess process;
    
#ifdef Q_OS_WIN
    // Windows系统使用PowerShell的Expand-Archive命令
    QString command = "powershell";
    QStringList arguments;
    // 转换路径格式，避免特殊字符问题
    QString normalizedZipPath = QDir::toNativeSeparators(zipPath);
    QString normalizedExtractDir = QDir::toNativeSeparators(extractDir);
    
    arguments << "-Command" 
              << QString("try { Expand-Archive -Path \"%1\" -DestinationPath \"%2\" -Force; exit 0 } catch { exit 1 }")
                 .arg(normalizedZipPath).arg(normalizedExtractDir);
#else
    // Unix/Linux/Mac系统使用unzip命令
    QString command = "unzip";
    QStringList arguments;
    arguments << "-o" << zipPath << "-d" << extractDir;
#endif

    qDebug() << "执行解压命令:" << command << arguments;
    connect(&process, &QProcess::errorOccurred, [](QProcess::ProcessError err){
        qDebug() << "QProcess error:" << err;
    });
    process.start(command, arguments);
    process.waitForFinished(30000); // 等待30秒
    
    qDebug() << "解压命令退出码:" << process.exitCode();
    qDebug() << "标准输出:" << process.readAllStandardOutput();
    qDebug() << "错误输出:" << process.readAllStandardError();
    
    if (process.exitCode() == 0) {
        // 验证是否确实解压了文件
        QDir extractedDir(extractDir);
        QStringList entries = extractedDir.entryList(QDir::Files | QDir::Dirs | QDir::NoDotAndDotDot);
        qDebug() << "解压后的文件/目录数量:" << entries.size();
        if (!entries.isEmpty()) {
            qDebug() << "解压成功，找到文件:" << entries;
            return true;
        }
    }
    else {
        qDebug() << "退出码" << process.exitCode();
    }
    
    qDebug() << "系统命令解压失败，尝试创建测试文件";
    return false; // 添加返回值
}



QList<FileMeta> TextFileHandler::scanTextFiles(const QString& dirPath) {
    qDebug() << "开始扫描文本文件，目录:" << dirPath;
    
    QList<FileMeta> textFiles;
    QDir dir(dirPath);
    
    if (!dir.exists()) {
        qDebug() << "目录不存在:" << dirPath;
        return textFiles;
    }
    
    // 首先列出所有文件和目录（不使用过滤器）
    QFileInfoList allEntries = dir.entryInfoList(QDir::Files | QDir::Dirs | QDir::NoDotAndDotDot, QDir::Name);
    qDebug() << "目录中总共有" << allEntries.size() << "个条目";
    
    for (const QFileInfo& info : allEntries) {
        qDebug() << "  条目:" << info.fileName() << (info.isDir() ? "[目录]" : "[文件]") << "大小:" << info.size();
    }
    
    // 扫描所有条目（不使用nameFilters，因为它会影响目录的遍历）
    for (const QFileInfo& fileInfo : allEntries) {
        if (fileInfo.isDir()) {
            qDebug() << "进入子目录:" << fileInfo.fileName();
            // 递归扫描子目录
            textFiles.append(scanTextFiles(fileInfo.absoluteFilePath()));
        } else {
            QString keyword = getFileKeyword(fileInfo.fileName());
            if (!keyword.isEmpty()) {
                FileMeta meta(
                    fileInfo.absoluteFilePath(),  // path
                    fileInfo.fileName(),          // name
                    fileInfo.size(),              // size
                    keyword,                      // keyword
                    getFileCategory(keyword)      // category
                );
                textFiles.append(meta);
                qDebug() << "添加文本文件:" << fileInfo.absoluteFilePath() 
                         << "关键字:" << keyword << "类别:" << meta.category;
            }
        }
    }
    
    qDebug() << "扫描完成，找到" << textFiles.size() << "个文本文件";
    return textFiles;
}

QString TextFileHandler::mergeTextFiles(const QList<FileMeta>& textFiles) {
    QString mergedContent;
    
    // 从后向前遍历列表，实现反向合并
    for (int i = textFiles.size() - 1; i >= 0; --i) {
        const FileMeta& fileMeta = textFiles[i];
        
        // 读取文件内容
        QFile file(fileMeta.path);
        if (file.open(QIODevice::ReadOnly | QIODevice::Text)) {
            QTextStream in(&file);
            in.setEncoding(QStringConverter::Utf8);
            
            QString content = in.readAll();
            mergedContent += content;
            
            file.close();
        } else {
            mergedContent += "[ 错误：无法读取文件内容 ]\n";
        }
    }
    
    return mergedContent;
}

QString TextFileHandler::getFileKeyword(const QString& fileName) {
    QString lowerFileName = fileName.toLower();
    
    // 定义关键字模式
    struct KeywordPattern {
        QString keyword;
        QRegularExpression exactRegex;
        QRegularExpression numberedRegex;
    };
    
    QList<KeywordPattern> patterns = {
        {"master", QRegularExpression("^master$"), QRegularExpression("^master\\.\\d+$")},
        {"chassis", QRegularExpression("^chassis$"), QRegularExpression("^chassis\\..*$")},
        {"guidance", QRegularExpression("^guidance$"), QRegularExpression("^guidance\\..*$")},
        {"sc2000a", QRegularExpression("^sc2000a$"), QRegularExpression("^sc2000a\\..*$")},
        {"vehicle", QRegularExpression("^vehicle$"), QRegularExpression("^vehicle\\..*$")},
        {"vehicle_navigator", QRegularExpression("^vehicle_navigator$"), QRegularExpression("^vehicle_navigator\\..*$")}
    };
    
    // 检查每个模式
    for (const auto& pattern : patterns) {
        if (pattern.exactRegex.match(lowerFileName).hasMatch() ||
            pattern.numberedRegex.match(lowerFileName).hasMatch()) {
            return pattern.keyword;
        }
    }
    
    // 检查扩展名
    QStringList textExtensions = {
        "txt", "md", "csv", "json", 
        "xml", "ini", "cfg", "yml", "yaml",
        "out", "err", "trace", "debug", "info"
    };
    
    QString extension = QFileInfo(fileName).suffix().toLower();
    if (!extension.isEmpty() && textExtensions.contains(extension)) {
        return "extension_" + extension;
    }
    
    // 检查日志关键词
    QStringList logKeywords = {
        "trace", "debug", "error", "err", "out", 
        "audit", "access", "system", "application"
    };
    
    for (const QString& keyword : logKeywords) {
        if (lowerFileName.contains(keyword)) {
            return "log_" + keyword;
        }
    }
    
    return QString(); // 未匹配
}

QString TextFileHandler::getFileCategory(const QString& keyword) {
    if (keyword.startsWith("extension_")) {
        return "通用文本文件";
    } else if (keyword.startsWith("log_")) {
        return "日志文件";
    } else if (keyword == "master") {
        return "主控文件";
    } else if (keyword == "chassis") {
        return "底盘文件";
    } else if (keyword == "guidance") {
        return "引导文件";
    } else if (keyword == "sc2000a") {
        return "SC2000A文件";
    } else if (keyword == "vehicle" || keyword == "vehicle_navigator") {
        return "车辆文件";
    }
    return "其他文件";
}

void TextFileHandler::cleanupTempFiles() {
    if (m_tempDir) {
        qDebug() << "清理临时文件目录:" << m_tempDir->path();
        m_tempDir.reset();  // 使用智能指针的reset方法
        qDebug() << "临时文件清理完成";
    }
}

void TextFileHandler::requestFileContent(const QString& filePath) {
    qDebug() << "请求文件内容:" << filePath;
    
    // 检查缓存
    if (auto cachedContent = m_fileCache.object(filePath)) {
        qDebug() << "缓存命中，直接返回内容";
        emit fileContentReady(*cachedContent, filePath);
        return;
    }
    
    qDebug() << "缓存未命中，启动异步加载";
    
    // 异步加载文件
    QThreadPool::globalInstance()->start([this, filePath]() {
        try {
            QFile file(filePath);
            if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
                QMetaObject::invokeMethod(this, [this, filePath]() {
                    emit loadError(QString("无法打开文件: %1").arg(filePath));
                }, Qt::QueuedConnection);
                return;
            }
            
            QTextStream in(&file);
            in.setEncoding(QStringConverter::Utf8);
            QString content = in.readAll();
            file.close();
            
            // 在主线程更新缓存和发出信号
            QMetaObject::invokeMethod(this, [this, filePath, content]() {
                qint64 fileSize = QFileInfo(filePath).size();
                m_fileCache.insert(filePath, new QString(content), fileSize);
                emit fileContentReady(content, filePath);
            }, Qt::QueuedConnection);
            
        } catch (const std::exception& e) {
            QMetaObject::invokeMethod(this, [this, filePath, e]() {
                emit loadError(QString("读取文件错误: %1 - %2").arg(filePath).arg(e.what()));
            }, Qt::QueuedConnection);
        }
    });
}

void TextFileHandler::clearFileCache() {
    qDebug() << "清理文件缓存";
    m_fileCache.clear();
}

void TextFileHandler::cancelFileLoading() {
    m_cancelLoading = true;
}

void TextFileHandler::showErrorMessage(const QString &title, const QString &message) {
    QMessageBox::warning(nullptr, title, message);
}