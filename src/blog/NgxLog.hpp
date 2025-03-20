#pragma once
#include "NgxPtr.hpp"
#include <string_view>
#include <string>
#include <cstdarg>
#include <memory>
#include <vector>


/**
 * @brief Nginx日志包装类
 */
class NgxLog : public NgxPtr<ngx_log_t> {
public:
    // 使用基类的构造函数
    using NgxPtr<ngx_log_t>::NgxPtr;
    
    // 日志级别
    enum class Level {
        STDERR = NGX_LOG_STDERR,
        EMERG = NGX_LOG_EMERG,
        ALERT = NGX_LOG_ALERT,
        CRIT = NGX_LOG_CRIT,
        ERR = NGX_LOG_ERR,
        WARN = NGX_LOG_WARN,
        NOTICE = NGX_LOG_NOTICE,
        INFO = NGX_LOG_INFO,
        DEBUG = NGX_LOG_DEBUG
    };
    
    // 写日志
    void write(Level level, std::string_view msg) const {
        if (ptr_) {
            ngx_log_error(static_cast<ngx_uint_t>(level), ptr_, 0, "%*s", 
                         msg.length(), msg.data());
        }
    }

    // 格式化写日志
    void write_fmt(Level level, const char* fmt, ...) const {
        if (!ptr_) return;
        
        va_list args;
        va_start(args, fmt);
        
        char buf[NGX_MAX_ERROR_STR];
        vsnprintf(buf, sizeof(buf), fmt, args);
        
        va_end(args);
        
        write(level, buf);
    }
    
    // 便捷方法 - 支持string_view
    void debug(std::string_view msg) const { write(Level::DEBUG, msg); }
    void info(std::string_view msg) const { write(Level::INFO, msg); }
    void notice(std::string_view msg) const { write(Level::NOTICE, msg); }
    void warn(std::string_view msg) const { write(Level::WARN, msg); }
    void error(std::string_view msg) const { write(Level::ERR, msg); }
    void crit(std::string_view msg) const { write(Level::CRIT, msg); }
    void alert(std::string_view msg) const { write(Level::ALERT, msg); }
    void emerg(std::string_view msg) const { write(Level::EMERG, msg); }

    // 便捷方法 - 支持printf风格格式化
    void debug(const char* fmt, ...) const {
        va_list args;
        va_start(args, fmt);
        char buf[NGX_MAX_ERROR_STR];
        vsnprintf(buf, sizeof(buf), fmt, args);
        va_end(args);
        write(Level::DEBUG, buf);
    }
    
    void info(const char* fmt, ...) const {
        va_list args;
        va_start(args, fmt);
        char buf[NGX_MAX_ERROR_STR];
        vsnprintf(buf, sizeof(buf), fmt, args);
        va_end(args);
        write(Level::INFO, buf);
    }
    
    void notice(const char* fmt, ...) const {
        va_list args;
        va_start(args, fmt);
        char buf[NGX_MAX_ERROR_STR];
        vsnprintf(buf, sizeof(buf), fmt, args);
        va_end(args);
        write(Level::NOTICE, buf);
    }
    
    void warn(const char* fmt, ...) const {
        va_list args;
        va_start(args, fmt);
        char buf[NGX_MAX_ERROR_STR];
        vsnprintf(buf, sizeof(buf), fmt, args);
        va_end(args);
        write(Level::WARN, buf);
    }
    
    void error(const char* fmt, ...) const {
        va_list args;
        va_start(args, fmt);
        char buf[NGX_MAX_ERROR_STR];
        vsnprintf(buf, sizeof(buf), fmt, args);
        va_end(args);
        write(Level::ERR, buf);
    }
    
    void crit(const char* fmt, ...) const {
        va_list args;
        va_start(args, fmt);
        char buf[NGX_MAX_ERROR_STR];
        vsnprintf(buf, sizeof(buf), fmt, args);
        va_end(args);
        write(Level::CRIT, buf);
    }
    
    void alert(const char* fmt, ...) const {
        va_list args;
        va_start(args, fmt);
        char buf[NGX_MAX_ERROR_STR];
        vsnprintf(buf, sizeof(buf), fmt, args);
        va_end(args);
        write(Level::ALERT, buf);
    }
    
    void emerg(const char* fmt, ...) const {
        va_list args;
        va_start(args, fmt);
        char buf[NGX_MAX_ERROR_STR];
        vsnprintf(buf, sizeof(buf), fmt, args);
        va_end(args);
        write(Level::EMERG, buf);
    }
    
    // 获取日志级别
    Level get_level() const noexcept {
        return ptr_ ? static_cast<Level>(ptr_->log_level) : Level::ERR;
    }
    
    // 设置日志级别
    void set_level(Level level) noexcept {
        if (ptr_) {
            ptr_->log_level = static_cast<ngx_uint_t>(level);
        }
    }
}; // class NgxLog