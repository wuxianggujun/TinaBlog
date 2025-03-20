#pragma once

#include "Nginx.hpp"
#include <string>
#include <memory>
#include <mutex>
#include <queue>
#include <condition_variable>
#include <thread>
#include <atomic>
#include <chrono>
#include <set>
#include <mysqlx/xdevapi.h>

/**
 * 数据库连接池
 * 管理MySQL连接的创建、复用和释放
 */
class ConnectionPool {
public:
    /**
     * 获取连接池单例
     * @return ConnectionPool单例引用
     */
    static ConnectionPool& getInstance();

    /**
     * 初始化连接池
     * @param connStr 数据库连接字符串
     * @param minConn 最小连接数
     * @param maxConn 最大连接数
     * @param timeout 获取连接超时时间(毫秒)
     * @return 初始化是否成功
     */
    bool initialize(const std::string& connStr, int minConn = 2, int maxConn = 10, int timeout = 5000);

    /**
     * 获取数据库连接
     * @param timeout_ms 获取连接的超时时间(毫秒)
     * @return 数据库连接，如果超时则抛出异常
     */
    std::shared_ptr<mysqlx::Session> getConnection(int timeout_ms = 5000);

    /**
     * 释放连接回连接池
     * @param conn 要释放的连接
     */
    void releaseConnection(std::shared_ptr<mysqlx::Session> conn);

    /**
     * 关闭连接池
     */
    void close();

    /**
     * 获取连接池状态
     * @return 包含连接池状态的结构体
     */
    struct PoolStatus {
        size_t idle_connections;      // 空闲连接数
        size_t active_connections;    // 活跃连接数
        int max_connections;          // 最大连接数
        int min_connections;          // 最小连接数
        int waiting_threads;          // 等待连接的线程数
    };
    
    PoolStatus getStatus();

private:
    // 单例模式私有构造
    ConnectionPool();
    ~ConnectionPool();
    ConnectionPool(const ConnectionPool&) = delete;
    ConnectionPool& operator=(const ConnectionPool&) = delete;

    // 验证连接是否有效
    bool validateConnection(std::shared_ptr<mysqlx::Session> conn);
    
    // 创建新连接
    std::shared_ptr<mysqlx::Session> createConnection();
    
    // 维护连接池的线程函数
    void maintainConnections();

    // 连接池数据
    std::deque<std::shared_ptr<mysqlx::Session>> m_idle_connections;  // 空闲连接队列
    std::set<std::shared_ptr<mysqlx::Session>> m_active_connections;  // 活动连接集合
    std::string m_connection_string;                                 // 连接字符串
    std::mutex m_mutex;                                              // 互斥锁
    std::condition_variable m_condition;                             // 条件变量
    int m_max_connections = 10;                                      // 最大连接数
    int m_min_connections = 2;                                       // 最小连接数
    int m_connection_timeout = 5000;                                 // 连接超时(毫秒)
    std::atomic<bool> m_running{true};                               // 连接池是否运行中
    std::thread m_maintenance_thread;                                // 维护线程
    std::atomic<int> m_waiting_threads{0};                           // 等待线程计数
}; 