#include "AuthController.hpp"
#include "../service/JwtService.hpp"
#include "../NgxLog.hpp"
#include <stdexcept>

// 获取单例实例
AuthController& AuthController::getInstance() {
    static AuthController instance;
    return instance;
}

// 注册路由
void AuthController::registerRoutes(BlogRoute& router) {
    // 登录API
    router.register_route("/api/auth/login", HttpMethod::POST_METHOD, 
        [this](NgxRequest& req, const NgxRouteParams& params) -> ngx_int_t {
            return this->handleLogin(req, params);
        });
    
    // 注册API
    router.register_route("/api/auth/register", HttpMethod::POST_METHOD, 
        [this](NgxRequest& req, const NgxRouteParams& params) -> ngx_int_t {
            return this->handleRegister(req, params);
        });
    
    // 注销API
    router.register_route("/api/auth/logout", HttpMethod::POST_METHOD, 
        [this](NgxRequest& req, const NgxRouteParams& params) -> ngx_int_t {
            return this->handleLogout(req, params);
        });
    
    // 获取当前用户信息API
    router.register_route("/api/auth/userinfo", HttpMethod::GET_METHOD, 
        [this](NgxRequest& req, const NgxRouteParams& params) -> ngx_int_t {
            return this->handleGetUserInfo(req, params);
        }, JwtVerifyLevel::JWT_REQUIRED);
    
    // 更新用户信息API
    router.register_route("/api/auth/userinfo", HttpMethod::PUT_METHOD, 
        [this](NgxRequest& req, const NgxRouteParams& params) -> ngx_int_t {
            return this->handleUpdateUserInfo(req, params);
        }, JwtVerifyLevel::JWT_REQUIRED);
}

// 处理登录请求
ngx_int_t AuthController::handleLogin(NgxRequest& req, const NgxRouteParams& params) {
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
        
        req.get_log().debug("Attempting login for user: %s", username.c_str());
        
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
        req.get_log().error("JSON parsing error: %s", e.what());
        return req.send_error(NGX_HTTP_BAD_REQUEST, std::string("Invalid JSON: ") + e.what());
    }
    catch (const std::runtime_error& e) {
        req.get_log().error("Runtime error during login: %s", e.what());
        return req.send_error(NGX_HTTP_INTERNAL_SERVER_ERROR, "Internal server error: database operation failed");
    }
    catch (const std::exception& e) {
        req.get_log().error("Login error: %s", e.what());
        return req.send_error(NGX_HTTP_INTERNAL_SERVER_ERROR, "Internal server error");
    }
    catch (...) {
        req.get_log().error("Unknown error during login");
        return req.send_error(NGX_HTTP_INTERNAL_SERVER_ERROR, "Unknown internal server error");
    }
}

// 处理注册请求
ngx_int_t AuthController::handleRegister(NgxRequest& req, const NgxRouteParams& params) {
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

// 处理注销请求
ngx_int_t AuthController::handleLogout(NgxRequest& req, const NgxRouteParams& params) {
    // 注销在客户端完成，只需返回成功响应
    json response = {
        {"message", "Logout successful"}
    };
    
    return req.send_json(response.dump());
}

// 处理获取当前用户信息请求
ngx_int_t AuthController::handleGetUserInfo(NgxRequest& req, const NgxRouteParams& params) {
    try {
        // 从JWT获取用户信息
        auto auth_header = req.get_header("Authorization");
        if (!auth_header) {
            return req.send_error(NGX_HTTP_UNAUTHORIZED, "Missing authorization header");
        }
        
        std::string token = *auth_header;
        if (token.substr(0, 7) == "Bearer ") {
            token = token.substr(7);
        }
        
        auto username_opt = JwtService::getInstance().getUsername(token);
        if (!username_opt) {
            return req.send_error(NGX_HTTP_UNAUTHORIZED, "Invalid token");
        }
        
        // 获取用户信息
        auto userInfo = UserService::getInstance().getUserInfo(*username_opt);
        if (!userInfo) {
            return req.send_error(NGX_HTTP_NOT_FOUND, "User not found");
        }
        
        return req.send_json(userInfo->dump());
    }
    catch (const std::exception& e) {
        req.get_log().error("Get user info error: %s", e.what());
        return req.send_error(NGX_HTTP_INTERNAL_SERVER_ERROR, "Internal server error");
    }
}

// 处理更新用户信息请求
ngx_int_t AuthController::handleUpdateUserInfo(NgxRequest& req, const NgxRouteParams& params) {
    std::string body = req.read_body();
    
    if (body.empty()) {
        return req.send_error(NGX_HTTP_BAD_REQUEST, "Empty request body");
    }
    
    try {
        // 从JWT获取用户ID
        auto auth_header = req.get_header("Authorization");
        if (!auth_header) {
            return req.send_error(NGX_HTTP_UNAUTHORIZED, "Missing authorization header");
        }
        
        std::string token = *auth_header;
        if (token.substr(0, 7) == "Bearer ") {
            token = token.substr(7);
        }
        
        auto user_id_opt = JwtService::getInstance().getUserId(token);
        if (!user_id_opt) {
            return req.send_error(NGX_HTTP_UNAUTHORIZED, "Invalid token");
        }
        
        int userId = *user_id_opt;
        
        // 解析JSON请求体
        json requestData = json::parse(body);
        
        // 验证请求参数
        if (requestData.empty()) {
            return req.send_error(NGX_HTTP_BAD_REQUEST, "Empty update data");
        }
        
        // 更新用户信息
        if (UserService::getInstance().updateUser(userId, requestData)) {
            json response = {
                {"message", "User information updated successfully"}
            };
            return req.send_json(response.dump());
        } else {
            return req.send_error(NGX_HTTP_BAD_REQUEST, "Failed to update user information");
        }
    }
    catch (const json::exception& e) {
        return req.send_error(NGX_HTTP_BAD_REQUEST, std::string("Invalid JSON: ") + e.what());
    }
    catch (const std::exception& e) {
        req.get_log().error("Update user info error: %s", e.what());
        return req.send_error(NGX_HTTP_INTERNAL_SERVER_ERROR, "Internal server error");
    }
} 