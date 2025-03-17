//
// Created by wuxianggujun on 2025/3/14.
//

#ifndef TINA_BLOG_BLOG_ROUTER_HPP
#define TINA_BLOG_BLOG_ROUTER_HPP

#include "Nginx.hpp"
#include "NgxString.hpp"
#include "NgxRequest.hpp"
#include "NgxLog.hpp"
#include <utility>
#include <vector>
#include <string>
#include <functional>
#include <unordered_map>
#include <nlohmann/json.hpp>

// 路由参数类型 (如 /blog/post/:id 中的 id => value)
using RouteParams = std::unordered_map<std::string, std::string>;

// 路由处理器类型
using RouteHandler = std::function<ngx_int_t(ngx_http_request_t*, const RouteParams&)>;

// 使用nlohmann json
using json = nlohmann::json;

// 发送JSON响应函数的声明 (向后兼容层，实际调用JsonResponse类)
ngx_int_t sendJsonResponse(ngx_http_request_t* r, const std::string& jsonContent, ngx_uint_t status = NGX_HTTP_OK);
ngx_int_t sendJsonResponse(ngx_http_request_t* r, const json& jsonObj, ngx_uint_t status = NGX_HTTP_OK);

// 定义HTTP方法枚举
enum HttpMethod
{
    GET_METHOD = NGX_HTTP_GET,
    HEAD_METHOD = NGX_HTTP_HEAD,
    POST_METHOD = NGX_HTTP_POST,
    PUT_METHOD = NGX_HTTP_PUT,
    DELETE_METHOD = NGX_HTTP_DELETE,
    OPTIONS_METHOD = NGX_HTTP_OPTIONS,
    ANY_METHOD = 0xFFFF
};

// 路由定义结构
struct Route
{
    HttpMethod method;
    std::string pattern;
    std::vector<std::string> paramNames;
    RouteHandler handler;

    Route(HttpMethod method, std::string pattern, RouteHandler handler): method(method),
        pattern(std::move(pattern)), handler(std::move(handler))
    {
        parsePattern();
    }
    
    // 新增：支持不指定HTTP方法的构造函数，默认为ANY_METHOD
    Route(std::string pattern, RouteHandler handler): method(ANY_METHOD),
        pattern(std::move(pattern)), handler(std::move(handler))
    {
        parsePattern();
    }

    void parsePattern();
    
    // 检查URI是否匹配该路由，并提取参数
    bool match(const ngx_str_t& uri, RouteParams& params) const;
};

// 路由器类
class Router
{
public:
    // 虚析构函数，使得Router成为多态类
    virtual ~Router() = default;
    
    // 添加路由
    virtual void addRoute(const Route& route);
    
    // 重置所有路由
    virtual void reset();
    
    // 路由处理
    virtual ngx_int_t route(ngx_http_request_t* r);
    
    // 获取路由数量
    virtual size_t getRouteCount() const;
    
    // 转储路由列表（用于调试）
    virtual std::vector<std::string> dumpRoutes() const;
    
protected:
    std::vector<Route> routes;
};

// 让BlogRouter可以转换为Router的兼容实现
class BlogRouter : public Router
{
public:
    // 注册GET路由
    void get(const std::string& pattern, const RouteHandler& handler)
    {
        addRoute(Route(GET_METHOD, pattern, handler));
    }

    // 注册POST路由
    void post(const std::string& pattern, const RouteHandler& handler)
    {
        addRoute(Route(POST_METHOD, pattern, handler));
    }

    // 注册支持任何HTTP方法的路由
    void any(const std::string& pattern, const RouteHandler& handler)
    {
        addRoute(Route(ANY_METHOD, pattern, handler));
    }

    /**
     * @brief 清除所有路由
     */
    void reset() 
    {
        Router::reset(); // 调用基类的reset方法
    }

    // 查找匹配的路由处理器
    RouteHandler match(ngx_http_request_t* r, const std::string& uri, RouteParams& params)
    {
        NgxLog logger(r);
        
        // 临时创建ngx_str_t
        ngx_str_t ngx_uri;
        ngx_uri.data = (u_char*)uri.c_str();
        ngx_uri.len = uri.length();
        
        logger.debug("Trying to match route: %s", uri.c_str());
        
        // 遍历所有路由并检查匹配（注：使用dumpRoutes只是为了日志）
        std::vector<std::string> routesList = Router::dumpRoutes();
        for (const auto& routeStr : routesList) {
            logger.debug("Checking route: %s", routeStr.c_str());
        }
        
        // 遍历所有路由并检查匹配
        // 使用基类的routes成员变量
        for (const auto& route : routes) {
            HttpMethod method = ANY_METHOD;
            if (r != nullptr) {
                method = static_cast<HttpMethod>(r->method);
            }
            
            // 检查HTTP方法匹配 (ANY_METHOD匹配任何方法)
            if (route.method != ANY_METHOD && route.method != method) {
                logger.debug("HTTP method mismatch: %d != %d", route.method, method);
                continue;
            }
            
            // 清空前一个路由可能设置的参数
            params.clear();
            
            // 尝试匹配路由模式
            if (route.match(ngx_uri, params)) {
                logger.debug("Found matching route: %s", route.pattern.c_str());
                return route.handler;
            }
        }
        
        logger.debug("No matching route found");
        return nullptr;  // 没有匹配的路由
    }

    /**
     * @brief 获取已注册的路由数量
     * @return 路由数量
     */
    size_t getRouteCount() const 
    {
        return Router::getRouteCount();
    }
    
    /**
     * @brief 打印所有已注册的路由，方便调试
     * @return 路由字符串列表
     */
    std::vector<std::string> dumpRoutes() const 
    {
        // 先调用基类的方法获取路由字符串列表
        std::vector<std::string> routeStrings = Router::dumpRoutes();
        
        // 打印日志
        for (const auto& route : routes) {
            // 将HTTP方法转换为可读字符串
            std::string methodStr;
            if (route.method == GET_METHOD) methodStr = "GET";
            else if (route.method == POST_METHOD) methodStr = "POST";
            else if (route.method == HEAD_METHOD) methodStr = "HEAD";
            else if (route.method == PUT_METHOD) methodStr = "PUT";
            else if (route.method == DELETE_METHOD) methodStr = "DELETE";
            else methodStr = "ANY";
            
            // 打印路由信息
            ngx_log_error(NGX_LOG_INFO, ngx_cycle->log, 0, 
                "Registered route: [%s] %s", 
                methodStr.c_str(), 
                route.pattern.c_str());
        }
        
        return routeStrings;
    }

private:
    // 删除重复的成员变量，使用从Router继承的routes
    // std::vector<Route> routes;
};

// 全局单例路由器
inline BlogRouter& getBlogRouter()
{
    static BlogRouter router;
    return router;
}

#endif //TINA_BLOG_BLOG_ROUTER_HPP
