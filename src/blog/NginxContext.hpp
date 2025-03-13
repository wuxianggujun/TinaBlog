//
// Created by wuxianggujun on 2025/3/13.
//

#ifndef TINA_BLOG_NGINX_CONTEXT_HPP
#define TINA_BLOG_NGINX_CONTEXT_HPP

#include "Nginx.hpp"
#include <type_traits>

/**
 * @brief 高性能Nginx对象封装模板基类
 * 
 * 用于包装Nginx原生C结构，提供类型安全、所有权管理和简洁接口
 * 通过CRTP(奇异递归模板模式)避免虚函数开销，实现静态多态
 * 
 * @tparam T 被包装的Nginx类型
 * @tparam Derived 派生类类型，用于CRTP模式
 */
template <typename T, typename Derived>
class NginxContextBase
{
public:
    /**
     * @brief 构造函数
     * @param ptr 指向Nginx对象的指针
     * @param owns_ptr 是否拥有指针生命周期（负责释放资源）
     */
    explicit NginxContextBase(T* ptr, bool owns_ptr = false) noexcept
        : ptr_(ptr), owns_ptr_(owns_ptr)
    {
    }

    /**
     * @brief 禁用复制构造函数
     * 防止意外的资源所有权复制
     */
    NginxContextBase(const NginxContextBase&) = delete;
    
    /**
     * @brief 禁用复制赋值运算符
     * 防止意外的资源所有权复制
     */
    NginxContextBase& operator=(const NginxContextBase&) = delete;

    /**
     * @brief 移动构造函数
     * 转移对象的所有权
     * @param rhs 右值引用，源对象
     */
    NginxContextBase(NginxContextBase&& rhs) noexcept 
        : ptr_(rhs.ptr_), owns_ptr_(rhs.owns_ptr_)
    {
        rhs.ptr_ = nullptr;
        rhs.owns_ptr_ = false;
    }

    /**
     * @brief 移动赋值运算符
     * 转移对象的所有权
     * @param rhs 右值引用，源对象
     * @return 当前对象的引用
     */
    NginxContextBase& operator=(NginxContextBase&& rhs) noexcept
    {
        if (this != &rhs)
        {
            // 使用CRTP调用派生类的清理方法
            static_cast<Derived*>(this)->cleanup_impl();
            
            ptr_ = rhs.ptr_;
            owns_ptr_ = rhs.owns_ptr_;
            rhs.ptr_ = nullptr;
            rhs.owns_ptr_ = false;
        }
        return *this;
    }

    /**
     * @brief 获取原始指针
     * @return 指向Nginx对象的原始指针
     */
    [[nodiscard]] inline T* get() const noexcept
    {
        return ptr_;
    }

    /**
     * @brief 检查指针是否有效
     * @return true如果指针非空
     */
    [[nodiscard]] inline bool valid() const noexcept
    {
        return ptr_;
    }

    /**
     * @brief 类型安全的指针转换
     * @tparam U 目标类型
     * @return 转换后的指针
     */
    template <typename U>
    [[nodiscard]] inline U* as() const noexcept
    {
        return static_cast<U*>(ptr_);
    }

    /**
     * @brief 重载->运算符，方便访问包装对象的成员
     * @return 指向Nginx对象的原始指针
     */
    inline T* operator->() const noexcept {
        return ptr_;
    }

    /**
     * @brief 重载隐式转换到bool
     * 用于条件判断，检查是否有效
     */
    explicit operator bool() const noexcept {
        return valid();
    }

    /**
     * @brief 析构函数
     * 通过CRTP调用派生类的资源清理方法
     */
    ~NginxContextBase() noexcept
    {
        static_cast<Derived*>(this)->cleanup_impl();
    }

protected:
    /**
     * @brief 默认资源清理实现
     * 派生类可覆盖此方法提供特定的资源清理逻辑
     */
    inline void cleanup_impl() noexcept
    {
        ptr_ = nullptr;
        owns_ptr_ = false;
    }

    T* ptr_ = nullptr;        ///< 指向Nginx对象的指针
    bool owns_ptr_ = false;   ///< 是否拥有指针生命周期标志
};

/**
 * @brief Nginx对象的具体封装类
 * 
 * 继承自NginxContextBase，并将自身作为CRTP模板参数
 * 提供特定类型的封装功能，可被进一步扩展
 * 
 * @tparam T 被包装的Nginx类型
 */
template <typename T>
class NginxContext : public NginxContextBase<T, NginxContext<T>>
{
public:
    // 导入基类构造函数
    using NginxContextBase<T, NginxContext<T>>::NginxContextBase;
    
    // 友元声明，允许基类访问派生类的私有方法
    friend class NginxContextBase<T, NginxContext<T>>;
    
private:
    /**
     * @brief 特定的资源清理实现
     * 默认与基类相同，可在特化版本中提供更具体的实现
     */
    inline void cleanup_impl() noexcept
    {
        // 只清除指针，不执行资源释放
        // 特化版本可以根据T的类型提供定制的释放逻辑
        this->ptr_ = nullptr;
        this->owns_ptr_ = false;
    }
};

/**
 * @brief 针对ngx_pool_t的特化，提供内存池管理
 */
template <>
class NginxContext<ngx_pool_t> : public NginxContextBase<ngx_pool_t, NginxContext<ngx_pool_t>>
{
public:
    /**
     * @brief 构造函数
     * @param pool 内存池指针
     * @param owns_pool 是否拥有内存池（负责释放）
     */
    explicit NginxContext(ngx_pool_t* pool, bool owns_pool = false) noexcept
        : NginxContextBase<ngx_pool_t, NginxContext<ngx_pool_t>>(pool, owns_pool)
    {
    }
    
    /**
     * @brief 创建新内存池
     * @param size 内存池大小
     * @param log 日志对象
     */
    explicit NginxContext(size_t size, ngx_log_t* log = nullptr) noexcept
        : NginxContextBase<ngx_pool_t, NginxContext<ngx_pool_t>>(
            ngx_create_pool(size, log), true)
    {
    }
    
    /**
     * @brief 分配内存
     * @tparam U 需要分配的类型
     * @return 分配的内存指针
     */
    template<typename U>
    inline U* alloc() const noexcept {
        if (!this->valid()) {
            return nullptr;
        }
        return static_cast<U*>(ngx_pcalloc(this->ptr_, sizeof(U)));
    }

    // 友元声明，允许基类访问派生类的私有方法
    friend class NginxContextBase<ngx_pool_t, NginxContext<ngx_pool_t>>;
    
private:
    /**
     * @brief 特定的资源清理实现
     * 如果拥有内存池，则销毁池
     */
    inline void cleanup_impl() noexcept {
        if (this->valid() && this->owns_ptr_) {
            ngx_destroy_pool(this->ptr_);
        }
        this->ptr_ = nullptr;
        this->owns_ptr_ = false;
    }
};

// 常用类型别名定义
using NgxPoolContext = NginxContext<ngx_pool_t>;
using NgxRequestContext = NginxContext<ngx_http_request_t>;
using NgxConfContext = NginxContext<ngx_conf_t>;

#endif //TINA_BLOG_NGINX_CONTEXT_HPP
