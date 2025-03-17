//
// Created by wuxianggujun on 2025/3/14.
//

#ifndef TINA_BLOG_NGX_RESPONSE_HPP
#define TINA_BLOG_NGX_RESPONSE_HPP

#include "Nginx.hpp"
#include "NginxContext.hpp"
#include "NgxString.hpp"
#include <string>
#include <vector>
#include <utility>

/**
 * @brief Nginx响应封装类
 * 
 * 封装Nginx HTTP响应操作，提供流式API
 */
class NgxResponse : public NginxContext<ngx_http_request_t> {
public:
    /**
     * @brief 构造函数
     * 
     * @param r Nginx请求对象
     */
    explicit NgxResponse(ngx_http_request_t* r) : NginxContext<ngx_http_request_t>(r), status_(NGX_HTTP_OK) {}
    
    /**
     * @brief 设置HTTP状态码
     * 
     * @param status HTTP状态码
     * @return NgxResponse& 返回自身引用，支持链式调用
     */
    NgxResponse& status(ngx_uint_t status) {
        status_ = status;
        return *this;
    }
    
    /**
     * @brief 设置内容类型
     * 
     * @param contentType 内容类型
     * @return NgxResponse& 返回自身引用，支持链式调用
     */
    NgxResponse& contentType(const std::string& contentType) {
        contentType_ = contentType;
        return *this;
    }
    
    /**
     * @brief 添加响应头
     * 
     * @param name 头部名称
     * @param value 头部值
     * @return NgxResponse& 返回自身引用，支持链式调用
     */
    NgxResponse& header(const std::string& name, const std::string& value) {
        headers_.push_back(std::make_pair(name, value));
        return *this;
    }
    
    /**
     * @brief 启用CORS支持
     * 
     * @param origin 允许的来源，默认为"*"
     * @return NgxResponse& 返回自身引用，支持链式调用
     */
    NgxResponse& enableCors(const std::string& origin = "*") {
        header("Access-Control-Allow-Origin", origin);
        header("Access-Control-Allow-Methods", "GET, POST, PUT, DELETE, OPTIONS");
        header("Access-Control-Allow-Headers", "Content-Type, Authorization, X-Requested-With");
        header("Access-Control-Allow-Credentials", "true");
        header("Access-Control-Max-Age", "86400"); // 24小时
        return *this;
    }
    
    /**
     * @brief 发送文本响应
     * 
     * @param content 响应内容
     * @return ngx_int_t Nginx状态码
     */
    ngx_int_t send(const std::string& content);
    
    /**
     * @brief 发送文本响应（使用NgxString）
     * 
     * @param content NgxString响应内容
     * @return ngx_int_t Nginx状态码
     */
    ngx_int_t send(const NgxString& content);
    
    /**
     * @brief 发送空响应（只有头部，没有响应体）
     * 
     * @return ngx_int_t Nginx状态码
     */
    ngx_int_t sendEmpty();
    
    /**
     * @brief 发送重定向响应
     * 
     * @param url 重定向URL
     * @param status 重定向状态码，默认为302
     * @return ngx_int_t Nginx状态码
     */
    ngx_int_t redirect(const std::string& url, ngx_uint_t status = NGX_HTTP_MOVED_TEMPORARILY);
    
private:
    ngx_uint_t status_;                             ///< HTTP状态码
    std::string contentType_;                       ///< 内容类型
    std::vector<std::pair<std::string, std::string>> headers_; ///< 响应头列表
    
    /**
     * @brief 应用所有设置的响应头
     */
    void applyHeaders();
};

#endif // TINA_BLOG_NGX_RESPONSE_HPP 