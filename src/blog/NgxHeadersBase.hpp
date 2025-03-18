//
// Created by wuxianggujun on 2025/3/18.
//
#ifndef TINA_BLOG_NGX_HEADERS_HPP
#define TINA_BLOG_NGX_HEADERS_HPP

#include "Nginx.hpp"
#include "NginxContext.hpp"
#include <string>
#include <optional>
#include <vector>

/**
 * @brief HTTP头部封装基类
 * 
 * @tparam HeaderType 头部类型(ngx_http_headers_in_t或ngx_http_headers_out_t)
 */
template <typename HeaderType>
class NgxHeadersBase : public NginxContext<HeaderType>
{
public:
    using NginxContext<HeaderType>::NginxContext;

    /**
     * @brief 获取特定头部值
     * @param name 头部名称
     * @return 头部值，不存在时返回空
     */
    [[nodiscard]] std::optional<std::string> get(const std::string& name) const
    {
        if (!this->valid()) return std::nullopt;

        ngx_list_part_t* part = &(this->ptr_->headers.part);
        ngx_table_elt_t* headers = static_cast<ngx_table_elt_t*>(part->elts);

        for (ngx_uint_t i = 0; /* void */; i++)
        {
            if (i >= part->nelts)
            {
                if (part->next == nullptr)
                {
                    break;
                }

                part = part->next;
                headers = static_cast<ngx_table_elt_t*>(part->elts);
                i = 0;
            }

            if (headers[i].key.len == name.length() &&
                ngx_strncasecmp(headers[i].key.data,
                                reinterpret_cast<u_char*>(const_cast<char*>(name.c_str())),
                                name.length()) == 0)
            {
                return std::string(
                    reinterpret_cast<char*>(headers[i].value.data),
                    headers[i].value.len);
            }
        }

        return std::nullopt;
    }

    /**
     * @brief 获取所有头部
     * @return 头部列表
     */
    [[nodiscard]] std::vector<std::pair<std::string, std::string>> get_all() const
    {
        std::vector<std::pair<std::string, std::string>> result;

        if (!this->valid()) return result;

        ngx_list_part_t* part = &(this->ptr_->headers.part);
        ngx_table_elt_t* headers = static_cast<ngx_table_elt_t*>(part->elts);

        for (ngx_uint_t i = 0; /* void */; i++)
        {
            if (i >= part->nelts)
            {
                if (part->next == nullptr)
                {
                    break;
                }

                part = part->next;
                headers = static_cast<ngx_table_elt_t*>(part->elts);
                i = 0;
            }

            result.emplace_back(
                std::string(reinterpret_cast<char*>(headers[i].key.data), headers[i].key.len),
                std::string(reinterpret_cast<char*>(headers[i].value.data), headers[i].value.len)
            );
        }

        return result;
    }
};

/**
 * @brief 请求头部封装类
 */
class NgxHeadersIn : public NgxHeadersBase<ngx_http_headers_in_t>
{
public:
    /**
     * @brief 从请求对象构造
     * @param r HTTP请求指针
     */
    explicit NgxHeadersIn(ngx_http_request_t* r) noexcept
        : NgxHeadersBase<ngx_http_headers_in_t>(&r->headers_in, false)
    {
    }

    /**
     * @brief 获取User-Agent头部
     * @return User-Agent值
     */
    [[nodiscard]] inline std::string user_agent() const
    {
        if (!this->valid() || !this->ptr_->user_agent)
        {
            return "";
        }

        return std::string(
            reinterpret_cast<char*>(this->ptr_->user_agent->value.data),
            this->ptr_->user_agent->value.len
        );
    }
};

/**
 * @brief 响应头部封装类
 */
class NgxHeadersOut : public NgxHeadersBase<ngx_http_headers_out_t>
{
public:
    /**
     * @brief 从请求对象构造
     * @param r HTTP请求指针
     */
    explicit NgxHeadersOut(ngx_http_request_t* r) noexcept
        : NgxHeadersBase<ngx_http_headers_out_t>(&r->headers_out, false)
    {
    }

    /**
     * @brief 设置状态码
     * @param status HTTP状态码
     * @return 自身引用，支持链式调用
     */
    NgxHeadersOut& set_status(ngx_uint_t status)
    {
        if (this->valid())
        {
            this->ptr_->status = status;
        }
        return *this;
    }

    /**
     * @brief 添加自定义头部
     * @param name 头部名称
     * @param value 头部值
     * @return 自身引用，支持链式调用
     */
    NgxHeadersOut& add(const std::string& name, const std::string& value)
    {
        if (!this->valid())
        {
            return *this;
        }

        ngx_table_elt_t* h = ngx_list_push(&this->ptr_->headers);
        if (!h)
        {
            return *this;
        }

        h->hash = 1;
        h->key.len = name.length();
        h->key.data = ngx_pnalloc(request_->pool, name.length());
        if (!h->key.data)
        {
            return *this;
        }
        ngx_memcpy(h->key.data, name.c_str(), name.length());

        h->value.len = value.length();
        h->value.data = ngx_pnalloc(request_->pool, value.length());
        if (!h->value.data)
        {
            return *this;
        }
        ngx_memcpy(h->value.data, value.c_str(), value.length());

        return *this;
    }

private:
    ngx_http_request_t* request_; // 保留对请求的引用，用于内存分配
};

#endif // TINA_BLOG_NGX_HEADERS_HPP
