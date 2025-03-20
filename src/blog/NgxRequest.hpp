#pragma once
#include "NgxPtr.hpp"
#include "NgxPool.hpp"
#include "NgxString.hpp"
#include <string>
#include <string_view>
#include <optional>
#include <vector>
#include <unordered_map>

/**
 * @brief Nginx HTTP 请求包装类
 * 
 * 提供了:
 * 1. 请求信息访问接口
 * 2. 请求参数解析
 * 3. 内存管理简化
 * 4. 响应生成helpers
 */
class NgxRequest : public NgxPtr<ngx_http_request_t> {
public:
    // 使用基类的构造函数
    using NgxPtr<ngx_http_request_t>::NgxPtr;
    
    // 获取请求的内存池封装对象
    [[nodiscard]] NgxPool get_pool() const {
        return NgxPool(get() ? get()->pool : nullptr);
    }
    
    // 获取日志对象
    [[nodiscard]] NgxLog get_log() const {
        return get() ? NgxLog(get()->connection->log) : NgxLog(nullptr);
    }
    
    // 获取请求URI
    [[nodiscard]] std::string_view get_uri() const {
        if (!get()) return {};
        return {reinterpret_cast<const char*>(get()->uri.data), get()->uri.len};
    }
    
    // 获取请求方法
    [[nodiscard]] std::string_view get_method() const {
        if (!get()) return {};
        return {reinterpret_cast<const char*>(get()->method_name.data), get()->method_name.len};
    }
    
    // 获取HTTP版本
    [[nodiscard]] std::string get_http_version() const {
        if (!get()) return {};
        
        switch (get()->http_version) {
            case NGX_HTTP_VERSION_9: return "HTTP/0.9";
            case NGX_HTTP_VERSION_10: return "HTTP/1.0";
            case NGX_HTTP_VERSION_11: return "HTTP/1.1";
            case NGX_HTTP_VERSION_20: return "HTTP/2.0";
            default: return "Unknown";
        }
    }
    
    // 获取请求头
    [[nodiscard]] std::optional<std::string> get_header(const std::string& name) const;
    
    // 获取所有请求头
    [[nodiscard]] std::unordered_map<std::string, std::string> get_headers() const;
    
    // 获取查询参数
    [[nodiscard]] std::optional<std::string> get_arg(const std::string& name) const;
    
    // 获取所有查询参数
    [[nodiscard]] std::unordered_map<std::string, std::string> get_args() const;
    
    // 读取请求体
    [[nodiscard]] std::string read_body();
    
    // 发送状态码响应
    ngx_int_t send_status(ngx_uint_t status_code);
    
    // 发送纯文本响应
    ngx_int_t send_text(const std::string& text, ngx_uint_t status_code = NGX_HTTP_OK);
    
    // 发送JSON响应
    ngx_int_t send_json(const std::string& json, ngx_uint_t status_code = NGX_HTTP_OK);
    
    // 发送HTML响应
    ngx_int_t send_html(const std::string& html, ngx_uint_t status_code = NGX_HTTP_OK);
    
    // 发送文件响应
    ngx_int_t send_file(const std::string& file_path);
    
    // 发送错误响应
    ngx_int_t send_error(ngx_uint_t status_code, const std::string& message = "");
    
    // 添加响应头
    bool add_header(const std::string& name, const std::string& value);
    
    // 设置内容类型
    bool set_content_type(const std::string& content_type);
    
    // 获取客户端IP
    [[nodiscard]] std::string get_client_ip() const;
    
    // 获取请求位置配置
    template<typename T>
    [[nodiscard]] T* get_loc_conf(ngx_module_t& module) const {
        if (!get()) return nullptr;
        return static_cast<T*>(ngx_http_get_module_loc_conf(get(), module));
    }
    
private:
    // 内部方法：解析查询参数
    void parse_args() const;
    
    // 缓存的查询参数
    mutable std::unordered_map<std::string, std::string> args_cache;
    mutable bool args_parsed = false;
}; 