//
// Created by wuxianggujun on 2025/3/12.
//

#ifndef TINA_BLOG_NGINX_DATETIME_HPP
#define TINA_BLOG_NGINX_DATETIME_HPP

// 包含统一的Nginx头文件
#include "Nginx.hpp"
#include "NginxException.hpp"
#include "NginxString.hpp"

/**
 * @class NginxDateTime
 * @brief 封装Nginx日期时间操作，处理平台差异
 * 
 * 提供跨平台的日期时间处理，特别是解决Windows平台缺少ngx_localtime函数的问题
 */
class NginxDateTime final
{
public:
    NginxDateTime() = default;
    ~NginxDateTime() = default;

    static std::time_t since()
    {
        return ngx_time();
    }

    static ngx_str_t today()
    {
        ngx_tm_t tm;
        
        // 使用当前时间
        std::time_t t = since();
        
        // 使用我们自己封装的localtime方法获取时间
        struct tm* stm = localtime(&t);
        
        // 转换为 ngx_tm_t 格式
        tm.ngx_tm_sec = stm->tm_sec;
        tm.ngx_tm_min = stm->tm_min;
        tm.ngx_tm_hour = stm->tm_hour;
        tm.ngx_tm_mday = stm->tm_mday;
        tm.ngx_tm_mon = stm->tm_mon;
        tm.ngx_tm_year = stm->tm_year;
        tm.ngx_tm_wday = stm->tm_wday;
        
        // 构造日期字符串
        u_char buf[32];
        ngx_str_t date;
        date.data = buf;
        
        // 格式化日期为 "yyyy-mm-dd" 格式
        date.len = ngx_sprintf(buf, "%4d-%02d-%02d", 
                               tm.ngx_tm_year + 1900, 
                               tm.ngx_tm_mon + 1, 
                               tm.ngx_tm_mday) - buf;
        
        return date;
    }
    
    // 获取当前本地时间的tm结构
    static tm* localtime(time_t* tp)
    {
#ifdef _WIN32
        // Windows平台使用localtime_s函数
        static tm result;
        localtime_s(&result, tp);
        return &result;
#else
        // 非Windows平台使用nginx原生函数
        return ngx_localtime(tp);
#endif
    }

    static ngx_str_t http(std::time_t t = since())
    {
        static u_char buf[50] = {};
        auto p = ngx_http_time(buf,t);
        return ngx_str_t{static_cast<std::size_t>(p-buf), buf};
    }

    // 日期字符串转时间戳
    static std::time_t http(ngx_str_t& str)
    {
        return ngx_parse_http_time(str.data, str.len);
    }
};

#endif //TINA_BLOG_NGINX_DATETIME_HPP
