#include "DbManager.hpp"
#include "Nginx.hpp"
#include <ngx_core.h>
#include <sstream>
#include <vector>
#include <mutex>
#include <regex>
#include <thread>

// 单例实例
DbManager& DbManager::getInstance() {
    static DbManager instance;
    return instance;
}

// 构造函数
DbManager::DbManager() : initialized(false), connectionAttempts(0) {
    // 构造函数初始化
}

// 析构函数
DbManager::~DbManager() {
    close();
}

// 初始化数据库连接
bool DbManager::initialize(const std::string& connStr) {
    std::lock_guard<std::mutex> lock(mutex);
    
    if (initialized) {
        return isConnected();
    }
    
    // 保存连接字符串
    connectionString = connStr;
    
    // 尝试连接
    bool success = connect();
    
    if (success) {
        initialized = true;
        ngx_log_error(NGX_LOG_INFO, ngx_cycle->log, 0, 
                     "Database connection initialized successfully");
    } else {
        ngx_log_error(NGX_LOG_ERR, ngx_cycle->log, 0, 
                     "Failed to initialize database connection");
    }
    
    return success;
}

// 检查连接是否活跃
bool DbManager::isConnected() {
    std::lock_guard<std::mutex> lock(mutex);
    
    if (!session) {
        return false;
    }
    
    try {
        // 尝试执行简单查询以验证连接是否有效
        session->sql("SELECT 1").execute();
        return true;
    } catch (const std::exception& e) {
        ngx_log_error(NGX_LOG_ERR, ngx_cycle->log, 0, 
                     "Database connection check failed: %s", e.what());
        return false;
    }
}

// 建立连接
bool DbManager::connect() {
    if (connectionString.empty()) {
        ngx_log_error(NGX_LOG_ERR, ngx_cycle->log, 0, 
                     "Connection string is empty");
        return false;
    }
    
    connectionAttempts++;
    
    if (connectionAttempts > MAX_CONNECTION_ATTEMPTS) {
        ngx_log_error(NGX_LOG_ERR, ngx_cycle->log, 0, 
                     "Maximum connection attempts reached, giving up");
        return false;
    }
    
    try {
        // 解析连接字符串
        std::string host = "localhost";
        std::string user = "root";
        std::string password = "";
        std::string database = "";
        int port = 33060;
        
        // 简单解析格式：host=...;user=...;password=...;database=...;port=...
        std::regex pattern("(\\w+)=([^;]+)");
        auto begin = std::sregex_iterator(connectionString.begin(), connectionString.end(), pattern);
        auto end = std::sregex_iterator();
        
        for (std::sregex_iterator i = begin; i != end; ++i) {
            std::smatch match = *i;
            std::string key = match[1];
            std::string value = match[2];
            
            if (key == "host") host = value;
            else if (key == "user") user = value;
            else if (key == "password") password = value;
            else if (key == "database") database = value;
            else if (key == "port") port = std::stoi(value);
        }
        
        // 创建会话
        ngx_log_error(NGX_LOG_INFO, ngx_cycle->log, 0, 
                     "Connecting to MySQL server at %s:%d as %s", 
                     host.c_str(), port, user.c_str());
        
        // 创建会话并设置连接选项
        mysqlx::SessionSettings settings(
            mysqlx::SessionOption::HOST, host,
            mysqlx::SessionOption::PORT, port,
            mysqlx::SessionOption::USER, user,
            mysqlx::SessionOption::PWD, password,
            mysqlx::SessionOption::DB, database
        );
        
        // 设置连接超时
        settings.set(mysqlx::SessionOption::CONNECT_TIMEOUT, 10000);
        
        // 创建新会话
        session = std::make_unique<mysqlx::Session>(settings);
        
        ngx_log_error(NGX_LOG_INFO, ngx_cycle->log, 0, 
                     "Connected to MySQL server, version: %s", 
                     session->sql("SELECT VERSION()").execute().fetchOne()[0].get<std::string>().c_str());
        
        return true;
    } 
    catch (const mysqlx::Error& err) {
        ngx_log_error(NGX_LOG_ERR, ngx_cycle->log, 0, 
                     "MySQL connection error: %s", 
                     err.what());
    }
    catch (const std::exception& e) {
        ngx_log_error(NGX_LOG_ERR, ngx_cycle->log, 0, 
                     "Connection error: %s", e.what());
    }
    
    return false;
}

// 获取会话
mysqlx::Session& DbManager::getSession() {
    std::lock_guard<std::mutex> lock(mutex);
    
    if (!session || !isConnected()) {
        if (!connect()) {
            throw std::runtime_error("Cannot execute query: not connected to database");
        }
    }
    
    return *session;
}

// 执行SQL查询
mysqlx::RowResult DbManager::executeQuery(const std::string& sql) {
    const int MAX_RETRIES = 3;
    int retry_count = 0;
    
    while (retry_count < MAX_RETRIES) {
        try {
            mysqlx::Session& sess = getSession();
            return sess.sql(sql).execute();
        } catch (const mysqlx::Error& e) {
            // 获取错误信息并记录
            std::string errorMsg = e.what();
            ngx_log_error(NGX_LOG_WARN, ngx_cycle->log, 0, 
                         "Database error (attempt %d/%d): %s", 
                         retry_count + 1, MAX_RETRIES, errorMsg.c_str());
            
            // 检查错误消息，判断是否是连接错误或死锁错误
            bool isConnectionError = 
                errorMsg.find("server has gone away") != std::string::npos || 
                errorMsg.find("Lost connection") != std::string::npos;
                
            bool isDeadlockError = 
                errorMsg.find("deadlock") != std::string::npos || 
                errorMsg.find("Deadlock") != std::string::npos;
                
            if (isConnectionError || isDeadlockError) {
                // 如果是连接错误，重新连接
                if (isConnectionError) {
                    std::lock_guard<std::mutex> lock(mutex);
                    session.reset();
                    if (!connect()) {
                        ngx_log_error(NGX_LOG_ERR, ngx_cycle->log, 0, 
                                     "Failed to reconnect to database");
                        throw;
                    }
                }
                
                // 如果是死锁错误，等待一小段时间
                if (isDeadlockError) {
                    // 随机延迟，避免所有线程同时重试
                    std::this_thread::sleep_for(std::chrono::milliseconds(100 * (retry_count + 1)));
                }
                
                retry_count++;
                continue;
            }
            
            // 其他错误直接抛出
            ngx_log_error(NGX_LOG_ERR, ngx_cycle->log, 0, 
                         "Error executing query: %s", e.what());
            throw;
        } catch (const std::exception& e) {
            ngx_log_error(NGX_LOG_ERR, ngx_cycle->log, 0, 
                         "Error executing query: %s", e.what());
            throw;
        }
    }
    
    // 达到最大重试次数，仍然失败
    ngx_log_error(NGX_LOG_ERR, ngx_cycle->log, 0, 
                 "Failed to execute query after %d retries", MAX_RETRIES);
    throw std::runtime_error("Failed to execute query after maximum retries");
}

// 执行SQL更新
int DbManager::executeUpdate(const std::string& sql) {
    const int MAX_RETRIES = 3;
    int retry_count = 0;
    
    while (retry_count < MAX_RETRIES) {
        try {
            mysqlx::Session& sess = getSession();
            mysqlx::SqlResult result = sess.sql(sql).execute();
            return result.getAffectedItemsCount();
        } catch (const mysqlx::Error& e) {
            // 获取错误信息并记录
            std::string errorMsg = e.what();
            ngx_log_error(NGX_LOG_WARN, ngx_cycle->log, 0, 
                         "Database update error (attempt %d/%d): %s", 
                         retry_count + 1, MAX_RETRIES, errorMsg.c_str());
            
            // 检查错误消息，判断是否是连接错误或死锁错误
            bool isConnectionError = 
                errorMsg.find("server has gone away") != std::string::npos || 
                errorMsg.find("Lost connection") != std::string::npos;
                
            bool isDeadlockError = 
                errorMsg.find("deadlock") != std::string::npos || 
                errorMsg.find("Deadlock") != std::string::npos;
                
            if (isConnectionError || isDeadlockError) {
                // 如果是连接错误，重新连接
                if (isConnectionError) {
                    std::lock_guard<std::mutex> lock(mutex);
                    session.reset();
                    if (!connect()) {
                        ngx_log_error(NGX_LOG_ERR, ngx_cycle->log, 0, 
                                     "Failed to reconnect to database");
                        return -1;
                    }
                }
                
                // 如果是死锁错误，等待一小段时间
                if (isDeadlockError) {
                    // 随机延迟，避免所有线程同时重试
                    std::this_thread::sleep_for(std::chrono::milliseconds(100 * (retry_count + 1)));
                }
                
                retry_count++;
                continue;
            }
            
            // 其他错误记录并返回失败
            ngx_log_error(NGX_LOG_ERR, ngx_cycle->log, 0, 
                         "Error executing update: %s", e.what());
            return -1;
        } catch (const std::exception& e) {
            ngx_log_error(NGX_LOG_ERR, ngx_cycle->log, 0, 
                         "Error executing update: %s", e.what());
            return -1;
        }
    }
    
    // 达到最大重试次数，仍然失败
    ngx_log_error(NGX_LOG_ERR, ngx_cycle->log, 0, 
                 "Failed to execute update after %d retries", MAX_RETRIES);
    return -1;
}

// 获取上一个插入的ID
uint64_t DbManager::getLastInsertId() {
    try {
        mysqlx::Session& sess = getSession();
        mysqlx::SqlResult result = sess.sql("SELECT LAST_INSERT_ID()").execute();
        mysqlx::Row row = result.fetchOne();
        return row[0].get<uint64_t>();
    } 
    catch (const std::exception& e) {
        ngx_log_error(NGX_LOG_ERR, ngx_cycle->log, 0, 
                    "Error getting last insert id: %s", e.what());
        return 0;
    }
}

// 关闭连接
void DbManager::close() {
    std::lock_guard<std::mutex> lock(mutex);
    
    if (session) {
        try {
            session->close();
            ngx_log_error(NGX_LOG_INFO, ngx_cycle->log, 0, 
                         "Database connection closed");
        } 
        catch (const std::exception& e) {
            ngx_log_error(NGX_LOG_ERR, ngx_cycle->log, 0, 
                         "Error closing database connection: %s", e.what());
        }
        
        session.reset();
    }
    
    initialized = false;
    connectionAttempts = 0;
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
        session->sql(createAuthorsTable).execute();
        session->sql(createPostsTable).execute();
        session->sql(createCategoriesTable).execute();
        session->sql(createTagsTable).execute();
        session->sql(createPostCategoriesTable).execute();
        session->sql(createPostTagsTable).execute();
        return true;
    } catch (const mysqlx::Error& err) {
        ngx_log_error(NGX_LOG_ERR, ngx_cycle->log, 0, "Error creating tables: %s", err.what());
        return false;
    }
}