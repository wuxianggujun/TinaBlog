//
// Created by wuxianggujun on 2025/3/14.
//

#include "NgxResponse.hpp"
#include "NgxLog.hpp"
#include "NgxBuf.hpp"
#include "NgxChain.hpp"
#include "NgxTableElt.hpp"
#include <cstring>

// 应用所有设置的响应头
void NgxResponse::applyHeaders() {
    // 使用get()访问请求对象
    // 设置HTTP状态码
    get()->headers_out.status = status_;
    
    // 设置内容类型
    if (!contentType_.empty()) {
        get()->headers_out.content_type.len = contentType_.length();
        get()->headers_out.content_type.data = (u_char*)contentType_.c_str();
    }
    
    // 添加自定义响应头
    for (const auto& header : headers_) {
        // 使用NgxTableElt创建头部
        NgxTableElt h = NgxTableElt::create_header(get());
        
        if (!h.get()) {
            // 创建日志对象
            NgxLog(get()->connection->log).error("Failed to add response header: %s", header.first.c_str());
            continue;
        }
        
        // 设置头部属性
        h.set_hash()
         .set_key_direct((u_char*)header.first.c_str(), header.first.length())
         .set_value_direct((u_char*)header.second.c_str(), header.second.length());
    }
}

// 发送文本响应（std::string版本）
ngx_int_t NgxResponse::send(const std::string& content) {
    // 应用所有响应头
    applyHeaders();
    
    // 设置内容长度
    get()->headers_out.content_length_n = content.length();
    
    // 发送HTTP头
    ngx_int_t rc = ngx_http_send_header(get());
    if (rc == NGX_ERROR || rc > NGX_OK || get()->header_only) {
        return rc;
    }
    
    // 使用NgxBuf创建包含内容的缓冲区
    NgxBuf buf = NgxBuf::create_with_string(get()->pool, content);
    if (!buf.get()) {
        return NGX_HTTP_INTERNAL_SERVER_ERROR;
    }
    
    // 设置为最后一个缓冲区
    buf.set_last_buf(true).set_last_in_chain(true).set_memory(true);
    
    // 创建响应链
    NgxChain chain = NgxChain::create(get()->pool, buf);
    if (!chain.get()) {
        return NGX_HTTP_INTERNAL_SERVER_ERROR;
    }
    
    // 发送响应体
    return ngx_http_output_filter(get(), chain.get());
}

// 发送空响应（只有头部，没有响应体）
ngx_int_t NgxResponse::sendEmpty() {
    // 应用所有响应头
    applyHeaders();
    
    // 设置内容长度为0
    get()->headers_out.content_length_n = 0;
    
    // 发送HTTP头
    ngx_int_t rc = ngx_http_send_header(get());
    if (rc == NGX_ERROR || rc > NGX_OK) {
        return rc;
    }
    
    // 使用NgxBuf创建空缓冲区
    NgxBuf buf = NgxBuf::create_empty_buf(get()->pool);
    if (!buf.get()) {
        return NGX_HTTP_INTERNAL_SERVER_ERROR;
    }
    
    // 创建响应链
    NgxChain chain = NgxChain::create(get()->pool, buf);
    if (!chain.get()) {
        return NGX_HTTP_INTERNAL_SERVER_ERROR;
    }
    
    // 发送响应体
    return ngx_http_output_filter(get(), chain.get());
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
    NgxTableElt location = NgxTableElt::create_header(get());
    
    if (!location.get()) {
        // 创建日志对象
        NgxLog(get()->connection->log).error("无法创建重定向响应头");
        return NGX_HTTP_INTERNAL_SERVER_ERROR;
    }
    
    // 设置Location头
    location.set_hash()
           .set_key_direct((u_char*)"Location", sizeof("Location") - 1)
           .set_value_direct((u_char*)url.c_str(), url.length());
    
    // 设置响应头中的location属性
    get()->headers_out.location = location.get();
    
    // 应用其他响应头
    applyHeaders();
    
    // 设置内容长度为0
    get()->headers_out.content_length_n = 0;
    
    // 发送响应头
    return ngx_http_send_header(get());
} 