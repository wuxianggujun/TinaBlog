//
// Created by wuxianggujun on 2025/3/12.
//

#ifndef TINA_BLOG_NGINX_ARRAY_HPP
#define TINA_BLOG_NGINX_ARRAY_HPP


#include "NginxPool.hpp"

template <typename T>
class NginxArray final : public NginxWrapper<ngx_array_t>
{
public:
    typedef NginxWrapper<ngx_array_t> super_type;
    typedef NginxArray this_type;
    typedef T value_type;

    explicit NginxArray(const NginxPool& pool, ngx_uint_t n = 10) : super_type(pool.array<T>(n))
    {
    }

    explicit NginxArray(ngx_array_t* array): super_type(array)
    {
    }

    explicit NginxArray(ngx_array_t& array): super_type(array)
    {
    }

    ~NginxArray() = default;

    ngx_uint_t size() const
    {
        return get() ? get()->nelts : 0;
    }

    T& operator[](ngx_uint_t i) const
    {
        NginxException::require(i < size() && get());
        return elts()[i];
    }

    bool empty() const
    {
        return get()->nelts == 0;
    }

    void clear() const
    {
        get()->nelts = 0;
    }

    template <typename U>
    NginxArray<U> reshape(ngx_uint_t n = 0, ngx_pool_t* pool = nullptr) const
    {
        const auto rc = ngx_array_init(get(), pool ? pool : get()->pool, n ? n : get()->nalloc, sizeof(U));
        NginxException::require(rc);

        return get();
    }

    void reinit(ngx_uint_t n = 0) const
    {
        reshape<T>(n);
    }

    template <typename V>
    void visit(V v) const
    {
        auto p = elts();

        for (ngx_uint_t i = 0; i < size(); ++i)
        {
            v(p[i]);
        }
    }

    T& prepare() const
    {
        auto tmp = ngx_array_push(get());

        NginxException::require(tmp);

        assert(tmp);
        return *reinterpret_cast<T*>(tmp);
    }

    void push(const T& x) const
    {
        prepare() = x;
    }

    void merge(const this_type& other) const
    {
        auto f = [this](const value_type& v)
        {
            prepare() = v;
        };
        other.visit(f);
    }

private:
    T* elts() const
    {
        return reinterpret_cast<T*>(get()->elts);
    }
};

typedef NginxArray<ngx_int_t> NginxIntArray;
typedef NginxArray<ngx_uint_t> NginxUIntArray;

typedef NginxArray<ngx_str_t> NginxStrArray;
typedef NginxArray<ngx_keyval_t> NginxKeyValArray;

#endif //TINA_BLOG_NGINX_ARRAY_HPP
