#ifndef SQLITE_TEXT_HANDLER_H
#define SQLITE_TEXT_HANDLER_H

#include <QObject>
#include <QString>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QSqlError>
#include <QThread>
#include <QMutex>
#include <QTemporaryDir>
#include <QAbstractListModel>
#include <memory>
#include <atomic>
#include <QPointer>
#include <QFileInfo>

// 前向声明
class FileListModel;
class SearchWorker;

// 数据库文件记录结构
struct DbFileRecord {
    int id;
    QString file_path;      // 原始文件路径
    QString file_name;      // 文件名
    QString keyword;        // 文件关键字
    QString category;       // 文件类别
    QString content;        // 文件内容
    qint64 file_size;       // 文件大小
    QString zip_source;     // 来源ZIP文件
    QDateTime import_time;  // 导入时间
};

// 数据库搜索结果
struct DbSearchResult {
    int file_id;
    QString file_name;
    QString keyword;
    int line_number;
    QString line_content;
    QString preview;
    int match_position;     // 匹配位置
};

// SQLite数据库管理类
class SqliteDbManager : public QObject {
    Q_OBJECT

public:
    explicit SqliteDbManager(QObject* parent = nullptr);
    ~SqliteDbManager();

    // 数据库初始化和连接
    bool InitializeDatabase(const QString& db_path = QString());
    bool ConnectDatabase();
    void DisconnectDatabase();
    bool IsConnected() const;

    // 文件操作
    bool InsertFile(const DbFileRecord& record);
    bool InsertFiles(const QList<DbFileRecord>& records);
    bool DeleteFilesByZipSource(const QString& zip_source);
    bool DeleteAllFiles();
    QList<DbFileRecord> GetFilesByKeyword(const QString& keyword);
    QList<DbFileRecord> GetAllFiles();
    
    // 内容操作
    QString GetMergedContentByKeyword(const QString& keyword);
    QStringList GetAllKeywords();
    
    // 搜索操作
    QList<DbSearchResult> SearchInFiles(const QString& search_text, int max_results = 100);
    QList<DbSearchResult> SearchInKeyword(const QString& keyword, const QString& search_text, int max_results = 100);
    
    // 统计信息
    int GetTotalFileCount();
    qint64 GetTotalSize();
    QMap<QString, int> GetFileCountByCategory();

    // 事务操作
    bool BeginTransaction();
    bool CommitTransaction();
    bool RollbackTransaction();

signals:
    void databaseError(const QString& error);
    void progressUpdate(int progress);

private:
    // 创建数据库表和索引
    bool CreateTables();
    bool CreateIndexes();
    
    // 执行SQL查询的辅助方法
    bool ExecuteQuery(const QString& query_str);
    QSqlQuery PrepareQuery(const QString& query_str);
    
    // 处理搜索结果
    void ProcessSearchResults(QSqlQuery& query, const QString& search_text, QList<DbSearchResult>& results, int max_results);

private:
    QSqlDatabase m_database_;
    QString m_database_path_;
    mutable QMutex m_mutex_;
    bool m_is_connected_;
    static constexpr const char* k_connection_name_ = "SqliteTextHandlerConnection";
};

// 多线程数据库搜索工作类
class DbSearchWorker : public QObject {
    Q_OBJECT

public:
    explicit DbSearchWorker(SqliteDbManager* db_manager, QObject* parent = nullptr);
    ~DbSearchWorker();

    void SetSearchData(const QString& keyword, const QString& search_text, int max_results = 100);
    void SetFullSearchData(const QString& search_text, int max_results = 100);
    void CancelSearch();

public slots:
    void StartSearch();

signals:
    void searchProgress(int progress);
    void searchResultReady(const QList<DbSearchResult>& results, const QString& highlighted_content);
    void searchFinished();
    void searchCancelled();

private:
    QString HighlightSearchResults(const QString& content, const QString& search_text);
    
private:
    SqliteDbManager* m_db_manager_;
    QString m_keyword_;
    QString m_search_text_;
    int m_max_results_;
    std::atomic<bool> m_cancelled_;
    bool m_is_full_search_;  // 是否全库搜索
    QMutex m_mutex_;
};

// 主处理类 - 与TextFileHandler接口兼容
class SqliteTextHandler : public QObject {
    Q_OBJECT

public:
    explicit SqliteTextHandler(QObject* parent = nullptr);
    ~SqliteTextHandler() override;

    // 公共接口 - 与TextFileHandler保持一致
    Q_INVOKABLE void loadTextFileAsync(const QString& file_name = QString());
    Q_INVOKABLE void startAsyncSearch(const QString& content, const QString& search_text, int max_results = 100);
    Q_INVOKABLE void cancelSearch();
    Q_INVOKABLE void cancelFileLoading();
    Q_INVOKABLE void requestFileContent(const QString& file_path);
    Q_INVOKABLE void clearFileCache();
    Q_INVOKABLE void cleanupSearchThread();
    
    // 数据库特有接口
    Q_INVOKABLE bool initializeDatabase(const QString& db_path = QString());
    Q_INVOKABLE void clearDatabase();
    Q_INVOKABLE QVariantMap getDatabaseStats();

    // 仅供C++层使用的访问器（不暴露给QML）
    SqliteDbManager* dbManager() const { return m_db_manager_.get(); }

signals:
    // 与TextFileHandler兼容的信号
    void loadProgress(int progress);
    void fileLoaded(const QString& content);
    void loadError(const QString& error_message);
    void searchProgress(int progress);
    void searchResultReady(const QVariantList& results, const QString& highlighted_content);
    void searchFinished();
    void searchCancelled();
    void fileListReady(FileListModel* model);
    void fileContentReady(const QString& content, const QString& file_path);
    
    // 数据库特有信号
    void databaseInitialized();
    void databaseError(const QString& error);

private:
    // ZIP文件处理
    void ProcessZipFile(const QString& zip_path);
    bool ExtractZipFile(const QString& zip_path, const QString& extract_dir);
    QList<DbFileRecord> ScanAndImportTextFiles(const QString& dir_path, const QString& zip_source);
    
    // 文件分类和关键字提取
    QString GetFileKeyword(const QString& file_name);
    QString GetFileCategory(const QString& keyword);
    bool IsTextFile(const QString& file_name);
    
    // 清理和初始化
    void CleanupTempFiles();
    void InitializeSearchThread();
    
    // 更新文件列表模型
    void UpdateFileListModel();

private:
    // 数据库管理器
    std::unique_ptr<SqliteDbManager> m_db_manager_;
    
    // 临时文件管理
    std::unique_ptr<QTemporaryDir> m_temp_dir_;
    
    // 取消标志
    std::atomic<bool> m_cancel_loading_;
    
    // 搜索线程管理
    std::unique_ptr<QThread> m_search_thread_;
    QPointer<DbSearchWorker> m_search_worker_;
    
    // 文件列表模型
    FileListModel* m_file_list_model_;
    
    // 当前加载的关键字（用于搜索）
    QString m_current_keyword_;
};

#endif // SQLITE_TEXT_HANDLER_H
