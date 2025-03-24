#pragma once

#include <string>
#include <memory>
#include <mutex>
#include <functional>
#include <atomic>

// 使用PostgreSQL的libpqxx库
#include <pqxx/pqxx>

// 查询回调类型
using QueryCallback = std::function<void(pqxx::result&)>;
using TransactionCallback = std::function<bool(pqxx::work&)>;

/**
 * 数据库连接管理器
 * 单例模式，负责维护PostgreSQL数据库连接
 */
class DbManager {
public:
    /**
     * 获取数据库管理器实例
     */
    static DbManager& getInstance();

    /**
     * 初始化数据库连接
     * @param connectionString 连接字符串，格式如: "host=localhost user=postgres password=secret dbname=blog"
     * @return 是否成功初始化
     */
    bool initialize(const std::string& connectionString);

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
    pqxx::result executeQuery(const std::string& sql);

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
     * @param table 表名
     * @param idColumn ID列名
     * @return 最后插入的ID
     */
    uint64_t getLastInsertId(const std::string& table = "", const std::string& idColumn = "id");

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
     * 创建数据库（特殊处理，不在事务中执行）
     * @param dbName 数据库名称
     * @return 是否成功创建数据库
     */
    bool createDatabase(const std::string& dbName);
    
    /**
     * 检查数据库是否存在
     * @param dbName 数据库名称
     * @return 是否存在
     */
    bool databaseExists(const std::string& dbName);

private:
    // 单例模式，私有构造函数
    DbManager();
    ~DbManager();

    // 删除复制构造和赋值操作
    DbManager(const DbManager&) = delete;
    DbManager& operator=(const DbManager&) = delete;

    // 直接使用一个数据库连接，而不是连接池
    std::unique_ptr<pqxx::connection> m_connection;
    
    // 连接字符串
    std::string m_connectionString;
    
    // 初始化状态
    std::atomic<bool> m_initialized{false};
    
    // 互斥锁，用于初始化保护
    std::mutex m_init_mutex;
}; 