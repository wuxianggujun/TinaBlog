// 
// Created by wuxianggujun on 2025/3/14.
//

#pragma once

#include <ngx_config.h>
#include <ngx_core.h>
#include <ngx_http.h>
#include <string>
#include <cstdarg>
#include "NginxContext.hpp"

/**
 * @brief NgxLog类封装了Nginx的日志功能，提供了更方便的日志记录接口
 */
class NgxLog : public NginxContext<ngx_log_t> {
public:
    /**
     * @brief 构造函数，使用当前请求创建日志记录器
     * @param r 当前HTTP请求对象
     */
    explicit NgxLog(ngx_http_request_t* r) 
        : NginxContext<ngx_log_t>(r && r->connection ? r->connection->log : nullptr, false),
          r_(r) {}
    
    /**
     * @brief 构造函数，使用日志对象创建日志记录器
     * @param log Nginx日志对象
     * @param take_ownership 是否接管日志对象所有权
     */
    explicit NgxLog(ngx_log_t* log, bool take_ownership = false)
        : NginxContext<ngx_log_t>(log, take_ownership),
          r_(nullptr) {}
          
    /**
     * @brief 默认构造函数
     * 创建一个无效日志对象
     */
    NgxLog() noexcept
        : NginxContext<ngx_log_t>(nullptr, false),
          r_(nullptr) {}

    /**
     * @brief 记录DEBUG级别日志
     * @param fmt 格式化字符串
     * @param ... 可变参数
     */
    void debug(const char* fmt, ...) const {
        va_list args;
        va_start(args, fmt);
        log_va(NGX_LOG_DEBUG, fmt, args);
        va_end(args);
    }

    /**
     * @brief 记录INFO级别日志
     * @param fmt 格式化字符串
     * @param ... 可变参数
     */
    void info(const char* fmt, ...) const {
        va_list args;
        va_start(args, fmt);
        log_va(NGX_LOG_INFO, fmt, args);
        va_end(args);
    }

    /**
     * @brief 记录NOTICE级别日志
     * @param fmt 格式化字符串
     * @param ... 可变参数
     */
    void notice(const char* fmt, ...) const {
        va_list args;
        va_start(args, fmt);
        log_va(NGX_LOG_NOTICE, fmt, args);
        va_end(args);
    }

    /**
     * @brief 记录WARN级别日志
     * @param fmt 格式化字符串
     * @param ... 可变参数
     */
    void warn(const char* fmt, ...) const {
        va_list args;
        va_start(args, fmt);
        log_va(NGX_LOG_WARN, fmt, args);
        va_end(args);
    }

    /**
     * @brief 记录ERROR级别日志
     * @param fmt 格式化字符串
     * @param ... 可变参数
     */
    void error(const char* fmt, ...) const {
        va_list args;
        va_start(args, fmt);
        log_va(NGX_LOG_ERR, fmt, args);
        va_end(args);
    }

    /**
     * @brief 记录CRIT级别日志
     * @param fmt 格式化字符串
     * @param ... 可变参数
     */
    void crit(const char* fmt, ...) const {
        va_list args;
        va_start(args, fmt);
        log_va(NGX_LOG_CRIT, fmt, args);
        va_end(args);
    }

    /**
     * @brief 记录ALERT级别日志
     * @param fmt 格式化字符串
     * @param ... 可变参数
     */
    void alert(const char* fmt, ...) const {
        va_list args;
        va_start(args, fmt);
        log_va(NGX_LOG_ALERT, fmt, args);
        va_end(args);
    }

    /**
     * @brief 记录EMERG级别日志
     * @param fmt 格式化字符串
     * @param ... 可变参数
     */
    void emerg(const char* fmt, ...) const {
        va_list args;
        va_start(args, fmt);
        log_va(NGX_LOG_EMERG, fmt, args);
        va_end(args);
    }

    /**
     * @brief 记录指定级别的日志
     * @param level 日志级别
     * @param fmt 格式化字符串
     * @param ... 可变参数
     */
    void log(ngx_uint_t level, const char* fmt, ...) const {
        va_list args;
        va_start(args, fmt);
        log_va(level, fmt, args);
        va_end(args);
    }

    /**
     * @brief 静态方法，记录指定级别的日志
     * @param log Nginx日志对象
     * @param level 日志级别
     * @param fmt 格式化字符串
     * @param ... 可变参数
     */
    static void log_static(ngx_log_t* log, ngx_uint_t level, const char* fmt, ...) {
        va_list args;
        va_start(args, fmt);
        ngx_log_error_core(level, log, 0, fmt, args);
        va_end(args);
    }
    
    /**
     * @brief 重载->运算符，方便访问日志对象成员
     * @return 日志对象指针
     */
    inline ngx_log_t* operator->() const noexcept {
        return ptr_;
    }

    // 友元声明，允许基类访问派生类的私有方法
    friend class NginxContextBase<ngx_log_t, NginxContext<ngx_log_t>>;

private:
    ngx_http_request_t* r_;  // 保存请求对象的引用，用于日志记录

    /**
     * @brief 使用可变参数记录日志
     * @param level 日志级别
     * @param fmt 格式化字符串
     * @param args 可变参数列表
     */
    void log_va(ngx_uint_t level, const char* fmt, va_list args) const {
        if (valid()) {
            // 创建临时缓冲区存储格式化后的字符串
            char buffer[2048];
            vsnprintf(buffer, sizeof(buffer), fmt, args);
            
            // 使用Nginx的日志函数记录
            ngx_log_error(level, ptr_, 0, "%s", buffer);
        } else if (r_ && r_->connection && r_->connection->log) {
            // 回退到请求的日志对象
            char buffer[2048];
            vsnprintf(buffer, sizeof(buffer), fmt, args);
            
            ngx_log_error(level, r_->connection->log, 0, "%s", buffer);
        }
    }
    
    /**
     * @brief 清理资源
     * 如果拥有日志对象所有权，则释放资源
     */
    void cleanup_impl() noexcept{
        // 在这个类中，我们通常不拥有日志对象的所有权
        // 如果owns_ptr_为true，则需要释放资源，但目前Nginx没有提供释放日志对象的API
        // 所以这里只是重置指针
        ptr_ = nullptr;
        owns_ptr_ = false;
    }
}; 