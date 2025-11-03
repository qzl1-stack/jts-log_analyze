#ifndef TEXTFILEHANDLER_H
#define TEXTFILEHANDLER_H

#include <QObject>
#include <QString>
#include <QFile>
#include <QFileDialog>
#include <QTextStream>
#include <QMessageBox>
#include <QStringConverter>
#include <QThread>
#include <QRunnable>
#include <QThreadPool>
#include <QDir>
#include <QTemporaryDir>
#include <QStandardPaths>
#include <atomic>
#include <QFileInfo>
#include <QCoreApplication>
#include <QProcess>
#include <QRegularExpression>
#include <QMutex>
#include <QTimer>
#include <memory>
#include <QPointer>
#include <QCache>
#include <QAbstractListModel>

// 搜索结果结构
struct SearchResult {
    int lineNumber;
    QString preview;
    QString fullLine;
};

// 文件元数据结构
struct FileMeta {
    QString path;      // 绝对路径
    QString name;      // 文件名
    qint64 size;       // 文件大小（字节）
    QString keyword;   // 命中的关键字
    QString category;  // 文件类别（用于分组显示）
    
    FileMeta() : size(0) {}
    FileMeta(const QString& p, const QString& n, qint64 s, const QString& k, const QString& c = QString())
        : path(p), name(n), size(s), keyword(k), category(c) {}
};

// 文件列表模型
class FileListModel : public QAbstractListModel {
    Q_OBJECT
    
public:
    enum Roles {
        PathRole = Qt::UserRole + 1,
        NameRole,
        SizeRole,
        KeywordRole,
        CategoryRole
    };
    
    explicit FileListModel(QObject* parent = nullptr);
    
    // QAbstractListModel 接口
    int rowCount(const QModelIndex& parent = QModelIndex()) const override;
    QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;
    QHash<int, QByteArray> roleNames() const override;
    
    // 自定义方法
    void setFiles(const QList<FileMeta>& files);
    void clear();
    Q_INVOKABLE QVariantMap getFile(int index) const;
    
private:
    QList<FileMeta> m_files;
};

// 多线程搜索工作类
class SearchWorker : public QObject {
    Q_OBJECT
private:
    QString m_content;
    QString m_searchText;
    int m_maxResults;
    std::atomic<bool> m_cancelled;
    QMutex m_mutex;

public:
    explicit SearchWorker(QObject *parent = nullptr);
    ~SearchWorker();
    
    void setSearchData(const QString &content, const QString &searchText, int maxResults = 100);
    void cancelSearch();

signals:
    void searchProgress(int progress);
    void searchResultReady(const QList<SearchResult> &results, const QString &highlightedContent);
    void searchFinished();
    void searchCancelled();

private slots:
    void performSearch();

public slots:
    void startSearch();
};


//文件处理流程和对外接口类
class TextFileHandler : public QObject {
    Q_OBJECT

public:
    explicit TextFileHandler(QObject *parent = nullptr);
    ~TextFileHandler() override;

public slots:
    Q_INVOKABLE void loadTextFileAsync(const QString &fileName = QString());
    Q_INVOKABLE void startAsyncSearch(const QString &content, const QString &searchText, int maxResults = 100);
    Q_INVOKABLE void cancelSearch();
    Q_INVOKABLE void cancelFileLoading();
    Q_INVOKABLE void requestFileContent(const QString& filePath);
    Q_INVOKABLE void clearFileCache();
    // 线程清理方法
    Q_INVOKABLE void cleanupSearchThread();

signals:
    void loadProgress(int progress);
    void fileLoaded(const QString &content);
    void loadError(const QString &errorMessage);
    void searchProgress(int progress);
    void searchResultReady(const QVariantList &results, const QString &highlightedContent);
    void searchFinished();
    void searchCancelled();
    
    // 新增信号
    void fileListReady(FileListModel* model);
    void fileContentReady(const QString& content, const QString& filePath);

private:
    // ZIP文件处理相关方法
    void processZipFile(const QString& zipPath);
    bool extractZipFile(const QString& zipPath, const QString& extractDir);
    QList<FileMeta> scanTextFiles(const QString& dirPath);  // 修改返回类型
    QString mergeTextFiles(const QList<FileMeta>& textFiles);  // 修改参数类型
    QString getFileKeyword(const QString& fileName);  // 新增：获取文件关键字
    QString getFileCategory(const QString& keyword);  // 新增：获取文件类别
    bool isTextFile(const QString& fileName);
    void cleanupTempFiles();
    void initializeSearchThread();
    
    void showErrorMessage(const QString &title, const QString &message);

    // 基本状态
    std::atomic<bool> m_cancelLoading;
    
    // 临时文件管理 - 使用智能指针
    std::unique_ptr<QTemporaryDir> m_tempDir;
    
    // 搜索相关 - 使用智能指针和Qt指针
    std::unique_ptr<QThread> m_searchThread;
    QPointer<SearchWorker> m_searchWorker;
    
    // 文件缓存和模型
    QCache<QString, QString> m_fileCache;  // 文件内容缓存
    FileListModel* m_fileListModel;        // 文件列表模型
};

#endif // TEXTFILEHANDLER_H 