#include "AuthController.hpp"
#include "blog/utils/HttpUtils.hpp"
#include "blog/utils/PasswordUtils.hpp"
#include "blog/utils/ErrorCode.hpp"

/**
 * 构造函数
 */
AuthController::AuthController() {
    // 在实际应用中应从配置文件中获取JWT密钥
    m_jwtSecret = "your-secret-key-change-this-in-production";
    
    // 使用单例模式获取数据库管理器
    m_dbManager = std::shared_ptr<DbManager>(&DbManager::getInstance());
    
    // 初始化JWT管理器
    m_jwtManager = std::make_shared<JwtManager>(m_jwtSecret);
    
    // 初始化密码工具类
    utils::PasswordUtils::initialize();
}

/**
 * 验证密码
 */
bool AuthController::verifyPassword(const std::string& password, const std::string& hashedPassword) const {
    return utils::PasswordUtils::verifyPassword(password, hashedPassword);
}

/**
 * 用户登录
 */
void AuthController::login(const drogon::HttpRequestPtr& req, 
                          std::function<void(const drogon::HttpResponsePtr&)>&& callback) const {
    try {
        // 检查Content-Type是否为application/json
        std::string contentType = req->getHeader("Content-Type");
        if (contentType.find("application/json") == std::string::npos) {
            callback(utils::createErrorResponse(utils::ErrorCode::INVALID_REQUEST));
            return;
        }

        // 解析请求体
        Json::Value requestJson;
        Json::Reader reader;
        if (!reader.parse(std::string(req->getBody()), requestJson)) {
            callback(utils::createErrorResponse(utils::ErrorCode::INVALID_REQUEST));
            return;
        }

        // 检查必要字段
        if (!requestJson.isMember("username") || !requestJson.isMember("password")) {
            callback(utils::createErrorResponse(utils::ErrorCode::INVALID_PARAMETER));
            return;
        }

        std::string username = requestJson["username"].asString();
        std::string password = requestJson["password"].asString();
        bool returnTokenInBody = requestJson.get("return_token_in_body", false).asBool();

        LOG_INFO << "用户 " << username << " 尝试登录";

        // 检查数据库连接
        if (!m_dbManager->isConnected()) {
            callback(utils::createErrorResponse(utils::ErrorCode::DB_CONNECTION_ERROR));
            return;
        }

        // 查询用户
        m_dbManager->executeQuery(
            "SELECT username, password, display_name, uuid, is_admin FROM users WHERE username = $1",
            [this, username, password, returnTokenInBody, callback](const drogon::orm::Result& result) {
                if (result.empty()) {
                    callback(utils::createErrorResponse(utils::ErrorCode::USER_NOT_FOUND));
                    return;
                }

                // 验证密码
                std::string storedHash = result[0]["password"].as<std::string>();
                if (!verifyPassword(password, storedHash)) {
                    callback(utils::createErrorResponse(utils::ErrorCode::PASSWORD_ERROR));
                    return;
                }

                // 生成JWT令牌
                std::string token;
                try {
                    token = m_jwtManager->generateToken(
                        result[0]["uuid"].as<std::string>(),  // userUuid
                        username,                              // username
                        result[0]["is_admin"].as<bool>()      // isAdmin
                    );
                } catch (const std::exception& e) {
                    LOG_ERROR << "生成JWT令牌失败: " << e.what();
                    callback(utils::createErrorResponse(utils::ErrorCode::SYSTEM_ERROR, "生成认证令牌失败"));
                    return;
                }

                // 准备响应数据
                Json::Value userData;
                userData["uuid"] = result[0]["uuid"].as<std::string>();
                userData["username"] = result[0]["username"].as<std::string>();
                userData["display_name"] = result[0]["display_name"].as<std::string>();

                // 创建响应
                Json::Value responseData;
                responseData["user"] = userData;
                if (returnTokenInBody) {
                    responseData["token"] = token;
                }

                auto resp = utils::createSuccessResponse("登录成功", responseData);
                if (!returnTokenInBody) {
                    drogon::Cookie tokenCookie("token", token);
                    tokenCookie.setHttpOnly(true);
                    tokenCookie.setPath("/");
                    resp->addCookie(std::move(tokenCookie));
                }

                callback(resp);
            },
            [callback](const drogon::orm::DrogonDbException& e) {
                LOG_ERROR << "数据库查询失败: " << e.base().what();
                callback(utils::createErrorResponse(utils::ErrorCode::DB_QUERY_ERROR));
            },
            username
        );
    } catch (const std::exception& e) {
        LOG_ERROR << "登录处理异常: " << e.what();
        callback(utils::createErrorResponse(utils::ErrorCode::SERVER_ERROR));
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
        auto resp = utils::createErrorResponse(
            utils::ErrorCode::INVALID_REQUEST,
            "无效的请求格式，需要JSON数据"
        );
        callback(resp);
        return;
    }
    
    // 获取用户名和密码
    if (!(*jsonBody)["username"].isString() || !(*jsonBody)["password"].isString()) {
        auto resp = utils::createErrorResponse(
            utils::ErrorCode::INVALID_PARAMETER,
            "缺少用户名或密码"
        );
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
        callback(utils::createErrorResponse(utils::ErrorCode::INVALID_PARAMETER));
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
                callback(utils::createErrorResponse(utils::ErrorCode::USERNAME_EXISTS));
                return;
            }
            
            // 检查邮箱是否已存在
            dbManager.executeQuery(
                "SELECT COUNT(*) AS count FROM users WHERE email=$1",
                [=, &dbManager, callback=callback](const drogon::orm::Result& result) {
                    if (result[0]["count"].as<int>() > 0) {
                        callback(utils::createErrorResponse(utils::ErrorCode::EMAIL_EXISTS));
                        return;
                    }
                    
                    // 使用libsodium进行密码哈希
                    std::string hashedPassword = utils::PasswordUtils::hashPassword(password);
                    if (hashedPassword.empty()) {
                        callback(utils::createErrorResponse(utils::ErrorCode::SYSTEM_ERROR));
                        return;
                    }
                    
                    // 插入新用户
                    dbManager.executeQuery(
                        "INSERT INTO users (uuid, username, password, email, display_name, is_admin, created_at, updated_at) VALUES "
                        "($1, $2, $3, $4, $5, false, NOW(), NOW()) RETURNING uuid",
                        [=, callback=callback](const drogon::orm::Result& result) {
                            // 生成JWT令牌
                            JwtManager jwtManager(m_jwtSecret);
                            std::string token = jwtManager.generateToken(
                                result[0]["uuid"].as<std::string>(),  // userUuid
                                username,                              // username
                                false                                  // isAdmin，新注册用户默认不是管理员
                            );
                            
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
                            callback(utils::createErrorResponse(utils::ErrorCode::DB_INSERT_ERROR));
                        },
                        uuid, username, hashedPassword, email, display_name);
                },
                [callback=callback](const drogon::orm::DrogonDbException& e) {
                    callback(utils::createErrorResponse(utils::ErrorCode::DB_QUERY_ERROR));
                },
                email);
        },
        [callback=callback](const drogon::orm::DrogonDbException& e) {
            callback(utils::createErrorResponse(utils::ErrorCode::DB_QUERY_ERROR));
        },
        username);
}

/**
 * 验证token
 */
void AuthController::verifyToken(const drogon::HttpRequestPtr& req, 
                               std::function<void(const drogon::HttpResponsePtr&)>&& callback) const {
    // 由于使用了JwtAuthFilter，如果请求能到达这里，说明token是有效的
    Json::Value responseData;
    responseData["success"] = true;
    responseData["message"] = "Token有效";
    
    // 从请求属性中获取用户信息
    std::string userUuid = req->getAttributes()->get<std::string>("user_uuid");
    std::string username = req->getAttributes()->get<std::string>("username");
    bool isAdmin = req->getAttributes()->get<bool>("is_admin");
    
    // 创建用户数据对象
    Json::Value userData;
    userData["uuid"] = userUuid;
    userData["username"] = username;
    userData["is_admin"] = isAdmin;
    
    responseData["user"] = userData;
    
    callback(utils::createSuccessResponse("Token验证成功", responseData));
}

/**
 * 用户登出
 */
void AuthController::logout(const drogon::HttpRequestPtr& req, 
                          std::function<void(const drogon::HttpResponsePtr&)>&& callback) const {
    // 由于使用了JWT，服务端不需要额外的处理
    // 客户端会清除token和登录状态
    auto resp = utils::createSuccessResponse("登出成功");
    
    // 清除cookie中的token
    drogon::Cookie tokenCookie("token", "");
    tokenCookie.setHttpOnly(true);
    tokenCookie.setPath("/");
    // 设置过期时间为过去的时间
    tokenCookie.setExpiresDate(trantor::Date::now().after(-1));
    resp->addCookie(std::move(tokenCookie));
    
    callback(resp);
} 