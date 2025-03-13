//
// Created by wuxianggujun on 2025/3/14.
//

#ifndef TINA_BLOG_HTTP_CONFIG_HELPER_HPP
#define TINA_BLOG_HTTP_CONFIG_HELPER_HPP

#include "Nginx.hpp"
#include <functional>
#include <string>
#include <memory>
#include <vector>

namespace blog {

/**
 * @brief 辅助处理HTTP模块配置的工具类
 *
 * 提供创建和管理nginx HTTP模块配置的辅助方法
 */
class HttpConfigHelper {
public:
    /**
     * @brief 创建ngx_command_t项
     * 
     * @param name 命令名称
     * @param type 命令类型
     * @param set 命令处理函数
     * @param conf 配置偏移量
     * @param offset 成员偏移量
     * @param post 后处理函数
     * @return ngx_command_t 命令对象
     */
    static ngx_command_t createCommand(
        const std::string& name,
        ngx_uint_t type,
        char* (*set)(ngx_conf_t*, ngx_command_t*, void*),
        ngx_uint_t conf,
        ngx_uint_t offset,
        void* post = nullptr
    );

    /**
     * @brief 创建结束命令（ngx_null_command）
     * 
     * @return ngx_command_t 空命令对象
     */
    static ngx_command_t createNullCommand();

    /**
     * @brief 创建命令数组
     * 
     * @param commands 命令列表
     * @return ngx_command_t* 命令数组，需要外部释放
     */
    static ngx_command_t* createCommandArray(const std::vector<ngx_command_t>& commands);

    /**
     * @brief 将C++字符串转换为ngx_str_t
     * 
     * @param str C++字符串
     * @return ngx_str_t Nginx字符串
     */
    static ngx_str_t stringToNgxStr(const std::string& str);

    /**
     * @brief 将ngx_str_t转换为C++字符串
     * 
     * @param str Nginx字符串
     * @return std::string C++字符串
     */
    static std::string ngxStrToString(const ngx_str_t& str);

    /**
     * @brief 设置允许连接访问的方法包装器
     * 
     * @param r HTTP请求
     * @param methods 允许的HTTP方法标志位
     * @return NGX_OK或错误码
     */
    static ngx_int_t setAllowedMethods(ngx_http_request_t* r, ngx_uint_t methods);
};

} // namespace blog

#endif // TINA_BLOG_HTTP_CONFIG_HELPER_HPP 