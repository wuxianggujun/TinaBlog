#include "BlogHandler.hpp"
#include "BlogModule.hpp"
#include "NgxString.hpp"
#include "NgxPool.hpp"
#include "service/UserService.hpp"
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

    // 需要先完成读取请求体的回调函数
    if (r->request_body == NULL) {
        ngx_int_t rc = ngx_http_read_client_request_body(r, [](ngx_http_request_t* r) {
            // 读取完成后重新执行handlePost
            ngx_http_finalize_request(r, BlogHandler::handlePost(r));
        });

        if (rc >= NGX_HTTP_SPECIAL_RESPONSE) {
            return rc;
        }

        return NGX_DONE; // 告诉Nginx我们将在请求体读取完成后再次处理该请求
    }

    NgxPool pool(r->pool);
    NgxString path = getRequestPath(r);
    
    ngx_log_error(NGX_LOG_DEBUG, r->connection->log, 0, 
        "Handling POST request to: %V", &path);
    
    try {
        if (path.compare("/api/auth/login") == 0) {
            return handleLogin(r);
        }
        else if (path.compare("/api/auth/register") == 0) {
            return handleRegister(r);
        }
        else if (path.compare("/api/posts") == 0) {
            return handleCreatePost(r);
        }
        
        return sendError(r, NGX_HTTP_NOT_FOUND, "API not found");
    }
    catch (const std::exception& e) {
        return sendError(r, NGX_HTTP_INTERNAL_SERVER_ERROR, e.what());
    }
}

ngx_int_t BlogHandler::handleLogin(ngx_http_request_t* r) {
    // 读取请求体
    NgxPool pool(r->pool);
    NgxString body(pool);
    ngx_int_t rc = parseRequestBody(r, body);
    if (rc != NGX_OK) {
        ngx_log_error(NGX_LOG_ERR, r->connection->log, 0, 
            "Failed to parse request body for login");
        return rc;
    }
    
    // 记录请求内容
    ngx_log_error(NGX_LOG_DEBUG, r->connection->log, 0, 
        "Login request body: %V", &body);
    
    try {
        // 解析JSON
        std::string bodyStr = body.str();
        if (bodyStr.empty()) {
            ngx_log_error(NGX_LOG_ERR, r->connection->log, 0, 
                "Empty request body for login");
            return sendError(r, NGX_HTTP_BAD_REQUEST, "Empty request body");
        }
        
        json request;
        try {
            request = json::parse(bodyStr);
        } catch (const json::exception& e) {
            ngx_log_error(NGX_LOG_ERR, r->connection->log, 0, 
                "Invalid JSON format: %s", e.what());
            return sendError(r, NGX_HTTP_BAD_REQUEST, std::string("Invalid JSON format: ") + e.what());
        }
        
        // 记录解析后的JSON
        ngx_log_error(NGX_LOG_DEBUG, r->connection->log, 0, 
            "Parsed JSON: %s", request.dump().c_str());
        
        // 验证必要字段
        if (!request.contains("username") || !request.contains("password")) {
            ngx_log_error(NGX_LOG_ERR, r->connection->log, 0, 
                "Missing username or password in login request");
            return sendError(r, NGX_HTTP_BAD_REQUEST, "Missing username or password");
        }
        
        std::string username = request["username"];
        std::string password = request["password"];
        
        ngx_log_error(NGX_LOG_DEBUG, r->connection->log, 0, 
            "Login attempt for user: %s", username.c_str());
        
        // 调用登录服务
        auto token = UserService::getInstance().login(username, password);
        
        if (!token) {
            ngx_log_error(NGX_LOG_ERR, r->connection->log, 0, 
                "Invalid credentials for user: %s", username.c_str());
            return sendError(r, NGX_HTTP_UNAUTHORIZED, "Invalid username or password");
        }
        
        // 获取用户信息
        auto userInfo = UserService::getInstance().getUserInfo(username);
        if (!userInfo) {
            ngx_log_error(NGX_LOG_ERR, r->connection->log, 0, 
                "Failed to get user info for user: %s", username.c_str());
            return sendError(r, NGX_HTTP_INTERNAL_SERVER_ERROR, "Failed to get user info");
        }
        
        // 构造响应
        json response = {
            {"token", *token},
            {"user", *userInfo}
        };
        
        ngx_log_error(NGX_LOG_DEBUG, r->connection->log, 0, 
            "Login successful for user: %s", username.c_str());
        
        return sendJsonResponse(r, response.dump());
    }
    catch (const json::exception& e) {
        ngx_log_error(NGX_LOG_ERR, r->connection->log, 0, 
            "JSON error: %s", e.what());
        return sendError(r, NGX_HTTP_BAD_REQUEST, std::string("Invalid JSON format: ") + e.what());
    }
    catch (const std::exception& e) {
        ngx_log_error(NGX_LOG_ERR, r->connection->log, 0, 
            "Error processing login: %s", e.what());
        return sendError(r, NGX_HTTP_INTERNAL_SERVER_ERROR, e.what());
    }
}

ngx_int_t BlogHandler::handleRegister(ngx_http_request_t* r) {
    // 读取请求体
    NgxPool pool(r->pool);
    NgxString body(pool);
    ngx_int_t rc = parseRequestBody(r, body);
    if (rc != NGX_OK) {
        return rc;
    }
    
    try {
        // 解析JSON
        json request = json::parse(body.str());
        
        // 验证必要字段
        if (!request.contains("username") || !request.contains("password") ||
            !request.contains("email") || !request.contains("displayName")) {
            return sendError(r, NGX_HTTP_BAD_REQUEST, "Missing required fields");
        }
        
        std::string username = request["username"];
        std::string password = request["password"];
        std::string email = request["email"];
        std::string displayName = request["displayName"];
        
        // 创建用户
        bool success = UserService::getInstance().createUser(
            username, password, displayName, email
        );
        
        if (!success) {
            return sendError(r, NGX_HTTP_BAD_REQUEST, "Username or email already exists");
        }
        
        // 自动登录
        auto token = UserService::getInstance().login(username, password);
        if (!token) {
            return sendError(r, NGX_HTTP_INTERNAL_SERVER_ERROR, "Failed to login after registration");
        }
        
        // 获取用户信息
        auto userInfo = UserService::getInstance().getUserInfo(username);
        if (!userInfo) {
            return sendError(r, NGX_HTTP_INTERNAL_SERVER_ERROR, "Failed to get user info");
        }
        
        // 构造响应
        json response = {
            {"token", *token},
            {"user", *userInfo}
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

ngx_int_t BlogHandler::handleCreatePost(ngx_http_request_t* r) {
    // 读取请求体
    NgxPool pool(r->pool);
    NgxString body(pool);
    ngx_int_t rc = parseRequestBody(r, body);
    if (rc != NGX_OK) {
        return rc;
    }
    
    try {
        // 解析JSON
        json request = json::parse(body.str());
        
        // 验证必要字段
        if (!request.contains("title") || !request.contains("content")) {
            return sendError(r, NGX_HTTP_BAD_REQUEST, "Missing title or content");
        }
        
        // TODO: 实现文章创建逻辑
        // 这里暂时返回一个模拟响应
        json response = {
            {"message", "文章创建成功"},
            {"post", {
                {"id", 1},  // 模拟ID
                {"title", request["title"]},
                {"content", request["content"]},
                {"created_at", std::time(nullptr)}
            }}
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
        ngx_log_error(NGX_LOG_ERR, r->connection->log, 0, 
            "Request body is null, you may need to call ngx_http_read_client_request_body first");
        return NGX_HTTP_BAD_REQUEST;
    }
    
    // 检查请求体是否在临时文件中
    if (r->request_body->temp_file) {
        ngx_log_error(NGX_LOG_ERR, r->connection->log, 0, 
            "Request body is in temp file, not supported");
        return NGX_HTTP_BAD_REQUEST; // 不处理大文件上传
    }
    
    // 检查请求体是否为空
    if (r->request_body->bufs == nullptr) {
        ngx_log_error(NGX_LOG_ERR, r->connection->log, 0, 
            "Request body buffers is null");
        return NGX_HTTP_BAD_REQUEST;
    }
    
    // 获取请求体缓冲区
    ngx_buf_t* buf = r->request_body->bufs->buf;
    if (!buf) {
        ngx_log_error(NGX_LOG_ERR, r->connection->log, 0, 
            "Request body buffer is null");
        return NGX_HTTP_BAD_REQUEST;
    }
    
    // 检查buf是否有内容
    if (buf->pos == nullptr || buf->last == nullptr || buf->pos == buf->last) {
        ngx_log_error(NGX_LOG_ERR, r->connection->log, 0, 
            "Request body is empty or invalid");
        return NGX_HTTP_BAD_REQUEST;
    }
    
    // 设置请求体内容
    size_t len = buf->last - buf->pos;
    ngx_log_error(NGX_LOG_DEBUG, r->connection->log, 0, 
        "Parsing request body: size=%d", len);
        
    body.set(buf->pos, len);
    
    // 记录请求体内容用于调试
    ngx_log_error(NGX_LOG_DEBUG, r->connection->log, 0, 
        "Request body content: %V", &body);
        
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