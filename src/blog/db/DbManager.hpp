#pragma once

#include "Nginx.hpp"
#include <string>
#include <memory>
#include <mutex>

// 使用MySQL Connector/C++ DevAPI
#include <mysqlx/xdevapi.h>

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
     * @param connStr 连接字符串，格式如: "host=localhost;user=root;password=secret;database=blog"
     * @param autoInit 自动初始化数据库表
     * @return 是否成功初始化
     */
    bool initialize(const std::string& connStr, bool autoInit = true);

    /**
     * 获取会话
     * @return MySQL会话对象
     */
    mysqlx::Session& getSession();

    /**
     * 检查数据库连接是否有效
     * @return 连接是否有效
     */
    bool isConnected() const;

    /**
     * 执行SQL查询
     * @param sql SQL语句
     * @return 查询结果集，失败时抛出异常
     */
    mysqlx::RowResult executeQuery(const std::string& sql);

    /**
     * 执行SQL更新（INSERT, UPDATE, DELETE等）
     * @param sql SQL语句
     * @return 受影响的行数，失败时返回-1
     */
    int executeUpdate(const std::string& sql);

    /**
     * 获取最后一次插入操作的ID
     * @return 最后插入的ID
     */
    uint64_t getLastInsertId();

    /**
     * 关闭数据库连接
     */
    void close();

private:
    // 私有构造函数，防止外部创建实例
    DbManager();
    ~DbManager();

    // 删除复制构造和赋值操作
    DbManager(const DbManager&) = delete;
    DbManager& operator=(const DbManager&) = delete;

    // 创建数据库表
    bool createTables();
    
    // 解析连接字符串
    bool parseConnectionString(const std::string& connStr);

private:
    // X DevAPI会话 - 使用裸指针，在close()中手动管理内存
    mysqlx::Session* session_;
    
    // 连接参数
    std::string host_;
    std::string user_;
    std::string password_;
    std::string database_;
    unsigned int port_;
    
    // 连接状态
    bool connected_;
    
    // 互斥锁，保护连接操作的线程安全
    std::mutex mutex_;
};