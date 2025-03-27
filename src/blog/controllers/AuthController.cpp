#include "AuthController.hpp"
#include "blog/utils/HttpUtils.hpp"
#include "blog/utils/PasswordUtils.hpp"
#include "blog/utils/ErrorCode.hpp"
#include <numeric> // 添加numeric头文件用于std::accumulate

/**
 * 构造函数
 */
AuthController::AuthController() {
    // 使用单例模式获取数据库管理器
    m_dbManager = std::shared_ptr<DbManager>(&DbManager::getInstance());
    
    // 使用单例模式获取JWT管理器
    m_jwtManager = std::shared_ptr<JwtManager>(&JwtManager::getInstance());
    
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
        
        // 检查auth_type参数
        std::string authType = "both"; // 默认同时使用cookie和响应体
        if (requestJson.isMember("auth_type") && requestJson["auth_type"].isString()) {
            authType = requestJson["auth_type"].asString();
        }
        bool returnTokenInBody = (authType == "token" || authType == "both");
        bool returnTokenInCookie = (authType == "cookie" || authType == "both");
        
        LOG_INFO << "用户 " << username << " 尝试登录, auth_type=" << authType;
        LOG_INFO << "返回token方式: cookie=" << (returnTokenInCookie ? "是" : "否") 
                 << ", body=" << (returnTokenInBody ? "是" : "否");

        // 检查数据库连接
        if (!m_dbManager->isConnected()) {
            callback(utils::createErrorResponse(utils::ErrorCode::DB_CONNECTION_ERROR));
            return;
        }

        // 查询用户
        m_dbManager->executeQuery(
            "SELECT username, password, display_name, uuid, is_admin FROM users WHERE username = $1",
            [this, username, password, authType, returnTokenInBody, returnTokenInCookie, callback](const drogon::orm::Result& result) {
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
                std::string userUuid = result[0]["uuid"].as<std::string>();
                bool isAdmin = result[0]["is_admin"].as<bool>();
                
                try {
                    token = m_jwtManager->generateToken(
                        userUuid,
                        username,
                        isAdmin
                    );
                    LOG_INFO << "生成JWT令牌成功, 长度: " << token.length();
                } catch (const std::exception& e) {
                    LOG_ERROR << "生成JWT令牌失败: " << e.what();
                    callback(utils::createErrorResponse(utils::ErrorCode::SYSTEM_ERROR, "生成认证令牌失败"));
                    return;
                }

                // 准备响应数据
                Json::Value userData;
                userData["uuid"] = userUuid;
                userData["username"] = result[0]["username"].as<std::string>();
                userData["display_name"] = result[0]["display_name"].as<std::string>();
                
                if (returnTokenInBody) {
                    LOG_INFO << "在响应体中返回token";
                    userData["token"] = token;
                }

                auto resp = utils::createSuccessResponse("登录成功", userData);
                
                if (returnTokenInCookie) {
                    LOG_INFO << "在Cookie中设置token";
                    drogon::Cookie tokenCookie("token", token);
                    tokenCookie.setHttpOnly(true);
                    tokenCookie.setPath("/");
                    resp->addCookie(std::move(tokenCookie));
                }
                
                LOG_INFO << "用户 " << username << " 登录成功，发送响应";
                LOG_INFO << "响应状态码: " << resp->getStatusCode();
                LOG_INFO << "响应Cookie数量: " << resp->cookies().size();
                
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
    
    LOG_INFO << "开始注册用户: " << username;
    
    // 检查认证方式首选项
    std::string authType = "both"; // 默认同时使用cookie和响应体
    if ((*jsonBody)["auth_type"].isString()) {
        authType = (*jsonBody)["auth_type"].asString();
    }
    bool returnTokenInBody = (authType == "token" || authType == "both");
    bool returnTokenInCookie = (authType == "cookie" || authType == "both");
    
    LOG_INFO << "返回token方式: cookie=" << (returnTokenInCookie ? "是" : "否") 
             << ", body=" << (returnTokenInBody ? "是" : "否");
    
    // 获取可选字段
    std::string email = (*jsonBody)["email"].isString() ? (*jsonBody)["email"].asString() : username + "@example.com";
    std::string display_name = (*jsonBody)["display_name"].isString() ? (*jsonBody)["display_name"].asString() : username;
    
    // 确保重要字段不为空
    if (username.empty() || password.empty() || email.empty()) {
        callback(utils::createErrorResponse(utils::ErrorCode::INVALID_PARAMETER));
        return;
    }
    
    // 生成UUID
    std::string uuid = drogon::utils::getUuid();
    LOG_INFO << "生成用户UUID: " << uuid;
    
    // 获取数据库连接
    auto& dbManager = DbManager::getInstance();
    
    // 检查用户名是否已存在
    dbManager.executeQuery(
        "SELECT COUNT(*) AS count FROM users WHERE username=$1",
        [=, &dbManager, callback=callback](const drogon::orm::Result& result) {
            if (result[0]["count"].as<int>() > 0) {
                LOG_WARN << "用户名已存在: " << username;
                callback(utils::createErrorResponse(utils::ErrorCode::USERNAME_EXISTS));
                return;
            }
            
            LOG_INFO << "用户名可用，检查邮箱: " << email;
            
            // 检查邮箱是否已存在
            dbManager.executeQuery(
                "SELECT COUNT(*) AS count FROM users WHERE email=$1",
                [=, &dbManager, callback=callback](const drogon::orm::Result& result) {
                    if (result[0]["count"].as<int>() > 0) {
                        LOG_WARN << "邮箱已存在: " << email;
                        callback(utils::createErrorResponse(utils::ErrorCode::EMAIL_EXISTS));
                        return;
                    }
                    
                    LOG_INFO << "邮箱可用，开始密码哈希";
                    
                    // 使用libsodium进行密码哈希
                    std::string hashedPassword = utils::PasswordUtils::hashPassword(password);
                    if (hashedPassword.empty()) {
                        LOG_ERROR << "密码哈希失败";
                        callback(utils::createErrorResponse(utils::ErrorCode::SYSTEM_ERROR));
                        return;
                    }
                    
                    LOG_INFO << "开始插入新用户数据";
                    
                    // 插入新用户
                    dbManager.executeQuery(
                        "INSERT INTO users (uuid, username, password, email, display_name, is_admin, created_at, updated_at) VALUES "
                        "($1, $2, $3, $4, $5, false, NOW(), NOW()) RETURNING uuid",
                        [=, callback=callback](const drogon::orm::Result& result) {
                            LOG_INFO << "用户数据插入成功，开始生成JWT令牌";
                            
                            // 生成JWT令牌
                            std::string token;
                            try {
                                token = JwtManager::getInstance().generateToken(
                                    uuid,  // userUuid
                                    username,  // username
                                    false   // isAdmin，新注册用户默认不是管理员
                                );
                                LOG_INFO << "生成JWT令牌成功, 长度: " << token.length();
                            } catch (const std::exception& e) {
                                LOG_ERROR << "生成JWT令牌失败: " << e.what();
                                callback(utils::createErrorResponse(utils::ErrorCode::SYSTEM_ERROR, "生成认证令牌失败"));
                                return;
                            }
                            
                            // 创建精简的响应数据
                            Json::Value userData;
                            userData["uuid"] = uuid;
                            userData["username"] = username;
                            userData["display_name"] = display_name;
                            
                            // 如果客户端需要，在响应体中返回token
                            if (returnTokenInBody) {
                                LOG_INFO << "在响应体中返回token";
                                userData["token"] = token;
                            }
                            
                            // 返回成功响应
                            auto resp = utils::createSuccessResponse("注册成功", userData);
                            
                            // 在Cookie中设置token
                            if (returnTokenInCookie) {
                                LOG_INFO << "在Cookie中设置token";
                                drogon::Cookie tokenCookie("token", token);
                                tokenCookie.setHttpOnly(true);
                                tokenCookie.setPath("/");
                                resp->addCookie(std::move(tokenCookie));
                            }
                            
                            LOG_INFO << "注册流程完成，发送响应";
                            LOG_INFO << "响应状态码: " << resp->getStatusCode();
                            LOG_INFO << "响应Cookie数量: " << resp->cookies().size();
                            
                            callback(resp);
                        },
                        [callback=callback](const drogon::orm::DrogonDbException& e) {
                            LOG_ERROR << "插入用户数据失败: " << e.base().what();
                            callback(utils::createErrorResponse(utils::ErrorCode::DB_INSERT_ERROR));
                        },
                        uuid, username, hashedPassword, email, display_name);
                },
                [callback=callback](const drogon::orm::DrogonDbException& e) {
                    LOG_ERROR << "检查邮箱失败: " << e.base().what();
                    callback(utils::createErrorResponse(utils::ErrorCode::DB_QUERY_ERROR));
                },
                email);
        },
        [callback=callback](const drogon::orm::DrogonDbException& e) {
            LOG_ERROR << "检查用户名失败: " << e.base().what();
            callback(utils::createErrorResponse(utils::ErrorCode::DB_QUERY_ERROR));
        },
        username);
}

/**
 * 验证token
 */
void AuthController::verifyToken(const drogon::HttpRequestPtr& req, 
                               std::function<void(const drogon::HttpResponsePtr&)>&& callback) const {
    try {
        // 从请求中获取token
        std::string token = m_jwtManager->getTokenFromRequest(req);
        if (token.empty()) {
            LOG_WARN << "验证token失败：未提供token";
            callback(utils::createErrorResponse(utils::ErrorCode::UNAUTHORIZED, "未提供认证令牌"));
            return;
        }

        LOG_INFO << "开始验证token，token长度: " << token.length();

        // 验证token
        JwtManager::VerifyResult result;
        if (!m_jwtManager->verifyToken(token, result)) {
            LOG_WARN << "验证token失败：" << result.reason;
            callback(utils::createErrorResponse(utils::ErrorCode::UNAUTHORIZED, result.reason));
            return;
        }

        LOG_INFO << "token验证成功，用户: " << result.username;

        // 创建用户数据对象
        Json::Value userData;
        userData["uuid"] = result.userUuid;
        userData["username"] = result.username;
        userData["is_admin"] = result.isAdmin;
        
        Json::Value responseData;
        responseData["success"] = true;
        responseData["message"] = "Token验证成功";
        responseData["user"] = userData;
        
        callback(utils::createSuccessResponse("Token验证成功", responseData));
    } catch (const std::exception& e) {
        LOG_ERROR << "验证token时发生异常: " << e.what();
        callback(utils::createErrorResponse(utils::ErrorCode::SERVER_ERROR, "服务器内部错误"));
    }
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

/**
 * 更新用户个人信息
 */
void AuthController::updateProfile(const drogon::HttpRequestPtr& req, 
                                 std::function<void(const drogon::HttpResponsePtr&)>&& callback) const {
    // 在函数入口处创建回调副本，确保catch块可以访问到
    auto callbackPtr = std::make_shared<std::function<void(const drogon::HttpResponsePtr&)>>(std::move(callback));
    
    try {
        // 从请求属性中获取用户UUID（已由JWT过滤器添加）
        std::string userUuid = req->getAttributes()->get<std::string>("user_uuid");
        if (userUuid.empty()) {
            (*callbackPtr)(utils::createErrorResponse(utils::ErrorCode::UNAUTHORIZED, "用户未认证"));
            return;
        }
        
        // 获取请求数据
        auto jsonBody = req->getJsonObject();
        if (!jsonBody) {
            (*callbackPtr)(utils::createErrorResponse(utils::ErrorCode::INVALID_REQUEST, "无效的请求格式"));
            return;
        }
        
        // 获取要更新的字段
        std::string displayName = (*jsonBody)["display_name"].isString() ? 
                                (*jsonBody)["display_name"].asString() : "";
        std::string email = (*jsonBody)["email"].isString() ? 
                          (*jsonBody)["email"].asString() : "";
        std::string bio = (*jsonBody)["bio"].isString() ? 
                        (*jsonBody)["bio"].asString() : "";
        
        // 验证电子邮箱格式
        if (!email.empty() && email.find('@') == std::string::npos) {
            (*callbackPtr)(utils::createErrorResponse(utils::ErrorCode::INVALID_PARAMETER, "无效的电子邮箱格式"));
            return;
        }
        
        // 构建更新语句
        std::vector<std::string> updateFields;
        
        if (!displayName.empty()) {
            updateFields.push_back("display_name = $1");
        }
        
        if (!email.empty()) {
            updateFields.push_back("email = $" + std::to_string(updateFields.size() + 1));
        }
        
        if (!bio.empty()) {
            updateFields.push_back("bio = $" + std::to_string(updateFields.size() + 1));
        }
        
        // 添加更新时间
        updateFields.push_back("updated_at = NOW()");
        
        if (updateFields.size() == 1) { // 只有更新时间
            (*callbackPtr)(utils::createErrorResponse(utils::ErrorCode::INVALID_PARAMETER, "没有要更新的内容"));
            return;
        }
        
        // 构建SQL语句 - 使用字符串连接而不是std::accumulate
        std::string sql = "UPDATE users SET ";
        for (size_t i = 0; i < updateFields.size(); ++i) {
            sql += updateFields[i];
            if (i < updateFields.size() - 1) {
                sql += ", ";
            }
        }
        sql += " WHERE uuid = $" + std::to_string(updateFields.size()) + " RETURNING username, display_name, email, bio";
        
        // 执行更新 - 单独传递每个参数
        auto& dbManager = DbManager::getInstance();
        
        // 创建通用响应处理函数
        auto responseHandler = [callbackPtr](const drogon::orm::Result& result) {
            if (result.size() > 0) {
                Json::Value userData;
                userData["username"] = result[0]["username"].as<std::string>();
                userData["display_name"] = result[0]["display_name"].as<std::string>();
                userData["email"] = result[0]["email"].as<std::string>();
                
                if (!result[0]["bio"].isNull()) {
                    userData["bio"] = result[0]["bio"].as<std::string>();
                }
                
                (*callbackPtr)(utils::createSuccessResponse("个人信息更新成功", userData));
            } else {
                (*callbackPtr)(utils::createErrorResponse(utils::ErrorCode::RESOURCE_NOT_FOUND, "用户不存在"));
            }
        };
        
        // 创建通用错误处理函数
        auto errorHandler = [callbackPtr](const drogon::orm::DrogonDbException& e) {
            std::cerr << "更新用户信息失败: " << e.base().what() << std::endl;
            (*callbackPtr)(utils::createErrorResponse(utils::ErrorCode::DB_QUERY_ERROR, "更新用户信息失败"));
        };
        
        // 处理不同的更新字段组合
        if (!displayName.empty() && !email.empty() && !bio.empty()) {
            dbManager.executeQuery(sql, responseHandler, errorHandler, displayName, email, bio, userUuid);
        }
        else if (!displayName.empty() && !email.empty()) {
            dbManager.executeQuery(sql, responseHandler, errorHandler, displayName, email, userUuid);
        }
        else if (!displayName.empty() && !bio.empty()) {
            dbManager.executeQuery(sql, responseHandler, errorHandler, displayName, bio, userUuid);
        }
        else if (!email.empty() && !bio.empty()) {
            dbManager.executeQuery(sql, responseHandler, errorHandler, email, bio, userUuid);
        }
        else if (!displayName.empty()) {
            dbManager.executeQuery(sql, responseHandler, errorHandler, displayName, userUuid);
        }
        else if (!email.empty()) {
            dbManager.executeQuery(sql, responseHandler, errorHandler, email, userUuid);
        }
        else if (!bio.empty()) {
            dbManager.executeQuery(sql, responseHandler, errorHandler, bio, userUuid);
        }
    } catch (const std::exception& e) {
        std::cerr << "更新用户信息异常: " << e.what() << std::endl;
        (*callbackPtr)(utils::createErrorResponse(utils::ErrorCode::SERVER_ERROR));
    }
}

/**
 * 修改用户密码
 */
void AuthController::changePassword(const drogon::HttpRequestPtr& req, 
                                  std::function<void(const drogon::HttpResponsePtr&)>&& callback) const {
    // 在函数入口处创建回调副本，确保catch块可以访问到
    auto callbackPtr = std::make_shared<std::function<void(const drogon::HttpResponsePtr&)>>(std::move(callback));
    
    try {
        // 从请求属性中获取用户UUID（已由JWT过滤器添加）
        std::string userUuid = req->getAttributes()->get<std::string>("user_uuid");
        if (userUuid.empty()) {
            (*callbackPtr)(utils::createErrorResponse(utils::ErrorCode::UNAUTHORIZED, "用户未认证"));
            return;
        }
        
        // 获取请求数据
        auto jsonBody = req->getJsonObject();
        if (!jsonBody) {
            (*callbackPtr)(utils::createErrorResponse(utils::ErrorCode::INVALID_REQUEST, "无效的请求格式"));
            return;
        }
        
        // 获取当前密码和新密码
        if (!(*jsonBody)["current_password"].isString() || !(*jsonBody)["new_password"].isString()) {
            (*callbackPtr)(utils::createErrorResponse(utils::ErrorCode::INVALID_PARAMETER, "缺少当前密码或新密码"));
            return;
        }
        
        std::string currentPassword = (*jsonBody)["current_password"].asString();
        std::string newPassword = (*jsonBody)["new_password"].asString();
        
        // 验证新密码长度
        if (newPassword.length() < 8) {
            (*callbackPtr)(utils::createErrorResponse(utils::ErrorCode::INVALID_PARAMETER, "新密码长度必须至少为8个字符"));
            return;
        }
        
        // 首先获取用户当前密码
        auto& dbManager = DbManager::getInstance();
        auto thisPtr = this; // 存储this指针
        
        dbManager.executeQuery(
            "SELECT password FROM users WHERE uuid = $1",
            [thisPtr, userUuid, currentPassword, newPassword, callbackPtr](const drogon::orm::Result& result) {
                if (result.size() == 0) {
                    (*callbackPtr)(utils::createErrorResponse(utils::ErrorCode::RESOURCE_NOT_FOUND, "用户不存在"));
                    return;
                }
                
                // 验证当前密码
                std::string storedPassword = result[0]["password"].as<std::string>();
                if (!thisPtr->verifyPassword(currentPassword, storedPassword)) {
                    (*callbackPtr)(utils::createErrorResponse(utils::ErrorCode::PASSWORD_ERROR, "当前密码不正确"));
                    return;
                }
                
                // 生成新密码的哈希
                std::string newPasswordHash = utils::PasswordUtils::hashPassword(newPassword);
                
                // 更新密码
                auto& dbManager = DbManager::getInstance();
                dbManager.executeQuery(
                    "UPDATE users SET password = $1, updated_at = NOW() WHERE uuid = $2",
                    [callbackPtr](const drogon::orm::Result& result) {
                        (*callbackPtr)(utils::createSuccessResponse("密码修改成功"));
                    },
                    [callbackPtr](const drogon::orm::DrogonDbException& e) {
                        std::cerr << "修改密码失败: " << e.base().what() << std::endl;
                        (*callbackPtr)(utils::createErrorResponse(utils::ErrorCode::DB_QUERY_ERROR, "修改密码失败"));
                    },
                    newPasswordHash, userUuid
                );
            },
            [callbackPtr](const drogon::orm::DrogonDbException& e) {
                std::cerr << "获取用户密码失败: " << e.base().what() << std::endl;
                (*callbackPtr)(utils::createErrorResponse(utils::ErrorCode::DB_QUERY_ERROR, "修改密码失败"));
            },
            userUuid
        );
    } catch (const std::exception& e) {
        std::cerr << "修改密码异常: " << e.what() << std::endl;
        (*callbackPtr)(utils::createErrorResponse(utils::ErrorCode::SERVER_ERROR));
    }
} 