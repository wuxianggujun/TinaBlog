#include "DbManager.hpp"
#include <sstream>
#include <mutex>
#include <regex>
#include <iostream>

// 单例实例
DbManager& DbManager::getInstance() {
    static DbManager instance;
    return instance;
}

// 构造函数
DbManager::DbManager() : m_initialized(false) {
    // 构造函数初始化
}

// 初始化数据库连接
bool DbManager::initialize(const std::string& connInfo, size_t connNum) {
    std::lock_guard<std::mutex> lock(m_init_mutex);
    
    // 如果已经初始化，直接返回
    if (m_initialized) {
        std::cout << "数据库管理器已初始化" << std::endl;
        return true;
    }
    
    // 保存连接信息
    m_connectionInfo = connInfo;
    
    try {
        // 创建PostgreSQL客户端
        m_dbClient = drogon::orm::DbClient::newPgClient(connInfo, connNum);
        m_initialized = true;
        std::cout << "数据库连接初始化成功" << std::endl;
        return true;
    } catch (const std::exception& e) {
        std::cerr << "数据库初始化错误: " << e.what() << std::endl;
        return false;
    }
}

// 检查数据库连接是否有效
bool DbManager::isConnected() {
    if (!m_initialized.load(std::memory_order_acquire) || !m_dbClient) {
        return false;
    }
    
    try {
        // 执行简单查询测试连接
        auto result = m_dbClient->execSqlSync("SELECT 1");
        return true;
    } catch (...) {
        return false;
    }
}

// 创建数据库表结构
bool DbManager::createTables() {
    try {
        // 创建用户表 - 使用UUID作为主键
        m_dbClient->execSqlSync(R"(
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
        
        // 创建文章表 - 使用user_uuid引用用户表，slug不唯一
        m_dbClient->execSqlSync(R"(
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
        
        // 创建评论表 - 使用user_uuid引用用户表
        m_dbClient->execSqlSync(R"(
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
        
        // 创建分类表 - slug可以为空
        m_dbClient->execSqlSync(R"(
            CREATE TABLE IF NOT EXISTS categories (
                id SERIAL PRIMARY KEY,
                name VARCHAR(50) UNIQUE NOT NULL,
                slug VARCHAR(50),
                description TEXT,
                parent_id INTEGER REFERENCES categories(id) ON DELETE SET NULL
            )
        )");
        
        // 创建文章分类关联表
        m_dbClient->execSqlSync(R"(
            CREATE TABLE IF NOT EXISTS article_categories (
                article_id INTEGER NOT NULL REFERENCES articles(id) ON DELETE CASCADE,
                category_id INTEGER NOT NULL REFERENCES categories(id) ON DELETE CASCADE,
                PRIMARY KEY (article_id, category_id)
            )
        )");
        
        // 创建标签表 - slug可以为空
        m_dbClient->execSqlSync(R"(
            CREATE TABLE IF NOT EXISTS tags (
                id SERIAL PRIMARY KEY,
                name VARCHAR(50) UNIQUE NOT NULL,
                slug VARCHAR(50)
            )
        )");
        
        // 创建文章标签关联表
        m_dbClient->execSqlSync(R"(
            CREATE TABLE IF NOT EXISTS article_tags (
                article_id INTEGER NOT NULL REFERENCES articles(id) ON DELETE CASCADE,
                tag_id INTEGER NOT NULL REFERENCES tags(id) ON DELETE CASCADE,
                PRIMARY KEY (article_id, tag_id)
            )
        )");
        
        // 创建设置表
        m_dbClient->execSqlSync(R"(
            CREATE TABLE IF NOT EXISTS settings (
                key VARCHAR(100) PRIMARY KEY,
                value TEXT,
                updated_at TIMESTAMP NOT NULL DEFAULT CURRENT_TIMESTAMP
            )
        )");
        
        // 创建索引以提高查询性能
        m_dbClient->execSqlSync("CREATE INDEX IF NOT EXISTS idx_articles_user_uuid ON articles(user_uuid)");
        m_dbClient->execSqlSync("CREATE INDEX IF NOT EXISTS idx_articles_created_at ON articles(created_at)");
        m_dbClient->execSqlSync("CREATE INDEX IF NOT EXISTS idx_comments_article_id ON comments(article_id)");
        m_dbClient->execSqlSync("CREATE INDEX IF NOT EXISTS idx_comments_user_uuid ON comments(user_uuid)");
        
        std::cout << "成功创建所有数据库表" << std::endl;
        return true;
    } catch (const drogon::orm::DrogonDbException& e) {
        std::cerr << "创建表错误: " << e.base().what() << std::endl;
        return false;
    }
}

// 创建数据库
bool DbManager::createDatabase(const std::string& dbName) {
    // 从连接字符串中解析信息
    std::regex re("dbname=[^ ]+");
    std::string pgConnStr = std::regex_replace(m_connectionInfo, re, "dbname=postgres");
    
    try {
        // 创建临时连接到postgres数据库
        auto tempClient = drogon::orm::DbClient::newPgClient(pgConnStr, 1);
        
        // 检查数据库是否存在
        auto result = tempClient->execSqlSync("SELECT 1 FROM pg_database WHERE datname=$1", dbName);
        
        if (result.size() == 0) {
            // 数据库不存在，创建
            std::string createSql = "CREATE DATABASE " + dbName;
            tempClient->execSqlSync(createSql);
            std::cout << "数据库 " << dbName << " 创建成功" << std::endl;
        } else {
            // 数据库已存在
            std::cout << "数据库 " << dbName << " 已存在" << std::endl;
        }
        return true;
    } catch (const drogon::orm::DrogonDbException& e) {
        std::cerr << "创建数据库错误: " << e.base().what() << std::endl;
        return false;
    }
}

// 检查数据库是否存在
bool DbManager::databaseExists(const std::string& dbName) {
    // 从连接字符串中解析信息
    std::regex re("dbname=[^ ]+");
    std::string pgConnStr = std::regex_replace(m_connectionInfo, re, "dbname=postgres");
    
    try {
        // 创建临时连接到postgres数据库
        auto tempClient = drogon::orm::DbClient::newPgClient(pgConnStr, 1);
        
        // 检查数据库是否存在
        auto result = tempClient->execSqlSync("SELECT 1 FROM pg_database WHERE datname=$1", dbName);
        
        return result.size() > 0;
    } catch (const drogon::orm::DrogonDbException& e) {
        std::cerr << "检查数据库存在性错误: " << e.base().what() << std::endl;
        return false;
    }
} 