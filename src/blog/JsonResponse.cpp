//
// Created by wuxianggujun on 2025/3/14.
//

#include "JsonResponse.hpp"
#include "NgxLog.hpp"

// 发送JSON字符串响应
ngx_int_t JsonResponse::send(ngx_http_request_t* r, const std::string& jsonContent, ngx_uint_t status) {
    // 设置响应头部
    r->headers_out.status = status;
    r->headers_out.content_type.len = sizeof("application/json") - 1;
    r->headers_out.content_type.data = (u_char *) "application/json";
    
    // 设置CORS头，允许所有来源访问
    ngx_table_elt_t *h = static_cast<ngx_table_elt_t*>(ngx_list_push(&r->headers_out.headers));
    if (h == NULL) {
        return NGX_HTTP_INTERNAL_SERVER_ERROR;
    }
    h->hash = 1;
    h->key.len = sizeof("Access-Control-Allow-Origin") - 1;
    h->key.data = (u_char *) "Access-Control-Allow-Origin";
    h->value.len = sizeof("*") - 1;
    h->value.data = (u_char *) "*";
    
    // 添加允许的HTTP方法
    h = static_cast<ngx_table_elt_t*>(ngx_list_push(&r->headers_out.headers));
    if (h == NULL) {
        return NGX_HTTP_INTERNAL_SERVER_ERROR;
    }
    h->hash = 1;
    h->key.len = sizeof("Access-Control-Allow-Methods") - 1;
    h->key.data = (u_char *) "Access-Control-Allow-Methods";
    h->value.len = sizeof("GET, POST, PUT, DELETE, OPTIONS") - 1;
    h->value.data = (u_char *) "GET, POST, PUT, DELETE, OPTIONS";
    
    // 添加允许的请求头
    h = static_cast<ngx_table_elt_t*>(ngx_list_push(&r->headers_out.headers));
    if (h == NULL) {
        return NGX_HTTP_INTERNAL_SERVER_ERROR;
    }
    h->hash = 1;
    h->key.len = sizeof("Access-Control-Allow-Headers") - 1;
    h->key.data = (u_char *) "Access-Control-Allow-Headers";
    h->value.len = sizeof("Content-Type, Authorization") - 1;
    h->value.data = (u_char *) "Content-Type, Authorization";
    
    // 设置响应长度
    r->headers_out.content_length_n = jsonContent.length();
    
    // 发送HTTP头
    ngx_int_t rc = ngx_http_send_header(r);
    if (rc == NGX_ERROR || rc > NGX_OK || r->header_only) {
        return rc;
    }
    
    // 创建缓冲区
    ngx_buf_t *b = static_cast<ngx_buf_t*>(ngx_pcalloc(r->pool, sizeof(ngx_buf_t)));
    if (b == NULL) {
        return NGX_HTTP_INTERNAL_SERVER_ERROR;
    }
    
    // 复制数据到缓冲区
    u_char *data = static_cast<u_char*>(ngx_palloc(r->pool, jsonContent.length()));
    if (data == NULL) {
        return NGX_HTTP_INTERNAL_SERVER_ERROR;
    }
    ngx_memcpy(data, jsonContent.c_str(), jsonContent.length());
    
    // 设置缓冲区
    b->pos = data;
    b->last = data + jsonContent.length();
    b->memory = 1;    // 内存缓冲区
    b->last_buf = 1;  // 最后一个缓冲区
    
    // 创建响应链
    ngx_chain_t out;
    out.buf = b;
    out.next = NULL;
    
    // 发送响应体
    return ngx_http_output_filter(r, &out);
}

// 发送JSON对象响应
ngx_int_t JsonResponse::send(ngx_http_request_t* r, const nlohmann::json& jsonObj, ngx_uint_t status) {
    try {
        // 将json对象转换为字符串并调用原来的函数
        std::string jsonContent = jsonObj.dump(2); // 缩进2个空格
        return send(r, jsonContent, status);
    }
    catch (const std::exception& e) {
        NgxLog logger(r);
        logger.error("JSON序列化失败: %s", e.what());
        
        // 返回错误响应
        nlohmann::json errorResponse = {
            {"success", false},
            {"error", {
                {"code", 500},
                {"message", "服务器内部错误：JSON序列化失败"}
            }}
        };
        
        std::string errorContent = errorResponse.dump();
        return send(r, errorContent, NGX_HTTP_INTERNAL_SERVER_ERROR);
    }
}

// 创建成功响应
nlohmann::json JsonResponse::success(const nlohmann::json& data) {
    nlohmann::json response;
    response["success"] = true;
    
    if (!data.is_null()) {
        response["data"] = data;
    }
    
    return response;
}

// 创建错误响应
nlohmann::json JsonResponse::error(const std::string& message, int code) {
    nlohmann::json response;
    response["success"] = false;
    response["error"]["code"] = code;
    response["error"]["message"] = message;
    
    return response;
} 