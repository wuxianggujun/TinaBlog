//
// Created by wuxianggujun on 2025/3/14.
//

#ifndef TINA_BLOG_JSON_RESPONSE_HPP
#define TINA_BLOG_JSON_RESPONSE_HPP

#include "Nginx.hpp"
#include <nlohmann/json.hpp>
#include <string>
#include <unordered_map>

/**
 * @brief JSON响应工具类
 * 
 * 提供用于创建和发送JSON响应的工具方法
 */
class JsonResponse {
public:
    /**
     * @brief 发送JSON响应
     * 
     * @param r Nginx请求对象
     * @param jsonContent JSON内容字符串
     * @param status HTTP状态码
     * @return ngx_int_t Nginx状态码
     */
    static ngx_int_t send(ngx_http_request_t* r, const std::string& jsonContent, ngx_uint_t status = NGX_HTTP_OK);
    
    /**
     * @brief 发送JSON响应
     * 
     * @param r Nginx请求对象
     * @param jsonObj JSON对象
     * @param status HTTP状态码
     * @return ngx_int_t Nginx状态码
     */
    static ngx_int_t send(ngx_http_request_t* r, const nlohmann::json& jsonObj, ngx_uint_t status = NGX_HTTP_OK);
    
    /**
     * @brief 创建成功响应
     * 
     * @param data 响应数据
     * @return nlohmann::json 成功响应对象
     */
    static nlohmann::json success(const nlohmann::json& data = {});
    
    /**
     * @brief 创建错误响应
     * 
     * @param message 错误消息
     * @param code 错误代码
     * @return nlohmann::json 错误响应对象
     */
    static nlohmann::json error(const std::string& message, int code = 500);
};

#endif // TINA_BLOG_JSON_RESPONSE_HPP 