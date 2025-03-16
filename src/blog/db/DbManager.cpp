#include "DbManager.hpp"
#include "Nginx.hpp"
#include <ngx_core.h>
#include <sstream>
#include <vector>
#include <mutex>


// 单例实例
DbManager& DbManager::getInstance() {
    static DbManager instance;
    return instance;
}

// 构造函数
DbManager::DbManager() 
    : host_("localhost"),
      user_("root"),
      password_(""),
      database_("blog"),
      port_(33060), // X Protocol默认端口
      connected_(false)
{
    // 构造函数体为空
}

// 析构函数
DbManager::~DbManager() {
    close();
}

// 初始化数据库连接
bool DbManager::initialize(const std::string& connStr, bool autoInit) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    // 如果已连接，先关闭
    if (connected_) {
        close();
    }
    
    // 解析连接字符串
    if (!parseConnectionString(connStr)) {
        ngx_log_error(NGX_LOG_ERR, ngx_cycle->log, 0, "Failed to parse database connection string");
        return false;
    }
    
    try {
        // 构建连接URL
        std::string url = "mysqlx://" + user_ + ":" + password_ + "@" + 
                        host_ + ":" + std::to_string(port_) + "/" + database_;
        
        ngx_log_error(NGX_LOG_INFO, ngx_cycle->log, 0, "Connecting with URL: %s", url.c_str());
        
        // 建立连接 - 使用直接构造函数而不是make_unique
        session_ = new mysqlx::Session(url);
        connected_ = true;
        
        ngx_log_error(NGX_LOG_INFO, ngx_cycle->log, 0, "Successfully connected to MySQL database using X Protocol");
        
        // 如果需要自动初始化数据库表
        if (autoInit && !createTables()) {
            ngx_log_error(NGX_LOG_ERR, ngx_cycle->log, 0, "Failed to initialize database tables");
            close();
            return false;
        }
        
        return true;
    }
    catch (const mysqlx::Error &err) {
        ngx_log_error(NGX_LOG_ERR, ngx_cycle->log, 0, "MySQL X DevAPI error: %s", err.what());
        
        // 尝试使用传统端口
        try {
            port_ = 3306;  // 切换到传统端口
            std::string url = "mysqlx://" + user_ + ":" + password_ + "@" + 
                            host_ + ":" + std::to_string(port_) + "/" + database_;
            
            ngx_log_error(NGX_LOG_INFO, ngx_cycle->log, 0, "Retrying with traditional port: %s", url.c_str());
            
            // 重新尝试连接
            session_ = new mysqlx::Session(url);
            connected_ = true;
            
            ngx_log_error(NGX_LOG_INFO, ngx_cycle->log, 0, "Successfully connected to MySQL database on port 3306");
            
            // 如果需要自动初始化数据库表
            if (autoInit && !createTables()) {
                ngx_log_error(NGX_LOG_ERR, ngx_cycle->log, 0, "Failed to initialize database tables");
                close();
                return false;
            }
            
            return true;
        }
        catch (const std::exception& ex) {
            ngx_log_error(NGX_LOG_ERR, ngx_cycle->log, 0, "Failed to connect on traditional port: %s", ex.what());
            return false;
        }
    }
    catch (const std::exception& ex) {
        ngx_log_error(NGX_LOG_ERR, ngx_cycle->log, 0, "Exception during MySQL connection: %s", ex.what());
        return false;
    }
}

// 获取会话
mysqlx::Session& DbManager::getSession() {
    std::lock_guard<std::mutex> lock(mutex_);
    if (!connected_ || !session_) {
        throw std::runtime_error("Database not connected");
    }
    return *session_;
}

// 检查数据库连接是否有效
bool DbManager::isConnected() const {
    if (!connected_ || !session_) {
        return false;
    }
    
    try {
        // 尝试通过简单的SQL查询检查连接是否有效
        mysqlx::SqlResult res = const_cast<mysqlx::Session*>(session_)->sql("SELECT 1").execute();
        return true;
    }
    catch (...) {
        return false;
    }
}

// 执行SQL查询
mysqlx::RowResult DbManager::executeQuery(const std::string& sql) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (!connected_ || !session_) {
        ngx_log_error(NGX_LOG_ERR, ngx_cycle->log, 0, "Cannot execute query: not connected to database");
        throw std::runtime_error("Database not connected");
    }
    
    try {
        return session_->sql(sql).execute();
    }
    catch (const mysqlx::Error& err) {
        ngx_log_error(NGX_LOG_ERR, ngx_cycle->log, 0, "MySQL query error: %s", err.what());
        throw;
    }
}

// 执行SQL更新
int DbManager::executeUpdate(const std::string& sql) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (!connected_ || !session_) {
        ngx_log_error(NGX_LOG_ERR, ngx_cycle->log, 0, "Cannot execute update: not connected to database");
        return -1;
    }
    
    try {
        mysqlx::SqlResult result = session_->sql(sql).execute();
        return static_cast<int>(result.getAffectedItemsCount());
    }
    catch (const mysqlx::Error& err) {
        ngx_log_error(NGX_LOG_ERR, ngx_cycle->log, 0, "MySQL update error: %s", err.what());
        return -1;
    }
}

// 获取最后一次插入操作的ID
uint64_t DbManager::getLastInsertId() {
    if (!connected_ || !session_) {
        return 0;
    }
    
    try {
        mysqlx::RowResult result = session_->sql("SELECT LAST_INSERT_ID()").execute();
        mysqlx::Row row = result.fetchOne();
        return row[0].get<uint64_t>();
    }
    catch (const std::exception& ex) {
        ngx_log_error(NGX_LOG_ERR, ngx_cycle->log, 0, "Error getting last insert ID: %s", ex.what());
        return 0;
    }
}

// 关闭数据库连接
void DbManager::close() {
    std::lock_guard<std::mutex> lock(mutex_);
    if (session_) {
        try {
            session_->close();
            delete session_; // 释放内存
            session_ = nullptr;
        }
        catch (...) {
            // 忽略关闭连接时的异常
        }
    }
    connected_ = false;
}

// 创建数据库表
bool DbManager::createTables() {
    // 博文表
    std::string createPostsTable = 
        "CREATE TABLE IF NOT EXISTS posts ("
        "  id INT UNSIGNED AUTO_INCREMENT PRIMARY KEY,"
        "  title VARCHAR(255) NOT NULL,"
        "  slug VARCHAR(255) NOT NULL UNIQUE,"
        "  content TEXT NOT NULL,"
        "  summary TEXT,"
        "  created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,"
        "  updated_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP ON UPDATE CURRENT_TIMESTAMP,"
        "  published BOOLEAN DEFAULT TRUE,"
        "  view_count INT UNSIGNED DEFAULT 0,"
        "  INDEX idx_slug (slug),"
        "  INDEX idx_created (created_at)"
        ") ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;";
    
    // 分类表
    std::string createCategoriesTable = 
        "CREATE TABLE IF NOT EXISTS categories ("
        "  id INT UNSIGNED AUTO_INCREMENT PRIMARY KEY,"
        "  name VARCHAR(100) NOT NULL UNIQUE,"
        "  slug VARCHAR(100) NOT NULL UNIQUE,"
        "  description TEXT"
        ") ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;";
    
    // 标签表
    std::string createTagsTable = 
        "CREATE TABLE IF NOT EXISTS tags ("
        "  id INT UNSIGNED AUTO_INCREMENT PRIMARY KEY,"
        "  name VARCHAR(50) NOT NULL UNIQUE,"
        "  slug VARCHAR(50) NOT NULL UNIQUE"
        ") ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;";
    
    // 博文分类关系表
    std::string createPostCategoriesTable = 
        "CREATE TABLE IF NOT EXISTS post_categories ("
        "  post_id INT UNSIGNED NOT NULL,"
        "  category_id INT UNSIGNED NOT NULL,"
        "  PRIMARY KEY (post_id, category_id),"
        "  FOREIGN KEY (post_id) REFERENCES posts(id) ON DELETE CASCADE,"
        "  FOREIGN KEY (category_id) REFERENCES categories(id) ON DELETE CASCADE"
        ") ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;";
    
    // 博文标签关系表
    std::string createPostTagsTable = 
        "CREATE TABLE IF NOT EXISTS post_tags ("
        "  post_id INT UNSIGNED NOT NULL,"
        "  tag_id INT UNSIGNED NOT NULL,"
        "  PRIMARY KEY (post_id, tag_id),"
        "  FOREIGN KEY (post_id) REFERENCES posts(id) ON DELETE CASCADE,"
        "  FOREIGN KEY (tag_id) REFERENCES tags(id) ON DELETE CASCADE"
        ") ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;";
    
    try {
        // 执行创建表操作
        session_->sql(createPostsTable).execute();
        session_->sql(createCategoriesTable).execute();
        session_->sql(createTagsTable).execute();
        session_->sql(createPostCategoriesTable).execute();
        session_->sql(createPostTagsTable).execute();
        return true;
    }
    catch (const mysqlx::Error& err) {
        ngx_log_error(NGX_LOG_ERR, ngx_cycle->log, 0, "Error creating tables: %s", err.what());
        return false;
    }
}

// 解析连接字符串
bool DbManager::parseConnectionString(const std::string& connStr) {
    // 默认值
    host_ = "localhost";
    user_ = "root";
    password_ = "";
    database_ = "blog";
    port_ = 33060; // X Protocol默认端口
    
    // 格式: host=localhost;user=root;password=secret;database=blog;port=33060
    std::istringstream ss(connStr);
    std::string token;
    
    while (std::getline(ss, token, ';')) {
        std::istringstream tokenStream(token);
        std::string key, value;
        
        if (std::getline(tokenStream, key, '=') && std::getline(tokenStream, value)) {
            if (key == "host") {
                host_ = value;
            } else if (key == "user") {
                user_ = value;
            } else if (key == "password") {
                password_ = value;
            } else if (key == "database") {
                database_ = value;
            } else if (key == "port") {
                try {
                    port_ = std::stoi(value);
                } catch (...) {
                    ngx_log_error(NGX_LOG_WARN, ngx_cycle->log, 0, 
                                 "Invalid port in connection string: %s", value.c_str());
                }
            }
        }
    }
    
    return true;
} 