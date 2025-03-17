//
// Created by wuxianggujun on 2025/3/25.
//

#ifndef TINA_BLOG_NGX_CHAIN_HPP
#define TINA_BLOG_NGX_CHAIN_HPP

#include "Nginx.hpp"
#include "NginxContext.hpp"
#include "NgxBuf.hpp"
#include <vector>

/**
 * @brief Nginx缓冲区链封装类
 * 
 * 封装ngx_chain_t结构，提供链管理功能
 */
class NgxChain : public NginxContext<ngx_chain_t> {
public:
    /**
     * @brief 从现有的ngx_chain_t构造
     * 
     * @param chain Nginx链指针
     * @param owns_memory 是否拥有内存(默认为false)
     */
    explicit NgxChain(ngx_chain_t* chain, bool owns_memory = false)
        : NginxContext<ngx_chain_t>(chain), owns_memory_(owns_memory) {}
    
    /**
     * @brief 基于内存池创建新的链节点
     * 
     * @param pool Nginx内存池
     * @param buf 缓冲区对象
     * @return NgxChain 新创建的链节点
     */
    static NgxChain create(ngx_pool_t* pool, const NgxBuf& buf) {
        ngx_chain_t* chain = static_cast<ngx_chain_t*>(ngx_palloc(pool, sizeof(ngx_chain_t)));
        if (!chain) {
            return NgxChain(nullptr);
        }
        
        chain->buf = buf.get();
        chain->next = nullptr;
        
        return NgxChain(chain);
    }
    
    /**
     * @brief 从多个缓冲区创建链
     * 
     * @param pool Nginx内存池
     * @param bufs 缓冲区数组
     * @return NgxChain 链的头节点
     */
    static NgxChain create_chain(ngx_pool_t* pool, const std::vector<NgxBuf>& bufs) {
        if (bufs.empty()) {
            return NgxChain(nullptr);
        }
        
        // 创建头节点
        ngx_chain_t* head = static_cast<ngx_chain_t*>(ngx_palloc(pool, sizeof(ngx_chain_t)));
        if (!head) {
            return NgxChain(nullptr);
        }
        
        head->buf = bufs[0].get();
        head->next = nullptr;
        
        // 构建链
        ngx_chain_t* prev = head;
        for (size_t i = 1; i < bufs.size(); ++i) {
            ngx_chain_t* curr = static_cast<ngx_chain_t*>(ngx_palloc(pool, sizeof(ngx_chain_t)));
            if (!curr) {
                // 注意：这里可能会有内存泄漏，但Nginx的内存池会在请求结束时释放
                return NgxChain(head);
            }
            
            curr->buf = bufs[i].get();
            curr->next = nullptr;
            
            prev->next = curr;
            prev = curr;
        }
        
        return NgxChain(head);
    }
    
    /**
     * @brief 设置链中最后一个缓冲区
     * 
     * 遍历链，将最后一个节点的缓冲区标记为last_buf和last_in_chain
     * 
     * @return NgxChain& 返回自身引用
     */
    NgxChain& set_last_buf() {
        if (!this->ptr_) {
            return *this;
        }
        
        // 找到最后一个节点
        ngx_chain_t* last = this->ptr_;
        while (last->next) {
            last = last->next;
        }
        
        // 设置为最后一个缓冲区
        if (last->buf) {
            last->buf->last_buf = 1;
            last->buf->last_in_chain = 1;
        }
        
        return *this;
    }
    
    /**
     * @brief 获取链中的节点数量
     * 
     * @return size_t 节点数量
     */
    size_t size() const {
        if (!this->ptr_) {
            return 0;
        }
        
        size_t count = 0;
        ngx_chain_t* curr = this->ptr_;
        while (curr) {
            ++count;
            curr = curr->next;
        }
        
        return count;
    }
    
    /**
     * @brief 链接另一个链到当前链的末尾
     * 
     * @param other 要链接的链
     * @return NgxChain& 返回自身引用
     */
    NgxChain& append(const NgxChain& other) {
        if (!this->ptr_ || !other.ptr_) {
            return *this;
        }
        
        // 找到最后一个节点
        ngx_chain_t* last = this->ptr_;
        while (last->next) {
            last = last->next;
        }
        
        // 链接
        last->next = other.ptr_;
        
        return *this;
    }
    
    /**
     * @brief 析构函数
     */
    ~NgxChain() {
        cleanup_impl();
    }
    
    /**
     * @brief 移动构造函数
     * 
     * @param other 源对象
     */
    NgxChain(NgxChain&& other) noexcept
        : NginxContext<ngx_chain_t>(other.ptr_, other.owns_ptr_),
          owns_memory_(other.owns_memory_)
    {
        other.ptr_ = nullptr;
        other.owns_ptr_ = false;
        other.owns_memory_ = false;
    }
    
    /**
     * @brief 移动赋值操作符
     * 
     * @param other 源对象
     * @return NgxChain& 自身引用
     */
    NgxChain& operator=(NgxChain&& other) noexcept {
        if (this != &other) {
            this->cleanup_impl();
            
            this->ptr_ = other.ptr_;
            this->owns_ptr_ = other.owns_ptr_;
            this->owns_memory_ = other.owns_memory_;
            
            other.ptr_ = nullptr;
            other.owns_ptr_ = false;
            other.owns_memory_ = false;
        }
        return *this;
    }
    
    // 禁止复制
    NgxChain(const NgxChain&) = delete;
    NgxChain& operator=(const NgxChain&) = delete;
    
private:
    /**
     * @brief 资源清理实现
     */
    inline void cleanup_impl() noexcept {
        if (owns_memory_ && this->ptr_) {
            // 注意：这里不会释放缓冲区，缓冲区应该由NgxBuf管理
            // 递归释放链
            ngx_chain_t* curr = this->ptr_;
            while (curr) {
                ngx_chain_t* next = curr->next;
                delete curr;
                curr = next;
            }
        }
        
        this->ptr_ = nullptr;
        this->owns_ptr_ = false;
        owns_memory_ = false;
    }
    
    bool owns_memory_ = false;  ///< 是否拥有链结构内存
};

#endif // TINA_BLOG_NGX_CHAIN_HPP 