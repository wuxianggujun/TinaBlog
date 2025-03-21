//
// Created by wuxianggujun on 2025/3/13.
//

#ifndef TINA_BLOG_NGX_REQUEST_HPP
#define TINA_BLOG_NGX_REQUEST_HPP

#include "Nginx.hpp"
#include "NginxContext.hpp"
#include "NgxString.hpp"
#include "NgxPool.hpp"
#include "NgxHeaders.hpp"
#include <string>
#include <optional>
#include <string_view>
#include <unordered_map>
#include <algorithm>
#include <sstream>

#include "NgxHeadersBase.hpp"


class NgxRequestBody final : public NginxContext<ngx_http_request_t> {
public:
    
    [[nodiscard]] ngx_chain_t* bufs() const {
        return get()->request_body ? get()->request_body->bufs : nullptr;
    }

    void discard() const {
        auto rc = ngx_http_discard_request_body(get());
        // NgxException::require(rc);
    }

    template<typename F>
    ngx_int_t read(F f) const {
        auto rc = ngx_http_read_client_request_body(get(), f);
        // NgxException::fail(rc >= NGX_HTTP_SPECIAL_RESPONSE, rc);
        return NGX_DONE;
    }
};

/**
 * @brief 高性能Nginx HTTP请求(ngx_http_request_t)封装
 * 
 * 提供对ngx_http_request_t的高效封装，支持HTTP请求处理
 * 设计为轻量级包装，不拥有请求生命周期
 */
class NgxRequest : public NginxContext<ngx_http_request_t> {
public:
    /**
     * @brief 从ngx_http_request_t构造
     * @param r HTTP请求指针
     */
    explicit NgxRequest(ngx_http_request_t* r) noexcept
        : NginxContext<ngx_http_request_t>(r, false)  // 不拥有请求对象所有权
    {
    }

    /**
     * @brief 默认构造函数
     * 创建一个无效请求对象
     */
    NgxRequest() noexcept
        : NginxContext<ngx_http_request_t>(nullptr, false)
    {
    }

    /**
     * @brief 重载->运算符，方便访问请求成员
     * @return 请求指针
     */
    inline ngx_http_request_t* operator->() const noexcept {
        return ptr_;
    }

    /**
     * @brief 获取请求方法
     * @return 请求方法字符串
     */
    [[nodiscard]] inline std::string method() const noexcept {
        if (!valid()) {
            return "INVALID";
        }
        
        if (ptr_->method_name.len > 0) {
            return std::string(
                reinterpret_cast<char*>(ptr_->method_name.data),
                ptr_->method_name.len);
        }
        
        // 如果没有方法名，返回方法ID
        switch (ptr_->method) {
            case NGX_HTTP_GET:
                return "GET";
            case NGX_HTTP_POST:
                return "POST";
            case NGX_HTTP_HEAD:
                return "HEAD";
            case NGX_HTTP_PUT:
                return "PUT";
            case NGX_HTTP_DELETE:
                return "DELETE";
            case NGX_HTTP_OPTIONS:
                return "OPTIONS";
            case NGX_HTTP_PATCH:
                return "PATCH";
            default:
                return "UNKNOWN";
        }
    }

    /**
     * @brief 获取请求URI
     * @return URI字符串
     */
    [[nodiscard]] inline NgxString uri() const noexcept {
        if (!valid()) {
            return NgxString();
        }
        return NgxString(ptr_->uri, false);
    }

    /**
     * @brief 获取请求URI的字符串形式
     * @return URI字符串
     */
    [[nodiscard]] inline std::string getUri() const noexcept {
        if (!valid() || !ptr_->uri.data) {
            return "";
        }
        return std::string(reinterpret_cast<char*>(ptr_->uri.data), ptr_->uri.len);
    }

    /**
     * @brief 获取请求查询参数
     * @return 查询参数字符串
     */
    [[nodiscard]] inline NgxString args() const noexcept {
        if (!valid()) {
            return NgxString();
        }
        return NgxString(ptr_->args, false);
    }

    /**
     * @brief 获取特定的查询参数
     * @param name 参数名
     * @return 参数值，如果不存在返回空
     */
    [[nodiscard]] inline std::optional<std::string> get_arg(const std::string& name) const {
        if (!valid()) {
            return std::nullopt;
        }
        
        ngx_str_t arg_name;
        arg_name.data = reinterpret_cast<u_char*>(const_cast<char*>(name.c_str()));
        arg_name.len = name.length();
        
        ngx_str_t arg_value;
        if (ngx_http_arg(ptr_, arg_name.data, arg_name.len, &arg_value) == NGX_OK) {
            return std::string(
                reinterpret_cast<char*>(arg_value.data),
                arg_value.len);
        }
        
        return std::nullopt;
    }

    /**
     * @brief 获取HTTP版本
     * @return HTTP版本字符串
     */
    [[nodiscard]] inline std::string http_version() const noexcept {
        if (!valid()) {
            return "";
        }
        
        switch (ptr_->http_version) {
            case NGX_HTTP_VERSION_9:
                return "HTTP/0.9";
            case NGX_HTTP_VERSION_10:
                return "HTTP/1.0";
            case NGX_HTTP_VERSION_11:
                return "HTTP/1.1";
            case NGX_HTTP_VERSION_20:
                return "HTTP/2.0";
            default:
                return "UNKNOWN";
        }
    }

    /**
     * @brief 获取请求头
     * @param name 头名称
     * @return 头值，如果不存在返回空
     */
    [[nodiscard]] inline std::optional<std::string> get_header(const std::string& name) const {
        if (!valid()) {
            return std::nullopt;
        }
        
        ngx_str_t header_name;
        header_name.data = reinterpret_cast<u_char*>(const_cast<char*>(name.c_str()));
        header_name.len = name.length();
        
        // 遍历请求头部列表查找匹配项
        ngx_list_part_t* part = &ptr_->headers_in.headers.part;
        ngx_table_elt_t* headers = static_cast<ngx_table_elt_t*>(part->elts);
        
        for (ngx_uint_t i = 0; /* void */; i++) {
            if (i >= part->nelts) {
                if (part->next == nullptr) {
                    break;
                }
                
                part = part->next;
                headers = static_cast<ngx_table_elt_t*>(part->elts);
                i = 0;
            }
            
            if (headers[i].key.len == header_name.len &&
                ngx_strncasecmp(headers[i].key.data, header_name.data, header_name.len) == 0) {
                // 找到匹配的头部
                return std::string(
                    reinterpret_cast<char*>(headers[i].value.data),
                    headers[i].value.len);
            }
        }
        
        return std::nullopt;
    }

    /**
     * @brief 获取内存池
     * @return 内存池封装
     */
    [[nodiscard]] inline NgxPool pool() const noexcept {
        if (!valid()) {
            return NgxPool(nullptr);
        }
        return NgxPool(ptr_->pool, false);
    }

    /**
     * @brief 获取连接
     * @return 连接指针
     */
    [[nodiscard]] inline ngx_connection_t* connection() const noexcept {
        if (!valid()) {
            return nullptr;
        }
        return ptr_->connection;
    }

    /**
     * @brief 获取客户端IP地址
     * @return IP地址字符串
     */
    [[nodiscard]] inline std::string client_ip() const noexcept {
        if (!valid() || !ptr_->connection) {
            return "";
        }
        
        u_char text[NGX_INET6_ADDRSTRLEN + 1] = {0};
        size_t len;
        
        len = ngx_sock_ntop(ptr_->connection->sockaddr, 
                          ptr_->connection->socklen,
                          text, NGX_INET6_ADDRSTRLEN, 0);
        
        return std::string(reinterpret_cast<char*>(text), len);
    }

    /**
     * @brief 获取模块配置
     * @tparam T 配置类型
     * @param module 模块
     * @return 模块配置指针
     */
    template<typename T>
    [[nodiscard]] inline T* get_module_loc_conf(ngx_module_t& module) const {
        if (!valid()) {
            return nullptr;
        }
        return static_cast<T*>(
            ngx_http_get_module_loc_conf(ptr_, module));
    }

    /**
     * @brief 获取模块服务器配置
     * @tparam T 配置类型
     * @param module 模块
     * @return 模块服务器配置指针
     */
    template<typename T>
    [[nodiscard]] inline T* get_module_srv_conf(ngx_module_t& module) const {
        if (!valid()) {
            return nullptr;
        }
        return static_cast<T*>(
            ngx_http_get_module_srv_conf(ptr_, module));
    }

    /**
     * @brief 获取模块主配置
     * @tparam T 配置类型
     * @param module 模块
     * @return 模块主配置指针
     */
    template<typename T>
    [[nodiscard]] inline T* get_module_main_conf(ngx_module_t& module) const {
        if (!valid()) {
            return nullptr;
        }
        return static_cast<T*>(
            ngx_http_get_module_main_conf(ptr_, module));
    }

    /**
     * @brief 发送响应
     * @param status_code HTTP状态码
     * @param content_type 内容类型
     * @param content 响应内容
     * @return Nginx状态码
     */
    inline ngx_int_t send_response(ngx_uint_t status_code, 
                                 const std::string& content_type,
                                 const std::string& content) const {
        if (!valid()) {
            return NGX_ERROR;
        }
        
        // 设置状态码
        ptr_->headers_out.status = status_code;
        
        // 设置内容类型
        ptr_->headers_out.content_type.len = content_type.length();
        ptr_->headers_out.content_type.data = 
            reinterpret_cast<u_char*>(const_cast<char*>(content_type.c_str()));
        
        // 设置内容长度
        ptr_->headers_out.content_length_n = content.length();
        
        // 发送头部
        ngx_int_t rc = ngx_http_send_header(ptr_);
        if (rc == NGX_ERROR || rc > NGX_OK || ptr_->header_only) {
            return rc;
        }
        
        // 创建缓冲区
        ngx_buf_t* buffer = static_cast<ngx_buf_t*>(
            ngx_pcalloc(ptr_->pool, sizeof(ngx_buf_t)));
        
        if (!buffer) {
            return NGX_ERROR;
        }
        
        // 设置缓冲区
        buffer->pos = reinterpret_cast<u_char*>(const_cast<char*>(content.c_str()));
        buffer->last = buffer->pos + content.length();
        buffer->memory = 1;
        buffer->last_buf = 1;
        
        // 创建缓冲区链
        ngx_chain_t out;
        out.buf = buffer;
        out.next = nullptr;
        
        // 发送内容
        return ngx_http_output_filter(ptr_, &out);
    }

    /**
     * @brief 发送响应头部
     * @return Nginx状态码
     */
    inline ngx_int_t send_header() const {
        if (!valid()) {
            return NGX_ERROR;
        }
        return ngx_http_send_header(ptr_);
    }

    /**
     * @brief 设置响应状态码
     * @param status 状态码
     */
    inline void set_status(ngx_uint_t status) const {
        if (valid()) {
            ptr_->headers_out.status = status;
        }
    }

    /**
     * @brief 设置响应内容类型
     * @param content_type 内容类型
     */
    inline void set_content_type(const std::string& content_type) const {
        if (valid()) {
            ptr_->headers_out.content_type.len = content_type.length();
            ptr_->headers_out.content_type.data = 
                reinterpret_cast<u_char*>(const_cast<char*>(content_type.c_str()));
        }
    }

    /**
     * @brief 设置响应内容长度
     * @param length 内容长度
     */
    inline void set_content_length(off_t length) const {
        if (valid()) {
            ptr_->headers_out.content_length_n = length;
        }
    }

    /**
     * @brief 获取日志对象
     * @return 日志对象指针
     */
    [[nodiscard]] inline ngx_log_t* log() const noexcept {
        if (!valid() || !ptr_->connection) {
            return nullptr;
        }
        return ptr_->connection->log;
    }

    /**
     * @brief 获取请求的HTTP方法
     * @return HTTP方法代码
     */
    [[nodiscard]] inline ngx_uint_t getMethod() const noexcept {
        if (!valid()) {
            return 0;
        }
        return ptr_->method;
    }

    /**
     * @brief 获取请求体内容
     * @return 请求体字符串
     */
    [[nodiscard]] inline std::string getRequestBody() const {
        if (!valid()) {
            return "";
        }
    
        // 检查请求体是否已经读取，如果未读取则主动读取
        if (!ptr_->request_body) {
            ngx_log_error(NGX_LOG_INFO, ptr_->connection->log, 0, 
                "请求体未读取，现在自动读取");
        
            // 设置读取选项 - 尽量在内存中保存请求体
            ptr_->request_body_in_single_buf = 1;  // 尝试使用单一缓冲区
            ptr_->request_body_in_file_only = 0;   // 不要只保存在文件中
        
            // 同步读取请求体 (这会阻塞当前线程直到请求体被完全读取)
            ngx_int_t rc = ngx_http_read_client_request_body(ptr_, nullptr);
        
            if (rc != NGX_OK) {
                ngx_log_error(NGX_LOG_ERR, ptr_->connection->log, 0, 
                    "读取请求体失败，状态码: %d", rc);
                return "";
            }
        
            // 如果请求体读取失败，仍然返回空字符串
            if (!ptr_->request_body) {
                ngx_log_error(NGX_LOG_ERR, ptr_->connection->log, 0, 
                    "请求体读取失败");
                return "";
            }
        }
    
        // 从缓冲区读取请求体内容
        std::string body;
        if (ptr_->request_body->bufs) {
            ngx_chain_t* chain;
            for (chain = ptr_->request_body->bufs; chain; chain = chain->next) {
                if (chain->buf && chain->buf->pos && chain->buf->pos < chain->buf->last) {
                    size_t len = chain->buf->last - chain->buf->pos;
                    body.append(reinterpret_cast<char*>(chain->buf->pos), len);
                }
            }
        } else if (ptr_->request_body->temp_file) {
            // 从临时文件读取请求体（大请求体情况）
            ngx_log_error(NGX_LOG_WARN, ptr_->connection->log, 0, 
                "请求体保存在临时文件中，当前实现不支持从文件读取");
        }
    
        ngx_log_error(NGX_LOG_INFO, ptr_->connection->log, 0, 
            "成功读取请求体，长度: %d", body.length());
    
        return body;
    }

    /**
     * @brief 解析表单数据
     * @param postData 表单数据字符串
     * @return 解析后的表单字段映射
     */
    [[nodiscard]] inline std::unordered_map<std::string, std::string> parseFormData(const std::string& postData) const {
        std::unordered_map<std::string, std::string> formData;
        
        if (postData.empty()) {
            return formData;
        }
        
        size_t pos = 0;
        std::string token;
        std::string data = postData;
        
        // 替换所有+为空格
        std::replace(data.begin(), data.end(), '+', ' ');
        
        // 按&分割参数
        while ((pos = data.find('&')) != std::string::npos) {
            token = data.substr(0, pos);
            parseKeyValue(token, formData);
            data.erase(0, pos + 1);
        }
        
        // 处理最后一个参数
        if (!data.empty()) {
            parseKeyValue(data, formData);
        }
        
        return formData;
    }

    void finalize(ngx_int_t rc = NGX_HTTP_INTERNAL_SERVER_ERROR) const
    {
        ngx_http_finalize_request(get(), rc);
    }
    
    
    // 友元声明，允许基类访问派生类的私有方法
    friend class NginxContextBase<ngx_http_request_t, NginxContext<ngx_http_request_t>>;


private:
    /**
     * @brief 清理资源
     * NgxRequest不拥有请求对象的所有权，因此不需要清理
     */
    void cleanup_impl() noexcept {
        // 请求对象由Nginx管理，我们不需要清理它
        ptr_ = nullptr;
        owns_ptr_ = false;
    }

    /**
     * @brief 解析键值对
     * @param token 键值对字符串
     * @param formData 表单数据映射
     */
    inline void parseKeyValue(const std::string& token, std::unordered_map<std::string, std::string>& formData) const {
        size_t equalPos = token.find('=');
        if (equalPos != std::string::npos) {
            std::string key = token.substr(0, equalPos);
            std::string value = token.substr(equalPos + 1);
            
            // URL解码
            formData[urlDecode(key)] = urlDecode(value);
        }
    }
    
    /**
     * @brief URL解码
     * @param encoded 编码的字符串
     * @return 解码后的字符串
     */
    [[nodiscard]] inline std::string urlDecode(const std::string& encoded) const {
        std::string result;
        result.reserve(encoded.length());
        
        for (size_t i = 0; i < encoded.length(); ++i) {
            if (encoded[i] == '%' && i + 2 < encoded.length()) {
                int value;
                std::istringstream is(encoded.substr(i + 1, 2));
                if (is >> std::hex >> value) {
                    result += static_cast<char>(value);
                    i += 2;
                } else {
                    result += encoded[i];
                }
            } else {
                result += encoded[i];
            }
        }
        
        return result;
    }

    NgxHeadersIn m_headers_in;
    NgxRequestBody m_request_body;
};

#endif // TINA_BLOG_NGX_REQUEST_HPP 