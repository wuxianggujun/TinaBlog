#pragma once
#include "NgxPtr.hpp"
#include <string>
#include <string_view>
#include <cstring>


class NgxPool;  // 前向声明

/**
 * @brief NgxString类封装了ngx_str_t，提供了现代C++接口
 * 
 * 内存管理说明：
 * 1. NgxString不负责内存的释放，内存由NgxPool管理
 * 2. 当pool被销毁时，所有通过pool分配的内存都会被自动释放
 * 3. 这符合Nginx的设计理念：请求相关的内存随请求结束而释放
 */
class NgxString : public NgxPtr<ngx_str_t> {
public:
    // 继承基类的构造函数
    using NgxPtr<ngx_str_t>::NgxPtr;
    
    // 自定义构造函数：从ngx_str_t引用构造（浅拷贝）
    explicit NgxString(const ngx_str_t& str, ngx_pool_t* pool) {
        ptr_ = allocate(pool, str.len);
        if (ptr_) {
            ngx_memcpy(ptr_->data, str.data, str.len);
        }
    }
    
    // 自定义构造函数：直接从数据和长度构造
    NgxString(const u_char* data, size_t len, ngx_pool_t* pool) {
        ptr_ = allocate(pool, len);
        if (ptr_ && data) {
            ngx_memcpy(ptr_->data, data, len);
        }
    }
    
    // 工厂方法：从std::string创建并分配内存
    static NgxString create(const std::string& str, ngx_pool_t* pool);
    
    // 工厂方法：从C字符串创建并分配内存
    static NgxString create(const char* str, ngx_pool_t* pool);
    
    // 工厂方法：从string_view创建并分配内存
    static NgxString create(std::string_view sv, ngx_pool_t* pool);
    
    // 转换为std::string_view (零拷贝)
    [[nodiscard]] std::string_view view() const noexcept {
        return ptr_ ? std::string_view(reinterpret_cast<const char*>(ptr_->data), ptr_->len) 
                   : std::string_view();
    }
    
    // 转换为std::string (会发生拷贝)
    [[nodiscard]] std::string str() const {
        return ptr_ ? std::string(reinterpret_cast<const char*>(ptr_->data), ptr_->len) 
                   : std::string();
    }
    
    // 获取长度
    [[nodiscard]] size_t length() const noexcept { return ptr_ ? ptr_->len : 0; }
    
    // 判断是否为空
    [[nodiscard]] bool empty() const noexcept { return !ptr_ || ptr_->len == 0; }
    
    // 比较操作符
    bool operator==(const NgxString& other) const noexcept;
    bool operator!=(const NgxString& other) const noexcept;
    
    // 从pool中创建新的ngx_str_t并分配内存
    static ngx_str_t* allocate(ngx_pool_t* pool, size_t len);
    
    // 追加字符串
    bool append(const NgxString& other, NgxPool& pool);
    bool append(const char* str, NgxPool& pool);
    bool append(const std::string& str, NgxPool& pool);
    
    // 子字符串
    NgxString substr(size_t pos, size_t len, NgxPool& pool) const;
    
    // 查找
    [[nodiscard]] size_t find(const NgxString& str, size_t pos = 0) const noexcept;
    size_t find(const char* str, size_t pos = 0) const noexcept;
    
private:
    friend class NgxPool;
}; // class NgxString
