//
// Created by wuxianggujun on 2025/3/13.
//

#ifndef TINA_BLOG_NGX_POOL_HPP
#define TINA_BLOG_NGX_POOL_HPP

#include "Nginx.hpp"
#include "NginxContext.hpp"
#include <type_traits>
#include <new>
#include <stdexcept>
#include <functional>


/**
 * @brief 高性能Nginx内存池(ngx_pool_t)封装
 * 
 * 提供对ngx_pool_t的高效封装，支持内存分配和资源管理
 * 内存池生命周期由该类负责管理（创建模式）或由外部管理（引用模式）
 */
class NgxPool : public NginxContext<ngx_pool_t> {
public:
    /**
     * @brief 创建新的内存池
     * @param size 内存池大小，默认为NGX_DEFAULT_POOL_SIZE
     * @param log 日志对象，默认为NULL
     * @throw std::bad_alloc 如果内存池创建失败
     */
    explicit NgxPool(size_t size = NGX_DEFAULT_POOL_SIZE, ngx_log_t* log = nullptr)
        : NginxContext<ngx_pool_t>(ngx_create_pool(size, log), true)
    {
        if (!valid()) {
            throw std::bad_alloc();
        }
    }

    /**
     * @brief 从已存在的内存池构造
     * @param pool 内存池指针
     * @param owns_pool 是否拥有内存池所有权，默认为false
     */
    explicit NgxPool(ngx_pool_t* pool, bool owns_pool = false) noexcept
        : NginxContext<ngx_pool_t>(pool, owns_pool)
    {
    }

    /**
     * @brief 重载->运算符，方便访问内存池成员
     * @return 内存池指针
     */
    inline ngx_pool_t* operator->() const noexcept {
        return ptr_;
    }

    /**
     * @brief 重置内存池
     * 清理所有内存池资源但保留内存池本身
     */
    inline void reset() noexcept {
        if (valid()) {
            ngx_reset_pool(ptr_);
        }
    }

    /**
     * @brief 分配未初始化内存
     * @param size 内存大小
     * @return 内存指针，分配失败返回nullptr
     */
    [[nodiscard]] inline void* palloc(size_t size) const noexcept {
        if (!valid()) {
            return nullptr;
        }
        return ngx_palloc(ptr_, size);
    }

    /**
     * @brief 分配并清零内存
     * @param size 内存大小
     * @return 内存指针，分配失败返回nullptr
     */
    [[nodiscard]] inline void* pcalloc(size_t size) const noexcept {
        if (!valid()) {
            return nullptr;
        }
        return ngx_pcalloc(ptr_, size);
    }

    /**
     * @brief 分配并清零内存（calloc是pcalloc的别名）
     * @param size 内存大小
     * @return 内存指针，分配失败返回nullptr
     */
    [[nodiscard]] inline void* calloc(size_t size) const noexcept {
        return pcalloc(size);
    }

    /**
     * @brief 分配指定类型的对象
     * @tparam T 对象类型
     * @return 类型T的指针，分配失败返回nullptr
     */
    template<typename T>
    [[nodiscard]] inline T* alloc() const noexcept {
        return static_cast<T*>(pcalloc(sizeof(T)));
    }

    /**
     * @brief 分配并构造指定类型的对象
     * @tparam T 对象类型
     * @tparam Args 构造参数类型
     * @param args 构造参数
     * @return 类型T的指针，分配或构造失败返回nullptr
     */
    template<typename T, typename... Args>
    [[nodiscard]] inline T* construct(Args&&... args) const noexcept {
        void* mem = pcalloc(sizeof(T));
        if (!mem) {
            return nullptr;
        }
        
        try {
            return new (mem) T(std::forward<Args>(args)...);
        } catch (...) {
            // 构造失败，但内存已分配，这里不需要做额外清理
            // 因为内存池会在销毁时释放所有内存
            return nullptr;
        }
    }

    /**
     * @brief 分配指定类型的对象数组
     * @tparam T 对象类型
     * @param count 数组元素个数
     * @return 数组起始指针，分配失败返回nullptr
     */
    template<typename T>
    [[nodiscard]] inline T* alloc_array(size_t count) const noexcept {
        return static_cast<T*>(pcalloc(sizeof(T) * count));
    }

    /**
     * @brief 添加清理回调
     * @param handler 清理处理函数
     * @param data 要清理的数据
     * @return 清理对象指针，失败返回nullptr
     */
    [[nodiscard]] inline ngx_pool_cleanup_t* add_cleanup(ngx_pool_cleanup_pt handler, void* data) const noexcept {
        if (!valid()) {
            return nullptr;
        }
        
        ngx_pool_cleanup_t* cleanup = ngx_pool_cleanup_add(ptr_, 0);
        if (cleanup) {
            cleanup->handler = handler;
            cleanup->data = data;
        }
        
        return cleanup;
    }

    /**
     * @brief 添加带自定义析构函数的类型清理
     * @tparam T 对象类型
     * @param obj 对象指针
     * @return 是否添加成功
     */
    template<typename T>
    inline bool add_typed_cleanup(T* obj) const noexcept {
        if (!obj) {
            return false;
        }
        
        auto cleanup = add_cleanup(
            [](void* data) {
                static_cast<T*>(data)->~T();
            },
            obj
        );
        
        return cleanup != nullptr;
    }

    /**
     * @brief 在内存池中分配内存创建文件
     * @param name 文件名
     * @param log 日志对象
     * @return 文件对象，失败返回nullptr
     */
    [[nodiscard]] inline ngx_file_t* create_file(const ngx_str_t& name, ngx_log_t* log) const noexcept {
        if (!valid()) {
            return nullptr;
        }
        
        ngx_file_t* file = static_cast<ngx_file_t*>(pcalloc(sizeof(ngx_file_t)));
        if (!file) {
            return nullptr;
        }
        
        file->name = name;
        file->log = log ? log : ptr_->log;
        
        return file;
    }

    /**
     * @brief 获取内存池日志对象
     * @return 日志对象指针
     */
    [[nodiscard]] inline ngx_log_t* log() const noexcept {
        return valid() ? ptr_->log : nullptr;
    }

    /**
     * @brief 从请求对象获取内存池
     * 
     * @param r Nginx请求对象
     * @return NgxPool 内存池对象
     */
    static NgxPool from_request(ngx_http_request_t* r) {
        return NgxPool(r->pool);
    }

    /**
     * @brief 在内存池中复制字符串
     * @param str 字符串
     * @return 复制后的字符串指针，失败返回nullptr
     */
    [[nodiscard]] inline u_char* strdup(const std::string& str) const noexcept {
        if (!valid() || str.empty()) {
            return nullptr;
        }
        
        u_char* p = static_cast<u_char*>(ngx_palloc(ptr_, str.length() + 1));
        if (p == nullptr) {
            return nullptr;
        }
        
        ngx_memcpy(p, str.c_str(), str.length());
        p[str.length()] = '\0';
        
        return p;
    }

protected:
    // 友元声明，允许基类访问派生类的私有方法
    friend class NginxContextBase<ngx_pool_t, NginxContext<ngx_pool_t>>;
    
private:
    /**
     * @brief 清理资源
     * 如果拥有内存池所有权，则销毁内存池
     */
    void cleanup_impl() noexcept {
        if (valid() && owns_ptr_) {
            ngx_destroy_pool(ptr_);
        }
        
        // 重置指针和所有权标志
        ptr_ = nullptr;
        owns_ptr_ = false;
    }
};

#endif // TINA_BLOG_NGX_POOL_HPP 