//
// Created by wuxianggujun on 2025/3/14.
//

#include "BlogConfig.hpp"
#include "BlogModule.hpp" // 在实现文件中包含，避免头文件循环依赖

// 从NgxRequest构造配置对象
BlogConfig::BlogConfig(const NgxRequest& request) : request_(&request), config_(nullptr)
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

// 从NgxConf构造配置对象
BlogConfig::BlogConfig(const NgxConf& conf, int type) : request_(nullptr), config_(nullptr)
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

// 直接从配置指针构造
BlogConfig::BlogConfig(BlogModuleConfig* config, const NgxRequest* request) 
  : request_(request), config_(config) {
    if (!config) {
        throw std::runtime_error("无效的配置指针");
    }
}

// 获取配置指针
BlogModuleConfig* BlogConfig::get() const noexcept {
    return config_;
}

// 检查配置是否有效
bool BlogConfig::valid() const noexcept {
    return config_ != nullptr;
}

// 获取博客基础路径
std::string BlogConfig::getBasePath() const {
    if (!valid() || !config_->base_path.data || config_->base_path.len == 0) {
        return "";
    }
    
    return std::string(reinterpret_cast<char*>(config_->base_path.data), 
                     config_->base_path.len);
}

// 获取博客基础路径作为NgxString
NgxString BlogConfig::getBasePathAsNgxString() const {
    if (!valid()) {
        return NgxString();
    }
    
    return NgxString(config_->base_path, false);
}

// 检查缓存是否启用
bool BlogConfig::isEnableCache() const noexcept {
    return valid() && config_->enable_cache;
}

// 获取缓存时间
ngx_uint_t BlogConfig::getCacheTime() const noexcept {
    return valid() ? config_->cache_time : 0;
}

// 检查是否有关联的请求
bool BlogConfig::hasRequest() const noexcept {
    return request_ != nullptr && request_->valid();
}

// 记录日志（如果有关联的请求）
void BlogConfig::log(ngx_uint_t level, const char* fmt, ...) const {
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

// 获取数据库连接字符串
std::string BlogConfig::getDbConnectionString() const {
    if (!valid() || !config_->db_conn_str.data || config_->db_conn_str.len == 0) {
        return "";
    }
    
    return std::string(reinterpret_cast<char*>(config_->db_conn_str.data), 
                     config_->db_conn_str.len);
}

// 是否自动连接数据库
bool BlogConfig::isDbAutoConnect() const noexcept {
    return valid() && config_->db_auto_connect;
}
