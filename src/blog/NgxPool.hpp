#pragma once
#include "NgxPtr.hpp"
#include "NgxString.hpp"
#include "NgxArray.hpp"
#include "NgxLog.hpp"
#include <memory>
#include <type_traits>

/**
 * @brief Nginx内存池包装类
 * 
 * 提供了:
 * 1. 内存分配接口
 * 2. 对象创建
 * 3. 内存对齐
 * 4. 临时内存管理
 */
class NgxPool : public NgxPtr<ngx_pool_t>
{
public:
    // 使用基类的构造函数
    using NgxPtr<ngx_pool_t>::NgxPtr;

    // 分配内存
    template <typename T>
    T* alloc()
    {
        return static_cast<T*>(ngx_pcalloc(ptr_, sizeof(T)));
    }

    // 分配指定大小的内存
    [[nodiscard]] void* alloc(const size_t size) const
    {
        return ngx_pcalloc(ptr_, size);
    }

    // 分配未初始化的内存
    [[nodiscard]] void* nalloc(size_t size) const
    {
        return ngx_pnalloc(ptr_, size);
    }

    // 分配对齐的内存
    [[nodiscard]] void* memalign(size_t alignment, size_t size) const
    {
        return ngx_pmemalign(ptr_, alignment, size);
    }

    // 创建子池
    [[nodiscard]] NgxPool create_pool(size_t size) const
    {
        return NgxPool(ngx_create_pool(size, ptr_->log));
    }

    // 清理所有数据
    void cleanup()
    {
        if (ptr_)
        {
            ngx_destroy_pool(ptr_);
            this->reset();
        }
    }

    // 重置池
    void reset_pool() const
    {
        if (ptr_)
        {
            ngx_reset_pool(ptr_);
        }
    }

    // 获取大块内存的大小
    [[nodiscard]] size_t large_size() const
    {
        return ptr_ ? ptr_->max : 0;
    }

    // 添加清理回调
    template <typename F>
    bool add_cleanup(F&& cleanup_handler)
    {
        using handler_type = std::remove_reference_t<F>;
        auto* cln = static_cast<ngx_pool_cleanup_t*>(
            ngx_pool_cleanup_add(ptr_, sizeof(handler_type))
        );
        if (!cln)
        {
            return false;
        }

        // 复制清理处理器
        auto* handler_ptr = new(cln->data) handler_type(std::forward<F>(cleanup_handler));
        cln->handler = [](void* data)
        {
            auto* h = static_cast<handler_type*>(data);
            (*h)();
            h->~handler_type();
        };

        return true;
    }

    // 创建字符串
    NgxString create_string(const char* str);
    NgxString create_string(const std::string& str);
    NgxString create_string(std::string_view sv);
    NgxString create_string(const u_char* data, size_t len);

    // 创建数组
    template <typename T>
    NgxArray<T> create_array(ngx_uint_t n)
    {
        return NgxArray<T>(ngx_array_create(ptr_, n, sizeof(T)));
    }

    // 获取日志对象
    [[nodiscard]] NgxLog get_log() const
    {
        return NgxLog(ptr_ ? ptr_->log : nullptr);
    }

    // 析构函数
    ~NgxPool() override
    {
        cleanup();
    }
}; // class NgxPool
