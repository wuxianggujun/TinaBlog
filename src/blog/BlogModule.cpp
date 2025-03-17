//
// Created by wuxianggujun on 2025/3/14.
//

#include "BlogModule.hpp"
#include "NgxConf.hpp"
#include "BlogRouter.hpp"
#include "BlogTemplate.hpp"
#include "BlogPostManager.hpp"
#include "db/DbManager.hpp"
#include "NgxLog.hpp"
#include "BlogConfig.hpp"
#include "JsonResponse.hpp"
#include <iostream>
#include <cstring>
#include <thread>
#include <mutex>
#include <regex>
#include <string>
#include <vector>
#include <fstream>
#include <sstream>
#include <iomanip>
#include <chrono>
#include <ctime>
#include <functional>
#include <nlohmann/json.hpp>

#include "BlogPostDao.hpp"

using json = nlohmann::json;

// 路由处理函数声明
namespace {
    // 所有路由处理函数的实现声明为文件内部静态函数
    ngx_int_t handleBlogIndex(NgxResponse& response, const RouteParams& params);
    ngx_int_t handleBlogPost(NgxResponse& response, const RouteParams& params);
    ngx_int_t handleBlogCategory(NgxResponse& response, const RouteParams& params);
    ngx_int_t handleBlogTag(NgxResponse& response, const RouteParams& params);
    ngx_int_t handleAdmin(NgxResponse& response, const RouteParams& params);
    ngx_int_t handleAddPost(NgxResponse& response, const RouteParams& params);
    ngx_int_t handleEditPost(NgxResponse& response, const RouteParams& params);
    ngx_int_t handleDeletePost(NgxResponse& response, const RouteParams& params);
    ngx_int_t handleBlogRedirect(NgxResponse& response, const RouteParams& params);
    ngx_int_t handleAdminStats(NgxResponse& response, const RouteParams& params);
    ngx_int_t handleGetPostForEdit(NgxResponse& response, const RouteParams& params);
    ngx_int_t handleOptionsRequest(NgxResponse& response, const RouteParams& params);
    
    void initBlogTemplates();
}

// 声明外部模块变量，这样代码中使用时编译器能识别它
extern "C" {
    extern ngx_module_t ngx_http_blog_module;
}

// 使用前向声明
namespace {
    void initBlogRouter(const BlogConfig& config);
    ngx_int_t handleRequest(ngx_http_request_t* r);
    ngx_int_t handleStaticRequest(ngx_http_request_t* r);
    ngx_int_t serveStaticFile(ngx_http_request_t* r, const std::string& path);
}

// 前向声明
static void initBlogRoutes(const std::string& basePath, Router& router);
// 新增前向声明
void initBlogRoutes(BlogRouter* router);

// 初始化静态成员变量
bool BlogModule::isRegistered_ = false;
ngx_module_t* BlogModule::blogModule_ = nullptr;
std::string BlogModule::basePath = "/blog";
std::string BlogModule::version = "1.0.0";
std::string BlogModule::dataDir = "/var/www/blog/data";
std::string BlogModule::staticPath = "/var/www/blog/static";

// 获取静态资源路径
std::string BlogModule::getStaticPath() {
    return staticPath;
}

// 定义模块名称和版本
constexpr const char* MODULE_NAME = "ngx_http_blog_module";
constexpr const char* MODULE_VERSION = "1.0.0";

// 命令定义 - 作为全局变量
static ngx_command_t blog_commands[] = {
    { ngx_string("blog_path"),
      NGX_HTTP_MAIN_CONF|NGX_HTTP_SRV_CONF|NGX_HTTP_LOC_CONF|NGX_CONF_TAKE1,
      BlogModule::setBlogPath,
      NGX_HTTP_LOC_CONF_OFFSET,
      offsetof(BlogModuleConfig, base_path),
      NULL },
    
    { ngx_string("blog_template_path"),
      NGX_HTTP_MAIN_CONF|NGX_HTTP_SRV_CONF|NGX_HTTP_LOC_CONF|NGX_CONF_TAKE1,
      BlogModule::setTemplatePath,
      NGX_HTTP_LOC_CONF_OFFSET,
      offsetof(BlogModuleConfig, template_path),
      NULL },
    
    { ngx_string("blog_enable_cache"),
      NGX_HTTP_MAIN_CONF|NGX_HTTP_SRV_CONF|NGX_HTTP_LOC_CONF|NGX_CONF_FLAG,
      BlogModule::setEnableCache,
      NGX_HTTP_LOC_CONF_OFFSET,
      offsetof(BlogModuleConfig, enable_cache),
      NULL },
    
    { ngx_string("blog_cache_time"),
      NGX_HTTP_MAIN_CONF|NGX_HTTP_SRV_CONF|NGX_HTTP_LOC_CONF|NGX_CONF_TAKE1,
      BlogModule::setCacheTime,
      NGX_HTTP_LOC_CONF_OFFSET,
      offsetof(BlogModuleConfig, cache_time),
      NULL },
    
    { ngx_string("blog_db_connection"),
      NGX_HTTP_MAIN_CONF|NGX_HTTP_SRV_CONF|NGX_HTTP_LOC_CONF|NGX_CONF_TAKE1,
      BlogModule::setDbConnectionString,
      NGX_HTTP_LOC_CONF_OFFSET,
      offsetof(BlogModuleConfig, db_conn_str),
      NULL },
    
    { ngx_string("blog_db_auto_connect"),
      NGX_HTTP_MAIN_CONF|NGX_HTTP_SRV_CONF|NGX_HTTP_LOC_CONF|NGX_CONF_FLAG,
      BlogModule::setDbAutoConnect,
      NGX_HTTP_LOC_CONF_OFFSET,
      offsetof(BlogModuleConfig, db_auto_connect),
      NULL },
    
    ngx_null_command
};

// 模块上下文定义 - 作为全局变量
static ngx_http_module_t blog_module_ctx = {
    BlogModule::preConfiguration,   /* preconfiguration */
    BlogModule::postConfiguration,  /* postconfiguration */
    BlogModule::createMainConfig,   /* create main configuration */
    BlogModule::initMainConfig,     /* init main configuration */
    BlogModule::createServerConfig, /* create server configuration */
    BlogModule::mergeServerConfig,  /* merge server configuration */
    BlogModule::createLocationConfig, /* create location configuration */
    BlogModule::mergeLocationConfig   /* merge location configuration */
};

// registerModule函数可以删除，因为现在我们直接在ngx_modules.c中注册
// 改为记录日志的函数
bool BlogModule::registerModule() {
    std::cout << "BlogModule is now registered directly in ngx_modules.c" << std::endl;
    std::cout << "Module name: " << MODULE_NAME << ", version: " << MODULE_VERSION << std::endl;
    return true;
}

const char* BlogModule::getModuleName() {
    return MODULE_NAME;
}

const char* BlogModule::getModuleVersion() {
    return MODULE_VERSION;
}

// 预配置回调
ngx_int_t BlogModule::preConfiguration(ngx_conf_t* cf) {
    // 使用NgxConf封装原始指针
    NgxConf conf(cf);
    
    // 这里可以进行一些预配置工作
    return NGX_OK;
}

// 后配置回调
ngx_int_t BlogModule::postConfiguration(ngx_conf_t* cf) {
    // 使用NgxConf封装原始指针
    NgxConf conf(cf);
    
    ngx_log_error(NGX_LOG_INFO, cf->log, 0, "BlogModule::postConfiguration - 开始配置");
    
    ngx_http_core_main_conf_t* cmcf;
    ngx_http_handler_pt* h;

    // 获取HTTP核心模块的主配置
    cmcf = static_cast<ngx_http_core_main_conf_t*>(
        ngx_http_conf_get_module_main_conf(conf.get(), ngx_http_core_module));

    // 添加处理器到访问阶段
    h = static_cast<ngx_http_handler_pt*>(
        ngx_array_push(&cmcf->phases[NGX_HTTP_CONTENT_PHASE].handlers));

    if (h == nullptr) {
        ngx_log_error(NGX_LOG_ERR, cf->log, 0, "无法添加内容处理阶段处理器");
        return NGX_ERROR;
    }

    // 设置处理器为BlogModule的请求处理函数
    *h = handleRequest;
    ngx_log_error(NGX_LOG_INFO, cf->log, 0, "请求处理器已注册");

    // 获取位置配置
    auto* loc_conf = static_cast<BlogModuleConfig*>(
        ngx_http_conf_get_module_loc_conf(cf, ngx_http_blog_module));
        
    // 获取主配置，用于获取全局数据库设置
    auto* main_conf = static_cast<BlogModuleConfig*>(
        ngx_http_conf_get_module_main_conf(cf, ngx_http_blog_module));

    // 首先初始化数据库连接（优先使用位置配置，如果没有则尝试使用主配置）
    bool db_initialized = false;
    
    // 尝试使用位置配置中的数据库连接字符串
    if (loc_conf && loc_conf->db_conn_str.len > 0) {
        std::string connStr((char*)loc_conf->db_conn_str.data, loc_conf->db_conn_str.len);
        
        // 使用日志记录进度
        ngx_log_error(NGX_LOG_INFO, cf->log, 0, 
                    "初始化数据库连接(位置配置)，连接字符串: %s", connStr.c_str());
                    
        try {
            auto& dbManager = DbManager::getInstance();
            bool success = dbManager.initialize(connStr);
            
            ngx_log_error(NGX_LOG_INFO, cf->log, 0, 
                        "数据库初始化%s", 
                        success ? "成功" : "失败");
                        
        } catch (const std::exception& e) {
            ngx_log_error(NGX_LOG_ERR, cf->log, 0, 
                        "初始化数据库异常: %s", e.what());
            return NGX_ERROR; // 如果初始化失败，直接返回错误
        }
        
        db_initialized = true;
    }
    // 如果位置配置没有数据库连接字符串，尝试使用主配置
    else if (main_conf && main_conf->db_conn_str.len > 0) {
        std::string connStr((char*)main_conf->db_conn_str.data, main_conf->db_conn_str.len);
        
        // 使用日志记录进度
        ngx_log_error(NGX_LOG_INFO, cf->log, 0, 
                    "初始化数据库连接(主配置)，连接字符串: %s", connStr.c_str());
                    
        try {
            auto& dbManager = DbManager::getInstance();
            bool success = dbManager.initialize(connStr);
            
            ngx_log_error(NGX_LOG_INFO, cf->log, 0, 
                        "数据库初始化%s", 
                        success ? "成功" : "失败");
                        
            if (!success) {
                return NGX_ERROR; // 如果初始化失败，直接返回错误
            }
                        
        } catch (const std::exception& e) {
            ngx_log_error(NGX_LOG_ERR, cf->log, 0, 
                        "初始化数据库异常: %s", e.what());
            return NGX_ERROR; // 如果初始化失败，直接返回错误
        }
        
        db_initialized = true;
    }
    // 如果没有任何配置，使用默认值
    else {
        ngx_log_error(NGX_LOG_WARN, cf->log, 0, "数据库连接字符串未配置(主配置和位置配置)，使用默认值");
        
        try {
            auto& dbManager = DbManager::getInstance();
            // 使用默认连接字符串初始化数据库
            std::string defaultConnStr = "mysqlx://root@127.0.0.1:3306/blog";
            bool success = dbManager.initialize(defaultConnStr);
            
            ngx_log_error(NGX_LOG_INFO, cf->log, 0, 
                        "使用默认设置初始化数据库%s", 
                        success ? "成功" : "失败");
                        
            if (!success) {
                return NGX_ERROR; // 如果初始化失败，直接返回错误
            }
        } catch (const std::exception& e) {
            ngx_log_error(NGX_LOG_ERR, cf->log, 0, 
                        "使用默认设置初始化数据库异常: %s", e.what());
            return NGX_ERROR; // 如果初始化失败，直接返回错误
        }
        
        db_initialized = true;
    }

    // 处理博客路径配置
    if (loc_conf && loc_conf->base_path.len > 0) {
        // 初始化文章管理器
        std::string basePath((char*)loc_conf->base_path.data, loc_conf->base_path.len);
        std::string postsPath = basePath + "/posts";
        
        // 使用日志记录进度
        ngx_log_error(NGX_LOG_INFO, cf->log, 0, 
                     "初始化博客文章管理器，文章目录: %s", postsPath.c_str());
                     
        // 异步初始化文章管理器
        std::thread([postsPath]() {
            try {
                auto& manager = BlogPostManager::getInstance();
                bool success = manager.initialize(postsPath);
                
                ngx_log_error(NGX_LOG_INFO, ngx_cycle->log, 0, 
                             "博客文章管理器初始化%s，加载了 %d 篇文章", 
                             success ? "成功" : "失败",
                             (int)manager.getPostCount());
            } catch (const std::exception& e) {
                ngx_log_error(NGX_LOG_ERR, ngx_cycle->log, 0, 
                             "初始化博客文章管理器异常: %s", e.what());
            }
        }).detach();

        // 显式初始化路由一次
        BlogConfig blogConfig(loc_conf);
        ngx_log_error(NGX_LOG_INFO, cf->log, 0, "Initializing router, base path: %s", basePath.c_str());
        
        // 获取路由器实例并初始化
        auto& router = getBlogRouter();
        router.reset(); // 确保路由器是空的
        initBlogRoutes(&router);
        
        // 输出已注册的路由
        ngx_log_error(NGX_LOG_INFO, cf->log, 0, "Total registered routes: %d", (int)router.getRouteCount());
        std::vector<std::string> routeStrings = router.dumpRoutes();
        for (const auto& routeStr : routeStrings) {
            ngx_log_error(NGX_LOG_DEBUG, cf->log, 0, "Route: %s", routeStr.c_str());
        }
    } else {
        ngx_log_error(NGX_LOG_WARN, cf->log, 0, "Blog base path not configured, using default value");
                
        // 获取路由器实例并初始化
        auto& router = getBlogRouter();
        router.reset(); // 确保路由器是空的
        initBlogRoutes(&router);
    }
    
    ngx_log_error(NGX_LOG_INFO, cf->log, 0, "BlogModule::postConfiguration - 配置完成");
    return NGX_OK;
}

// 创建主配置
void* BlogModule::createMainConfig(ngx_conf_t* cf) {
    // 使用NgxConf封装原始指针
    NgxConf conf(cf);
    
    // 分配内存
    auto* config = static_cast<BlogModuleConfig*>(
        ngx_pcalloc(conf.pool(), sizeof(BlogModuleConfig)));

    if (config == nullptr) {
        return nullptr;
    }

    // 设置默认值
    config->enable_cache = NGX_CONF_UNSET;
    config->cache_time = NGX_CONF_UNSET_UINT;

    return config;
}

// 初始化主配置
char* BlogModule::initMainConfig(ngx_conf_t* cf, void* conf) {
    // 使用NgxConf封装原始指针
    NgxConf ctx(cf);
    
    // 这里可以进行主配置的初始化
    return NGX_CONF_OK;
}

// 创建服务器配置
void* BlogModule::createServerConfig(ngx_conf_t* cf) {
    // 使用NgxConf封装原始指针
    NgxConf conf(cf);
    
    // 分配内存
    auto* config = static_cast<BlogModuleConfig*>(
        ngx_pcalloc(conf.pool(), sizeof(BlogModuleConfig)));

    if (config == nullptr) {
        return nullptr;
    }

    // 设置默认值
    config->enable_cache = NGX_CONF_UNSET;
    config->cache_time = NGX_CONF_UNSET_UINT;

    return config;
}

// 合并服务器配置
char* BlogModule::mergeServerConfig(ngx_conf_t* cf, void* parent, void* child) {
    // 使用NgxConf封装原始指针
    NgxConf conf(cf);
    
    auto* prev = static_cast<BlogModuleConfig*>(parent);
    auto* curr = static_cast<BlogModuleConfig*>(child);

    // 合并配置
    ngx_conf_merge_str_value(curr->base_path, prev->base_path, "");
    ngx_conf_merge_str_value(curr->template_path, prev->template_path, "");
    ngx_conf_merge_value(curr->enable_cache, prev->enable_cache, 0);
    ngx_conf_merge_uint_value(curr->cache_time, prev->cache_time, 60);

    return NGX_CONF_OK;
}

// 创建位置配置
void* BlogModule::createLocationConfig(ngx_conf_t* cf)
{
    // 创建配置结构体
    BlogModuleConfig* conf = static_cast<BlogModuleConfig*>(
        ngx_pcalloc(cf->pool, sizeof(BlogModuleConfig)));
        
    if (conf == nullptr) {
        return NGX_CONF_ERROR;
    }
    
    // 设置默认值
    conf->enable_cache = NGX_CONF_UNSET;  // 未设置标记
    conf->cache_time = NGX_CONF_UNSET_UINT;  // 未设置标记
    conf->db_auto_connect = NGX_CONF_UNSET; // 未设置标记
    
    return conf;
}

// 合并位置配置
char* BlogModule::mergeLocationConfig(ngx_conf_t* cf, void* parent, void* child)
{
    // 转换父子配置为模块配置类型
    BlogModuleConfig* prev = static_cast<BlogModuleConfig*>(parent);
    BlogModuleConfig* conf = static_cast<BlogModuleConfig*>(child);
    
    // 合并配置，优先使用子配置（就近原则）
    
    // 处理base_path（未设置则继承）
    if (conf->base_path.data == nullptr) {
        conf->base_path = prev->base_path;
    }
    
    // 处理template_path（未设置则继承）
    if (conf->template_path.data == nullptr) {
        conf->template_path = prev->template_path;
    }
    
    // 处理enable_cache（未设置则继承，如果父级也未设置则默认为0）
    ngx_conf_merge_value(conf->enable_cache, prev->enable_cache, 0);
    
    // 处理cache_time（未设置则继承，如果父级也未设置则默认为60秒）
    ngx_conf_merge_uint_value(conf->cache_time, prev->cache_time, 60);
    
    // 处理数据库连接字符串（未设置则继承）
    if (conf->db_conn_str.data == nullptr) {
        conf->db_conn_str = prev->db_conn_str;
    }
    
    // 处理自动连接数据库标志（未设置则继承，如果父级也未设置则默认为0）
    ngx_conf_merge_value(conf->db_auto_connect, prev->db_auto_connect, 0);
    
    return NGX_CONF_OK;
}

// 博客请求处理函数
ngx_int_t BlogModule::handleRequest(ngx_http_request_t* r) {
    try {
        // 封装请求
        NgxRequest request(r);
        
        // 创建配置对象
        BlogConfig config(request);
        
        // 创建日志对象
        NgxLog logger(r);
        
        // 获取URI
        std::string uri = request.getUri();
        logger.info("处理请求: %s, 方法: %d", uri.c_str(), r->method);
        
        // 检查是否是静态文件请求
        if (uri.find("/static/") != std::string::npos) {
            logger.debug("静态文件请求，转发到静态处理程序");
            return handleStaticRequest(r);
        }
        
        // 获取当前路由器
        auto& router = getBlogRouter();
        logger.debug("路由器中注册的路由数量: %d", router.getRouteCount());
        
        // 路由参数
        RouteParams params;
        
        // 直接尝试匹配路由
        ngx_str_t ngx_uri;
        ngx_uri.data = (u_char*)uri.c_str();
        ngx_uri.len = uri.length();
        
        std::vector<std::string> routeStrings = router.dumpRoutes();
        for (const auto& routeStr : routeStrings) {
            logger.debug("尝试与路由 '%s' 匹配", routeStr.c_str());
        }
        
        // 使用路由器处理请求
        RouteHandler handler = router.match(r, uri, params);
        
        if (handler) {
            logger.info("找到匹配的路由处理器，执行处理");
            return handler(r, params);
        }
        
        // 尝试使用route方法作为备用
        ngx_int_t status = router.route(r);
        
        if (status != NGX_DECLINED) {
            return status;
        }
        
        // 未找到匹配的路由，返回404
        logger.warn("No matching route found: %s", uri.c_str());
        return NGX_HTTP_NOT_FOUND;
    }
    catch (const std::exception& e) {
        ngx_log_error(NGX_LOG_ERR, r->connection->log, 0, 
                     "处理博客请求异常: %s", e.what());
        return NGX_HTTP_INTERNAL_SERVER_ERROR;
    }
}

// 博客路径指令处理函数
char* BlogModule::setBlogPath(ngx_conf_t* cf, ngx_command_t* cmd, void* conf)
{
    // 包装NgxConf类方便处理，同时记录方法入口日志
    NgxConf ngxConf(cf);
    ngx_log_debug0(NGX_LOG_DEBUG_HTTP, cf->log, 0, "Enter BlogModule::setBlogPath");
    
    // 使用NgxConf包装类处理复杂配置逻辑
    try {
        // 获取配置参数
        NgxString value = ngxConf.getValue(1);
        
        // 调试输出
        ngx_log_debug1(NGX_LOG_DEBUG_HTTP, cf->log, 0, 
                    "Setting blog_path to: %V", &value.get());
        
        // 设置配置
        BlogModuleConfig* config = static_cast<BlogModuleConfig*>(conf);
        config->base_path = value.get();
        
        return NGX_CONF_OK;
    }
    catch (const std::exception& e) {
        ngx_log_error(NGX_LOG_ERR, cf->log, 0, "Error in setBlogPath: %s", e.what());
        return const_cast<char*>("failed to set blog_path directive");
    }
}

// 模板路径指令处理函数
char* BlogModule::setTemplatePath(ngx_conf_t* cf, ngx_command_t* cmd, void* conf)
{
    // 包装NgxConf类方便处理，同时记录方法入口日志
    NgxConf ngxConf(cf);
    ngx_log_debug0(NGX_LOG_DEBUG_HTTP, cf->log, 0, "Enter BlogModule::setTemplatePath");
    
    // 使用NgxConf包装类处理复杂配置逻辑
    try {
        // 获取配置参数
        NgxString value = ngxConf.getValue(1);
        
        // 调试输出
        ngx_log_debug1(NGX_LOG_DEBUG_HTTP, cf->log, 0, 
                    "Setting blog_template_path to: %V", &value.get());
        
        // 设置配置
        BlogModuleConfig* config = static_cast<BlogModuleConfig*>(conf);
        config->template_path = value.get();
        
        return NGX_CONF_OK;
    }
    catch (const std::exception& e) {
        ngx_log_error(NGX_LOG_ERR, cf->log, 0, "Error in setTemplatePath: %s", e.what());
        return const_cast<char*>("failed to set blog_template_path directive");
    }
}

// 启用缓存指令处理函数
char* BlogModule::setEnableCache(ngx_conf_t* cf, ngx_command_t* cmd, void* conf)
{
    // 使用Nginx内置函数处理bool值
    return ngx_conf_set_flag_slot(cf, cmd, conf);
}

// 缓存时间指令处理函数
char* BlogModule::setCacheTime(ngx_conf_t* cf, ngx_command_t* cmd, void* conf)
{
    // 使用Nginx内置函数处理数值
    return ngx_conf_set_num_slot(cf, cmd, conf);
}

// 数据库连接字符串指令处理函数
char* BlogModule::setDbConnectionString(ngx_conf_t* cf, ngx_command_t* cmd, void* conf)
{
    // 包装NgxConf类方便处理，同时记录方法入口日志
    NgxConf ngxConf(cf);
    ngx_log_debug0(NGX_LOG_DEBUG_HTTP, cf->log, 0, "Enter BlogModule::setDbConnectionString");
    
    // 使用NgxConf包装类处理复杂配置逻辑
    try {
        // 获取配置参数
        NgxString value = ngxConf.getValue(1);
        
        // 调试输出
        ngx_log_debug1(NGX_LOG_DEBUG_HTTP, cf->log, 0, 
                    "Setting blog_db_connection to: %V", &value.get());
        
        // 设置配置
        BlogModuleConfig* config = static_cast<BlogModuleConfig*>(conf);
        config->db_conn_str = value.get();
        
        return NGX_CONF_OK;
    }
    catch (const std::exception& e) {
        ngx_log_error(NGX_LOG_ERR, cf->log, 0, "Error in setDbConnectionString: %s", e.what());
        return const_cast<char*>("failed to set blog_db_connection directive");
    }
}

// 自动连接数据库指令处理函数
char* BlogModule::setDbAutoConnect(ngx_conf_t* cf, ngx_command_t* cmd, void* conf)
{
    // 使用Nginx内置函数处理bool值
    return ngx_conf_set_flag_slot(cf, cmd, conf);
}

// 实现模板服务函数
ngx_int_t BlogModule::serveTemplate(ngx_http_request_t* r, const char* templateName) {
    // 获取模块配置
    BlogModuleConfig* bmcf = static_cast<BlogModuleConfig*>(
        ngx_http_get_module_loc_conf(r, ngx_http_blog_module));
    
    if (bmcf == nullptr) {
        ngx_log_error(NGX_LOG_ERR, r->connection->log, 0, "Failed to get blog module configuration");
        return NGX_HTTP_INTERNAL_SERVER_ERROR;
    }

    // 检查模板路径是否配置
    if (bmcf->template_path.len == 0 || bmcf->template_path.data == nullptr) {
        ngx_log_error(NGX_LOG_ERR, r->connection->log, 0, "Template path is not set");
        return NGX_HTTP_INTERNAL_SERVER_ERROR;
    }

    ngx_log_error(NGX_LOG_INFO, r->connection->log, 0, "Template path: %s", bmcf->template_path.data);

    // 构建模板文件路径
    ngx_str_t template_file;
    template_file.len = bmcf->template_path.len + ngx_strlen(templateName) + 1; // +1 for '/'
    template_file.data = static_cast<u_char*>(ngx_palloc(r->pool, template_file.len + 1));
    
    if (template_file.data == nullptr) {
        return NGX_HTTP_INTERNAL_SERVER_ERROR;
    }
    
    // 复制路径并确保路径结尾有斜杠
    u_char* p = ngx_copy(template_file.data, bmcf->template_path.data, bmcf->template_path.len);
    
    // 检查最后一个字符是否为斜杠
    if (bmcf->template_path.len > 0 && *(p - 1) != '/' && *(p - 1) != '\\') {
        *p++ = '/';
    } else {
        // 如果已经有斜杠，减少总长度
        template_file.len--;
    }
    
    // 添加文件名
    p = ngx_cpystrn(p, (u_char*)templateName, ngx_strlen(templateName) + 1);
    
    // 确保字符串以NULL结尾
    *p = '\0';
    
    // 打开模板文件
    ngx_log_error(NGX_LOG_INFO, r->connection->log, 0, "Opening template file: %s", template_file.data);
    
    // 使用直接的方式打开文件
    ngx_file_t file;
    ngx_memzero(&file, sizeof(ngx_file_t));
    
    file.name = template_file;
    file.log = r->connection->log;
    
    file.fd = ngx_open_file(template_file.data, NGX_FILE_RDONLY, NGX_FILE_OPEN, 0);
    
    if (file.fd == NGX_INVALID_FILE) {
        ngx_log_error(NGX_LOG_ERR, r->connection->log, ngx_errno,
                      "Failed to open template file: %s", template_file.data);
        return NGX_HTTP_NOT_FOUND;
    }
    
    // 获取文件大小
    off_t file_size;
    
    // 首先获取文件信息
    if (ngx_file_info(template_file.data, &file.info) == NGX_FILE_ERROR) {
        ngx_log_error(NGX_LOG_ERR, r->connection->log, ngx_errno,
                     "Failed to get file info: %s", template_file.data);
        ngx_close_file(file.fd);
        return NGX_HTTP_INTERNAL_SERVER_ERROR;
    }
    
    // 然后通过宏获取文件大小
    file_size = ngx_file_size(&file.info);
    
    if (file_size == 0) {
        ngx_log_error(NGX_LOG_ERR, r->connection->log, 0,
                      "Template file is empty: %s", template_file.data);
        ngx_close_file(file.fd);
        return NGX_HTTP_INTERNAL_SERVER_ERROR;
    }
    
    // 设置响应头类型
    r->headers_out.content_type_len = sizeof("text/html") - 1;
    r->headers_out.content_type.len = sizeof("text/html") - 1;
    r->headers_out.content_type.data = (u_char *) "text/html";
    
    // 设置响应码
    r->headers_out.status = NGX_HTTP_OK;
    
    // 设置响应内容长度
    r->headers_out.content_length_n = file_size;
    
    // 如果是HEAD请求，只发送头部
    if (r->method == NGX_HTTP_HEAD) {
        ngx_int_t rc = ngx_http_send_header(r);
        ngx_close_file(file.fd);
        
        if (rc == NGX_ERROR || rc > NGX_OK) {
            return rc;
        }
        
        return NGX_OK;
    }
    
    // 发送响应头
    ngx_int_t rc = ngx_http_send_header(r);
    if (rc == NGX_ERROR || rc > NGX_OK) {
        ngx_close_file(file.fd);
        return rc;
    }
    
    // 创建读取文件的缓冲区
    ngx_buf_t* b = static_cast<ngx_buf_t*>(ngx_calloc_buf(r->pool));
    if (b == nullptr) {
        ngx_close_file(file.fd);
        return NGX_HTTP_INTERNAL_SERVER_ERROR;
    }
    
    // 设置缓冲区与文件关联
    b->file = static_cast<ngx_file_t*>(ngx_pcalloc(r->pool, sizeof(ngx_file_t)));
    if (b->file == nullptr) {
        ngx_close_file(file.fd);
        return NGX_HTTP_INTERNAL_SERVER_ERROR;
    }
    
    // 复制文件信息
    b->file->fd = file.fd;
    b->file->name = file.name;
    b->file->log = file.log;
    
    b->file_pos = 0;
    b->file_last = file_size;
    
    b->in_file = 1;
    b->last_buf = 1;
    b->last_in_chain = 1;
    b->sync = 1;     // 建议使用同步读取
    
    // 创建缓冲区链
    ngx_chain_t out;
    out.buf = b;
    out.next = nullptr;
    
    // 发送响应体
    return ngx_http_output_filter(r, &out);
}

ngx_int_t BlogModule::serveTemplateWithVariables(ngx_http_request_t* r, 
                                                const std::string& templateName,
                                                const std::unordered_map<std::string, std::string>& variables) {
    // 获取模块配置
    auto conf = static_cast<BlogModuleConfig*>(
        ngx_http_get_module_loc_conf(r, ngx_http_blog_module));
    if (conf == NULL) {
        ngx_log_error(NGX_LOG_ERR, r->connection->log, 0, 
                      "获取博客模块配置失败");
        return NGX_HTTP_INTERNAL_SERVER_ERROR;
    }

    // 创建日志对象
    NgxLog logger(r);

    // 检查模板路径
    if (conf->template_path.len == 0) {
        logger.error("模板路径未配置");
        return NGX_HTTP_INTERNAL_SERVER_ERROR;
    }

    // 构建完整的模板路径
    std::string templatePath = std::string((char*)conf->template_path.data, conf->template_path.len);
    if (templatePath.back() != '/' && templatePath.back() != '\\') {
        templatePath += '/';
    }
    templatePath += templateName;

    logger.info("使用模板: %s", templatePath.c_str());

    // 读取模板文件
    std::ifstream file(templatePath);
    if (!file.is_open()) {
        logger.error("无法打开模板文件: %s", templatePath.c_str());
        return NGX_HTTP_INTERNAL_SERVER_ERROR;
    }

    // 读取文件内容
    std::stringstream buffer;
    buffer << file.rdbuf();
    std::string content = buffer.str();
    file.close();

    // 增强的模板变量替换处理
    std::string processedContent = processTemplate(content, variables);

    // 设置响应
    r->headers_out.content_type_len = sizeof("text/html") - 1;
    r->headers_out.content_type.data = (u_char*)"text/html";
    r->headers_out.status = NGX_HTTP_OK;

    // 设置缓存控制
    if (conf->enable_cache) {
        u_char timeStr[128];
        time_t now = ngx_time();
        time_t expires = now + conf->cache_time;

        // 创建expires头
        ngx_table_elt_t* expires_header = static_cast<ngx_table_elt_t*>(
            ngx_list_push(&r->headers_out.headers));
        if (expires_header != nullptr) {
            expires_header->hash = 1;
            expires_header->key.data = (u_char*)"Expires";
            expires_header->key.len = sizeof("Expires") - 1;

            // 格式化时间
            ngx_tm_t tm;
#if _WIN32
            // 转换时间到tm结构体
            struct tm winTm;
            gmtime_s(&winTm, &expires);
            
            // 复制到nginx的tm结构体
            tm.ngx_tm_sec = winTm.tm_sec;
            tm.ngx_tm_min = winTm.tm_min;
            tm.ngx_tm_hour = winTm.tm_hour;
            tm.ngx_tm_mday = winTm.tm_mday;
            tm.ngx_tm_mon = winTm.tm_mon;
            tm.ngx_tm_year = winTm.tm_year;
            tm.ngx_tm_wday = winTm.tm_wday;
#else
            ngx_libc_gmtime(expires, &tm);
#endif
            // 调用ngx_http_time处理正确类型
            expires_header->value.len = ngx_http_time(timeStr, expires) - timeStr;
            expires_header->value.data = static_cast<u_char*>(
                ngx_pnalloc(r->pool, expires_header->value.len));
            
            if (expires_header->value.data != nullptr) {
                ngx_memcpy(expires_header->value.data, timeStr, expires_header->value.len);
            }
        }

        // 添加Cache-Control头
        ngx_table_elt_t* cache_control = static_cast<ngx_table_elt_t*>(
            ngx_list_push(&r->headers_out.headers));
        if (cache_control != nullptr) {
            cache_control->hash = 1;
            cache_control->key.data = (u_char*)"Cache-Control";
            cache_control->key.len = sizeof("Cache-Control") - 1;

            std::string cacheValue = "max-age=" + std::to_string(conf->cache_time);
            cache_control->value.len = cacheValue.length();
            cache_control->value.data = static_cast<u_char*>(
                ngx_pnalloc(r->pool, cache_control->value.len));
            
            if (cache_control->value.data != nullptr) {
                ngx_memcpy(cache_control->value.data, cacheValue.c_str(), cache_control->value.len);
            }
        }
    } else {
        // 无缓存设置
        ngx_table_elt_t* cache_control = static_cast<ngx_table_elt_t*>(
            ngx_list_push(&r->headers_out.headers));
        if (cache_control != nullptr) {
            cache_control->hash = 1;
            cache_control->key.data = (u_char*)"Cache-Control";
            cache_control->key.len = sizeof("Cache-Control") - 1;
            cache_control->value.data = (u_char*)"no-cache, no-store, must-revalidate";
            cache_control->value.len = sizeof("no-cache, no-store, must-revalidate") - 1;
        }
    }

    // 分配响应缓冲区
    ngx_str_t response;
    response.len = processedContent.length();
    response.data = static_cast<u_char*>(ngx_pnalloc(r->pool, response.len));
    if (response.data == nullptr) {
        logger.error("分配响应内存失败");
        return NGX_HTTP_INTERNAL_SERVER_ERROR;
    }

    // 复制响应内容
    ngx_memcpy(response.data, processedContent.c_str(), response.len);

    // 设置响应长度
    r->headers_out.content_length_n = response.len;

    // 发送HTTP头
    ngx_int_t rc = ngx_http_send_header(r);
    if (rc == NGX_ERROR || rc > NGX_OK || r->header_only) {
        return rc;
    }

    // 创建并初始化缓冲链
    ngx_buf_t* b = static_cast<ngx_buf_t*>(ngx_pcalloc(r->pool, sizeof(ngx_buf_t)));
    if (b == nullptr) {
        logger.error("分配缓冲区失败");
        return NGX_HTTP_INTERNAL_SERVER_ERROR;
    }

    b->pos = response.data;
    b->last = response.data + response.len;
    b->memory = 1;
    b->last_buf = 1;

    ngx_chain_t out;
    out.buf = b;
    out.next = nullptr;

    // 发送响应体
    return ngx_http_output_filter(r, &out);
}

// 增强的模板处理函数
std::string BlogModule::processTemplate(
    const std::string& content, 
    const std::unordered_map<std::string, std::string>& variables) {
    
    std::string result = content;
    
    // 1. 处理条件段落: {{#section}}...{{/section}}
    for (const auto& var : variables) {
        if (var.first[0] == '#') {  // 找到以#开头的特殊变量
            std::string sectionName = var.first.substr(1);  // 去掉#
            std::string startTag = "{{#" + sectionName + "}}";
            std::string endTag = "{{/" + sectionName + "}}";
            
            size_t startPos = result.find(startTag);
            while (startPos != std::string::npos) {
                size_t endPos = result.find(endTag, startPos);
                if (endPos != std::string::npos) {
                    // 获取段落内容
                    size_t contentStart = startPos + startTag.length();
                    std::string sectionContent = result.substr(contentStart, endPos - contentStart);
                    
                    // 用变量值替换整个段落（包括标签）
                    result.replace(startPos, endPos + endTag.length() - startPos, var.second);
                    
                    // 查找下一个匹配
                    startPos = result.find(startTag, startPos + var.second.length());
                } else {
                    break;  // 找不到结束标签
                }
            }
        }
        else if (var.first[0] == '^') {  // 处理反向条件段落 {{^section}}...{{/section}}
            std::string sectionName = var.first.substr(1);  // 去掉^
            std::string startTag = "{{^" + sectionName + "}}";
            std::string endTag = "{{/" + sectionName + "}}";
            
            size_t startPos = result.find(startTag);
            while (startPos != std::string::npos) {
                size_t endPos = result.find(endTag, startPos);
                if (endPos != std::string::npos) {
                    // 获取段落内容
                    size_t contentStart = startPos + startTag.length();
                    std::string sectionContent = result.substr(contentStart, endPos - contentStart);
                    
                    // 用变量值替换整个段落（包括标签）
                    result.replace(startPos, endPos + endTag.length() - startPos, var.second);
                    
                    // 查找下一个匹配
                    startPos = result.find(startTag, startPos + var.second.length());
                } else {
                    break;  // 找不到结束标签
                }
            }
        }
    }
    
    // 2. 处理普通变量替换 {{var}}
    for (const auto& var : variables) {
        // 跳过特殊变量（已在上面处理）
        if (var.first[0] == '#' || var.first[0] == '^') {
            continue;
        }
        
        std::string placeholder = "{{" + var.first + "}}";
        size_t pos = result.find(placeholder);
        while (pos != std::string::npos) {
            result.replace(pos, placeholder.length(), var.second);
            pos = result.find(placeholder, pos + var.second.length());
        }
    }
    
    return result;
}

// 在命名空间外部定义全局可见的模块结构
extern "C" {
    ngx_module_t ngx_http_blog_module = {
        NGX_MODULE_V1,
        &blog_module_ctx,     /* module context */
        blog_commands,        /* module directives */
        NGX_HTTP_MODULE,            /* module type */
        NULL,                       /* init master */
        NULL,                       /* init module */
        NULL,                       /* init process */
        NULL,                       /* init thread */
        NULL,                       /* exit thread */
        NULL,                       /* exit process */
        NULL,                       /* exit master */
        NGX_MODULE_V1_PADDING
    };
}

// MODULE_API中的函数定义

// 初始化函数
ngx_int_t BlogModule::init(ngx_cycle_t* cycle) {
    try {
        // 设置日志
        NgxLog logger(cycle->log);
        logger.info("初始化博客模块...");
        
        // 模板引擎将在需要时自动初始化
        
        // 初始化博客配置默认值
        BlogModule::basePath = "/blog";
        BlogModule::version = "1.0.0";
        BlogModule::dataDir = "/var/www/blog/data";
        
        return NGX_OK;
    }
    catch (const std::exception& e) {
        ngx_log_error(NGX_LOG_ERR, cycle->log, 0, "初始化博客模块失败: %s", e.what());
        return NGX_ERROR;
    }
}

// 请求处理回调函数
namespace {
    void initBlogRouter(const BlogConfig& config) {
        // 获取博客路由器实例
        auto& router = getBlogRouter();
        
        // 使用新的initBlogRoutes函数，传入路由器实例
        initBlogRoutes(&router);
    }
    
    // 处理静态文件请求
    ngx_int_t handleStaticRequest(ngx_http_request_t* r) {
        NgxRequest request(r);
        NgxLog logger(r);
        
        std::string uri = request.getUri();
        logger.info("Handling static file request: %s", uri.c_str());
        
        // 如果是API请求，直接返回DECLINED让API路由处理
        if (uri.find("/api/") == 0) {
            logger.debug("API request, not handled by static file handler: %s", uri.c_str());
            return NGX_DECLINED;
        }
        
        // 构建前端页面目录的基础路径
        std::string frontendBasePath = "C:/Users/wuxianggujun/CodeSpace/CMakeProjects/TinaBlog/html";
        
        // 处理常见静态资源类型
        if (uri.find("/static/") == 0 || 
            uri.find(".css") != std::string::npos || 
            uri.find(".js") != std::string::npos || 
            uri.find(".png") != std::string::npos || 
            uri.find(".jpg") != std::string::npos || 
            uri.find(".jpeg") != std::string::npos || 
            uri.find(".gif") != std::string::npos || 
            uri.find(".svg") != std::string::npos || 
            uri.find(".ico") != std::string::npos ||
            uri.find(".woff") != std::string::npos ||
            uri.find(".woff2") != std::string::npos ||
            uri.find(".ttf") != std::string::npos ||
            uri.find(".eot") != std::string::npos ||
            uri.find(".json") != std::string::npos) {
            
            // 构建完整文件路径
            std::string fullPath = frontendBasePath + uri;
            logger.info("Serving static resource file: %s", fullPath.c_str());
            return serveStaticFile(r, fullPath);
        }
        
        // 对于所有其他请求，提供index.html（SPA应用的入口点）
        // 这样前端路由可以接管导航
        std::string indexPath = frontendBasePath + "/index.html";
        logger.info("Serving SPA entry page: %s (actual file: %s)", uri.c_str(), indexPath.c_str());
        return serveStaticFile(r, indexPath);
    }
    
    // 提供静态文件
    ngx_int_t serveStaticFile(ngx_http_request_t* r, const std::string& path) {
        NgxLog logger(r);
        
        // 打开文件
        std::ifstream file(path, std::ios::binary | std::ios::ate);
        if (!file.is_open()) {
            logger.error("Failed to open file: %s", path.c_str());
            return NGX_HTTP_NOT_FOUND;
        }
        
        // 获取文件大小
        std::streamsize size = file.tellg();
        file.seekg(0, std::ios::beg);
        
        // 设置内容类型
        std::string contentType = "text/html";
        if (path.find(".css") != std::string::npos) {
            contentType = "text/css";
        } else if (path.find(".js") != std::string::npos) {
            contentType = "application/javascript";
        } else if (path.find(".json") != std::string::npos) {
            contentType = "application/json";
        } else if (path.find(".png") != std::string::npos) {
            contentType = "image/png";
        } else if (path.find(".jpg") != std::string::npos || path.find(".jpeg") != std::string::npos) {
            contentType = "image/jpeg";
        } else if (path.find(".gif") != std::string::npos) {
            contentType = "image/gif";
        } else if (path.find(".svg") != std::string::npos) {
            contentType = "image/svg+xml";
        } else if (path.find(".ico") != std::string::npos) {
            contentType = "image/x-icon";
        } else if (path.find(".woff") != std::string::npos) {
            contentType = "font/woff";
        } else if (path.find(".woff2") != std::string::npos) {
            contentType = "font/woff2";
        } else if (path.find(".ttf") != std::string::npos) {
            contentType = "font/ttf";
        } else if (path.find(".eot") != std::string::npos) {
            contentType = "application/vnd.ms-fontobject";
        }
        
        // 设置响应头
        r->headers_out.status = NGX_HTTP_OK;
        r->headers_out.content_length_n = size;
        
        ngx_str_t content_type;
        content_type.data = (u_char*)contentType.c_str();
        content_type.len = contentType.length();
        r->headers_out.content_type = content_type;
        
        // 发送HTTP头
        ngx_int_t rc = ngx_http_send_header(r);
        if (rc == NGX_ERROR || rc > NGX_OK || r->header_only) {
            logger.error("Failed to send HTTP header");
            return rc;
        }
        
        // 读取文件内容并发送
        std::vector<char> buffer(size);
        if (file.read(buffer.data(), size)) {
            // 分配内存
            ngx_buf_t *b = ngx_create_temp_buf(r->pool, size);
            if (b == NULL) {
                logger.error("Failed to allocate buffer");
                return NGX_HTTP_INTERNAL_SERVER_ERROR;
            }
            
            // 复制数据
            ngx_memcpy(b->pos, buffer.data(), size);
            b->last = b->pos + size;
            b->memory = 1;    // 这是一个内存缓冲区
            b->last_buf = 1;  // 这是最后一个缓冲区
            
            // 创建输出链
            ngx_chain_t out;
            out.buf = b;
            out.next = nullptr;
            
            return ngx_http_output_filter(r, &out);
        } else {
            logger.error("Failed to read file content: %s", path.c_str());
            return NGX_HTTP_INTERNAL_SERVER_ERROR;
        }
    }
    
    // 处理博客首页
    ngx_int_t handleBlogIndex(NgxResponse& response, const RouteParams& params) {
        try {
            // 获取请求对象和日志
            ngx_http_request_t* r = response.get();
            NgxLog logger(r);
            logger.info("处理博客首页API请求");
            
            // 创建配置对象
            NgxRequest request(r);
            BlogConfig config(request);
            
            // 从数据库中获取文章列表
            BlogPostDao dao;
            std::vector<BlogPostRecord> posts = dao.getAllPosts(10); // 获取前10篇文章
            
            // 构建文章数据JSON
            json postsJson = json::array();
            for (const auto& post : posts) {
                json postJson;
                postJson["id"] = post.id;
                postJson["title"] = post.title;
                postJson["author"] = post.author;
                postJson["summary"] = post.summary;
                postJson["created_at"] = post.created_at;
                postJson["updated_at"] = post.updated_at;
                postJson["view_count"] = post.view_count;
                postJson["categories"] = post.categories;
                postJson["tags"] = post.tags;
                
                postsJson.push_back(postJson);
            }
            
            // 构建完整JSON响应
            json data;
            data["posts"] = postsJson;
            json successResponse = JsonResponse::success(data);
            
            // 使用NgxResponse返回JSON响应
            return response
                .status(NGX_HTTP_OK)
                .contentType("application/json")
                .enableCors()
                .send(successResponse.dump(2));
        }
        catch (const std::exception& e) {
            NgxLog logger(response.get()->connection->log);
            logger.error("处理博客首页API异常: %s", e.what());
            
            // 使用NgxResponse返回错误JSON响应
            json errorResponse = JsonResponse::error(e.what(), 500);
            return response
                .status(NGX_HTTP_INTERNAL_SERVER_ERROR)
                .contentType("application/json")
                .enableCors()
                .send(errorResponse.dump(2));
        }
    }

    // 处理博客文章详情页
    ngx_int_t handleBlogPost(NgxResponse& response, const RouteParams& params) {
        try {
            // 获取请求对象和日志
            ngx_http_request_t* r = response.get();
            NgxLog logger(r);
            logger.info("处理博客文章详情API请求");

            // 从路由参数中获取文章ID
            std::string postIdParam = params.at("id");
            int postId = std::stoi(postIdParam);

            // 创建配置对象
            NgxRequest request(r);
            BlogConfig config(request);
            
            // 从数据库获取文章
            BlogPostDao dao;
            auto post = dao.getPostById(postId);
            
            // 判断文章是否存在
            if (post.has_value()) {
                // 更新文章浏览量
                dao.incrementViewCount(postId);
                
                // 构建文章数据
                json postData;
                postData["id"] = post->id;
                postData["title"] = post->title;
                postData["content"] = post->content;
                postData["summary"] = post->summary;
                postData["author"] = post->author;
                postData["created_at"] = post->created_at;
                postData["updated_at"] = post->updated_at;
                postData["view_count"] = post->view_count + 1; // 包括当前访问
                postData["categories"] = post->categories;
                postData["tags"] = post->tags;
                
                // 返回成功响应
                json successResponse = JsonResponse::success(postData);
                return response
                    .status(NGX_HTTP_OK)
                    .contentType("application/json")
                    .enableCors()
                    .send(successResponse.dump(2));
            } else {
                // 文章不存在
                logger.warn("Post with ID %d not found", postId);
                
                // 返回404错误
                json errorResponse = JsonResponse::error("文章不存在", 404);
                return response
                    .status(NGX_HTTP_NOT_FOUND)
                    .contentType("application/json")
                    .enableCors()
                    .send(errorResponse.dump(2));
            }
        }
        catch (const std::exception& e) {
            NgxLog logger(response.get()->connection->log);
            logger.error("处理博客文章详情API异常: %s", e.what());
            
            // 返回服务器错误
            json errorResponse = JsonResponse::error(e.what(), 500);
            return response
                .status(NGX_HTTP_INTERNAL_SERVER_ERROR)
                .contentType("application/json")
                .enableCors()
                .send(errorResponse.dump(2));
        }
    }

    // 处理博客分类页面
    ngx_int_t handleBlogCategory(NgxResponse& response, const RouteParams& params) {
        try {
            // 获取请求对象和日志
            ngx_http_request_t* r = response.get();
            NgxLog logger(r);
            
            // 获取分类参数
            auto it = params.find("category");
            if (it == params.end()) {
                logger.error("缺少分类参数");
                json errorResponse = JsonResponse::error("缺少分类参数", 400);
                return response
                    .status(NGX_HTTP_BAD_REQUEST)
                    .contentType("application/json")
                    .enableCors()
                    .send(errorResponse.dump(2));
            }
            
            std::string category = it->second;
            logger.info("处理分类API请求: %s", category.c_str());
            
            // 创建配置对象
            NgxRequest request(r);
            BlogConfig config(request);
            
            // 从数据库获取该分类的文章
            BlogPostDao dao;
            auto posts = dao.getPostsByCategory(category, 10); // 获取前10篇文章
            
            // 构建文章列表JSON
            json postsArray = json::array();
            for (const auto& post : posts) {
                json postJson;
                postJson["id"] = post.id;
                postJson["title"] = post.title;
                postJson["author"] = post.author;
                postJson["summary"] = post.summary;
                postJson["created_at"] = post.created_at;
                postJson["updated_at"] = post.updated_at;
                postJson["view_count"] = post.view_count;
                postJson["categories"] = post.categories;
                postJson["tags"] = post.tags;
                
                postsArray.push_back(postJson);
            }
            
            // 构建响应数据
            json responseData;
            responseData["category"] = category;
            responseData["posts"] = postsArray;
            
            // 返回成功响应
            json successResponse = JsonResponse::success(responseData);
            return response
                .status(NGX_HTTP_OK)
                .contentType("application/json")
                .enableCors()
                .send(successResponse.dump(2));
        } 
        catch (const std::exception& e) {
            NgxLog logger(response.get()->connection->log);
            logger.error("处理分类API请求异常: %s", e.what());
            
            // 返回服务器错误
            json errorResponse = JsonResponse::error(e.what(), 500);
            return response
                .status(NGX_HTTP_INTERNAL_SERVER_ERROR)
                .contentType("application/json")
                .enableCors()
                .send(errorResponse.dump(2));
        }
    }

    // 处理博客标签页面
    ngx_int_t handleBlogTag(NgxResponse& response, const RouteParams& params) {
        try {
            // 获取请求对象和日志
            ngx_http_request_t* r = response.get();
            NgxLog logger(r);
            
            // 获取标签参数
            auto it = params.find("tag");
            if (it == params.end()) {
                logger.error("缺少标签参数");
                
                json errorResponse = JsonResponse::error("缺少标签参数", 400);
                return response
                    .status(NGX_HTTP_BAD_REQUEST)
                    .contentType("application/json")
                    .enableCors()
                    .send(errorResponse.dump(2));
            }
            
            std::string tag = it->second;
            logger.info("处理标签API请求: %s", tag.c_str());
            
            // 创建配置对象
            NgxRequest request(r);
            BlogConfig config(request);
            
            // 从数据库获取该标签的文章
            BlogPostDao dao;
            auto posts = dao.getPostsByTag(tag, 10); // 获取前10篇文章
            
            // 构建响应JSON
            json data;
            data["tag"] = tag;
            data["posts"] = json::array();
            
            for (const auto& post : posts) {
                json postJson;
                postJson["id"] = post.id;
                postJson["title"] = post.title;
                postJson["author"] = post.author;
                postJson["summary"] = post.summary;
                postJson["created_at"] = post.created_at;
                postJson["updated_at"] = post.updated_at;
                postJson["view_count"] = post.view_count;
                postJson["categories"] = post.categories;
                postJson["tags"] = post.tags;
                
                // 将文章添加到列表
                data["posts"].push_back(postJson);
            }
            
            // 返回成功响应
            json successResponse = JsonResponse::success(data);
            return response
                .status(NGX_HTTP_OK)
                .contentType("application/json")
                .enableCors()
                .send(successResponse.dump(2));
        } 
        catch (const std::exception& e) {
            NgxLog logger(response.get()->connection->log);
            logger.error("处理标签API请求异常: %s", e.what());
            
            // 返回错误响应
            json errorResponse = JsonResponse::error(e.what(), 500);
            return response
                .status(NGX_HTTP_INTERNAL_SERVER_ERROR)
                .contentType("application/json")
                .enableCors()
                .send(errorResponse.dump(2));
        }
    }

    // 处理博客管理页面
    ngx_int_t handleAdmin(NgxResponse& response, const RouteParams& params) {
        try {
            // 获取请求对象和日志
            ngx_http_request_t* r = response.get();
            NgxLog logger(r);
            logger.info("处理博客管理API请求");
            
            // 创建配置对象
            NgxRequest request(r);
            BlogConfig config(request);
            
            // 从数据库获取所有文章
            BlogPostDao dao;
            auto posts = dao.getAllPosts();
            
            // 构建响应JSON
            json postsArray = json::array();
            for (const auto& post : posts) {
                json postJson;
                postJson["id"] = post.id;
                postJson["title"] = post.title;
                postJson["author"] = post.author;
                postJson["summary"] = post.summary;
                postJson["created_at"] = post.created_at;
                postJson["updated_at"] = post.updated_at;
                postJson["view_count"] = post.view_count;
                postJson["categories"] = post.categories;
                postJson["tags"] = post.tags;
                
                postsArray.push_back(postJson);
            }
            
            // 返回成功响应
            json data;
            data["posts"] = postsArray;
            json successResponse = JsonResponse::success(data);
            
            return response
                .status(NGX_HTTP_OK)
                .contentType("application/json")
                .enableCors()
                .send(successResponse.dump(2));
        }
        catch (const std::exception& e) {
            NgxLog logger(response.get()->connection->log);
            logger.error("处理博客管理API异常: %s", e.what());
            
            // 返回错误响应
            json errorResponse = JsonResponse::error(e.what(), 500);
            return response
                .status(NGX_HTTP_INTERNAL_SERVER_ERROR)
                .contentType("application/json")
                .enableCors()
                .send(errorResponse.dump(2));
        }
    }

    // 处理添加文章请求
    ngx_int_t handleAddPost(NgxResponse& response, const RouteParams& params) {
        try {
            // 获取请求对象和日志
            ngx_http_request_t* r = response.get();
            NgxLog logger(r);
            logger.info("处理添加文章API请求");
            
            // 创建配置对象
            NgxRequest request(r);
            BlogConfig config(request);
            
            // 解析请求体中的JSON数据
            std::string requestBody = request.getRequestBody();
            if (requestBody.empty()) {
                // 如果请求体为空，返回错误
                json errorResponse = JsonResponse::error("请求体不能为空", 400);
                return response
                    .status(NGX_HTTP_BAD_REQUEST)
                    .contentType("application/json")
                    .enableCors()
                    .send(errorResponse.dump(2));
            }
            
            // 解析JSON
            json postData;
            try {
                postData = json::parse(requestBody);
            } catch (const json::parse_error& e) {
                // JSON解析错误
                json errorResponse = JsonResponse::error("JSON格式错误: " + std::string(e.what()), 400);
                return response
                    .status(NGX_HTTP_BAD_REQUEST)
                    .contentType("application/json")
                    .enableCors()
                    .send(errorResponse.dump(2));
            }
            
            // 提取字段
            if (!postData.contains("title") || !postData.contains("content")) {
                // 标题和内容是必需的
                json errorResponse = JsonResponse::error("标题和内容不能为空", 400);
                return response
                    .status(NGX_HTTP_BAD_REQUEST)
                    .contentType("application/json")
                    .enableCors()
                    .send(errorResponse.dump(2));
            }
            
            std::string title = postData["title"];
            std::string content = postData["content"];
            std::string summary = postData.contains("summary") ? postData["summary"] : "";
            bool isPublished = postData.contains("is_published") ? postData["is_published"].get<bool>() : true;
            
            // 获取分类和标签
            std::vector<std::string> categories;
            if (postData.contains("categories") && postData["categories"].is_array()) {
                for (const auto& category : postData["categories"]) {
                    categories.push_back(category);
                }
            }
            
            std::vector<std::string> tags;
            if (postData.contains("tags") && postData["tags"].is_array()) {
                for (const auto& tag : postData["tags"]) {
                    tags.push_back(tag);
                }
            }
            
            // 使用DAO保存文章到数据库
            BlogPostDao dao;
            int postId = dao.createPost(title, content, summary, categories, tags, isPublished);
            
            if (postId > 0) {
                logger.info("文章创建成功，ID: %d", postId);
                
                // 获取新创建的文章
                auto post = dao.getPostById(postId);
                if (post.has_value()) {
                    // 构建响应数据
                    json responseData;
                    responseData["id"] = post->id;
                    responseData["title"] = post->title;
                    responseData["content"] = post->content;
                    responseData["summary"] = post->summary;
                    responseData["author"] = post->author;
                    responseData["categories"] = post->categories;
                    responseData["tags"] = post->tags;
                    responseData["created_at"] = post->created_at;
                    responseData["updated_at"] = post->updated_at;
                    responseData["is_published"] = post->published;
                    
                    // 返回成功响应
                    json successResponse = JsonResponse::success(responseData);
                    return response
                        .status(NGX_HTTP_CREATED)
                        .contentType("application/json")
                        .enableCors()
                        .send(successResponse.dump(2));
                }
            }
            
            // 如果到这里，说明创建失败
            logger.error("创建文章失败");
            json errorResponse = JsonResponse::error("创建文章失败", 500);
            return response
                .status(NGX_HTTP_INTERNAL_SERVER_ERROR)
                .contentType("application/json")
                .enableCors()
                .send(errorResponse.dump(2));
        }
        catch (const std::exception& e) {
            NgxLog logger(response.get()->connection->log);
            logger.error("处理添加文章请求异常: %s", e.what());
            
            // 返回错误响应
            json errorResponse = JsonResponse::error(e.what(), 500);
            return response
                .status(NGX_HTTP_INTERNAL_SERVER_ERROR)
                .contentType("application/json")
                .enableCors()
                .send(errorResponse.dump(2));
        }
    }

    // 处理文章编辑页面
    ngx_int_t handleEditPost(NgxResponse& response, const RouteParams& params) {
        try {
            // 获取请求对象和日志
            ngx_http_request_t* r = response.get();
            NgxLog logger(r);
            logger.info("处理编辑文章API请求");
            
            // 检查是否有文章ID
            if (params.find("id") == params.end()) {
                json errorResponse = JsonResponse::error("缺少文章ID参数", 400);
                return response
                    .status(NGX_HTTP_BAD_REQUEST)
                    .contentType("application/json")
                    .enableCors()
                    .send(errorResponse.dump(2));
            }
            
            // 获取文章ID
            int postId = std::stoi(params.at("id"));
            
            // 创建配置对象
            NgxRequest request(r);
            BlogConfig config(request);
            
            // 解析请求体中的JSON数据
            std::string requestBody = request.getRequestBody();
            if (requestBody.empty()) {
                // 如果请求体为空，返回错误
                json errorResponse = JsonResponse::error("请求体不能为空", 400);
                return response
                    .status(NGX_HTTP_BAD_REQUEST)
                    .contentType("application/json")
                    .enableCors()
                    .send(errorResponse.dump(2));
            }
            
            // 解析JSON
            json postData;
            try {
                postData = json::parse(requestBody);
            } catch (const json::parse_error& e) {
                // JSON解析错误
                json errorResponse = JsonResponse::error("JSON格式错误: " + std::string(e.what()), 400);
                return response
                    .status(NGX_HTTP_BAD_REQUEST)
                    .contentType("application/json")
                    .enableCors()
                    .send(errorResponse.dump(2));
            }
            
            // 使用DAO更新文章
            BlogPostDao dao;
            
            // 首先检查文章是否存在
            auto post = dao.getPostById(postId);
            if (!post.has_value()) {
                // 文章不存在
                json errorResponse = JsonResponse::error("文章不存在", 404);
                return response
                    .status(NGX_HTTP_NOT_FOUND)
                    .contentType("application/json")
                    .enableCors()
                    .send(errorResponse.dump(2));
            }
            
            // TODO: 实现文章更新逻辑
            // 现在简单返回未实现
            json errorResponse = JsonResponse::error("文章编辑功能尚未实现", 501);
            return response
                .status(NGX_HTTP_NOT_IMPLEMENTED)
                .contentType("application/json")
                .enableCors()
                .send(errorResponse.dump(2));
        }
        catch (const std::exception& e) {
            NgxLog logger(response.get()->connection->log);
            logger.error("处理编辑文章请求异常: %s", e.what());
            
            // 返回错误响应
            json errorResponse = JsonResponse::error(e.what(), 500);
            return response
                .status(NGX_HTTP_INTERNAL_SERVER_ERROR)
                .contentType("application/json")
                .enableCors()
                .send(errorResponse.dump(2));
        }
    }

    // 处理文章删除功能
    ngx_int_t handleDeletePost(NgxResponse& response, const RouteParams& params) {
        try {
            // 获取请求对象和日志
            ngx_http_request_t* r = response.get();
            NgxLog logger(r);
            logger.info("处理删除文章API请求");
            
            // 检查是否有文章ID
            if (params.find("id") == params.end()) {
                json errorResponse = JsonResponse::error("缺少文章ID参数", 400);
                return response
                    .status(NGX_HTTP_BAD_REQUEST)
                    .contentType("application/json")
                    .enableCors()
                    .send(errorResponse.dump(2));
            }
            
            // 获取文章ID
            int postId = std::stoi(params.at("id"));
            
            // 创建配置对象
            NgxRequest request(r);
            BlogConfig config(request);
            
            // 使用DAO删除文章
            BlogPostDao dao;
            
            // 首先检查文章是否存在
            auto post = dao.getPostById(postId);
            if (!post.has_value()) {
                // 文章不存在
                json errorResponse = JsonResponse::error("文章不存在", 404);
                return response
                    .status(NGX_HTTP_NOT_FOUND)
                    .contentType("application/json")
                    .enableCors()
                    .send(errorResponse.dump(2));
            }
            
            // TODO: 实现文章删除逻辑
            // 现在简单返回未实现
            json errorResponse = JsonResponse::error("文章删除功能尚未实现", 501);
            return response
                .status(NGX_HTTP_NOT_IMPLEMENTED)
                .contentType("application/json")
                .enableCors()
                .send(errorResponse.dump(2));
        }
        catch (const std::exception& e) {
            NgxLog logger(response.get()->connection->log);
            logger.error("处理删除文章请求异常: %s", e.what());
            
            // 返回错误响应
            json errorResponse = JsonResponse::error(e.what(), 500);
            return response
                .status(NGX_HTTP_INTERNAL_SERVER_ERROR)
                .contentType("application/json")
                .enableCors()
                .send(errorResponse.dump(2));
        }
    }

    // 处理博客首页重定向
    ngx_int_t handleBlogRedirect(NgxResponse& response, const RouteParams& params) {
        NgxLog logger(response.get());
        logger.info("从/blog/重定向到/");
        
        // 直接使用传入的response对象进行重定向
        return response
            .status(NGX_HTTP_MOVED_PERMANENTLY)
            .redirect("/", NGX_HTTP_MOVED_PERMANENTLY);
    }
    
    // 处理管理面板统计数据API请求
    ngx_int_t handleAdminStats(NgxResponse& response, const RouteParams& params) {
        try {
            // 获取请求对象和日志
            ngx_http_request_t* r = response.get();
            NgxLog logger(r);
            logger.info("处理管理面板统计数据API请求");
            
            // 创建配置对象
            NgxRequest request(r);
            BlogConfig config(request);
            
            // 从数据库获取统计数据
            BlogPostDao dao;
            
            // 获取总文章数
            int totalPosts = dao.getPostCount();
            
            // 获取已发布文章数
            int publishedPosts = dao.getPublishedPostCount();
            
            // 获取总浏览量
            int totalViews = dao.getTotalViewCount();
            
            // 获取分类数量
            int categoriesCount = dao.getCategoryCount();
            
            // 获取标签数量
            int tagsCount = dao.getTagCount();
            
            // 获取最近文章
            std::vector<BlogPostRecord> recentPosts = dao.getAllPosts(5); // 获取5篇最新文章
            
            // 构建响应数据
            json statsData;
            statsData["total_posts"] = totalPosts;
            statsData["published_posts"] = publishedPosts;
            statsData["total_views"] = totalViews;
            statsData["categories_count"] = categoriesCount;
            statsData["tags_count"] = tagsCount;
            
            // 构建最近文章数据
            json recentPostsArray = json::array();
            for (const auto& post : recentPosts) {
                json postJson;
                postJson["id"] = post.id;
                postJson["title"] = post.title;
                postJson["author"] = post.author;
                postJson["created_at"] = post.created_at;
                postJson["view_count"] = post.view_count;
                
                recentPostsArray.push_back(postJson);
            }
            
            // 构建完整响应数据
            json responseData;
            responseData["stats"] = statsData;
            responseData["recent_posts"] = recentPostsArray;
            
            // 使用封装的NgxResponse返回JSON响应
            json successResponse = JsonResponse::success(responseData);
            std::string jsonContent = successResponse.dump(2);
            
            return response
                .status(NGX_HTTP_OK)
                .contentType("application/json")
                .enableCors()
                .send(jsonContent);
        }
        catch (const std::exception& e) {
            NgxLog logger(response.get()->connection->log);
            logger.error("处理管理面板统计数据请求异常: %s", e.what());
            
            // 构建错误响应JSON
            json errorResponse = JsonResponse::error(e.what(), 500);
            std::string jsonContent = errorResponse.dump(2);
            
            // 使用封装的NgxResponse返回错误JSON响应
            return response
                .status(NGX_HTTP_INTERNAL_SERVER_ERROR)
                .contentType("application/json")
                .enableCors()
                .send(jsonContent);
        }
    }
    
    // 处理获取文章编辑数据的请求
    ngx_int_t handleGetPostForEdit(NgxResponse& response, const RouteParams& params) {
        try {
            // 获取请求对象和日志
            ngx_http_request_t* r = response.get();
            NgxLog logger(r);
            logger.info("处理获取文章编辑数据请求");
            
            // 检查postId参数
            if (params.find("id") == params.end()) {
                // 使用NgxResponse返回错误JSON响应
                json errorResponse = JsonResponse::error("缺少文章ID参数", 400);
                return response
                    .status(NGX_HTTP_BAD_REQUEST)
                    .contentType("application/json")
                    .enableCors()
                    .send(errorResponse.dump(2));
            }
            
            // 获取文章ID
            int postId = std::stoi(params.at("id"));
            
            // 创建配置对象
            NgxRequest request(r);
            BlogConfig config(request);
            
            // 从数据库获取文章
            BlogPostDao dao;
            auto post = dao.getPostById(postId);
            
            if (!post) {
                // 使用NgxResponse返回错误JSON响应
                json errorResponse = JsonResponse::error("文章不存在", 404);
                return response
                    .status(NGX_HTTP_NOT_FOUND)
                    .contentType("application/json")
                    .enableCors()
                    .send(errorResponse.dump(2));
            }
            
            // 构建响应数据
            json postData;
            postData["id"] = post->id;
            postData["title"] = post->title;
            postData["content"] = post->content;
            postData["author"] = post->author;
            postData["categories"] = post->categories;
            postData["tags"] = post->tags;
            postData["created_at"] = post->created_at;
            postData["updated_at"] = post->updated_at;
            postData["is_published"] = post->published;
            
            // 使用NgxResponse返回JSON响应
            json successResponse = JsonResponse::success(postData);
            return response
                .status(NGX_HTTP_OK)
                .contentType("application/json")
                .enableCors()
                .send(successResponse.dump(2));
        }
        catch (const std::exception& e) {
            NgxLog logger(response.get()->connection->log);
            logger.error("处理获取文章编辑数据请求异常: %s", e.what());
            
            // 使用NgxResponse返回错误JSON响应
            json errorResponse = JsonResponse::error(e.what(), 500);
            return response
                .status(NGX_HTTP_INTERNAL_SERVER_ERROR)
                .contentType("application/json")
                .enableCors()
                .send(errorResponse.dump(2));
        }
    }
    
    // 处理OPTIONS请求，支持CORS预检请求
    ngx_int_t handleOptionsRequest(NgxResponse& response, const RouteParams& params) {
        NgxLog logger(response.get());
        logger.info("处理OPTIONS请求: %s", ((NgxRequest(response.get())).getUri()).c_str());
        
        // 使用NgxResponse处理CORS预检请求
        return response
            .status(NGX_HTTP_OK)
            .enableCors()
            .sendEmpty();
    }
}

// 初始化博客路由
static void initBlogRoutes(const std::string& basePath, Router& router) {
    // 此函数已废弃，使用下面的新版initBlogRoutes(BlogRouter*)替代
    NgxLog logger(ngx_cycle->log);
    logger.warn("Using deprecated initBlogRoutes function, please use the new implementation");
    
    // 尝试将Router转换为BlogRouter
    // 由于Router现在是多态类，可以使用dynamic_cast
    BlogRouter* blogRouter = dynamic_cast<BlogRouter*>(&router);
    if (blogRouter) {
        initBlogRoutes(blogRouter);
    } else {
        logger.error("Cannot convert Router to BlogRouter, please use BlogRouter type");
    }
}

// 初始化博客路由
void initBlogRoutes(BlogRouter* router) {
    if (!router) {
        return;
    }

    // 清除现有路由并设置新的API路由
    router->reset();
    
    // 读取配置的基础路径
    NgxLog logger(ngx_cycle->log);
    std::string basePath = "/api"; // 前后端分离模式下使用/api作为基础路径
    logger.info("Initializing blog routes, API base path: %s", basePath.c_str());
    
    // 静态文件服务路由 - 处理所有非API请求
    router->addRoute(Route(HttpMethod::GET_METHOD, "/*", [](ngx_http_request_t* r, const RouteParams& params) -> ngx_int_t {
        return handleStaticRequest(r);
    }));
    
    // API 路由 - 博客文章列表
    router->addRoute(Route(HttpMethod::GET_METHOD, basePath + "/posts", [](ngx_http_request_t* r, const RouteParams& params) -> ngx_int_t {
        NgxResponse response(r);
        return handleBlogIndex(response, params);
    }));
    
    // API 路由 - 获取单篇博客文章
    router->addRoute(Route(HttpMethod::GET_METHOD, basePath + "/posts/:id", [](ngx_http_request_t* r, const RouteParams& params) -> ngx_int_t {
        NgxResponse response(r);
        return handleBlogPost(response, params);
    }));
    
    // API 路由 - 分类文章列表
    router->addRoute(Route(HttpMethod::GET_METHOD, basePath + "/categories/:category", [](ngx_http_request_t* r, const RouteParams& params) -> ngx_int_t {
        NgxResponse response(r);
        return handleBlogCategory(response, params);
    }));
    
    // API 路由 - 标签文章列表
    router->addRoute(Route(HttpMethod::GET_METHOD, basePath + "/tags/:tag", [](ngx_http_request_t* r, const RouteParams& params) -> ngx_int_t {
        NgxResponse response(r);
        return handleBlogTag(response, params);
    }));
    
    // API 路由 - 管理面板
    router->addRoute(Route(HttpMethod::GET_METHOD, basePath + "/admin", [](ngx_http_request_t* r, const RouteParams& params) -> ngx_int_t {
        NgxResponse response(r);
        return handleAdmin(response, params);
    }));
    
    // API 路由 - 获取管理面板统计数据
    router->addRoute(Route(HttpMethod::GET_METHOD, basePath + "/admin/stats", [](ngx_http_request_t* r, const RouteParams& params) -> ngx_int_t {
        NgxResponse response(r);
        return handleAdminStats(response, params);
    }));
    
    // API 路由 - 获取文章编辑数据
    router->addRoute(Route(HttpMethod::GET_METHOD, basePath + "/admin/posts/:id/edit", [](ngx_http_request_t* r, const RouteParams& params) -> ngx_int_t {
        NgxResponse response(r);
        return handleGetPostForEdit(response, params);
    }));
    
    // API 路由 - 添加博客文章
    router->addRoute(Route(HttpMethod::POST_METHOD, basePath + "/admin/posts", [](ngx_http_request_t* r, const RouteParams& params) -> ngx_int_t {
        NgxResponse response(r);
        return handleAddPost(response, params);
    }));
    
    // API 路由 - 编辑博客文章
    router->addRoute(Route(HttpMethod::PUT_METHOD, basePath + "/admin/posts/:id", [](ngx_http_request_t* r, const RouteParams& params) -> ngx_int_t {
        NgxResponse response(r);
        return handleEditPost(response, params);
    }));
    
    // API 路由 - 删除博客文章
    router->addRoute(Route(HttpMethod::DELETE_METHOD, basePath + "/admin/posts/:id", [](ngx_http_request_t* r, const RouteParams& params) -> ngx_int_t {
        NgxResponse response(r);
        return handleDeletePost(response, params);
    }));
    
    // 重定向路由
    router->addRoute(Route(HttpMethod::GET_METHOD, "/blog", [](ngx_http_request_t* r, const RouteParams& params) -> ngx_int_t {
        NgxResponse response(r);
        return handleBlogRedirect(response, params);
    }));
    
    // CORS 预检请求处理
    router->addRoute(Route(HttpMethod::OPTIONS_METHOD, basePath + "/*", [](ngx_http_request_t* r, const RouteParams& params) -> ngx_int_t {
        NgxResponse response(r);
        return handleOptionsRequest(response, params);
    }));
}