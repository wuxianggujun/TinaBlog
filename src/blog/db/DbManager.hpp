#pragma once

#include <string>
#include <memory>
#include <mutex>
#include <functional>
#include <atomic>
#include <drogon/orm/DbClient.h>
#include <drogon/orm/Exception.h>
#include <stdexcept>
#include <iostream>

/**
 * 数据库连接管理器
 * 单例模式，使用Drogon的DbClient
 */
class DbManager {
public:
    /**
     * 获取数据库管理器实例
     */
    static DbManager& getInstance();

    /**
     * 初始化数据库连接
     * @param connInfo 连接字符串，如: "host=localhost user=postgres password=secret dbname=blog"
     * @param connNum 连接池中的连接数量
     * @return 是否成功初始化
     */
    bool initialize(const std::string& connInfo, size_t connNum = 3);

    /**
     * 检查数据库连接是否有效
     * @return 连接是否有效
     */
    bool isConnected();

    /**
     * 异步执行SQL查询（无需转换结果）
     * @param sql SQL语句
     * @param resultCallback 成功回调
     * @param exceptionCallback 异常回调
     * @param args 绑定参数
     */
    template <typename... Args>
    void executeQuery(const std::string& sql,
                     std::function<void(const drogon::orm::Result&)> resultCallback,
                     std::function<void(const drogon::orm::DrogonDbException&)> exceptionCallback,
                     Args&&... args);

    /**
     * 同步执行SQL查询
     * @param sql SQL语句
     * @param args 绑定参数
     * @return 查询结果
     * @throws std::runtime_error 数据库操作异常
     */
    template <typename... Args>
    drogon::orm::Result execSyncQuery(const std::string& sql, Args&&... args);

    /**
     * 创建数据库
     * @param dbName 数据库名称
     * @return 是否成功创建
     */
    bool createDatabase(const std::string& dbName);
    
    /**
     * 检查数据库是否存在
     * @param dbName 数据库名称
     * @return 数据库是否存在
     */
    bool databaseExists(const std::string& dbName);
    
    /**
     * 创建数据库表结构
     * @return 是否成功创建表
     */
    bool createTables();

    /**
     * 为数据库表和列添加中文注释
     * @return 是否成功添加注释
     */
    bool createComments();

    /**
     * 初始化数据库（创建数据库、表结构、注释等）
     * @return 是否初始化成功
     */
    bool init();

    /**
     * 插入默认数据
     * @return 是否成功插入默认数据
     */
    bool insertDefaultData();

    /**
     * 析构函数
     */
    virtual ~DbManager() = default;

private:
    // 单例模式，私有构造函数
    DbManager();

    // 删除复制构造和赋值操作
    DbManager(const DbManager&) = delete;
    DbManager& operator=(const DbManager&) = delete;

    // 数据库客户端
    drogon::orm::DbClientPtr m_dbClient;
    
    // 连接信息
    std::string m_connectionInfo;
    
    // 连接池大小
    size_t m_connectionNumber{3};
    
    // 初始化状态
    std::atomic<bool> m_initialized{false};
    
    // 互斥锁，用于初始化保护
    std::mutex m_init_mutex;
};

// 模板函数实现
template <typename... Args>
void DbManager::executeQuery(const std::string& sql,
                            std::function<void(const drogon::orm::Result&)> resultCallback,
                            std::function<void(const drogon::orm::DrogonDbException&)> exceptionCallback,
                            Args&&... args) {
    if (m_dbClient) {
        m_dbClient->execSqlAsync(sql, 
                              std::move(resultCallback), 
                              std::move(exceptionCallback), 
                              std::forward<Args>(args)...);
    } else {
        std::cerr << "数据库客户端未初始化，无法执行查询" << std::endl;
        // 不要尝试调用回调，因为无法创建DrogonDbException
    }
}

template <typename... Args>
drogon::orm::Result DbManager::execSyncQuery(const std::string& sql, Args&&... args) {
    if (m_dbClient) {
        return m_dbClient->execSqlSync(sql, std::forward<Args>(args)...);
    } else {
        throw std::runtime_error("数据库客户端未初始化");
    }
} 