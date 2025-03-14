//
// Created by wuxianggujun on 2025/3/14.
//

#ifndef TINA_BLOG_BLOG_ROUTER_HPP
#define TINA_BLOG_BLOG_ROUTER_HPP

#include "Nginx.hpp"
#include "NgxString.hpp"
#include "NgxRequest.hpp"
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

            // 如果模式不包含参数，直接比较
            if (paramNames.empty())
            {
                return uriStr == pattern;
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
                        return false;
                    }
                }

                // 提取参数值
                std::string paramValue = uriStr.substr(uriPos, paramEndPos - uriPos);
                params[paramName] = paramValue;

                // 更新位置
                uriPos = paramEndPos;
            }

            // 检查路径末尾
            std::string endPart = pattern.substr(patternPos);
            return uriStr.substr(uriPos) == endPart;
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
