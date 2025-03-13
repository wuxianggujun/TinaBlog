//
// Created by wuxianggujun on 2025/3/11.
//

// 内部工程头文件
#include "ngx_http_blog_module.hpp"
// #include "NginxLog.hpp"
// #include "NginxDateTime.hpp"
// #include "NginxPool.hpp"
#include "NginxUnsetValue.hpp"
#include "NginxModuleLoader.hpp"
#include "NginxExtensions.hpp"

// 创建博客模块主配置
static void* ngx_http_blog_create_main_conf(ngx_conf_t* cf)
{
    auto conf = ngx_pcalloc_safe<ngx_http_blog_main_conf_t>(cf->pool);
    
    if (!conf)
    {
        return nullptr;
    }
    
    // 设置默认值
    conf->cache_time = NGX_CONF_UNSET_UINT;
    conf->enable_comment = NGX_CONF_UNSET;
    
    return conf;
}

// 合并博客模块主配置
static char* ngx_http_blog_init_main_conf(ngx_conf_t* cf, void* conf)
{
    auto mcf = static_cast<ngx_http_blog_main_conf_t*>(conf);
    
    // 设置默认值
    ngx_conf_init_uint_value(mcf->cache_time, 3600);
    ngx_conf_init_value(mcf->enable_comment, 1);
    
    return NGX_CONF_OK;
}

// 创建博客模块定位配置
static void* ngx_http_blog_create_loc_conf(ngx_conf_t* cf)
{
    auto conf = ngx_pcalloc_safe<ngx_http_blog_loc_conf_t>(cf->pool);
    
    if (!conf)
    {
        return nullptr;
    }
    
    // 设置默认值
    conf->enable = NGX_CONF_UNSET;
    
    return conf;
}

// 合并博客模块定位配置
static char* ngx_http_blog_merge_loc_conf(ngx_conf_t* cf, void* parent, void* child)
{
    auto prev = static_cast<ngx_http_blog_loc_conf_t*>(parent);
    auto conf = static_cast<ngx_http_blog_loc_conf_t*>(child);
    
    // 合并配置值
    ngx_conf_merge_value(conf->enable, prev->enable, 0);
    ngx_conf_merge_str_value(conf->template_path, prev->template_path, "templates");
    
    return NGX_CONF_OK;
}

// 简单的请求处理函数 - 只记录访问日志
static ngx_int_t ngx_http_blog_handler(ngx_http_request_t* r)
{
    // 只处理GET和HEAD请求
    if (!(r->method & (NGX_HTTP_GET | NGX_HTTP_HEAD)))
    {
        return NGX_HTTP_NOT_ALLOWED;
    }
    
    // 获取模块配置
    auto* mcf = static_cast<ngx_http_blog_main_conf_t*>(
        ngx_http_get_module_main_conf(r, ngx_http_blog_module));
        
    auto* lcf = static_cast<ngx_http_blog_loc_conf_t*>(
        ngx_http_get_module_loc_conf(r, ngx_http_blog_module));
    
    // 检查是否启用了博客功能
    if (lcf->enable != 1)
    {
        return NGX_DECLINED;
    }
    
    // 记录访问日志
    NginxLogInfo(r).print("Blog module accessed: %V", &r->uri);
    NginxLogInfo(r).print("Blog root: %V", &mcf->blog_root);
    
    // 设置简单的响应
    ngx_str_t response;
    // 使用重命名后的函数，避免宏冲突
    ngx_string_set(&response, "<html><body><h1>TinaBlog Module</h1><p>Module is working!</p></body></html>");
    
    // 设置响应头
    r->headers_out.content_type_len = sizeof("text/html") - 1;
    r->headers_out.content_type.len = sizeof("text/html") - 1;
    r->headers_out.content_type.data = (u_char*) "text/html";
    r->headers_out.status = NGX_HTTP_OK;
    r->headers_out.content_length_n = response.len;
    
    // 发送响应头
    auto rc = ngx_http_send_header(r);
    if (rc == NGX_ERROR || rc > NGX_OK || r->header_only)
    {
        return rc;
    }
    
    // 创建缓冲区链
    ngx_buf_t* b = ngx_pcalloc_safe<ngx_buf_t>(r->pool);
    if (!b)
    {
        return NGX_HTTP_INTERNAL_SERVER_ERROR;
    }
    
    // 设置缓冲区
    b->pos = response.data;
    b->last = response.data + response.len;
    b->memory = 1;
    b->last_buf = 1;
    
    // 创建输出链
    ngx_chain_t out;
    out.buf = b;
    out.next = nullptr;
    
    // 发送响应体
    return ngx_http_output_filter(r, &out);
}

// 在模块初始化阶段设置处理函数
static ngx_int_t ngx_http_blog_init(ngx_conf_t* cf)
{
    ngx_http_handler_pt* h;
    ngx_http_core_main_conf_t* cmcf;
    
    // 获取HTTP核心模块配置
    cmcf = static_cast<ngx_http_core_main_conf_t*>(
        ngx_http_conf_get_module_main_conf(cf, ngx_http_core_module));
    
    // 添加博客处理器到内容阶段
    h = static_cast<ngx_http_handler_pt*>(
        ngx_array_push(&cmcf->phases[NGX_HTTP_CONTENT_PHASE].handlers));
    
    if (h == nullptr) {
        return NGX_ERROR;
    }
    *h = ngx_http_blog_handler;
    
    // 注册模块到加载器
    NginxModuleLoader::instance().registerModule("ngx_http_blog_module", &ngx_http_blog_module);
    
    // 打印模块加载信息到日志
    NginxStderrLog().print("TinaBlog module initialized");
    
    return NGX_OK;
}

// 添加一个预配置函数，打印模块用法信息
static ngx_int_t ngx_http_blog_pre_conf(ngx_conf_t* cf)
{
    // 打印模块使用信息
    NginxStderrLog().print("TinaBlog module usage:\n%s", 
                          NginxModuleLoader::instance().getUsageInfo());
    return NGX_OK;
}

// 博客模块命令
static ngx_command_t ngx_http_blog_commands[] = {
    {
        ngx_string("blog_enable"),
        NGX_HTTP_MAIN_CONF | NGX_HTTP_SRV_CONF | NGX_HTTP_LOC_CONF | NGX_CONF_FLAG,
        ngx_conf_set_flag_slot,
        NGX_HTTP_LOC_CONF_OFFSET,
        offsetof(ngx_http_blog_loc_conf_t, enable),
        nullptr
    },
    {
        ngx_string("blog_root"),
        NGX_HTTP_MAIN_CONF | NGX_CONF_TAKE1,
        ngx_conf_set_str_slot,
        NGX_HTTP_MAIN_CONF_OFFSET,
        offsetof(ngx_http_blog_main_conf_t, blog_root),
        nullptr
    },
    {
        ngx_string("blog_db_path"),
        NGX_HTTP_MAIN_CONF | NGX_CONF_TAKE1,
        ngx_conf_set_str_slot,
        NGX_HTTP_MAIN_CONF_OFFSET,
        offsetof(ngx_http_blog_main_conf_t, database_path),
        nullptr
    },
    {
        ngx_string("blog_cache_time"),
        NGX_HTTP_MAIN_CONF | NGX_CONF_TAKE1,
        ngx_conf_set_num_slot,
        NGX_HTTP_MAIN_CONF_OFFSET,
        offsetof(ngx_http_blog_main_conf_t, cache_time),
        nullptr
    },
    {
        ngx_string("blog_enable_comment"),
        NGX_HTTP_MAIN_CONF | NGX_CONF_FLAG,
        ngx_conf_set_flag_slot,
        NGX_HTTP_MAIN_CONF_OFFSET,
        offsetof(ngx_http_blog_main_conf_t, enable_comment),
        nullptr
    },
    {
        ngx_string("blog_template_path"),
        NGX_HTTP_LOC_CONF | NGX_CONF_TAKE1,
        ngx_conf_set_str_slot,
        NGX_HTTP_LOC_CONF_OFFSET,
        offsetof(ngx_http_blog_loc_conf_t, template_path),
        nullptr
    },
    ngx_null_command
};

// 博客模块上下文
static ngx_http_module_t ngx_http_blog_module_ctx = {
    ngx_http_blog_pre_conf,            /* preconfiguration */
    ngx_http_blog_init,                /* postconfiguration */
    
    ngx_http_blog_create_main_conf,    /* create main configuration */
    ngx_http_blog_init_main_conf,      /* init main configuration */
    
    nullptr,                           /* create server configuration */
    nullptr,                           /* merge server configuration */
    
    ngx_http_blog_create_loc_conf,     /* create location configuration */
    ngx_http_blog_merge_loc_conf       /* merge location configuration */
};

// 博客模块定义
ngx_module_t ngx_http_blog_module = {
    NGX_MODULE_V1,
    &ngx_http_blog_module_ctx,          /* module context */
    ngx_http_blog_commands,             /* module directives */
    NGX_HTTP_MODULE,                    /* module type */
    nullptr,                             /* init master */
    nullptr,                             /* init module */
    nullptr,                             /* init process */
    nullptr,                             /* init thread */
    nullptr,                             /* exit thread */
    nullptr,                             /* exit process */
    nullptr,                             /* exit master */
    NGX_MODULE_V1_PADDING
};
