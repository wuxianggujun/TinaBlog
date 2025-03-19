#ifndef NGINX_NGX_PTR_HPP
#define NGINX_NGX_PTR_HPP

#include "Nginx.hpp"
/**
 * @brief Nginx指针包装器的基类
 * 
 * @tparam T Nginx数据结构类型（如ngx_str_t, ngx_array_t等）
 * 
 * 这个类提供了：
 * 1. 统一的指针管理接口
 * 2. 移动语义支持
 * 3. 禁用拷贝操作
 * 4. 基本的有效性检查
 */
template<typename T>
class NgxPtr {
public:
    // 默认构造函数
    NgxPtr() noexcept : ptr_(nullptr) {}
    
    // 从原始指针构造
    explicit NgxPtr(T* ptr) noexcept : ptr_(ptr) {}
    
    // 移动构造函数
    NgxPtr(NgxPtr&& other) noexcept : ptr_(other.ptr_) {
        other.ptr_ = nullptr;
    }
    
    // 移动赋值运算符
    NgxPtr& operator=(NgxPtr&& other) noexcept {
        if (this != &other) {
            ptr_ = other.ptr_;
            other.ptr_ = nullptr;
        }
        return *this;
    }
    
    // 禁用拷贝
    NgxPtr(const NgxPtr&) = delete;
    NgxPtr& operator=(const NgxPtr&) = delete;
    
    // 析构函数 - 内存由pool管理，不需要手动释放
    virtual ~NgxPtr() = default;
    
    // 获取原始指针
    const T* get() const noexcept { return ptr_; }
    T* get() noexcept { return ptr_; }
    
    // 检查指针是否有效
    [[nodiscard]] bool valid() const noexcept { return ptr_ != nullptr; }
    
    // 重置指针
    void reset() noexcept { ptr_ = nullptr; }
    
    // 解引用操作符
    const T& operator*() const noexcept { return *ptr_; }
    T& operator*() noexcept { return *ptr_; }
    
    // 箭头操作符
    const T* operator->() const noexcept { return ptr_; }
    T* operator->() noexcept { return ptr_; }
    
    // 布尔转换操作符
    explicit operator bool() const noexcept { return valid(); }

protected:
    T* ptr_;  // 由Nginx内存池管理的指针
}; // class NgxPtr

#endif // NGINX_NGX_PTR_HPP