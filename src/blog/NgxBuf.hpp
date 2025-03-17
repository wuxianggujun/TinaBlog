//
// Created by wuxianggujun on 2025/3/25.
//

#ifndef TINA_BLOG_NGX_BUF_HPP
#define TINA_BLOG_NGX_BUF_HPP

#include "Nginx.hpp"
#include "NginxContext.hpp"
#include <string>
#include <vector>

/**
 * @brief Nginx缓冲区封装类
 * 
 * 封装ngx_buf_t结构，提供高级操作接口
 */
class NgxBuf : public NginxContext<ngx_buf_t> {
public:
    /**
     * @brief 从现有的ngx_buf_t构造
     * 
     * @param buf Nginx缓冲区指针
     * @param owns_memory 是否拥有内存(默认为false)
     */
    explicit NgxBuf(ngx_buf_t* buf, bool owns_memory = false) 
        : NginxContext<ngx_buf_t>(buf), owns_memory_(owns_memory), own_data_(nullptr) {}
    
    /**
     * @brief 基于内存池创建新的空缓冲区
     * 
     * @param pool Nginx内存池
     * @return NgxBuf 新创建的缓冲区对象
     */
    static NgxBuf create(ngx_pool_t* pool) {
        ngx_buf_t* buf = static_cast<ngx_buf_t*>(ngx_pcalloc(pool, sizeof(ngx_buf_t)));
        return NgxBuf(buf);
    }
    
    /**
     * @brief 基于内存池创建包含字符串内容的缓冲区
     * 
     * @param pool Nginx内存池
     * @param content 字符串内容
     * @return NgxBuf 新创建的缓冲区对象
     */
    static NgxBuf create_with_string(ngx_pool_t* pool, const std::string& content) {
        // 分配缓冲区结构
        ngx_buf_t* buf = static_cast<ngx_buf_t*>(ngx_pcalloc(pool, sizeof(ngx_buf_t)));
        if (!buf) {
            return NgxBuf(nullptr);
        }
        
        // 分配内容内存
        if (!content.empty()) {
            u_char* data = static_cast<u_char*>(ngx_palloc(pool, content.length()));
            if (!data) {
                return NgxBuf(nullptr);
            }
            
            // 复制内容
            ngx_memcpy(data, content.c_str(), content.length());
            
            // 设置缓冲区
            buf->pos = data;
            buf->last = data + content.length();
            buf->memory = 1;  // 这是内存缓冲区
        }
        
        return NgxBuf(buf);
    }
    
    /**
     * @brief 创建内存中的缓冲区(不依赖Nginx内存池)
     * 
     * @param content 字符串内容
     * @return NgxBuf 新创建的缓冲区对象
     */
    static NgxBuf create_in_memory(const std::string& content) {
        // 分配缓冲区结构
        ngx_buf_t* buf = new ngx_buf_t();
        std::memset(buf, 0, sizeof(ngx_buf_t));
        
        // 分配内容内存并复制数据
        u_char* data = nullptr;
        if (!content.empty()) {
            data = new u_char[content.length()];
            std::memcpy(data, content.c_str(), content.length());
            
            // 设置缓冲区
            buf->pos = data;
            buf->last = data + content.length();
            buf->memory = 1;  // 这是内存缓冲区
        }
        
        // 返回拥有内存所有权的NgxBuf
        NgxBuf result(buf, true);
        result.own_data_ = data;
        return result;
    }
    
    /**
     * @brief 将缓冲区设置为最后一个缓冲区
     * 
     * @param is_last 是否为最后一个(默认为true)
     * @return NgxBuf& 返回自身引用，支持链式调用
     */
    NgxBuf& set_last_buf(bool is_last = true) {
        if (this->ptr_) {
            this->ptr_->last_buf = is_last ? 1 : 0;
        }
        return *this;
    }
    
    /**
     * @brief 将缓冲区设置为链中最后一个
     * 
     * @param is_last 是否为最后一个(默认为true)
     * @return NgxBuf& 返回自身引用，支持链式调用
     */
    NgxBuf& set_last_in_chain(bool is_last = true) {
        if (this->ptr_) {
            this->ptr_->last_in_chain = is_last ? 1 : 0;
        }
        return *this;
    }
    
    /**
     * @brief 将缓冲区设置为临时缓冲区
     * 
     * @param is_temp 是否为临时缓冲区(默认为true)
     * @return NgxBuf& 返回自身引用，支持链式调用
     */
    NgxBuf& set_temp_file(bool is_temp = true) {
        if (this->ptr_) {
            this->ptr_->temp_file = is_temp ? 1 : 0;
        }
        return *this;
    }
    
    /**
     * @brief 将缓冲区设置为内存缓冲区
     * 
     * @param is_memory 是否为内存缓冲区(默认为true)
     * @return NgxBuf& 返回自身引用，支持链式调用
     */
    NgxBuf& set_memory(bool is_memory = true) {
        if (this->ptr_) {
            this->ptr_->memory = is_memory ? 1 : 0;
        }
        return *this;
    }
    
    /**
     * @brief 将缓冲区设置为同步缓冲区
     * 
     * @param is_sync 是否为同步缓冲区(默认为true)
     * @return NgxBuf& 返回自身引用，支持链式调用
     */
    NgxBuf& set_sync(bool is_sync = true) {
        if (this->ptr_) {
            this->ptr_->sync = is_sync ? 1 : 0;
        }
        return *this;
    }
    
    /**
     * @brief 获取缓冲区内容长度
     * 
     * @return size_t 内容长度
     */
    size_t size() const {
        if (!this->ptr_ || !this->ptr_->pos || !this->ptr_->last) {
            return 0;
        }
        return this->ptr_->last - this->ptr_->pos;
    }
    
    /**
     * @brief 获取缓冲区内容字符串
     * 
     * @return std::string 内容字符串
     */
    std::string to_string() const {
        if (!this->ptr_ || !this->ptr_->pos || !this->ptr_->last) {
            return "";
        }
        
        return std::string(reinterpret_cast<const char*>(this->ptr_->pos), 
                          this->ptr_->last - this->ptr_->pos);
    }
    
    /**
     * @brief 获取缓冲区开始位置
     * 
     * @return u_char* 缓冲区开始位置
     */
    [[nodiscard]] u_char* data() const {
        return this->ptr_ ? this->ptr_->pos : nullptr;
    }
    
    /**
     * @brief 创建空缓冲区(用于表示响应结束)
     * 
     * @param pool Nginx内存池
     * @return NgxBuf 空缓冲区对象
     */
    static NgxBuf create_empty_buf(ngx_pool_t* pool) {
        ngx_buf_t* b = static_cast<ngx_buf_t*>(ngx_pcalloc(pool, sizeof(ngx_buf_t)));
        if (!b) {
            return NgxBuf(nullptr);
        }
        
        b->last_buf = 1;      // 标记为最后一个缓冲区
        b->last_in_chain = 1; // 标记为链中最后一个
        b->pos = b->last = nullptr; // 明确指示这是一个零大小的缓冲区
        b->sync = 1;          // 标记为同步操作
        
        return NgxBuf(b);
    }
    
    /**
     * @brief 创建ngx_chain_t并关联到此缓冲区
     * 
     * @param pool Nginx内存池
     * @return ngx_chain_t* 新创建的链节点
     */
    ngx_chain_t* create_chain(ngx_pool_t* pool) const {
        if (!this->ptr_) {
            return nullptr;
        }
        
        ngx_chain_t* out = static_cast<ngx_chain_t*>(ngx_palloc(pool, sizeof(ngx_chain_t)));
        if (!out) {
            return nullptr;
        }
        
        out->buf = this->ptr_;
        out->next = nullptr;
        
        return out;
    }
    
    /**
     * @brief 析构函数
     * 
     * 处理自己分配的内存资源释放
     */
    ~NgxBuf() {
        cleanup_impl();
    }
    
    /**
     * @brief 移动构造函数
     * 
     * @param other 源对象
     */
    NgxBuf(NgxBuf&& other) noexcept 
        : NginxContext<ngx_buf_t>(other.ptr_, other.owns_ptr_),
          owns_memory_(other.owns_memory_),
          own_data_(other.own_data_)
    {
        other.ptr_ = nullptr;
        other.owns_ptr_ = false;
        other.owns_memory_ = false;
        other.own_data_ = nullptr;
    }
    
    /**
     * @brief 移动赋值操作符
     * 
     * @param other 源对象
     * @return NgxBuf& 自身引用
     */
    NgxBuf& operator=(NgxBuf&& other) noexcept {
        if (this != &other) {
            this->cleanup_impl();
            
            this->ptr_ = other.ptr_;
            this->owns_ptr_ = other.owns_ptr_;
            this->owns_memory_ = other.owns_memory_;
            this->own_data_ = other.own_data_;
            
            other.ptr_ = nullptr;
            other.owns_ptr_ = false;
            other.owns_memory_ = false;
            other.own_data_ = nullptr;
        }
        return *this;
    }
    
    // 禁止复制
    NgxBuf(const NgxBuf&) = delete;
    NgxBuf& operator=(const NgxBuf&) = delete;
    
private:
    /**
     * @brief 资源清理实现
     * 
     * 清理自己持有的资源
     */
    inline void cleanup_impl() noexcept {
        if (owns_memory_ && this->ptr_) {
            delete this->ptr_;
            this->ptr_ = nullptr;
        }
        
        if (own_data_) {
            delete[] own_data_;
            own_data_ = nullptr;
        }
        
        this->owns_ptr_ = false;
        owns_memory_ = false;
    }
    
    bool owns_memory_ = false;  ///< 是否拥有缓冲区结构内存
    u_char* own_data_ = nullptr; ///< 自己分配的数据内存
};

#endif // TINA_BLOG_NGX_BUF_HPP 