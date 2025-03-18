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
    : host_("127.0.0.1"),  // 使用IP地址而不是主机名
      user_("root"),
      password_(""),
      database_("blog"),
      port_(33060), // X Protocol默认端口
      connected_(false),
      session_(nullptr)
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
    
    // 记录输入的连接字符串
    ngx_log_error(NGX_LOG_INFO, ngx_cycle->log, 0, "配置的连接字符串: %s", connStr.c_str());
    
    // 解析连接字符串
    parseConnectionString(connStr);
    
    // 打印连接参数
    ngx_log_error(NGX_LOG_INFO, ngx_cycle->log, 0, 
                 "连接参数 - 主机: %s, 端口: %d, 用户: %s, 数据库: %s", 
                 host_.c_str(), port_, user_.c_str(), database_.c_str());

    try {
        // 使用参数方式连接MySQL X Protocol
        ngx_log_error(NGX_LOG_INFO, ngx_cycle->log, 0, "使用SessionOption连接到MySQL X Protocol");
        
        session_ = new mysqlx::Session(
            mysqlx::SessionOption::HOST, host_,
            mysqlx::SessionOption::PORT, port_,  // 使用解析后的端口，默认33060
            mysqlx::SessionOption::USER, user_,
            mysqlx::SessionOption::PWD, password_,
            mysqlx::SessionOption::DB, database_
        );
        
        connected_ = true;
        ngx_log_error(NGX_LOG_INFO, ngx_cycle->log, 0, "成功连接到MySQL数据库");
        
        // 显示MySQL服务器版本信息
        try {
            mysqlx::RowResult res = session_->sql("SHOW VARIABLES LIKE 'version'").execute();
            if (mysqlx::Row row = res.fetchOne()) {
                const auto version = row[1].get<std::string>();
                ngx_log_error(NGX_LOG_INFO, ngx_cycle->log, 0, "MySQL 服务器版本: %s", version.c_str());
            }
        } catch (const std::exception& ex) {
            ngx_log_error(NGX_LOG_WARN, ngx_cycle->log, 0, "无法获取MySQL版本: %s", ex.what());
        }
        
        // 如果需要自动初始化数据库表
        if (autoInit && !createTables()) {
            ngx_log_error(NGX_LOG_ERR, ngx_cycle->log, 0, "创建数据库表失败");
            close();
            return false;
        }
        
        return true;
    } catch (const mysqlx::Error& err) {
        ngx_log_error(NGX_LOG_ERR, ngx_cycle->log, 0, "MySQL X DevAPI 错误: %s", err.what());
        return false;
    } catch (const std::exception& ex) {
        ngx_log_error(NGX_LOG_ERR, ngx_cycle->log, 0, "连接过程中异常: %s", ex.what());
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
    } catch (...) {
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
    } catch (const mysqlx::Error& err) {
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
    } catch (const mysqlx::Error& err) {
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
    } catch (const std::exception& ex) {
        ngx_log_error(NGX_LOG_ERR, ngx_cycle->log, 0, "Error getting last insert ID: %s", ex.what());
        return 0;
    }
}

// 关闭数据库连接
void DbManager::close() {
    std::lock_guard<std::mutex> lock(mutex_);
    if (session_) {
        try {
            // 使用session的close方法关闭连接
            if (connected_) {
                session_->close();
            }
            
            // 释放内存
            delete session_;
            session_ = nullptr;
        } catch (...) {
            // 忽略关闭连接时的异常
            ngx_log_error(NGX_LOG_WARN, ngx_cycle->log, 0, "Exception ignored while closing MySQL connection");
        }
    }
    connected_ = false;
}

// 创建数据库表
bool DbManager::createTables() {

    // 创建作者表
    std::string createAuthorsTable = 
        "CREATE TABLE IF NOT EXISTS authors ("
        "  id INT UNSIGNED AUTO_INCREMENT PRIMARY KEY,"
        "  username VARCHAR(50) NOT NULL UNIQUE,"
        "  display_name VARCHAR(100) NOT NULL,"
        "  email VARCHAR(100) NOT NULL UNIQUE,"
        "  password_hash VARCHAR(255) NOT NULL,"
        "  bio TEXT,"
        "  profile_image VARCHAR(255),"
        "  created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,"
        "  updated_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP ON UPDATE CURRENT_TIMESTAMP,"
        "  is_admin BOOLEAN DEFAULT FALSE"
        ") ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;";
    
    // 博文表 - 使用author_id外键代替author字符串
    std::string createPostsTable = 
        "CREATE TABLE IF NOT EXISTS posts ("
        "  id INT UNSIGNED AUTO_INCREMENT PRIMARY KEY,"
        "  title VARCHAR(255) NOT NULL,"
        "  slug VARCHAR(255) NOT NULL UNIQUE,"
        "  content TEXT NOT NULL,"
        "  summary TEXT,"
        "  author_id INT UNSIGNED NOT NULL,"  // 外键字段
        "  created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,"
        "  updated_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP ON UPDATE CURRENT_TIMESTAMP,"
        "  published BOOLEAN DEFAULT TRUE,"
        "  view_count INT UNSIGNED DEFAULT 0,"
        "  INDEX idx_slug (slug),"
        "  INDEX idx_created (created_at),"
        "  FOREIGN KEY (author_id) REFERENCES authors(id)"  // 外键关联
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
        session_->sql(createAuthorsTable).execute();
        session_->sql(createPostsTable).execute();
        session_->sql(createCategoriesTable).execute();
        session_->sql(createTagsTable).execute();
        session_->sql(createPostCategoriesTable).execute();
        session_->sql(createPostTagsTable).execute();
        return true;
    } catch (const mysqlx::Error& err) {
        ngx_log_error(NGX_LOG_ERR, ngx_cycle->log, 0, "Error creating tables: %s", err.what());
        return false;
    }
}

// 解析连接字符串
bool DbManager::parseConnectionString(const std::string& connStr) {
    // 默认值
    host_ = "127.0.0.1";  // 使用IP地址而不是主机名
    user_ = "root";
    password_ = "";
    database_ = "blog";
    port_ = 33060; // X Protocol默认端口
    
    // 如果连接字符串为空，使用默认值
    if (connStr.empty()) {
        return true;
    }
    
    // 格式: host=localhost;user=root;password=secret;database=blog;port=33060
    std::istringstream ss(connStr);
    std::string token;
    
    while (std::getline(ss, token, ';')) {
        size_t pos = token.find('=');
        if (pos != std::string::npos) {
            std::string key = token.substr(0, pos);
            std::string value = token.substr(pos + 1);
            
            // 去除可能的空格
            auto trim = [](std::string& s) {
                s.erase(0, s.find_first_not_of(" \t\r\n"));
                s.erase(s.find_last_not_of(" \t\r\n") + 1);
            };
            
            trim(key);
            trim(value);
            
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
                                 "端口格式不正确: %s", value.c_str());
                }
            }
        }
    }
    
    return true;
} 