//
// Created by wuxianggujun on 2025/3/14.
//

#include "BlogModule.hpp"
#include <iostream>
#include <cstring>

namespace blog {

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
    // 这里可以进行一些预配置工作
    return NGX_OK;
}

// 后配置回调
ngx_int_t BlogModule::postConfiguration(ngx_conf_t* cf) {
    ngx_http_core_main_conf_t* cmcf;
    ngx_http_handler_pt* h;

    // 获取HTTP核心模块的主配置
    cmcf = static_cast<ngx_http_core_main_conf_t*>(
        ngx_http_conf_get_module_main_conf(cf, ngx_http_core_module));

    // 添加处理器到访问阶段
    h = static_cast<ngx_http_handler_pt*>(
        ngx_array_push(&cmcf->phases[NGX_HTTP_CONTENT_PHASE].handlers));

    if (h == nullptr) {
        return NGX_ERROR;
    }

    // 设置处理器为BlogModule的请求处理函数
    *h = handleBlogRequest;

    return NGX_OK;
}

// 创建主配置
void* BlogModule::createMainConfig(ngx_conf_t* cf) {
    // 分配内存
    auto* conf = static_cast<BlogModuleConfig*>(
        ngx_pcalloc(cf->pool, sizeof(BlogModuleConfig)));

    if (conf == nullptr) {
        return nullptr;
    }

    // 设置默认值
    conf->enable_cache = NGX_CONF_UNSET;
    conf->cache_time = NGX_CONF_UNSET_UINT;

    return conf;
}

// 初始化主配置
char* BlogModule::initMainConfig(ngx_conf_t* cf, void* conf) {
    // 这里可以进行主配置的初始化
    return NGX_CONF_OK;
}

// 创建服务器配置
void* BlogModule::createServerConfig(ngx_conf_t* cf) {
    // 分配内存
    auto* conf = static_cast<BlogModuleConfig*>(
        ngx_pcalloc(cf->pool, sizeof(BlogModuleConfig)));

    if (conf == nullptr) {
        return nullptr;
    }

    // 设置默认值
    conf->enable_cache = NGX_CONF_UNSET;
    conf->cache_time = NGX_CONF_UNSET_UINT;

    return conf;
}

// 合并服务器配置
char* BlogModule::mergeServerConfig(ngx_conf_t* cf, void* parent, void* child) {
    auto* prev = static_cast<BlogModuleConfig*>(parent);
    auto* conf = static_cast<BlogModuleConfig*>(child);

    // 合并配置
    ngx_conf_merge_str_value(conf->base_path, prev->base_path, "");
    ngx_conf_merge_str_value(conf->template_path, prev->template_path, "");
    ngx_conf_merge_value(conf->enable_cache, prev->enable_cache, 0);
    ngx_conf_merge_uint_value(conf->cache_time, prev->cache_time, 60);

    return NGX_CONF_OK;
}

// 创建位置配置
void* BlogModule::createLocationConfig(ngx_conf_t* cf) {
    // 分配内存
    auto* conf = static_cast<BlogModuleConfig*>(
        ngx_pcalloc(cf->pool, sizeof(BlogModuleConfig)));

    if (conf == nullptr) {
        return nullptr;
    }

    // 设置默认值
    conf->enable_cache = NGX_CONF_UNSET;
    conf->cache_time = NGX_CONF_UNSET_UINT;

    return conf;
}

// 合并位置配置
char* BlogModule::mergeLocationConfig(ngx_conf_t* cf, void* parent, void* child) {
    auto* prev = static_cast<BlogModuleConfig*>(parent);
    auto* conf = static_cast<BlogModuleConfig*>(child);

    // 合并配置
    ngx_conf_merge_str_value(conf->base_path, prev->base_path, "");
    ngx_conf_merge_str_value(conf->template_path, prev->template_path, "");
    ngx_conf_merge_value(conf->enable_cache, prev->enable_cache, 0);
    ngx_conf_merge_uint_value(conf->cache_time, prev->cache_time, 60);

    return NGX_CONF_OK;
}

// 处理博客请求
ngx_int_t BlogModule::handleBlogRequest(ngx_http_request_t* r) {
    // 获取位置配置
    auto* conf = static_cast<BlogModuleConfig*>(
        ngx_http_get_module_loc_conf(r, ngx_http_core_module));

    // 移除特定路径前缀检查，处理所有请求
    // 或者，如果您希望保留某些检查逻辑，可以修改为更宽松的条件
    
    /*
    // 原来的检查代码
    if (r->uri.len < 6 || ngx_strncmp(r->uri.data, "/blog/", 6) != 0) {
        return NGX_DECLINED;
    }
    */
    
    // 新的检查逻辑 - 例如可以排除某些路径
    if (r->uri.len > 8 && ngx_strncmp(r->uri.data, "/static/", 8) == 0) {
        return NGX_DECLINED;  // 静态资源路径交给其他处理器
    }

    // 只接受GET和HEAD方法
    if (!(r->method & (NGX_HTTP_GET|NGX_HTTP_HEAD))) {
        return NGX_HTTP_NOT_ALLOWED;
    }

    // 忽略请求体
    ngx_int_t rc = ngx_http_discard_request_body(r);
    if (rc != NGX_OK) {
        return rc;
    }

    // 设置响应头
    r->headers_out.content_type_len = sizeof("text/html") - 1;
    ngx_str_set(&r->headers_out.content_type, "text/html");
    r->headers_out.status = NGX_HTTP_OK;

    // 示例响应
    ngx_str_t response = ngx_string("<html><body><h1>Blog Module</h1>"
                                   "<p>This is a placeholder for the blog module.</p>"
                                   "</body></html>");

    // 设置响应内容长度
    r->headers_out.content_length_n = response.len;

    // 发送响应头
    rc = ngx_http_send_header(r);
    if (rc == NGX_ERROR || rc > NGX_OK || r->header_only) {
        return rc;
    }

    // 创建缓冲区链
    ngx_buf_t* b = static_cast<ngx_buf_t*>(ngx_calloc_buf(r->pool));
    if (b == nullptr) {
        return NGX_HTTP_INTERNAL_SERVER_ERROR;
    }

    // 设置缓冲区属性
    b->pos = response.data;
    b->last = response.data + response.len;
    b->memory = 1;    // 内存缓冲区
    b->last_buf = 1;  // 最后一个缓冲区

    // 创建缓冲区链
    ngx_chain_t out;
    out.buf = b;
    out.next = nullptr;

    // 发送响应体
    return ngx_http_output_filter(r, &out);
}

// 博客路径指令处理函数
char* BlogModule::setBlogPath(ngx_conf_t* cf, ngx_command_t* cmd, void* conf) {
    auto* bmconf = static_cast<BlogModuleConfig*>(conf);
    ngx_str_t* value = static_cast<ngx_str_t*>(cf->args->elts);

    // 设置博客路径
    bmconf->base_path = value[1];

    return NGX_CONF_OK;
}

// 模板路径指令处理函数
char* BlogModule::setTemplatePath(ngx_conf_t* cf, ngx_command_t* cmd, void* conf) {
    auto* bmconf = static_cast<BlogModuleConfig*>(conf);
    ngx_str_t* value = static_cast<ngx_str_t*>(cf->args->elts);

    // 设置模板路径
    bmconf->template_path = value[1];

    return NGX_CONF_OK;
}

// 启用缓存指令处理函数
char* BlogModule::setEnableCache(ngx_conf_t* cf, ngx_command_t* cmd, void* conf) {
    auto* bmconf = static_cast<BlogModuleConfig*>(conf);
    ngx_str_t* value = static_cast<ngx_str_t*>(cf->args->elts);

    // 创建临时ngx_str_t来存储比较字符串
    ngx_str_t on_str = ngx_string("on");
    ngx_str_t off_str = ngx_string("off");
    
    // 处理启用/禁用缓存
    if (ngx_strcasecmp(value[1].data, on_str.data) == 0) {
        bmconf->enable_cache = 1;
    } else if (ngx_strcasecmp(value[1].data, off_str.data) == 0) {
        bmconf->enable_cache = 0;
    } else {
        return const_cast<char*>("Invalid value (on/off expected)");
    }

    return NGX_CONF_OK;
}

// 缓存时间指令处理函数
char* BlogModule::setCacheTime(ngx_conf_t* cf, ngx_command_t* cmd, void* conf) {
    auto* bmconf = static_cast<BlogModuleConfig*>(conf);
    ngx_str_t* value = static_cast<ngx_str_t*>(cf->args->elts);

    // 设置缓存时间
    bmconf->cache_time = ngx_atoi(value[1].data, value[1].len);
    if (bmconf->cache_time == NGX_ERROR) {
        return const_cast<char*>("Invalid cache time value");
    }

    return NGX_CONF_OK;
}

} // namespace blog

// 在命名空间外部定义全局可见的模块结构
extern "C" {
    ngx_module_t ngx_http_blog_module = {
        NGX_MODULE_V1,
        &blog::blog_module_ctx,     /* module context */
        blog::blog_commands,        /* module directives */
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