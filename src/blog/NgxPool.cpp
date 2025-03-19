#include "NgxPool.hpp"
#include "NgxString.hpp"


NgxString NgxPool::create_string(const char* str) {
    if (!str || !ptr_) {
        return NgxString();
    }
    
    size_t len = std::strlen(str);
    if (len == 0) {
        return NgxString();
    }
    
    // 分配ngx_str_t结构体
    auto* ngx_str = static_cast<ngx_str_t*>(ngx_pcalloc(ptr_, sizeof(ngx_str_t)));
    if (!ngx_str) {
        return NgxString();
    }
    
    // 分配数据缓冲区
    ngx_str->data = static_cast<u_char*>(ngx_pnalloc(ptr_, len));
    if (!ngx_str->data) {
        return NgxString();
    }
    
    ngx_memcpy(ngx_str->data, str, len);
    ngx_str->len = len;
    
    return NgxString(ngx_str);
}

NgxString NgxPool::create_string(const std::string& str) {
    if (str.empty() || !ptr_) {
        return NgxString();
    }
    
    // 分配ngx_str_t结构体
    auto* ngx_str = static_cast<ngx_str_t*>(ngx_pcalloc(ptr_, sizeof(ngx_str_t)));
    if (!ngx_str) {
        return NgxString();
    }
    
    // 分配数据缓冲区
    ngx_str->data = static_cast<u_char*>(ngx_pnalloc(ptr_, str.length()));
    if (!ngx_str->data) {
        return NgxString();
    }
    
    ngx_memcpy(ngx_str->data, str.data(), str.length());
    ngx_str->len = str.length();
    
    return NgxString(ngx_str);
}

NgxString NgxPool::create_string(std::string_view sv) {
    if (sv.empty() || !ptr_) {
        return NgxString();
    }
    
    // 分配ngx_str_t结构体
    auto* ngx_str = static_cast<ngx_str_t*>(ngx_pcalloc(ptr_, sizeof(ngx_str_t)));
    if (!ngx_str) {
        return NgxString();
    }
    
    // 分配数据缓冲区
    ngx_str->data = static_cast<u_char*>(ngx_pnalloc(ptr_, sv.length()));
    if (!ngx_str->data) {
        return NgxString();
    }
    
    ngx_memcpy(ngx_str->data, sv.data(), sv.length());
    ngx_str->len = sv.length();
    
    return NgxString(ngx_str);
}

NgxString NgxPool::create_string(const u_char* data, size_t len) {
    if (!data || len == 0 || !ptr_) {
        return NgxString();
    }
    
    // 分配ngx_str_t结构体
    auto* ngx_str = static_cast<ngx_str_t*>(ngx_pcalloc(ptr_, sizeof(ngx_str_t)));
    if (!ngx_str) {
        return NgxString();
    }
    
    // 分配数据缓冲区
    ngx_str->data = static_cast<u_char*>(ngx_pnalloc(ptr_, len));
    if (!ngx_str->data) {
        return NgxString();
    }
    
    ngx_memcpy(ngx_str->data, data, len);
    ngx_str->len = len;
    
    return NgxString(ngx_str);
}