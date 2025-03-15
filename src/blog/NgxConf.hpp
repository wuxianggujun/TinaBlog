//
// Created by wuxianggujun on 2025/3/13.
//

#ifndef TINA_BLOG_NGX_CONF_HPP
#define TINA_BLOG_NGX_CONF_HPP

#include "Nginx.hpp"
#include "NginxContext.hpp"
#include "NgxString.hpp"
#include <string>
#include <vector>
#include <optional>

/**
 * @brief 高性能Nginx配置上下文(ngx_conf_t)封装
 * 
 * 提供对ngx_conf_t的高效封装，支持配置处理
 * 设计为轻量级包装，不拥有配置对象生命周期
 */
class NgxConf : public NginxContext<ngx_conf_t> {
public:
    /**
     * @brief 从ngx_conf_t构造
     * @param conf 配置上下文指针
     */
    explicit NgxConf(ngx_conf_t* conf) noexcept
        : NginxContext<ngx_conf_t>(conf, false)  // 不拥有配置对象所有权
    {
    }

    /**
     * @brief 默认构造函数
     * 创建一个无效配置对象
     */
    NgxConf() noexcept
        : NginxContext<ngx_conf_t>(nullptr, false)
    {
    }

    /**
     * @brief 重载->运算符，方便访问配置成员
     * @return 配置指针
     */
    inline ngx_conf_t* operator->() const noexcept {
        return ptr_;
    }

    /**
     * @brief 获取配置参数个数
     * @return 参数个数
     */
    [[nodiscard]] inline ngx_uint_t args_count() const noexcept {
        if (!valid() || !ptr_->args) {
            return 0;
        }
        return ptr_->args->nelts;
    }

    /**
     * @brief 获取指定索引的参数
     * @param index 参数索引，0是指令名称
     * @return 参数值，如果索引无效则返回空字符串
     */
    [[nodiscard]] inline NgxString get_arg(ngx_uint_t index) const {
        if (!valid() || !ptr_->args || index >= ptr_->args->nelts) {
            return NgxString();
        }
        
        ngx_str_t* args = static_cast<ngx_str_t*>(ptr_->args->elts);
        return NgxString(args[index], false);
    }

    /**
     * @brief 获取所有参数
     * @return 参数列表
     */
    [[nodiscard]] inline std::vector<NgxString> get_args() const {
        std::vector<NgxString> result;
        
        if (!valid() || !ptr_->args) {
            return result;
        }
        
        ngx_str_t* args = static_cast<ngx_str_t*>(ptr_->args->elts);
        result.reserve(ptr_->args->nelts);
        
        for (ngx_uint_t i = 0; i < ptr_->args->nelts; ++i) {
            result.emplace_back(args[i], false);
        }
        
        return result;
    }

    /**
     * @brief 获取指令名称（首个参数）
     * @return 指令名称
     */
    [[nodiscard]] inline NgxString directive_name() const {
        return get_arg(0);
    }

    /**
     * @brief 获取当前配置文件名
     * @return 配置文件名
     */
    [[nodiscard]] inline NgxString filename() const {
        if (!valid() || !ptr_->conf_file) {
            return NgxString();
        }
        return NgxString(ptr_->conf_file->file.name, false);
    }

    /**
     * @brief 获取当前配置行号
     * @return 行号
     */
    [[nodiscard]] inline ngx_uint_t line_number() const noexcept {
        if (!valid() || !ptr_->conf_file) {
            return 0;
        }
        return ptr_->conf_file->line;
    }

    /**
     * @brief 获取模块的上下文数据
     * @tparam T 返回类型
     * @return 模块上下文数据指针
     */
    template<typename T>
    [[nodiscard]] inline T* get_module_ctx() const noexcept {
        if (!valid() || !ptr_->ctx) {
            return nullptr;
        }
        return static_cast<T*>(ptr_->ctx);
    }

    /**
     * @brief 获取当前配置级别
     * @return 配置级别（如：NGX_CORE_CONF, NGX_HTTP_MAIN_CONF等）
     */
    [[nodiscard]] inline ngx_uint_t get_context_type() const noexcept {
        if (!valid()) {
            return 0;
        }
        return ptr_->cmd_type;
    }

    /**
     * @brief 获取配置的内存池
     * @return 内存池指针
     */
    [[nodiscard]] inline ngx_pool_t* pool() const noexcept {
        if (!valid()) {
            return nullptr;
        }
        return ptr_->pool;
    }

    /**
     * @brief 获取配置的日志对象
     * @return 日志对象指针
     */
    [[nodiscard]] inline ngx_log_t* log() const noexcept {
        if (!valid()) {
            return nullptr;
        }
        return ptr_->log;
    }

    /**
     * @brief 创建配置错误消息
     * @param fmt 格式化字符串
     * @param ... 格式化参数
     * @return NGX_CONF_ERROR
     */
    template<typename... Args>
    [[nodiscard]] inline char* error(const char* fmt, Args... args) const {
        if (!valid()) {
            return reinterpret_cast<char*>(const_cast<u_char*>(reinterpret_cast<const u_char*>(NGX_CONF_ERROR)));
        }
        
        // 使用Nginx的格式化函数创建错误消息
        // 为错误消息分配一个合理大小的缓冲区
        constexpr size_t ERROR_MSG_SIZE = 256;
        u_char* p = static_cast<u_char*>(ngx_palloc(ptr_->pool, ERROR_MSG_SIZE));
        
        if (p == nullptr) {
            return reinterpret_cast<char*>(const_cast<u_char*>(reinterpret_cast<const u_char*>(NGX_CONF_ERROR)));
        }
        
        // 使用ngx_snprintf格式化错误消息
        ngx_snprintf(p, ERROR_MSG_SIZE, fmt, args...);
        
        return reinterpret_cast<char*>(p);
    }

    /**
     * @brief 检查参数数量是否符合要求
     * @param min_args 最小参数数（包含指令名称）
     * @param max_args 最大参数数（包含指令名称），0表示无上限
     * @return 如果参数数量符合要求返回true
     */
    [[nodiscard]] inline bool check_args(ngx_uint_t min_args, ngx_uint_t max_args = 0) const noexcept {
        ngx_uint_t nelts = args_count();
        
        if (nelts < min_args) {
            return false;
        }
        
        if (max_args > 0 && nelts > max_args) {
            return false;
        }
        
        return true;
    }

    /**
     * @brief 获取指定索引的参数值
     * 这是get_arg方法的别名，提供更直观的接口
     * @param index 参数索引，0是指令名称
     * @return 参数值，如果索引无效则返回空字符串
     */
    [[nodiscard]] inline NgxString getValue(ngx_uint_t index) const {
        return get_arg(index);
    }

    // 友元声明，允许基类访问派生类的私有方法
    friend class NginxContextBase<ngx_conf_t, NginxContext<ngx_conf_t>>;

private:
    /**
     * @brief 清理资源
     * NgxConf不拥有配置对象所有权，因此不需要清理
     */
    void cleanup_impl() noexcept {
        // 配置对象由Nginx管理，我们不需要清理它
        ptr_ = nullptr;
        owns_ptr_ = false;
    }
};

#endif // TINA_BLOG_NGX_CONF_HPP 