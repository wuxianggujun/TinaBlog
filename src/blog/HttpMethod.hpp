//
// Created by wuxianggujun on 2025/3/20.
//

#pragma once

#include "Nginx.hpp"

enum class HttpMethod
{
    UNKNOWN_METHOD = 0,
    GET_METHOD = NGX_HTTP_GET,
    HEAD_METHOD = NGX_HTTP_HEAD,
    POST_METHOD = NGX_HTTP_POST,
    PUT_METHOD = NGX_HTTP_PUT,
    DELETE_METHOD = NGX_HTTP_DELETE,
    OPTIONS_METHOD = NGX_HTTP_OPTIONS,
    PATCH_METHOD = NGX_HTTP_PATCH,
    TRACE_METHOD = NGX_HTTP_TRACE,
    CONNECT_METHOD = NGX_HTTP_CONNECT
};

// 判断方法是否需要请求体
inline bool methodRequiresBody(HttpMethod method)
{
    return method == HttpMethod::POST_METHOD ||
        method == HttpMethod::PUT_METHOD ||
        method == HttpMethod::PATCH_METHOD ||
        method == HttpMethod::DELETE_METHOD;
}

// 从nginx方法转换为HttpMethod枚举类型
inline HttpMethod fromNginxMethod(ngx_uint_t method)
{
    switch (method)
    {
    case NGX_HTTP_GET:
        return HttpMethod::GET_METHOD;
    case NGX_HTTP_HEAD:
        return HttpMethod::HEAD_METHOD;
    case NGX_HTTP_POST:
        return HttpMethod::POST_METHOD;
    case NGX_HTTP_PUT:
        return HttpMethod::PUT_METHOD;
    case NGX_HTTP_DELETE:
        return HttpMethod::DELETE_METHOD;
    case NGX_HTTP_OPTIONS:
        return HttpMethod::OPTIONS_METHOD;
    case NGX_HTTP_PATCH:
        return HttpMethod::PATCH_METHOD;
    case NGX_HTTP_TRACE:
        return HttpMethod::TRACE_METHOD;
    case NGX_HTTP_CONNECT:
        return HttpMethod::CONNECT_METHOD;
    default:
        return HttpMethod::UNKNOWN_METHOD;
    }
}

inline const char* methodToString(HttpMethod method)
{
    switch (method)
    {
    case HttpMethod::GET_METHOD:
        return "GET";
    case HttpMethod::HEAD_METHOD:
        return "HEAD";
    case HttpMethod::POST_METHOD:
        return "POST";
    case HttpMethod::PUT_METHOD:
        return "PUT";
    case HttpMethod::DELETE_METHOD:
        return "DELETE";
    case HttpMethod::OPTIONS_METHOD:
        return "OPTIONS";
    case HttpMethod::PATCH_METHOD:
        return "PATCH";
    case HttpMethod::TRACE_METHOD:
        return "TRACE";
    case HttpMethod::CONNECT_METHOD:
        return "CONNECT";
    default:
        return "UNKNOWN";
    }
}
