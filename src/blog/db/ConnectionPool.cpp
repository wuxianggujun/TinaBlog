#include "ConnectionPool.hpp"
#include <regex>
#include <random>

// 获取连接池单例
ConnectionPool& ConnectionPool::getInstance() {
    static ConnectionPool instance;
    return instance;
}

// 构造函数
ConnectionPool::ConnectionPool() : m_running(true) {
    ngx_log_error(NGX_LOG_INFO, ngx_cycle->log, 0, "ConnectionPool created");
}

// 析构函数
ConnectionPool::~ConnectionPool() {
    close();
    ngx_log_error(NGX_LOG_INFO, ngx_cycle->log, 0, "ConnectionPool destroyed");
}

// 初始化连接池
bool ConnectionPool::initialize(const std::string& connStr, int minConn, int maxConn, int timeout) {
    std::lock_guard<std::timed_mutex> lock(m_mutex);
    
    if (m_connection_string.length() > 0) {
        // 已经初始化过
        ngx_log_error(NGX_LOG_WARN, ngx_cycle->log, 0, "ConnectionPool already initialized");
        return true;
    }
    
    m_connection_string = connStr;
    m_min_connections = minConn;
    m_max_connections = maxConn;
    m_connection_timeout = timeout;
    
    // 测试连接是否可用
    try {
        auto testConn = createConnection();
        if (!testConn) {
            ngx_log_error(NGX_LOG_ERR, ngx_cycle->log, 0, "Failed to create test connection");
            return false;
        }
        
        // 第一个测试连接成功，保留它
        m_idle_connections.push_back(testConn);
        
        // 预创建剩余的最小连接数
        for (int i = 1; i < m_min_connections; i++) {
            try {
                auto conn = createConnection();
                if (conn) {
                    m_idle_connections.push_back(conn);
                }
            } catch (const std::exception& e) {
                // 如果创建额外连接失败，记录错误但不影响整体初始化
                ngx_log_error(NGX_LOG_WARN, ngx_cycle->log, 0, 
                             "Failed to create additional connection %d: %s", i, e.what());
            }
        }
        
        // 启动维护线程
        m_running = true;
        m_maintenance_thread = std::thread(&ConnectionPool::maintainConnections, this);
        
        ngx_log_error(NGX_LOG_INFO, ngx_cycle->log, 0, 
                     "ConnectionPool initialized with %d connections (min=%d, max=%d)", 
                     m_idle_connections.size(), m_min_connections, m_max_connections);
        
        return true;
    } catch (const std::exception& e) {
        m_idle_connections.clear();
        ngx_log_error(NGX_LOG_ERR, ngx_cycle->log, 0, 
                     "ConnectionPool initialization failed: %s", e.what());
        return false;
    }
}

// 获取数据库连接
std::shared_ptr<mysqlx::Session> ConnectionPool::getConnection(int timeout_ms) {
    // 增加等待线程计数
    m_waiting_threads++;
    
    // 快速检查连接池状态，如果有足够空闲连接直接返回
    {
        std::unique_lock<std::timed_mutex> quick_check(m_mutex, std::try_to_lock);
        if (quick_check.owns_lock() && !m_idle_connections.empty()) {
            auto conn = m_idle_connections.front();
            m_idle_connections.pop_front();
            m_active_connections.insert(conn);
            m_waiting_threads--;
            return conn;
        }
    }
    
    std::unique_lock<std::timed_mutex> lock(m_mutex);
    auto start_time = std::chrono::steady_clock::now();
    
    // 等待可用连接或者可以创建新连接
    while(m_idle_connections.empty() && m_active_connections.size() >= m_max_connections) {
        // 记录等待情况
        if (m_waiting_threads > 3) {
            ngx_log_error(NGX_LOG_WARN, ngx_cycle->log, 0, 
                         "High connection demand: %d threads waiting for connection (active=%d, idle=%d, max=%d)", 
                         m_waiting_threads.load(), m_active_connections.size(), 
                         m_idle_connections.size(), m_max_connections);
        }
        
        auto wait_result = m_condition.wait_for(lock, std::chrono::milliseconds(timeout_ms / 4));
        
        if (wait_result == std::cv_status::timeout) {
            // 检查是否超过总超时时间
            auto now = std::chrono::steady_clock::now();
            if (std::chrono::duration_cast<std::chrono::milliseconds>(now - start_time).count() > timeout_ms) {
                // 减少等待线程计数
                m_waiting_threads--;
                
                ngx_log_error(NGX_LOG_ERR, ngx_cycle->log, 0, 
                             "Timeout waiting for database connection (pool status: active=%d, idle=%d, max=%d, waiting=%d)", 
                             m_active_connections.size(), m_idle_connections.size(), 
                             m_max_connections, m_waiting_threads.load());
                throw std::runtime_error("Timeout waiting for database connection");
            }
            
            // 没有超时，继续等待
            continue;
        }
        
        // 条件变量被通知，检查是否有空闲连接或可以创建新连接
        if (!m_idle_connections.empty() || m_active_connections.size() < m_max_connections) {
            break;
        }
    }
    
    std::shared_ptr<mysqlx::Session> conn;
    
    if (!m_idle_connections.empty()) {
        // 使用空闲连接
        conn = m_idle_connections.front();
        m_idle_connections.pop_front();
        
        // 检查连接是否有效
        if (!validateConnection(conn)) {
            ngx_log_error(NGX_LOG_WARN, ngx_cycle->log, 0, "Invalid connection from pool, creating new one");
            
            try {
                conn = createConnection();
            } catch (const std::exception& e) {
                // 减少等待线程计数
                m_waiting_threads--;
                
                ngx_log_error(NGX_LOG_ERR, ngx_cycle->log, 0, 
                             "Failed to create new connection: %s", e.what());
                throw std::runtime_error(std::string("Failed to create database connection: ") + e.what());
            }
        }
    } else {
        // 创建新连接
        try {
            conn = createConnection();
        } catch (const std::exception& e) {
            // 减少等待线程计数
            m_waiting_threads--;
            
            ngx_log_error(NGX_LOG_ERR, ngx_cycle->log, 0, 
                         "Failed to create new connection: %s", e.what());
            throw std::runtime_error(std::string("Failed to create database connection: ") + e.what());
        }
    }
    
    // 减少等待线程计数
    m_waiting_threads--;
    
    // 将连接加入活动连接集合
    m_active_connections.insert(conn);
    
    return conn;
}

// 释放连接
void ConnectionPool::releaseConnection(std::shared_ptr<mysqlx::Session> conn) {
    if (!conn) {
        return;
    }
    
    std::lock_guard<std::timed_mutex> lock(m_mutex);
    
    // 从活动连接中移除
    auto it = m_active_connections.find(conn);
    if (it != m_active_connections.end()) {
        m_active_connections.erase(it);
        
        // 如果连接有效，放回空闲池
        if (validateConnection(conn)) {
            m_idle_connections.push_back(conn);
        }
        
        // 通知等待的线程
        m_condition.notify_one();
    }
}

// 关闭连接池
void ConnectionPool::close() {
    // 停止维护线程
    {
        std::lock_guard<std::timed_mutex> lock(m_mutex);
        m_running = false;
    }
    
    // 等待维护线程结束
    if (m_maintenance_thread.joinable()) {
        m_condition.notify_all();  // 唤醒维护线程
        m_maintenance_thread.join();
    }
    
    // 关闭并清理所有连接
    std::lock_guard<std::timed_mutex> lock(m_mutex);
    
    for (auto& conn : m_idle_connections) {
        try {
            conn->close();
        } catch (...) {
            // 忽略关闭错误
        }
    }
    m_idle_connections.clear();
    
    for (auto& conn : m_active_connections) {
        try {
            conn->close();
        } catch (...) {
            // 忽略关闭错误
        }
    }
    m_active_connections.clear();
    
    ngx_log_error(NGX_LOG_INFO, ngx_cycle->log, 0, "ConnectionPool closed");
}

// 获取连接池状态
ConnectionPool::PoolStatus ConnectionPool::getStatus() {
    std::lock_guard<std::timed_mutex> lock(m_mutex);
    
    ConnectionPool::PoolStatus status;
    status.idle_connections = m_idle_connections.size();
    status.active_connections = m_active_connections.size();
    status.max_connections = m_max_connections;
    status.min_connections = m_min_connections;
    status.waiting_threads = m_waiting_threads.load();
    return status;
}

// 验证连接是否有效
bool ConnectionPool::validateConnection(std::shared_ptr<mysqlx::Session> conn) {
    if (!conn) {
        return false;
    }
    
    try {
        // 执行简单查询验证连接
        conn->sql("SELECT 1").execute();
        return true;
    } catch (const std::exception& e) {
        ngx_log_error(NGX_LOG_WARN, ngx_cycle->log, 0, 
                     "Connection validation failed: %s", e.what());
        return false;
    }
}

// 创建新连接
std::shared_ptr<mysqlx::Session> ConnectionPool::createConnection() {
    if (m_connection_string.empty()) {
        ngx_log_error(NGX_LOG_ERR, ngx_cycle->log, 0, "Connection string is empty");
        throw std::runtime_error("Connection string is empty");
    }
    
    try {
        // 解析连接字符串
        std::string host = "localhost";
        std::string user = "root";
        std::string password = "";
        std::string database = "";
        int port = 33060;
        
        // 解析格式：host=...;user=...;password=...;database=...;port=...
        std::regex pattern("(\\w+)=([^;]+)");
        auto begin = std::sregex_iterator(m_connection_string.begin(), m_connection_string.end(), pattern);
        auto end = std::sregex_iterator();
        
        for (std::sregex_iterator i = begin; i != end; ++i) {
            std::smatch match = *i;
            std::string key = match[1];
            std::string value = match[2];
            
            if (key == "host") host = value;
            else if (key == "user") user = value;
            else if (key == "password") password = value;
            else if (key == "database") database = value;
            else if (key == "port") port = std::stoi(value);
        }
        
        // 创建会话
        ngx_log_error(NGX_LOG_DEBUG, ngx_cycle->log, 0, 
                     "Creating new MySQL connection to %s:%d as %s", 
                     host.c_str(), port, user.c_str());
        
        // 创建会话设置
        mysqlx::SessionSettings settings(
            mysqlx::SessionOption::HOST, host,
            mysqlx::SessionOption::PORT, port,
            mysqlx::SessionOption::USER, user,
            mysqlx::SessionOption::PWD, password,
            mysqlx::SessionOption::DB, database
        );
        
        // 设置连接超时
        settings.set(mysqlx::SessionOption::CONNECT_TIMEOUT, m_connection_timeout);
        
        // 创建新会话
        auto session = std::make_shared<mysqlx::Session>(settings);
        
        // 测试连接是否成功
        auto result = session->sql("SELECT 1").execute();
        
        ngx_log_error(NGX_LOG_DEBUG, ngx_cycle->log, 0, 
                     "New MySQL connection created successfully");
        
        return session;
    } 
    catch (const mysqlx::Error& err) {
        ngx_log_error(NGX_LOG_ERR, ngx_cycle->log, 0, 
                     "MySQL connection error: %s", err.what());
        throw;
    }
    catch (const std::exception& e) {
        ngx_log_error(NGX_LOG_ERR, ngx_cycle->log, 0, 
                     "Connection error: %s", e.what());
        throw;
    }
}

// 维护连接池的线程函数
void ConnectionPool::maintainConnections() {
    ngx_log_error(NGX_LOG_INFO, ngx_cycle->log, 0, "Connection pool maintenance thread started");
    
    // 记录上次记录状态的时间
    auto last_status_log = std::chrono::steady_clock::now();
    
    while (m_running) {
        // 每15秒执行一次维护 (原来是30秒，降低了时间间隔以提高响应性)
        std::this_thread::sleep_for(std::chrono::seconds(15));
        
        // 如果连接池已经停止，退出循环
        if (!m_running) {
            break;
        }
        
        try {
            std::unique_lock<std::timed_mutex> lock(m_mutex, std::defer_lock);
            
            // 尝试获取锁，如果无法立即获取则跳过本次维护
            if (!lock.try_lock_for(std::chrono::milliseconds(1000))) {
                ngx_log_error(NGX_LOG_WARN, ngx_cycle->log, 0, 
                             "Pool maintenance skipped - could not acquire lock");
                continue;
            }
            
            // 定期记录连接池状态 (每10分钟)
            auto now = std::chrono::steady_clock::now();
            bool should_log_status = std::chrono::duration_cast<std::chrono::minutes>(
                now - last_status_log).count() >= 10;
            
            // 检查是否有长时间不用的空闲连接
            size_t idle_count_before = m_idle_connections.size();
            
            // 如果空闲连接过多，且远超过最小连接数要求，则移除一些连接
            if (m_idle_connections.size() > m_min_connections + 3 && m_waiting_threads == 0) {
                // 保留最小连接数+额外的2个备用连接
                size_t to_keep = m_min_connections + 2;
                while (m_idle_connections.size() > to_keep) {
                    m_idle_connections.pop_back();
                }
                
                if (should_log_status || idle_count_before - m_idle_connections.size() > 2) {
                    ngx_log_error(NGX_LOG_INFO, ngx_cycle->log, 0, 
                                 "Reduced excess idle connections from %d to %d", 
                                 idle_count_before, m_idle_connections.size());
                }
            }
            
            // 检查并保持最小连接数
            size_t connections_to_add = 0;
            if (m_idle_connections.size() < m_min_connections) {
                connections_to_add = m_min_connections - m_idle_connections.size();
            }
            
            // 释放锁后再创建连接，避免长时间持有锁
            lock.unlock();
            
            // 创建需要的连接
            std::vector<std::shared_ptr<mysqlx::Session>> new_connections;
            for (size_t i = 0; i < connections_to_add; i++) {
                try {
                    auto conn = createConnection();
                    if (conn) {
                        new_connections.push_back(conn);
                    }
                } catch (const std::exception& e) {
                    ngx_log_error(NGX_LOG_ERR, ngx_cycle->log, 0, 
                                 "Failed to create connection during maintenance: %s", e.what());
                    break;
                }
            }
            
            // 重新获取锁，添加新创建的连接到空闲池
            if (!new_connections.empty()) {
                std::lock_guard<std::timed_mutex> add_lock(m_mutex);
                for (auto& conn : new_connections) {
                    m_idle_connections.push_back(conn);
                }
                ngx_log_error(NGX_LOG_DEBUG, ngx_cycle->log, 0, 
                             "Added %d new connections to maintain minimum pool size", 
                             new_connections.size());
            }
            
            // 记录当前连接池状态
            if (should_log_status) {
                ngx_log_error(NGX_LOG_INFO, ngx_cycle->log, 0, 
                             "Connection pool status: active=%d, idle=%d, waiting=%d, min=%d, max=%d", 
                             m_active_connections.size(), m_idle_connections.size(), 
                             m_waiting_threads.load(), m_min_connections, m_max_connections);
                last_status_log = now;
            }
        } catch (const std::exception& e) {
            // 捕获维护过程中的异常，确保维护线程不会崩溃
            ngx_log_error(NGX_LOG_ERR, ngx_cycle->log, 0, 
                         "Error in pool maintenance thread: %s", e.what());
        }
    }
    
    ngx_log_error(NGX_LOG_INFO, ngx_cycle->log, 0, "Connection pool maintenance thread stopped");
} 