//
// Created by wuxianggujun on 2025/3/12.
//

#ifndef TINA_BLOG_NGINX_EXCEPTION_HPP
#define TINA_BLOG_NGINX_EXCEPTION_HPP

// 包含统一的Nginx头文件
#include "Nginx.hpp"

// Boost库相关包含
#include <boost/exception/all.hpp>
#include <boost/utility/string_ref.hpp>

class NginxException final : public virtual std::exception, public virtual boost::exception
{
public:
    typedef boost::string_ref string_ref_type;

private:
    ngx_int_t m_code = NGX_ERROR;
    std::string m_msg;

public:
    explicit NginxException(const ngx_int_t x, const string_ref_type msg) : m_code(x), m_msg(msg)
    {
    }

    // 委托构造
    explicit NginxException(ngx_int_t x = NGX_ERROR) : NginxException(x, "")
    {
    }

    // 委托构造
    explicit NginxException(const string_ref_type msg): NginxException(NGX_ERROR, msg)
    {
    }

    ~NginxException() noexcept override
    {
    }

    ngx_int_t code() const
    {
        return m_code;
    }

    const char* what() const noexcept override
    {
        return m_msg.c_str();
    }

    // 静态函数，抛出异常
    static void raise(ngx_int_t rc = NGX_ERROR, string_ref_type msg = "")
    {
        throw NginxException(rc, msg);
    }

    // 检查条件是否满足
    static void require(const bool cond, ngx_int_t e = NGX_ERROR)
    {
        if (!cond)
        {
            raise(e);
        }
    }

    // 检查错误码
    static void require(const ngx_int_t rc, ngx_int_t x = NGX_OK)
    {
        require(rc == x, rc);
    }

    template <typename T>
    static void require(T* p, ngx_int_t e = NGX_ERROR)
    {
        // 如果是空指针则抛出异常
        require(p != nullptr, e);
    }

    static void fail(bool cond, ngx_int_t e = NGX_ERROR)
    {
        // 如果符合预期则抛出异常
        if (cond)
        {
            raise(e);
        }
    }
};


#endif //TINA_BLOG_NGINX_EXCEPTION_HPP
