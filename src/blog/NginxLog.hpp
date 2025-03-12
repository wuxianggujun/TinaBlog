//
// Created by wuxianggujun on 2025/3/12.
//

#ifndef TINA_BLOG_NGINX_LOG_HPP
#define TINA_BLOG_NGINX_LOG_HPP

#include <boost/tti/has_type.hpp>
#include <boost/tti/has_member_data.hpp>
#include <boost/tti/has_member_function.hpp>
#include <boost/mpl/logical.hpp>
#include <boost/utility/enable_if.hpp>
#include <boost/tti/has_member_data.hpp>
#include <boost/tti/member_type.hpp>

#include "NginxWrapper.hpp"


template <ngx_uint_t level = NGX_LOG_DEBUG>
class NginxLog final : public NginxWrapper<ngx_log_t>
{
public:
    typedef NginxWrapper<ngx_log_t> super_type;
    typedef NginxLog this_type;

public:
    explicit NginxLog(ngx_log_t* log): super_type(log)
    {
    }

    explicit NginxLog(ngx_log_t& log) : super_type(log)
    {
    }

    BOOST_TTI_HAS_MEMBER_DATA(log)

    BOOST_TTI_HAS_MEMBER_DATA(connection)

    BOOST_TTI_HAS_TYPE(wrapped_type)

    template <typename T>
    explicit NginxLog(T* x, typename boost::enable_if<has_member_data_log<T, ngx_log_t*>>::type* p = 0): NginxLog(
        x->log)
    {
    }

    template <typename T>
    explicit NginxLog(T* x,
                      typename boost::enable_if<
                          boost::mpl::and_<
                              has_member_data_connection<T, ngx_connection_t*>,
                              boost::mpl::not_<has_member_data_log<T, ngx_log_t*>>
                          >
                      >::type* p = 0):
        NginxLog(x->connection)
    {
    }


    // 使用 SFINAE 的构造函数
    template <typename U>
    explicit NginxLog(
        const U& x,
        typename boost::enable_if<
            has_type_wrapped_type<U> // 检查 U 是否有 wrapped_type
        >::type* = nullptr
    ) : NginxLog(x.get())
    {
    }

    template <typename... Args>
    void print(const char* fmt, const Args&... args) const
    {
        ngx_log_error(level, get(), 0, fmt, args...);
    }

    template <typename... Args>
    void print(ngx_err_t err, const Args&... args) const
    {
        ngx_log_error(level, get(), err, args...);
    }
};

typedef NginxLog<NGX_LOG_DEBUG> NginxLogDebug;
typedef NginxLog<NGX_LOG_INFO> NginxLogInfo;
typedef NginxLog<NGX_LOG_WARN> NginxLogWarn;
typedef NginxLog<NGX_LOG_ERR> NginxLogError;
typedef NginxLog<NGX_LOG_ALERT> NginxLogAlert;

class NginxStderrLog final
{
public:
    typedef NginxStderrLog self_type;

    NginxStderrLog() = default;
    ~NginxStderrLog() = default;

    template <typename... Args>
    void print(const char* fmt, const Args&... args) const
    {
        ngx_log_stderr(0, fmt, args...);
    }

    template <typename... Args>
    void print(ngx_err_t err, const Args&... args) const
    {
        ngx_log_stderr(err, args...);
    }
};


#endif //TINA_BLOG_NGINX_LOG_HPP
