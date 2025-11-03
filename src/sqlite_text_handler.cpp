#include "sqlite_text_handler.h"
#include "textfilehandler.h"  // 复用FileListModel
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QSqlError>
#include <QSqlRecord>
#include <QFileDialog>
#include <QFile>
#include <QTextStream>
#include <QProcess>
#include <QDir>
#include <QDebug>
#include <QRegularExpression>
#include <QVariantList>
#include <QVariantMap>
#include <QStandardPaths>
#include <QDateTime>
#include <QTimer>
#include <QUrl>
#include <QStringConverter>
#include <algorithm>


SqliteDbManager::SqliteDbManager(QObject* parent)
    : QObject(parent), m_is_connected_(false) {
}

SqliteDbManager::~SqliteDbManager() {
    DisconnectDatabase();
}

bool SqliteDbManager::InitializeDatabase(const QString& db_path) {
    QMutexLocker locker(&m_mutex_);
    
    // 设置数据库路径
    if (db_path.isEmpty()) {
        // 默认在用户文档目录创建数据库
        QString docs_path = QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation);
        m_database_path_ = docs_path + "/log_analyzer_data.db";
    } else {
        m_database_path_ = db_path;
    }
    
    qDebug() << "初始化数据库，路径：" << m_database_path_;
    
    // 连接数据库
    if (!ConnectDatabase()) {
        return false;
    }
    
    // 创建表和索引
    if (!CreateTables()) {
        DisconnectDatabase();
        return false;
    }
    
    if (!CreateIndexes()) {
        DisconnectDatabase();
        return false;
    }
    
    qDebug() << "数据库初始化成功";
    return true;
}

bool SqliteDbManager::ConnectDatabase() {
    if (m_is_connected_) {
        return true;
    }
    
    // 创建SQLite连接
    m_database_ = QSqlDatabase::addDatabase("QSQLITE", k_connection_name_);
    m_database_.setDatabaseName(m_database_path_);
    
    if (!m_database_.open()) {
        qCritical() << "无法打开数据库：" << m_database_.lastError().text();
        emit databaseError(m_database_.lastError().text());
        return false;
    }
    
    // 设置SQLite优化参数
    QSqlQuery query(m_database_);
    query.exec("PRAGMA journal_mode = WAL");       // 启用WAL模式，提高并发性能
    query.exec("PRAGMA synchronous = NORMAL");     // 平衡性能和安全性
    query.exec("PRAGMA cache_size = 10000");       // 增加缓存大小
    query.exec("PRAGMA temp_store = MEMORY");      // 临时表存储在内存
    
    m_is_connected_ = true;
    qDebug() << "数据库连接成功";
    return true;
}

void SqliteDbManager::DisconnectDatabase() {
    QMutexLocker locker(&m_mutex_);
    
    if (m_is_connected_) {
        m_database_.close();
        QSqlDatabase::removeDatabase(k_connection_name_);
        m_is_connected_ = false;
        qDebug() << "数据库连接已关闭";
    }
}

bool SqliteDbManager::IsConnected() const {
    return m_is_connected_;
}

bool SqliteDbManager::CreateTables() {
    // 创建文件表
    QString create_files_table = R"(
        CREATE TABLE IF NOT EXISTS files (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            file_path TEXT NOT NULL,
            file_name TEXT NOT NULL,
            keyword TEXT NOT NULL,
            category TEXT,
            content TEXT,
            file_size INTEGER,
            zip_source TEXT,
            import_time TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
            UNIQUE(file_path, zip_source)
        )
    )";
    
    if (!ExecuteQuery(create_files_table)) {
        qCritical() << "创建files表失败";
        return false;
    }
    
    // 创建搜索历史表（可选，用于优化常用搜索）
    QString create_search_history = R"(
        CREATE TABLE IF NOT EXISTS search_history (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            search_text TEXT NOT NULL,
            search_time TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
            result_count INTEGER
        )
    )";
    
    if (!ExecuteQuery(create_search_history)) {
        qCritical() << "创建search_history表失败";
        return false;
    }
    
    qDebug() << "数据库表创建成功";
    return true;
}

bool SqliteDbManager::CreateIndexes() {
    // 创建关键字索引
    ExecuteQuery("CREATE INDEX IF NOT EXISTS idx_keyword ON files(keyword)");
    
    // 创建类别索引
    ExecuteQuery("CREATE INDEX IF NOT EXISTS idx_category ON files(category)");
    
    // 创建ZIP源索引
    ExecuteQuery("CREATE INDEX IF NOT EXISTS idx_zip_source ON files(zip_source)");
    
    // 创建全文搜索索引（使用FTS5）
    QString create_fts = R"(
        CREATE VIRTUAL TABLE IF NOT EXISTS files_fts USING fts5(
            file_name, 
            content,
            content=files,
            content_rowid=id
        )
    )";
    
    if (!ExecuteQuery(create_fts)) {
        qWarning() << "创建全文搜索索引失败（可能不支持FTS5）";
        // 不是致命错误，可以继续使用LIKE搜索
    }
    
    qDebug() << "数据库索引创建成功";
    return true;
}

bool SqliteDbManager::ExecuteQuery(const QString& query_str) {
    QSqlQuery query(m_database_);
    if (!query.exec(query_str)) {
        qCritical() << "SQL执行失败：" << query.lastError().text();
        qCritical() << "SQL语句：" << query_str;
        emit databaseError(query.lastError().text());
        return false;
    }
    return true;
}

QSqlQuery SqliteDbManager::PrepareQuery(const QString& query_str) {
    QSqlQuery query(m_database_);
    query.prepare(query_str);
    return query;
}

bool SqliteDbManager::BeginTransaction() {
    return m_database_.transaction();
}

bool SqliteDbManager::CommitTransaction() {
    return m_database_.commit();
}

bool SqliteDbManager::RollbackTransaction() {
    return m_database_.rollback();
}

bool SqliteDbManager::InsertFile(const DbFileRecord& record) {
    QMutexLocker locker(&m_mutex_);
    
    QSqlQuery query = PrepareQuery(R"(
        INSERT OR REPLACE INTO files 
        (file_path, file_name, keyword, category, content, file_size, zip_source, import_time)
        VALUES (?, ?, ?, ?, ?, ?, ?, ?)
    )");
    
    query.addBindValue(record.file_path);
    query.addBindValue(record.file_name);
    query.addBindValue(record.keyword);
    query.addBindValue(record.category);
    query.addBindValue(record.content);
    query.addBindValue(record.file_size);
    query.addBindValue(record.zip_source);
    query.addBindValue(record.import_time);
    
    if (!query.exec()) {
        qCritical() << "插入文件记录失败：" << query.lastError().text();
        emit databaseError(query.lastError().text());
        return false;
    }
    
    return true;
}

bool SqliteDbManager::InsertFiles(const QList<DbFileRecord>& records) {
    QMutexLocker locker(&m_mutex_);
    
    if (!BeginTransaction()) {
        return false;
    }
    
    QSqlQuery query = PrepareQuery(R"(
        INSERT OR REPLACE INTO files 
        (file_path, file_name, keyword, category, content, file_size, zip_source, import_time)
        VALUES (?, ?, ?, ?, ?, ?, ?, ?)
    )");
    
    int progress = 0;
    int total = records.size();
    
    for (const auto& record : records) {
        query.addBindValue(record.file_path);
        query.addBindValue(record.file_name);
        query.addBindValue(record.keyword);
        query.addBindValue(record.category);
        query.addBindValue(record.content);
        query.addBindValue(record.file_size);
        query.addBindValue(record.zip_source);
        query.addBindValue(record.import_time);
        
        if (!query.exec()) {
            qCritical() << "批量插入失败：" << query.lastError().text();
            RollbackTransaction();
            return false;
        }
        
        progress++;
        if (progress % 10 == 0) {
            emit progressUpdate((progress * 100) / total);
        }
    }
    
    if (!CommitTransaction()) {
        return false;
    }
    
    emit progressUpdate(100);
    qDebug() << "成功插入" << records.size() << "条文件记录";
    return true;
}

bool SqliteDbManager::DeleteAllFiles() {
    QMutexLocker locker(&m_mutex_);
    
    QSqlQuery query = PrepareQuery("DELETE FROM files");
    
    if (!query.exec()) {
        qCritical() << "删除所有文件记录失败：" << query.lastError().text();
        return false;
    }
    
    qDebug() << "删除了" << query.numRowsAffected() << "条记录";
    
    // 重置自增ID
    QSqlQuery reset_sequence_query = PrepareQuery("DELETE FROM sqlite_sequence WHERE name='files'");
    if (!reset_sequence_query.exec()) {
        qWarning() << "重置sqlite_sequence失败：" << reset_sequence_query.lastError().text();
    }
    
    return true;
}

QList<DbFileRecord> SqliteDbManager::GetFilesByKeyword(const QString& keyword) {
    QMutexLocker locker(&m_mutex_);
    
    QList<DbFileRecord> records;
    
    QSqlQuery query = PrepareQuery("SELECT * FROM files WHERE keyword = ?");
    query.addBindValue(keyword);
    
    if (!query.exec()) {
        qCritical() << "查询失败：" << query.lastError().text();
        return records;
    }
    
    while (query.next()) {
        DbFileRecord record;
        record.id = query.value("id").toInt();
        record.file_path = query.value("file_path").toString();
        record.file_name = query.value("file_name").toString();
        record.keyword = query.value("keyword").toString();
        record.category = query.value("category").toString();
        record.content = query.value("content").toString();
        record.file_size = query.value("file_size").toLongLong();
        record.zip_source = query.value("zip_source").toString();
        record.import_time = query.value("import_time").toDateTime();
        records.append(record);
    }
    
    return records;
}

QList<DbFileRecord> SqliteDbManager::GetAllFiles() {
    QMutexLocker locker(&m_mutex_);
    
    QList<DbFileRecord> records;
    
    QSqlQuery query(m_database_);
    if (!query.exec("SELECT * FROM files ORDER BY keyword, file_name")) {
        qCritical() << "查询失败：" << query.lastError().text();
        return records;
    }
    
    while (query.next()) {
        DbFileRecord record;
        record.id = query.value("id").toInt();
        record.file_path = query.value("file_path").toString();
        record.file_name = query.value("file_name").toString();
        record.keyword = query.value("keyword").toString();
        record.category = query.value("category").toString();
        record.content = query.value("content").toString();
        record.file_size = query.value("file_size").toLongLong();
        record.zip_source = query.value("zip_source").toString();
        record.import_time = query.value("import_time").toDateTime();
        records.append(record);
    }
    
    return records;
}

QString SqliteDbManager::GetMergedContentByKeyword(const QString& keyword) {
    QMutexLocker locker(&m_mutex_);
    
    QString merged_content;
    
    QSqlQuery query = PrepareQuery(
        "SELECT content FROM files WHERE keyword = ? ORDER BY file_name DESC"
    );
    query.addBindValue(keyword);
    
    if (!query.exec()) {
        qCritical() << "查询内容失败：" << query.lastError().text();
        return merged_content;
    }
    
    while (query.next()) {
        merged_content += query.value("content").toString();
    }
    
    return merged_content;
}

QStringList SqliteDbManager::GetAllKeywords() {
    QMutexLocker locker(&m_mutex_);
    
    QStringList keywords;
    
    QSqlQuery query(m_database_);
    if (!query.exec("SELECT DISTINCT keyword FROM files ORDER BY keyword")) {
        qCritical() << "查询关键字失败：" << query.lastError().text();
        return keywords;
    }
    
    while (query.next()) {
        keywords.append(query.value(0).toString());
    }
    
    return keywords;
}

QList<DbSearchResult> SqliteDbManager::SearchInFiles(const QString& search_text, int max_results) {
    QMutexLocker locker(&m_mutex_);
    
    QList<DbSearchResult> results;
    
    // 尝试使用全文搜索
    QSqlQuery fts_query = PrepareQuery(R"(
        SELECT files.id, files.file_name, files.keyword, files.content
        FROM files 
        JOIN files_fts ON files.id = files_fts.rowid
        WHERE files_fts MATCH ?
        LIMIT ?
    )");
    
    fts_query.addBindValue(search_text);
    fts_query.addBindValue(max_results);
    
    bool use_fts = fts_query.exec();
    
    if (!use_fts) {
        // 回退到LIKE搜索
        QSqlQuery like_query = PrepareQuery(R"(
            SELECT id, file_name, keyword, content
            FROM files
            WHERE content LIKE ?
            LIMIT ?
        )");
        
        like_query.addBindValue("%" + search_text + "%");
        like_query.addBindValue(max_results);
        
        if (!like_query.exec()) {
            qCritical() << "搜索失败：" << like_query.lastError().text();
            return results;
        }
        
        ProcessSearchResults(like_query, search_text, results, max_results);
    } else {
        ProcessSearchResults(fts_query, search_text, results, max_results);
    }
    
    return results;
}

QList<DbSearchResult> SqliteDbManager::SearchInKeyword(const QString& keyword, const QString& search_text, int max_results) {
    QMutexLocker locker(&m_mutex_);
    
    QList<DbSearchResult> results;
    
    QSqlQuery query = PrepareQuery(R"(
        SELECT id, file_name, keyword, content
        FROM files
        WHERE keyword = ? AND content LIKE ?
        LIMIT ?
    )");
    
    query.addBindValue(keyword);
    query.addBindValue("%" + search_text + "%");
    query.addBindValue(max_results);
    
    if (!query.exec()) {
        qCritical() << "搜索失败：" << query.lastError().text();
        return results;
    }
    
    ProcessSearchResults(query, search_text, results, max_results);
    
    return results;
}

void SqliteDbManager::ProcessSearchResults(QSqlQuery& query, const QString& search_text, QList<DbSearchResult>& results, int max_results) {
    QRegularExpression search_regex(QRegularExpression::escape(search_text), 
                                   QRegularExpression::CaseInsensitiveOption);
    
    while (query.next()) {
        int file_id = query.value(0).toInt();
        QString file_name = query.value(1).toString();
        QString keyword = query.value(2).toString();
        QString content = query.value(3).toString();
        
        // 分割内容为行并查找匹配
        QStringList lines = content.split('\n');
        for (int i = 0; i < lines.size(); ++i) {
            const QString& line = lines[i];
            QRegularExpressionMatch match = search_regex.match(line);
            
            if (match.hasMatch()) {
                DbSearchResult result;
                result.file_id = file_id;
                result.file_name = file_name;
                result.keyword = keyword;
                result.line_number = i + 1;
                result.line_content = line;
                result.preview = line.length() > 50 ? line.left(50) + "..." : line;
                result.match_position = match.capturedStart();
                
                results.append(result);
                
                if (results.size() >= max_results) {
                    return;
                }
            }
        }
    }
}

int SqliteDbManager::GetTotalFileCount() {
    QMutexLocker locker(&m_mutex_);
    
    QSqlQuery query(m_database_);
    if (query.exec("SELECT COUNT(*) FROM files") && query.next()) {
        return query.value(0).toInt();
    }
    return 0;
}

qint64 SqliteDbManager::GetTotalSize() {
    QMutexLocker locker(&m_mutex_);
    
    QSqlQuery query(m_database_);
    if (query.exec("SELECT SUM(file_size) FROM files") && query.next()) {
        return query.value(0).toLongLong();
    }
    return 0;
}

QMap<QString, int> SqliteDbManager::GetFileCountByCategory() {
    QMutexLocker locker(&m_mutex_);
    
    QMap<QString, int> counts;
    
    QSqlQuery query(m_database_);
    if (query.exec("SELECT category, COUNT(*) FROM files GROUP BY category")) {
        while (query.next()) {
            counts[query.value(0).toString()] = query.value(1).toInt();
        }
    }
    
    return counts;
}


// ==================== DbSearchWorker 实现 ====================

DbSearchWorker::DbSearchWorker(SqliteDbManager* db_manager, QObject* parent)
    : QObject(parent)
    , m_db_manager_(db_manager)
    , m_max_results_(100)
    , m_cancelled_(false)
    , m_is_full_search_(false) {
}

DbSearchWorker::~DbSearchWorker() {
    m_cancelled_ = true;
}

void DbSearchWorker::SetSearchData(const QString& keyword, const QString& search_text, int max_results) {
    QMutexLocker locker(&m_mutex_);
    m_keyword_ = keyword;
    m_search_text_ = search_text;
    m_max_results_ = max_results;
    m_is_full_search_ = false;
    m_cancelled_ = false;
}

void DbSearchWorker::SetFullSearchData(const QString& search_text, int max_results) {
    QMutexLocker locker(&m_mutex_);
    m_keyword_.clear();
    m_search_text_ = search_text;
    m_max_results_ = max_results;
    m_is_full_search_ = true;
    m_cancelled_ = false;
}

void DbSearchWorker::CancelSearch() {
    m_cancelled_ = true;
}

void DbSearchWorker::StartSearch() {
    qDebug() << "开始数据库搜索，搜索词：" << m_search_text_;
    
    if (m_search_text_.isEmpty()) {
        emit searchFinished();
        return;
    }
    
    // 执行数据库搜索
    QList<DbSearchResult> results;
    
    if (m_is_full_search_) {
        results = m_db_manager_->SearchInFiles(m_search_text_, m_max_results_);
    } else if (!m_keyword_.isEmpty()) {
        results = m_db_manager_->SearchInKeyword(m_keyword_, m_search_text_, m_max_results_);
    }
    
    if (m_cancelled_) {
        emit searchCancelled();
        return;
    }
    
    // 生成高亮内容
    QString highlighted_content;
    if (!m_keyword_.isEmpty()) {
        QString content = m_db_manager_->GetMergedContentByKeyword(m_keyword_);
        highlighted_content = HighlightSearchResults(content, m_search_text_);
    }
    
    emit searchResultReady(results, highlighted_content);
    emit searchFinished();
}

QString DbSearchWorker::HighlightSearchResults(const QString& content, const QString& search_text) {
    QString highlighted;
    QStringList lines = content.split('\n');
    QRegularExpression search_regex(QRegularExpression::escape(search_text), 
                                   QRegularExpression::CaseInsensitiveOption);
    
    for (const QString& line : lines) {
        QString highlighted_line = line;
        QRegularExpressionMatch match = search_regex.match(line);
        
        if (match.hasMatch()) {
            // 高亮匹配的文本
            int offset = 0;
            while (true) {
                match = search_regex.match(highlighted_line, offset);
                if (!match.hasMatch()) break;
                
                int start = match.capturedStart();
                int length = match.capturedLength();
                
                highlighted_line.replace(
                    start, length,
                    QString("<span style=\"background-color: #DBEAFE; color: #1D4ED8; font-weight: bold;\">%1</span>")
                        .arg(highlighted_line.mid(start, length))
                );
                
                offset = start + length + 73; // 跳过高亮标签
            }
        } else {
            // HTML转义
            highlighted_line.replace("&", "&amp;")
                           .replace("<", "&lt;")
                           .replace(">", "&gt;");
        }
        
        highlighted += QString("<p style=\"margin: 0; padding: 4px 0; line-height: 1.5; border-bottom: 1px solid #F3F4F6;\">%1</p>")
                            .arg(highlighted_line.isEmpty() ? "&nbsp;" : highlighted_line);
    }
    
    return highlighted;
}

// ==================== SqliteTextHandler 实现 ====================

SqliteTextHandler::SqliteTextHandler(QObject* parent)
    : QObject(parent)
    , m_cancel_loading_(false) {
    
    qDebug() << "SqliteTextHandler 构造函数开始";
    
    // 初始化数据库管理器
    m_db_manager_ = std::make_unique<SqliteDbManager>(this);
    
    // 连接数据库信号
    connect(m_db_manager_.get(), &SqliteDbManager::databaseError,
            this, &SqliteTextHandler::databaseError);
    connect(m_db_manager_.get(), &SqliteDbManager::progressUpdate,
            this, &SqliteTextHandler::loadProgress);
    
    // 初始化文件列表模型
    m_file_list_model_ = new FileListModel(this);
    
    // 初始化搜索线程
    InitializeSearchThread();
    
    // 自动初始化数据库
    initializeDatabase();
}

SqliteTextHandler::~SqliteTextHandler() {
    cleanupSearchThread();
    CleanupTempFiles();
}

bool SqliteTextHandler::initializeDatabase(const QString& db_path) {
    if (m_db_manager_->InitializeDatabase(db_path)) {
        emit databaseInitialized();
        return true;
    }
    return false;
}

void SqliteTextHandler::clearDatabase() {
    m_db_manager_->DeleteAllFiles();
    UpdateFileListModel();
}

QVariantMap SqliteTextHandler::getDatabaseStats() {
    QVariantMap stats;
    stats["totalFiles"] = m_db_manager_->GetTotalFileCount();
    stats["totalSize"] = m_db_manager_->GetTotalSize();
    stats["categories"] = QVariant::fromValue(m_db_manager_->GetFileCountByCategory());
    stats["keywords"] = m_db_manager_->GetAllKeywords();
    return stats;
}

void SqliteTextHandler::InitializeSearchThread() {
    qDebug() << "初始化搜索线程";
    
    m_search_thread_ = std::make_unique<QThread>();
    m_search_worker_ = new DbSearchWorker(m_db_manager_.get());
    m_search_worker_->moveToThread(m_search_thread_.get());
    
    // 连接信号
    connect(m_search_worker_, &DbSearchWorker::searchProgress,
            this, &SqliteTextHandler::searchProgress);
    connect(m_search_worker_, &DbSearchWorker::searchResultReady,
            this, [this](const QList<DbSearchResult>& results, const QString& highlighted_content) {
        QVariantList variant_results;
        for (const auto& result : results) {
            QVariantMap map;
            map["lineNumber"] = result.line_number;
            map["preview"] = result.preview;
            map["fullLine"] = result.line_content;
            map["fileName"] = result.file_name;
            map["keyword"] = result.keyword;
            variant_results.append(map);
        }
        emit searchResultReady(variant_results, highlighted_content);
    });
    connect(m_search_worker_, &DbSearchWorker::searchFinished,
            this, &SqliteTextHandler::searchFinished);
    connect(m_search_worker_, &DbSearchWorker::searchCancelled,
            this, &SqliteTextHandler::searchCancelled);
    
    m_search_thread_->start();
    qDebug() << "搜索线程已启动";
}

void SqliteTextHandler::cleanupSearchThread() {
    qDebug() << "清理搜索线程";
    
    if (m_search_worker_) {
        m_search_worker_->CancelSearch();
    }
    
    if (m_search_thread_ && m_search_thread_->isRunning()) {
        m_search_thread_->quit();
        if (!m_search_thread_->wait(5000)) {
            qWarning() << "搜索线程未能正常退出，强制终止";
            m_search_thread_->terminate();
            m_search_thread_->wait();
        }
    }
    
    if (m_search_worker_) {
        m_search_worker_->deleteLater();
        m_search_worker_ = nullptr;
    }
    
    m_search_thread_.reset();
    qDebug() << "搜索线程清理完成";
}

void SqliteTextHandler::loadTextFileAsync(const QString& file_name) {
    m_cancel_loading_ = false;
    
    QString selected_file_name = file_name;
    if (selected_file_name.isEmpty()) {
        selected_file_name = QFileDialog::getOpenFileName(
            nullptr,
            "选择文件",
            "",
            "支持的文件 (*.txt *.log *.md *.csv *.zip);;文本文件 (*.txt *.log *.md *.csv);;压缩文件 (*.zip);;所有文件 (*)"
        );
    } else {
        QUrl url(selected_file_name);
        selected_file_name = url.toLocalFile();
    }
    
    if (selected_file_name.isEmpty()) {
        emit loadError("未选择文件");
        return;
    }
    
    // 检查是否为ZIP文件
    if (selected_file_name.toLower().endsWith(".zip")) {
        try {
            ProcessZipFile(selected_file_name);
        } catch (const std::exception& e) {
            emit loadError(QString("ZIP文件处理错误：%1").arg(e.what()));
        }
        return;
    }
    
}

void SqliteTextHandler::ProcessZipFile(const QString& zip_path) {
    emit loadProgress(10);
    
    // 清理之前的临时文件
    CleanupTempFiles();
    m_temp_dir_ = std::make_unique<QTemporaryDir>();
    if (!m_temp_dir_->isValid()) {
        throw std::runtime_error("无法创建临时目录");
    }
    
    emit loadProgress(20);
    
    // 解压ZIP文件
    if (!ExtractZipFile(zip_path, m_temp_dir_->path())) {
        throw std::runtime_error("ZIP文件解压失败");
    }
    
    emit loadProgress(60);
    
    // 扫描并导入文本文件到数据库
    QFileInfo zip_info(zip_path);
    QList<DbFileRecord> records = ScanAndImportTextFiles(m_temp_dir_->path(), zip_info.fileName());
    
    if (records.isEmpty()) {
        throw std::runtime_error("ZIP文件中未找到可识别的文本文件");
    }
    
    emit loadProgress(80);
    
    // 批量插入到数据库
    if (!m_db_manager_->InsertFiles(records)) {
        throw std::runtime_error("无法将文件导入数据库");
    }
    
    // 更新文件列表模型
    UpdateFileListModel();
    
    emit loadProgress(100);
    
    // 自动加载第一个关键字的内容
    QStringList keywords = m_db_manager_->GetAllKeywords();
    if (!keywords.isEmpty()) {
        m_current_keyword_ = keywords.first();
        QString content = m_db_manager_->GetMergedContentByKeyword(m_current_keyword_);
        emit fileLoaded(content);
    }
}

bool SqliteTextHandler::ExtractZipFile(const QString& zip_path, const QString& extract_dir) {
    qDebug() << "开始解压ZIP文件:" << zip_path << "到目录:" << extract_dir;

    // 确保目标目录存在
    QDir dir(extract_dir);
    if (!dir.exists()) {
        if (!dir.mkpath(".")) {
            qDebug() << "无法创建解压目录:" << extract_dir;
            return false;
        }
    }

    // 方案1: 优先使用 tar (Windows 10/11 内置)
    QProcess process;
    
#ifdef Q_OS_WIN
    QString command = "tar";
    QStringList arguments;
    QString normalized_zip_path = QDir::toNativeSeparators(zip_path);
    normalized_zip_path.remove(QRegularExpression("^\"|\"$"));
    QString normalized_extract_dir = QDir::toNativeSeparators(extract_dir);
    normalized_extract_dir.remove(QRegularExpression("^\"|\"$"));
    arguments << "-xf" << normalized_zip_path << "-C" << normalized_extract_dir;
#else
    QString command = "unzip"; // Unix/Linux/Mac系统使用unzip命令
    QStringList arguments;
    arguments << "-o" << zip_path << "-d" << extract_dir;
#endif
    
    qDebug() << "解压命令:" << command << arguments;
    process.start(command, arguments);
    bool finished = process.waitForFinished(30000);

    qDebug() << "解压是否完成:" << finished;
    qDebug() << "解压命令退出码:" << process.exitCode();
    qDebug() << "标准输出:" << process.readAllStandardOutput();
    qDebug() << "错误输出:" << process.readAllStandardError();
    
    if (finished && process.exitCode() == 0) {
        QDir extracted_dir(extract_dir);
        // 解压空包也算成功
        qDebug() << "解压成功:" << command;
        return true;
    }

    // 方案2: 备用方案 7-Zip
    qDebug() << command << "解压失败，尝试备用解压方案 7-Zip";
    
    QProcess sevenZipProcess;
    QString sevenZipCommand = "7z";
    QStringList sevenZipArgs;

    QString normalized_zip_path_7z = QDir::toNativeSeparators(zip_path);
    normalized_zip_path_7z.remove(QRegularExpression("^\"|\"$"));
    QString normalized_extract_dir_7z = QDir::toNativeSeparators(extract_dir);
    normalized_extract_dir_7z.remove(QRegularExpression("^\"|\"$"));

    sevenZipArgs << "x" << normalized_zip_path_7z << QString("-o%1").arg(normalized_extract_dir_7z) << "-y";
    
    qDebug() << "7-Zip解压命令:" << sevenZipCommand << sevenZipArgs;
    
    sevenZipProcess.start(sevenZipCommand, sevenZipArgs);
    bool sevenZipFinished = sevenZipProcess.waitForFinished(30000);

    qDebug() << "7-Zip解压是否完成:" << sevenZipFinished;
    qDebug() << "7-Zip解压命令退出码:" << sevenZipProcess.exitCode();
    qDebug() << "7-Zip标准输出:" << sevenZipProcess.readAllStandardOutput();
    qDebug() << "7-Zip错误输出:" << sevenZipProcess.readAllStandardError();

    if (sevenZipFinished && sevenZipProcess.exitCode() == 0) {
         qDebug() << "7-Zip解压成功。";
         return true;
    }

    qDebug() << "所有解压方案均失败";
    return false;
}

QList<DbFileRecord> SqliteTextHandler::ScanAndImportTextFiles(const QString& dir_path, const QString& zip_source) {
    qDebug() << "扫描文本文件，目录:" << dir_path;
    
    // 在导入前，先删除该zip_source的所有旧记录
    m_db_manager_->DeleteAllFiles();
    
    QList<DbFileRecord> records;
    QDir dir(dir_path);
    
    if (!dir.exists()) {
        return records;
    }
    
    QFileInfoList all_entries = dir.entryInfoList(QDir::Files | QDir::Dirs | QDir::NoDotAndDotDot, QDir::Name);
    
    for (const QFileInfo& file_info : all_entries) {
        if (file_info.isDir()) {
            // 递归扫描子目录
            records.append(ScanAndImportTextFiles(file_info.absoluteFilePath(), zip_source));
        } else {
            QString keyword = GetFileKeyword(file_info.fileName());
            if (!keyword.isEmpty()) {
                // 读取文件内容
                QFile file(file_info.absoluteFilePath());
                if (file.open(QIODevice::ReadOnly | QIODevice::Text)) {
                    QTextStream in(&file);
                    in.setEncoding(QStringConverter::Utf8);
                    QString content = in.readAll();
                    file.close();
                    
                    DbFileRecord record;
                    record.file_path = file_info.absoluteFilePath();
                    record.file_name = file_info.fileName();
                    record.keyword = keyword;
                    record.category = GetFileCategory(keyword);
                    record.content = content;
                    record.file_size = file_info.size();
                    record.zip_source = zip_source;
                    record.import_time = QDateTime::currentDateTime();
                    
                    records.append(record);
                    qDebug() << "添加文件记录:" << file_info.fileName() << "关键字:" << keyword;
                }
            }
        }
    }
    
    qDebug() << "扫描完成，找到" << records.size() << "个文本文件";
    return records;
}

QString SqliteTextHandler::GetFileKeyword(const QString& file_name) {
    QString lower_file_name = file_name.toLower();
    
    // 定义关键字模式（与原TextFileHandler保持一致）
    struct KeywordPattern {
        QString keyword;
        QRegularExpression exact_regex;
        QRegularExpression numbered_regex;
    };
    
    QList<KeywordPattern> patterns = {
        // {"master", QRegularExpression("^master$"), QRegularExpression("^master\\.\\d+$")},
        // {"chassis", QRegularExpression("^chassis$"), QRegularExpression("^chassis\\..*$")},
        // {"guidance", QRegularExpression("^guidance$"), QRegularExpression("^guidance\\..*$")},
        // {"sc2000a", QRegularExpression("^sc2000a$"), QRegularExpression("^sc2000a\\..*$")},
        {"vehicle", QRegularExpression("^vehicle$"), QRegularExpression("^vehicle\\..*$")},
        // {"vehicle_navigator", QRegularExpression("^vehicle_navigator$"), QRegularExpression("^vehicle_navigator\\..*$")},
        {"map", QRegularExpression("^map$"), QRegularExpression("^map\\..*$")}
    };
    
    for (const auto& pattern : patterns) {
        if (pattern.exact_regex.match(lower_file_name).hasMatch() ||pattern.numbered_regex.match(lower_file_name).hasMatch()) {
            return pattern.keyword;
        }
    }
    
    // 检查扩展名
    // QStringList text_extensions = {
    //     "txt", "md", "csv", "json", 
    //     "xml", "ini", "cfg", "yml", "yaml",
    //     "out", "err", "trace", "debug", "info"
    // };
    
    // QString extension = QFileInfo(file_name).suffix().toLower();
    // if (!extension.isEmpty() && text_extensions.contains(extension)) {
    //     return "extension_" + extension;
    // }
    
    // 检查日志关键词
    // QStringList log_keywords = {
    //     "trace", "debug", "error", "err", "out", 
    //     "audit", "access", "system", "application"
    // };
    
    // for (const QString& keyword : log_keywords) {
    //     if (lower_file_name.contains(keyword)) {
    //         return "log_" + keyword;
    //     }
    // }
    
    return QString();
}

QString SqliteTextHandler::GetFileCategory(const QString& keyword) {
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

bool SqliteTextHandler::IsTextFile(const QString& file_name) {
    return !GetFileKeyword(file_name).isEmpty();
}

void SqliteTextHandler::CleanupTempFiles() {
    if (m_temp_dir_) {
        qDebug() << "清理临时文件目录:" << m_temp_dir_->path();
        m_temp_dir_.reset();
    }
}

void SqliteTextHandler::UpdateFileListModel() {
    // 从数据库获取文件分组信息
    QMap<QString, QList<DbFileRecord>> grouped_files;
    QList<DbFileRecord> all_files = m_db_manager_->GetAllFiles();
    
    for (const auto& record : all_files) {
        grouped_files[record.keyword].append(record);
    }
    
    // 创建文件列表模型数据
    QList<FileMeta> model_files;
    for (auto it = grouped_files.constBegin(); it != grouped_files.constEnd(); ++it) {
        const QString& keyword = it.key();
        const QList<DbFileRecord>& group = it.value();
        
        qint64 total_size = 0;
        for (const auto& record : group) {
            total_size += record.file_size;
        }
        
        QString display_name = group.size() > 1
            ? QString("%1 (%2 个文件)").arg(keyword).arg(group.size())
            : keyword;
        
        FileMeta meta(
            keyword,                      // path作为唯一标识
            display_name,                 // 显示名称
            total_size,                   // 总大小
            keyword,                      // 关键字
            GetFileCategory(keyword)      // 类别
        );
        model_files.append(meta);
    }
    
    // // 排序
    std::sort(model_files.begin(), model_files.end(), [](const FileMeta& a, const FileMeta& b){
        return a.name < b.name;
    });
    
    m_file_list_model_->setFiles(model_files);
    qDebug() << "m_file_list_model_ size:" << m_file_list_model_->rowCount();
    emit fileListReady(m_file_list_model_);
}

void SqliteTextHandler::startAsyncSearch(const QString& content, const QString& search_text, int max_results) {
    Q_UNUSED(content)  // 数据库版本不需要传入content
    
    qDebug() << "SqliteTextHandler::startAsyncSearch 被调用";
    qDebug() << "搜索词:" << search_text;
    qDebug() << "当前关键字:" << m_current_keyword_;
    
    if (m_search_worker_) {
        if (!m_current_keyword_.isEmpty()) {
            // 在特定关键字内搜索
            m_search_worker_->SetSearchData(m_current_keyword_, search_text, max_results);
        } else {
            // 全库搜索
            m_search_worker_->SetFullSearchData(search_text, max_results);
        }
        QMetaObject::invokeMethod(m_search_worker_, "StartSearch", Qt::QueuedConnection);
    } else {
        emit loadError("搜索工作对象未初始化");
    }
}

void SqliteTextHandler::cancelSearch() {
    if (m_search_worker_) {
        m_search_worker_->CancelSearch();
    }
}

void SqliteTextHandler::cancelFileLoading() {
    m_cancel_loading_ = true;
}

void SqliteTextHandler::requestFileContent(const QString& file_path) {
    qDebug() << "请求文件内容，关键字:" << file_path;
    
    // file_path 实际上是 keyword
    m_current_keyword_ = file_path;
    QString content = m_db_manager_->GetMergedContentByKeyword(file_path);
    
    if (!content.isEmpty()) {
        emit fileContentReady(content, file_path);
    } else {
        // emit loadError(QString("未找到关键字 %1 的内容").arg(file_path));
    }
}

void SqliteTextHandler::clearFileCache() {
    // 数据库版本不需要清理缓存，但可以清理临时文件
    CleanupTempFiles();
}
