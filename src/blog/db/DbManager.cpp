#include "DbManager.hpp"
#include "Nginx.hpp"
#include <ngx_core.h>
#include <sstream>
#include <vector>
#include <mutex>

// 直接包含MySQL相关头文件
#include <jdbc/mysql_connection.h>
#include <jdbc/mysql_driver.h>
#include <jdbc/cppconn/statement.h>
#include <jdbc/cppconn/resultset.h>
#include <jdbc/cppconn/prepared_statement.h>
#include <jdbc/cppconn/exception.h>

// 单例实例
DbManager& DbManager::getInstance() {
    static DbManager instance;
    return instance;
}

// 构造函数
DbManager::DbManager() 
    : connection_(nullptr),
      host_("localhost"),
      user_("root"),
      password_(""),
      database_("blog"),
      port_(3306),
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
    
    // 初始化MySQL库
    connection_ = mysql_init(nullptr);
    if (!connection_) {
        ngx_log_error(NGX_LOG_ERR, ngx_cycle->log, 0, "Failed to initialize MySQL client");
        return false;
    }
    
    // 设置自动重连
    bool reconnect = true;
    mysql_options(connection_, MYSQL_OPT_RECONNECT, &reconnect);
    
    // 建立连接
    if (!mysql_real_connect(connection_, host_.c_str(), user_.c_str(), password_.c_str(), 
                           database_.c_str(), port_, nullptr, 0)) {
        ngx_log_error(NGX_LOG_ERR, ngx_cycle->log, 0, "Failed to connect to MySQL: %s", 
                     mysql_error(connection_));
        mysql_close(connection_);
        connection_ = nullptr;
        return false;
    }
    
    // 设置为UTF8连接
    mysql_set_character_set(connection_, "utf8mb4");
    
    connected_ = true;
    
    ngx_log_error(NGX_LOG_INFO, ngx_cycle->log, 0, "Successfully connected to MySQL database");
    
    // 如果需要自动初始化数据库表
    if (autoInit && !createTables()) {
        ngx_log_error(NGX_LOG_ERR, ngx_cycle->log, 0, "Failed to initialize database tables");
        close();
        return false;
    }
    
    return true;
}

// 获取数据库连接
MYSQL* DbManager::getConnection() {
    std::lock_guard<std::mutex> lock(mutex_);
    if (!connected_ || !connection_) {
        return nullptr;
    }
    return connection_;
}

// 检查数据库连接是否有效
bool DbManager::isConnected() const {
    return connected_ && connection_ && mysql_ping(connection_) == 0;
}

// 执行SQL查询
MYSQL_RES* DbManager::executeQuery(const std::string& sql) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (!connected_ || !connection_) {
        ngx_log_error(NGX_LOG_ERR, ngx_cycle->log, 0, "Cannot execute query: not connected to database");
        return nullptr;
    }
    
    if (mysql_query(connection_, sql.c_str()) != 0) {
        ngx_log_error(NGX_LOG_ERR, ngx_cycle->log, 0, "MySQL query error: %s", mysql_error(connection_));
        return nullptr;
    }
    
    return mysql_store_result(connection_);
}

// 执行SQL更新
int DbManager::executeUpdate(const std::string& sql) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (!connected_ || !connection_) {
        ngx_log_error(NGX_LOG_ERR, ngx_cycle->log, 0, "Cannot execute update: not connected to database");
        return -1;
    }
    
    if (mysql_query(connection_, sql.c_str()) != 0) {
        ngx_log_error(NGX_LOG_ERR, ngx_cycle->log, 0, "MySQL update error: %s", mysql_error(connection_));
        return -1;
    }
    
    return (int)mysql_affected_rows(connection_);
}

// 获取最后一次插入操作的ID
unsigned long long DbManager::getLastInsertId() {
    if (!connected_ || !connection_) {
        return 0;
    }
    return mysql_insert_id(connection_);
}

// 关闭数据库连接
void DbManager::close() {
    std::lock_guard<std::mutex> lock(mutex_);
    if (connection_) {
        mysql_close(connection_);
        connection_ = nullptr;
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
    
    // 执行创建表操作
    return  executeUpdate(createPostsTable) != -1 &&
            executeUpdate(createCategoriesTable) != -1 &&
            executeUpdate(createTagsTable) != -1 &&
            executeUpdate(createPostCategoriesTable) != -1 &&
            executeUpdate(createPostTagsTable) != -1;
}

// 解析连接字符串
bool DbManager::parseConnectionString(const std::string& connStr) {
    // 默认值
    host_ = "localhost";
    user_ = "root";
    password_ = "";
    database_ = "blog";
    port_ = 3306;
    
    // 格式: host=localhost;user=root;password=secret;database=blog;port=3306
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
    
    return !database_.empty();  // 至少需要数据库名
} 