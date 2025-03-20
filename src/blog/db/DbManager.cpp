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
DbManager::DbManager() : m_pool(ConnectionPool::getInstance()), m_initialized(false) {
    // 构造函数初始化
}

// 析构函数
DbManager::~DbManager() {
    close();
}

// 初始化数据库连接
bool DbManager::initialize(const std::string& connStr, int minConnections, int maxConnections) {
    std::lock_guard<std::mutex> lock(m_init_mutex);
    
    if (m_initialized) {
        return isConnected();
    }
    
    // 初始化连接池
    bool success = m_pool.initialize(connStr, minConnections, maxConnections);
    
    if (success) {
        m_initialized = true;
        ngx_log_error(NGX_LOG_INFO, ngx_cycle->log, 0, 
                     "Database connection pool initialized successfully");
    } else {
        ngx_log_error(NGX_LOG_ERR, ngx_cycle->log, 0, 
                     "Failed to initialize database connection pool");
    }
    
    return success;
}

// 检查连接是否活跃
bool DbManager::isConnected() {
    if (!m_initialized) {
        return false;
    }
    
    try {
        // 从连接池获取连接并测试
        ConnectionWrapper conn(m_pool);
        conn->sql("SELECT 1").execute();
        return true;
    } catch (const std::exception& e) {
        ngx_log_error(NGX_LOG_ERR, ngx_cycle->log, 0, 
                     "Database connection check failed: %s", e.what());
        return false;
    }
}

// 执行SQL查询
mysqlx::RowResult DbManager::executeQuery(const std::string& sql) {
    const int MAX_RETRIES = 3;
    int retry_count = 0;
    
    while (retry_count < MAX_RETRIES) {
        try {
            ConnectionWrapper conn(m_pool);
            return conn->sql(sql).execute();
        } catch (const mysqlx::Error& e) {
            // 获取错误信息并记录
            std::string errorMsg = e.what();
            ngx_log_error(NGX_LOG_WARN, ngx_cycle->log, 0, 
                         "Database error (attempt %d/%d): %s", 
                         retry_count + 1, MAX_RETRIES, errorMsg.c_str());
            
            // 检查错误消息，判断是否是死锁错误
            bool isDeadlockError = 
                errorMsg.find("deadlock") != std::string::npos || 
                errorMsg.find("Deadlock") != std::string::npos;
                
            if (isDeadlockError) {
                // 如果是死锁错误，随机延迟后重试
                std::this_thread::sleep_for(std::chrono::milliseconds(100 * (retry_count + 1)));
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

// 执行带回调的SQL查询
bool DbManager::executeQuery(const std::string& sql, QueryCallback callback) {
    try {
        auto result = executeQuery(sql);
        callback(result);
        return true;
    } catch (const std::exception& e) {
        ngx_log_error(NGX_LOG_ERR, ngx_cycle->log, 0, 
                     "Error executing query with callback: %s", e.what());
        return false;
    }
}

// 执行SQL更新
int DbManager::executeUpdate(const std::string& sql) {
    const int MAX_RETRIES = 3;
    int retry_count = 0;
    
    while (retry_count < MAX_RETRIES) {
        try {
            ConnectionWrapper conn(m_pool);
            mysqlx::SqlResult result = conn->sql(sql).execute();
            return result.getAffectedItemsCount();
        } catch (const mysqlx::Error& e) {
            // 获取错误信息并记录
            std::string errorMsg = e.what();
            ngx_log_error(NGX_LOG_WARN, ngx_cycle->log, 0, 
                         "Database update error (attempt %d/%d): %s", 
                         retry_count + 1, MAX_RETRIES, errorMsg.c_str());
            
            // 检查错误消息，判断是否是死锁错误
            bool isDeadlockError = 
                errorMsg.find("deadlock") != std::string::npos || 
                errorMsg.find("Deadlock") != std::string::npos;
                
            if (isDeadlockError) {
                // 如果是死锁错误，随机延迟后重试
                std::this_thread::sleep_for(std::chrono::milliseconds(100 * (retry_count + 1)));
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

// 执行事务
bool DbManager::executeTransaction(TransactionCallback callback) {
    try {
        ConnectionWrapper conn(m_pool);
        
        // 开始事务
        conn->startTransaction();
        
        // 执行事务回调
        bool commit = callback(*conn);
        
        // 根据回调结果提交或回滚
        if (commit) {
            conn->commit();
            return true;
        } else {
            conn->rollback();
            return false;
        }
    } catch (const std::exception& e) {
        ngx_log_error(NGX_LOG_ERR, ngx_cycle->log, 0, 
                     "Transaction error: %s", e.what());
        return false;
    }
}

// 获取上一个插入的ID
uint64_t DbManager::getLastInsertId() {
    try {
        ConnectionWrapper conn(m_pool);
        mysqlx::SqlResult result = conn->sql("SELECT LAST_INSERT_ID()").execute();
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
    if (m_initialized) {
        m_pool.close();
        m_initialized = false;
        ngx_log_error(NGX_LOG_INFO, ngx_cycle->log, 0, "Database connections closed");
    }
}

// 获取连接池状态
ConnectionPool::PoolStatus DbManager::getPoolStatus() {
    return m_pool.getStatus();
}

// 创建数据库表
bool DbManager::createTables() {
    try {
        // 用户表
        executeUpdate(
            "CREATE TABLE IF NOT EXISTS users ("
            "id INT AUTO_INCREMENT PRIMARY KEY,"
            "username VARCHAR(50) NOT NULL UNIQUE,"
            "password VARCHAR(255) NOT NULL,"
            "email VARCHAR(100) NOT NULL UNIQUE,"
            "display_name VARCHAR(100) NOT NULL,"
            "bio TEXT,"
            "profile_image VARCHAR(255),"
            "created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,"
            "updated_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP ON UPDATE CURRENT_TIMESTAMP,"
            "last_login_at TIMESTAMP NULL,"
            "INDEX (username),"
            "INDEX (email)"
            ") ENGINE=InnoDB;"
        );
        
        // 博客文章表
        executeUpdate(
            "CREATE TABLE IF NOT EXISTS posts ("
            "id INT AUTO_INCREMENT PRIMARY KEY,"
            "title VARCHAR(255) NOT NULL,"
            "slug VARCHAR(255) NOT NULL UNIQUE,"
            "content TEXT NOT NULL,"
            "summary TEXT,"
            "author_id INT NOT NULL,"
            "created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,"
            "updated_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP ON UPDATE CURRENT_TIMESTAMP,"
            "published BOOLEAN DEFAULT TRUE,"
            "view_count INT DEFAULT 0,"
            "FOREIGN KEY (author_id) REFERENCES users(id) ON DELETE CASCADE,"
            "INDEX (slug),"
            "INDEX (author_id),"
            "INDEX (created_at)"
            ") ENGINE=InnoDB;"
        );
        
        // 分类表
        executeUpdate(
            "CREATE TABLE IF NOT EXISTS categories ("
            "id INT AUTO_INCREMENT PRIMARY KEY,"
            "name VARCHAR(50) NOT NULL UNIQUE,"
            "slug VARCHAR(50) NOT NULL UNIQUE,"
            "description TEXT"
            ") ENGINE=InnoDB;"
        );
        
        // 文章分类关联表
        executeUpdate(
            "CREATE TABLE IF NOT EXISTS post_categories ("
            "post_id INT NOT NULL,"
            "category_id INT NOT NULL,"
            "PRIMARY KEY (post_id, category_id),"
            "FOREIGN KEY (post_id) REFERENCES posts(id) ON DELETE CASCADE,"
            "FOREIGN KEY (category_id) REFERENCES categories(id) ON DELETE CASCADE"
            ") ENGINE=InnoDB;"
        );
        
        // 标签表
        executeUpdate(
            "CREATE TABLE IF NOT EXISTS tags ("
            "id INT AUTO_INCREMENT PRIMARY KEY,"
            "name VARCHAR(50) NOT NULL UNIQUE,"
            "slug VARCHAR(50) NOT NULL UNIQUE"
            ") ENGINE=InnoDB;"
        );
        
        // 文章标签关联表
        executeUpdate(
            "CREATE TABLE IF NOT EXISTS post_tags ("
            "post_id INT NOT NULL,"
            "tag_id INT NOT NULL,"
            "PRIMARY KEY (post_id, tag_id),"
            "FOREIGN KEY (post_id) REFERENCES posts(id) ON DELETE CASCADE,"
            "FOREIGN KEY (tag_id) REFERENCES tags(id) ON DELETE CASCADE"
            ") ENGINE=InnoDB;"
        );
        
        // 评论表 - 重新定义确保数据类型一致
        executeUpdate(
            "DROP TABLE IF EXISTS comments"
        );
        
        executeUpdate(
            "CREATE TABLE IF NOT EXISTS comments ("
            "id INT AUTO_INCREMENT PRIMARY KEY,"
            "post_id INT NOT NULL,"
            "user_id INT,"
            "parent_id INT,"
            "content TEXT NOT NULL,"
            "author_name VARCHAR(50),"
            "author_email VARCHAR(100),"
            "created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,"
            "approved BOOLEAN DEFAULT FALSE,"
            "INDEX (post_id),"
            "INDEX (user_id),"
            "INDEX (parent_id)"
            ") ENGINE=InnoDB;"
        );
        
        // 添加外键约束
        executeUpdate(
            "ALTER TABLE comments "
            "ADD CONSTRAINT fk_comments_post "
            "FOREIGN KEY (post_id) REFERENCES posts(id) ON DELETE CASCADE"
        );
        
        executeUpdate(
            "ALTER TABLE comments "
            "ADD CONSTRAINT fk_comments_user "
            "FOREIGN KEY (user_id) REFERENCES users(id) ON DELETE SET NULL"
        );
        
        executeUpdate(
            "ALTER TABLE comments "
            "ADD CONSTRAINT fk_comments_parent "
            "FOREIGN KEY (parent_id) REFERENCES comments(id) ON DELETE CASCADE"
        );
        
        ngx_log_error(NGX_LOG_INFO, ngx_cycle->log, 0, "Database tables created successfully");
        return true;
    } 
    catch (const std::exception& e) {
        ngx_log_error(NGX_LOG_ERR, ngx_cycle->log, 0, 
                     "Error creating database tables: %s", e.what());
        return false;
    }
}