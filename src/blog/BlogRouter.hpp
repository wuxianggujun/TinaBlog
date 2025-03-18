//
// Created by wuxianggujun on 2025/3/14.
//

#ifndef TINA_BLOG_BLOG_ROUTER_HPP
#define TINA_BLOG_BLOG_ROUTER_HPP

#include "Nginx.hpp"
#include "Router.hpp"
#include "NgxRequest.hpp"
#include <unordered_map>
#include <nlohmann/json_fwd.hpp>

extern "C" {
// 前向声明外部模块
extern ngx_module_t ngx_http_blog_module;
}

using json = nlohmann::json;

// 发送JSON响应函数的声明 (向后兼容层，实际调用JsonResponse类)
ngx_int_t sendJsonResponse(ngx_http_request_t* r, const std::string& jsonContent, ngx_uint_t status = NGX_HTTP_OK);
ngx_int_t sendJsonResponse(ngx_http_request_t* r, const json& jsonObj, ngx_uint_t status = NGX_HTTP_OK);


// 博客路由器，继承自Router基类
class BlogRouter : public Router
{


public:
    static BlogRouter& getInstance()
    {
        static BlogRouter instance;
        return instance;
    }

    void addRoute(const Route& route) override
    {
        if (methodRequiresBody(route.method()))
        {
            routesWithBody_.push_back(route);
        }
        else
        {
            routes_.push_back(route);
        }
    }

    // 添加新的方法，用于注册需要请求体的路由
    void addRouteWithBody(const Route& route)
    {
        routesWithBody_.push_back(route);
    }

    void reset() override
    {
        routes_.clear();
        routesWithBody_.clear();
    }

    [[nodiscard]] size_t getRouteCount() const override
    {
        return routes_.size() + routesWithBody_.size();
    }

    ngx_int_t route(ngx_http_request_t* r) override
    {
        try
        {
            const NgxLog logger(r->connection->log);

            // 获取URI
            const std::string uri(reinterpret_cast<char*>(r->uri.data), r->uri.len);

            // 匹配路由
            RouteParams params;

            if (const RouteHandler handler = match(r, uri, params))
            {
                logger.debug("*%ui 找到匹配的路由: %s", r->connection->number, uri.c_str());
                return handler(r, params);
            }

            // 未找到路由
            logger.warn("*%ui 未找到匹配的路由: %s", r->connection->number, uri.c_str());
            return NGX_DECLINED;
        }
        catch (const std::exception& e)
        {
            NgxLog logger(r->connection->log);
            logger.error("*%ui 路由处理异常: %s", r->connection->number, e.what());
            return NGX_HTTP_INTERNAL_SERVER_ERROR;
        }
    }


    RouteHandler match(ngx_http_request_t* r, const std::string& uri, RouteParams& params) override
    {
        const NgxLog logger(r);

        // 首先匹配带请求体的路由
        for (const auto& route : routesWithBody_)
        {
            if (route.match(r, uri, params))
            {
                // 如果需要读取请求体且请求体未读取
                if (r->request_body == NULL)
                {
                    logger.debug("*%ui 找到需要请求体的路由: %s", r->connection->number, route.toString().c_str());

                    // 保存路由和参数上下文
                    auto* ctx = new RouteContext{route, params};

                    // 创建清理回调，确保内存被释放
                    ngx_http_cleanup_t* cln = ngx_http_cleanup_add(r, 0);
                    if (cln)
                    {
                        cln->handler = [](void* data)
                        {
                            auto* ctx = static_cast<RouteContext*>(data);
                            delete ctx;
                        };
                        cln->data = ctx;
                    }

                    // 保存上下文指针到请求中
                    ngx_http_set_ctx(r, ctx, ngx_http_blog_module);

                    // 设置请求体读取选项
                    r->request_body_in_single_buf = 1;
                    r->request_body_in_file_only = 0;

                    // 读取请求体
                    logger.debug("*%ui 开始读取请求体", r->connection->number);
                    ngx_int_t rc = ngx_http_read_client_request_body(r, handleRequestBodyRead);

                    if (rc >= NGX_HTTP_SPECIAL_RESPONSE)
                    {
                        // 返回空处理器，由Nginx处理特殊响应
                        return nullptr;
                    }

                    // 请求体读取延迟处理，返回一个表示延迟状态的处理器
                    return [](ngx_http_request_t*, const RouteParams&) -> ngx_int_t
                    {
                        return NGX_DONE;
                    };
                }

                // 请求体已读取，返回实际处理函数
                logger.debug("*%ui 找到已有请求体的路由: %s", r->connection->number, route.toString().c_str());
                return route.handler();
            }
        }

        // 然后匹配普通路由
        for (const auto& route : routes_)
        {
            if (route.match(r, uri, params))
            {
                logger.debug("*%ui 找到普通路由: %s", r->connection->number, route.toString().c_str());
                return route.handler();
            }
        }

        // 未找到路由
        return nullptr;
    }

    // 获取所有路由信息，用于调试（实现基类方法）
    [[nodiscard]] std::vector<std::string> dumpRoutes() const override
    {
        std::vector<std::string> result;

        for (const auto& route : routes_)
        {
            result.push_back(route.toString());
        }

        for (const auto& route : routesWithBody_)
        {
            result.push_back(route.toString() + " (需要请求体)");
        }

        return result;
    }

private:
    BlogRouter() = default;

    // 禁止拷贝和移动
    BlogRouter(const BlogRouter& other) = delete;
    BlogRouter& operator=(const BlogRouter& other) = delete;
    BlogRouter(BlogRouter&& other) = delete;
    BlogRouter& operator=(BlogRouter&& other) = delete;

    struct RouteContext
    {
        Route route;
        RouteParams params;
    };
    
    static void handleRequestBodyRead(ngx_http_request_t* r)
    {
        NgxLog logger(r);

        logger.debug("*%ui 请求体读取完成", r->connection->number);

        // 获取路由上下文
        auto* ctx = static_cast<RouteContext*>(ngx_http_get_module_ctx(r, ngx_http_blog_module));

        if (!ctx)
        {
            logger.error("*%ui 未找到路由上下文", r->connection->number);
            ngx_http_finalize_request(r, NGX_HTTP_INTERNAL_SERVER_ERROR);
            return;
        }

        // 执行路由处理函数
        ngx_int_t rc;

        try
        {
            rc = ctx->route.handler()(r, ctx->params);
        }
        catch (const std::exception& e)
        {
            logger.error("*%ui 路由处理器执行异常: %s", r->connection->number, e.what());
            rc = NGX_HTTP_INTERNAL_SERVER_ERROR;
        }
        // 完成请求处理
        ngx_http_finalize_request(r, rc);
    }

    std::vector<Route> routes_;
    std::vector<Route> routesWithBody_;
};

// 获取全局路由器实例
inline BlogRouter& getBlogRouter() {
    return BlogRouter::getInstance();
}

#endif //TINA_BLOG_BLOG_ROUTER_HPP
