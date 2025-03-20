#include "BlogModule.hpp"
#include "BlogHandler.hpp"
#include <windows.h>
#include <string>
#include <sodium.h>
#include <nlohmann/json.hpp>
#include "db/BlogPostDao.hpp"
#include "service/UserService.hpp"
#include "api/AuthController.hpp"
#include "api/PostController.hpp"
#include "api/HealthController.hpp"

using json = nlohmann::json;

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
    // 创建NgxRequest包装器
    NgxRequest req(r);
    
    // 获取location配置
    auto* lcf = req.get_loc_conf<BlogModuleConfig>(ngx_http_blog_module);
    
    if (lcf == nullptr) {
        return NGX_DECLINED;
    }

    req.get_log().debug("Handling request: %s, base_path: %s", 
                        std::string(req.get_uri()).c_str(), 
                        std::string(reinterpret_cast<char*>(lcf->base_path.data), lcf->base_path.len).c_str());
    
    // 检查请求路径是否以/api/开头
    std::string uri_str = std::string(req.get_uri());
    if (uri_str.length() >= 5 && uri_str.substr(0, 5) == "/api/") {
        // API请求，交给BlogRoute处理
        return BlogRoute::getInstance().handle_request(r);
    }
    
    // 非API请求，尝试作为静态文件处理
    ngx_str_t path;
    
    // 处理URI以确保安全
    ngx_str_t uri = r->uri;
    
    // 如果URI是根目录，直接使用index.html
    if (uri.len == 1 && uri.data[0] == '/') {
        uri.data = (u_char*)"/index.html";
        uri.len = sizeof("/index.html") - 1;
        
        req.get_log().debug("Root path detected, serving index.html");
    } else if (r->uri.len > 1 && r->uri.data[0] == '/' && !ngx_strchr(r->uri.data, '.')) {
        // 没有扩展名的路径，可能是Vue路由，直接使用index.html
        uri.data = (u_char*)"/index.html";
        uri.len = sizeof("/index.html") - 1;
        
        req.get_log().debug("Route without extension, serving index.html for path: %s", 
                           std::string(req.get_uri()).c_str());
    }
    
    // 计算文件完整路径
    path.len = lcf->base_path.len + uri.len;
    path.data = static_cast<u_char*>(ngx_palloc(r->pool, path.len + 1));
    
    if (path.data == nullptr) {
        return NGX_HTTP_INTERNAL_SERVER_ERROR;
    }
    
    // 构建完整路径 - 确保路径正确拼接
    ngx_cpystrn(path.data, lcf->base_path.data, lcf->base_path.len + 1);
    ngx_cpystrn(path.data + lcf->base_path.len, uri.data, uri.len + 1);
    
    req.get_log().debug("Attempting to open file: %s", 
                       std::string(reinterpret_cast<char*>(path.data), path.len).c_str());
    
    // 检查请求的文件是否存在
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
    
    if (ngx_open_cached_file(clcf->open_file_cache, &path, &of, r->pool) != NGX_OK) {
        // 文件不存在，尝试返回index.html
        req.get_log().debug("File not found, trying index.html: %s", 
                           std::string(reinterpret_cast<char*>(path.data), path.len).c_str());
            
        // 这是SPA应用，任何不存在的路径都返回index.html
        ngx_str_t index_path;
        index_path.len = lcf->base_path.len + sizeof("/index.html") - 1;
        index_path.data = static_cast<u_char*>(ngx_palloc(r->pool, index_path.len + 1));

        if (index_path.data == nullptr) {
            return NGX_HTTP_INTERNAL_SERVER_ERROR;
        }

        ngx_cpystrn(index_path.data, lcf->base_path.data, lcf->base_path.len + 1);
        ngx_cpystrn(index_path.data + lcf->base_path.len, (u_char*)"/index.html", sizeof("/index.html"));

        req.get_log().debug("Trying to serve index.html: %s", 
                           std::string(reinterpret_cast<char*>(index_path.data), index_path.len).c_str());

        // 重新打开index.html
        if (ngx_open_cached_file(clcf->open_file_cache, &index_path, &of, r->pool) != NGX_OK) {
            req.get_log().error("Index.html not found: %s", 
                              std::string(reinterpret_cast<char*>(index_path.data), index_path.len).c_str());
            return NGX_HTTP_NOT_FOUND;
        }
        path = index_path;
    }
    
    // 如果是目录，尝试返回index.html
    if (of.is_dir) {
        req.get_log().debug("Path is a directory, trying index.html: %s", 
                           std::string(reinterpret_cast<char*>(path.data), path.len).c_str());
        
        // 构建index.html路径
        ngx_str_t index_path;
        
        // 确保路径末尾有斜杠
        if (path.data[path.len - 1] != '/') {
            index_path.len = path.len + sizeof("index.html");
            index_path.data = static_cast<u_char*>(ngx_palloc(r->pool, index_path.len + 1));
            if (index_path.data == nullptr) {
                return NGX_HTTP_INTERNAL_SERVER_ERROR;
            }
            ngx_cpystrn(index_path.data, path.data, path.len + 1);
            index_path.data[path.len] = '/';
            ngx_cpystrn(index_path.data + path.len + 1, (u_char*)"index.html", sizeof("index.html"));
        } else {
            index_path.len = path.len + sizeof("index.html") - 1;
            index_path.data = static_cast<u_char*>(ngx_palloc(r->pool, index_path.len + 1));
            if (index_path.data == nullptr) {
                return NGX_HTTP_INTERNAL_SERVER_ERROR;
            }
            ngx_cpystrn(index_path.data, path.data, path.len + 1);
            ngx_cpystrn(index_path.data + path.len, (u_char*)"index.html", sizeof("index.html"));
        }
        
        req.get_log().debug("Trying to serve index.html in directory: %s", 
                           std::string(reinterpret_cast<char*>(index_path.data), index_path.len).c_str());
        
        // 尝试打开目录下的index.html
        ngx_memzero(&of, sizeof(ngx_open_file_info_t));
        of.read_ahead = 1;
        of.directio = NGX_MAX_OFF_T_VALUE;
        of.valid = 60 * 1000;
        of.min_uses = 1;
        of.errors = 1;
        of.events = 1;
        
        if (ngx_open_cached_file(clcf->open_file_cache, &index_path, &of, r->pool) != NGX_OK) {
            req.get_log().error("Index.html not found in directory: %s", 
                              std::string(reinterpret_cast<char*>(index_path.data), index_path.len).c_str());
            return NGX_HTTP_NOT_FOUND;
        }
        
        path = index_path;
    }
    
    if (of.fd == NGX_INVALID_FILE) {
        req.get_log().error("Invalid file: %s, fd: %d", 
                           std::string(reinterpret_cast<char*>(path.data), path.len).c_str(), of.fd);
        return NGX_HTTP_NOT_FOUND;
    }
    
    if (of.size == 0) {
        req.get_log().error("File size is zero: %s", 
                           std::string(reinterpret_cast<char*>(path.data), path.len).c_str());
        return NGX_HTTP_NOT_FOUND;
    }
    
    req.get_log().debug("Successfully opened file: %s, size: %d, fd: %d", 
                       std::string(reinterpret_cast<char*>(path.data), path.len).c_str(), 
                       static_cast<int>(of.size), of.fd);
    
    // 设置响应头和发送文件内容
    r->root_tested = 1;
    r->allow_ranges = 1;
    r->headers_out.status = NGX_HTTP_OK;
    r->headers_out.content_length_n = of.size;
    r->headers_out.last_modified_time = of.mtime;
    
    // 设置Content-Type头
    ngx_str_t type = ngx_string("text/html");
    r->headers_out.content_type = type;
    r->headers_out.content_type_len = type.len;
    
    // 对于不同类型的文件，设置不同的Content-Type
    std::string path_str(reinterpret_cast<const char*>(path.data), path.len);
    if (path_str.find(".css") != std::string::npos) {
        r->headers_out.content_type.len = sizeof("text/css") - 1;
        r->headers_out.content_type.data = (u_char *) "text/css";
    } else if (path_str.find(".js") != std::string::npos) {
        r->headers_out.content_type.len = sizeof("application/javascript") - 1;
        r->headers_out.content_type.data = (u_char *) "application/javascript";
    } else if (path_str.find(".html") != std::string::npos || path_str.find(".htm") != std::string::npos) {
        r->headers_out.content_type.len = sizeof("text/html") - 1;
        r->headers_out.content_type.data = (u_char *) "text/html";
    } else if (path_str.find(".jpg") != std::string::npos || path_str.find(".jpeg") != std::string::npos) {
        r->headers_out.content_type.len = sizeof("image/jpeg") - 1;
        r->headers_out.content_type.data = (u_char *) "image/jpeg";
    } else if (path_str.find(".png") != std::string::npos) {
        r->headers_out.content_type.len = sizeof("image/png") - 1;
        r->headers_out.content_type.data = (u_char *) "image/png";
    } else if (path_str.find(".gif") != std::string::npos) {
        r->headers_out.content_type.len = sizeof("image/gif") - 1;
        r->headers_out.content_type.data = (u_char *) "image/gif";
    } else if (path_str.find(".svg") != std::string::npos) {
        r->headers_out.content_type.len = sizeof("image/svg+xml") - 1;
        r->headers_out.content_type.data = (u_char *) "image/svg+xml";
    } else if (path_str.find(".json") != std::string::npos) {
        r->headers_out.content_type.len = sizeof("application/json") - 1;
        r->headers_out.content_type.data = (u_char *) "application/json";
    } else if (path_str.find(".woff") != std::string::npos) {
        r->headers_out.content_type.len = sizeof("font/woff") - 1;
        r->headers_out.content_type.data = (u_char *) "font/woff";
    } else if (path_str.find(".woff2") != std::string::npos) {
        r->headers_out.content_type.len = sizeof("font/woff2") - 1;
        r->headers_out.content_type.data = (u_char *) "font/woff2";
    } else if (path_str.find(".ttf") != std::string::npos) {
        r->headers_out.content_type.len = sizeof("font/ttf") - 1;
        r->headers_out.content_type.data = (u_char *) "font/ttf";
    } else if (path_str.find(".ico") != std::string::npos) {
        r->headers_out.content_type.len = sizeof("image/x-icon") - 1;
        r->headers_out.content_type.data = (u_char *) "image/x-icon";
    } else {
        // 默认使用 ngx_http_set_content_type
        if (ngx_http_set_content_type(r) != NGX_OK) {
            return NGX_HTTP_INTERNAL_SERVER_ERROR;
        }
    }
    
    // 添加其他重要的HTTP头
    ngx_table_elt_t *h = reinterpret_cast<ngx_table_elt_t*>(ngx_list_push(&r->headers_out.headers));
    if (h != NULL) {
        h->hash = 1;
        ngx_str_set(&h->key, "X-Content-Type-Options");
        ngx_str_set(&h->value, "nosniff");
    }
    
    // 设置缓存控制头
    h = reinterpret_cast<ngx_table_elt_t*>(ngx_list_push(&r->headers_out.headers));
    if (h != NULL) {
        h->hash = 1;
        ngx_str_set(&h->key, "Cache-Control");
        
        // 为静态资源设置缓存
        if (path_str.find("/assets/") != std::string::npos) {
            ngx_str_set(&h->value, "public, max-age=31536000"); // 1年
        } else {
            ngx_str_set(&h->value, "no-cache, no-store, must-revalidate");
        }
    }
    
    // 发送响应头
    ngx_int_t rc = ngx_http_send_header(r);
    if (rc == NGX_ERROR || rc > NGX_OK || r->header_only) {
        return rc;
    }
    
    // 发送文件内容
    ngx_buf_t* b = reinterpret_cast<ngx_buf_t*>(ngx_pcalloc(r->pool, sizeof(ngx_buf_t)));
    if (b == nullptr) {
        req.get_log().error("Failed to allocate buffer");
        return NGX_HTTP_INTERNAL_SERVER_ERROR;
    }
    
    b->file = reinterpret_cast<ngx_file_t*>(ngx_pcalloc(r->pool, sizeof(ngx_file_t)));
    if (b->file == nullptr) {
        req.get_log().error("Failed to allocate file structure");
        return NGX_HTTP_INTERNAL_SERVER_ERROR;
    }

    // 添加更多的调试日志
    req.get_log().debug("File size: %d, FD: %d", 
                       static_cast<int>(of.size), of.fd);
    
    b->file_pos = 0;
    b->file_last = of.size;
    b->in_file = 1;  // 确保设置为1
    b->last_buf = 1;
    b->last_in_chain = 1;
    
    b->file->fd = of.fd;
    b->file->name = path;
    b->file->log = r->connection->log;
    
    // 创建输出链
    ngx_chain_t* out = reinterpret_cast<ngx_chain_t*>(ngx_pcalloc(r->pool, sizeof(ngx_chain_t)));
    if (out == nullptr) {
        req.get_log().error("Failed to allocate chain link");
        return NGX_HTTP_INTERNAL_SERVER_ERROR;
    }
    out->buf = b;
    out->next = nullptr;
    
    req.get_log().debug("Setting content type for file: %s to %s", 
                      std::string(reinterpret_cast<char*>(path.data), path.len).c_str(),
                      std::string(reinterpret_cast<char*>(r->headers_out.content_type.data), 
                                r->headers_out.content_type.len).c_str());
    
    return ngx_http_output_filter(r, out);
}

// 进程初始化函数
ngx_int_t BlogModule::initProcess(ngx_cycle_t* cycle) {
    // 使用静态变量确保只初始化一次
    static bool initialized = false;
    
    if (initialized) {
        ngx_log_error(NGX_LOG_INFO, cycle->log, 0, "博客模块已经初始化");
        return NGX_OK;
    }
    
    initialized = true;
    
    ngx_log_error(NGX_LOG_NOTICE, cycle->log, 0, "博客模块进程初始化");
    
    // 初始化JWT服务
    JwtService::getInstance().init("blog_module_secret_key_change_in_production", 86400); // 24小时过期
    
    // 初始化路由系统
    BlogRoute::getInstance().init(cycle->pool);
    
    // 注册API路由
    registerApiRoutes();
    
    // 尝试从配置中获取数据库连接信息
    BlogModuleConfig* lcf = nullptr;
    
    // 从全局配置中读取
    ngx_http_conf_ctx_t* conf_ctx = reinterpret_cast<ngx_http_conf_ctx_t*>(cycle->conf_ctx[ngx_http_module.index]);
    if (conf_ctx) {
        lcf = static_cast<BlogModuleConfig*>(
            conf_ctx->loc_conf[ngx_http_blog_module.ctx_index]
        );
    }
    
    if (lcf) {
        // 检查是否有数据库连接字符串
        if (lcf->db_connection.len > 0) {
            std::string connStr(reinterpret_cast<char*>(lcf->db_connection.data), lcf->db_connection.len);
            
            // 初始化数据库连接
            if (lcf->db_auto_connect) {
                try {
                    ngx_log_error(NGX_LOG_INFO, cycle->log, 0, 
                                "Initializing database connection with: %s", connStr.c_str());

                    // 使用连接池初始化数据库，设置最小连接数为4，最大连接数为20
                    if (DbManager::getInstance().initialize(connStr, 4, 20)) {
                        ngx_log_error(NGX_LOG_INFO, cycle->log, 0, 
                                    "Database connection successful");
                        
                        // 数据库连接成功后，创建数据表
                        if (DbManager::getInstance().createTables()) {
                            ngx_log_error(NGX_LOG_INFO, cycle->log, 0, 
                                        "Database tables created or verified successfully");
                        } else {
                            ngx_log_error(NGX_LOG_ERR, cycle->log, 0, 
                                        "Failed to create or verify database tables");
                        }
                        
                        // 记录连接池状态
                        auto poolStatus = DbManager::getInstance().getPoolStatus();
                        ngx_log_error(NGX_LOG_INFO, cycle->log, 0, 
                                    "Database connection pool status: idle=%d, active=%d, max=%d", 
                                    poolStatus.idle_connections, 
                                    poolStatus.active_connections,
                                    poolStatus.max_connections);
                    } else {
                        ngx_log_error(NGX_LOG_ERR, cycle->log, 0, 
                                    "Failed to initialize database connection");
                    }
                } catch (const std::exception& e) {
                    ngx_log_error(NGX_LOG_ERR, cycle->log, 0, 
                                "Error initializing database: %s", e.what());
                }
            } else {
                ngx_log_error(NGX_LOG_INFO, cycle->log, 0, 
                            "Auto database connection is disabled");
            }
        } else {
            ngx_log_error(NGX_LOG_WARN, cycle->log, 0, 
                        "No database connection string configured");
        }
    } else {
        ngx_log_error(NGX_LOG_WARN, cycle->log, 0, 
                    "Could not find blog module configuration");
    }
    
    return NGX_OK;
}

// 进程退出函数
void BlogModule::exitProcess(ngx_cycle_t* cycle) {
    ngx_log_error(NGX_LOG_NOTICE, cycle->log, 0, "博客模块进程退出");
    
    // 关闭数据库连接池
    try {
        DbManager::getInstance().close();
    } catch (const std::exception& e) {
        ngx_log_error(NGX_LOG_ERR, cycle->log, 0, 
                    "Error closing database connection pool: %s", e.what());
    }
}

// 注册API路由
void BlogModule::registerApiRoutes() {
    auto& router = BlogRoute::getInstance();
    
    // 注册健康检查API
    HealthController::getInstance().registerRoutes(router);
    
    // 注册认证相关API
    AuthController::getInstance().registerRoutes(router);
    
    // 注册文章相关API
    PostController::getInstance().registerRoutes(router);
    
    ngx_log_error(NGX_LOG_INFO, ngx_cycle->log, 0, "API路由注册完成");
}

