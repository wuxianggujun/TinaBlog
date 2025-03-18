//
// Created by wuxianggujun on 2025/3/14.
//

#include "BlogModule.hpp"
#include "NgxConf.hpp"
#include "BlogRouter.hpp"
#include "BlogPostManager.hpp"
#include "db/DbManager.hpp"
#include "NgxLog.hpp"
#include "BlogConfig.hpp"
#include "JsonResponse.hpp"
#include "NgxResponse.hpp"
#include "NgxRequest.hpp"
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


// 声明外部模块变量，这样代码中使用时编译器能识别它
extern "C" {
extern ngx_module_t ngx_http_blog_module;
}

// 初始化静态成员变量
bool BlogModule::isRegistered_ = false;
ngx_module_t* BlogModule::blogModule_ = nullptr;
std::string BlogModule::basePath = "/blog";
std::string BlogModule::version = "1.0.0";
std::string BlogModule::dataDir = "/var/www/blog/data";
std::string BlogModule::staticPath = "/var/www/blog/static";

// 获取静态资源路径
std::string BlogModule::getStaticPath()
{
    return staticPath;
}

// 定义模块名称和版本
constexpr const char* MODULE_NAME = "ngx_http_blog_module";
constexpr const char* MODULE_VERSION = "1.0.0";

// 命令定义 - 作为全局变量
static ngx_command_t blog_commands[] = {
    {
        ngx_string("blog_path"),
        NGX_HTTP_MAIN_CONF | NGX_HTTP_SRV_CONF | NGX_HTTP_LOC_CONF | NGX_CONF_TAKE1,
        BlogModule::setBlogPath,
        NGX_HTTP_LOC_CONF_OFFSET,
        offsetof(BlogModuleConfig, base_path),
        NULL
    },

    {
        ngx_string("blog_enable_cache"),
        NGX_HTTP_MAIN_CONF | NGX_HTTP_SRV_CONF | NGX_HTTP_LOC_CONF | NGX_CONF_FLAG,
        BlogModule::setEnableCache,
        NGX_HTTP_LOC_CONF_OFFSET,
        offsetof(BlogModuleConfig, enable_cache),
        NULL
    },

    {
        ngx_string("blog_cache_time"),
        NGX_HTTP_MAIN_CONF | NGX_HTTP_SRV_CONF | NGX_HTTP_LOC_CONF | NGX_CONF_TAKE1,
        BlogModule::setCacheTime,
        NGX_HTTP_LOC_CONF_OFFSET,
        offsetof(BlogModuleConfig, cache_time),
        NULL
    },

    {
        ngx_string("blog_db_connection"),
        NGX_HTTP_MAIN_CONF | NGX_HTTP_SRV_CONF | NGX_HTTP_LOC_CONF | NGX_CONF_TAKE1,
        BlogModule::setDbConnectionString,
        NGX_HTTP_LOC_CONF_OFFSET,
        offsetof(BlogModuleConfig, db_conn_str),
        NULL
    },

    {
        ngx_string("blog_db_auto_connect"),
        NGX_HTTP_MAIN_CONF | NGX_HTTP_SRV_CONF | NGX_HTTP_LOC_CONF | NGX_CONF_FLAG,
        BlogModule::setDbAutoConnect,
        NGX_HTTP_LOC_CONF_OFFSET,
        offsetof(BlogModuleConfig, db_auto_connect),
        NULL
    },

    ngx_null_command
};

// 模块上下文定义 - 作为全局变量
static ngx_http_module_t blog_module_ctx = {
    BlogModule::preConfiguration, /* preconfiguration */
    BlogModule::postConfiguration, /* postconfiguration */
    BlogModule::createMainConfig, /* create main configuration */
    BlogModule::initMainConfig, /* init main configuration */
    BlogModule::createServerConfig, /* create server configuration */
    BlogModule::mergeServerConfig, /* merge server configuration */
    BlogModule::createLocationConfig, /* create location configuration */
    BlogModule::mergeLocationConfig /* merge location configuration */
};


const char* BlogModule::getModuleName()
{
    return MODULE_NAME;
}

const char* BlogModule::getModuleVersion()
{
    return MODULE_VERSION;
}

// 预配置回调
ngx_int_t BlogModule::preConfiguration(ngx_conf_t* cf)
{
    // 使用NgxConf封装原始指针
    NgxConf conf(cf);

    // 这里可以进行一些预配置工作
    return NGX_OK;
}

// 后配置回调
ngx_int_t BlogModule::postConfiguration(ngx_conf_t* cf)
{
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

    if (h == nullptr)
    {
        ngx_log_error(NGX_LOG_ERR, cf->log, 0, "无法添加内容处理阶段处理器");
        return NGX_ERROR;
    }

    // 设置处理器为BlogModule的请求处理函数
    *h = BlogModule::handleRequest;
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
    if (loc_conf && loc_conf->db_conn_str.len > 0)
    {
        std::string connStr((char*)loc_conf->db_conn_str.data, loc_conf->db_conn_str.len);

        // 使用日志记录进度
        ngx_log_error(NGX_LOG_INFO, cf->log, 0,
                      "初始化数据库连接(位置配置)，连接字符串: %s", connStr.c_str());

        try
        {
            auto& dbManager = DbManager::getInstance();
            bool success = dbManager.initialize(connStr);

            ngx_log_error(NGX_LOG_INFO, cf->log, 0,
                          "数据库初始化%s",
                          success ? "成功" : "失败");
        }
        catch (const std::exception& e)
        {
            ngx_log_error(NGX_LOG_ERR, cf->log, 0,
                          "初始化数据库异常: %s", e.what());
            return NGX_ERROR; // 如果初始化失败，直接返回错误
        }

        db_initialized = true;
    }
    // 如果位置配置没有数据库连接字符串，尝试使用主配置
    else if (main_conf && main_conf->db_conn_str.len > 0)
    {
        std::string connStr((char*)main_conf->db_conn_str.data, main_conf->db_conn_str.len);

        // 使用日志记录进度
        ngx_log_error(NGX_LOG_INFO, cf->log, 0,
                      "初始化数据库连接(主配置)，连接字符串: %s", connStr.c_str());

        try
        {
            auto& dbManager = DbManager::getInstance();
            bool success = dbManager.initialize(connStr);

            ngx_log_error(NGX_LOG_INFO, cf->log, 0,
                          "数据库初始化%s",
                          success ? "成功" : "失败");

            if (!success)
            {
                return NGX_ERROR; // 如果初始化失败，直接返回错误
            }
        }
        catch (const std::exception& e)
        {
            ngx_log_error(NGX_LOG_ERR, cf->log, 0,
                          "初始化数据库异常: %s", e.what());
            return NGX_ERROR; // 如果初始化失败，直接返回错误
        }

        db_initialized = true;
    }

    // 处理博客路径配置
    if (loc_conf && loc_conf->base_path.len > 0)
    {
        // 初始化文章管理器
        std::string basePath((char*)loc_conf->base_path.data, loc_conf->base_path.len);
        std::string postsPath = basePath + "/posts";

        // 使用日志记录进度
        ngx_log_error(NGX_LOG_INFO, cf->log, 0,
                      "初始化博客文章管理器，文章目录: %s", postsPath.c_str());

        // 异步初始化文章管理器
        std::thread([postsPath]()
        {
            try
            {
                auto& manager = BlogPostManager::getInstance();
                bool success = manager.initialize(postsPath);

                ngx_log_error(NGX_LOG_INFO, ngx_cycle->log, 0,
                              "博客文章管理器初始化%s，加载了 %d 篇文章",
                              success ? "成功" : "失败",
                              (int)manager.getPostCount());
            }
            catch (const std::exception& e)
            {
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
        for (const auto& routeStr : routeStrings)
        {
            ngx_log_error(NGX_LOG_DEBUG, cf->log, 0, "Route: %s", routeStr.c_str());
        }
    }
    else
    {
        ngx_log_error(NGX_LOG_WARN, cf->log, 0, "Blog base path not configured, using default value");

        // 获取路由器实例并初始化
        auto& router = getBlogRouter();
        router.reset(); // 确保路由器是空的
        BlogModule::initBlogRoutes(&router);
    }

    ngx_log_error(NGX_LOG_INFO, cf->log, 0, "BlogModule::postConfiguration - 配置完成");
    return NGX_OK;
}

// 创建主配置
void* BlogModule::createMainConfig(ngx_conf_t* cf)
{
    // 使用NgxConf封装原始指针
    const NgxConf conf(cf);
    const NgxLog logger(cf->log);

    logger.debug("创建主配置");

    const NgxPool pool(conf.pool());

    auto* config = pool.alloc<BlogModuleConfig>();

    if (config == nullptr)
    {
        logger.error("无法分配内存用于主配置");
        return nullptr;
    }

    // 设置默认值
    config->base_path.data = nullptr;
    config->base_path.len = 0;

    config->enable_cache = NGX_CONF_UNSET;
    config->cache_time = NGX_CONF_UNSET_UINT;

    config->db_conn_str.data = nullptr;
    config->db_conn_str.len = 0;

    config->db_auto_connect = NGX_CONF_UNSET;

    logger.debug("主配置创建完成");
    return config;
}

// 初始化主配置
char* BlogModule::initMainConfig(ngx_conf_t* cf, void* conf)
{
    // 使用NgxConf封装原始指针
    NgxConf ctx(cf);

    // 这里可以进行主配置的初始化
    return NGX_CONF_OK;
}

// 创建服务器配置
void* BlogModule::createServerConfig(ngx_conf_t* cf)
{
    // 使用NgxConf封装原始指针
    NgxConf conf(cf);
    NgxLog logger(cf->log);

    logger.debug("创建服务器配置");

    NgxPool pool(conf.pool());

    auto* config = pool.alloc<BlogModuleConfig>();

    if (config == nullptr)
    {
        logger.error("无法分配内存用于服务器配置");
        return nullptr;
    }

    // 设置默认值
    config->base_path.data = nullptr;
    config->base_path.len = 0;

    config->enable_cache = NGX_CONF_UNSET;
    config->cache_time = NGX_CONF_UNSET_UINT;

    config->db_conn_str.data = nullptr;
    config->db_conn_str.len = 0;

    config->db_auto_connect = NGX_CONF_UNSET;

    logger.debug("服务器配置创建完成");
    return config;
}

// 合并服务器配置
char* BlogModule::mergeServerConfig(ngx_conf_t* cf, void* parent, void* child)
{
    // 使用NgxConf封装原始指针
    NgxConf conf(cf);
    NgxLog logger(cf->log);

    auto* prev = static_cast<BlogModuleConfig*>(parent);
    auto* curr = static_cast<BlogModuleConfig*>(child);

    // 合并配置
    ngx_conf_merge_str_value(curr->base_path, prev->base_path, "");

    ngx_conf_merge_value(curr->enable_cache, prev->enable_cache, 0);
    ngx_conf_merge_uint_value(curr->cache_time, prev->cache_time, 60);

    // 处理数据库连接字符串
    ngx_conf_merge_str_value(curr->db_conn_str, prev->db_conn_str, "");

    // 处理自动连接数据库标志
    ngx_conf_merge_value(curr->db_auto_connect, prev->db_auto_connect, 0);

    logger.debug("服务器配置合并完成");
    return NGX_CONF_OK;
}

// 创建位置配置
void* BlogModule::createLocationConfig(ngx_conf_t* cf)
{
    NgxLog logger(cf->log);
    logger.debug("创建位置配置");

    NgxPool pool(cf->pool);

    auto* conf = pool.alloc<BlogModuleConfig>();

    if (conf == nullptr)
    {
        logger.error("无法分配内存用于位置配置");
        return NGX_CONF_ERROR;
    }

    // 设置默认值
    conf->base_path.data = nullptr;
    conf->base_path.len = 0;

    conf->enable_cache = NGX_CONF_UNSET; // 未设置标记
    conf->cache_time = NGX_CONF_UNSET_UINT; // 未设置标记

    conf->db_conn_str.data = nullptr;
    conf->db_conn_str.len = 0;

    conf->db_auto_connect = NGX_CONF_UNSET; // 未设置标记

    logger.debug("位置配置创建完成");
    return conf;
}

// 合并位置配置
char* BlogModule::mergeLocationConfig(ngx_conf_t* cf, void* parent, void* child)
{
    // 转换父子配置为模块配置类型
    auto* prev = static_cast<BlogModuleConfig*>(parent);
    auto* conf = static_cast<BlogModuleConfig*>(child);

    NgxLog logger(cf->log);
    logger.debug("合并位置配置开始");

    // 合并配置，优先使用子配置（就近原则）

    // 处理base_path（未设置则继承）
    ngx_conf_merge_str_value(conf->base_path, prev->base_path, "");

    // 处理enable_cache（未设置则继承，如果父级也未设置则默认为0）
    ngx_conf_merge_value(conf->enable_cache, prev->enable_cache, 0);

    // 处理cache_time（未设置则继承，如果父级也未设置则默认为60秒）
    ngx_conf_merge_uint_value(conf->cache_time, prev->cache_time, 60);

    // 处理数据库连接字符串（未设置则继承）
    ngx_conf_merge_str_value(conf->db_conn_str, prev->db_conn_str, "");

    // 处理自动连接数据库标志（未设置则继承，如果父级也未设置则默认为0）
    ngx_conf_merge_value(conf->db_auto_connect, prev->db_auto_connect, 0);

    logger.debug("位置配置合并完成");
    return NGX_CONF_OK;
}

// 博客请求处理函数
ngx_int_t BlogModule::handleRequest(ngx_http_request_t* r)
{
    try
    {
        // 封装请求
        NgxRequest request(r);

        // 创建日志对象
        NgxLog logger(r);

        // 获取URI
        std::string uri = request.getUri();
        logger.info("处理请求: %s, 方法: %d", uri.c_str(), r->method);

        // 检查是否是静态文件请求
        if (uri.find("/static/") != std::string::npos)
        {
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
        for (const auto& routeStr : routeStrings)
        {
            logger.debug("尝试与路由 '%s' 匹配", routeStr.c_str());
        }

        // 使用路由器处理请求
        RouteHandler handler = router.match(r, uri, params);

        if (handler)
        {
            logger.info("找到匹配的路由处理器，执行处理");
            return handler(r, params);
        }

        // 尝试使用route方法作为备用
        ngx_int_t status = router.route(r);

        if (status != NGX_DECLINED)
        {
            return status;
        }

        // 未找到匹配的路由，返回404
        logger.warn("No matching route found: %s", uri.c_str());
        return NGX_HTTP_NOT_FOUND;
    }
    catch (const std::exception& e)
    {
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
    try
    {
        // 获取配置参数
        NgxString value = ngxConf.getValue(1);

        // 调试输出
        ngx_log_debug1(NGX_LOG_DEBUG_HTTP, cf->log, 0,
                       "Setting blog_path to: %V", &value.get());

        // 设置配置
        auto* config = static_cast<BlogModuleConfig*>(conf);
        config->base_path = value.get();

        return NGX_CONF_OK;
    }
    catch (const std::exception& e)
    {
        ngx_log_error(NGX_LOG_ERR, cf->log, 0, "Error in setBlogPath: %s", e.what());
        return const_cast<char*>("failed to set blog_path directive");
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
    try
    {
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
    catch (const std::exception& e)
    {
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

ngx_int_t handleOptionsRequest(NgxResponse& response, const RouteParams& params)
{
    // 调用BlogModule的静态方法
    return BlogModule::handleOptionsRequest(response, params);
}

// 在命名空间外部定义全局可见的模块结构
extern "C" {
ngx_module_t ngx_http_blog_module = {
    NGX_MODULE_V1,
    &blog_module_ctx, /* module context */
    blog_commands, /* module directives */
    NGX_HTTP_MODULE, /* module type */
    NULL, /* init master */
    NULL, /* init module */
    NULL, /* init process */
    NULL, /* init thread */
    NULL, /* exit thread */
    NULL, /* exit process */
    NULL, /* exit master */
    NGX_MODULE_V1_PADDING
};
}

// MODULE_API中的函数定义

// 处理静态文件请求
ngx_int_t BlogModule::handleStaticRequest(ngx_http_request_t* r)
{
    NgxRequest request(r);
    NgxLog logger(r);

    std::string uri(reinterpret_cast<char*>(r->uri.data), r->uri.len);
    logger.info("Handling static file request: %s", uri.c_str());

    // 如果是API请求，直接返回DECLINED让API路由处理
    if (uri.find("/api/") == 0)
    {
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
        uri.find(".json") != std::string::npos)
    {
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
ngx_int_t BlogModule::serveStaticFile(ngx_http_request_t* r, const std::string& path)
{
    NgxLog logger(r);

    // 打开文件
    std::ifstream file(path, std::ios::binary | std::ios::ate);
    if (!file.is_open())
    {
        logger.error("Failed to open file: %s", path.c_str());
        return NGX_HTTP_NOT_FOUND;
    }

    // 获取文件大小
    std::streamsize size = file.tellg();
    file.seekg(0, std::ios::beg);

    // 设置内容类型
    std::string contentType = "text/html";
    if (path.find(".css") != std::string::npos)
    {
        contentType = "text/css";
    }
    else if (path.find(".js") != std::string::npos)
    {
        contentType = "application/javascript";
    }
    else if (path.find(".json") != std::string::npos)
    {
        contentType = "application/json";
    }
    else if (path.find(".png") != std::string::npos)
    {
        contentType = "image/png";
    }
    else if (path.find(".jpg") != std::string::npos || path.find(".jpeg") != std::string::npos)
    {
        contentType = "image/jpeg";
    }
    else if (path.find(".gif") != std::string::npos)
    {
        contentType = "image/gif";
    }
    else if (path.find(".svg") != std::string::npos)
    {
        contentType = "image/svg+xml";
    }
    else if (path.find(".ico") != std::string::npos)
    {
        contentType = "image/x-icon";
    }
    else if (path.find(".woff") != std::string::npos)
    {
        contentType = "font/woff";
    }
    else if (path.find(".woff2") != std::string::npos)
    {
        contentType = "font/woff2";
    }
    else if (path.find(".ttf") != std::string::npos)
    {
        contentType = "font/ttf";
    }
    else if (path.find(".eot") != std::string::npos)
    {
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
    if (rc == NGX_ERROR || rc > NGX_OK || r->header_only)
    {
        logger.error("Failed to send HTTP header");
        return rc;
    }

    // 读取文件内容并发送
    std::vector<char> buffer(size);
    if (file.read(buffer.data(), size))
    {
        // 分配内存
        ngx_buf_t* b = ngx_create_temp_buf(r->pool, size);
        if (b == NULL)
        {
            logger.error("Failed to allocate buffer");
            return NGX_HTTP_INTERNAL_SERVER_ERROR;
        }

        // 复制数据
        ngx_memcpy(b->pos, buffer.data(), size);
        b->last = b->pos + size;
        b->memory = 1; // 这是一个内存缓冲区
        b->last_buf = 1; // 这是最后一个缓冲区

        // 创建输出链
        ngx_chain_t out;
        out.buf = b;
        out.next = nullptr;

        return ngx_http_output_filter(r, &out);
    }
    logger.error("Failed to read file content: %s", path.c_str());
    return NGX_HTTP_INTERNAL_SERVER_ERROR;
}

// 初始化博客路由
void BlogModule::initBlogRoutes(BlogRouter* router)
{
    if (!router)
    {
        return;
    }

    // 清除现有路由并设置新的API路由
    router->reset();

    // 读取配置的基础路径
    NgxLog logger(ngx_cycle->log);
    std::string basePath = "/api"; // 前后端分离模式下使用/api作为基础路径
    logger.info("Initializing blog routes, API base path: %s", basePath.c_str());

    //------------------------------------------------------------------
    // 1. 静态文件处理路由
    //------------------------------------------------------------------
    
    // 静态文件服务路由 - 处理所有非API请求
    router->addRoute(Route(HttpMethod::GET_METHOD, "/*",
                           [](ngx_http_request_t* r, const RouteParams& params) -> ngx_int_t
                           {
                               return handleStaticRequest(r);
                           }));

    //------------------------------------------------------------------
    // 2. GET方法的公共API路由
    //------------------------------------------------------------------
    
    // API 路由 - 博客文章列表
    router->addRoute(Route(HttpMethod::GET_METHOD, basePath + "/posts",
                           [](ngx_http_request_t* r, const RouteParams& params) -> ngx_int_t
                           {
                               NgxResponse response(r);
                               return handleBlogIndex(response, params);
                           }));

    // API 路由 - 获取单篇博客文章
    router->addRoute(Route(HttpMethod::GET_METHOD, basePath + "/posts/:id",
                           [](ngx_http_request_t* r, const RouteParams& params) -> ngx_int_t
                           {
                               NgxResponse response(r);
                               return handleBlogPost(response, params);
                           }));

    // API 路由 - 分类文章列表
    router->addRoute(Route(HttpMethod::GET_METHOD, basePath + "/categories/:category",
                           [](ngx_http_request_t* r, const RouteParams& params) -> ngx_int_t
                           {
                               NgxResponse response(r);
                               return handleBlogCategory(response, params);
                           }));

    // API 路由 - 标签文章列表
    router->addRoute(Route(HttpMethod::GET_METHOD, basePath + "/tags/:tag",
                           [](ngx_http_request_t* r, const RouteParams& params) -> ngx_int_t
                           {
                               NgxResponse response(r);
                               return handleBlogTag(response, params);
                           }));

    //------------------------------------------------------------------
    // 3. 管理面板的GET方法API路由
    //------------------------------------------------------------------

    // API 路由 - 管理面板
    router->addRoute(Route(HttpMethod::GET_METHOD, basePath + "/admin",
                           [](ngx_http_request_t* r, const RouteParams& params) -> ngx_int_t
                           {
                               NgxResponse response(r);
                               return handleAdmin(response, params);
                           }));

    // API 路由 - 获取管理面板统计数据
    router->addRoute(Route(HttpMethod::GET_METHOD, basePath + "/admin/stats",
                           [](ngx_http_request_t* r, const RouteParams& params) -> ngx_int_t
                           {
                               NgxResponse response(r);
                               return handleAdminStats(response, params);
                           }));

    // API 路由 - 获取文章编辑数据
    router->addRoute(Route(HttpMethod::GET_METHOD, basePath + "/admin/posts/:id/edit",
                           [](ngx_http_request_t* r, const RouteParams& params) -> ngx_int_t
                           {
                               NgxResponse response(r);
                               return handleGetPostForEdit(response, params);
                           }));

    //------------------------------------------------------------------
    // 4. 需要请求体的POST/PUT/DELETE方法API路由
    //------------------------------------------------------------------
    
    // API 路由 - 添加博客文章
    router->addRoute(Route(HttpMethod::POST_METHOD, basePath + "/admin/posts",
                           [](ngx_http_request_t* r, const RouteParams& params) -> ngx_int_t
                           {
                               NgxResponse response(r);
                               return handleAddPost(response, params);
                           }));

    // API 路由 - 编辑博客文章
    router->addRoute(Route(HttpMethod::PUT_METHOD, basePath + "/admin/posts/:id",
                           [](ngx_http_request_t* r, const RouteParams& params) -> ngx_int_t
                           {
                               NgxResponse response(r);
                               return handleEditPost(response, params);
                           }));

    // API 路由 - 删除博客文章
    router->addRoute(Route(HttpMethod::DELETE_METHOD, basePath + "/admin/posts/:id",
                           [](ngx_http_request_t* r, const RouteParams& params) -> ngx_int_t
                           {
                               NgxResponse response(r);
                               return handleDeletePost(response, params);
                           }));

    //------------------------------------------------------------------
    // 5. 特殊路由
    //------------------------------------------------------------------

    // 重定向路由
    router->addRoute(Route(HttpMethod::GET_METHOD, "/blog",
                           [](ngx_http_request_t* r, const RouteParams& params) -> ngx_int_t
                           {
                               NgxResponse response(r);
                               return handleBlogRedirect(response, params);
                           }));

    // CORS 预检请求处理
    router->addRoute(Route(HttpMethod::OPTIONS_METHOD, basePath + "/*",
                           [](ngx_http_request_t* r, const RouteParams& params) -> ngx_int_t
                           {
                               NgxResponse response(r);
                               return handleOptionsRequest(response, params);
                           }));
}

// 处理博客首页
ngx_int_t BlogModule::handleBlogIndex(NgxResponse& response, const RouteParams& params)
{
    try
    {
        NgxLog logger(response.get());
        logger.info("处理博客首页API请求");

        // 从数据库中获取文章列表
        BlogPostDao dao;
        std::vector<BlogPostRecord> posts = dao.getAllPosts(10); // 获取前10篇文章

        // 构建文章数据JSON
        json postsJson = json::array();
        for (const auto& post : posts)
        {
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
    catch (const std::exception& e)
    {
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
ngx_int_t BlogModule::handleBlogPost(NgxResponse& response, const RouteParams& params)
{
    try
    {
        NgxLog logger(response.get());
        logger.info("处理博客文章详情API请求");

        // 从路由参数中获取文章ID
        std::string postIdParam = params.at("id");
        int postId = std::stoi(postIdParam);

        // 从数据库获取文章
        BlogPostDao dao;
        auto post = dao.getPostById(postId);

        // 判断文章是否存在
        if (post.has_value())
        {
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
        }
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
    catch (const std::exception& e)
    {
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
ngx_int_t BlogModule::handleBlogCategory(NgxResponse& response, const RouteParams& params)
{
    try
    {
        NgxLog logger(response.get());

        // 获取分类参数
        auto it = params.find("category");
        if (it == params.end())
        {
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

        // 从数据库获取该分类的文章
        BlogPostDao dao;
        auto posts = dao.getPostsByCategory(category, 10); // 获取前10篇文章

        // 构建文章列表JSON
        json postsArray = json::array();
        for (const auto& post : posts)
        {
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
    catch (const std::exception& e)
    {
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
ngx_int_t BlogModule::handleBlogTag(NgxResponse& response, const RouteParams& params)
{
    try
    {
        NgxLog logger(response.get());

        // 获取标签参数
        auto it = params.find("tag");
        if (it == params.end())
        {
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

        // 从数据库获取该标签的文章
        BlogPostDao dao;
        auto posts = dao.getPostsByTag(tag, 10); // 获取前10篇文章

        // 构建响应JSON
        json data;
        data["tag"] = tag;
        data["posts"] = json::array();

        for (const auto& post : posts)
        {
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
    catch (const std::exception& e)
    {
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
ngx_int_t BlogModule::handleAdmin(NgxResponse& response, const RouteParams& params)
{
    try
    {
        NgxLog logger(response.get());
        logger.info("处理博客管理API请求");

        // 创建配置对象
        NgxRequest request(response.get());

        // 从数据库获取所有文章
        BlogPostDao dao;
        auto posts = dao.getAllPosts();

        // 构建响应JSON
        json postsArray = json::array();
        for (const auto& post : posts)
        {
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
    catch (const std::exception& e)
    {
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
ngx_int_t BlogModule::handleAddPost(NgxResponse& response, const RouteParams& params)
{
    try
    {
        NgxLog logger(response.get());
        logger.info("处理添加文章API请求");

        // 创建配置对象
        NgxRequest request(response.get());

        // 解析请求体中的JSON数据
        std::string requestBody = request.getRequestBody();
        if (requestBody.empty())
        {
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
        try
        {
            postData = json::parse(requestBody);
        }
        catch (const json::parse_error& e)
        {
            // JSON解析错误
            json errorResponse = JsonResponse::error("JSON格式错误: " + std::string(e.what()), 400);
            return response
                   .status(NGX_HTTP_BAD_REQUEST)
                   .contentType("application/json")
                   .enableCors()
                   .send(errorResponse.dump(2));
        }

        // 提取字段
        if (!postData.contains("title") || !postData.contains("content"))
        {
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
        std::string author = postData["author"];
        bool isPublished = postData.contains("published") ? postData["published"].get<bool>() : true;

        // 获取分类和标签
        std::vector<std::string> categories;
        if (postData.contains("categories") && postData["categories"].is_array())
        {
            for (const auto& category : postData["categories"])
            {
                categories.push_back(category);
            }
        }

        std::vector<std::string> tags;
        if (postData.contains("tags") && postData["tags"].is_array())
        {
            for (const auto& tag : postData["tags"])
            {
                tags.push_back(tag);
            }
        }

        // 使用DAO保存文章到数据库
        BlogPostDao dao;
        int postId = dao.createPost(title, content, summary, author,categories, tags, isPublished);

        if (postId > 0)
        {
            logger.info("文章创建成功，ID: %d", postId);

            // 获取新创建的文章
            auto post = dao.getPostById(postId);
            if (post.has_value())
            {
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
                responseData["published"] = post->published;

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
    catch (const std::exception& e)
    {
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
ngx_int_t BlogModule::handleEditPost(NgxResponse& response, const RouteParams& params)
{
    try
    {
        NgxLog logger(response.get());
        logger.info("处理编辑文章API请求");

        // 检查是否有文章ID
        if (params.find("id") == params.end())
        {
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
        NgxRequest request(response.get());

        // 解析请求体中的JSON数据
        std::string requestBody = request.getRequestBody();
        if (requestBody.empty())
        {
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
        try
        {
            postData = json::parse(requestBody);
        }
        catch (const json::parse_error& e)
        {
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
        if (!post.has_value())
        {
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
    catch (const std::exception& e)
    {
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
ngx_int_t BlogModule::handleDeletePost(NgxResponse& response, const RouteParams& params)
{
    try
    {
        NgxLog logger(response.get());
        logger.info("处理删除文章API请求");

        // 检查是否有文章ID
        if (params.find("id") == params.end())
        {
            json errorResponse = JsonResponse::error("缺少文章ID参数", 400);
            return response
                   .status(NGX_HTTP_BAD_REQUEST)
                   .contentType("application/json")
                   .enableCors()
                   .send(errorResponse.dump(2));
        }

        // 获取文章ID
        int postId = std::stoi(params.at("id"));

        // 使用DAO删除文章
        BlogPostDao dao;

        // 首先检查文章是否存在
        auto post = dao.getPostById(postId);
        if (!post.has_value())
        {
            // 文章不存在
            json errorResponse = JsonResponse::error("文章不存在", 404);
            return response
                   .status(NGX_HTTP_NOT_FOUND)
                   .contentType("application/json")
                   .enableCors()
                   .send(errorResponse.dump(2));
        }

        bool success = dao.deletePost(postId);

        if (success)
        {
            // 返回成功响应
            json successResponse = JsonResponse::success({{"message", "文章删除成功"}});
            return response
                   .status(NGX_HTTP_OK)
                   .contentType("application/json")
                   .enableCors()
                   .send(successResponse.dump(2));
        }
        // 删除失败
        json errorResponse = JsonResponse::error("删除文章失败", 500);
        return response
               .status(NGX_HTTP_INTERNAL_SERVER_ERROR)
               .contentType("application/json")
               .enableCors()
               .send(errorResponse.dump(2));
    }
    catch (const std::exception& e)
    {
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
ngx_int_t BlogModule::handleBlogRedirect(NgxResponse& response, const RouteParams& params)
{
    NgxLog logger(response.get());
    logger.info("从/blog/重定向到/");

    // 直接使用传入的response对象进行重定向
    return response
           .status(NGX_HTTP_MOVED_PERMANENTLY)
           .redirect("/", NGX_HTTP_MOVED_PERMANENTLY);
}

// 处理管理面板统计数据API请求
ngx_int_t BlogModule::handleAdminStats(NgxResponse& response, const RouteParams& params)
{
    try
    {
        NgxLog logger(response.get());
        logger.info("处理管理面板统计数据API请求");

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
        for (const auto& post : recentPosts)
        {
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
    catch (const std::exception& e)
    {
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

// 处理获取编辑文章数据的API请求
ngx_int_t BlogModule::handleGetPostForEdit(NgxResponse& response, const RouteParams& params)
{
    try
    {
        NgxLog logger(response.get());
        logger.info("处理获取文章编辑数据API请求");

        // 检查是否有文章ID
        if (params.find("id") == params.end())
        {
            json errorResponse = JsonResponse::error("缺少文章ID参数", 400);
            return response
                   .status(NGX_HTTP_BAD_REQUEST)
                   .contentType("application/json")
                   .enableCors()
                   .send(errorResponse.dump(2));
        }

        // 获取文章ID
        int postId = std::stoi(params.at("id"));

        // 使用DAO获取文章
        BlogPostDao dao;
        auto post = dao.getPostById(postId);

        if (!post.has_value())
        {
            // 文章不存在
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
        postData["summary"] = post->summary;
        postData["author"] = post->author;
        postData["categories"] = post->categories;
        postData["tags"] = post->tags;
        postData["created_at"] = post->created_at;
        postData["updated_at"] = post->updated_at;
        postData["published"] = post->published;
        postData["view_count"] = post->view_count;

        // 从现有数据中收集所有分类和标签信息
        // 注意：由于没有专门的方法获取所有分类和标签，我们使用现有数据
        // 实际应用中可能需要从数据库获取完整列表

        // 准备分类和标签的数组
        json categoriesArray = json::array();
        json tagsArray = json::array();

        // 这里简单地使用当前文章的分类和标签
        // 在真实环境中，应该查询数据库获取完整的分类和标签列表
        for (const std::string& category : post->categories)
        {
            categoriesArray.push_back(category);
        }

        for (const std::string& tag : post->tags)
        {
            tagsArray.push_back(tag);
        }

        // 构建完整响应数据
        json responseData;
        responseData["post"] = postData;
        responseData["available_categories"] = categoriesArray;
        responseData["available_tags"] = tagsArray;

        // 使用封装的NgxResponse返回JSON响应
        json successResponse = JsonResponse::success(responseData);
        std::string jsonContent = successResponse.dump(2);

        return response
               .status(NGX_HTTP_OK)
               .contentType("application/json")
               .enableCors()
               .send(jsonContent);
    }
    catch (const std::exception& e)
    {
        NgxLog logger(response.get()->connection->log);
        logger.error("处理获取文章编辑数据请求异常: %s", e.what());

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

// 处理OPTIONS预检请求（CORS）
ngx_int_t BlogModule::handleOptionsRequest(NgxResponse& response, const RouteParams& params)
{
    try
    {
        NgxLog logger(response.get());
        logger.info("处理OPTIONS预检请求");

        // 设置CORS响应头
        return response
               .status(NGX_HTTP_OK)
               .header("Access-Control-Allow-Origin", "*")
               .header("Access-Control-Allow-Methods", "GET, POST, PUT, DELETE, OPTIONS")
               .header("Access-Control-Allow-Headers", "Content-Type, Authorization, X-Requested-With")
               .header("Access-Control-Max-Age", "86400") // 24小时
               .header("Content-Length", "0")
               .send(std::string(""));
    }
    catch (const std::exception& e)
    {
        NgxLog logger(response.get()->connection->log);
        logger.error("处理OPTIONS请求异常: %s", e.what());

        // 即使出错，仍然返回200状态码，以允许跨域请求继续
        return response
               .status(NGX_HTTP_OK)
               .header("Access-Control-Allow-Origin", "*")
               .header("Access-Control-Allow-Methods", "GET, POST, PUT, DELETE, OPTIONS")
               .header("Access-Control-Allow-Headers", "Content-Type, Authorization, X-Requested-With")
               .send(std::string(""));
    }
}
