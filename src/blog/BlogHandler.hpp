#pragma once

#include "Nginx.hpp"
#include <string>

class BlogHandler {
public:
    // HTTP请求处理函数
    static ngx_int_t handleRequest(ngx_http_request_t* r);

private:
    // 处理不同的HTTP方法
    static ngx_int_t handleGet(ngx_http_request_t* r);
    static ngx_int_t handlePost(ngx_http_request_t* r);
    static ngx_int_t handlePut(ngx_http_request_t* r);
    static ngx_int_t handleDelete(ngx_http_request_t* r);

    // 辅助函数
    static ngx_int_t sendJsonResponse(ngx_http_request_t* r, const std::string& json);
    static ngx_int_t sendError(ngx_http_request_t* r, ngx_uint_t status, const std::string& message);
    static ngx_int_t parseRequestBody(ngx_http_request_t* r, std::string& body);
    static std::string getRequestPath(ngx_http_request_t* r);
    static std::string getQueryParam(ngx_http_request_t* r, const std::string& name);
}; 