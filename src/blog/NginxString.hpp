//
// Created by wuxianggujun on 2025/3/12.
//

#ifndef TINA_BLOG_NGINX_STRING_HPP
#define TINA_BLOG_NGINX_STRING_HPP

#include "NginxWrapper.hpp"
#include <boost/utility/string_ref.hpp>

class NginxString final : public NginxWrapper<ngx_str_t>
{
public:
    typedef NginxWrapper<ngx_str_t> super_type;
    typedef NginxString this_type;

    typedef boost::string_ref string_ref_type;

    explicit NginxString(ngx_str_t& str) : super_type(str)
    {
    }

    ~NginxString() = default;

    // 获取字符串
    const char* data() const
    {
        return reinterpret_cast<const char*>(get()->data);
    }

    // 获取长度
    std::size_t size() const
    {
        return get()->len;
    }

    // 是否是空字符串
    bool empty() const
    {
        return !get()->data || !get()->len;
    }

    // 转换为char*字符串形式
    string_ref_type str() const
    {
        return string_ref_type(data(), size());
    }

    // 字符串转整数类型
    operator ngx_int_t() const
    {
        return ngx_atoi(get()->data, get()->len);
    }

    // 重载比较运算符，大小写敏感比较两个ngx_str_t对象
    friend bool operator==(const this_type& lhs, const this_type& rhs)
    {
        return lhs.size() == rhs.size() &&
            ngx_strncmp(lhs.data(), rhs.data(), lhs.size()) == 0;
    }

    template <typename ... Args>
    void printf(const Args& ... args)const
    {
        auto p = ngx_snprintf(get()->data,get()->len,args...);
        // 计算实际长度
        get()->len = static_cast<std::size_t>(p - get()->data);
    }

    // 重载流输出操作符
    template<typename T>
    friend T& operator<<(T& os, const this_type& str)
    {
        // 把字符串写入流
        os.write(str.data(), str.size());
        return os;
    }
    
};

#endif //TINA_BLOG_NGINX_STRING_HPP 
