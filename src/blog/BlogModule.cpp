//
// Created by wuxianggujun on 2025/3/14.
//

#include "BlogModule.hpp"
#include "NgxConf.hpp"
#include "BlogRouter.hpp"
#include <iostream>
#include <cstring>

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
    *h = handleBlogRequest;

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
void* BlogModule::createLocationConfig(ngx_conf_t* cf) {
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

// 合并位置配置
char* BlogModule::mergeLocationConfig(ngx_conf_t* cf, void* parent, void* child) {
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

// 博客请求处理函数
ngx_int_t BlogModule::handleBlogRequest(ngx_http_request_t* r) {
    // 确认请求方法
    if (!(r->method & (NGX_HTTP_GET|NGX_HTTP_HEAD|NGX_HTTP_POST))) {
        return NGX_HTTP_NOT_ALLOWED;
    }

    // 丢弃请求体 (对于GET和HEAD请求)
    if (r->method & (NGX_HTTP_GET|NGX_HTTP_HEAD)) {
        ngx_int_t rc = ngx_http_discard_request_body(r);
        if (rc != NGX_OK) {
            return rc;
        }
    }
    
    // 对于POST请求，需要读取请求体
    if (r->method == NGX_HTTP_POST) {
        ngx_int_t rc = ngx_http_read_client_request_body(r, nullptr);
        if (rc >= NGX_HTTP_SPECIAL_RESPONSE) {
            return rc;
        }
    }

    // 使用路由系统分发请求
    return getBlogRouter().dispatch(r);
}

// 博客路径指令处理函数
char* BlogModule::setBlogPath(ngx_conf_t* cf, ngx_command_t* cmd, void* conf) {
    // 使用NgxConf封装原始指针
    NgxConf ctx(cf);
    
    auto* bmconf = static_cast<BlogModuleConfig*>(conf);
    
    // 使用NgxConf提供的方法获取参数
    NgxString arg = ctx.get_arg(1);
    if (!arg.valid()) {
        return ctx.error("Invalid blog path");
    }

    // 设置博客路径
    bmconf->base_path = *arg.get();

    return NGX_CONF_OK;
}

// 模板路径指令处理函数
char* BlogModule::setTemplatePath(ngx_conf_t* cf, ngx_command_t* cmd, void* conf) {
    // 使用NgxConf封装原始指针
    NgxConf ctx(cf);
    
    auto* bmconf = static_cast<BlogModuleConfig*>(conf);
    
    // 使用NgxConf提供的方法获取参数
    NgxString arg = ctx.get_arg(1);
    if (!arg.valid()) {
        return ctx.error("Invalid template path");
    }

    // 设置模板路径
    bmconf->template_path = *arg.get();

    return NGX_CONF_OK;
}

// 启用缓存指令处理函数
char* BlogModule::setEnableCache(ngx_conf_t* cf, ngx_command_t* cmd, void* conf) {
    // 使用NgxConf封装原始指针
    NgxConf ctx(cf);
    
    auto* bmconf = static_cast<BlogModuleConfig*>(conf);
    
    // 使用NgxConf提供的方法获取参数
    NgxString arg = ctx.get_arg(1);
    if (!arg.valid()) {
        return ctx.error("Invalid cache setting (on/off expected)");
    }

    // 使用NgxString的方法进行比较，而不是直接使用C函数
    if (arg.equals("on")) {
        bmconf->enable_cache = 1;
    } else if (arg.equals("off")) {
        bmconf->enable_cache = 0;
    } else {
        return ctx.error("Invalid value (on/off expected)");
    }

    return NGX_CONF_OK;
}

// 缓存时间指令处理函数
char* BlogModule::setCacheTime(ngx_conf_t* cf, ngx_command_t* cmd, void* conf) {
    // 使用NgxConf封装原始指针
    NgxConf ctx(cf);
    
    auto* bmconf = static_cast<BlogModuleConfig*>(conf);
    
    // 使用NgxConf提供的方法获取参数
    NgxString arg = ctx.get_arg(1);
    if (!arg.valid()) {
        return ctx.error("Invalid cache time");
    }

    // 设置缓存时间
    ngx_int_t value = ngx_atoi(arg->data, arg->len);
    if (value == NGX_ERROR) {
        return ctx.error("Invalid cache time value");
    }
    
    bmconf->cache_time = value;

    return NGX_CONF_OK;
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