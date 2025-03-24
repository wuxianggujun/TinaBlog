#include "AuthController.hpp"


/**
 * 构造函数
 */
AuthController::AuthController() {
    // 在实际应用中应从配置文件中获取JWT密钥
    m_jwtSecret = "your-secret-key-change-this-in-production";
}

/**
 * 用户登录
 */
void AuthController::login(const drogon::HttpRequestPtr& req, 
                           std::function<void(const drogon::HttpResponsePtr&)>&& callback) const {
    Json::Value json;
    auto jsonBody = req->getJsonObject();
    
    // 检查JSON请求体
    if (!jsonBody) {
        json["status"] = "error";
        json["message"] = "无效的请求格式，需要JSON数据";
        auto resp = drogon::HttpResponse::newHttpJsonResponse(json);
        resp->setStatusCode(drogon::k400BadRequest);
        callback(resp);
        return;
    }
    
    // 获取用户名和密码
    if (!(*jsonBody)["username"].isString() || !(*jsonBody)["password"].isString()) {
        json["status"] = "error";
        json["message"] = "缺少用户名或密码";
        auto resp = drogon::HttpResponse::newHttpJsonResponse(json);
        resp->setStatusCode(drogon::k400BadRequest);
        callback(resp);
        return;
    }
    
    std::string username = (*jsonBody)["username"].asString();
    std::string password = (*jsonBody)["password"].asString();
    
    // 获取数据库连接
    auto& dbManager = DbManager::getInstance();
    
    try {
        // 查询用户信息
        std::string query = "SELECT id, username, password, is_admin FROM users WHERE username = '" + username + "'";
        auto result = dbManager.executeQuery(query);
        
        if (result.empty()) {
            // 用户不存在
            json["status"] = "error";
            json["message"] = "用户名或密码错误";
            auto resp = drogon::HttpResponse::newHttpJsonResponse(json);
            resp->setStatusCode(drogon::k401Unauthorized);
            callback(resp);
            return;
        }
        
        // 获取用户信息
        int userId = result[0][0].as<int>();
        std::string storedPassword = result[0][2].as<std::string>();
        bool isAdmin = result[0][3].as<bool>();
        
        // TODO: 在实际应用中应使用加密算法比较密码哈希
        if (password != storedPassword) {
            // 密码错误
            json["status"] = "error";
            json["message"] = "用户名或密码错误";
            auto resp = drogon::HttpResponse::newHttpJsonResponse(json);
            resp->setStatusCode(drogon::k401Unauthorized);
            callback(resp);
            return;
        }
        
        // 创建JWT令牌管理器
       JwtManager jwtManager(m_jwtSecret);
        
        // 生成JWT令牌
        std::string token = jwtManager.generateToken(userId, username, isAdmin);
        
        // 返回令牌
        json["status"] = "ok";
        json["message"] = "登录成功";
        json["token"] = token;
        json["user"] = Json::Value();
        json["user"]["id"] = userId;
        json["user"]["username"] = username;
        json["user"]["is_admin"] = isAdmin;
        
        auto resp = drogon::HttpResponse::newHttpJsonResponse(json);
        
        // 设置Cookie
        drogon::Cookie tokenCookie("token", token);
        tokenCookie.setHttpOnly(true);
        tokenCookie.setPath("/");
        resp->addCookie(std::move(tokenCookie));
        
        callback(resp);
    } catch (const std::exception& e) {
        // 数据库查询异常
        json["status"] = "error";
        json["message"] = "登录过程中发生错误: " + std::string(e.what());
        auto resp = drogon::HttpResponse::newHttpJsonResponse(json);
        resp->setStatusCode(drogon::k500InternalServerError);
        callback(resp);
    }
}

/**
 * 获取当前登录用户信息
 */
void AuthController::getUserInfo(const drogon::HttpRequestPtr& req, 
                                std::function<void(const drogon::HttpResponsePtr&)>&& callback) const {
    // 从请求属性中获取用户信息（已由JWT过滤器添加）
    int userId = req->getAttributes()->get<int>("user_id");
    std::string username = req->getAttributes()->get<std::string>("username");
    bool isAdmin = req->getAttributes()->get<bool>("is_admin");
    
    // 返回用户信息
    Json::Value json;
    json["status"] = "ok";
    json["user"] = Json::Value();
    json["user"]["id"] = userId;
    json["user"]["username"] = username;
    json["user"]["is_admin"] = isAdmin;
    
    auto resp = drogon::HttpResponse::newHttpJsonResponse(json);
    callback(resp);
} 