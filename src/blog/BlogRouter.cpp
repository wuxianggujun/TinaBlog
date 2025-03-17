//
// Created by wuxianggujun on 2025/3/14.
//

#include "BlogRouter.hpp"
#include <BlogConfig.hpp>
#include "BlogModule.hpp"
#include "BlogPostManager.hpp"
#include <fstream>
#include <sstream>
#include "db/BlogPostDao.hpp"
#include <algorithm>
#include <cstring>
#include <regex>

// Route::parsePattern 实现
void Route::parsePattern() {
    std::string current;
    bool inParam = false;

    for (char c : pattern) {
        if (c == ':' && !inParam) {
            inParam = true;
            current.clear();
        }
        else if (inParam && (c == '/' || c == '.')) {
            if (!current.empty()) {
                paramNames.push_back(std::move(current));
            }
            inParam = false;
        }
        else if (inParam) {
            current += c;
        }
    }

    if (inParam && !current.empty()) {
        paramNames.push_back(std::move(current));
    }
}

// Route::match 实现
bool Route::match(const ngx_str_t& uri, RouteParams& params) const {
    std::string uriStr(reinterpret_cast<const char*>(uri.data), uri.len);
    
    // 打印调试信息
    ngx_log_error(NGX_LOG_DEBUG, ngx_cycle->log, 0, 
        "尝试匹配: URI=%s 与模式=%s", 
        uriStr.c_str(), pattern.c_str());
    
    // 特殊处理根路径和/blog路径
    if (pattern == "/" && (uriStr == "/" || uriStr.empty())) {
        return true;
    }
    
    if (pattern == "/blog" && (uriStr == "/blog" || uriStr == "/blog/")) {
        return true;
    }

    // 如果模式不包含参数，直接比较
    if (paramNames.empty()) {
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

    for (const auto& paramName : paramNames) {
        // 找到参数前的静态部分
        size_t paramPos = pattern.find(":" + paramName, patternPos);
        if (paramPos == std::string::npos) continue;

        // 检查静态部分是否匹配
        std::string staticPart = pattern.substr(patternPos, paramPos - patternPos);
        if (uriStr.substr(uriPos, staticPart.length()) != staticPart) {
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
        if (patternPos >= pattern.length()) {
            // 参数在结尾
            paramEndPos = uriStr.length();
        }
        else {
            // 参数后面有内容，查找下一个分隔符
            char nextChar = pattern[patternPos];
            paramEndPos = uriStr.find(nextChar, uriPos);
            if (paramEndPos == std::string::npos) {
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

// Router类实现
void Router::addRoute(const Route& route) {
    routes.push_back(route);
}

void Router::reset() {
    routes.clear();
}

ngx_int_t Router::route(ngx_http_request_t* r) {
    // 获取请求URI
    std::string uri((char*)r->uri.data, r->uri.len);
    
    NgxLog logger(r);
    logger.info("正在路由请求: %s, 方法: %d", uri.c_str(), r->method);
    logger.info("当前路由器有 %d 条路由规则", routes.size());
    
    // 提取参数
    RouteParams params;
    
    // 遍历所有路由，寻找匹配
    for (const auto& route : routes) {
        // 检查HTTP方法匹配
        if (route.method != ANY_METHOD && route.method != r->method) {
            logger.debug("路由 '%s' 的HTTP方法不匹配: %d != %d", 
                      route.pattern.c_str(), route.method, r->method);
            continue;
        }
        
        // 检查URI匹配
        if (route.match(r->uri, params)) {
            logger.info("找到匹配的路由: %s，参数数量: %d", 
                      route.pattern.c_str(), params.size());
            
            // 查看提取的参数
            for (const auto& param : params) {
                logger.debug("参数: %s = %s", param.first.c_str(), param.second.c_str());
            }
            
            // 调用处理器
            return route.handler(r, params);
        }
    }
    
    logger.warn("没有找到匹配的路由，共尝试 %d 条路由规则", routes.size());
    return NGX_DECLINED;
}

size_t Router::getRouteCount() const {
    return routes.size();
}

std::vector<std::string> Router::dumpRoutes() const {
    std::vector<std::string> result;
    
    for (const auto& route : routes) {
        std::string methodStr;
        switch (route.method) {
            case GET_METHOD: methodStr = "GET"; break;
            case POST_METHOD: methodStr = "POST"; break;
            case PUT_METHOD: methodStr = "PUT"; break;
            case DELETE_METHOD: methodStr = "DELETE"; break;
            case HEAD_METHOD: methodStr = "HEAD"; break;
            case ANY_METHOD: methodStr = "ANY"; break;
            default: methodStr = "UNKNOWN"; break;
        }
        
        result.push_back(methodStr + " " + route.pattern);
    }
    
    return result;
}

// 静态函数声明废弃，移动到BlogModule.cpp中实现
// 将其他函数声明为extern，使其可在其他文件中使用
extern ngx_int_t handleBlogIndex(ngx_http_request_t* r, const RouteParams& params);
extern ngx_int_t handleBlogPost(ngx_http_request_t* r, const RouteParams& params);
extern ngx_int_t handleBlogCategory(ngx_http_request_t* r, const RouteParams& params);
extern ngx_int_t handleBlogTag(ngx_http_request_t* r, const RouteParams& params);
extern ngx_int_t handleAdmin(ngx_http_request_t* r, const RouteParams& params);
extern ngx_int_t handleAddPost(ngx_http_request_t* r, const RouteParams& params);
extern ngx_int_t handleEditPost(ngx_http_request_t* r, const RouteParams& params);
extern ngx_int_t handleDeletePost(ngx_http_request_t* r, const RouteParams& params);
extern ngx_int_t handleBlogRedirect(ngx_http_request_t* r, const RouteParams& params);
