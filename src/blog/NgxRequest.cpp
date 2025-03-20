#include "NgxRequest.hpp"
#include <algorithm>
#include <fstream>

// 获取请求头
std::optional<std::string> NgxRequest::get_header(const std::string& name) const {
    if (!get()) return std::nullopt;
    
    // 手动遍历headers列表来查找header
    ngx_list_part_t* part = const_cast<ngx_list_part_t*>(&get()->headers_in.headers.part);
    ngx_table_elt_t* h = static_cast<ngx_table_elt_t*>(part->elts);
    
    for (ngx_uint_t i = 0; /* void */; i++) {
        if (i >= part->nelts) {
            if (part->next == nullptr) {
                break;
            }
            
            part = part->next;
            h = static_cast<ngx_table_elt_t*>(part->elts);
            i = 0;
        }
        
        if (h[i].key.len == name.length() && 
            ngx_strncasecmp(h[i].key.data, 
                           const_cast<u_char*>(reinterpret_cast<const u_char*>(name.c_str())),
                           h[i].key.len) == 0) {
            return std::string(reinterpret_cast<const char*>(h[i].value.data), h[i].value.len);
        }
    }
    
    return std::nullopt;
}

// 获取所有请求头
std::unordered_map<std::string, std::string> NgxRequest::get_headers() const {
    std::unordered_map<std::string, std::string> headers;
    
    if (!get()) return headers;
    
    // 遍历请求头列表
    const ngx_list_part_t* part = &get()->headers_in.headers.part;
    ngx_table_elt_t* h = static_cast<ngx_table_elt_t*>(part->elts);
    
    for (ngx_uint_t i = 0; /* void */; i++) {
        if (i >= part->nelts) {
            if (part->next == nullptr) {
                break;
            }
            
            part = part->next;
            h = static_cast<ngx_table_elt_t*>(part->elts);
            i = 0;
        }
        
        headers.emplace(
            std::string(reinterpret_cast<const char*>(h[i].key.data), h[i].key.len),
            std::string(reinterpret_cast<const char*>(h[i].value.data), h[i].value.len)
        );
    }
    
    return headers;
}

// 解析查询参数
void NgxRequest::parse_args() const {
    if (args_parsed || !get()) return;
    
    // 清空缓存
    args_cache.clear();
    
    // 获取args字符串
    if (get()->args.len == 0) {
        args_parsed = true;
        return;
    }
    
    std::string_view args_str(reinterpret_cast<const char*>(get()->args.data), get()->args.len);
    size_t start = 0;
    size_t pos = 0;
    
    // 解析形如 key1=value1&key2=value2 的参数字符串
    while (pos != std::string_view::npos) {
        pos = args_str.find('&', start);
        std::string_view param = args_str.substr(start, pos - start);
        
        size_t eq_pos = param.find('=');
        if (eq_pos != std::string_view::npos) {
            std::string key(param.substr(0, eq_pos));
            std::string value(param.substr(eq_pos + 1));
            
            // URL解码(简化版)
            auto url_decode = [](const std::string& in) -> std::string {
                std::string out;
                out.reserve(in.size());
                
                for (size_t i = 0; i < in.size(); ++i) {
                    if (in[i] == '%' && i + 2 < in.size()) {
                        int value = 0;
                        std::sscanf(in.c_str() + i + 1, "%2x", &value);
                        out += static_cast<char>(value);
                        i += 2;
                    } else if (in[i] == '+') {
                        out += ' ';
                    } else {
                        out += in[i];
                    }
                }
                
                return out;
            };
            
            // 存储解码后的参数
            args_cache[url_decode(key)] = url_decode(value);
        }
        
        start = pos + 1;
    }
    
    args_parsed = true;
}

// 获取查询参数
std::optional<std::string> NgxRequest::get_arg(const std::string& name) const {
    parse_args();
    
    auto it = args_cache.find(name);
    if (it != args_cache.end()) {
        return it->second;
    }
    
    return std::nullopt;
}

// 获取所有查询参数
std::unordered_map<std::string, std::string> NgxRequest::get_args() const {
    parse_args();
    return args_cache;
}

// 读取请求体
std::string NgxRequest::read_body() {
    if (!get()) return {};
    
    // 确保请求体已经被读取
    if (get()->request_body == nullptr) {
        // 如果请求体尚未被读取，则返回空字符串
        // 实际应用中，应该使用ngx_http_read_client_request_body
        // 但这需要异步处理，这里简化处理
        return {};
    }
    
    if (get()->request_body->temp_file) {
        // 如果请求体存储在临时文件中
        // 从文件读取数据
        std::string body;
        ngx_str_t file_name = get()->request_body->temp_file->file.name;
        
        std::ifstream file(reinterpret_cast<const char*>(file_name.data), 
                          std::ios::binary | std::ios::ate);
        
        if (file.is_open()) {
            size_t size = file.tellg();
            file.seekg(0, std::ios::beg);
            
            body.resize(size);
            file.read(&body[0], size);
            file.close();
            
            return body;
        }
        
        return {};
    } else if (get()->request_body->bufs) {
        // 如果请求体在内存中
        std::string body;
        
        // 遍历所有缓冲区，拼接数据
        for (ngx_chain_t* cl = get()->request_body->bufs; cl; cl = cl->next) {
            body.append(reinterpret_cast<const char*>(cl->buf->pos), 
                       cl->buf->last - cl->buf->pos);
        }
        
        return body;
    }
    
    return {};
}

// 发送状态码响应
ngx_int_t NgxRequest::send_status(ngx_uint_t status_code) {
    if (!get()) return NGX_ERROR;
    
    get()->headers_out.status = status_code;
    get()->headers_out.content_length_n = 0;
    
    return ngx_http_send_header(get());
}

// 发送纯文本响应
ngx_int_t NgxRequest::send_text(const std::string& text, ngx_uint_t status_code) {
    if (!get()) return NGX_ERROR;
    
    // 设置响应头
    get()->headers_out.status = status_code;
    get()->headers_out.content_length_n = text.length();
    
    // 设置Content-Type
    ngx_str_t content_type = ngx_string("text/plain");
    get()->headers_out.content_type = content_type;
    get()->headers_out.content_type_len = content_type.len;
    
    // 发送响应头
    ngx_int_t rc = ngx_http_send_header(get());
    if (rc == NGX_ERROR || rc > NGX_OK || get()->header_only) {
        return rc;
    }
    
    // 分配缓冲区
    ngx_buf_t* b = ngx_create_temp_buf(get()->pool, text.length());
    if (!b) {
        return NGX_HTTP_INTERNAL_SERVER_ERROR;
    }
    
    // 复制数据到缓冲区
    ngx_memcpy(b->pos, text.c_str(), text.length());
    b->last = b->pos + text.length();
    b->last_buf = 1;
    b->last_in_chain = 1;
    
    // 创建输出链
    ngx_chain_t out;
    out.buf = b;
    out.next = nullptr;
    
    // 发送响应体
    return ngx_http_output_filter(get(), &out);
}

// 发送JSON响应
ngx_int_t NgxRequest::send_json(const std::string& json, ngx_uint_t status_code) {
    if (!get()) return NGX_ERROR;
    
    // 设置响应头
    get()->headers_out.status = status_code;
    get()->headers_out.content_length_n = json.length();
    
    // 设置Content-Type
    ngx_str_t content_type = ngx_string("application/json");
    get()->headers_out.content_type = content_type;
    get()->headers_out.content_type_len = content_type.len;
    
    // 发送响应头
    ngx_int_t rc = ngx_http_send_header(get());
    if (rc == NGX_ERROR || rc > NGX_OK || get()->header_only) {
        return rc;
    }
    
    // 分配缓冲区
    ngx_buf_t* b = ngx_create_temp_buf(get()->pool, json.length());
    if (!b) {
        return NGX_HTTP_INTERNAL_SERVER_ERROR;
    }
    
    // 复制数据到缓冲区
    ngx_memcpy(b->pos, json.c_str(), json.length());
    b->last = b->pos + json.length();
    b->last_buf = 1;
    b->last_in_chain = 1;
    
    // 创建输出链
    ngx_chain_t out;
    out.buf = b;
    out.next = nullptr;
    
    // 发送响应体
    return ngx_http_output_filter(get(), &out);
}

// 发送HTML响应
ngx_int_t NgxRequest::send_html(const std::string& html, ngx_uint_t status_code) {
    if (!get()) return NGX_ERROR;
    
    // 设置响应头
    get()->headers_out.status = status_code;
    get()->headers_out.content_length_n = html.length();
    
    // 设置Content-Type
    ngx_str_t content_type = ngx_string("text/html");
    get()->headers_out.content_type = content_type;
    get()->headers_out.content_type_len = content_type.len;
    
    // 发送响应头
    ngx_int_t rc = ngx_http_send_header(get());
    if (rc == NGX_ERROR || rc > NGX_OK || get()->header_only) {
        return rc;
    }
    
    // 分配缓冲区
    ngx_buf_t* b = ngx_create_temp_buf(get()->pool, html.length());
    if (!b) {
        return NGX_HTTP_INTERNAL_SERVER_ERROR;
    }
    
    // 复制数据到缓冲区
    ngx_memcpy(b->pos, html.c_str(), html.length());
    b->last = b->pos + html.length();
    b->last_buf = 1;
    b->last_in_chain = 1;
    
    // 创建输出链
    ngx_chain_t out;
    out.buf = b;
    out.next = nullptr;
    
    // 发送响应体
    return ngx_http_output_filter(get(), &out);
}

// 发送文件响应
ngx_int_t NgxRequest::send_file(const std::string& file_path) {
    if (!get()) return NGX_ERROR;
    
    // 打开文件
    ngx_open_file_info_t of;
    ngx_memzero(&of, sizeof(ngx_open_file_info_t));
    
    of.read_ahead = 1;
    of.directio = NGX_MAX_OFF_T_VALUE;
    of.valid = 60 * 1000;
    of.min_uses = 1;
    of.errors = 1;
    of.events = 1;
    
    // 获取核心配置
    auto* clcf = static_cast<ngx_http_core_loc_conf_t*>(
        ngx_http_get_module_loc_conf(get(), ngx_http_core_module)
    );
    
    // 文件路径
    ngx_str_t path;
    path.len = file_path.length();
    path.data = reinterpret_cast<u_char*>(ngx_palloc(get()->pool, path.len + 1));
    
    if (path.data == nullptr) {
        return NGX_HTTP_INTERNAL_SERVER_ERROR;
    }
    
    ngx_memcpy(path.data, file_path.c_str(), path.len);
    path.data[path.len] = '\0';
    
    // 尝试打开文件
    if (ngx_open_cached_file(clcf->open_file_cache, &path, &of, get()->pool) != NGX_OK) {
        get()->headers_out.status = NGX_HTTP_NOT_FOUND;
        return ngx_http_send_header(get());
    }
    
    // 设置响应头
    get()->headers_out.status = NGX_HTTP_OK;
    get()->headers_out.content_length_n = of.size;
    get()->headers_out.last_modified_time = of.mtime;
    
    // 根据文件扩展名设置内容类型
    std::string ext;
    size_t dot_pos = file_path.find_last_of('.');
    
    if (dot_pos != std::string::npos) {
        ext = file_path.substr(dot_pos + 1);
        std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
    }
    
    ngx_str_t content_type = ngx_string("application/octet-stream"); // 默认类型
    
    if (ext == "html" || ext == "htm") {
        content_type = ngx_string("text/html");
    } else if (ext == "css") {
        content_type = ngx_string("text/css");
    } else if (ext == "js") {
        content_type = ngx_string("application/javascript");
    } else if (ext == "json") {
        content_type = ngx_string("application/json");
    } else if (ext == "jpg" || ext == "jpeg") {
        content_type = ngx_string("image/jpeg");
    } else if (ext == "png") {
        content_type = ngx_string("image/png");
    } else if (ext == "gif") {
        content_type = ngx_string("image/gif");
    } else if (ext == "svg") {
        content_type = ngx_string("image/svg+xml");
    } else if (ext == "pdf") {
        content_type = ngx_string("application/pdf");
    } else if (ext == "txt") {
        content_type = ngx_string("text/plain");
    }
    
    get()->headers_out.content_type = content_type;
    get()->headers_out.content_type_len = content_type.len;
    
    // 发送响应头
    ngx_int_t rc = ngx_http_send_header(get());
    if (rc == NGX_ERROR || rc > NGX_OK || get()->header_only) {
        return rc;
    }
    
    // 创建缓冲区
    ngx_buf_t* b = reinterpret_cast<ngx_buf_t*>(ngx_palloc(get()->pool, sizeof(ngx_buf_t)));
    if (b == nullptr) {
        return NGX_HTTP_INTERNAL_SERVER_ERROR;
    }
    
    ngx_memzero(b, sizeof(ngx_buf_t));
    
    b->file = reinterpret_cast<ngx_file_t*>(ngx_palloc(get()->pool, sizeof(ngx_file_t)));
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
    b->file->log = get()->connection->log;
    
    // 创建输出链
    ngx_chain_t out;
    out.buf = b;
    out.next = nullptr;
    
    // 发送文件
    return ngx_http_output_filter(get(), &out);
}

// 发送错误响应
ngx_int_t NgxRequest::send_error(ngx_uint_t status_code, const std::string& message) {
    if (!get()) return NGX_ERROR;
    
    // 如果提供了错误消息，则发送包含错误信息的HTML
    if (!message.empty()) {
        std::string html = "<html><head><title>Error</title></head><body><h1>Error ";
        html += std::to_string(status_code);
        html += "</h1><p>";
        html += message;
        html += "</p></body></html>";
        
        return send_html(html, status_code);
    }
    
    // 否则简单发送状态码
    get()->headers_out.status = status_code;
    
    // 如果是标准错误码，让nginx处理
    return ngx_http_send_special(get(), NGX_HTTP_LAST);
}

// 添加响应头
bool NgxRequest::add_header(const std::string& name, const std::string& value) {
    if (!get()) return false;
    
    ngx_table_elt_t* h = reinterpret_cast<ngx_table_elt_t*>(
        ngx_list_push(&get()->headers_out.headers)
    );
    
    if (h == nullptr) {
        return false;
    }
    
    h->hash = 1; // 标记为有效
    
    // 分配并设置名称
    h->key.len = name.length();
    h->key.data = reinterpret_cast<u_char*>(ngx_palloc(get()->pool, name.length()));
    if (h->key.data == nullptr) {
        return false;
    }
    ngx_memcpy(h->key.data, name.c_str(), name.length());
    
    // 分配并设置值
    h->value.len = value.length();
    h->value.data = reinterpret_cast<u_char*>(ngx_palloc(get()->pool, value.length()));
    if (h->value.data == nullptr) {
        return false;
    }
    ngx_memcpy(h->value.data, value.c_str(), value.length());
    
    return true;
}

// 设置内容类型
bool NgxRequest::set_content_type(const std::string& content_type) {
    if (!get()) return false;
    
    // 设置Content-Type
    ngx_str_t type;
    type.len = content_type.length();
    type.data = reinterpret_cast<u_char*>(ngx_palloc(get()->pool, type.len));
    
    if (type.data == nullptr) {
        return false;
    }
    
    ngx_memcpy(type.data, content_type.c_str(), type.len);
    
    get()->headers_out.content_type = type;
    get()->headers_out.content_type_len = type.len;
    
    return true;
}

// 获取客户端IP
std::string NgxRequest::get_client_ip() const {
    if (!get() || !get()->connection) {
        return "";
    }
    
    // 尝试获取X-Forwarded-For
    auto forwarded = get_header("X-Forwarded-For");
    if (forwarded) {
        // 解析第一个IP (可能有多个，用逗号分隔)
        size_t pos = forwarded->find(',');
        if (pos != std::string::npos) {
            return forwarded->substr(0, pos);
        }
        return *forwarded;
    }
    
    // 如果没有X-Forwarded-For，使用socket IP
    return std::string(
        reinterpret_cast<const char*>(get()->connection->addr_text.data), 
        get()->connection->addr_text.len
    );
} 