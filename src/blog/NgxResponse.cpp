//
// Created by wuxianggujun on 2025/3/14.
//

#include "NgxResponse.hpp"
#include "NgxLog.hpp"
#include "NgxBuf.hpp"
#include "NgxChain.hpp"
#include <cstring>

// 应用所有设置的响应头
void NgxResponse::applyHeaders() {
    // 使用this->ptr_访问请求对象
    // 设置HTTP状态码
    this->ptr_->headers_out.status = status_;
    
    // 设置内容类型
    if (!contentType_.empty()) {
        this->ptr_->headers_out.content_type.len = contentType_.length();
        this->ptr_->headers_out.content_type.data = (u_char*)contentType_.c_str();
    }
    
    // 添加自定义响应头
    for (const auto& header : headers_) {
        ngx_table_elt_t* h = static_cast<ngx_table_elt_t*>(
            ngx_list_push(&this->ptr_->headers_out.headers));
            
        if (h == nullptr) {
            // 创建新的NgxLog对象而不是复制
            NgxLog(this->ptr_->connection->log).error("Failed to add response header: %s", header.first.c_str());
            continue;
        }
        
        h->hash = 1;
        h->key.len = header.first.length();
        h->key.data = (u_char*)header.first.c_str();
        h->value.len = header.second.length();
        h->value.data = (u_char*)header.second.c_str();
    }
}

// 发送文本响应（std::string版本）
ngx_int_t NgxResponse::send(const std::string& content) {
    // 应用所有响应头
    applyHeaders();
    
    // 设置内容长度
    this->ptr_->headers_out.content_length_n = content.length();
    
    // 发送HTTP头
    ngx_int_t rc = ngx_http_send_header(this->ptr_);
    if (rc == NGX_ERROR || rc > NGX_OK || this->ptr_->header_only) {
        return rc;
    }
    
    // 使用NgxBuf创建包含内容的缓冲区
    NgxBuf buf = NgxBuf::create_with_string(this->ptr_->pool, content);
    if (!buf.get()) {
        return NGX_HTTP_INTERNAL_SERVER_ERROR;
    }
    
    // 设置为最后一个缓冲区
    buf.set_last_buf(true).set_last_in_chain(true).set_memory(true);
    
    // 创建响应链
    NgxChain chain = NgxChain::create(this->ptr_->pool, buf);
    if (!chain.get()) {
        return NGX_HTTP_INTERNAL_SERVER_ERROR;
    }
    
    // 发送响应体
    return ngx_http_output_filter(this->ptr_, chain.get());
}

// 发送空响应（只有头部，没有响应体）
ngx_int_t NgxResponse::sendEmpty() {
    // 应用所有响应头
    applyHeaders();
    
    // 设置内容长度为0
    this->ptr_->headers_out.content_length_n = 0;
    
    // 发送HTTP头
    ngx_int_t rc = ngx_http_send_header(this->ptr_);
    if (rc == NGX_ERROR || rc > NGX_OK) {
        return rc;
    }
    
    // 使用NgxBuf创建空缓冲区
    NgxBuf buf = NgxBuf::create_empty_buf(this->ptr_->pool);
    if (!buf.get()) {
        return NGX_HTTP_INTERNAL_SERVER_ERROR;
    }
    
    // 创建响应链
    NgxChain chain = NgxChain::create(this->ptr_->pool, buf);
    if (!chain.get()) {
        return NGX_HTTP_INTERNAL_SERVER_ERROR;
    }
    
    // 发送响应体
    return ngx_http_output_filter(this->ptr_, chain.get());
}

// 发送文本响应（NgxString版本）
ngx_int_t NgxResponse::send(const NgxString& content) {
    // 转换为标准字符串并调用另一个重载
    return send(content.to_string());
}

// 发送重定向响应
ngx_int_t NgxResponse::redirect(const std::string& url, ngx_uint_t status) {
    // 设置重定向状态码
    status_ = status;
    
    // 创建重定向响应头
    ngx_table_elt_t* location = static_cast<ngx_table_elt_t*>(
        ngx_list_push(&this->ptr_->headers_out.headers));
    
    if (location == nullptr) {
        // 创建新的NgxLog对象而不是复制
        NgxLog(this->ptr_->connection->log).error("无法创建重定向响应头");
        return NGX_HTTP_INTERNAL_SERVER_ERROR;
    }
    
    // 设置Location头
    location->hash = 1;
    location->key.len = sizeof("Location") - 1;
    location->key.data = (u_char*)"Location";
    location->value.len = url.length();
    location->value.data = (u_char*)url.c_str();
    
    // 设置响应头中的location属性
    this->ptr_->headers_out.location = location;
    
    // 应用其他响应头
    applyHeaders();
    
    // 设置内容长度为0
    this->ptr_->headers_out.content_length_n = 0;
    
    // 发送响应头
    return ngx_http_send_header(this->ptr_);
} 