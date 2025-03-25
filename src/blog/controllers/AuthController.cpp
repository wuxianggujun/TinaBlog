#include "AuthController.hpp"
#include "blog/utils/HttpUtils.hpp"
#include "blog/utils/PasswordUtils.hpp"

/**
 * 构造函数
 */
AuthController::AuthController() {
    // 在实际应用中应从配置文件中获取JWT密钥
    m_jwtSecret = "your-secret-key-change-this-in-production";
    
    // 初始化密码工具类
    utils::PasswordUtils::initialize();
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
    
    // 使用异步查询获取用户信息
    dbManager.executeQuery(
        "SELECT uuid, username, password, is_admin, display_name FROM users WHERE username=$1",
        [=, callback=std::move(callback)](const drogon::orm::Result& result) {
            // 用户不存在
            if (result.size() == 0) {
                auto resp = utils::createErrorResponse("用户名或密码错误", drogon::k401Unauthorized);
                callback(resp);
                return;
            }
            
            // 获取用户信息
            std::string userUuid = result[0]["uuid"].as<std::string>();
            std::string storedPassword = result[0]["password"].as<std::string>();
            bool isAdmin = result[0]["is_admin"].as<bool>();
            std::string displayName = result[0]["display_name"].as<std::string>();
            
            // 使用libsodium验证密码哈希
            if (!utils::PasswordUtils::verifyPassword(password, storedPassword)) {
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
        },
        [callback=std::move(callback)](const drogon::orm::DrogonDbException& e) {
            // 数据库查询异常
            auto resp = utils::createErrorResponse("登录过程中发生错误: " + std::string(e.base().what()), 
                                                 drogon::k500InternalServerError, 500);
            callback(resp);
        },
        username);
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
    
    dbManager.executeQuery(
        "SELECT email, display_name FROM users WHERE uuid=$1",
        [=, callback=std::move(callback)](const drogon::orm::Result& result) {
            Json::Value userData;
            userData["uuid"] = userUuid;
            userData["username"] = username;
            userData["is_admin"] = isAdmin;
            
            if (result.size() > 0) {
                userData["email"] = result[0]["email"].as<std::string>();
                userData["display_name"] = result[0]["display_name"].as<std::string>();
            }
            
            // 返回成功响应
            auto resp = utils::createSuccessResponse("获取用户信息成功", userData);
            callback(resp);
        },
        [=, callback=std::move(callback)](const drogon::orm::DrogonDbException& e) {
            // 如果查询失败，仍然返回基本信息
            std::cerr << "Error fetching additional user info: " << e.base().what() << std::endl;
            
            Json::Value userData;
            userData["uuid"] = userUuid;
            userData["username"] = username;
            userData["is_admin"] = isAdmin;
            
            auto resp = utils::createSuccessResponse("获取基本用户信息成功", userData);
            callback(resp);
        },
        userUuid);
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
    
    // 获取数据库连接
    auto& dbManager = DbManager::getInstance();
    
    // 检查用户名是否已存在
    dbManager.executeQuery(
        "SELECT COUNT(*) AS count FROM users WHERE username=$1",
        [=, &dbManager, callback=callback](const drogon::orm::Result& result) {
            if (result[0]["count"].as<int>() > 0) {
                auto resp = utils::createErrorResponse("用户名已存在", drogon::k409Conflict);
                callback(resp);
                return;
            }
            
            // 检查邮箱是否已存在
            dbManager.executeQuery(
                "SELECT COUNT(*) AS count FROM users WHERE email=$1",
                [=, &dbManager, callback=callback](const drogon::orm::Result& result) {
                    if (result[0]["count"].as<int>() > 0) {
                        auto resp = utils::createErrorResponse("邮箱已被注册", drogon::k409Conflict);
                        callback(resp);
                        return;
                    }
                    
                    // 使用libsodium进行密码哈希
                    std::string hashedPassword = utils::PasswordUtils::hashPassword(password);
                    if (hashedPassword.empty()) {
                        auto resp = utils::createErrorResponse("密码加密失败", drogon::k500InternalServerError);
                        callback(resp);
                        return;
                    }
                    
                    // 插入新用户
                    dbManager.executeQuery(
                        "INSERT INTO users (uuid, username, password, email, display_name, is_admin, created_at, updated_at) VALUES "
                        "($1, $2, $3, $4, $5, false, NOW(), NOW()) RETURNING uuid",
                        [=, callback=callback](const drogon::orm::Result& result) {
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
                        },
                        [callback=callback](const drogon::orm::DrogonDbException& e) {
                            auto resp = utils::createErrorResponse("注册过程中发生错误: " + std::string(e.base().what()), 
                                                                 drogon::k500InternalServerError, 500);
                            callback(resp);
                        },
                        uuid, username, hashedPassword, email, display_name);
                },
                [callback=callback](const drogon::orm::DrogonDbException& e) {
                    auto resp = utils::createErrorResponse("注册过程中发生错误: " + std::string(e.base().what()), 
                                                        drogon::k500InternalServerError, 500);
                    callback(resp);
                },
                email);
        },
        [callback=callback](const drogon::orm::DrogonDbException& e) {
            auto resp = utils::createErrorResponse("注册过程中发生错误: " + std::string(e.base().what()), 
                                                drogon::k500InternalServerError, 500);
            callback(resp);
        },
        username);
} 