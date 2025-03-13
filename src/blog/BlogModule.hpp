//
// Created by wuxianggujun on 2025/3/14.
//

#ifndef TINA_BLOG_BLOG_MODULE_HPP
#define TINA_BLOG_BLOG_MODULE_HPP

#include "Nginx.hpp"
#include <string>

namespace blog {

/**
 * @brief Blog模块配置结构体
 */
struct BlogModuleConfig {
    ngx_str_t base_path;    // 博客基础路径
    ngx_str_t template_path; // 模板路径
    ngx_flag_t enable_cache; // 是否启用缓存
    ngx_uint_t cache_time;   // 缓存时间（秒）
};

/**
 * @brief 博客模块处理器
 * 
 * 提供博客相关功能的HTTP处理器
 */
class BlogModule {
public:
    /**
     * @brief 注册博客模块
     * 
     * @return 是否注册成功
     */
    static bool registerModule();

    /**
     * @brief 获取模块名称
     * 
     * @return 模块名称
     */
    static const char* getModuleName();

    /**
     * @brief 获取模块版本
     * 
     * @return 模块版本
     */
    static const char* getModuleVersion();

private:
    // Nginx模块回调函数
    
    /**
     * @brief 预配置回调
     */
    static ngx_int_t preConfiguration(ngx_conf_t* cf);
    
    /**
     * @brief 后配置回调
     */
    static ngx_int_t postConfiguration(ngx_conf_t* cf);
    
    /**
     * @brief 创建主配置
     */
    static void* createMainConfig(ngx_conf_t* cf);
    
    /**
     * @brief 初始化主配置
     */
    static char* initMainConfig(ngx_conf_t* cf, void* conf);
    
    /**
     * @brief 创建服务器配置
     */
    static void* createServerConfig(ngx_conf_t* cf);
    
    /**
     * @brief 合并服务器配置
     */
    static char* mergeServerConfig(ngx_conf_t* cf, void* parent, void* child);
    
    /**
     * @brief 创建位置配置
     */
    static void* createLocationConfig(ngx_conf_t* cf);
    
    /**
     * @brief 合并位置配置
     */
    static char* mergeLocationConfig(ngx_conf_t* cf, void* parent, void* child);
    
    /**
     * @brief 处理博客请求
     */
    static ngx_int_t handleBlogRequest(ngx_http_request_t* r);
    
    /**
     * @brief 处理博客路径指令
     */
    static char* setBlogPath(ngx_conf_t* cf, ngx_command_t* cmd, void* conf);
    
    /**
     * @brief 处理模板路径指令
     */
    static char* setTemplatePath(ngx_conf_t* cf, ngx_command_t* cmd, void* conf);
    
    /**
     * @brief 处理启用缓存指令
     */
    static char* setEnableCache(ngx_conf_t* cf, ngx_command_t* cmd, void* conf);
    
    /**
     * @brief 处理缓存时间指令
     */
    static char* setCacheTime(ngx_conf_t* cf, ngx_command_t* cmd, void* conf);

private:
    // 静态成员，防止多次创建
    static bool isRegistered_;
    static ngx_module_t* blogModule_;
};

} // namespace blog

#endif // TINA_BLOG_BLOG_MODULE_HPP 