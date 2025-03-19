#include "BlogHandler.hpp"
#include "BlogModule.hpp"
#include "NgxString.hpp"
#include "NgxPool.hpp"
#include <nlohmann/json.hpp>

using json = nlohmann::json;

ngx_int_t BlogHandler::handleRequest(ngx_http_request_t* r) {
    // 根据HTTP方法分发请求
    switch (r->method) {
        case NGX_HTTP_GET:
            return handleGet(r);
        case NGX_HTTP_POST:
            return handlePost(r);
        case NGX_HTTP_PUT:
            return handlePut(r);
        case NGX_HTTP_DELETE:
            return handleDelete(r);
        default:
            return sendError(r, NGX_HTTP_NOT_ALLOWED, "Method not allowed");
    }
}

ngx_int_t BlogHandler::handleGet(ngx_http_request_t* r) {
    if (!r || !r->pool) {
        return NGX_HTTP_INTERNAL_SERVER_ERROR;
    }

    NgxPool pool(r->pool);
    NgxString path = getRequestPath(r);
    
    try {
        json response;
        
        if (path.compare("/api/posts") == 0) {
            response = {
                {"message", "获取文章列表的API"}
            };
        } 
        else if (path.find("/api/posts/") == 0) {
            response = {
                {"message", "获取单篇文章的API"}
            };
        }
        else {
            return sendError(r, NGX_HTTP_NOT_FOUND, "API not found");
        }
        
        return sendJsonResponse(r, response.dump());
    }
    catch (const std::exception& e) {
        return sendError(r, NGX_HTTP_INTERNAL_SERVER_ERROR, e.what());
    }
}

ngx_int_t BlogHandler::handlePost(ngx_http_request_t* r) {
    if (!r || !r->pool) {
        return NGX_HTTP_INTERNAL_SERVER_ERROR;
    }

    NgxPool pool(r->pool);
    NgxString path = getRequestPath(r);
    
    if (path.compare("/api/posts") != 0) {
        return sendError(r, NGX_HTTP_NOT_FOUND, "API not found");
    }
    
    // 读取请求体
    NgxString body(pool);  // 使用已创建的 pool 对象
    ngx_int_t rc = parseRequestBody(r, body);
    if (rc != NGX_OK) {
        return rc;
    }
    
    try {
        json request = json::parse(body.str());
        json response = {
            {"message", "创建文章的API"},
            {"received", request}
        };
        
        return sendJsonResponse(r, response.dump());
    }
    catch (const json::exception&) {
        return sendError(r, NGX_HTTP_BAD_REQUEST, "Invalid JSON format");
    }
    catch (const std::exception& e) {
        return sendError(r, NGX_HTTP_INTERNAL_SERVER_ERROR, e.what());
    }
}

ngx_int_t BlogHandler::handlePut(ngx_http_request_t* r) {
    if (!r || !r->pool) {
        return NGX_HTTP_INTERNAL_SERVER_ERROR;
    }

    NgxPool pool(r->pool);
    NgxString path = getRequestPath(r);
    
    if (path.find("/api/posts/") != 0) {
        return sendError(r, NGX_HTTP_NOT_FOUND, "API not found");
    }
    
    // 读取请求体
    NgxString body(pool);  // 使用已创建的 pool 对象
    ngx_int_t rc = parseRequestBody(r, body);
    if (rc != NGX_OK) {
        return rc;
    }
    
    try {
        json request = json::parse(body.str());
        json response = {
            {"message", "更新文章的API"},
            {"received", request}
        };
        
        return sendJsonResponse(r, response.dump());
    }
    catch (const json::exception& e) {
        return sendError(r, NGX_HTTP_BAD_REQUEST, "Invalid JSON format");
    }
    catch (const std::exception& e) {
        return sendError(r, NGX_HTTP_INTERNAL_SERVER_ERROR, e.what());
    }
}

ngx_int_t BlogHandler::handleDelete(ngx_http_request_t* r) {
    NgxString path = getRequestPath(r);
    
    if (path.find("/api/posts/") != 0) {
        return sendError(r, NGX_HTTP_NOT_FOUND, "API not found");
    }
    
    try {
        json response = {
            {"message", "删除文章的API"}
        };
        
        return sendJsonResponse(r, response.dump());
    }
    catch (const std::exception& e) {
        return sendError(r, NGX_HTTP_INTERNAL_SERVER_ERROR, e.what());
    }
}

ngx_int_t BlogHandler::sendJsonResponse(ngx_http_request_t* r, const std::string& json) {
    if (!r || !r->pool) {
        return NGX_HTTP_INTERNAL_SERVER_ERROR;
    }

    NgxPool pool(r->pool);
    
    r->headers_out.status = NGX_HTTP_OK;
    r->headers_out.content_type_len = sizeof("application/json") - 1;
    r->headers_out.content_type.len = sizeof("application/json") - 1;
    r->headers_out.content_type.data = (u_char*)"application/json";
    r->headers_out.content_length_n = json.length();
    
    ngx_buf_t *b = (ngx_buf_t *)ngx_pcalloc(r->pool, sizeof(ngx_buf_t));
    if (b == nullptr) {
        return NGX_HTTP_INTERNAL_SERVER_ERROR;
    }

    ngx_chain_t *out = (ngx_chain_t *)ngx_pcalloc(r->pool, sizeof(ngx_chain_t));
    if (out == nullptr) {
        return NGX_HTTP_INTERNAL_SERVER_ERROR;
    }

    u_char *data = (u_char *)ngx_pcalloc(r->pool, json.length());
    if (data == nullptr) {
        return NGX_HTTP_INTERNAL_SERVER_ERROR;
    }

    ngx_memcpy(data, json.c_str(), json.length());

    b->pos = data;
    b->last = data + json.length();
    b->memory = 1;
    b->last_buf = 1;

    out->buf = b;
    out->next = nullptr;
    
    ngx_int_t rc = ngx_http_send_header(r);
    if (rc == NGX_ERROR || rc > NGX_OK || r->header_only) {
        return rc;
    }
    
    return ngx_http_output_filter(r, out);
}

ngx_int_t BlogHandler::sendError(ngx_http_request_t* r, ngx_uint_t status, const std::string& message) {
    if (!r) {
        return NGX_HTTP_INTERNAL_SERVER_ERROR;
    }
    
    json error = {
        {"error", message}
    };
    
    r->headers_out.status = status;
    return sendJsonResponse(r, error.dump());
}

ngx_int_t BlogHandler::parseRequestBody(ngx_http_request_t* r, NgxString& body) {
    if (r->request_body == nullptr) {
        return NGX_HTTP_BAD_REQUEST;
    }
    
    if (r->request_body->temp_file) {
        return NGX_HTTP_BAD_REQUEST; // 不处理大文件上传
    }
    
    ngx_buf_t* buf = r->request_body->bufs->buf;
    if (!buf) {
        return NGX_HTTP_BAD_REQUEST;
    }
    
    body.set(buf->pos, buf->last - buf->pos);
    return NGX_OK;
}

NgxString BlogHandler::getRequestPath(ngx_http_request_t* r) {
    if (!r || !r->pool || !r->uri.data || r->uri.len == 0) {
        return NgxString();
    }
    
    NgxPool pool(r->pool);
    return NgxString(r->uri.data, r->uri.len, pool);
}

NgxString BlogHandler::getQueryParam(ngx_http_request_t* r, const NgxString& name) {
    if (!r || !r->pool || !r->args.data || r->args.len == 0) {
        return NgxString();
    }
    
    NgxPool pool(r->pool);
    NgxString args(r->args.data, r->args.len, pool);
    auto search_str = pool.create_string("=");
    
    NgxString search = name;
    if (!search.append(search_str.str().c_str(), pool)) {
        return NgxString();
    }
    
    size_t pos = args.find(search.str().c_str());
    if (pos == std::string::npos) {
        return NgxString(pool);
    }
    
    pos += search.length();
    size_t end = args.find("&", pos);
    if (end == std::string::npos) {
        end = args.length();
    }
    
    return args.substr(pos, end - pos, pool);
} 