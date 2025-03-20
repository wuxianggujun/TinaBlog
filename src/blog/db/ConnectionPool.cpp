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
    std::lock_guard<std::mutex> lock(m_mutex);
    
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
    
    std::unique_lock<std::mutex> lock(m_mutex);
    auto start_time = std::chrono::steady_clock::now();
    
    // 等待可用连接或者可以创建新连接
    while(m_idle_connections.empty() && m_active_connections.size() >= m_max_connections) {
        auto wait_result = m_condition.wait_for(lock, std::chrono::milliseconds(timeout_ms));
        
        if (wait_result == std::cv_status::timeout) {
            // 减少等待线程计数
            m_waiting_threads--;
            
            ngx_log_error(NGX_LOG_ERR, ngx_cycle->log, 0, 
                         "Timeout waiting for database connection (pool status: active=%d, idle=%d, max=%d)", 
                         m_active_connections.size(), m_idle_connections.size(), m_max_connections);
            throw std::runtime_error("Timeout waiting for database connection");
        }
        
        // 检查是否超过总超时时间
        auto now = std::chrono::steady_clock::now();
        if (std::chrono::duration_cast<std::chrono::milliseconds>(now - start_time).count() > timeout_ms) {
            // 减少等待线程计数
            m_waiting_threads--;
            
            ngx_log_error(NGX_LOG_ERR, ngx_cycle->log, 0, 
                         "Timeout waiting for database connection (pool status: active=%d, idle=%d, max=%d)", 
                         m_active_connections.size(), m_idle_connections.size(), m_max_connections);
            throw std::runtime_error("Timeout waiting for database connection");
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
    
    std::lock_guard<std::mutex> lock(m_mutex);
    
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
        std::lock_guard<std::mutex> lock(m_mutex);
        m_running = false;
    }
    
    // 等待维护线程结束
    if (m_maintenance_thread.joinable()) {
        m_condition.notify_all();  // 唤醒维护线程
        m_maintenance_thread.join();
    }
    
    // 关闭并清理所有连接
    std::lock_guard<std::mutex> lock(m_mutex);
    
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
    std::lock_guard<std::mutex> lock(m_mutex);
    
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
    
    while (m_running) {
        // 每30秒执行一次维护
        std::this_thread::sleep_for(std::chrono::seconds(30));
        
        // 如果连接池已经停止，退出循环
        if (!m_running) {
            break;
        }
        
        std::lock_guard<std::mutex> lock(m_mutex);
        
        // 检查并保持最小连接数
        while (m_idle_connections.size() < m_min_connections) {
            try {
                auto conn = createConnection();
                if (conn) {
                    m_idle_connections.push_back(conn);
                    ngx_log_error(NGX_LOG_DEBUG, ngx_cycle->log, 0, 
                                 "Created new connection to maintain minimum pool size");
                }
            } catch (const std::exception& e) {
                ngx_log_error(NGX_LOG_ERR, ngx_cycle->log, 0, 
                             "Failed to create connection during maintenance: %s", e.what());
                break;
            }
        }
        
        // 检查空闲连接有效性
        auto it = m_idle_connections.begin();
        while (it != m_idle_connections.end()) {
            if (!validateConnection(*it)) {
                // 移除无效连接
                it = m_idle_connections.erase(it);
                ngx_log_error(NGX_LOG_WARN, ngx_cycle->log, 0, 
                             "Removed invalid connection from pool during maintenance");
            } else {
                ++it;
            }
        }
        
        // 记录当前连接池状态
        ngx_log_error(NGX_LOG_DEBUG, ngx_cycle->log, 0, 
                     "Connection pool status: active=%d, idle=%d, waiting=%d", 
                     m_active_connections.size(), m_idle_connections.size(), m_waiting_threads.load());
    }
    
    ngx_log_error(NGX_LOG_INFO, ngx_cycle->log, 0, "Connection pool maintenance thread stopped");
} 