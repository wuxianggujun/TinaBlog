#include "BlogModule.hpp"
#include "BlogHandler.hpp"
#include <windows.h>
#include <string>
#include <sodium.h>

// 设置控制台编码为UTF-8
static void setConsoleUTF8() {
    // 设置控制台代码页为UTF-8
    SetConsoleOutputCP(CP_UTF8);
    SetConsoleCP(CP_UTF8);
}

// 添加UTF8转GBK的辅助函数
static std::string utf8_to_gbk(const char* utf8_str) {
    // 获取需要的缓冲区大小
    int len = MultiByteToWideChar(CP_UTF8, 0, utf8_str, -1, NULL, 0);
    if (len == 0) {
        return "";
    }
    
    // 转换UTF8到UTF16
    wchar_t* wstr = new wchar_t[len];
    MultiByteToWideChar(CP_UTF8, 0, utf8_str, -1, wstr, len);
    
    // 获取GBK需要的缓冲区大小
    len = WideCharToMultiByte(CP_ACP, 0, wstr, -1, NULL, 0, NULL, NULL);
    if (len == 0) {
        delete[] wstr;
        return "";
    }
    
    // 转换UTF16到GBK
    char* gbk_str = new char[len];
    WideCharToMultiByte(CP_ACP, 0, wstr, -1, gbk_str, len, NULL, NULL);
    
    std::string result(gbk_str);
    delete[] wstr;
    delete[] gbk_str;
    
    return result;
}

// 模块指令定义
static ngx_command_t ngx_http_blog_commands[] = {
    // 博客路径配置指令
    {
        ngx_string("blog_path"),                    // 指令名称
        NGX_HTTP_MAIN_CONF|NGX_HTTP_SRV_CONF|NGX_HTTP_LOC_CONF|NGX_CONF_TAKE1,  // 指令类型和作用域
        BlogModule::setBlogPath,                    // 处理函数
        NGX_HTTP_LOC_CONF_OFFSET,                  // 配置结构体偏移
        offsetof(BlogModuleConfig, base_path),     // 字段偏移
        nullptr                                    // 额外数据
    },
    
    // 缓存启用配置指令
    {
        ngx_string("blog_enable_cache"),
        NGX_HTTP_MAIN_CONF|NGX_HTTP_SRV_CONF|NGX_HTTP_LOC_CONF|NGX_CONF_FLAG,
        BlogModule::setEnableCache,
        NGX_HTTP_LOC_CONF_OFFSET,
        offsetof(BlogModuleConfig, enable_cache),
        nullptr
    },
    
    // 缓存时间配置指令
    {
        ngx_string("blog_cache_time"),
        NGX_HTTP_MAIN_CONF|NGX_HTTP_SRV_CONF|NGX_HTTP_LOC_CONF|NGX_CONF_TAKE1,
        BlogModule::setCacheTime,
        NGX_HTTP_LOC_CONF_OFFSET,
        offsetof(BlogModuleConfig, cache_time),
        nullptr
    },
    
    // 数据库连接字符串配置指令
    {
        ngx_string("blog_db_connection"),
        NGX_HTTP_MAIN_CONF|NGX_HTTP_SRV_CONF|NGX_HTTP_LOC_CONF|NGX_CONF_TAKE1,
        BlogModule::setDbConnection,
        NGX_HTTP_LOC_CONF_OFFSET,
        offsetof(BlogModuleConfig, db_connection),
        nullptr
    },
    
    // 数据库自动连接配置指令
    {
        ngx_string("blog_db_auto_connect"),
        NGX_HTTP_MAIN_CONF|NGX_HTTP_SRV_CONF|NGX_HTTP_LOC_CONF|NGX_CONF_FLAG,
        BlogModule::setDbAutoConnect,
        NGX_HTTP_LOC_CONF_OFFSET,
        offsetof(BlogModuleConfig, db_auto_connect),
        nullptr
    },
    
    ngx_null_command  // 命令数组结束标志
};

// 模块上下文定义
static ngx_http_module_t ngx_http_blog_module_ctx = {
    BlogModule::preConfiguration,    /* 预配置回调 */
    BlogModule::postConfiguration,   /* 后配置回调 */
    nullptr,                        /* 创建main配置 */
    nullptr,                        /* 初始化main配置 */
    nullptr,                        /* 创建server配置 */
    nullptr,                        /* 合并server配置 */
    BlogModule::createLocConf,      /* 创建location配置 */
    BlogModule::mergeLocConf        /* 合并location配置 */
};

// 模块定义
ngx_module_t ngx_http_blog_module = {
    NGX_MODULE_V1,                 /* 标准模块宏 */
    &ngx_http_blog_module_ctx,     /* 模块上下文 */
    ngx_http_blog_commands,        /* 模块指令 */
    NGX_HTTP_MODULE,              /* 模块类型 */
    nullptr,                      /* init master */
    nullptr,                      /* init module */
    BlogModule::initProcess,      /* init process */
    nullptr,                      /* init thread */
    nullptr,                      /* exit thread */
    BlogModule::exitProcess,      /* exit process */
    nullptr,                      /* exit master */
    NGX_MODULE_V1_PADDING
};

// 配置创建函数
void* BlogModule::createLocConf(ngx_conf_t* cf) {
    // 在第一次创建配置时设置控制台编码
    static bool first_time = true;
    if (first_time) {
        setConsoleUTF8();
        first_time = false;
    }
    
    auto* conf = static_cast<BlogModuleConfig*>(
        ngx_pcalloc(cf->pool, sizeof(BlogModuleConfig))
    );
    
    if (conf == nullptr) {
        return nullptr;
    }
    
    // 初始化配置默认值
    conf->enable_cache = NGX_CONF_UNSET;      // 缓存默认未设置
    conf->cache_time = NGX_CONF_UNSET_UINT;   // 缓存时间默认未设置
    conf->db_auto_connect = NGX_CONF_UNSET;   // 数据库自动连接默认未设置
    ngx_str_null(&conf->base_path);           // 路径默认为空
    ngx_str_null(&conf->db_connection);       // 数据库连接字符串默认为空
    
    return conf;
}

// 配置合并函数 - 合并父子配置块的配置
char* BlogModule::mergeLocConf(ngx_conf_t* cf, void* parent, void* child) {
    auto* prev = static_cast<BlogModuleConfig*>(parent);
    auto* conf = static_cast<BlogModuleConfig*>(child);
    
    // 合并各个配置项，使用父配置或默认值
    ngx_conf_merge_str_value(conf->base_path, prev->base_path, "html");
    ngx_conf_merge_value(conf->enable_cache, prev->enable_cache, 0);
    ngx_conf_merge_uint_value(conf->cache_time, prev->cache_time, 60);
    ngx_conf_merge_str_value(conf->db_connection, prev->db_connection, "");
    ngx_conf_merge_value(conf->db_auto_connect, prev->db_auto_connect, 0);
    
    return NGX_CONF_OK;
}

// 博客路径配置处理函数
char* BlogModule::setBlogPath(ngx_conf_t* cf, ngx_command_t* cmd, void* conf) {
    auto* lcf = static_cast<BlogModuleConfig*>(conf);
    ngx_str_t* value = static_cast<ngx_str_t*>(cf->args->elts);
    
    lcf->base_path = value[1];  // 设置博客路径
    return NGX_CONF_OK;
}

// 缓存启用配置处理函数
char* BlogModule::setEnableCache(ngx_conf_t* cf, ngx_command_t* cmd, void* conf) {
    auto* lcf = static_cast<BlogModuleConfig*>(conf);
    ngx_str_t* value = static_cast<ngx_str_t*>(cf->args->elts);
    
    // 解析on/off值
    if (ngx_strcasecmp(value[1].data, (u_char*)"on") == 0) {
        lcf->enable_cache = 1;
    } else if (ngx_strcasecmp(value[1].data, (u_char*)"off") == 0) {
        lcf->enable_cache = 0;
    } else {
        return (char*)"invalid value (expected: on|off)";
    }
    
    return NGX_CONF_OK;
}

// 缓存时间配置处理函数
char* BlogModule::setCacheTime(ngx_conf_t* cf, ngx_command_t* cmd, void* conf) {
    auto* lcf = static_cast<BlogModuleConfig*>(conf);
    ngx_str_t* value = static_cast<ngx_str_t*>(cf->args->elts);
    
    // 解析数值
    lcf->cache_time = ngx_atoi(value[1].data, value[1].len);
    if (lcf->cache_time == (ngx_uint_t)NGX_ERROR) {
        return (char*)"invalid number";
    }
    
    return NGX_CONF_OK;
}

// 数据库连接配置处理函数
char* BlogModule::setDbConnection(ngx_conf_t* cf, ngx_command_t* cmd, void* conf) {
    auto* lcf = static_cast<BlogModuleConfig*>(conf);
    ngx_str_t* value = static_cast<ngx_str_t*>(cf->args->elts);
    
    lcf->db_connection = value[1];  // 设置数据库连接字符串
    return NGX_CONF_OK;
}

// 数据库自动连接配置处理函数
char* BlogModule::setDbAutoConnect(ngx_conf_t* cf, ngx_command_t* cmd, void* conf) {
    auto* lcf = static_cast<BlogModuleConfig*>(conf);
    ngx_str_t* value = static_cast<ngx_str_t*>(cf->args->elts);
    
    // 解析on/off值
    if (ngx_strcasecmp(value[1].data, (u_char*)"on") == 0) {
        lcf->db_auto_connect = 1;
    } else if (ngx_strcasecmp(value[1].data, (u_char*)"off") == 0) {
        lcf->db_auto_connect = 0;
    } else {
        return (char*)"invalid value (expected: on|off)";
    }
    
    return NGX_CONF_OK;
}

// 模块预配置函数
ngx_int_t BlogModule::preConfiguration(ngx_conf_t* cf) {
    ngx_log_error(NGX_LOG_NOTICE, cf->log, 0, "博客模块开始预配置");
    
    // 初始化libsodium
    if (sodium_init() < 0) {
        ngx_log_error(NGX_LOG_ERR, cf->log, 0, "Failed to initialize libsodium");
        return NGX_ERROR;
    }
    
    return NGX_OK;
}

// 模块后配置函数
ngx_int_t BlogModule::postConfiguration(ngx_conf_t* cf) {
    ngx_log_error(NGX_LOG_NOTICE, cf->log, 0, "博客模块完成后配置");
    
    // 获取HTTP核心模块配置
    ngx_http_core_main_conf_t* cmcf = static_cast<ngx_http_core_main_conf_t*>(
        ngx_http_conf_get_module_main_conf(cf, ngx_http_core_module)
    );
    
    // 注册请求处理函数
    ngx_http_handler_pt* h = static_cast<ngx_http_handler_pt*>(
        ngx_array_push(&cmcf->phases[NGX_HTTP_CONTENT_PHASE].handlers)
    );
    
    if (h == nullptr) {
        return NGX_ERROR;
    }
    
    *h = handleRequest;
    
    return NGX_OK;
}

// 请求处理函数
ngx_int_t BlogModule::handleRequest(ngx_http_request_t* r) {
    // 获取location配置
    auto* lcf = static_cast<BlogModuleConfig*>(
        ngx_http_get_module_loc_conf(r, ngx_http_blog_module)
    );
    
    if (lcf == nullptr) {
        return NGX_DECLINED;
    }
    
    // 检查请求路径是否以/api/开头
    if (r->uri.len >= 5 && ngx_strncmp(r->uri.data, (u_char*)"/api/", 5) == 0) {
        // API请求，交给BlogHandler处理
        return BlogHandler::handleRequest(r);
    }
    
    // 非API请求，尝试作为静态文件处理
    ngx_str_t path;
    
    // 计算文件完整路径
    path.len = lcf->base_path.len + r->uri.len;
    path.data = static_cast<u_char*>(ngx_palloc(r->pool, path.len + 1));
    
    if (path.data == nullptr) {
        return NGX_HTTP_INTERNAL_SERVER_ERROR;
    }
    
    ngx_cpystrn(path.data, lcf->base_path.data, lcf->base_path.len + 1);
    ngx_cpystrn(path.data + lcf->base_path.len, r->uri.data, r->uri.len + 1);
    
    // 打开文件
    ngx_open_file_info_t of;
    ngx_memzero(&of, sizeof(ngx_open_file_info_t));
    
    of.read_ahead = 1;
    of.directio = NGX_MAX_OFF_T_VALUE;
    of.valid = 60 * 1000;  // 缓存1分钟
    of.min_uses = 1;
    of.errors = 1;
    of.events = 1;
    
    // 获取核心配置
    auto* clcf = static_cast<ngx_http_core_loc_conf_t*>(
        ngx_http_get_module_loc_conf(r, ngx_http_core_module)
    );
    
    if (ngx_http_set_disable_symlinks(r, clcf, &path, &of) != NGX_OK) {
        return NGX_HTTP_INTERNAL_SERVER_ERROR;
    }
    
    if (ngx_open_cached_file(clcf->open_file_cache, &path, &of, r->pool) != NGX_OK) {
        switch (of.err) {
            case 0:
                return NGX_HTTP_INTERNAL_SERVER_ERROR;
            case NGX_ENOENT:
            case NGX_ENOTDIR:
            case NGX_ENAMETOOLONG:
                return NGX_HTTP_NOT_FOUND;
            case NGX_EACCES:
                return NGX_HTTP_FORBIDDEN;
            default:
                return NGX_HTTP_INTERNAL_SERVER_ERROR;
        }
    }
    
    // 设置响应头
    r->root_tested = 1;
    r->allow_ranges = 1;
    r->headers_out.status = NGX_HTTP_OK;
    r->headers_out.content_length_n = of.size;
    r->headers_out.last_modified_time = of.mtime;
    
    if (ngx_http_set_content_type(r) != NGX_OK) {
        return NGX_HTTP_INTERNAL_SERVER_ERROR;
    }
    
    // 发送响应头
    ngx_int_t rc = ngx_http_send_header(r);
    if (rc == NGX_ERROR || rc > NGX_OK || r->header_only) {
        return rc;
    }
    
    // 发送文件内容
    ngx_buf_t* b = reinterpret_cast<ngx_buf_t*>(ngx_pcalloc(r->pool, sizeof(ngx_buf_t)));
    if (b == nullptr) {
        return NGX_HTTP_INTERNAL_SERVER_ERROR;
    }
    
    b->file = reinterpret_cast<ngx_file_t*>(ngx_pcalloc(r->pool, sizeof(ngx_file_t)));
    if (b->file == nullptr) {
        return NGX_HTTP_INTERNAL_SERVER_ERROR;
    }
    
    b->file_pos = 0;
    b->file_last = of.size;
    
    b->in_file = 1;
    b->last_buf = 1;
    b->last_in_chain = 1;
    
    b->file->fd = of.fd;
    b->file->name = path;
    b->file->log = r->pool->log;
    
    ngx_chain_t out;
    out.buf = b;
    out.next = nullptr;
    
    return ngx_http_output_filter(r, &out);
}

// 进程初始化函数
ngx_int_t BlogModule::initProcess(ngx_cycle_t* cycle) {
    ngx_log_error(NGX_LOG_NOTICE, cycle->log, 0, "博客模块进程初始化");
    return NGX_OK;
}

// 进程退出函数
void BlogModule::exitProcess(ngx_cycle_t* cycle) {
    ngx_log_error(NGX_LOG_NOTICE, cycle->log, 0, "博客模块进程退出");
}