//
// Created by wuxianggujun on 2025/3/14.
//

#ifndef TINA_BLOG_BLOG_MODULE_HPP
#define TINA_BLOG_BLOG_MODULE_HPP

#include "Nginx.hpp"
#include <string>
#include <unordered_map>
#include "BlogConfig.hpp"

// 前向声明
class NgxString;
class NgxConf;
class NgxRequest;

/**
 * @brief 博客模块处理器
 * 
 * 提供博客相关功能的HTTP处理器
 * 内部使用NgxConf、NgxString和NgxRequest封装类处理Nginx数据结构
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

    /**
     * @brief 初始化博客模块
     * 
     * @param cycle Nginx周期对象
     * @return ngx_int_t Nginx状态码
     */
    static ngx_int_t init(ngx_cycle_t* cycle);

    /**
     * @brief 获取静态资源路径
     * 
     * @return 静态资源路径
     */
    static std::string getStaticPath();

    /**
     * @brief 加载并提供模板文件
     * 
     * @param r HTTP请求对象
     * @param templateName 模板文件名
     * @return ngx_int_t Nginx状态码
     */
    static ngx_int_t serveTemplate(ngx_http_request_t* r, const char* templateName);

    /**
     * @brief 使用模板变量渲染并发送响应
     * 
     * @param r HTTP请求对象
     * @param templateName 模板文件名
     * @param variables 模板变量
     * @return ngx_int_t Nginx状态码
     */
    static ngx_int_t serveTemplateWithVariables(
        ngx_http_request_t* r,
        const std::string& templateName, 
        const std::unordered_map<std::string, std::string>& variables);

    /**
     * @brief 处理模板，替换变量
     * 
     * @param content 模板内容
     * @param variables 模板变量
     * @return std::string 处理后的模板内容
     */
    static std::string processTemplate(
        const std::string& content, 
        const std::unordered_map<std::string, std::string>& variables);

    // Nginx模块回调函数 - 保持签名不变以兼容Nginx API
    
    /**
     * @brief 预配置回调
     * 内部使用NgxConf封装类处理配置
     */
    static ngx_int_t preConfiguration(ngx_conf_t* cf);
    
    /**
     * @brief 后配置回调
     * 内部使用NgxConf封装类处理配置
     */
    static ngx_int_t postConfiguration(ngx_conf_t* cf);
    
    /**
     * @brief 创建主配置
     * 内部使用NgxConf封装类处理配置
     */
    static void* createMainConfig(ngx_conf_t* cf);
    
    /**
     * @brief 初始化主配置
     * 内部使用NgxConf封装类处理配置
     */
    static char* initMainConfig(ngx_conf_t* cf, void* conf);
    
    /**
     * @brief 创建服务器配置
     * 内部使用NgxConf封装类处理配置
     */
    static void* createServerConfig(ngx_conf_t* cf);
    
    /**
     * @brief 合并服务器配置
     * 内部使用NgxConf封装类处理配置
     */
    static char* mergeServerConfig(ngx_conf_t* cf, void* parent, void* child);
    
    /**
     * @brief 创建位置配置
     * 内部使用NgxConf封装类处理配置
     */
    static void* createLocationConfig(ngx_conf_t* cf);
    
    /**
     * @brief 合并位置配置
     * 内部使用NgxConf封装类处理配置
     */
    static char* mergeLocationConfig(ngx_conf_t* cf, void* parent, void* child);
    
    /**
     * @brief 处理HTTP请求
     * 内部可使用NgxRequest封装类处理请求
     */
    static ngx_int_t handleRequest(ngx_http_request_t* r);
    
    /**
     * @brief 处理博客请求
     * 内部可使用NgxRequest封装类处理请求
     */
    static ngx_int_t handleBlogRequest(ngx_http_request_t* r);
    
    /**
     * @brief 处理博客路径指令
     * 内部使用NgxConf和NgxString封装类处理配置和字符串
     */
    static char* setBlogPath(ngx_conf_t* cf, ngx_command_t* cmd, void* conf);
    
    /**
     * @brief 处理模板路径指令
     * 内部使用NgxConf和NgxString封装类处理配置和字符串
     */
    static char* setTemplatePath(ngx_conf_t* cf, ngx_command_t* cmd, void* conf);
    
    /**
     * @brief 处理启用缓存指令
     * 内部使用NgxConf和NgxString封装类处理配置和字符串
     */
    static char* setEnableCache(ngx_conf_t* cf, ngx_command_t* cmd, void* conf);
    
    /**
     * @brief 处理缓存时间指令
     * 内部使用NgxConf和NgxString封装类处理配置和字符串
     */
    static char* setCacheTime(ngx_conf_t* cf, ngx_command_t* cmd, void* conf);
    
    /**
     * @brief 处理数据库连接字符串指令
     * 内部使用NgxConf和NgxString封装类处理配置和字符串
     */
    static char* setDbConnectionString(ngx_conf_t* cf, ngx_command_t* cmd, void* conf);
    
    /**
     * @brief 处理自动连接数据库指令
     * 内部使用NgxConf和NgxString封装类处理配置和字符串
     */
    static char* setDbAutoConnect(ngx_conf_t* cf, ngx_command_t* cmd, void* conf);

public:
    // 全局配置变量
    static std::string basePath;   // 博客路径
    static std::string version;    // 版本号
    static std::string dataDir;    // 数据目录
    static std::string staticPath; // 静态文件路径

private:
    // 静态成员，防止多次创建
    static bool isRegistered_;
    static ngx_module_t* blogModule_;
};

#endif // TINA_BLOG_BLOG_MODULE_HPP 