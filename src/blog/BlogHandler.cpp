#include "BlogHandler.hpp"
#include "BlogModule.hpp"
#include "NgxString.hpp"
#include "NgxPool.hpp"
#include "service/UserService.hpp"
#include <nlohmann/json.hpp>

using json = nlohmann::json;

// 初始化静态成员
std::unordered_map<std::string, BlogHandler::RouteHandler> BlogHandler::getRoutes;
std::unordered_map<std::string, BlogHandler::RouteHandler> BlogHandler::postRoutes;
std::unordered_map<std::string, BlogHandler::RouteHandler> BlogHandler::putRoutes;
std::unordered_map<std::string, BlogHandler::RouteHandler> BlogHandler::deleteRoutes;
std::mutex BlogHandler::routesMutex;

// 初始化路由
void BlogHandler::initRoutes() {
    std::lock_guard<std::mutex> lock(routesMutex);
    
    // GET路由
    getRoutes["/api/posts"] = [](NgxRequest& req) {
        json response = {{"message", "获取文章列表的API"}};
        return req.send_json(response.dump());
    };
    
    getRoutes["/api/health"] = &BlogHandler::handleHealthCheck;
    
    // POST路由
    postRoutes["/api/auth/login"] = &BlogHandler::handleLogin;
    postRoutes["/api/auth/register"] = &BlogHandler::handleRegister;
    postRoutes["/api/posts"] = &BlogHandler::handleCreatePost;
    
    // PUT路由
    putRoutes["/api/posts"] = &BlogHandler::handleUpdatePost;
    
    // DELETE路由
    deleteRoutes["/api/posts"] = &BlogHandler::handleDeletePost;
}

ngx_int_t BlogHandler::handleRequest(ngx_http_request_t* r) {
    // 创建NgxRequest包装器
    NgxRequest req(r);
    
    // 根据HTTP方法分发请求
    switch (r->method) {
        case NGX_HTTP_GET:
            return handleGet(req);
        case NGX_HTTP_POST:
            return handlePost(req);
        case NGX_HTTP_PUT:
            return handlePut(req);
        case NGX_HTTP_DELETE:
            return handleDelete(req);
        case NGX_HTTP_OPTIONS:
            return handleOptions(req);
        default:
            return req.send_error(NGX_HTTP_NOT_ALLOWED, "Method not allowed");
    }
}

ngx_int_t BlogHandler::handleGet(NgxRequest& req) {
    if (!req.valid()) {
        return NGX_HTTP_INTERNAL_SERVER_ERROR;
    }

    std::string path = getRequestPath(req);
    req.get_log().debug("Handling GET request to: %s", path.c_str());
    
    // 查找路由
    {
        std::lock_guard<std::mutex> lock(routesMutex);
        auto it = getRoutes.find(path);
        if (it != getRoutes.end()) {
            return it->second(req);
        }
    }
    
    // 检查是否是获取单篇文章的请求
    if (path.find("/api/posts/") == 0) {
        json response = {{"message", "获取单篇文章的API"}};
        return req.send_json(response.dump());
    }
    
    return req.send_error(NGX_HTTP_NOT_FOUND, "API not found");
}

ngx_int_t BlogHandler::handlePost(NgxRequest& req) {
    if (!req.valid()) {
        return NGX_HTTP_INTERNAL_SERVER_ERROR;
    }

    // 如果请求体还未读取，需要设置读取回调
    if (req.get()->request_body == nullptr) {
        ngx_int_t rc = ngx_http_read_client_request_body(req.get(), [](ngx_http_request_t* r) {
            // 读取完成后重新执行handleRequest
            NgxRequest new_req(r);
            ngx_http_finalize_request(r, BlogHandler::handlePost(new_req));
        });

        if (rc >= NGX_HTTP_SPECIAL_RESPONSE) {
            return rc;
        }

        return NGX_DONE; // 告诉Nginx我们将在请求体读取完成后再次处理
    }

    std::string path = getRequestPath(req);
    req.get_log().debug("Handling POST request to: %s", path.c_str());
    
    // 查找路由
    {
        std::lock_guard<std::mutex> lock(routesMutex);
        auto it = postRoutes.find(path);
        if (it != postRoutes.end()) {
            return it->second(req);
        }
    }
    
    return req.send_error(NGX_HTTP_NOT_FOUND, "API not found");
}

// 获取健康检查
ngx_int_t BlogHandler::handleHealthCheck(NgxRequest& req) {
    try {
        bool dbConnected = DbManager::getInstance().isConnected();
        
        json response = {
            {"status", dbConnected ? "healthy" : "unhealthy"},
            {"database", dbConnected ? "connected" : "disconnected"},
            {"timestamp", std::time(nullptr)}
        };
        
        return req.send_json(response.dump());
    } catch (const std::exception& e) {
        return req.send_error(NGX_HTTP_INTERNAL_SERVER_ERROR, e.what());
    }
}

// 获取请求路径
std::string BlogHandler::getRequestPath(const NgxRequest& req) {
    return std::string(req.get_uri());
}

ngx_int_t BlogHandler::handlePut(NgxRequest& req) {
    if (!req.valid()) {
        return NGX_HTTP_INTERNAL_SERVER_ERROR;
    }

    // 如果请求体还未读取，需要设置读取回调
    if (req.get()->request_body == nullptr) {
        ngx_int_t rc = ngx_http_read_client_request_body(req.get(), [](ngx_http_request_t* r) {
            NgxRequest new_req(r);
            ngx_http_finalize_request(r, BlogHandler::handlePut(new_req));
        });

        if (rc >= NGX_HTTP_SPECIAL_RESPONSE) {
            return rc;
        }

        return NGX_DONE;
    }

    std::string path = getRequestPath(req);
    req.get_log().debug("Handling PUT request to: %s", path.c_str());
    
    // 查找路由
    {
        std::lock_guard<std::mutex> lock(routesMutex);
        auto it = putRoutes.find(path);
        if (it != putRoutes.end()) {
            return it->second(req);
        }
    }
    
    return req.send_error(NGX_HTTP_NOT_FOUND, "API not found");
}

ngx_int_t BlogHandler::handleDelete(NgxRequest& req) {
    if (!req.valid()) {
        return NGX_HTTP_INTERNAL_SERVER_ERROR;
    }

    std::string path = getRequestPath(req);
    req.get_log().debug("Handling DELETE request to: %s", path.c_str());
    
    // 查找路由
    {
        std::lock_guard<std::mutex> lock(routesMutex);
        auto it = deleteRoutes.find(path);
        if (it != deleteRoutes.end()) {
            return it->second(req);
        }
    }
    
    return req.send_error(NGX_HTTP_NOT_FOUND, "API not found");
}

ngx_int_t BlogHandler::handleOptions(NgxRequest& req) {
    if (!req.valid()) {
        return NGX_HTTP_INTERNAL_SERVER_ERROR;
    }
    
    // 添加CORS头
    req.add_header("Access-Control-Allow-Origin", "*");
    req.add_header("Access-Control-Allow-Methods", "GET, POST, PUT, DELETE, OPTIONS");
    req.add_header("Access-Control-Allow-Headers", "Content-Type, Authorization");
    req.add_header("Access-Control-Max-Age", "86400");
    
    return req.send_status(NGX_HTTP_OK);
}

ngx_int_t BlogHandler::handleLogin(NgxRequest& req) {
    std::string body = req.read_body();
    
    if (body.empty()) {
        return req.send_error(NGX_HTTP_BAD_REQUEST, "Empty request body");
    }
    
    try {
        // 解析JSON请求体
        json requestData = json::parse(body);
        
        // 验证请求参数
        if (!requestData.contains("username") || !requestData.contains("password")) {
            return req.send_error(NGX_HTTP_BAD_REQUEST, "Missing username or password");
        }
        
        std::string username = requestData["username"];
        std::string password = requestData["password"];
        
        // 验证登录
        auto token = UserService::getInstance().login(username, password);
        
        if (!token) {
            return req.send_error(NGX_HTTP_UNAUTHORIZED, "Invalid username or password");
        }
        
        // 返回token
        json response = {
            {"token", *token},
            {"message", "Login successful"}
        };
        
        return req.send_json(response.dump());
    }
    catch (const json::exception& e) {
        return req.send_error(NGX_HTTP_BAD_REQUEST, std::string("Invalid JSON: ") + e.what());
    }
    catch (const std::exception& e) {
        req.get_log().error("Login error: %s", e.what());
        return req.send_error(NGX_HTTP_INTERNAL_SERVER_ERROR, "Internal server error");
    }
}

ngx_int_t BlogHandler::handleRegister(NgxRequest& req) {
    std::string body = req.read_body();
    
    if (body.empty()) {
        return req.send_error(NGX_HTTP_BAD_REQUEST, "Empty request body");
    }
    
    try {
        // 解析JSON请求体
        json requestData = json::parse(body);
        
        // 验证请求参数
        if (!requestData.contains("username") || 
            !requestData.contains("password") || 
            !requestData.contains("email") ||
            !requestData.contains("displayName")) {
            return req.send_error(NGX_HTTP_BAD_REQUEST, "Missing required fields");
        }
        
        std::string username = requestData["username"];
        std::string password = requestData["password"];
        std::string email = requestData["email"];
        std::string displayName = requestData["displayName"];
        
        // 执行注册
        if (UserService::getInstance().createUser(username, password, displayName, email)) {
            // 注册成功，自动登录
            auto token = UserService::getInstance().login(username, password);
            
            json response = {
                {"token", token.value_or("")},
                {"message", "Registration successful"}
            };
            
            return req.send_json(response.dump());
        } else {
            return req.send_error(NGX_HTTP_BAD_REQUEST, "Registration failed, username or email may already exist");
        }
    }
    catch (const json::exception& e) {
        return req.send_error(NGX_HTTP_BAD_REQUEST, std::string("Invalid JSON: ") + e.what());
    }
    catch (const std::exception& e) {
        req.get_log().error("Registration error: %s", e.what());
        return req.send_error(NGX_HTTP_INTERNAL_SERVER_ERROR, "Internal server error");
    }
}

ngx_int_t BlogHandler::handleCreatePost(NgxRequest& req) {
    std::string body = req.read_body();
    
    if (body.empty()) {
        return req.send_error(NGX_HTTP_BAD_REQUEST, "Empty request body");
    }
    
    try {
        // 获取认证信息
        auto auth_header = req.get_header("Authorization");
        if (!auth_header) {
            return req.send_error(NGX_HTTP_UNAUTHORIZED, "Missing authorization header");
        }
        
        std::string token = *auth_header;
        // 移除"Bearer "前缀
        if (token.substr(0, 7) == "Bearer ") {
            token = token.substr(7);
        }
        
        // TODO: 验证token并获取用户ID
        
        // 解析JSON请求体
        json requestData = json::parse(body);
        
        // 验证请求参数
        if (!requestData.contains("title") || 
            !requestData.contains("content")) {
            return req.send_error(NGX_HTTP_BAD_REQUEST, "Missing required fields");
        }
        
        // 模拟创建博客文章
        json response = {
            {"id", 1},
            {"title", requestData["title"]},
            {"message", "Post created successfully"}
        };
        
        return req.send_json(response.dump());
    }
    catch (const json::exception& e) {
        return req.send_error(NGX_HTTP_BAD_REQUEST, std::string("Invalid JSON: ") + e.what());
    }
    catch (const std::exception& e) {
        req.get_log().error("Create post error: %s", e.what());
        return req.send_error(NGX_HTTP_INTERNAL_SERVER_ERROR, "Internal server error");
    }
}

ngx_int_t BlogHandler::handleUpdatePost(NgxRequest& req) {
    std::string body = req.read_body();
    
    if (body.empty()) {
        return req.send_error(NGX_HTTP_BAD_REQUEST, "Empty request body");
    }
    
    try {
        // 获取认证信息
        auto auth_header = req.get_header("Authorization");
        if (!auth_header) {
            return req.send_error(NGX_HTTP_UNAUTHORIZED, "Missing authorization header");
        }
        
        // 解析JSON请求体
        json requestData = json::parse(body);
        
        // 验证请求参数
        if (!requestData.contains("id")) {
            return req.send_error(NGX_HTTP_BAD_REQUEST, "Missing post id");
        }
        
        // 模拟更新博客文章
        json response = {
            {"id", requestData["id"]},
            {"message", "Post updated successfully"}
        };
        
        return req.send_json(response.dump());
    }
    catch (const json::exception& e) {
        return req.send_error(NGX_HTTP_BAD_REQUEST, std::string("Invalid JSON: ") + e.what());
    }
    catch (const std::exception& e) {
        req.get_log().error("Update post error: %s", e.what());
        return req.send_error(NGX_HTTP_INTERNAL_SERVER_ERROR, "Internal server error");
    }
}

ngx_int_t BlogHandler::handleDeletePost(NgxRequest& req) {
    try {
        // 获取认证信息
        auto auth_header = req.get_header("Authorization");
        if (!auth_header) {
            return req.send_error(NGX_HTTP_UNAUTHORIZED, "Missing authorization header");
        }
        
        // 获取请求参数中的post_id
        auto post_id = req.get_arg("id");
        if (!post_id) {
            return req.send_error(NGX_HTTP_BAD_REQUEST, "Missing post id");
        }
        
        // 模拟删除博客文章
        json response = {
            {"id", *post_id},
            {"message", "Post deleted successfully"}
        };
        
        return req.send_json(response.dump());
    }
    catch (const std::exception& e) {
        req.get_log().error("Delete post error: %s", e.what());
        return req.send_error(NGX_HTTP_INTERNAL_SERVER_ERROR, "Internal server error");
    }
} 