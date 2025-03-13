//
// Created by wuxianggujun on 2025/3/13.
//

#ifndef TINA_BLOG_NGX_STRING_HPP
#define TINA_BLOG_NGX_STRING_HPP

#include "Nginx.hpp"
#include "NginxContext.hpp"
#include <string>
#include <string_view>
#include <cstring>
#include <algorithm>


/**
 * @brief 高性能Nginx字符串(ngx_str_t)封装
 * 
 * 提供对ngx_str_t的安全、高效封装，支持与std::string互操作
 * 默认不管理内存所有权，但可配置为拥有内存所有权
 */
class NgxString : public NginxContext<ngx_str_t> {
public:
    /**
     * @brief 从ngx_str_t构造
     * @param str Nginx字符串
     * @param owns_memory 是否拥有内存所有权
     */
    inline NgxString(const ngx_str_t& str, bool owns_memory = false) noexcept
        : NginxContext<ngx_str_t>(new ngx_str_t(str), true),
          owns_memory_(owns_memory)
    {
        if (owns_memory_ && str.len > 0) {
            // 如果拥有内存所有权，则复制数据
            data_ = new u_char[str.len];
            std::memcpy(data_, str.data, str.len);
            ptr_->data = data_;
            ptr_->len = str.len;
        } else {
            // 不拥有所有权，直接使用原始指针
            data_ = str.data;
            ptr_->data = data_;
            ptr_->len = str.len;
        }
    }

    /**
     * @brief 从C字符串构造
     * @param str C字符串
     * @param owns_memory 是否拥有内存所有权，默认为true
     */
    inline NgxString(const char* str, bool owns_memory = true) noexcept
        : NginxContext<ngx_str_t>(new ngx_str_t(), true),
          owns_memory_(owns_memory)
    {
        if (!str) {
            ptr_->len = 0;
            ptr_->data = nullptr;
            data_ = nullptr;
            return;
        }

        ptr_->len = std::strlen(str);
        
        if (ptr_->len > 0) {
            if (owns_memory_) {
                // 如果拥有内存所有权，则复制数据
                data_ = new u_char[ptr_->len];
                std::memcpy(data_, str, ptr_->len);
            } else {
                // 不拥有所有权，直接使用原始指针
                data_ = reinterpret_cast<u_char*>(const_cast<char*>(str));
            }
            ptr_->data = data_;
        } else {
            data_ = nullptr;
            ptr_->data = nullptr;
        }
    }

    /**
     * @brief 从std::string构造
     * @param str 标准库字符串
     * @param owns_memory 是否拥有内存所有权，默认为true
     */
    inline NgxString(const std::string& str, bool owns_memory = true) noexcept
        : NginxContext<ngx_str_t>(new ngx_str_t(), true),
          owns_memory_(owns_memory)
    {
        ptr_->len = str.length();
        
        if (ptr_->len > 0) {
            if (owns_memory_) {
                // 如果拥有内存所有权，则复制数据
                data_ = new u_char[ptr_->len];
                std::memcpy(data_, str.data(), ptr_->len);
            } else {
                // 不拥有所有权，直接使用原始指针
                data_ = reinterpret_cast<u_char*>(const_cast<char*>(str.data()));
            }
            ptr_->data = data_;
        } else {
            data_ = nullptr;
            ptr_->data = nullptr;
        }
    }

    /**
     * @brief 从字符串视图构造
     * @param sv 字符串视图
     * @param owns_memory 是否拥有内存所有权，默认为true
     */
    inline NgxString(std::string_view sv, bool owns_memory = true) noexcept
        : NginxContext<ngx_str_t>(new ngx_str_t(), true),
          owns_memory_(owns_memory)
    {
        ptr_->len = sv.length();
        
        if (ptr_->len > 0) {
            if (owns_memory_) {
                // 如果拥有内存所有权，则复制数据
                data_ = new u_char[ptr_->len];
                std::memcpy(data_, sv.data(), ptr_->len);
            } else {
                // 不拥有所有权，直接使用原始指针
                data_ = reinterpret_cast<u_char*>(const_cast<char*>(sv.data()));
            }
            ptr_->data = data_;
        } else {
            data_ = nullptr;
            ptr_->data = nullptr;
        }
    }

    /**
     * @brief 使用Nginx内存池在池内分配字符串
     * @param str 源字符串
     * @param pool Nginx内存池
     * @return 新的NgxString对象，数据位于内存池中
     */
    static inline NgxString create_in_pool(const std::string& str, ngx_pool_t* pool) {
        if (!pool || str.empty()) {
            return NgxString();
        }

        ngx_str_t ngx_str;
        ngx_str.len = str.length();
        ngx_str.data = static_cast<u_char*>(ngx_pcalloc(pool, ngx_str.len));
        
        if (ngx_str.data) {
            std::memcpy(ngx_str.data, str.data(), ngx_str.len);
        } else {
            ngx_str.len = 0;
        }

        // 从池分配的内存由池管理，不需要在NgxString中管理
        return NgxString(ngx_str, false);
    }

    /**
     * @brief 默认构造函数，创建空字符串
     */
    inline NgxString() noexcept
        : NginxContext<ngx_str_t>(new ngx_str_t(), true),
          owns_memory_(false), data_(nullptr)
    {
        ptr_->len = 0;
        ptr_->data = nullptr;
    }

    /**
     * @brief 移动构造函数
     * @param other 源对象
     */
    inline NgxString(NgxString&& other) noexcept
        : NginxContext<ngx_str_t>(other.ptr_, other.owns_ptr_),
          owns_memory_(other.owns_memory_), data_(other.data_)
    {
        other.ptr_ = nullptr;
        other.owns_ptr_ = false;
        other.data_ = nullptr;
        other.owns_memory_ = false;
    }

    /**
     * @brief 移动赋值运算符
     * @param other 源对象
     * @return 自身引用
     */
    inline NgxString& operator=(NgxString&& other) noexcept {
        if (this != &other) {
            // 先释放自身资源（基类会通过CRTP调用cleanup_impl）
            if (owns_memory_ && data_) {
                delete[] data_;
                data_ = nullptr;
                owns_memory_ = false;
            }
            
            // 通过基类移动操作符处理基础指针
            NginxContext<ngx_str_t>::operator=(std::move(other));
            
            // 转移数据资源
            data_ = other.data_;
            owns_memory_ = other.owns_memory_;

            // 清空源对象
            other.data_ = nullptr;
            other.owns_memory_ = false;
        }
        return *this;
    }

    /**
     * @brief 拷贝构造函数
     * @param other 源对象
     */
    inline NgxString(const NgxString& other)
        : NginxContext<ngx_str_t>(new ngx_str_t(), true),
          owns_memory_(true), data_(nullptr)
    {
        if (other.ptr_) {
            ptr_->len = other.ptr_->len;
            if (ptr_->len > 0) {
                data_ = new u_char[ptr_->len];
                std::memcpy(data_, other.ptr_->data, ptr_->len);
                ptr_->data = data_;
            } else {
                ptr_->data = nullptr;
            }
        } else {
            ptr_->len = 0;
            ptr_->data = nullptr;
        }
    }

    /**
     * @brief 拷贝赋值运算符
     * @param other 源对象
     * @return 自身引用
     */
    inline NgxString& operator=(const NgxString& other) {
        if (this != &other) {
            // 释放旧资源
            if (owns_memory_ && data_) {
                delete[] data_;
                data_ = nullptr;
                owns_memory_ = false;
            }
            
            if (ptr_) {
                delete ptr_;
            }
            
            // 创建新的ngx_str_t
            ptr_ = new ngx_str_t();
            owns_ptr_ = true;
            owns_memory_ = true;
            
            if (other.ptr_) {
                ptr_->len = other.ptr_->len;
                if (ptr_->len > 0) {
                    data_ = new u_char[ptr_->len];
                    std::memcpy(data_, other.ptr_->data, ptr_->len);
                    ptr_->data = data_;
                } else {
                    data_ = nullptr;
                    ptr_->data = nullptr;
                }
            } else {
                ptr_->len = 0;
                ptr_->data = nullptr;
                data_ = nullptr;
            }
        }
        return *this;
    }

    /**
     * @brief 转换为std::string
     * @return 标准库字符串
     */
    [[nodiscard]] inline std::string to_string() const {
        if (!valid() || !ptr_->data || ptr_->len == 0) {
            return std::string();
        }
        return std::string(reinterpret_cast<const char*>(ptr_->data), ptr_->len);
    }

    /**
     * @brief 转换为std::string_view
     * @return 字符串视图
     */
    [[nodiscard]] inline std::string_view to_string_view() const noexcept {
        if (!valid() || !ptr_->data || ptr_->len == 0) {
            return std::string_view();
        }
        return std::string_view(reinterpret_cast<const char*>(ptr_->data), ptr_->len);
    }

    /**
     * @brief 判断字符串是否为空
     * @return 如果字符串为空则返回true
     */
    [[nodiscard]] inline bool empty() const noexcept {
        return !valid() || ptr_->len == 0 || ptr_->data == nullptr;
    }

    /**
     * @brief 获取字符串长度
     * @return 字符串长度
     */
    [[nodiscard]] inline size_t length() const noexcept {
        return valid() ? ptr_->len : 0;
    }

    /**
     * @brief 获取原始数据指针
     * @return 原始数据指针
     */
    [[nodiscard]] inline const u_char* data() const noexcept {
        return valid() ? ptr_->data : nullptr;
    }

    /**
     * @brief 字符串比较
     * @param other 要比较的字符串
     * @return 如果相等返回true
     */
    [[nodiscard]] inline bool equals(const NgxString& other) const noexcept {
        if (!valid() || !other.valid()) {
            return false;
        }
        
        if (ptr_->len != other.ptr_->len) {
            return false;
        }
        
        if (ptr_->len == 0) {
            return true;
        }
        
        return std::memcmp(ptr_->data, other.ptr_->data, ptr_->len) == 0;
    }

    /**
     * @brief 字符串比较，忽略大小写
     * @param other 要比较的字符串
     * @return 如果相等返回true
     */
    [[nodiscard]] inline bool iequals(const NgxString& other) const noexcept {
        if (!valid() || !other.valid()) {
            return false;
        }
        
        if (ptr_->len != other.ptr_->len) {
            return false;
        }
        
        if (ptr_->len == 0) {
            return true;
        }
        
        return ngx_strncasecmp(ptr_->data, other.ptr_->data, ptr_->len) == 0;
    }

    /**
     * @brief 相等运算符
     * @param other 要比较的字符串
     * @return 如果相等返回true
     */
    inline bool operator==(const NgxString& other) const noexcept {
        return equals(other);
    }

    /**
     * @brief 不等运算符
     * @param other 要比较的字符串
     * @return 如果不等返回true
     */
    inline bool operator!=(const NgxString& other) const noexcept {
        return !equals(other);
    }

    /**
     * @brief 判断字符串是否以指定前缀开始
     * @param prefix 前缀
     * @return 如果是则返回true
     */
    [[nodiscard]] inline bool starts_with(const NgxString& prefix) const noexcept {
        if (!valid() || !prefix.valid()) {
            return false;
        }
        
        if (prefix.ptr_->len > ptr_->len || prefix.empty() || empty()) {
            return false;
        }
        
        return std::memcmp(ptr_->data, prefix.ptr_->data, prefix.ptr_->len) == 0;
    }

    /**
     * @brief 判断字符串是否以指定后缀结束
     * @param suffix 后缀
     * @return 如果是则返回true
     */
    [[nodiscard]] inline bool ends_with(const NgxString& suffix) const noexcept {
        if (!valid() || !suffix.valid()) {
            return false;
        }
        
        if (suffix.ptr_->len > ptr_->len || suffix.empty() || empty()) {
            return false;
        }
        
        return std::memcmp(
            ptr_->data + ptr_->len - suffix.ptr_->len, 
            suffix.ptr_->data, 
            suffix.ptr_->len
        ) == 0;
    }

    /**
     * @brief 尝试查找子字符串
     * @param sub 要查找的子字符串
     * @return 如果找到则返回首次出现的位置，否则返回npos
     */
    [[nodiscard]] inline size_t find(const NgxString& sub) const noexcept {
        if (!valid() || !sub.valid() || sub.empty() || empty() || sub.ptr_->len > ptr_->len) {
            return npos;
        }

        // 使用简单的内存搜索
        for (size_t i = 0; i <= ptr_->len - sub.ptr_->len; ++i) {
            if (std::memcmp(ptr_->data + i, sub.ptr_->data, sub.ptr_->len) == 0) {
                return i;
            }
        }
        
        return npos;
    }

    /**
     * @brief 获取子字符串
     * @param pos 起始位置
     * @param len 长度，默认到字符串结束
     * @return 子字符串
     */
    [[nodiscard]] inline NgxString substr(size_t pos, size_t len = npos) const {
        if (!valid() || pos >= ptr_->len) {
            return NgxString();
        }

        size_t actual_len = (len == npos || pos + len > ptr_->len) ? ptr_->len - pos : len;
        
        ngx_str_t sub;
        sub.data = ptr_->data + pos;
        sub.len = actual_len;
        
        // 子字符串总是创建副本，拥有所有权
        return NgxString(sub, true);
    }

    /**
     * @brief 从ngx_string宏创建
     * @param literal 字面量
     * @return NgxString对象
     */
    static inline NgxString from_literal(const char* literal) {
        ngx_str_t temp = ngx_string(literal);
        return NgxString(temp, false);  // 不拥有内存，因为指向的是常量
    }

    /// 表示没有找到的值
    static constexpr size_t npos = static_cast<size_t>(-1);

    // 友元声明，允许基类访问派生类的私有方法
    friend class NginxContextBase<ngx_str_t, NginxContext<ngx_str_t>>;

private:
    /**
     * @brief 清理资源实现
     * 释放字符串数据和ngx_str_t结构
     */
    void cleanup_impl() noexcept {
        if (owns_memory_ && data_) {
            delete[] data_;
            data_ = nullptr;
        }
        
        // 重置内部状态
        owns_memory_ = false;
        
        // 指针的释放由基类负责
    }

private:
    u_char* data_ = nullptr;     ///< 数据指针
    bool owns_memory_ = false;   ///< 是否拥有数据内存
};

#endif // TINA_BLOG_NGX_STRING_HPP 