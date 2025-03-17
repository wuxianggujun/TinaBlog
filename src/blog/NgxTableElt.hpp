//
// Created by wuxianggujun on 2025/3/25.
//

#ifndef TINA_BLOG_NGX_TABLE_ELT_HPP
#define TINA_BLOG_NGX_TABLE_ELT_HPP

#include "Nginx.hpp"
#include "NginxContext.hpp"
#include "NgxPool.hpp"
#include "NgxString.hpp"
#include <string>

/**
 * @brief Nginx表元素封装类
 * 
 * 封装ngx_table_elt_t，用于HTTP头部等表格式数据
 */
class NgxTableElt : public NginxContext<ngx_table_elt_t> {
public:
    /**
     * @brief 从现有的ngx_table_elt_t构造
     * 
     * @param elt 表元素指针
     */
    explicit NgxTableElt(ngx_table_elt_t* elt) : NginxContext<ngx_table_elt_t>(elt) {}
    
    /**
     * @brief 在内存池中创建新的表元素
     * 
     * @param pool 内存池
     * @return NgxTableElt 新创建的表元素
     */
    static NgxTableElt create(const NgxPool& pool) {
        ngx_table_elt_t* elt = static_cast<ngx_table_elt_t*>(pool.pcalloc(sizeof(ngx_table_elt_t)));
        return NgxTableElt(elt);
    }
    
    /**
     * @brief 在请求的头部列表中创建新表元素
     * 
     * @param r 请求对象
     * @return NgxTableElt 新创建的表元素
     */
    static NgxTableElt create_header(ngx_http_request_t* r) {
        ngx_table_elt_t* h = static_cast<ngx_table_elt_t*>(
            ngx_list_push(&r->headers_out.headers));
        return NgxTableElt(h);
    }
    
    /**
     * @brief 设置键
     * 
     * @param key 键字符串
     * @param pool 内存池
     * @return NgxTableElt& 返回自身引用
     */
    NgxTableElt& set_key(const std::string& key, const NgxPool& pool) {
        if (!this->ptr_) {
            return *this;
        }
        
        this->ptr_->key.data = pool.strdup(key);
        if (this->ptr_->key.data) {
            this->ptr_->key.len = key.length();
        }
        
        return *this;
    }
    
    /**
     * @brief 直接设置键（不复制，直接使用指针）
     * 
     * @param key 键字符串
     * @param len 键长度
     * @return NgxTableElt& 返回自身引用
     */
    NgxTableElt& set_key_direct(u_char* key, size_t len) {
        if (!this->ptr_) {
            return *this;
        }
        
        this->ptr_->key.data = key;
        this->ptr_->key.len = len;
        
        return *this;
    }
    
    /**
     * @brief 设置值
     * 
     * @param value 值字符串
     * @param pool 内存池
     * @return NgxTableElt& 返回自身引用
     */
    NgxTableElt& set_value(const std::string& value, const NgxPool& pool) {
        if (!this->ptr_) {
            return *this;
        }
        
        this->ptr_->value.data = pool.strdup(value);
        if (this->ptr_->value.data) {
            this->ptr_->value.len = value.length();
        }
        
        return *this;
    }
    
    /**
     * @brief 直接设置值（不复制，直接使用指针）
     * 
     * @param value 值字符串
     * @param len 值长度
     * @return NgxTableElt& 返回自身引用
     */
    NgxTableElt& set_value_direct(u_char* value, size_t len) {
        if (!this->ptr_) {
            return *this;
        }
        
        this->ptr_->value.data = value;
        this->ptr_->value.len = len;
        
        return *this;
    }
    
    /**
     * @brief 设置哈希标记
     * 
     * @param hash 哈希值(默认为1)
     * @return NgxTableElt& 返回自身引用
     */
    NgxTableElt& set_hash(ngx_uint_t hash = 1) {
        if (this->ptr_) {
            this->ptr_->hash = hash;
        }
        return *this;
    }
    
    /**
     * @brief 获取键字符串
     * 
     * @return std::string 键字符串
     */
    std::string key() const {
        if (!this->ptr_ || !this->ptr_->key.data) {
            return "";
        }
        
        return std::string(reinterpret_cast<char*>(this->ptr_->key.data), this->ptr_->key.len);
    }
    
    /**
     * @brief 获取值字符串
     * 
     * @return std::string 值字符串
     */
    std::string value() const {
        if (!this->ptr_ || !this->ptr_->value.data) {
            return "";
        }
        
        return std::string(reinterpret_cast<char*>(this->ptr_->value.data), this->ptr_->value.len);
    }
    
    /**
     * @brief 获取键为NgxString
     * 
     * @return NgxString 键字符串
     */
    NgxString key_str() const {
        if (!this->ptr_) {
            return NgxString();
        }
        
        return NgxString(this->ptr_->key, false);
    }
    
    /**
     * @brief 获取值为NgxString
     * 
     * @return NgxString 值字符串
     */
    NgxString value_str() const {
        if (!this->ptr_) {
            return NgxString();
        }
        
        return NgxString(this->ptr_->value, false);
    }
};

#endif // TINA_BLOG_NGX_TABLE_ELT_HPP 