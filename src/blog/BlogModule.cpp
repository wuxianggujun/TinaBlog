//
// Created by wuxianggujun on 2025/3/14.
//

#include "BlogModule.hpp"
#include "NgxConf.hpp"
#include "BlogRouter.hpp"
#include "BlogTemplate.hpp"
#include "BlogPostManager.hpp"
#include "db/DbManager.hpp"
#include <iostream>
#include <cstring>
#include <thread>
#include <mutex>

// 声明外部模块变量，这样代码中使用时编译器能识别它
extern "C" {
    extern ngx_module_t ngx_http_blog_module;
}

// 声明初始化路由的函数
void initBlogRoutes();

// 初始化静态成员变量 - 不再需要这种单例方式
bool BlogModule::isRegistered_ = false;
ngx_module_t* BlogModule::blogModule_ = nullptr;

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
    
    ngx_http_core_main_conf_t* cmcf;
    ngx_http_handler_pt* h;

    // 获取HTTP核心模块的主配置
    cmcf = static_cast<ngx_http_core_main_conf_t*>(
        ngx_http_conf_get_module_main_conf(conf.get(), ngx_http_core_module));

    // 添加处理器到访问阶段
    h = static_cast<ngx_http_handler_pt*>(
        ngx_array_push(&cmcf->phases[NGX_HTTP_CONTENT_PHASE].handlers));

    if (h == nullptr) {
        return NGX_ERROR;
    }

    // 设置处理器为BlogModule的请求处理函数
    *h = handleRequest;

    // 获取全局配置中的博客路径设置
    auto* config = static_cast<BlogModuleConfig*>(
        ngx_http_conf_get_module_loc_conf(cf, ngx_http_blog_module));

    if (config && config->base_path.len > 0) {
        // 初始化文章管理器
        std::string basePath((char*)config->base_path.data, config->base_path.len);
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
    }

    // 初始化路由
    initBlogRoutes();
    
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
        // 封装Nginx请求
        NgxRequest request(r);
        
        // 创建日志对象
        NgxLog logger(r);
        
        // 获取请求URI
        std::string uri = request.getUri();
        logger.info("处理请求: %s", uri.c_str());
        
        // 获取模块配置
        auto conf = static_cast<BlogModuleConfig*>(
            ngx_http_get_module_loc_conf(r, ngx_http_blog_module));
        if (!conf) {
            logger.error("获取模块配置失败");
            return NGX_HTTP_INTERNAL_SERVER_ERROR;
        }
        
        // 从配置中获取前缀
        std::string prefix;
        if (conf->prefix.data) {
            prefix = std::string((char*)conf->prefix.data, conf->prefix.len);
            logger.info("配置的前缀路径: %s", prefix.c_str());
        }
        
        // 使用博客配置对象提供额外信息 - 但不检查前缀
        // 因为我们的路由可以处理多种路径格式
        BlogConfig config(request);
        logger.info("博客基础路径: %s", config.getBasePath().c_str());
        
        // 初始化数据库连接（如果配置为自动连接）
        static std::once_flag dbInitFlag;
        if (config.isDbAutoConnect()) {
            try {
                std::call_once(dbInitFlag, [&]() {
                    std::string connStr = config.getDbConnectionString();
                    if (!connStr.empty()) {
                        logger.info("初始化数据库连接: %s", connStr.c_str());
                        if (!DbManager::getInstance().initialize(connStr)) {
                            logger.error("数据库连接初始化失败");
                        } else {
                            logger.info("数据库连接初始化成功");
                        }
                    } else {
                        logger.warn("未配置数据库连接字符串，跳过数据库初始化");
                    }
                });
            } catch (const std::exception& ex) {
                logger.error("数据库初始化异常: %s", ex.what());
            }
        }
        
        // 调用路由处理请求
        auto& router = getBlogRouter();
        RouteParams params;
        
        logger.info("尝试匹配路由: %s", uri.c_str());
        auto handler = router.match(r, uri, params);
        
        if (handler) {
            logger.info("找到路由处理器，执行处理");
            return handler(r, params);
        } else {
            logger.error("未找到路由处理器: %s", uri.c_str());
            return NGX_HTTP_NOT_FOUND;
        }
    }
    catch (const std::exception& e) {
        ngx_log_error(NGX_LOG_ERR, r->connection->log, 0, 
                     "处理请求异常: %s", e.what());
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