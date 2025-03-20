#pragma once

#include "Nginx.hpp"
#include "NgxString.hpp"
#include "NgxRequest.hpp"
#include <string>
#include <functional>
#include <unordered_map>
#include <memory>
#include <mutex>

// 前向声明
class ApiEndpoint;

class BlogHandler {
public:
    // 初始化路由
    static void initRoutes();
    
    // HTTP请求处理函数
    static ngx_int_t handleRequest(ngx_http_request_t* r);

private:
    // 处理不同的HTTP方法
    static ngx_int_t handleGet(NgxRequest& req);
    static ngx_int_t handlePost(NgxRequest& req);
    static ngx_int_t handlePut(NgxRequest& req);
    static ngx_int_t handleDelete(NgxRequest& req);
    static ngx_int_t handleOptions(NgxRequest& req);

    // 处理具体的API端点
    static ngx_int_t handleLogin(NgxRequest& req);
    static ngx_int_t handleRegister(NgxRequest& req);
    static ngx_int_t handleCreatePost(NgxRequest& req);
    static ngx_int_t handleUpdatePost(NgxRequest& req);
    static ngx_int_t handleDeletePost(NgxRequest& req);
    static ngx_int_t handleHealthCheck(NgxRequest& req);

    // 辅助函数
    static std::string getRequestPath(const NgxRequest& req);
    
    // 路由映射
    typedef std::function<ngx_int_t(NgxRequest&)> RouteHandler;
    static std::unordered_map<std::string, RouteHandler> getRoutes;
    static std::unordered_map<std::string, RouteHandler> postRoutes;
    static std::unordered_map<std::string, RouteHandler> putRoutes;
    static std::unordered_map<std::string, RouteHandler> deleteRoutes;
    
    // 保护路由映射的互斥锁
    static std::mutex routesMutex;
}; 

// API终端点基类
class ApiEndpoint {
public:
    virtual ~ApiEndpoint() = default;
    virtual ngx_int_t handleGet(NgxRequest& req) { return NGX_HTTP_NOT_ALLOWED; }
    virtual ngx_int_t handlePost(NgxRequest& req) { return NGX_HTTP_NOT_ALLOWED; }
    virtual ngx_int_t handlePut(NgxRequest& req) { return NGX_HTTP_NOT_ALLOWED; }
    virtual ngx_int_t handleDelete(NgxRequest& req) { return NGX_HTTP_NOT_ALLOWED; }
    
protected:
    // 基类不再需要辅助方法，因为NgxRequest已经提供了所有需要的功能
}; 