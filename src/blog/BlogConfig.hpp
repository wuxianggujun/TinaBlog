//
// Created by wuxianggujun on 2025/3/14.
//

#ifndef TINA_BLOG_BLOG_CONFIG_HPP
#define TINA_BLOG_BLOG_CONFIG_HPP

#include <BlogModule.hpp>

#include "Nginx.hpp"
#include "NgxString.hpp"
#include "NgxRequest.hpp"
#include "NgxConf.hpp"
#include <string>
#include <stdexcept>


// 前向声明，避免循环依赖
extern "C" {
extern ngx_module_t ngx_http_blog_module;
}

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
    explicit BlogConfig(const NgxRequest& request) : request_(&request), config_(nullptr)
    {
        if (!request.valid())
        {
            throw std::runtime_error("无效的HTTP请求");
        }

        config_ = static_cast<BlogModuleConfig*>(ngx_http_get_module_loc_conf(request.get(), ngx_http_blog_module));

        if (!config_)
        {
            throw std::runtime_error("获取博客模块配置失败");
        }
        
    }

    /**
     * @brief 从NgxConf构造配置对象
     * 
     * @param conf 配置上下文
     * @param type 配置类型（主配置、服务器配置或位置配置）
     * @throws std::runtime_error 如果配置获取失败
     */
    explicit BlogConfig(const NgxConf& conf,int type = NGX_HTTP_LOC_CONF) : request_(nullptr), config_(nullptr)
    {
        if (!conf.valid()) {
            throw std::runtime_error("无效的配置上下文");
        }
        
        switch (type) {
        case NGX_HTTP_MAIN_CONF:
            config_ = static_cast<BlogModuleConfig*>(
                ngx_http_conf_get_module_main_conf(conf.get(), ngx_http_blog_module));
            break;
                
        case NGX_HTTP_SRV_CONF:
            config_ = static_cast<BlogModuleConfig*>(
                ngx_http_conf_get_module_srv_conf(conf.get(), ngx_http_blog_module));
            break;
                
        case NGX_HTTP_LOC_CONF:
        default:
            config_ = static_cast<BlogModuleConfig*>(
                ngx_http_conf_get_module_loc_conf(conf.get(), ngx_http_blog_module));
            break;
        }
        
        if (!config_) {
            throw std::runtime_error("获取博客模块配置失败");
        }
    }

    /**
     * @brief 直接从配置指针构造
     * 
     * @param config 配置指针
     * @param request 可选的请求对象引用
     * @throws std::runtime_error 如果配置指针为空
     */
    explicit BlogConfig(BlogModuleConfig* config, const NgxRequest* request = nullptr) 
      : request_(request), config_(config) {
        if (!config) {
            throw std::runtime_error("无效的配置指针");
        }
    }

    /**
     * @brief 获取配置指针
     * 
     * @return BlogModuleConfig* 原始配置指针
     */
    BlogModuleConfig* get() const noexcept {
        return config_;
    }

    /**
     * @brief 检查配置是否有效
     * 
     * @return bool 配置是否有效
     */
    bool valid() const noexcept {
        return config_ != nullptr;
    }

    /**
     * @brief 获取博客基础路径
     * 
     * @return std::string 基础路径
     */
    std::string getBasePath() const {
        if (!valid() || !config_->base_path.data || config_->base_path.len == 0) {
            return "";
        }
        
        return std::string(reinterpret_cast<char*>(config_->base_path.data), 
                         config_->base_path.len);
    }

    /**
     * @brief 获取博客基础路径作为NgxString
     * 
     * @return NgxString 基础路径
     */
    NgxString getBasePathAsNgxString() const {
        if (!valid()) {
            return NgxString();
        }
        
        return NgxString(config_->base_path, false);
    }

    /**
     * @brief 获取模板路径
     * 
     * @return std::string 模板路径
     */
    std::string getTemplatePath() const {
        if (!valid() || !config_->template_path.data || config_->template_path.len == 0) {
            return "";
        }
        
        return std::string(reinterpret_cast<char*>(config_->template_path.data), 
                         config_->template_path.len);
    }

    /**
     * @brief 获取模板路径作为NgxString
     * 
     * @return NgxString 模板路径
     */
    NgxString getTemplatePathAsNgxString() const {
        if (!valid()) {
            return NgxString();
        }
        
        return NgxString(config_->template_path, false);
    }

    /**
    * @brief 检查缓存是否启用
    * 
    * @return bool 缓存是否启用
    */
    bool isEnableCache() const noexcept {
        return valid() && config_->enable_cache;
    }
    
    /**
     * @brief 获取缓存时间
     * 
     * @return ngx_uint_t 缓存时间（秒）
     */
    ngx_uint_t getCacheTime() const noexcept {
        return valid() ? config_->cache_time : 0;
    }
    
    /**
     * @brief 获取完整模板文件路径
     * 
     * @param templateName 模板文件名
     * @return std::string 完整的模板文件路径
     */
    std::string getFullTemplatePath(const std::string& templateName) const {
        std::string pathStr = getTemplatePath();
        
        if (pathStr.empty()) {
            if (request_ && request_->valid()) {
                ngx_log_error(NGX_LOG_ERR, request_->get()->connection->log, 0, "模板路径未设置");
            }
            return "";
        }
        
        // 确保路径以分隔符结尾
        if (pathStr.back() != '/' && pathStr.back() != '\\') {
            pathStr += '/';
        }
        
        pathStr += templateName;
        return pathStr;
    }
    
    /**
     * @brief 检查是否有关联的请求
     * 
     * @return bool 是否有关联的请求
     */
    bool hasRequest() const noexcept {
        return request_ != nullptr && request_->valid();
    }
    
    /**
     * @brief 记录日志（如果有关联的请求）
     * 
     * @param level 日志级别
     * @param fmt 格式字符串
     * @param ... 额外参数
     */
    void log(ngx_uint_t level, const char* fmt, ...) const {
        if (!hasRequest() || !request_->get() || !request_->get()->connection || 
            !request_->get()->connection->log) {
            return;
            }
    
        va_list args;
        va_start(args, fmt);
    
        // 使用更安全的日志记录方法
        ngx_log_error_core(level, request_->get()->connection->log, 0, fmt, args);
    
        va_end(args);
    }

private:
    const NgxRequest* request_;
    // 博客模块配置指针
    BlogModuleConfig* config_;
};


#endif //TINA_BLOG_BLOG_CONFIG_HPP
