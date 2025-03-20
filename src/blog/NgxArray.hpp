#pragma once
#include "NgxPtr.hpp"
#include <type_traits>
#include <vector>
#include <optional>
#include <stdexcept>

/**
 * @brief Nginx数组包装类
 * 
 * @tparam T 数组元素类型
 */
template<typename T>
class NgxArray : public NgxPtr<ngx_array_t> {
public:
    using value_type = T;
    using pointer = T*;
    using const_pointer = const T*;
    using reference = T&;
    using const_reference = const T&;
    using size_type = ngx_uint_t;
    using iterator = T*;
    using const_iterator = const T*;
    
    // 使用基类的构造函数
    using NgxPtr<ngx_array_t>::NgxPtr;
    
    // 创建数组
    static NgxArray<T> create(ngx_pool_t* pool, ngx_uint_t n) {
        return NgxArray<T>(ngx_array_create(pool, n, sizeof(T)));
    }
    
    // 初始化现有数组
    bool init(ngx_pool_t* pool, ngx_uint_t n) {
        return ngx_array_init(ptr_, pool, n, sizeof(T)) == NGX_OK;
    }
    
    // 获取元素数量
    [[nodiscard]] size_type size() const noexcept {
        return ptr_ ? ptr_->nelts : 0;
    }
    
    // 获取分配的容量
    [[nodiscard]] size_type capacity() const noexcept {
        return ptr_ ? ptr_->nalloc : 0;
    }
    
    // 是否为空
    [[nodiscard]] bool empty() const noexcept {
        return size() == 0;
    }
    
    // 获取元素
    [[nodiscard]] const_reference operator[](size_type i) const {
        if (!ptr_ || i >= size()) {
            throw std::out_of_range("Array index out of range");
        }
        return static_cast<const_pointer>(ptr_->elts)[i];
    }
    
    // 获取元素(可修改)
    [[nodiscard]] reference operator[](size_type i) {
        if (!ptr_ || i >= size()) {
            throw std::out_of_range("Array index out of range");
        }
        return static_cast<pointer>(ptr_->elts)[i];
    }
    
    // 获取第一个元素
    [[nodiscard]] const_reference front() const {
        if (!ptr_ || empty()) {
            throw std::out_of_range("Array is empty");
        }
        return static_cast<const_pointer>(ptr_->elts)[0];
    }
    
    // 获取第一个元素(可修改)
    [[nodiscard]] reference front() {
        if (!ptr_ || empty()) {
            throw std::out_of_range("Array is empty");
        }
        return static_cast<pointer>(ptr_->elts)[0];
    }
    
    // 获取最后一个元素
    [[nodiscard]] const_reference back() const {
        if (!ptr_ || empty()) {
            throw std::out_of_range("Array is empty");
        }
        return static_cast<const_pointer>(ptr_->elts)[size() - 1];
    }
    
    // 获取最后一个元素(可修改)
    [[nodiscard]] reference back() {
        if (!ptr_ || empty()) {
            throw std::out_of_range("Array is empty");
        }
        return static_cast<pointer>(ptr_->elts)[size() - 1];
    }
    
    // 添加元素(返回可以用于填充的指针)
    pointer push() {
        if (!ptr_) return nullptr;
        return static_cast<pointer>(ngx_array_push(ptr_));
    }
    
    // 添加多个元素(返回可以用于填充的指针)
    pointer push_n(ngx_uint_t n) {
        if (!ptr_) return nullptr;
        return static_cast<pointer>(ngx_array_push_n(ptr_, n));
    }
    
    // 添加元素(复制方式)
    bool push(const T& element) {
        pointer p = push();
        if (!p) return false;
        *p = element;
        return true;
    }
    
    // 转换为vector (复制所有元素)
    std::vector<T> to_vector() const {
        if (!ptr_ || empty()) return {};
        
        return std::vector<T>(
            static_cast<const_pointer>(ptr_->elts),
            static_cast<const_pointer>(ptr_->elts) + size()
        );
    }
    
    // 迭代器支持
    [[nodiscard]] iterator begin() noexcept {
        return ptr_ ? static_cast<pointer>(ptr_->elts) : nullptr;
    }
    
    [[nodiscard]] const_iterator begin() const noexcept {
        return ptr_ ? static_cast<const_pointer>(ptr_->elts) : nullptr;
    }
    
    [[nodiscard]] const_iterator cbegin() const noexcept {
        return begin();
    }
    
    [[nodiscard]] iterator end() noexcept {
        return ptr_ ? static_cast<pointer>(ptr_->elts) + size() : nullptr;
    }
    
    [[nodiscard]] const_iterator end() const noexcept {
        return ptr_ ? static_cast<const_pointer>(ptr_->elts) + size() : nullptr;
    }
    
    [[nodiscard]] const_iterator cend() const noexcept {
        return end();
    }
    
    // 获取原始数据指针
    [[nodiscard]] pointer data() noexcept {
        return ptr_ ? static_cast<pointer>(ptr_->elts) : nullptr;
    }
    
    [[nodiscard]] const_pointer data() const noexcept {
        return ptr_ ? static_cast<const_pointer>(ptr_->elts) : nullptr;
    }
    
    // 销毁数组
    void destroy() {
        if (ptr_) {
            ngx_array_destroy(ptr_);
            ptr_ = nullptr;
        }
    }
}; // class NgxArray
