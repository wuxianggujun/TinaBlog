//
// Created by wuxianggujun on 2025/3/12.
//

#ifndef TINA_BLOG_NGINX_POOL_HPP
#define TINA_BLOG_NGINX_POOL_HPP

#include "NginxWrapper.hpp"
#include "NginxException.hpp"

class NginxPool final : public NginxWrapper<ngx_pool_t>
{
public:
    typedef NginxWrapper<ngx_pool_t> super_type;
    typedef NginxPool this_type;

    explicit NginxPool(ngx_pool_t* p): super_type(p)
    {
    }

    ~NginxPool() = default;

    template <typename T>
    NginxPool(T* x): NginxPool(x->pool)
    {
    }

    template <typename T, bool no_exception = false>
    T* palloc() const
    {
        // 分配内存
        auto p = ngx_pcalloc(get(), sizeof(T));
        // 检查空指针
        if (!p)
        {
            // 是否运行抛出异常
            if (no_exception)
            {
                return nullptr;
            }
            // 抛出异常
            NginxException::raise();
        }
        assert(p);
        return new(p)T();
    }

    // 抛出异常版本
    template <typename T>
    T* alloc() const
    {
        return palloc<T, false>();
    }

    // 不抛出异常版本
    template <typename T>
    T* alloc_noexcept() const
    {
        return palloc<T, true>();
    }

    template <typename F, typename T>
    ngx_pool_cleanup_t* cleanup(F func, T* data, std::size_t size = 0) const
    {
        auto p = ngx_pool_cleanup_add(get(), size);
        // 检查空指针
        NginxException::require(p);

        // 设置清理函数
        p->handler = func;
        if (data)
        {
            p->data = data;
        }
        // 返回清理对象信息
        return p;
    }

    // 适配析构函数符合Nginx要求
    template <typename T>
    static void destroy(void* p)
    {
        (reinterpret_cast<T*>(p))->~T();
    }

    // 重载cleanup函数
    template <typename T>
    void cleanup(T* data) const
    {
        cleanup(&this_type::destroy<T>, data);
    }

    ngx_str_t dup(ngx_str_t& str) const
    {
        ngx_str_t tmp;

        // 设置字符串长度
        tmp.len = str.len;
        // 内存池复制字符串
        tmp.data = ngx_pstrdup(get(), &str);

        NginxException::require(tmp.data);
        return tmp;
    }

    ngx_str_t dup(const boost::string_ref str) const
    {
        // 转换为ngx_str_t对象
        ngx_str_t tmp{str.size(), (u_char*)str.data()};
        return dup(tmp);
    }

    template <typename T>
    ngx_array_t* array(ngx_uint_t n = 10) const
    {
        // 使用内存池创建数组
        auto p = ngx_array_create(get(), n, sizeof(T));
        NginxException::require(p);
        return p;
    }
};


#endif //TINA_BLOG_NGINX_POOL_HPP
