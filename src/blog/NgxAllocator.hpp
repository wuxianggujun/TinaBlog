//
// Created by wuxianggujun on 2025/3/14.
//

#ifndef TINA_BLOG_NGX_ALLOCATOR_HPP
#define TINA_BLOG_NGX_ALLOCATOR_HPP

#include "Nginx.hpp"
#include "NginxContext.hpp"
#include <limits>
#include <map>
#include <new>
#include <set>

/**
 * @brief Nginx内存分配器
 * 
 * 为STL容器提供基于Nginx内存池的分配器
 * @tparam T 分配的元素类型
 */
template<typename T>
class NgxAllocator : public NginxContext<ngx_pool_t> {
public:
    // STL分配器必需的类型定义
    using value_type = T;
    using pointer = T*;
    using const_pointer = const T*;
    using reference = T&;
    using const_reference = const T&;
    using size_type = std::size_t;
    using difference_type = std::ptrdiff_t;
    
    // 类型重绑定机制，允许容器为不同类型分配内存
    template<typename U>
    struct rebind {
        using other = NgxAllocator<U>;
    };

    /**
     * @brief 从Nginx内存池构造分配器
     * @param pool 内存池指针
     * @param owns 是否拥有内存池（通常为false）
     */
    explicit NgxAllocator(ngx_pool_t* pool, bool owns = false) noexcept
        : NginxContext<ngx_pool_t>(pool, owns) {}
    
    /**
     * @brief 从另一个分配器拷贝构造
     */
    NgxAllocator(const NgxAllocator&) noexcept = default;
    
    /**
     * @brief 从另一个类型的分配器拷贝构造
     */
    template<typename U>
    NgxAllocator(const NgxAllocator<U>& other) noexcept
        : NginxContext<ngx_pool_t>(other.get(), false) {}
    
    /**
     * @brief 分配内存
     * @param n 要分配的元素数量
     * @return 分配的内存指针
     * @throws std::bad_alloc 如果分配失败
     */
    pointer allocate(size_type n) {
        if (!valid()) {
            throw std::bad_alloc();
        }
        
        // 检查是否可能发生整数溢出
        // 使用括号避免与可能存在的max宏冲突
        if (n > (std::numeric_limits<size_type>::max)() / sizeof(T)) {
            throw std::bad_alloc();
        }
        
        // 从Nginx内存池分配内存
        void* p = ngx_palloc(get(), n * sizeof(T));
        if (!p) {
            throw std::bad_alloc();
        }
        
        return static_cast<pointer>(p);
    }
    
    /**
     * @brief 释放内存
     * 
     * 注意：在Nginx内存池中，通常只有大块内存才能被单独释放
     * 小内存块可能无法释放，将随内存池一起销毁
     * @param p 要释放的内存指针
     * @param n 释放的元素数量（未使用）
     */
    void deallocate(pointer p, size_type n) noexcept {
        // 尝试释放内存，但注意这可能不会成功
        // 只有通过ngx_palloc分配的大块内存才能被ngx_pfree释放
        if (valid() && p) {
            ngx_pfree(get(), p);
            // 注意：即使返回NGX_DECLINED，也不应视为错误
            // 这是Nginx内存池的预期行为
        }
    }
    
    /**
     * @brief 在已分配内存上构造对象
     * @param p 内存位置
     * @param val 要构造的值
     */
    void construct(pointer p, const_reference val) {
        new(static_cast<void*>(p)) T(val);
    }
    
    /**
     * @brief 构造带参数的对象
     */
    template<typename U, typename... Args>
    void construct(U* p, Args&&... args) {
        new(static_cast<void*>(p)) U(std::forward<Args>(args)...);
    }
    
    /**
     * @brief 销毁对象
     * @param p 要销毁的对象指针
     */
    void destroy(pointer p) noexcept {
        p->~T();
    }
    
    /**
     * @brief 销毁任意类型对象
     */
    template<typename U>
    void destroy(U* p) noexcept {
        p->~U();
    }
    
    /**
     * @brief 获取可分配的最大元素数量
     */
    size_type max_size() const noexcept {
        return (std::numeric_limits<size_type>::max)() / sizeof(T);
    }
    
    /**
     * @brief 获取元素地址
     */
    pointer address(reference x) const noexcept {
        return std::addressof(x);
    }
    
    /**
     * @brief 获取常量元素地址
     */
    const_pointer address(const_reference x) const noexcept {
        return std::addressof(x);
    }

private:
    /**
     * @brief 清理资源
     * 
     * 如果拥有内存池，则什么也不做（内存池应由外部管理）
     */
    void cleanup_impl() noexcept
    {
        // 内存池通常由其创建者负责清理
        // 我们只需要将指针置空
        if (ptr_ && owns_ptr_) {
            ptr_ = nullptr;
            owns_ptr_ = false;
        }
    }
    
    // 允许不同类型的分配器访问私有成员
    template<typename U>
    friend class NgxAllocator;
};

/**
 * @brief 分配器相等比较
 * @return 如果分配器使用相同内存池则返回true
 */
template<typename T, typename U>
bool operator==(const NgxAllocator<T>& lhs, const NgxAllocator<U>& rhs) noexcept {
    return lhs.get() == rhs.get();
}

/**
 * @brief 分配器不等比较
 * @return 如果分配器使用不同内存池则返回true
 */
template<typename T, typename U>
bool operator!=(const NgxAllocator<T>& lhs, const NgxAllocator<U>& rhs) noexcept {
    return lhs.get() != rhs.get();
}

// STL容器类型定义
template<typename T>
using NgxStdVector = std::vector<T, NgxAllocator<T>>;

template<typename K, typename V, typename Compare = std::less<K>>
using NgxStdMap = std::map<K, V, Compare, NgxAllocator<std::pair<const K, V>>>;

template<typename K, typename V, typename Hash = std::hash<K>, typename KeyEqual = std::equal_to<K>>
using NgxStdUnorderedMap = std::unordered_map<K, V, Hash, KeyEqual, NgxAllocator<std::pair<const K, V>>>;

template<typename T, typename Compare = std::less<T>>
using NgxStdSet = std::set<T, Compare, NgxAllocator<T>>;

using NgxStdString = std::basic_string<char, std::char_traits<char>, NgxAllocator<char>>;

#endif // TINA_BLOG_NGX_ALLOCATOR_HPP
