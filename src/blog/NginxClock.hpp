//
// Created by wuxianggujun on 2025/3/12.
//

#ifndef TINA_BLOG_NGINX_CLOCK_HPP
#define TINA_BLOG_NGINX_CLOCK_HPP

class NginxClock final
{
public:
    NginxClock() = default;
    ~NginxClock() = default;


    static const ngx_time_t& now()
    {
        ngx_time_update();
        return * ngx_timeofday();
    }

    // 计算流逝的时间
    [[nodiscard]] ngx_time_t delta() const
    {
        auto t = now();
        t.sec -= m_time.sec;
        t.msec -= m_time.msec;
        return t;
    }
    
    [[nodiscard]] double elapsed() const
    {
        const auto t = delta();
        return t.sec + t.msec * 1.0 / 1000.0;
    }

    // 复位计时器
    void reset()
    {
        m_time = now();
    }
    
private:
    // 初始化为当前时间
    ngx_time_t m_time = now();
};

#endif //TINA_BLOG_NGINX_CLOCK_HPP
