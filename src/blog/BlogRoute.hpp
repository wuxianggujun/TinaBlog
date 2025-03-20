#pragma once

#include "Nginx.hpp"
#include "NgxPtr.hpp"
#include "NgxRequest.hpp"
#include "HttpMethod.hpp"
#include <functional>

// 路由参数结构体，使用Nginx内存池来存储数据，避免使用STL
struct NgxRouteParams {
    ngx_str_t* keys;            // 参数名数组
    ngx_str_t* values;          // 参数值数组
    ngx_uint_t count;           // 参数数量
    
    // 构造函数
    NgxRouteParams(ngx_pool_t* pool, ngx_uint_t capacity);
    
    // 添加参数
    bool add(ngx_pool_t* pool, const ngx_str_t* key, const ngx_str_t* value);
    
    // 根据名称获取参数值
    const ngx_str_t* get(const ngx_str_t* key) const;
    const ngx_str_t* get(const char* key) const;
};

// JWT验证级别
enum class JwtVerifyLevel {
    JWT_NONE,       // 不需要验证
    JWT_OPTIONAL,   // 可选验证（有token则验证，无token则跳过）
    JWT_REQUIRED    // 必须验证
};

// 路由处理器函数类型
using RouteHandler = std::function<ngx_int_t(NgxRequest&, const NgxRouteParams&)>;

// 中间件函数类型，返回true表示继续处理，返回false表示终止处理链
using MiddlewareHandler = std::function<bool(NgxRequest&, const NgxRouteParams&)>;

// 路由项结构，描述一个API路由
struct RouteEntry {
    ngx_str_t pattern;                   // 路由模式（支持参数）
    HttpMethod method;                   // HTTP方法
    RouteHandler handler;                // 处理函数
    JwtVerifyLevel jwt_level;            // JWT验证级别
    MiddlewareHandler* middlewares;      // 中间件数组
    ngx_uint_t middleware_count;         // 中间件数量
    
    // 构造函数
    RouteEntry(ngx_pool_t* pool,
               const char* pattern_str,
               HttpMethod method,
               RouteHandler handler,
               JwtVerifyLevel jwt_level = JwtVerifyLevel::JWT_NONE);
    
    // 添加中间件
    bool add_middleware(ngx_pool_t* pool, MiddlewareHandler middleware);
};

/**
 * @brief 博客路由管理类
 * 
 * 特点：
 * 1. 高性能：使用Nginx内存池，避免动态内存分配
 * 2. 兼容Nginx异步架构
 * 3. 支持路由参数匹配
 * 4. 集成JWT验证
 * 5. 支持中间件机制
 */
class BlogRoute {
public:
    // 单例模式获取实例
    static BlogRoute& getInstance();
    
    // 初始化路由表
    bool init(ngx_pool_t* pool);
    
    // 注册路由（返回是否成功）
    bool register_route(const char* pattern, 
                       HttpMethod method, 
                       RouteHandler handler,
                       JwtVerifyLevel jwt_level = JwtVerifyLevel::JWT_NONE);
    
    // 添加中间件到路由
    bool add_middleware(const char* pattern,
                       HttpMethod method,
                       MiddlewareHandler middleware);
    
    // 全局中间件（适用于所有路由）
    bool add_global_middleware(MiddlewareHandler middleware);
    
    // 处理HTTP请求
    ngx_int_t handle_request(ngx_http_request_t* r);
    
    // 工具方法：解析HTTP方法
    static HttpMethod parse_http_method(ngx_http_request_t* r);
    
    // 工具方法：将HttpMethod转为字符串
    static const char* http_method_to_string(HttpMethod method);
    
    // JWT验证中间件工厂方法
    static MiddlewareHandler create_jwt_middleware(JwtVerifyLevel level);
    
private:
    // 单例模式
    BlogRoute() = default;
    ~BlogRoute() = default;
    BlogRoute(const BlogRoute&) = delete;
    BlogRoute& operator=(const BlogRoute&) = delete;
    
    // 处理不同的HTTP方法
    ngx_int_t handle_method(NgxRequest& req, HttpMethod method);
    
    // 匹配并处理路由
    ngx_int_t match_and_process_route(NgxRequest& req, HttpMethod method);
    
    // 路由匹配
    RouteEntry* match_route(ngx_str_t* uri, HttpMethod method, NgxRouteParams& params);
    
    // 执行路由处理链（中间件 + 处理器）
    ngx_int_t execute_route_chain(NgxRequest& req, 
                                 RouteEntry* route,
                                 const NgxRouteParams& params);
    
    // 生成JWT中间件
    MiddlewareHandler build_jwt_middleware(JwtVerifyLevel level);
    
    // 验证JWT token
    bool verify_jwt_token(const ngx_str_t* token);
    
    // 成员变量
    ngx_pool_t* pool_ = nullptr;                // 内存池
    RouteEntry** routes_ = nullptr;             // 路由表
    ngx_uint_t route_count_ = 0;                // 路由数量
    ngx_uint_t route_capacity_ = 0;             // 路由容量
    MiddlewareHandler* global_middlewares_ = nullptr;  // 全局中间件
    ngx_uint_t global_middleware_count_ = 0;           // 全局中间件数量
}; 