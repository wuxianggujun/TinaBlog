//
// Created by wuxianggujun on 2025/3/14.
//

#ifndef TINA_BLOG_BLOG_CONFIG_HPP
#define TINA_BLOG_BLOG_CONFIG_HPP

#include "Nginx.hpp"
#include "NgxString.hpp"
#include "NgxRequest.hpp"
#include "NgxConf.hpp"
#include <string>
// #include <stdexcept>

// 前向声明，避免循环依赖
extern "C" {
extern ngx_module_t ngx_http_blog_module;
}

// 完整声明BlogModuleConfig结构体
struct BlogModuleConfig {
    ngx_str_t base_path;       // 博客基础路径
    ngx_str_t template_path;   // 模板路径
    ngx_str_t template_root;   // 模板根目录
    ngx_str_t prefix;          // URL前缀
    ngx_flag_t enable_cache;   // 是否启用缓存
    ngx_uint_t cache_time;     // 缓存时间（秒）
    ngx_str_t db_conn_str;     // MySQL数据库连接字符串
    ngx_flag_t db_auto_connect; // 是否自动连接数据库
};

/**
 * @brief 博客配置封装类
 * 
 * 封装对BlogModuleConfig的访问，提供友好的C++接口
 */
class BlogConfig
{
public:
    /**
     * @brief 从NgxRequest构造配置对象
     * 
     * @param request NgxRequest对象的引用
     * @throws std::runtime_error 如果配置获取失败
     */
    explicit BlogConfig(const NgxRequest& request);

    /**
     * @brief 从NgxConf构造配置对象
     * 
     * @param conf 配置上下文
     * @param type 配置类型（主配置、服务器配置或位置配置）
     * @throws std::runtime_error 如果配置获取失败
     */
    explicit BlogConfig(const NgxConf& conf, int type = NGX_HTTP_LOC_CONF);

    /**
     * @brief 直接从配置指针构造
     * 
     * @param config 配置指针
     * @param request 可选的请求对象引用
     * @throws std::runtime_error 如果配置指针为空
     */
    explicit BlogConfig(BlogModuleConfig* config, const NgxRequest* request = nullptr);

    /**
     * @brief 获取配置指针
     * 
     * @return BlogModuleConfig* 原始配置指针
     */
    BlogModuleConfig* get() const noexcept;

    /**
     * @brief 检查配置是否有效
     * 
     * @return bool 配置是否有效
     */
    bool valid() const noexcept;

    /**
     * @brief 获取博客基础路径
     * 
     * @return std::string 基础路径
     */
    std::string getBasePath() const;

    /**
     * @brief 获取博客基础路径作为NgxString
     * 
     * @return NgxString 基础路径
     */
    NgxString getBasePathAsNgxString() const;

    /**
     * @brief 获取模板路径
     * 
     * @return std::string 模板路径
     */
    std::string getTemplatePath() const;

    /**
     * @brief 获取模板路径作为NgxString
     * 
     * @return NgxString 模板路径
     */
    NgxString getTemplatePathAsNgxString() const;

    /**
    * @brief 检查缓存是否启用
    * 
    * @return bool 缓存是否启用
    */
    bool isEnableCache() const noexcept;
    
    /**
     * @brief 获取缓存时间
     * 
     * @return ngx_uint_t 缓存时间（秒）
     */
    ngx_uint_t getCacheTime() const noexcept;
    
    /**
     * @brief 获取完整模板文件路径
     * 
     * @param templateName 模板文件名
     * @return std::string 完整的模板文件路径
     */
    std::string getFullTemplatePath(const std::string& templateName) const;
    
    /**
     * @brief 获取数据库连接字符串
     * 
     * @return std::string 数据库连接字符串
     */
    std::string getDbConnectionString() const;
    
    /**
     * @brief 检查是否自动连接数据库
     * 
     * @return bool 是否自动连接数据库
     */
    bool isDbAutoConnect() const noexcept;
    
    /**
     * @brief 检查是否有关联的请求
     * 
     * @return bool 是否有关联的请求
     */
    bool hasRequest() const noexcept;
    
    /**
     * @brief 记录日志（如果有关联的请求）
     * 
     * @param level 日志级别
     * @param fmt 格式字符串
     * @param ... 额外参数
     */
    void log(ngx_uint_t level, const char* fmt, ...) const;

private:
    const NgxRequest* request_;
    BlogModuleConfig* config_;
};

#endif //TINA_BLOG_BLOG_CONFIG_HPP
