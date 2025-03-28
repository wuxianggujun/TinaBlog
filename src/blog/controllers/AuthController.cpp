#include "AuthController.hpp"
#include "blog/utils/HttpUtils.hpp"
#include "blog/utils/PasswordUtils.hpp"
#include "blog/utils/ErrorCode.hpp"
#include <numeric> // 添加numeric头文件用于std::accumulate
#include <fstream> // 添加fstream头文件用于文件操作
#include <sstream> // 添加sstream头文件用于字符串流
#include <ctime>   // 添加ctime头文件用于时间操作
#include <curl/curl.h>  // 添加libcurl头文件

// libcurl写入回调函数声明
static size_t WriteCallback(void *contents, size_t size, size_t nmemb, void *userp) {
    ((std::string*)userp)->append((char*)contents, size * nmemb);
    return size * nmemb;
}

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
    // 创建回调函数的共享指针，防止回调函数被意外移动
    auto callbackPtr = std::make_shared<std::function<void(const drogon::HttpResponsePtr&)>>(std::move(callback));
    
    try {
        // 检查Content-Type是否为application/json
        std::string contentType = req->getHeader("Content-Type");
        if (contentType.find("application/json") == std::string::npos) {
            (*callbackPtr)(utils::createErrorResponse(utils::ErrorCode::INVALID_REQUEST));
            return;
        }

        // 解析请求体
        Json::Value requestJson;
        Json::Reader reader;
        if (!reader.parse(std::string(req->getBody()), requestJson)) {
            (*callbackPtr)(utils::createErrorResponse(utils::ErrorCode::INVALID_REQUEST));
            return;
        }

        // 检查必要字段
        if (!requestJson.isMember("username") || !requestJson.isMember("password")) {
            (*callbackPtr)(utils::createErrorResponse(utils::ErrorCode::INVALID_PARAMETER));
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
            (*callbackPtr)(utils::createErrorResponse(utils::ErrorCode::DB_CONNECTION_ERROR));
            return;
        }

        // 查询用户
        m_dbManager->executeQuery(
            "SELECT username, password, display_name, uuid, is_admin FROM users WHERE username = $1",
            [this, username, password, authType, returnTokenInBody, returnTokenInCookie, callbackPtr](const drogon::orm::Result& result) {
                if (result.empty()) {
                    (*callbackPtr)(utils::createErrorResponse(utils::ErrorCode::USER_NOT_FOUND));
                    return;
                }

                // 验证密码
                std::string storedHash = result[0]["password"].as<std::string>();
                if (!verifyPassword(password, storedHash)) {
                    (*callbackPtr)(utils::createErrorResponse(utils::ErrorCode::PASSWORD_ERROR));
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
                    (*callbackPtr)(utils::createErrorResponse(utils::ErrorCode::SYSTEM_ERROR, "生成认证令牌失败"));
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
                
                (*callbackPtr)(resp);
            },
            [callbackPtr](const drogon::orm::DrogonDbException& e) {
                LOG_ERROR << "数据库查询失败: " << e.base().what();
                (*callbackPtr)(utils::createErrorResponse(utils::ErrorCode::DB_QUERY_ERROR));
            },
            username
        );
    } catch (const std::exception& e) {
        LOG_ERROR << "登录处理异常: " << e.what();
        (*callbackPtr)(utils::createErrorResponse(utils::ErrorCode::SERVER_ERROR));
    }
}

/**
 * 获取当前登录用户信息
 */
void AuthController::getUserInfo(const drogon::HttpRequestPtr& req, 
                                std::function<void(const drogon::HttpResponsePtr&)>&& callback) const {
    // 创建回调函数的共享指针，防止回调函数被意外移动
    auto callbackPtr = std::make_shared<std::function<void(const drogon::HttpResponsePtr&)>>(std::move(callback));
    
    try {
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
            "SELECT email, display_name, bio, avatar FROM users WHERE uuid=$1",
            [=, &dbManager](const drogon::orm::Result& result) {
                Json::Value userData;
                userData["uuid"] = userUuid;
                userData["username"] = username;
                userData["is_admin"] = isAdmin;
                
                if (result.size() > 0) {
                    userData["email"] = result[0]["email"].as<std::string>();
                    
                    // 添加display_name字段，如果不为空
                    if (!result[0]["display_name"].isNull()) {
                        userData["display_name"] = result[0]["display_name"].as<std::string>();
                    }
                    
                    // 添加bio字段，如果不为空
                    if (!result[0]["bio"].isNull()) {
                        userData["bio"] = result[0]["bio"].as<std::string>();
                    }
                    
                    // 添加avatar字段，如果不为空
                    if (!result[0]["avatar"].isNull()) {
                        userData["avatar"] = result[0]["avatar"].as<std::string>();
                    }
                    
                    // 然后查询用户的社交链接信息
                    dbManager.executeQuery(
                        "SELECT link_type, link_url FROM user_links WHERE user_uuid=$1",
                        [=](const drogon::orm::Result& linkResult) mutable {
                            // 处理用户链接
                            for (const auto& row : linkResult) {
                                std::string linkType = row["link_type"].as<std::string>();
                                std::string linkUrl = row["link_url"].as<std::string>();
                                
                                // 将链接添加到用户数据中
                                userData[linkType] = linkUrl;
                            }
                            
                            // 返回成功响应
                            auto resp = utils::createSuccessResponse("获取用户信息成功", userData);
                            (*callbackPtr)(resp);
                        },
                        [=](const drogon::orm::DrogonDbException& e) mutable {
                            // 如果查询链接失败，仍然返回基本信息
                            std::cerr << "Error fetching user links: " << e.base().what() << std::endl;
                            
                            // 返回成功响应，但没有链接数据
                            auto resp = utils::createSuccessResponse("获取用户基本信息成功", userData);
                            (*callbackPtr)(resp);
                        },
                        userUuid
                    );
                } else {
                    // 返回成功响应，只有基本信息
                    auto resp = utils::createSuccessResponse("获取用户基本信息成功", userData);
                    (*callbackPtr)(resp);
                }
            },
            [=](const drogon::orm::DrogonDbException& e) {
                // 如果查询失败，仍然返回基本信息
                std::cerr << "Error fetching additional user info: " << e.base().what() << std::endl;
                
                Json::Value userData;
                userData["uuid"] = userUuid;
                userData["username"] = username;
                userData["is_admin"] = isAdmin;
                
                auto resp = utils::createSuccessResponse("获取基本用户信息成功", userData);
                (*callbackPtr)(resp);
            },
            userUuid);
    } catch (const std::exception& e) {
        std::cerr << "获取用户信息异常: " << e.what() << std::endl;
        (*callbackPtr)(utils::createErrorResponse(utils::ErrorCode::SERVER_ERROR));
    }
}

void AuthController::registerUser(const drogon::HttpRequestPtr& req, 
                               std::function<void(const drogon::HttpResponsePtr&)>&& callback) const 
{
    // 创建回调函数的共享指针，防止回调函数被意外移动
    auto callbackPtr = std::make_shared<std::function<void(const drogon::HttpResponsePtr&)>>(std::move(callback));
    
    try {
        auto jsonBody = req->getJsonObject();
        
        // 检查JSON请求体
        if (!jsonBody) {
            auto resp = utils::createErrorResponse(
                utils::ErrorCode::INVALID_REQUEST,
                "无效的请求格式，需要JSON数据"
            );
            (*callbackPtr)(resp);
            return;
        }
        
        // 获取用户名和密码
        if (!(*jsonBody)["username"].isString() || !(*jsonBody)["password"].isString()) {
            auto resp = utils::createErrorResponse(
                utils::ErrorCode::INVALID_PARAMETER,
                "缺少用户名或密码"
            );
            (*callbackPtr)(resp);
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
            (*callbackPtr)(utils::createErrorResponse(utils::ErrorCode::INVALID_PARAMETER));
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
            [=, &dbManager](const drogon::orm::Result& result) {
                if (result[0]["count"].as<int>() > 0) {
                    LOG_WARN << "用户名已存在: " << username;
                    (*callbackPtr)(utils::createErrorResponse(utils::ErrorCode::USERNAME_EXISTS));
                    return;
                }
                
                LOG_INFO << "用户名可用，检查邮箱: " << email;
                
                // 检查邮箱是否已存在
                dbManager.executeQuery(
                    "SELECT COUNT(*) AS count FROM users WHERE email=$1",
                    [=, &dbManager](const drogon::orm::Result& result) {
                        if (result[0]["count"].as<int>() > 0) {
                            LOG_WARN << "邮箱已存在: " << email;
                            (*callbackPtr)(utils::createErrorResponse(utils::ErrorCode::EMAIL_EXISTS));
                            return;
                        }
                        
                        LOG_INFO << "邮箱可用，开始密码哈希";
                        
                        // 使用libsodium进行密码哈希
                        std::string hashedPassword = utils::PasswordUtils::hashPassword(password);
                        if (hashedPassword.empty()) {
                            LOG_ERROR << "密码哈希失败";
                            (*callbackPtr)(utils::createErrorResponse(utils::ErrorCode::SYSTEM_ERROR));
                            return;
                        }
                        
                        LOG_INFO << "开始插入新用户数据";
                        
                        // 插入新用户
                        dbManager.executeQuery(
                            "INSERT INTO users (uuid, username, password, email, display_name, is_admin, created_at, updated_at) VALUES "
                            "($1, $2, $3, $4, $5, false, NOW(), NOW()) RETURNING uuid",
                            [=](const drogon::orm::Result& result) {
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
                                    (*callbackPtr)(utils::createErrorResponse(utils::ErrorCode::SYSTEM_ERROR, "生成认证令牌失败"));
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
                                
                                (*callbackPtr)(resp);
                            },
                            [=](const drogon::orm::DrogonDbException& e) {
                                LOG_ERROR << "插入用户数据失败: " << e.base().what();
                                (*callbackPtr)(utils::createErrorResponse(utils::ErrorCode::DB_INSERT_ERROR));
                            },
                            uuid, username, hashedPassword, email, display_name);
                    },
                    [=](const drogon::orm::DrogonDbException& e) {
                        LOG_ERROR << "检查邮箱失败: " << e.base().what();
                        (*callbackPtr)(utils::createErrorResponse(utils::ErrorCode::DB_QUERY_ERROR));
                    },
                    email);
            },
            [=](const drogon::orm::DrogonDbException& e) {
                LOG_ERROR << "检查用户名失败: " << e.base().what();
                (*callbackPtr)(utils::createErrorResponse(utils::ErrorCode::DB_QUERY_ERROR));
            },
            username);
    } catch (const std::exception& e) {
        LOG_ERROR << "注册用户异常: " << e.what();
        (*callbackPtr)(utils::createErrorResponse(utils::ErrorCode::SERVER_ERROR));
    }
}

/**
 * 验证token
 */
void AuthController::verifyToken(const drogon::HttpRequestPtr& req, 
                               std::function<void(const drogon::HttpResponsePtr&)>&& callback) const {
    // 创建回调函数的共享指针，防止回调函数被意外移动
    auto callbackPtr = std::make_shared<std::function<void(const drogon::HttpResponsePtr&)>>(std::move(callback));
    
    try {
        // 从请求中获取token
        std::string token = m_jwtManager->getTokenFromRequest(req);
        if (token.empty()) {
            LOG_WARN << "验证token失败：未提供token";
            (*callbackPtr)(utils::createErrorResponse(utils::ErrorCode::UNAUTHORIZED, "未提供认证令牌"));
            return;
        }

        LOG_INFO << "开始验证token，token长度: " << token.length();

        // 验证token
        JwtManager::VerifyResult result;
        if (!m_jwtManager->verifyToken(token, result)) {
            LOG_WARN << "验证token失败：" << result.reason;
            (*callbackPtr)(utils::createErrorResponse(utils::ErrorCode::UNAUTHORIZED, result.reason));
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
        
        (*callbackPtr)(utils::createSuccessResponse("Token验证成功", responseData));
    } catch (const std::exception& e) {
        LOG_ERROR << "验证token时发生异常: " << e.what();
        (*callbackPtr)(utils::createErrorResponse(utils::ErrorCode::SERVER_ERROR, "服务器内部错误"));
    }
}

/**
 * 用户登出
 */
void AuthController::logout(const drogon::HttpRequestPtr& req, 
                          std::function<void(const drogon::HttpResponsePtr&)>&& callback) const {
    // 创建回调函数的共享指针，防止回调函数被意外移动
    auto callbackPtr = std::make_shared<std::function<void(const drogon::HttpResponsePtr&)>>(std::move(callback));
    
    try {
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
        
        (*callbackPtr)(resp);
    } catch (const std::exception& e) {
        std::cerr << "登出异常: " << e.what() << std::endl;
        (*callbackPtr)(utils::createErrorResponse(utils::ErrorCode::SERVER_ERROR));
    }
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

/**
 * 更新用户头像
 */
void AuthController::updateAvatar(const drogon::HttpRequestPtr& req, 
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
        
        // 获取头像URL
        if (!(*jsonBody)["avatar_url"].isString()) {
            (*callbackPtr)(utils::createErrorResponse(utils::ErrorCode::INVALID_PARAMETER, "缺少头像URL"));
            return;
        }
        
        std::string avatarUrl = (*jsonBody)["avatar_url"].asString();
        
        // 验证URL格式
        if (avatarUrl.empty() || (avatarUrl.find("http://") != 0 && avatarUrl.find("https://") != 0)) {
            (*callbackPtr)(utils::createErrorResponse(utils::ErrorCode::INVALID_PARAMETER, "无效的头像URL格式"));
            return;
        }
        
        // 构建更新语句
        std::string sql = "UPDATE users SET avatar = $1, updated_at = NOW() WHERE uuid = $2 RETURNING username, display_name, email, bio, avatar";
        
        // 执行更新
        auto& dbManager = DbManager::getInstance();
        dbManager.executeQuery(
            sql,
            [callbackPtr](const drogon::orm::Result& result) {
                if (result.size() > 0) {
                    Json::Value userData;
                    userData["username"] = result[0]["username"].as<std::string>();
                    userData["display_name"] = result[0]["display_name"].as<std::string>();
                    userData["email"] = result[0]["email"].as<std::string>();
                    
                    if (!result[0]["bio"].isNull()) {
                        userData["bio"] = result[0]["bio"].as<std::string>();
                    }
                    
                    if (!result[0]["avatar"].isNull()) {
                        userData["avatar"] = result[0]["avatar"].as<std::string>();
                    }
                    
                    (*callbackPtr)(utils::createSuccessResponse("头像更新成功", userData));
                } else {
                    (*callbackPtr)(utils::createErrorResponse(utils::ErrorCode::RESOURCE_NOT_FOUND, "用户不存在"));
                }
            },
            [callbackPtr](const drogon::orm::DrogonDbException& e) {
                std::cerr << "更新用户头像失败: " << e.base().what() << std::endl;
                (*callbackPtr)(utils::createErrorResponse(utils::ErrorCode::DB_QUERY_ERROR, "更新用户头像失败"));
            },
            avatarUrl, userUuid
        );
    } catch (const std::exception& e) {
        std::cerr << "更新用户头像异常: " << e.what() << std::endl;
        (*callbackPtr)(utils::createErrorResponse(utils::ErrorCode::SERVER_ERROR));
    }
}

/**
 * 获取图片上传授权
 */
void AuthController::getUploadToken(const drogon::HttpRequestPtr& req, 
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
        
        // 这里是图床API密钥，应该保存在服务器的环境变量或配置文件中
        // 在实际生产环境中，请不要硬编码API密钥
        const std::string IMAGE_HUB_API_KEY = "chv_s9Gk_c68593c1beea9e74ca78bf4598e6c22acfb50f2d2b4e411a3aec12aeea36ce4bc338cda2818eefb15243f12fa6ae4c30497b1c58edd9912356d687bf3b3d8480";
        
        // 创建响应数据，包含上传所需的信息
        Json::Value uploadInfo;
        uploadInfo["upload_url"] = "https://www.imagehub.cc/api/1/upload";
        uploadInfo["api_key"] = IMAGE_HUB_API_KEY;
        uploadInfo["param_name"] = "source";  // ImageHub API使用'source'作为文件参数名
        uploadInfo["headers"] = Json::objectValue;
        uploadInfo["headers"]["Content-Type"] = "multipart/form-data";
        uploadInfo["headers"]["X-API-Key"] = IMAGE_HUB_API_KEY;
        
        // 返回上传授权信息
        (*callbackPtr)(utils::createSuccessResponse("获取上传授权成功", uploadInfo));
    } catch (const std::exception& e) {
        std::cerr << "获取上传授权异常: " << e.what() << std::endl;
        (*callbackPtr)(utils::createErrorResponse(utils::ErrorCode::SERVER_ERROR));
    }
}

/**
 * 代理上传图片到图床
 */
void AuthController::uploadImage(const drogon::HttpRequestPtr& req, 
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
        
        // 检查Content-Type
        std::string contentType = req->getHeader("Content-Type");
        LOG_INFO << "收到上传请求，Content-Type: " << contentType;
        
        if (contentType.find("multipart/form-data") == std::string::npos) {
            (*callbackPtr)(utils::createErrorResponse(utils::ErrorCode::INVALID_REQUEST, "需要multipart/form-data请求"));
            return;
        }
        
        // 获取boundary
        std::string boundary;
        size_t boundaryPos = contentType.find("boundary=");
        if (boundaryPos != std::string::npos) {
            boundary = contentType.substr(boundaryPos + 9); // 9 is the length of "boundary="
        } else {
            LOG_ERROR << "未找到boundary参数";
            (*callbackPtr)(utils::createErrorResponse(utils::ErrorCode::INVALID_REQUEST, "无效的multipart请求"));
            return;
        }
        
        LOG_INFO << "解析到boundary: " << boundary;
        
        // 检查请求体是否为空
        if (req->bodyLength() == 0) {
            (*callbackPtr)(utils::createErrorResponse(utils::ErrorCode::INVALID_REQUEST, "请求体为空"));
            return;
        }
        
        LOG_INFO << "请求体长度: " << req->bodyLength() << " 字节";
        
        // 解析multipart请求，提取文件内容
        std::string reqBody(req->bodyData(), req->bodyLength());
        
        // 在multipart请求中寻找source字段和文件数据
        std::string startDelimiter = "--" + boundary + "\r\n";
        std::string middleDelimiter = "\r\n--" + boundary + "\r\n";
        std::string endDelimiter = "\r\n--" + boundary + "--";
        
        std::string fileDataStr;
        size_t pos = 0;
        bool fileFound = false;
        
        // 遍历所有部分
        while (pos < reqBody.length()) {
            size_t partStart = pos;
            size_t partEnd;
            
            // 查找下一个分隔符
            if (pos == 0) {
                // 第一个部分以startDelimiter开始
                if (reqBody.substr(pos, startDelimiter.length()) != startDelimiter) {
                    LOG_ERROR << "请求体格式错误: 缺少开始分隔符";
                    (*callbackPtr)(utils::createErrorResponse(utils::ErrorCode::INVALID_REQUEST, "请求体格式错误"));
                    return;
                }
                pos += startDelimiter.length();
                partStart = pos;
                partEnd = reqBody.find(middleDelimiter, pos);
            } else {
                // 后续部分
                partEnd = reqBody.find(middleDelimiter, pos);
            }
            
            // 如果找不到中间分隔符，可能是最后一个部分
            if (partEnd == std::string::npos) {
                partEnd = reqBody.find(endDelimiter, pos);
                if (partEnd == std::string::npos) {
                    LOG_ERROR << "请求体格式错误: 缺少结束分隔符";
                    (*callbackPtr)(utils::createErrorResponse(utils::ErrorCode::INVALID_REQUEST, "请求体格式错误"));
                    return;
                }
            }
            
            // 解析当前部分
            std::string part = reqBody.substr(partStart, partEnd - partStart);
            
            // 检查是否包含source字段
            if (part.find("name=\"source\"") != std::string::npos || part.find("name='source'") != std::string::npos) {
                LOG_INFO << "找到source字段";
                
                // 提取文件内容 - 在两个\r\n\r\n之间的是头部信息，之后的是文件内容
                size_t headerEnd = part.find("\r\n\r\n");
                if (headerEnd != std::string::npos) {
                    fileDataStr = part.substr(headerEnd + 4); // +4表示跳过"\r\n\r\n"
                    fileFound = true;
                    LOG_INFO << "提取到文件数据，大小: " << fileDataStr.size() << " 字节";
                    break;
                } else {
                    LOG_ERROR << "未找到文件头结束标记";
                }
            }
            
            // 移动到下一个部分
            pos = partEnd + middleDelimiter.length();
            
            // 如果到达了结束分隔符，退出循环
            if (partEnd + endDelimiter.length() <= reqBody.length() && 
                reqBody.substr(partEnd, endDelimiter.length()) == endDelimiter) {
                break;
            }
        }
        
        if (!fileFound || fileDataStr.empty()) {
            LOG_ERROR << "未找到有效的文件数据";
            (*callbackPtr)(utils::createErrorResponse(utils::ErrorCode::INVALID_PARAMETER, "未找到有效的文件数据"));
            return;
        }
        
        // 将文件数据写入临时文件
        std::string tempDir;
        
        // 根据操作系统确定临时目录
        #ifdef _WIN32
            tempDir = "C:/temp/";  // Windows临时目录
            // 确保目录存在
            system("mkdir C:\\temp 2>NUL");
        #else
            tempDir = "/tmp/";     // Unix/Linux临时目录
        #endif
        
        std::string tempFileName = tempDir + "avatar_" + std::to_string(std::time(nullptr)) + ".jpg";
        LOG_INFO << "临时文件路径: " << tempFileName;
        
        std::ofstream outFile(tempFileName, std::ios::binary);
        if (!outFile) {
            LOG_ERROR << "无法创建临时文件: " << tempFileName;
            (*callbackPtr)(utils::createErrorResponse(utils::ErrorCode::SERVER_ERROR, "无法创建临时文件"));
            return;
        }
        
        // 写入文件数据
        outFile.write(fileDataStr.data(), fileDataStr.size());
        outFile.close();
        
        // 验证文件大小（限制为2MB）
        std::ifstream tempFile(tempFileName, std::ios::binary | std::ios::ate);
        if (!tempFile) {
            LOG_ERROR << "无法打开临时文件进行验证: " << tempFileName;
            std::remove(tempFileName.c_str());
            (*callbackPtr)(utils::createErrorResponse(utils::ErrorCode::SERVER_ERROR, "内部服务器错误"));
            return;
        }
        
        auto fileSize = tempFile.tellg();
        tempFile.close();
        
        LOG_INFO << "上传文件大小: " << fileSize << " 字节";
        
        if (fileSize > 2 * 1024 * 1024) {
            std::remove(tempFileName.c_str());
            (*callbackPtr)(utils::createErrorResponse(utils::ErrorCode::INVALID_PARAMETER, "图片大小不能超过2MB"));
            return;
        }
        
        // ImageHub API密钥
        const std::string IMAGE_HUB_API_KEY = "chv_s9Gk_c68593c1beea9e74ca78bf4598e6c22acfb50f2d2b4e411a3aec12aeea36ce4bc338cda2818eefb15243f12fa6ae4c30497b1c58edd9912356d687bf3b3d8480";
        
        // 使用Drogon的HTTP客户端上传图片，而不是curl命令
        auto client = drogon::HttpClient::newHttpClient("https://www.imagehub.cc");
        
        // 读取文件到内存
        std::ifstream inFile(tempFileName, std::ios::binary);
        if (!inFile) {
            LOG_ERROR << "无法读取临时文件: " << tempFileName;
            std::remove(tempFileName.c_str());
            (*callbackPtr)(utils::createErrorResponse(utils::ErrorCode::SERVER_ERROR, "无法读取临时文件"));
            return;
        }
        
        // 读取文件到内存
        std::vector<char> fileData((std::istreambuf_iterator<char>(inFile)), std::istreambuf_iterator<char>());
        inFile.close();
        
        LOG_INFO << "已读取文件到内存，大小: " << fileData.size() << " 字节";
        
        // 删除临时文件
        std::remove(tempFileName.c_str());
        
        // 使用libcurl进行上传
        CURL *curl;
        CURLcode res;
        std::string responseString;
        
        // 初始化CURL
        curl = curl_easy_init();
        if (!curl) {
            LOG_ERROR << "初始化CURL失败";
            (*callbackPtr)(utils::createErrorResponse(utils::ErrorCode::EXTERNAL_SERVICE_ERROR, "初始化上传服务失败"));
            return;
        }
        
        LOG_INFO << "初始化CURL成功";
        
        // 设置CURL参数
        curl_easy_setopt(curl, CURLOPT_URL, "https://www.imagehub.cc/api/1/upload");
        
        // 设置写入回调函数
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &responseString);
        
        // 创建HTTP头
        struct curl_slist *headers = NULL;
        headers = curl_slist_append(headers, "Accept: application/json");
        headers = curl_slist_append(headers, ("X-API-Key: " + IMAGE_HUB_API_KEY).c_str());
        curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
        
        // 创建multipart/form-data表单
        curl_mime *mime = curl_mime_init(curl);
        curl_mimepart *part = curl_mime_addpart(mime);
        
        // 添加图片文件部分
        curl_mime_name(part, "source");
        curl_mime_filename(part, "avatar.jpg");
        curl_mime_type(part, "image/jpeg");
        curl_mime_data(part, fileData.data(), fileData.size());
        
        // 设置form数据
        curl_easy_setopt(curl, CURLOPT_MIMEPOST, mime);
        
        LOG_INFO << "开始上传图片到图床...";
        
        // 执行请求
        res = curl_easy_perform(curl);
        
        // 检查请求结果
        if (res != CURLE_OK) {
            LOG_ERROR << "上传图片到图床失败: " << curl_easy_strerror(res);
            curl_mime_free(mime);
            curl_slist_free_all(headers);
            curl_easy_cleanup(curl);
            (*callbackPtr)(utils::createErrorResponse(utils::ErrorCode::EXTERNAL_SERVICE_ERROR, "上传图片到图床失败"));
            return;
        }
        
        LOG_INFO << "图床响应: " << responseString;
        
        // 解析响应
        Json::Value respJson;
        Json::Reader reader;
        if (!reader.parse(responseString, respJson) || !respJson.isObject()) {
            LOG_ERROR << "解析图床API响应失败: " << responseString;
            curl_mime_free(mime);
            curl_slist_free_all(headers);
            curl_easy_cleanup(curl);
            (*callbackPtr)(utils::createErrorResponse(utils::ErrorCode::EXTERNAL_SERVICE_ERROR, "解析图床响应失败"));
            return;
        }
        
        // 清理CURL资源
        curl_mime_free(mime);
        curl_slist_free_all(headers);
        curl_easy_cleanup(curl);
        
        // 检查上传是否成功
        if (respJson["status_code"].asInt() != 200) {
            std::string errorMsg = "图床上传失败: ";
            if (respJson["status_txt"].isString()) {
                errorMsg += respJson["status_txt"].asString();
            }
            LOG_ERROR << errorMsg;
            (*callbackPtr)(utils::createErrorResponse(utils::ErrorCode::EXTERNAL_SERVICE_ERROR, errorMsg));
            return;
        }
        
        // 获取图片URL
        if (!respJson["image"].isObject() || !respJson["image"]["url"].isString()) {
            LOG_ERROR << "图床响应格式错误";
            (*callbackPtr)(utils::createErrorResponse(utils::ErrorCode::EXTERNAL_SERVICE_ERROR, "图床响应格式错误"));
            return;
        }
        
        std::string imageUrl = respJson["image"]["url"].asString();
        LOG_INFO << "图片上传成功，URL: " << imageUrl;
        
        // 将图片URL保存到用户资料中
        auto& dbManager = DbManager::getInstance();
        dbManager.executeQuery(
            "UPDATE users SET avatar = $1, updated_at = NOW() WHERE uuid = $2 RETURNING username, display_name, email, bio, avatar",
            [callbackPtr, imageUrl](const drogon::orm::Result& result) {
                if (result.size() > 0) {
                    Json::Value userData;
                    userData["username"] = result[0]["username"].as<std::string>();
                    userData["display_name"] = result[0]["display_name"].as<std::string>();
                    userData["email"] = result[0]["email"].as<std::string>();
                    userData["avatar"] = imageUrl;
                    
                    if (!result[0]["bio"].isNull()) {
                        userData["bio"] = result[0]["bio"].as<std::string>();
                    }
                    
                    // 返回成功响应，包含图片URL和用户数据
                    Json::Value responseData;
                    responseData["image_url"] = imageUrl;
                    responseData["user_info"] = userData;
                    
                    (*callbackPtr)(utils::createSuccessResponse("头像上传并更新成功", responseData));
                } else {
                    (*callbackPtr)(utils::createErrorResponse(utils::ErrorCode::RESOURCE_NOT_FOUND, "用户不存在"));
                }
            },
            [callbackPtr](const drogon::orm::DrogonDbException& e) {
                LOG_ERROR << "更新用户头像失败: " << e.base().what();
                (*callbackPtr)(utils::createErrorResponse(utils::ErrorCode::DB_QUERY_ERROR, "更新用户头像失败"));
            },
            imageUrl, userUuid
        );
    } catch (const std::exception& e) {
        LOG_ERROR << "头像上传异常: " << e.what();
        (*callbackPtr)(utils::createErrorResponse(utils::ErrorCode::SERVER_ERROR));
    }
}

/**
 * 更新用户社交链接
 */
void AuthController::updateSocialLinks(const drogon::HttpRequestPtr& req, 
                           std::function<void(const drogon::HttpResponsePtr&)>&& callback) const {
    // 创建回调函数的共享指针，防止回调函数被意外移动
    auto callbackPtr = std::make_shared<std::function<void(const drogon::HttpResponsePtr&)>>(std::move(callback));
    
    try {
        // 从请求属性中获取用户UUID（已由JWT过滤器添加）
        std::string userUuid = req->getAttributes()->get<std::string>("user_uuid");
        if (userUuid.empty()) {
            (*callbackPtr)(utils::createErrorResponse(utils::ErrorCode::UNAUTHORIZED, "请先登录"));
            return;
        }
        
        // 获取请求体
        auto jsonBody = req->getJsonObject();
        if (!jsonBody) {
            (*callbackPtr)(utils::createErrorResponse(utils::ErrorCode::INVALID_REQUEST, "无效的请求格式"));
            return;
        }
        
        // 定义可能的链接类型
        const std::vector<std::string> linkTypes = {
            "github", "website", "twitter", "weibo", "linkedin", "contact_email"
        };
        
        // 验证URL格式的函数
        auto validateUrl = [](const std::string& url) -> bool {
            return url.empty() || url.find("http://") == 0 || url.find("https://") == 0;
        };
        
        // 验证并收集链接数据
        std::vector<std::pair<std::string, std::string>> validLinks;
        
        for (const auto& type : linkTypes) {
            if ((*jsonBody)[type].isString()) {
                std::string url = (*jsonBody)[type].asString();
                
                // 特殊处理email类型
                if (type == "contact_email") {
                    if (!url.empty() && url.find('@') == std::string::npos) {
                        (*callbackPtr)(utils::createErrorResponse(utils::ErrorCode::INVALID_REQUEST, "无效的邮箱格式"));
                        return;
                    }
                } else {
                    // 验证URL格式
                    if (!validateUrl(url)) {
                        (*callbackPtr)(utils::createErrorResponse(utils::ErrorCode::INVALID_REQUEST, 
                            "无效的URL格式，必须以http://或https://开头"));
                        return;
                    }
                }
                
                // 如果URL不为空，添加到有效链接列表
                if (!url.empty()) {
                    validLinks.push_back(std::make_pair(type, url));
                }
            }
        }
        
        // 开始数据库事务
        auto& dbManager = DbManager::getInstance();
        auto clientPtr = drogon::app().getDbClient();
        auto transPtr = clientPtr->newTransaction();
        
        try {
            // 先删除现有的链接
            transPtr->execSqlSync("DELETE FROM user_links WHERE user_uuid = $1", userUuid);
            
            // 插入新的链接
            for (const auto& link : validLinks) {
                transPtr->execSqlSync(
                    "INSERT INTO user_links (user_uuid, link_type, link_url, created_at, updated_at) "
                    "VALUES ($1, $2, $3, NOW(), NOW())",
                    userUuid, link.first, link.second
                );
            }
            
            // 提交事务
            transPtr->execSqlSync("COMMIT");
            
            // 查询更新后的用户完整信息
            dbManager.executeQuery(
                "SELECT uuid, username, email, display_name, bio, avatar FROM users WHERE uuid = $1",
                [=, &dbManager](const drogon::orm::Result& result) {
                    if (result.size() > 0) {
                        Json::Value userData;
                        userData["uuid"] = result[0]["uuid"].as<std::string>();
                        userData["username"] = result[0]["username"].as<std::string>();
                        userData["email"] = result[0]["email"].as<std::string>();
                        
                        if (!result[0]["display_name"].isNull()) {
                            userData["display_name"] = result[0]["display_name"].as<std::string>();
                        }
                        
                        if (!result[0]["bio"].isNull()) {
                            userData["bio"] = result[0]["bio"].as<std::string>();
                        }
                        
                        if (!result[0]["avatar"].isNull()) {
                            userData["avatar"] = result[0]["avatar"].as<std::string>();
                        }
                        
                        // 查询用户的社交链接
                        dbManager.executeQuery(
                            "SELECT link_type, link_url FROM user_links WHERE user_uuid = $1",
                            [=](const drogon::orm::Result& linkResult) mutable {
                                // 添加社交链接到用户数据
                                for (const auto& row : linkResult) {
                                    std::string linkType = row["link_type"].as<std::string>();
                                    std::string linkUrl = row["link_url"].as<std::string>();
                                    userData[linkType] = linkUrl;
                                }
                                
                                // 返回成功响应
                                (*callbackPtr)(utils::createSuccessResponse("社交链接更新成功", userData));
                            },
                            [=](const drogon::orm::DrogonDbException& e) mutable {
                                LOG_ERROR << "获取用户链接失败: " << e.base().what();
                                // 仍然返回基本用户数据
                                (*callbackPtr)(utils::createSuccessResponse("社交链接更新成功，但无法获取完整链接信息", userData));
                            },
                            userUuid
                        );
                    } else {
                        (*callbackPtr)(utils::createErrorResponse(utils::ErrorCode::RESOURCE_NOT_FOUND, "用户不存在"));
                    }
                },
                [=](const drogon::orm::DrogonDbException& e) {
                    LOG_ERROR << "获取用户信息失败: " << e.base().what();
                    (*callbackPtr)(utils::createErrorResponse(utils::ErrorCode::DB_QUERY_ERROR, "更新社交链接成功，但无法获取用户信息"));
                },
                userUuid
            );
        } catch (const drogon::orm::DrogonDbException& e) {
            LOG_ERROR << "更新用户社交链接失败: " << e.base().what();
            (*callbackPtr)(utils::createErrorResponse(utils::ErrorCode::DB_QUERY_ERROR, "更新社交链接失败"));
        }
    } catch (const std::exception& e) {
        LOG_ERROR << "更新用户社交链接异常: " << e.what();
        (*callbackPtr)(utils::createErrorResponse(utils::ErrorCode::SERVER_ERROR));
    }
} 