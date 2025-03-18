//
// Created by wuxianggujun on 2025/3/18.
//

#ifndef TINA_BLOG_ROUTE_HPP
#define TINA_BLOG_ROUTE_HPP

#include "Nginx.hpp"
#include "HttpMethod.hpp"
#include "NgxLog.hpp"
#include <string>
#include <functional>
#include <map>
#include <utility>
#include <vector>
#include <regex>

// 路由参数类型
using RouteParams = std::map<std::string, std::string>;

// 路由处理函数类型
using RouteHandler = std::function<ngx_int_t(ngx_http_request_t*,const RouteParams&)>;

class Route
{
public:
    explicit Route(HttpMethod method,const std::string& pattern,RouteHandler handler):method_(method),pattern_(pattern),handler_(std::move(handler))
    {
        std::string regexPattern  = pattern;
        // 处理路径参数，形如 :param
        const std::regex paramRegex(":([a-zA-Z0-9_]+)");
        std::smatch matches;
        auto searchStart(regexPattern.cbegin());

        while (std::regex_search(searchStart,regexPattern.cend(),matches,paramRegex))
        {
            // 记录参数名
            paramNames_.push_back(matches[1].str());

            const size_t paramPos = matches.position(0) + (searchStart - regexPattern.cbegin());
            const size_t paramLen = matches.length();
            regexPattern.replace(paramPos, paramLen, "([^/]+)");
            searchStart = regexPattern.cbegin() + paramPos + 7; // 7 is length of "([^/]+)"
        }
        // 处理通配符
        const std::regex wildcardRegex("\\*");
        regexPattern = std::regex_replace(regexPattern, wildcardRegex, ".*");
        
        // 确保整个字符串匹配
        regexPattern = "^" + regexPattern + "$";
        
        // 编译正则表达式
        regex_ = std::regex(regexPattern);
    }


    // 匹配路由
    bool match(const ngx_http_request_t* request,const std::string& uri,RouteParams& params) const
    {
        const NgxLog logger(request->connection->log);

        // 检查HttpMethod是否匹配
        if (const HttpMethod requestMethod = fromNginxMethod(request->method); requestMethod != method_)
        {
            logger.debug("HTTP方法不匹配: %s != %s", 
                 methodToString(requestMethod), 
                 methodToString(method_));
            return false;
        }

        // 使用日志进行匹配调试
        logger.debug("尝试匹配: URI=%s 与模式=%s", uri.c_str(), pattern_.c_str());

        // 使用正则表达式匹配
        if (std::smatch matches; std::regex_match(uri, matches, regex_)) {
            // 如果匹配成功，提取参数
            for (size_t i = 0; i < paramNames_.size() && (i + 1) < matches.size(); ++i) {
                params[paramNames_[i]] = matches[i + 1].str();
            }
            
            logger.debug("完全匹配成功: URI=%s 与模式=%s", uri.c_str(), pattern_.c_str());
            return true;
        }
        return false;
    }

    // 获取路由处理函数
    [[nodiscard]] RouteHandler handler() const {
        return handler_;
    }
    
    // 获取HTTP方法
    [[nodiscard]] HttpMethod method() const {
        return method_;
    }
    
    // 获取路由模式
    [[nodiscard]] std::string pattern() const {
        return pattern_;
    }
    
    // 将路由转换为字符串表示（用于调试）
    [[nodiscard]] std::string toString() const {
        return std::string(methodToString(method_)) + " " + pattern_;
    }

private:
    HttpMethod method_;
    std::string pattern_;
    RouteHandler handler_;
    std::regex regex_;
    std::vector<std::string> paramNames_;
};




#endif //TINA_BLOG_ROUTE_HPP
