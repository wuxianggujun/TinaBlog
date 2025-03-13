//
// Created by wuxianggujun on 2025/3/13.
//

#ifndef TINA_BLOG_NGINX_EXTENSIONS_HPP
#define TINA_BLOG_NGINX_EXTENSIONS_HPP

#include "Nginx.hpp"
#include <cstdarg>
#include <cstring>

/**
 * 本文件提供了对Nginx API的扩展和兼容性函数
 */

// 安全复制const字符串的函数
inline u_char* ngx_pstrdup_const(ngx_pool_t* pool, const ngx_str_t* src) {
    if (!pool || !src || !src->data) {
        return nullptr;
    }
    
    ngx_str_t temp;
    temp.len = src->len;
    temp.data = src->data;
    return ngx_pstrdup(pool, &temp);
}

// 从C字符串复制到ngx_str_t的函数
inline u_char* ngx_pstrdup_from_cstr(ngx_pool_t* pool, const char* src, size_t len = 0) {
    if (!pool || !src) {
        return nullptr;
    }
    
    size_t real_len = len ? len : strlen(src);
    u_char* dst = static_cast<u_char*>(ngx_pnalloc(pool, real_len + 1));
    if (!dst) {
        return nullptr;
    }
    
    ngx_memcpy(dst, src, real_len);
    dst[real_len] = '\0';  // 确保字符串以NULL结尾
    
    return dst;
}

// 设置ngx_str_t的辅助函数 - 重命名以避免与Nginx宏冲突
inline void ngx_string_set(ngx_str_t* str, const char* text) {
    if (!str || !text) {
        return;
    }
    
    str->len = strlen(text);
    str->data = reinterpret_cast<u_char*>(const_cast<char*>(text));
}

// 安全分配并初始化结构体
template <typename T>
inline T* ngx_pcalloc_safe(ngx_pool_t* pool) {
    if (!pool) {
        return nullptr;
    }
    
    return static_cast<T*>(ngx_pcalloc(pool, sizeof(T)));
}

// 格式化到ngx_str_t的辅助函数
inline ngx_int_t ngx_str_format(ngx_pool_t* pool, ngx_str_t* dst, const char* fmt, ...) {
    if (!pool || !dst || !fmt) {
        return NGX_ERROR;
    }
    
    va_list args;
    va_start(args, fmt);
    
    // 计算格式化后的字符串长度
    va_list args_copy;
    va_copy(args_copy, args);
    size_t len = vsnprintf(nullptr, 0, fmt, args_copy) + 1;  // +1 for null terminator
    va_end(args_copy);
    
    // 分配内存
    u_char* p = static_cast<u_char*>(ngx_pnalloc(pool, len));
    if (!p) {
        va_end(args);
        return NGX_ERROR;
    }
    
    // 格式化字符串
    vsnprintf(reinterpret_cast<char*>(p), len, fmt, args);
    va_end(args);
    
    dst->data = p;
    dst->len = len - 1;  // exclude null terminator
    
    return NGX_OK;
}

#endif // TINA_BLOG_NGINX_EXTENSIONS_HPP 