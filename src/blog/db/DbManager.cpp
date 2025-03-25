#include "DbManager.hpp"
#include <sstream>
#include <vector>
#include <mutex>
#include <regex>
#include <thread>
#include <iostream>

// 单例实例
DbManager& DbManager::getInstance() {
    static DbManager instance;
    return instance;
}

// 构造函数
DbManager::DbManager() : m_initialized(false) {
    // 构造函数初始化，不再创建连接池
}

// 析构函数
DbManager::~DbManager() {
    close();
}

// 初始化数据库连接
bool DbManager::initialize(const std::string& connStr) {
    std::lock_guard<std::mutex> lock(m_init_mutex);
    
    // 如果已经初始化，需要先关闭当前连接
    if (m_initialized) {
        std::cout << "管理器已初始化，正在重置状态..." << std::endl;
        close();
        m_initialized = false;
    }
    
    // 保存连接字符串
    m_connectionString = connStr;
    
    try {
        // 直接创建一个连接
        m_connection = std::make_unique<pqxx::connection>(connStr);
        
        if (m_connection && m_connection->is_open()) {
            m_initialized = true;
            std::cout << "Database connection initialized successfully" << std::endl;
            return true;
        } else {
            std::cerr << "Failed to initialize database connection" << std::endl;
            return false;
        }
    } catch (const std::exception& e) {
        std::cerr << "Database initialization error: " << e.what() << std::endl;
        return false;
    }
}

// 检查数据库连接是否有效
bool DbManager::isConnected() {
    // 首先进行快速检查
    if (!m_initialized.load(std::memory_order_acquire) || !m_connection) {
        return false;
    }
    
    try {
        // 验证连接是否有效
        return m_connection->is_open();
    } 
    catch (const std::exception& e) {
        std::cerr << "Connection check failed: " << e.what() << std::endl;
        return false;
    }
}

// 执行SQL查询
pqxx::result DbManager::executeQuery(const std::string& sql) {
    const int MAX_RETRIES = 3;
    int retry_count = 0;
    
    while (retry_count < MAX_RETRIES) {
        try {
            // 检查连接状态
            if (!m_connection || !m_connection->is_open()) {
                // 尝试重新连接
                if (!m_connection) {
                    m_connection = std::make_unique<pqxx::connection>(m_connectionString);
                } else if (!m_connection->is_open()) {
                    // libpqxx没有activate方法，需要重新创建连接
                    m_connection = std::make_unique<pqxx::connection>(m_connectionString);
                }
                
                if (!m_connection->is_open()) {
                    std::cerr << "Connection is not open, retrying (" 
                              << retry_count + 1 << "/" << MAX_RETRIES << ")..." << std::endl;
                    retry_count++;
                    std::this_thread::sleep_for(std::chrono::seconds(1));
                    continue;
                }
            }
            
            // 打印详细信息
            std::cout << "执行SQL查询: " << sql.substr(0, 50) << (sql.length() > 50 ? "..." : "") << std::endl;
            
            // 执行查询
            pqxx::work tx(*m_connection);
            pqxx::result result = tx.exec(sql);
            tx.commit();
            
            return result;
        } catch (const pqxx::broken_connection& e) {
            // 连接断开，尝试重新连接
            std::cerr << "Database connection broken during query: " << e.what() << std::endl;
            
            // 重置连接
            m_connection.reset();
            
            retry_count++;
            std::this_thread::sleep_for(std::chrono::seconds(1));
            continue;
        } catch (const pqxx::sql_error& e) {
            // 获取错误信息并记录
            std::string errorMsg = e.what();
            std::cerr << "Database error (attempt " << retry_count + 1 << "/" 
                      << MAX_RETRIES << "): " << errorMsg << std::endl;
            
            // 检查错误消息，判断是否是死锁错误
            bool isDeadlockError = 
                errorMsg.find("deadlock") != std::string::npos || 
                errorMsg.find("Deadlock") != std::string::npos;
                
            if (isDeadlockError) {
                // 如果是死锁错误，随机延迟后重试
                std::this_thread::sleep_for(std::chrono::seconds(1));
                retry_count++;
                continue;
            }
            
            // 其他错误直接抛出
            std::cerr << "Error executing query: " << e.what() << std::endl;
            throw;
        } catch (const std::exception& e) {
            std::cerr << "Error executing query: " << e.what() << std::endl;
            throw;
        }
    }
    
    // 达到最大重试次数，仍然失败
    std::cerr << "Failed to execute query after " << MAX_RETRIES << " retries" << std::endl;
    throw std::runtime_error("Failed to execute query after maximum retries");
}

// 执行带回调的SQL查询
bool DbManager::executeQuery(const std::string& sql, QueryCallback callback) {
    try {
        auto result = executeQuery(sql);
        callback(result);
        return true;
    } catch (const std::exception& e) {
        std::cerr << "Error executing query with callback: " << e.what() << std::endl;
        return false;
    }
}

// 执行SQL更新
int DbManager::executeUpdate(const std::string& sql) {
    const int MAX_RETRIES = 3;
    int retry_count = 0;
    
    while (retry_count < MAX_RETRIES) {
        try {
            // 检查连接状态
            if (!m_connection || !m_connection->is_open()) {
                // 尝试重新连接
                if (!m_connection) {
                    m_connection = std::make_unique<pqxx::connection>(m_connectionString);
                } else if (!m_connection->is_open()) {
                    // libpqxx没有activate方法，需要重新创建连接
                    m_connection = std::make_unique<pqxx::connection>(m_connectionString);
                }
                
                if (!m_connection->is_open()) {
                    std::cerr << "Connection is not open, retrying (" 
                              << retry_count + 1 << "/" << MAX_RETRIES << ")..." << std::endl;
                    retry_count++;
                    std::this_thread::sleep_for(std::chrono::seconds(1));
                    continue;
                }
            }
            
            // 打印详细信息
            std::cout << "执行SQL更新: " << sql.substr(0, 50) << (sql.length() > 50 ? "..." : "") << std::endl;
            
            // 执行更新
            pqxx::work tx(*m_connection);
            pqxx::result result = tx.exec(sql);
            tx.commit();
            
            return result.affected_rows();
        } catch (const pqxx::broken_connection& e) {
            // 连接断开，尝试重新连接
            std::cerr << "Database connection broken during update: " << e.what() << std::endl;
            
            // 重置连接
            m_connection.reset();
            
            retry_count++;
            std::this_thread::sleep_for(std::chrono::seconds(1));
            continue;
        } catch (const pqxx::sql_error& e) {
            // 获取错误信息并记录
            std::string errorMsg = e.what();
            std::cerr << "Database update error (attempt " << retry_count + 1 << "/" 
                      << MAX_RETRIES << "): " << errorMsg << std::endl;
            
            // 检查错误消息，判断是否是死锁错误
            bool isDeadlockError = 
                errorMsg.find("deadlock") != std::string::npos || 
                errorMsg.find("Deadlock") != std::string::npos;
                
            if (isDeadlockError) {
                // 如果是死锁错误，随机延迟后重试
                std::this_thread::sleep_for(std::chrono::seconds(1));
                retry_count++;
                continue;
            }
            
            // 其他错误直接抛出
            std::cerr << "Error executing update: " << e.what() << std::endl;
            throw;
        } catch (const std::exception& e) {
            std::cerr << "Error executing update: " << e.what() << std::endl;
            throw;
        }
    }
    
    // 达到最大重试次数，仍然失败
    std::cerr << "Failed to execute update after " << MAX_RETRIES << " retries" << std::endl;
    throw std::runtime_error("Failed to execute update after maximum retries");
}

// 在事务中执行操作
bool DbManager::executeTransaction(TransactionCallback callback) {
    const int MAX_RETRIES = 3;
    int retry_count = 0;
    
    while (retry_count < MAX_RETRIES) {
        try {
            // 检查连接状态
            if (!m_connection || !m_connection->is_open()) {
                // 尝试重新连接
                if (!m_connection) {
                    m_connection = std::make_unique<pqxx::connection>(m_connectionString);
                } else if (!m_connection->is_open()) {
                    // libpqxx没有activate方法，需要重新创建连接
                    m_connection = std::make_unique<pqxx::connection>(m_connectionString);
                }
                
                if (!m_connection->is_open()) {
                    std::cerr << "Connection is not open, retrying (" 
                              << retry_count + 1 << "/" << MAX_RETRIES << ")..." << std::endl;
                    retry_count++;
                    std::this_thread::sleep_for(std::chrono::seconds(1));
                    continue;
                }
            }
            
            // 执行事务
            pqxx::work tx(*m_connection);
            bool result = callback(tx);
            
            if (result) {
                tx.commit();
                return true;
            } else {
                tx.abort();
                return false;
            }
        } catch (const pqxx::broken_connection& e) {
            // 连接断开，尝试重新连接
            std::cerr << "Database connection broken during transaction: " << e.what() << std::endl;
            
            // 重置连接
            m_connection.reset();
            
            retry_count++;
            std::this_thread::sleep_for(std::chrono::seconds(1));
            continue;
        } catch (const pqxx::sql_error& e) {
            // 获取错误信息并记录
            std::string errorMsg = e.what();
            std::cerr << "Database transaction error (attempt " << retry_count + 1 << "/" 
                      << MAX_RETRIES << "): " << errorMsg << std::endl;
            
            // 检查错误消息，判断是否是死锁错误
            bool isDeadlockError = 
                errorMsg.find("deadlock") != std::string::npos || 
                errorMsg.find("Deadlock") != std::string::npos;
                
            if (isDeadlockError) {
                // 如果是死锁错误，随机延迟后重试
                std::this_thread::sleep_for(std::chrono::seconds(1));
                retry_count++;
                continue;
            }
            
            // 其他错误直接抛出
            std::cerr << "Error executing transaction: " << e.what() << std::endl;
            throw;
        } catch (const std::exception& e) {
            std::cerr << "Error executing transaction: " << e.what() << std::endl;
            throw;
        }
    }
    
    // 达到最大重试次数，仍然失败
    std::cerr << "Failed to execute transaction after " << MAX_RETRIES << " retries" << std::endl;
    throw std::runtime_error("Failed to execute transaction after maximum retries");
}

// 获取最后一次插入操作的ID
uint64_t DbManager::getLastInsertId(const std::string& table, const std::string& idColumn) {
    std::string sql;
    
    if (table.empty()) {
        sql = "SELECT lastval()";
    } else {
        sql = "SELECT currval(pg_get_serial_sequence('" + table + "', '" + idColumn + "'))";
    }
    
    try {
        auto result = executeQuery(sql);
        if (result.empty()) {
            return 0;
        }
        return result[0][0].as<uint64_t>();
    } catch (const std::exception& e) {
        std::cerr << "Error getting last insert ID: " << e.what() << std::endl;
        return 0;
    }
}

// 关闭数据库连接
void DbManager::close() {
    std::lock_guard<std::mutex> lock(m_init_mutex);
    
    if (m_connection) {
        try {
            if (m_connection->is_open()) {
                // libpqxx没有disconnect方法，但会在析构时自动关闭连接
                // m_connection->disconnect();
            }
            m_connection.reset();
        } catch (const std::exception& e) {
            std::cerr << "Error closing database connection: " << e.what() << std::endl;
        }
    }
    
    m_initialized = false;
}

// 创建数据库表结构
bool DbManager::createTables() {
    // 这里实现创建数据库表的代码
    try {
        // 创建所有表的事务
        return executeTransaction([](pqxx::work& tx) {
            // 1. 创建用户表 - 使用UUID作为主键
            tx.exec(R"(
                CREATE TABLE IF NOT EXISTS users (
                    uuid VARCHAR(36) PRIMARY KEY,
                    username VARCHAR(50) UNIQUE NOT NULL,
                    password VARCHAR(100) NOT NULL,
                    email VARCHAR(100) UNIQUE NOT NULL,
                    display_name VARCHAR(100),
                    bio TEXT,
                    avatar VARCHAR(255),
                    is_admin BOOLEAN DEFAULT FALSE,
                    created_at TIMESTAMP NOT NULL DEFAULT CURRENT_TIMESTAMP,
                    updated_at TIMESTAMP NOT NULL DEFAULT CURRENT_TIMESTAMP
                )
            )");
            
            // 2. 创建文章表 - 使用user_uuid引用用户表，slug不唯一
            tx.exec(R"(
                CREATE TABLE IF NOT EXISTS articles (
                    id SERIAL PRIMARY KEY,
                    title VARCHAR(200) NOT NULL,
                    slug VARCHAR(200),
                    content TEXT NOT NULL,
                    summary TEXT,
                    cover_image VARCHAR(255),
                    view_count INTEGER DEFAULT 0,
                    user_uuid VARCHAR(36) NOT NULL REFERENCES users(uuid),
                    is_published BOOLEAN DEFAULT TRUE,
                    created_at TIMESTAMP NOT NULL DEFAULT CURRENT_TIMESTAMP,
                    updated_at TIMESTAMP NOT NULL DEFAULT CURRENT_TIMESTAMP
                )
            )");
            
            // 3. 创建评论表 - 使用user_uuid引用用户表
            tx.exec(R"(
                CREATE TABLE IF NOT EXISTS comments (
                    id SERIAL PRIMARY KEY,
                    content TEXT NOT NULL,
                    article_id INTEGER NOT NULL REFERENCES articles(id) ON DELETE CASCADE,
                    user_uuid VARCHAR(36) REFERENCES users(uuid) ON DELETE SET NULL,
                    parent_id INTEGER REFERENCES comments(id) ON DELETE CASCADE,
                    author_name VARCHAR(50),
                    author_email VARCHAR(100),
                    is_approved BOOLEAN DEFAULT FALSE,
                    created_at TIMESTAMP NOT NULL DEFAULT CURRENT_TIMESTAMP
                )
            )");
            
            // 4. 创建分类表 - slug可以为空
            tx.exec(R"(
                CREATE TABLE IF NOT EXISTS categories (
                    id SERIAL PRIMARY KEY,
                    name VARCHAR(50) UNIQUE NOT NULL,
                    slug VARCHAR(50),
                    description TEXT,
                    parent_id INTEGER REFERENCES categories(id) ON DELETE SET NULL
                )
            )");
            
            // 5. 创建文章分类关联表
            tx.exec(R"(
                CREATE TABLE IF NOT EXISTS article_categories (
                    article_id INTEGER NOT NULL REFERENCES articles(id) ON DELETE CASCADE,
                    category_id INTEGER NOT NULL REFERENCES categories(id) ON DELETE CASCADE,
                    PRIMARY KEY (article_id, category_id)
                )
            )");
            
            // 6. 创建标签表 - slug可以为空
            tx.exec(R"(
                CREATE TABLE IF NOT EXISTS tags (
                    id SERIAL PRIMARY KEY,
                    name VARCHAR(50) UNIQUE NOT NULL,
                    slug VARCHAR(50)
                )
            )");
            
            // 7. 创建文章标签关联表
            tx.exec(R"(
                CREATE TABLE IF NOT EXISTS article_tags (
                    article_id INTEGER NOT NULL REFERENCES articles(id) ON DELETE CASCADE,
                    tag_id INTEGER NOT NULL REFERENCES tags(id) ON DELETE CASCADE,
                    PRIMARY KEY (article_id, tag_id)
                )
            )");
            
            // 8. 创建设置表
            tx.exec(R"(
                CREATE TABLE IF NOT EXISTS settings (
                    key VARCHAR(100) PRIMARY KEY,
                    value TEXT,
                    updated_at TIMESTAMP NOT NULL DEFAULT CURRENT_TIMESTAMP
                )
            )");
            
            // 创建索引以提高查询性能
            tx.exec("CREATE INDEX IF NOT EXISTS idx_articles_user_uuid ON articles(user_uuid)");
            tx.exec("CREATE INDEX IF NOT EXISTS idx_articles_created_at ON articles(created_at)");
            tx.exec("CREATE INDEX IF NOT EXISTS idx_comments_article_id ON comments(article_id)");
            tx.exec("CREATE INDEX IF NOT EXISTS idx_comments_user_uuid ON comments(user_uuid)");
            
            // 成功创建所有表，提交事务
            return true;
        });
    } catch (const std::exception& e) {
        std::cerr << "Error creating tables: " << e.what() << std::endl;
        return false;
    }
}

// 创建数据库
bool DbManager::createDatabase(const std::string& dbName) {
    try {
        // 创建数据库不能在事务中执行，需要特殊处理
        std::string connStr = m_connectionString;
        // 修改连接字符串，指向postgres数据库
        std::regex re("dbname=[^ ]+");
        connStr = std::regex_replace(connStr, re, "dbname=postgres");
        
        // 创建临时连接
        pqxx::connection conn(connStr);
        pqxx::nontransaction ntx(conn);
        
        // 检查数据库是否存在
        std::string checkSql = "SELECT 1 FROM pg_database WHERE datname = " + ntx.quote(dbName);
        pqxx::result result = ntx.exec(checkSql);
        
        if (result.empty()) {
            // 数据库不存在，创建它
            std::string createSql = "CREATE DATABASE " + ntx.esc(dbName);
            ntx.exec(createSql);
            ntx.commit();
            std::cout << "Database " << dbName << " created successfully" << std::endl;
            return true;
        } else {
            // 数据库已存在
            std::cout << "Database " << dbName << " already exists" << std::endl;
            return true;
        }
    } catch (const std::exception& e) {
        std::cerr << "Error creating database: " << e.what() << std::endl;
        return false;
    }
}

// 检查数据库是否存在
bool DbManager::databaseExists(const std::string& dbName) {
    try {
        // 修改连接字符串，指向postgres数据库
        std::string connStr = m_connectionString;
        std::regex re("dbname=[^ ]+");
        connStr = std::regex_replace(connStr, re, "dbname=postgres");
        
        // 创建临时连接
        pqxx::connection conn(connStr);
        pqxx::nontransaction ntx(conn);
        
        // 检查数据库是否存在
        std::string checkSql = "SELECT 1 FROM pg_database WHERE datname = " + ntx.quote(dbName);
        pqxx::result result = ntx.exec(checkSql);
        
        return !result.empty();
    } catch (const std::exception& e) {
        std::cerr << "Error checking database existence: " << e.what() << std::endl;
        return false;
    }
} 