#pragma once
#include "NgxPtr.hpp"
#include <type_traits>


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
    
    // 使用基类的构造函数
    using NgxPtr<ngx_array_t>::NgxPtr;
    
    // 获取元素数量
    [[nodiscard]] size_type size() const noexcept {
        return get() ? get()->nelts : 0;
    }
    
    // 获取容量
    [[nodiscard]] size_type capacity() const noexcept {
        return get() ? get()->nalloc : 0;
    }
    
    // 是否为空
    [[nodiscard]] bool empty() const noexcept {
        return size() == 0;
    }
    
    // 获取数据指针
    pointer data() noexcept {
        return get() ? static_cast<pointer>(get()->elts) : nullptr;
    }
    
    const_pointer data() const noexcept {
        return get() ? static_cast<const_pointer>(get()->elts) : nullptr;
    }
    
    // 访问元素
    reference operator[](size_type i) noexcept {
        return data()[i];
    }
    
    const_reference operator[](size_type i) const noexcept {
        return data()[i];
    }
    
    // 添加元素
    pointer push() {
        if (!ptr_) return nullptr;
        return static_cast<pointer>(ngx_array_push(ptr_));
    }
    
    // 添加多个元素
    pointer push_n(size_type n) {
        if (!ptr_) return nullptr;
        return static_cast<pointer>(ngx_array_push_n(ptr_, n));
    }
    
    // 迭代器支持
    pointer begin() noexcept { return data(); }
    pointer end() noexcept { return data() + size(); }
    const_pointer begin() const noexcept { return data(); }
    const_pointer end() const noexcept { return data() + size(); }
    const_pointer cbegin() const noexcept { return begin(); }
    const_pointer cend() const noexcept { return end(); }
}; // class NgxArray
