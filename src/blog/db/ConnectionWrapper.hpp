#pragma once

#include "ConnectionPool.hpp"
#include <memory>
#include <mysqlx/xdevapi.h>

/**
 * 数据库连接包装器
 * 使用RAII模式自动归还连接到连接池
 */
class ConnectionWrapper {
public:
    /**
     * 构造函数，从连接池获取连接
     * @param pool 连接池引用
     * @param timeout_ms 获取连接的超时时间，默认5秒
     */
    ConnectionWrapper(ConnectionPool& pool, int timeout_ms = 5000)
        : m_pool(pool) {
        m_conn = pool.getConnection(timeout_ms);
    }
    
    /**
     * 析构函数，自动归还连接
     */
    ~ConnectionWrapper() {
        m_pool.releaseConnection(m_conn);
    }
    
    /**
     * 获取原始连接
     * @return 数据库连接引用
     */
    mysqlx::Session& get() {
        return *m_conn;
    }
    
    /**
     * 解引用操作符
     * @return 数据库连接引用
     */
    mysqlx::Session& operator*() {
        return *m_conn;
    }
    
    /**
     * 成员访问操作符
     * @return 数据库连接指针
     */
    mysqlx::Session* operator->() {
        return m_conn.get();
    }
    
    /**
     * 检查连接是否有效
     * @return 连接是否有效
     */
    bool isValid() const {
        return m_conn != nullptr;
    }
    
private:
    std::shared_ptr<mysqlx::Session> m_conn;  // 数据库连接
    ConnectionPool& m_pool;                  // 连接池引用
    
    // 禁止复制
    ConnectionWrapper(const ConnectionWrapper&) = delete;
    ConnectionWrapper& operator=(const ConnectionWrapper&) = delete;
}; 