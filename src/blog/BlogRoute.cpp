#include "BlogRoute.hpp"
#include "NgxPool.hpp"
#include "NgxString.hpp"
#include "NgxLog.hpp"
#include "service/JwtService.hpp"
#include <cstring>
#include <utility>

// NgxRouteParams 实现

NgxRouteParams::NgxRouteParams(ngx_pool_t* pool, ngx_uint_t capacity)
    : count(0), capacity_(capacity)
{
    keys = static_cast<ngx_str_t*>(ngx_pcalloc(pool, sizeof(ngx_str_t) * capacity));
    values = static_cast<ngx_str_t*>(ngx_pcalloc(pool, sizeof(ngx_str_t) * capacity));
}

bool NgxRouteParams::add(ngx_pool_t* pool, const ngx_str_t* key, const ngx_str_t* value)
{
    if (count >= capacity_) {
        return false;
    }
    
    // 复制参数名
    keys[count].len = key->len;
    keys[count].data = static_cast<u_char*>(ngx_pnalloc(pool, key->len));
    if (!keys[count].data) {
        return false;
    }
    ngx_memcpy(keys[count].data, key->data, key->len);
    
    // 复制参数值
    values[count].len = value->len;
    values[count].data = static_cast<u_char*>(ngx_pnalloc(pool, value->len));
    if (!values[count].data) {
        return false;
    }
    ngx_memcpy(values[count].data, value->data, value->len);
    
    count++;
    return true;
}

const ngx_str_t* NgxRouteParams::get(const ngx_str_t* key) const
{
    for (ngx_uint_t i = 0; i < count; i++)
    {
        if (key->len == keys[i].len &&
            ngx_strncmp(key->data, keys[i].data, key->len) == 0)
        {
            return &values[i];
        }
    }
    return nullptr;
}

const ngx_str_t* NgxRouteParams::get(const char* key) const
{
    ngx_str_t key_str;
    key_str.data = (u_char*)key;
    key_str.len = ngx_strlen(key);
    return get(&key_str);
}

// RouteEntry 实现

RouteEntry::RouteEntry(ngx_pool_t* pool,
                       const char* pattern_str,
                       HttpMethod method,
                       RouteHandler handler,
                       JwtVerifyLevel jwt_level)
    : method(method),
      handler(std::move(handler)),
      jwt_level(jwt_level),
      middlewares(nullptr),
      middleware_count(0)
{
    pattern.len = ngx_strlen(pattern_str);
    pattern.data = static_cast<u_char*>(ngx_pnalloc(pool, pattern.len));
    ngx_memcpy(pattern.data, pattern_str, pattern.len);
}

bool RouteEntry::add_middleware(ngx_pool_t* pool, MiddlewareHandler middleware)
{
    // 第一次添加中间件，创建数组
    if (!middlewares)
    {
        middlewares = static_cast<MiddlewareHandler*>(
            ngx_pcalloc(pool, sizeof(MiddlewareHandler) * 8)); // 每个路由最多8个中间件
        if (!middlewares) return false;
    }

    // 检查容量
    if (middleware_count >= 8) return false;

    // 添加中间件
    middlewares[middleware_count++] = middleware;
    return true;
}

// BlogRoute 实现

BlogRoute& BlogRoute::getInstance()
{
    static BlogRoute instance;
    return instance;
}

bool BlogRoute::init(ngx_pool_t* pool)
{
    // 存储内存池
    pool_ = pool;

    // 初始化路由表
    route_capacity_ = 32; // 默认支持32个路由
    routes_ = static_cast<RouteEntry**>(ngx_pcalloc(pool, sizeof(RouteEntry*) * route_capacity_));
    if (!routes_) return false;

    // 初始化全局中间件
    global_middlewares_ = static_cast<MiddlewareHandler*>(
        ngx_pcalloc(pool, sizeof(MiddlewareHandler) * 8)); // 最多8个全局中间件
    if (!global_middlewares_) return false;

    return true;
}

bool BlogRoute::register_route(const char* pattern,
                               HttpMethod method,
                               RouteHandler handler,
                               JwtVerifyLevel jwt_level)
{
    if (!pool_ || !routes_) return false;

    // 检查容量
    if (route_count_ >= route_capacity_)
    {
        // 扩容（在Nginx中应避免扩容，应提前设置足够容量）
        ngx_log_error(NGX_LOG_WARN, ngx_cycle->log, 0,
                      "BlogRoute: Route capacity reached, consider increasing initial capacity");
        return false;
    }

    // 创建新路由
    routes_[route_count_] = static_cast<RouteEntry*>(ngx_pcalloc(pool_, sizeof(RouteEntry)));
    if (!routes_[route_count_]) return false;

    // 初始化路由
    new(routes_[route_count_]) RouteEntry(pool_, pattern, method, handler, jwt_level);

    // 根据JWT级别添加相应的中间件
    if (jwt_level != JwtVerifyLevel::JWT_NONE)
    {
        routes_[route_count_]->add_middleware(pool_, create_jwt_middleware(jwt_level));
    }

    route_count_++;
    return true;
}

bool BlogRoute::add_middleware(const char* pattern,
                               HttpMethod method,
                               MiddlewareHandler middleware)
{
    if (!pool_ || !routes_) return false;

    // 查找匹配的路由
    for (ngx_uint_t i = 0; i < route_count_; i++)
    {
        if (routes_[i]->method == method &&
            ngx_strcmp(routes_[i]->pattern.data, (u_char*)pattern) == 0)
        {
            // 添加中间件
            return routes_[i]->add_middleware(pool_, middleware);
        }
    }

    return false; // 未找到匹配的路由
}

bool BlogRoute::add_global_middleware(MiddlewareHandler middleware)
{
    if (!pool_ || !global_middlewares_) return false;

    // 检查容量
    if (global_middleware_count_ >= 8) return false;

    // 添加全局中间件
    global_middlewares_[global_middleware_count_++] = middleware;
    return true;
}

HttpMethod BlogRoute::parse_http_method(ngx_http_request_t* r)
{
    if (!r) return HttpMethod::UNKNOWN_METHOD;
    return fromNginxMethod(r->method);
}

const char* BlogRoute::http_method_to_string(HttpMethod method)
{
  return methodToString(method);
}

ngx_int_t BlogRoute::handle_request(ngx_http_request_t* r)
{
    // 创建请求包装器
    NgxRequest req(r);

    // 解析HTTP方法
    HttpMethod method = parse_http_method(r);
    if (method == HttpMethod::UNKNOWN_METHOD)
    {
        return req.send_error(NGX_HTTP_NOT_ALLOWED, "Method not allowed");
    }

    // 处理OPTIONS请求（CORS预检）
    if (method == HttpMethod::OPTIONS_METHOD)
    {
        // 添加CORS头
        req.add_header("Access-Control-Allow-Origin", "*");
        req.add_header("Access-Control-Allow-Methods", "GET, POST, PUT, DELETE, OPTIONS");
        req.add_header("Access-Control-Allow-Headers", "Content-Type, Authorization");
        req.add_header("Access-Control-Max-Age", "86400");

        return req.send_status(NGX_HTTP_OK);
    }

    // 处理请求
    return handle_method(req, method);
}

ngx_int_t BlogRoute::handle_method(NgxRequest& req, HttpMethod method)
{
    // 如果是POST或PUT请求，需要先读取请求体
    if (method == HttpMethod::POST_METHOD || method == HttpMethod::PUT_METHOD)
    {
        if (req.get()->request_body == nullptr)
        {
            ngx_int_t rc = ngx_http_read_client_request_body(req.get(), [](ngx_http_request_t* r)
            {
                // 请求体读取完成后的回调
                BlogRoute::getInstance().handle_request(r);
            });

            if (rc >= NGX_HTTP_SPECIAL_RESPONSE)
            {
                return rc;
            }

            return NGX_DONE; // 告知Nginx我们将在请求体读取完成后再次处理
        }
    }

    // 匹配并处理路由
    return match_and_process_route(req, method);
}

ngx_int_t BlogRoute::match_and_process_route(NgxRequest& req, HttpMethod method)
{
    // 准备路由参数
    NgxRouteParams params(req.get()->pool, 10); // 最多10个参数

    // 匹配路由
    RouteEntry* route = match_route(&req.get()->uri, method, params);

    if (!route)
    {
        NgxLog log(req.get()->connection->log);
        log.debug("No route found for %s %.*s",
                  http_method_to_string(method),
                  req.get()->uri.len, req.get()->uri.data);
        return req.send_error(NGX_HTTP_NOT_FOUND, "API not found");
    }

    // 执行路由处理链
    return execute_route_chain(req, route, params);
}

RouteEntry* BlogRoute::match_route(ngx_str_t* uri, HttpMethod method, NgxRouteParams& params)
{
    if (!uri || !routes_) return nullptr;

    for (ngx_uint_t i = 0; i < route_count_; i++)
    {
        RouteEntry* route = routes_[i];

        // 方法必须匹配
        if (route->method != method) continue;

        // 检查是否是简单匹配（不包含参数标记 :）
        bool has_params = false;
        for (ngx_uint_t j = 0; j < route->pattern.len; j++)
        {
            if (route->pattern.data[j] == ':')
            {
                has_params = true;
                break;
            }
        }

        // 简单匹配
        if (!has_params)
        {
            if (uri->len == route->pattern.len &&
                ngx_strncmp(uri->data, route->pattern.data, uri->len) == 0)
            {
                return route;
            }
            continue;
        }

        // 参数匹配
        // 此处为简化实现，真实应用中应当构建正则表达式或使用更高效的匹配算法
        // 目前仅支持 /api/users/:id 这样的简单参数模式

        ngx_str_t pattern_parts[10]; // 路由模式的各部分
        ngx_str_t uri_parts[10]; // URI的各部分
        ngx_uint_t pattern_count = 0;
        ngx_uint_t uri_count = 0;

        // 分割模式
        ngx_str_t pattern = route->pattern;
        u_char* start = pattern.data;
        for (ngx_uint_t j = 0; j < pattern.len; j++)
        {
            if (pattern.data[j] == '/')
            {
                if (j > 0 && start < pattern.data + j)
                {
                    pattern_parts[pattern_count].data = start;
                    pattern_parts[pattern_count].len = pattern.data + j - start;
                    pattern_count++;
                }
                start = pattern.data + j + 1;
            }
        }

        // 处理最后一部分
        if (start < pattern.data + pattern.len)
        {
            pattern_parts[pattern_count].data = start;
            pattern_parts[pattern_count].len = pattern.data + pattern.len - start;
            pattern_count++;
        }

        // 分割URI
        start = uri->data;
        for (ngx_uint_t j = 0; j < uri->len; j++)
        {
            if (uri->data[j] == '/')
            {
                if (j > 0 && start < uri->data + j)
                {
                    uri_parts[uri_count].data = start;
                    uri_parts[uri_count].len = uri->data + j - start;
                    uri_count++;
                }
                start = uri->data + j + 1;
            }
        }

        // 处理最后一部分
        if (start < uri->data + uri->len)
        {
            uri_parts[uri_count].data = start;
            uri_parts[uri_count].len = uri->data + uri->len - start;
            uri_count++;
        }

        // 部分数量必须匹配
        if (pattern_count != uri_count) continue;

        // 逐部分匹配并提取参数
        bool match = true;
        for (ngx_uint_t j = 0; j < pattern_count; j++)
        {
            // 检查是否是参数部分
            if (pattern_parts[j].len > 0 && pattern_parts[j].data[0] == ':')
            {
                // 提取参数名
                ngx_str_t param_name = {
                    pattern_parts[j].len - 1,
                    pattern_parts[j].data + 1
                };

                // 添加参数
                if (!params.add(pool_, &param_name, &uri_parts[j])) {
                    NgxLog log(ngx_cycle->log);
                    log.error("Failed to add route parameter: %.*s", 
                              (int)param_name.len, param_name.data);
                    // 继续匹配下一个路由
                    match = false;
                    break;
                }
            }
            else
            {
                // 普通部分，必须精确匹配
                if (pattern_parts[j].len != uri_parts[j].len ||
                    ngx_strncmp(pattern_parts[j].data, uri_parts[j].data, pattern_parts[j].len) != 0)
                {
                    match = false;
                    break;
                }
            }
        }

        if (match)
        {
            return route;
        }
    }

    return nullptr;
}

ngx_int_t BlogRoute::execute_route_chain(NgxRequest& req,
                                         RouteEntry* route,
                                         const NgxRouteParams& params)
{
    // 执行全局中间件
    for (ngx_uint_t i = 0; i < global_middleware_count_; i++)
    {
        if (!global_middlewares_[i](req, params))
        {
            // 中间件终止了处理链
            return NGX_ERROR;
        }
    }

    // 执行路由特定的中间件
    if (route->middlewares)
    {
        for (ngx_uint_t i = 0; i < route->middleware_count; i++)
        {
            if (!route->middlewares[i](req, params))
            {
                // 中间件终止了处理链
                return NGX_ERROR;
            }
        }
    }

    // 执行路由处理器
    return route->handler(req, params);
}

MiddlewareHandler BlogRoute::create_jwt_middleware(JwtVerifyLevel level)
{
    return [level](NgxRequest& req, const NgxRouteParams& params) -> bool
    {
        // 从Authorization头获取JWT
        auto auth_header = req.get_header("Authorization");
        if (!auth_header)
        {
            // 如果JWT是必需的但未提供，则拒绝请求
            if (level == JwtVerifyLevel::JWT_REQUIRED)
            {
                req.send_error(NGX_HTTP_UNAUTHORIZED, "Missing authorization token");
                return false;
            }
            // 对于OPTIONAL级别，允许继续
            return true;
        }

        std::string token = *auth_header;
        // 移除Bearer前缀
        if (token.substr(0, 7) == "Bearer ")
        {
            token = token.substr(7);
        }

        // 验证JWT
        try
        {
            if (!JwtService::getInstance().verify(token))
            {
                req.send_error(NGX_HTTP_UNAUTHORIZED, "Invalid or expired token");
                return false;
            }

            // JWT验证成功，可以继续处理
            return true;
        }
        catch (const std::exception& e)
        {
            NgxLog log(req.get()->connection->log);
            log.error("JWT verification error: %s", e.what());

            req.send_error(NGX_HTTP_UNAUTHORIZED, "Token verification failed");
            return false;
        }
    };
}

bool BlogRoute::verify_jwt_token(const ngx_str_t* token)
{
    if (!token || token->len == 0) return false;

    std::string token_str(reinterpret_cast<char*>(token->data), token->len);
    return JwtService::getInstance().verify(token_str);
}
