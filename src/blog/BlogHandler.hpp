#pragma once

#include "Nginx.hpp"
#include "NgxString.hpp"
#include <string>
#include <functional>
#include <unordered_map>
#include <memory>

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
    static ngx_int_t handleGet(ngx_http_request_t* r);
    static ngx_int_t handlePost(ngx_http_request_t* r);
    static ngx_int_t handlePut(ngx_http_request_t* r);
    static ngx_int_t handleDelete(ngx_http_request_t* r);
    static ngx_int_t handleOptions(ngx_http_request_t* r);

    // 处理具体的API端点
    static ngx_int_t handleLogin(ngx_http_request_t* r);
    static ngx_int_t handleRegister(ngx_http_request_t* r);
    static ngx_int_t handleCreatePost(ngx_http_request_t* r);
    static ngx_int_t handleUpdatePost(ngx_http_request_t* r);
    static ngx_int_t handleDeletePost(ngx_http_request_t* r);

    // 辅助函数
    static ngx_int_t sendJsonResponse(ngx_http_request_t* r, const std::string& json);
    static ngx_int_t sendError(ngx_http_request_t* r, ngx_uint_t status, const std::string& message);
    static ngx_int_t parseRequestBody(ngx_http_request_t* r, NgxString& body);
    static NgxString getRequestPath(ngx_http_request_t* r);
    static NgxString getQueryParam(ngx_http_request_t* r, const NgxString& name);
    
    // 路由映射
    typedef std::function<ngx_int_t(ngx_http_request_t*)> RouteHandler;
    static std::unordered_map<std::string, RouteHandler> getRoutes;
    static std::unordered_map<std::string, RouteHandler> postRoutes;
    static std::unordered_map<std::string, RouteHandler> putRoutes;
    static std::unordered_map<std::string, RouteHandler> deleteRoutes;
}; 

// API终端点基类
class ApiEndpoint {
public:
    virtual ~ApiEndpoint() = default;
    virtual ngx_int_t handleGet(ngx_http_request_t* r) { return NGX_HTTP_NOT_ALLOWED; }
    virtual ngx_int_t handlePost(ngx_http_request_t* r) { return NGX_HTTP_NOT_ALLOWED; }
    virtual ngx_int_t handlePut(ngx_http_request_t* r) { return NGX_HTTP_NOT_ALLOWED; }
    virtual ngx_int_t handleDelete(ngx_http_request_t* r) { return NGX_HTTP_NOT_ALLOWED; }
    
protected:
    // 辅助方法
    ngx_int_t sendJsonResponse(ngx_http_request_t* r, const std::string& json);
    ngx_int_t sendError(ngx_http_request_t* r, ngx_uint_t status, const std::string& message);
    ngx_int_t parseRequestBody(ngx_http_request_t* r, NgxString& body);
}; 