//
// Created by wuxianggujun on 2025/3/12.
//

#ifndef TINA_BLOG_NGINX_VALUE_HPP
#define TINA_BLOG_NGINX_VALUE_HPP

#include "NginxUnsetValue.hpp"

class NginxValue final
{
public:
    NginxValue() = default;
    ~NginxValue() = default;

    template <typename T>
    static bool inValid(const T& value)
    {
        return value == static_cast<T>(NginxUnsetValue::get());
    }

    template <typename T, typename U>
    static void init(T& x, const U& v)
    {
        if (inValid(x))
        {
            x = v;
        }
    }

    template <typename T, typename U, typename V>
    static void merge(T& c, const U& p, const V& d)
    {
        if (inValid(c))
        {
            // 检查p，无效则赋值为d
            c = inValid(p) ? d : p;
        }
    }
    
    static void merge(ngx_str_t& c, const ngx_str_t& p, const ngx_str_t& d){
        //检查字符串是否为空
          if(!c.data){
            c = p.data ? p : d;
           }
      }
    

    template<typename T,typename ... Args>
    static void unset(T& v,Args&... args)
    {
        v = NginxUnsetValue::get();
        // 递归处理剩余的模版参数
        unset(args...);
    }
    
};

#endif //TINA_BLOG_NGINX_VALUE_HPP
