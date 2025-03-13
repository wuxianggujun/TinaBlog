//
// Created by wuxianggujun on 2025/3/12.
//

#ifndef TINA_BLOG_NGINX_UNSET_VALUE_HPP
#define TINA_BLOG_NGINX_UNSET_VALUE_HPP

#include "Nginx.hpp"

/**
 * 相当于Nginx里的NGX_CONF_UNSET_XXX宏
 */
class NginxUnsetValue final
{
public:
    static const NginxUnsetValue& get()
    {
        static constexpr NginxUnsetValue v = {};
        return v;
    }

    template <typename T>
    explicit operator T() const
    {
        return static_cast<T>(-1);
    }

    template <typename T>
    explicit operator T*() const
    {
        return reinterpret_cast<T*>(-1);
    }
};


#endif //TINA_BLOG_NGINX_UNSET_VALUE_HPP
