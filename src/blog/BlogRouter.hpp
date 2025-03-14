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

// 路由参数类型 (如 /blog/post/:id 中的 id => value)
using RouteParams = std::unordered_map<std::string, std::string>;

class BlogRouter
{
public:
    using RouteHandler = std::function<ngx_int_t(ngx_http_request_t*, const RouteParams&)>;

    enum HttpMethod
    {
        GET_METHOD = NGX_HTTP_GET,
        HEAD_METHOD = NGX_HTTP_HEAD,
        POST_METHOD = NGX_HTTP_POST,
        PUT_METHOD = NGX_HTTP_PUT,
        DELETE_METHOD = NGX_HTTP_DELETE,
        ANY_METHOD = 0xFFFF
    };

    struct Route
    {
        HttpMethod method;
        std::string pattern;
        std::vector<std::string> paramNames;
        RouteHandler handler;

        Route(const HttpMethod method, std::string pattern, RouteHandler handler): method(method),
            pattern(std::move(pattern)), handler(std::move(handler))
        {
            parsePattern();
        }

        void parsePattern()
        {
            std::string current;
            bool inParam = false;

            for (char c : pattern)
            {
                if (c == ':' && !inParam)
                {
                    inParam = true;
                    current.clear();
                }
                else if (inParam && (c == '/' || c == '.'))
                {
                    if (!current.empty())
                    {
                        paramNames.push_back(std::move(current));
                    }
                    inParam = false;
                }
                else if (inParam)
                {
                    current += c;
                }
            }

            if (inParam && !current.empty())
            {
                paramNames.push_back(std::move(current));
            }
        }

        // 检查URI是否匹配该路由，并提取参数
        bool match(const ngx_str_t& uri, RouteParams& params) const
        {
            std::string uriStr(reinterpret_cast<const char*>(uri.data), uri.len);
            
            // 打印调试信息
            ngx_log_error(NGX_LOG_DEBUG, ngx_cycle->log, 0, 
                "尝试匹配: URI=%s 与模式=%s", 
                uriStr.c_str(), pattern.c_str());

            // 如果模式不包含参数，直接比较
            if (paramNames.empty())
            {
                // 完全相同的情况
                if (uriStr == pattern) {
                    ngx_log_error(NGX_LOG_DEBUG, ngx_cycle->log, 0, 
                        "完全匹配成功: URI=%s 与模式=%s", 
                        uriStr.c_str(), pattern.c_str());
                    return true;
                }
                
                // 尾部斜杠的特殊处理
                if (pattern.back() == '/' && uriStr == pattern.substr(0, pattern.length()-1)) {
                    ngx_log_error(NGX_LOG_DEBUG, ngx_cycle->log, 0, 
                        "尾部斜杠匹配成功: URI=%s 与模式=%s", 
                        uriStr.c_str(), pattern.c_str());
                    return true;
                }
                
                if (uriStr.back() == '/' && pattern == uriStr.substr(0, uriStr.length()-1)) {
                    ngx_log_error(NGX_LOG_DEBUG, ngx_cycle->log, 0, 
                        "尾部斜杠匹配成功: URI=%s 与模式=%s", 
                        uriStr.c_str(), pattern.c_str());
                    return true;
                }
                
                ngx_log_error(NGX_LOG_DEBUG, ngx_cycle->log, 0, 
                    "匹配失败: URI=%s 与模式=%s", 
                    uriStr.c_str(), pattern.c_str());
                return false;
            }

            // 将模式转换为正则表达式进行匹配
            // 简化实现：仅支持基本的 /:param/ 格式
            size_t patternPos = 0;
            size_t uriPos = 0;

            for (const auto& paramName : paramNames)
            {
                // 找到参数前的静态部分
                size_t paramPos = pattern.find(":" + paramName, patternPos);
                if (paramPos == std::string::npos) continue;

                // 检查静态部分是否匹配
                std::string staticPart = pattern.substr(patternPos, paramPos - patternPos);
                if (uriStr.substr(uriPos, staticPart.length()) != staticPart)
                {
                    ngx_log_error(NGX_LOG_DEBUG, ngx_cycle->log, 0, 
                        "静态部分不匹配: URI=%s 部分=%s", 
                        uriStr.substr(uriPos, staticPart.length()).c_str(), staticPart.c_str());
                    return false;
                }

                // 移动位置
                uriPos += staticPart.length();
                patternPos = paramPos + paramName.length() + 1; // +1 for ':'

                // 寻找参数结束位置
                size_t paramEndPos;
                if (patternPos >= pattern.length())
                {
                    // 参数在结尾
                    paramEndPos = uriStr.length();
                }
                else
                {
                    // 参数后面有内容，查找下一个分隔符
                    char nextChar = pattern[patternPos];
                    paramEndPos = uriStr.find(nextChar, uriPos);
                    if (paramEndPos == std::string::npos)
                    {
                        ngx_log_error(NGX_LOG_DEBUG, ngx_cycle->log, 0, 
                            "找不到参数结束位置: 字符=%c URI=%s", 
                            nextChar, uriStr.c_str());
                        return false;
                    }
                }

                // 提取参数值
                std::string paramValue = uriStr.substr(uriPos, paramEndPos - uriPos);
                params[paramName] = paramValue;
                
                ngx_log_error(NGX_LOG_DEBUG, ngx_cycle->log, 0, 
                    "提取参数: %s=%s", 
                    paramName.c_str(), paramValue.c_str());

                // 更新位置
                uriPos = paramEndPos;
            }

            // 检查路径末尾
            std::string endPart = pattern.substr(patternPos);
            bool result = uriStr.substr(uriPos) == endPart;
            
            if (result) {
                ngx_log_error(NGX_LOG_DEBUG, ngx_cycle->log, 0, 
                    "完整匹配成功: 剩余URI=%s 剩余模式=%s", 
                    uriStr.substr(uriPos).c_str(), endPart.c_str());
            } else {
                ngx_log_error(NGX_LOG_DEBUG, ngx_cycle->log, 0, 
                    "末尾不匹配: 剩余URI=%s 剩余模式=%s", 
                    uriStr.substr(uriPos).c_str(), endPart.c_str());
            }
            
            return result;
        }
    };


    // 注册GET路由
    void get(const std::string& pattern, const RouteHandler& handler)
    {
        routes.emplace_back(GET_METHOD, pattern, handler);
    }

    // 注册POST路由
    void post(const std::string& pattern, const RouteHandler& handler)
    {
        routes.emplace_back(POST_METHOD, pattern, handler);
    }

    // 注册支持任何HTTP方法的路由
    void any(const std::string& pattern, const RouteHandler& handler)
    {
        routes.emplace_back(ANY_METHOD, pattern, handler);
    }

    /**
     * @brief 清除所有路由
     */
    void reset() 
    {
        routes.clear();
    }

    // 分发请求到匹配的路由
    ngx_int_t dispatch(ngx_http_request_t* r)
    {
        ngx_uint_t method = r->method;
        ngx_str_t uri = r->uri;

        // 遍历所有路由
        for (const auto& route : routes)
        {
            // 检查HTTP方法是否匹配
            if (route.method != ANY_METHOD && !(route.method & method))
            {
                continue;
            }

            // 尝试匹配路由并提取参数
            RouteParams params;
            if (route.match(uri, params))
            {
                // 调用处理函数
                return route.handler(r, params);
            }
        }

        // 没有匹配的路由
        return NGX_HTTP_NOT_FOUND;
    }

    // 查找匹配的路由处理器
    RouteHandler match(ngx_http_request_t* r, const std::string& uri, RouteParams& params)
    {
        NgxLog logger(r);
        ngx_uint_t method = r->method;
        
        // 临时创建ngx_str_t
        ngx_str_t ngx_uri;
        ngx_uri.data = (u_char*)uri.c_str();
        ngx_uri.len = uri.length();
        
        logger.debug("尝试匹配路由: %s", uri.c_str());
        
        // 遍历所有路由
        for (const auto& route : routes)
        {
            // 检查HTTP方法是否匹配
            if (route.method != ANY_METHOD && !(route.method & method))
            {
                logger.debug("路由方法不匹配: %d vs %d", route.method, method);
                continue;
            }
            
            logger.debug("检查路由模式: %s", route.pattern.c_str());
            
            // 尝试匹配路由并提取参数
            params.clear(); // 清除之前的参数
            if (route.match(ngx_uri, params))
            {
                logger.info("路由匹配成功: %s", route.pattern.c_str());
                
                // 记录提取到的参数
                for (const auto& param : params) {
                    logger.debug("参数: %s = %s", param.first.c_str(), param.second.c_str());
                }
                
                return route.handler;
            }
        }
        
        logger.warn("没有找到匹配的路由: %s", uri.c_str());
        return nullptr;  // 没有匹配的路由
    }

    /**
     * @brief 获取已注册的路由数量
     * @return 路由数量
     */
    size_t getRouteCount() const 
    {
        return routes.size();
    }
    
    /**
     * @brief 打印所有已注册的路由，方便调试
     */
    void dumpRoutes() const 
    {
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
                "已注册路由: [%s] %s", 
                methodStr.c_str(), 
                route.pattern.c_str());
        }
    }

private:
    std::vector<Route> routes;
};

// 全局单例路由器
inline BlogRouter& getBlogRouter()
{
    static BlogRouter router;
    return router;
}

#endif //TINA_BLOG_BLOG_ROUTER_HPP
