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
    
    // 检查认证方式首选项（如果有）
    bool returnTokenInBody = true;
    if ((*jsonBody)["auth_type"].isString()) {
        std::string authType = (*jsonBody)["auth_type"].asString();
        returnTokenInBody = (authType == "token" || authType == "both");
    }
    
    // 获取数据库连接
    auto& dbManager = DbManager::getInstance();
    
    try {
        // 查询用户信息 - 修改为使用UUID
        std::string query = "SELECT uuid, username, password, is_admin, display_name FROM users WHERE username = '" + username + "'";
        auto result = dbManager.executeQuery(query);
        
        if (result.empty()) {
            // 用户不存在
            auto resp = utils::createErrorResponse("用户名或密码错误", drogon::k401Unauthorized);
            callback(resp);
            return;
        }
        
        // 获取用户信息
        std::string userUuid = result[0][0].as<std::string>();
        std::string storedPassword = result[0][2].as<std::string>();
        bool isAdmin = result[0][3].as<bool>();
        std::string displayName = result[0][4].as<std::string>();
        
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
        std::string token = jwtManager.generateToken(userUuid, username, isAdmin);
        
        // 创建精简的响应数据
        Json::Value userData;
        userData["uuid"] = userUuid;
        userData["username"] = username;
        userData["display_name"] = displayName;
        
        // 如果客户端需要，在响应体中返回token
        if (returnTokenInBody) {
            userData["token"] = token;
        }
        
        // 返回成功响应
        auto resp = utils::createSuccessResponse("登录成功", userData);
        
        // 默认总是在Cookie中设置token，便于浏览器环境
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
    std::string userUuid = req->getAttributes()->get<std::string>("user_uuid");
    std::string username = req->getAttributes()->get<std::string>("username");
    bool isAdmin = req->getAttributes()->get<bool>("is_admin");
    
    // 创建用户数据
    Json::Value userData;
    userData["uuid"] = userUuid;
    userData["username"] = username;
    userData["is_admin"] = isAdmin;
    
    // 尝试获取更多用户信息
    auto& dbManager = DbManager::getInstance();
    try {
        std::string query = "SELECT email, display_name FROM users WHERE uuid = '" + userUuid + "'";
        auto result = dbManager.executeQuery(query);
        
        if (!result.empty()) {
            userData["email"] = result[0][0].as<std::string>();
            userData["display_name"] = result[0][1].as<std::string>();
        }
    } catch (const std::exception& e) {
        // 如果查询失败，只是记录错误，仍然返回基本信息
        std::cerr << "Error fetching additional user info: " << e.what() << std::endl;
    }
    
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
    
    // 检查认证方式首选项（如果有）
    bool returnTokenInBody = true;
    if ((*jsonBody)["auth_type"].isString()) {
        std::string authType = (*jsonBody)["auth_type"].asString();
        returnTokenInBody = (authType == "token" || authType == "both");
    }
    
    // 获取可选字段
    std::string email = (*jsonBody)["email"].isString() ? (*jsonBody)["email"].asString() : username + "@example.com"; // 默认邮箱
    std::string display_name = (*jsonBody)["display_name"].isString() ? (*jsonBody)["display_name"].asString() : username; // 默认显示名称
    
    // 确保重要字段不为空
    if (username.empty() || password.empty() || email.empty()) {
        auto resp = utils::createErrorResponse("用户名、密码和邮箱不能为空", drogon::k400BadRequest);
        callback(resp);
        return;
    }
    
    // 生成UUID
    std::string uuid = drogon::utils::getUuid();
    
    // 检查用户名是否已存在
    auto& dbManager = DbManager::getInstance();
    
    try {
        // 检查用户名是否已存在
        std::string checkSql = "SELECT COUNT(*) FROM users WHERE username = '" + username + "'";
        auto checkResult = dbManager.executeQuery(checkSql);
        
        if (!checkResult.empty() && checkResult[0][0].as<int>() > 0) {
            auto resp = utils::createErrorResponse("用户名已存在", drogon::k409Conflict);
            callback(resp);
            return;
        }
        
        // 检查邮箱是否已存在
        checkSql = "SELECT COUNT(*) FROM users WHERE email = '" + email + "'";
        checkResult = dbManager.executeQuery(checkSql);
        
        if (!checkResult.empty() && checkResult[0][0].as<int>() > 0) {
            auto resp = utils::createErrorResponse("邮箱已被注册", drogon::k409Conflict);
            callback(resp);
            return;
        }
        
        // 密码加密 (实际应用应使用更安全的哈希算法如bcrypt)
        // 这里暂时使用明文密码，实际生产中应当使用加密算法
        std::string hashedPassword = password;
        
        // 插入新用户 - 使用UUID作为主键
        std::string insertSql = "INSERT INTO users (uuid, username, password, email, display_name, is_admin, created_at, updated_at) VALUES "
                                "('" + uuid + "', '" + username + "', '" + hashedPassword + "', '" + email + "', '" + display_name + "', false, NOW(), NOW()) RETURNING uuid";
        auto result = dbManager.executeQuery(insertSql);
        
        if (result.empty()) {
            auto resp = utils::createErrorResponse("注册失败，请稍后重试", drogon::k500InternalServerError);
            callback(resp);
            return;
        }
        
        // 生成JWT令牌
        JwtManager jwtManager(m_jwtSecret);
        std::string token = jwtManager.generateToken(uuid, username, false);
        
        // 创建精简的响应数据
        Json::Value userData;
        userData["uuid"] = uuid;
        userData["username"] = username;
        userData["display_name"] = display_name;
        
        // 如果客户端需要，在响应体中返回token
        if (returnTokenInBody) {
            userData["token"] = token;
        }
        
        // 返回成功响应
        auto resp = utils::createSuccessResponse("注册成功", userData);
        
        // 默认总是在Cookie中设置token，便于浏览器环境
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