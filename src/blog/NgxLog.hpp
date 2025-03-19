#pragma once
#include "NgxPtr.hpp"
#include <string_view>


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
    
    // 便捷方法
    void debug(std::string_view msg) const { write(Level::DEBUG, msg); }
    void info(std::string_view msg) const { write(Level::INFO, msg); }
    void notice(std::string_view msg) const { write(Level::NOTICE, msg); }
    void warn(std::string_view msg) const { write(Level::WARN, msg); }
    void error(std::string_view msg) const { write(Level::ERR, msg); }
    void crit(std::string_view msg) const { write(Level::CRIT, msg); }
    void alert(std::string_view msg) const { write(Level::ALERT, msg); }
    void emerg(std::string_view msg) const { write(Level::EMERG, msg); }
    
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