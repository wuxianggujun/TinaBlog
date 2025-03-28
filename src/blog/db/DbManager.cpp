#include "DbManager.hpp"
#include <sstream>
#include <mutex>
#include <regex>
#include <iostream>
#include <drogon/utils/Utilities.h>

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
    m_connectionNumber = connNum;
    
    try {
        // 创建PostgreSQL客户端
        m_dbClient = drogon::orm::DbClient::newPgClient(connInfo, connNum);
        
        // 验证数据库连接
        if (!m_dbClient) {
            std::cerr << "数据库客户端创建失败" << std::endl;
            return false;
        }
        
        // 测试连接
        auto result = m_dbClient->execSqlSync("SELECT current_database()");
        std::string currentDb = result[0]["current_database"].as<std::string>();
        std::cout << "当前连接的数据库: " << currentDb << std::endl;
        
        // 只有在连接到blog数据库时才进行验证
        if (currentDb == "blog") {
            m_initialized = true;
            std::cout << "数据库连接初始化成功" << std::endl;
            return true;
        } else if (currentDb == "postgres") {
            // 如果连接到postgres数据库，也允许初始化
            m_initialized = true;
            std::cout << "已连接到postgres数据库，可以执行数据库创建操作" << std::endl;
            return true;
        } else {
            std::cerr << "警告：当前连接的数据库不是blog或postgres数据库" << std::endl;
            return false;
        }
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
        // 详细验证当前数据库连接
        std::cout << "createTables: 开始验证数据库连接..." << std::endl;
        
        if (!m_dbClient) {
            std::cerr << "createTables: 错误 - 数据库客户端未初始化" << std::endl;
            return false;
        }
        
        // 从连接字符串中解析当前数据库名称
        std::string expectedDb = "blog";
        std::regex dbRegex("dbname=([^ ]+)");
        std::smatch matches;
        if (std::regex_search(m_connectionInfo, matches, dbRegex) && matches.size() > 1) {
            std::string connDbName = matches[1].str();
            std::cout << "createTables: 连接字符串中的数据库名称: " << connDbName << std::endl;
        }
        
        // 验证当前数据库
        auto result = m_dbClient->execSqlSync("SELECT current_database()");
        std::string currentDb = result[0]["current_database"].as<std::string>();
        std::cout << "createTables: 当前实际连接的数据库: " << currentDb << std::endl;
        
        if (currentDb != "blog") {
            std::cerr << "错误：当前数据库不是blog数据库，无法创建表" << std::endl;
            
            // 尝试强制切换到blog数据库
            std::cout << "createTables: 尝试强制切换到blog数据库..." << std::endl;
            std::regex re("dbname=[^ ]+");
            std::string newConnStr = std::regex_replace(m_connectionInfo, re, "dbname=blog");
            
            try {
                m_dbClient = drogon::orm::DbClient::newPgClient(newConnStr, 1);
                auto checkResult = m_dbClient->execSqlSync("SELECT current_database()");
                std::string newDb = checkResult[0]["current_database"].as<std::string>();
                std::cout << "createTables: 强制切换后的数据库: " << newDb << std::endl;
                
                if (newDb != "blog") {
                    std::cerr << "createTables: 强制切换失败，仍然连接到错误的数据库" << std::endl;
                    return false;
                }
            } catch (const std::exception& e) {
                std::cerr << "createTables: 强制切换数据库时出错: " << e.what() << std::endl;
                return false;
            }
        }
        
        std::cout << "开始在blog数据库中创建表..." << std::endl;
        
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
                is_banned BOOLEAN DEFAULT FALSE,
                ban_reason TEXT,
                banned_at TIMESTAMP,
                banned_by VARCHAR(36) REFERENCES users(uuid),
                created_at TIMESTAMP NOT NULL DEFAULT CURRENT_TIMESTAMP,
                updated_at TIMESTAMP NOT NULL DEFAULT CURRENT_TIMESTAMP
            )
        )");
        
        // 创建文章表 - 使用user_uuid引用用户表，slug在同一用户下唯一
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
                updated_at TIMESTAMP NOT NULL DEFAULT CURRENT_TIMESTAMP,
                UNIQUE(user_uuid, slug)
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
        
        // 创建管理员操作日志表
        m_dbClient->execSqlSync(R"(
            CREATE TABLE IF NOT EXISTS admin_logs (
                id SERIAL PRIMARY KEY,
                action VARCHAR(50) NOT NULL,
                admin_uuid VARCHAR(36) NOT NULL REFERENCES users(uuid),
                target_uuid VARCHAR(36),
                details TEXT,
                created_at TIMESTAMP NOT NULL DEFAULT CURRENT_TIMESTAMP
            )
        )");
        
        // 创建管理员日志索引
        m_dbClient->execSqlSync("CREATE INDEX IF NOT EXISTS idx_admin_logs_admin_uuid ON admin_logs(admin_uuid)");
        m_dbClient->execSqlSync("CREATE INDEX IF NOT EXISTS idx_admin_logs_created_at ON admin_logs(created_at)");
        
        // 创建用户外链表
        m_dbClient->execSqlSync(R"(
            CREATE TABLE IF NOT EXISTS user_links (
                id SERIAL PRIMARY KEY,
                user_uuid VARCHAR(36) NOT NULL REFERENCES users(uuid) ON DELETE CASCADE,
                link_type VARCHAR(50) NOT NULL,
                link_url VARCHAR(255) NOT NULL,
                created_at TIMESTAMP NOT NULL DEFAULT CURRENT_TIMESTAMP,
                updated_at TIMESTAMP NOT NULL DEFAULT CURRENT_TIMESTAMP,
                UNIQUE(user_uuid, link_type)
            )
        )");
        
        // 创建用户外链索引
        m_dbClient->execSqlSync("CREATE INDEX IF NOT EXISTS idx_user_links_user_uuid ON user_links(user_uuid)");
        m_dbClient->execSqlSync("CREATE INDEX IF NOT EXISTS idx_user_links_link_type ON user_links(link_type)");
        
        std::cout << "所有数据库表创建成功" << std::endl;
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

        // 重新连接到新创建的数据库
        std::string newConnStr = std::regex_replace(m_connectionInfo, re, "dbname=" + dbName);
        m_dbClient = drogon::orm::DbClient::newPgClient(newConnStr, 1);
        
        // 验证新连接
        auto newResult = m_dbClient->execSqlSync("SELECT current_database()");
        std::string newDb = newResult[0]["current_database"].as<std::string>();
        std::cout << "已重新连接到数据库: " << newDb << std::endl;
        
        // 更新连接状态
        if (newDb == dbName) {
            m_initialized = true;
            std::cout << "数据库连接状态已更新" << std::endl;
            return true;
        } else {
            std::cerr << "错误：无法连接到目标数据库 " << dbName << std::endl;
            return false;
        }
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

// 添加数据库表和列的中文注释
bool DbManager::createComments() {
    try {
        if (!m_dbClient) {
            std::cerr << "createComments: 错误 - 数据库客户端未初始化" << std::endl;
            return false;
        }
        
        std::cout << "开始添加数据库表和列的中文注释..." << std::endl;
        
        // users表注释
        m_dbClient->execSqlSync("COMMENT ON TABLE users IS '用户表，存储所有用户信息'");
        m_dbClient->execSqlSync("COMMENT ON COLUMN users.uuid IS '用户UUID，全局唯一标识符'");
        m_dbClient->execSqlSync("COMMENT ON COLUMN users.username IS '用户名，用于登录'");
        m_dbClient->execSqlSync("COMMENT ON COLUMN users.password IS '密码哈希值'");
        m_dbClient->execSqlSync("COMMENT ON COLUMN users.email IS '电子邮箱'");
        m_dbClient->execSqlSync("COMMENT ON COLUMN users.display_name IS '显示名称，用于前端展示'");
        m_dbClient->execSqlSync("COMMENT ON COLUMN users.bio IS '个人简介'");
        m_dbClient->execSqlSync("COMMENT ON COLUMN users.avatar IS '头像URL'");
        m_dbClient->execSqlSync("COMMENT ON COLUMN users.is_admin IS '是否为管理员'");
        m_dbClient->execSqlSync("COMMENT ON COLUMN users.is_banned IS '是否被封禁'");
        m_dbClient->execSqlSync("COMMENT ON COLUMN users.ban_reason IS '封禁原因'");
        m_dbClient->execSqlSync("COMMENT ON COLUMN users.banned_at IS '封禁时间'");
        m_dbClient->execSqlSync("COMMENT ON COLUMN users.banned_by IS '执行封禁的管理员UUID'");
        m_dbClient->execSqlSync("COMMENT ON COLUMN users.created_at IS '创建时间'");
        m_dbClient->execSqlSync("COMMENT ON COLUMN users.updated_at IS '更新时间'");
        
        // user_links表注释
        m_dbClient->execSqlSync("COMMENT ON TABLE user_links IS '用户外部链接表，存储用户的社交媒体等链接'");
        m_dbClient->execSqlSync("COMMENT ON COLUMN user_links.id IS '链接ID，自增主键'");
        m_dbClient->execSqlSync("COMMENT ON COLUMN user_links.user_uuid IS '用户UUID，关联到users表'");
        m_dbClient->execSqlSync("COMMENT ON COLUMN user_links.link_type IS '链接类型，如github、website等'");
        m_dbClient->execSqlSync("COMMENT ON COLUMN user_links.link_url IS '链接URL'");
        m_dbClient->execSqlSync("COMMENT ON COLUMN user_links.created_at IS '创建时间'");
        m_dbClient->execSqlSync("COMMENT ON COLUMN user_links.updated_at IS '更新时间'");
        
        // articles表注释
        m_dbClient->execSqlSync("COMMENT ON TABLE articles IS '文章表，存储博客文章内容'");
        m_dbClient->execSqlSync("COMMENT ON COLUMN articles.id IS '文章ID，自增主键'");
        m_dbClient->execSqlSync("COMMENT ON COLUMN articles.title IS '文章标题'");
        m_dbClient->execSqlSync("COMMENT ON COLUMN articles.slug IS '文章别名，用于URL'");
        m_dbClient->execSqlSync("COMMENT ON COLUMN articles.content IS '文章内容'");
        m_dbClient->execSqlSync("COMMENT ON COLUMN articles.summary IS '文章摘要'");
        m_dbClient->execSqlSync("COMMENT ON COLUMN articles.cover_image IS '封面图片URL'");
        m_dbClient->execSqlSync("COMMENT ON COLUMN articles.view_count IS '浏览量'");
        m_dbClient->execSqlSync("COMMENT ON COLUMN articles.user_uuid IS '作者UUID，关联到users表'");
        m_dbClient->execSqlSync("COMMENT ON COLUMN articles.is_published IS '是否发布'");
        m_dbClient->execSqlSync("COMMENT ON COLUMN articles.created_at IS '创建时间'");
        m_dbClient->execSqlSync("COMMENT ON COLUMN articles.updated_at IS '更新时间'");
        
        // categories表注释
        m_dbClient->execSqlSync("COMMENT ON TABLE categories IS '分类表，存储文章分类'");
        m_dbClient->execSqlSync("COMMENT ON COLUMN categories.id IS '分类ID，自增主键'");
        m_dbClient->execSqlSync("COMMENT ON COLUMN categories.name IS '分类名称'");
        m_dbClient->execSqlSync("COMMENT ON COLUMN categories.slug IS '分类别名，用于URL'");
        m_dbClient->execSqlSync("COMMENT ON COLUMN categories.description IS '分类描述'");
        m_dbClient->execSqlSync("COMMENT ON COLUMN categories.parent_id IS '父分类ID，用于分类层级'");
        
        // article_categories表注释
        m_dbClient->execSqlSync("COMMENT ON TABLE article_categories IS '文章分类关联表，多对多关系'");
        m_dbClient->execSqlSync("COMMENT ON COLUMN article_categories.article_id IS '文章ID，关联到articles表'");
        m_dbClient->execSqlSync("COMMENT ON COLUMN article_categories.category_id IS '分类ID，关联到categories表'");
        
        // tags表注释
        m_dbClient->execSqlSync("COMMENT ON TABLE tags IS '标签表，存储文章标签'");
        m_dbClient->execSqlSync("COMMENT ON COLUMN tags.id IS '标签ID，自增主键'");
        m_dbClient->execSqlSync("COMMENT ON COLUMN tags.name IS '标签名称'");
        m_dbClient->execSqlSync("COMMENT ON COLUMN tags.slug IS '标签别名，用于URL'");
        
        // article_tags表注释
        m_dbClient->execSqlSync("COMMENT ON TABLE article_tags IS '文章标签关联表，多对多关系'");
        m_dbClient->execSqlSync("COMMENT ON COLUMN article_tags.article_id IS '文章ID，关联到articles表'");
        m_dbClient->execSqlSync("COMMENT ON COLUMN article_tags.tag_id IS '标签ID，关联到tags表'");
        
        // comments表注释
        m_dbClient->execSqlSync("COMMENT ON TABLE comments IS '评论表，存储文章评论'");
        m_dbClient->execSqlSync("COMMENT ON COLUMN comments.id IS '评论ID，自增主键'");
        m_dbClient->execSqlSync("COMMENT ON COLUMN comments.content IS '评论内容'");
        m_dbClient->execSqlSync("COMMENT ON COLUMN comments.article_id IS '文章ID，关联到articles表'");
        m_dbClient->execSqlSync("COMMENT ON COLUMN comments.user_uuid IS '评论者UUID，关联到users表，可为NULL（游客评论）'");
        m_dbClient->execSqlSync("COMMENT ON COLUMN comments.parent_id IS '父评论ID，用于回复功能，关联到comments表'");
        m_dbClient->execSqlSync("COMMENT ON COLUMN comments.author_name IS '评论者名称，用于游客评论'");
        m_dbClient->execSqlSync("COMMENT ON COLUMN comments.author_email IS '评论者邮箱，用于游客评论'");
        m_dbClient->execSqlSync("COMMENT ON COLUMN comments.created_at IS '创建时间'");
        
        // admin_logs表注释
        m_dbClient->execSqlSync("COMMENT ON TABLE admin_logs IS '管理员操作日志表，记录管理操作'");
        m_dbClient->execSqlSync("COMMENT ON COLUMN admin_logs.id IS '日志ID，自增主键'");
        m_dbClient->execSqlSync("COMMENT ON COLUMN admin_logs.action IS '操作类型，如DELETE_USER、BAN_USER等'");
        m_dbClient->execSqlSync("COMMENT ON COLUMN admin_logs.admin_uuid IS '管理员UUID，关联到users表'");
        m_dbClient->execSqlSync("COMMENT ON COLUMN admin_logs.target_uuid IS '操作目标UUID'");
        m_dbClient->execSqlSync("COMMENT ON COLUMN admin_logs.details IS '操作详情，JSON格式'");
        m_dbClient->execSqlSync("COMMENT ON COLUMN admin_logs.created_at IS '创建时间'");
        
        // settings表注释
        m_dbClient->execSqlSync("COMMENT ON TABLE settings IS '系统设置表，存储全局配置'");
        m_dbClient->execSqlSync("COMMENT ON COLUMN settings.key IS '设置键名，主键'");
        m_dbClient->execSqlSync("COMMENT ON COLUMN settings.value IS '设置值'");
        m_dbClient->execSqlSync("COMMENT ON COLUMN settings.updated_at IS '更新时间'");
        
        std::cout << "数据库表和列的中文注释添加成功" << std::endl;
        return true;
    } catch (const drogon::orm::DrogonDbException& e) {
        std::cerr << "添加数据库注释错误: " << e.base().what() << std::endl;
        return false;
    }
}

bool DbManager::init() {
    std::cout << "初始化数据库管理器..." << std::endl;

    try {
        // 解析连接字符串获取数据库名称
        std::string dbName;
        std::regex re("dbname=([^ ]+)");
        std::smatch match;
        if (std::regex_search(m_connectionInfo, match, re) && match.size() > 1) {
            dbName = match[1].str();
            std::cout << "数据库名称: " << dbName << std::endl;
        } else {
            std::cerr << "无法从连接字符串解析数据库名称" << std::endl;
            return false;
        }

        // 检查数据库是否需要创建
        bool needInitDb = false;
        if (!databaseExists(dbName)) {
            if (!createDatabase(dbName)) {
                std::cerr << "创建数据库失败" << std::endl;
                return false;
            }
            needInitDb = true;
        }

        // 连接到目标数据库
        m_dbClient = drogon::orm::DbClient::newPgClient(m_connectionInfo, m_connectionNumber);
        
        std::cout << "数据库连接成功" << std::endl;

        // 如果是新创建的数据库，初始化表结构
        if (needInitDb) {
            if (!createTables()) {
                std::cerr << "创建表失败" << std::endl;
                return false;
            }
            
            if (!insertDefaultData()) {
                std::cerr << "插入默认数据失败" << std::endl;
                return false;
            }
            
            // 添加数据库注释
            if (!createComments()) {
                std::cerr << "添加数据库注释失败" << std::endl;
                // 注释失败不影响系统功能，继续执行
            }
        }

        return true;
    } catch (const drogon::orm::DrogonDbException &e) {
        std::cerr << "数据库初始化错误: " << e.base().what() << std::endl;
        return false;
    }
}

// 插入默认数据
bool DbManager::insertDefaultData() {
    try {
        if (!m_dbClient) {
            std::cerr << "insertDefaultData: 错误 - 数据库客户端未初始化" << std::endl;
            return false;
        }
        
        std::cout << "开始插入默认数据..." << std::endl;
        
        // 检查是否已有管理员账户
        auto adminResult = m_dbClient->execSqlSync(
            "SELECT COUNT(*) as count FROM users WHERE is_admin = TRUE"
        );
        
        int adminCount = adminResult[0]["count"].as<int>();
        
        if (adminCount == 0) {
            // 创建默认管理员账户
            std::string adminUuid = drogon::utils::getUuid();
            std::string adminUsername = "admin";
            std::string adminPassword = "$argon2id$v=19$m=65536,t=2,p=1$c29tZXNhbHQ$GpZ3SLIzGWyAZU/oFiWUrg"; // "password" 的哈希值
            
            m_dbClient->execSqlSync(
                "INSERT INTO users (uuid, username, password, email, display_name, is_admin, created_at, updated_at) "
                "VALUES ($1, $2, $3, $4, $5, TRUE, CURRENT_TIMESTAMP, CURRENT_TIMESTAMP)",
                adminUuid, adminUsername, adminPassword, "admin@example.com", "管理员"
            );
            
            std::cout << "创建默认管理员账户: " << adminUsername << std::endl;
        } else {
            std::cout << "已存在管理员账户，跳过创建默认管理员" << std::endl;
        }
        
        // 检查是否已有默认分类
        auto categoryResult = m_dbClient->execSqlSync(
            "SELECT COUNT(*) as count FROM categories"
        );
        
        int categoryCount = categoryResult[0]["count"].as<int>();
        
        if (categoryCount == 0) {
            // 创建默认分类
            m_dbClient->execSqlSync(
                "INSERT INTO categories (name, slug, created_at, updated_at) "
                "VALUES ('未分类', 'uncategorized', CURRENT_TIMESTAMP, CURRENT_TIMESTAMP)"
            );
            
            m_dbClient->execSqlSync(
                "INSERT INTO categories (name, slug, created_at, updated_at) "
                "VALUES ('技术', 'technology', CURRENT_TIMESTAMP, CURRENT_TIMESTAMP)"
            );
            
            m_dbClient->execSqlSync(
                "INSERT INTO categories (name, slug, created_at, updated_at) "
                "VALUES ('生活', 'life', CURRENT_TIMESTAMP, CURRENT_TIMESTAMP)"
            );
            
            std::cout << "创建默认分类完成" << std::endl;
        } else {
            std::cout << "已存在分类，跳过创建默认分类" << std::endl;
        }
        
        // 检查是否已有默认标签
        auto tagResult = m_dbClient->execSqlSync(
            "SELECT COUNT(*) as count FROM tags"
        );
        
        int tagCount = tagResult[0]["count"].as<int>();
        
        if (tagCount == 0) {
            // 创建默认标签
            m_dbClient->execSqlSync(
                "INSERT INTO tags (name, slug, created_at, updated_at) "
                "VALUES ('C++', 'cpp', CURRENT_TIMESTAMP, CURRENT_TIMESTAMP)"
            );
            
            m_dbClient->execSqlSync(
                "INSERT INTO tags (name, slug, created_at, updated_at) "
                "VALUES ('Web开发', 'web-dev', CURRENT_TIMESTAMP, CURRENT_TIMESTAMP)"
            );
            
            m_dbClient->execSqlSync(
                "INSERT INTO tags (name, slug, created_at, updated_at) "
                "VALUES ('数据库', 'database', CURRENT_TIMESTAMP, CURRENT_TIMESTAMP)"
            );
            
            std::cout << "创建默认标签完成" << std::endl;
        } else {
            std::cout << "已存在标签，跳过创建默认标签" << std::endl;
        }
        
        std::cout << "默认数据插入完成" << std::endl;
        return true;
    } catch (const drogon::orm::DrogonDbException& e) {
        std::cerr << "插入默认数据错误: " << e.base().what() << std::endl;
        return false;
    }
} 