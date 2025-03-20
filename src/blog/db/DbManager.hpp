#pragma once

#include "Nginx.hpp"
#include "ConnectionPool.hpp"
#include "ConnectionWrapper.hpp"
#include <string>
#include <memory>
#include <mutex>
#include <functional>

// 使用MySQL Connector/C++ DevAPI
#include <mysqlx/xdevapi.h>

// 查询回调类型
using QueryCallback = std::function<void(mysqlx::RowResult&)>;
using TransactionCallback = std::function<bool(mysqlx::Session&)>;

/**
 * 数据库连接管理器
 * 单例模式，负责维护MySQL数据库连接
 */
class DbManager {
public:
    /**
     * 获取数据库管理器实例
     */
    static DbManager& getInstance();

    /**
     * 初始化数据库连接
     * @param connectionString 连接字符串，格式如: "host=localhost;user=root;password=secret;database=blog"
     * @param minConnections 最小连接数
     * @param maxConnections 最大连接数
     * @return 是否成功初始化
     */
    bool initialize(const std::string& connectionString, int minConnections = 2, int maxConnections = 10);

    /**
     * 检查数据库连接是否有效
     * @return 连接是否有效
     */
    bool isConnected();

    /**
     * 执行SQL查询
     * @param sql SQL语句
     * @return 查询结果集，失败时抛出异常
     */
    mysqlx::RowResult executeQuery(const std::string& sql);

    /**
     * 执行带回调的SQL查询
     * @param sql SQL语句
     * @param callback 处理结果的回调函数
     * @return 是否成功执行
     */
    bool executeQuery(const std::string& sql, QueryCallback callback);

    /**
     * 执行SQL更新（INSERT, UPDATE, DELETE等）
     * @param sql SQL语句
     * @return 受影响的行数，失败时返回-1
     */
    int executeUpdate(const std::string& sql);

    /**
     * 在事务中执行操作
     * @param callback 事务回调函数，返回true表示提交，返回false表示回滚
     * @return 事务是否成功执行并提交
     */
    bool executeTransaction(TransactionCallback callback);

    /**
     * 获取最后一次插入操作的ID
     * @return 最后插入的ID
     */
    uint64_t getLastInsertId();

    /**
     * 关闭数据库连接
     */
    void close();

    /**
     * 创建数据库表结构
     * @return 是否成功创建表
     */
    bool createTables();

    /**
     * 获取连接池状态
     * @return 连接池状态
     */
    ConnectionPool::PoolStatus getPoolStatus();

    /**
     * 获取连接池实例
     * @return 连接池引用
     */
    ConnectionPool& getPool() { return m_pool; }

private:
    // 单例模式，私有构造函数
    DbManager();
    ~DbManager();

    // 删除复制构造和赋值操作
    DbManager(const DbManager&) = delete;
    DbManager& operator=(const DbManager&) = delete;

    // 连接池
    ConnectionPool& m_pool;
    
    // 初始化状态
    std::atomic<bool> m_initialized{false};
    
    // 互斥锁，用于初始化保护
    std::mutex m_init_mutex;
};