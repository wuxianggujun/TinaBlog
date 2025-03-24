#include "AuthController.hpp"
#include "blog/utils/HttpUtils.hpp"

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
    auto jsonBody = req->getJsonObject();
    
    // 检查JSON请求体
    if (!jsonBody) {
        auto resp = utils::createErrorResponse("无效的请求格式，需要JSON数据", drogon::k400BadRequest);
        callback(resp);
        return;
    }
    
    // 获取用户名和密码
    if (!(*jsonBody)["username"].isString() || !(*jsonBody)["password"].isString()) {
        auto resp = utils::createErrorResponse("缺少用户名或密码", drogon::k400BadRequest);
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
            auto resp = utils::createErrorResponse("用户名或密码错误", drogon::k401Unauthorized);
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
            auto resp = utils::createErrorResponse("用户名或密码错误", drogon::k401Unauthorized);
            callback(resp);
            return;
        }
        
        // 创建JWT令牌管理器
       JwtManager jwtManager(m_jwtSecret);
        
        // 生成JWT令牌
        std::string token = jwtManager.generateToken(userId, username, isAdmin);
        
        // 创建响应数据
        Json::Value userData;
        userData["id"] = userId;
        userData["username"] = username;
        userData["is_admin"] = isAdmin;
        userData["token"] = token;
        
        // 返回成功响应
        auto resp = utils::createSuccessResponse("登录成功", userData);
        
        // 设置Cookie
        drogon::Cookie tokenCookie("token", token);
        tokenCookie.setHttpOnly(true);
        tokenCookie.setPath("/");
        resp->addCookie(std::move(tokenCookie));
        
        callback(resp);
    } catch (const std::exception& e) {
        // 数据库查询异常
        auto resp = utils::createErrorResponse("登录过程中发生错误："+std::string(e.what()), 
                                               drogon::k500InternalServerError, 500);
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
    
    // 创建用户数据
    Json::Value userData;
    userData["id"] = userId;
    userData["username"] = username;
    userData["is_admin"] = isAdmin;
    
    // 返回成功响应
    auto resp = utils::createSuccessResponse("获取用户信息成功", userData);
    callback(resp);
}

void AuthController::registerUser(const drogon::HttpRequestPtr& req, 
                               std::function<void(const drogon::HttpResponsePtr&)>&& callback) const 
{
    auto jsonBody = req->getJsonObject();
    
    // 检查JSON请求体
    if (!jsonBody) {
        auto resp = utils::createErrorResponse("无效的请求格式，需要JSON数据", drogon::k400BadRequest);
        callback(resp);
        return;
    }
    
    // 获取用户名和密码
    if (!(*jsonBody)["username"].isString() || !(*jsonBody)["password"].isString()) {
        auto resp = utils::createErrorResponse("缺少用户名或密码", drogon::k400BadRequest);
        callback(resp);
        return;
    }
    
    std::string username = (*jsonBody)["username"].asString();
    std::string password = (*jsonBody)["password"].asString();
    
    // 检查用户名是否已存在
    auto& dbManager = DbManager::getInstance();
    
    try {
        std::string checkSql = "SELECT COUNT(*) FROM users WHERE username = '" + username + "'";
        auto checkResult = dbManager.executeQuery(checkSql);
        
        if (!checkResult.empty() && checkResult[0][0].as<int>() > 0) {
            auto resp = utils::createErrorResponse("用户名已存在", drogon::k409Conflict);
            callback(resp);
            return;
        }
        
        // 密码加密 (实际应用应使用更安全的哈希算法如bcrypt)
        // 这里暂时使用明文密码，实际生产中应当使用加密算法
        std::string hashedPassword = password;
        
        // 插入新用户
        std::string insertSql = "INSERT INTO users (username, password, is_admin, created_at) VALUES "
                                "('" + username + "', '" + hashedPassword + "', false, NOW()) RETURNING id";
        auto result = dbManager.executeQuery(insertSql);
        
        if (result.empty()) {
            auto resp = utils::createErrorResponse("注册失败，请稍后重试", drogon::k500InternalServerError);
            callback(resp);
            return;
        }
        
        int userId = result[0][0].as<int>();
        
        // 生成JWT令牌
        JwtManager jwtManager(m_jwtSecret);
        std::string token = jwtManager.generateToken(userId, username, false);
        
        // 创建响应数据
        Json::Value userData;
        userData["id"] = userId;
        userData["username"] = username;
        userData["is_admin"] = false;
        userData["token"] = token;
        
        // 返回成功响应
        auto resp = utils::createSuccessResponse("注册成功", userData);
        
        // 设置Cookie
        drogon::Cookie tokenCookie("token", token);
        tokenCookie.setHttpOnly(true);
        tokenCookie.setPath("/");
        resp->addCookie(std::move(tokenCookie));
        
        callback(resp);
    } catch (const std::exception& e) {
        // 数据库操作异常
        auto resp = utils::createErrorResponse("注册过程中发生错误：" + std::string(e.what()), 
                                               drogon::k500InternalServerError, 500);
        callback(resp);
    }
} 