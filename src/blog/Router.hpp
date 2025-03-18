//
// Created by wuxianggujun on 2025/3/18.
//

#ifndef TINA_BLOG_ROUTER_BASE_HPP
#define TINA_BLOG_ROUTER_BASE_HPP

#include "Nginx.hpp"
#include "Route.hpp"

// 路由器基类接口
class Router
{
public:
    virtual ~Router() = default;

    // 添加路由
    virtual void addRoute(const Route& route) = 0;

    virtual ngx_int_t route(ngx_http_request_t* request) = 0;

    virtual RouteHandler match(ngx_http_request_t* request,const std::string& uri,RouteParams& params) = 0;

    virtual size_t getRouteCount() const = 0;

    virtual void reset() = 0;

    virtual std::vector<std::string> dumpRoutes() const = 0;
    
};


#endif //TINA_BLOG_ROUTER_BASE_HPP
