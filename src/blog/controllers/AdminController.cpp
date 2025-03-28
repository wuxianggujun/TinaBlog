#include "AdminController.hpp"
#include "../db/DbManager.hpp"
#include "../utils/HttpUtils.hpp"
#include "../utils/ErrorCode.hpp"
#include <json/json.h>
#include <drogon/HttpResponse.h>
#include <drogon/HttpRequest.h>
#include <drogon/utils/Utilities.h>
#include <iostream>

using namespace api::v1;

// 验证管理员权限
bool AdminController::validateAdmin(const drogon::HttpRequestPtr& req, 
                             std::function<void(const drogon::HttpResponsePtr&)>& callback) const {
    auto session = req->getSession();
    if (!session) {
        auto resp = utils::HttpUtils::buildUnauthorizedResponse("未登录");
        callback(resp);
        return false;
    }

    if (!session->find("user_uuid")) {
        auto resp = utils::HttpUtils::buildUnauthorizedResponse("会话无效");
        callback(resp);
        return false;
    }

    std::string userUuid = session->get<std::string>("user_uuid");
    
    try {
        auto& dbManager = DbManager::getInstance();
        auto result = dbManager.execSyncQuery(
            "SELECT is_admin FROM users WHERE uuid = $1", 
            userUuid
        );
        
        if (result.size() == 0) {
            auto resp = utils::HttpUtils::buildUnauthorizedResponse("用户不存在");
            callback(resp);
            return false;
        }
        
        bool isAdmin = result[0]["is_admin"].as<bool>();
        if (!isAdmin) {
            auto resp = utils::HttpUtils::buildForbiddenResponse("需要管理员权限");
            callback(resp);
            return false;
        }
        
        return true;
    } catch (const std::exception& e) {
        auto resp = utils::HttpUtils::buildServerErrorResponse("权限验证过程中发生错误");
        callback(resp);
        return false;
    }
}

// 获取所有用户
void AdminController::getUsers(const drogon::HttpRequestPtr& req, 
                      std::function<void(const drogon::HttpResponsePtr&)>&& callback) const {
    auto callbackPtr = std::make_shared<std::function<void(const drogon::HttpResponsePtr&)>>(std::move(callback));
    
    if (!validateAdmin(req, *callbackPtr)) {
        return;
    }

    try {
        auto& dbManager = DbManager::getInstance();
        auto result = dbManager.execSyncQuery(
            "SELECT uuid, username, display_name, email, bio, avatar, github_url, "
            "website_url, twitter_url, weibo_url, linkedin_url, contact_email, "
            "is_admin, is_banned, ban_reason, banned_at, created_at, updated_at "
            "FROM users ORDER BY created_at DESC"
        );
        
        Json::Value users(Json::arrayValue);
        for (const auto& row : result) {
            Json::Value user;
            user["uuid"] = row["uuid"].as<std::string>();
            user["username"] = row["username"].as<std::string>();
            user["email"] = row["email"].as<std::string>();
            
            // 处理可能为空的字段
            if (!row["display_name"].isNull())
                user["display_name"] = row["display_name"].as<std::string>();
            
            if (!row["bio"].isNull())
                user["bio"] = row["bio"].as<std::string>();
            
            if (!row["avatar"].isNull())
                user["avatar"] = row["avatar"].as<std::string>();
            
            if (!row["github_url"].isNull())
                user["github_url"] = row["github_url"].as<std::string>();
            
            if (!row["website_url"].isNull())
                user["website_url"] = row["website_url"].as<std::string>();
            
            if (!row["twitter_url"].isNull())
                user["twitter_url"] = row["twitter_url"].as<std::string>();
            
            if (!row["weibo_url"].isNull())
                user["weibo_url"] = row["weibo_url"].as<std::string>();
            
            if (!row["linkedin_url"].isNull())
                user["linkedin_url"] = row["linkedin_url"].as<std::string>();
            
            if (!row["contact_email"].isNull())
                user["contact_email"] = row["contact_email"].as<std::string>();
            
            if (!row["ban_reason"].isNull())
                user["ban_reason"] = row["ban_reason"].as<std::string>();
            
            if (!row["banned_at"].isNull())
                user["banned_at"] = row["banned_at"].as<std::string>();
            
            // 处理布尔值
            user["is_admin"] = row["is_admin"].as<bool>();
            user["is_banned"] = row["is_banned"].as<bool>();
            
            // 时间戳
            user["created_at"] = row["created_at"].as<std::string>();
            user["updated_at"] = row["updated_at"].as<std::string>();
            
            users.append(user);
        }
        
        auto resp = utils::HttpUtils::buildSuccessResponse(users);
        (*callbackPtr)(resp);
    } catch (const std::exception& e) {
        std::cerr << "获取用户列表错误: " << e.what() << std::endl;
        auto resp = utils::HttpUtils::buildServerErrorResponse(
            "获取用户列表失败: " + std::string(e.what())
        );
        (*callbackPtr)(resp);
    }
}

// 封禁用户
void AdminController::banUser(const drogon::HttpRequestPtr& req, 
                    std::function<void(const drogon::HttpResponsePtr&)>&& callback,
                    const std::string& uuid) const {
    auto callbackPtr = std::make_shared<std::function<void(const drogon::HttpResponsePtr&)>>(std::move(callback));
    
    if (!validateAdmin(req, *callbackPtr)) {
        return;
    }
    
    auto session = req->getSession();
    std::string adminUuid = session->get<std::string>("user_uuid");
    
    // 解析请求体获取封禁原因
    std::string banReason;
    try {
        auto json = req->getJsonObject();
        if (json && (*json)["reason"].isString()) {
            banReason = (*json)["reason"].asString();
        }
    } catch (...) {
        // 如果没有提供封禁原因，使用默认文本
        banReason = "管理员执行的封禁";
    }
    
    // 不允许封禁自己或其他管理员
    try {
        auto& dbManager = DbManager::getInstance();
        
        // 检查目标用户是否是管理员或自己
        auto checkResult = dbManager.execSyncQuery(
            "SELECT is_admin FROM users WHERE uuid = $1", 
            uuid
        );
        
        if (checkResult.size() == 0) {
            auto resp = utils::HttpUtils::buildErrorResponse(
                utils::ErrorCode::RESOURCE_NOT_FOUND,
                "用户不存在"
            );
            (*callbackPtr)(resp);
            return;
        }
        
        if (uuid == adminUuid) {
            auto resp = utils::HttpUtils::buildErrorResponse(
                utils::ErrorCode::BAD_REQUEST,
                "不能封禁自己"
            );
            (*callbackPtr)(resp);
            return;
        }
        
        if (checkResult[0]["is_admin"].as<bool>()) {
            auto resp = utils::HttpUtils::buildErrorResponse(
                utils::ErrorCode::FORBIDDEN,
                "不能封禁管理员"
            );
            (*callbackPtr)(resp);
            return;
        }
        
        // 更新用户封禁状态
        auto result = dbManager.execSyncQuery(
            "UPDATE users SET is_banned = TRUE, ban_reason = $1, banned_at = CURRENT_TIMESTAMP, "
            "banned_by = $2 WHERE uuid = $3 RETURNING uuid, username, is_banned",
            banReason, adminUuid, uuid
        );
        
        if (result.size() == 0) {
            auto resp = utils::HttpUtils::buildErrorResponse(
                utils::ErrorCode::RESOURCE_NOT_FOUND,
                "用户不存在"
            );
            (*callbackPtr)(resp);
            return;
        }
        
        Json::Value responseData;
        responseData["uuid"] = result[0]["uuid"].as<std::string>();
        responseData["username"] = result[0]["username"].as<std::string>();
        responseData["is_banned"] = result[0]["is_banned"].as<bool>();
        responseData["message"] = "用户已被封禁";
        
        auto resp = utils::HttpUtils::buildSuccessResponse(responseData);
        (*callbackPtr)(resp);
        
    } catch (const std::exception& e) {
        std::cerr << "封禁用户错误: " << e.what() << std::endl;
        auto resp = utils::HttpUtils::buildServerErrorResponse(
            "封禁用户失败: " + std::string(e.what())
        );
        (*callbackPtr)(resp);
    }
}

// 解除封禁
void AdminController::unbanUser(const drogon::HttpRequestPtr& req, 
                      std::function<void(const drogon::HttpResponsePtr&)>&& callback,
                      const std::string& uuid) const {
    auto callbackPtr = std::make_shared<std::function<void(const drogon::HttpResponsePtr&)>>(std::move(callback));
    
    if (!validateAdmin(req, *callbackPtr)) {
        return;
    }
    
    try {
        auto& dbManager = DbManager::getInstance();
        
        auto result = dbManager.execSyncQuery(
            "UPDATE users SET is_banned = FALSE, ban_reason = NULL, banned_at = NULL, "
            "banned_by = NULL WHERE uuid = $1 RETURNING uuid, username, is_banned",
            uuid
        );
        
        if (result.size() == 0) {
            auto resp = utils::HttpUtils::buildErrorResponse(
                utils::ErrorCode::RESOURCE_NOT_FOUND,
                "用户不存在"
            );
            (*callbackPtr)(resp);
            return;
        }
        
        Json::Value responseData;
        responseData["uuid"] = result[0]["uuid"].as<std::string>();
        responseData["username"] = result[0]["username"].as<std::string>();
        responseData["is_banned"] = result[0]["is_banned"].as<bool>();
        responseData["message"] = "用户已解除封禁";
        
        auto resp = utils::HttpUtils::buildSuccessResponse(responseData);
        (*callbackPtr)(resp);
        
    } catch (const std::exception& e) {
        std::cerr << "解除封禁错误: " << e.what() << std::endl;
        auto resp = utils::HttpUtils::buildServerErrorResponse(
            "解除封禁失败: " + std::string(e.what())
        );
        (*callbackPtr)(resp);
    }
}

// 删除用户
void AdminController::deleteUser(const drogon::HttpRequestPtr& req, 
                       std::function<void(const drogon::HttpResponsePtr&)>&& callback,
                       const std::string& uuid) const {
    auto callbackPtr = std::make_shared<std::function<void(const drogon::HttpResponsePtr&)>>(std::move(callback));
    
    if (!validateAdmin(req, *callbackPtr)) {
        return;
    }
    
    auto session = req->getSession();
    std::string adminUuid = session->get<std::string>("user_uuid");
    
    // 不允许删除自己或其他管理员
    try {
        auto& dbManager = DbManager::getInstance();
        
        // 检查目标用户是否是管理员或自己
        auto checkResult = dbManager.execSyncQuery(
            "SELECT is_admin, username FROM users WHERE uuid = $1", 
            uuid
        );
        
        if (checkResult.size() == 0) {
            auto resp = utils::HttpUtils::buildErrorResponse(
                utils::ErrorCode::RESOURCE_NOT_FOUND,
                "用户不存在"
            );
            (*callbackPtr)(resp);
            return;
        }
        
        std::string username = checkResult[0]["username"].as<std::string>();
        
        if (uuid == adminUuid) {
            auto resp = utils::HttpUtils::buildErrorResponse(
                utils::ErrorCode::BAD_REQUEST,
                "不能删除自己"
            );
            (*callbackPtr)(resp);
            return;
        }
        
        if (checkResult[0]["is_admin"].as<bool>()) {
            auto resp = utils::HttpUtils::buildErrorResponse(
                utils::ErrorCode::FORBIDDEN,
                "不能删除管理员账户"
            );
            (*callbackPtr)(resp);
            return;
        }
        
        // 开始事务
        auto clientPtr = drogon::app().getDbClient();
        auto transPtr = clientPtr->newTransaction();
        
        // 1. 首先获取用户相关信息以记录日志
        std::string userInfo;
        try {
            auto userResult = transPtr->execSqlSync(
                "SELECT uuid, username, email FROM users WHERE uuid = $1", 
                uuid
            );
            
            if (userResult.size() > 0) {
                userInfo = "UUID: " + userResult[0]["uuid"].as<std::string>() + 
                           ", Username: " + userResult[0]["username"].as<std::string>() + 
                           ", Email: " + userResult[0]["email"].as<std::string>();
            }
        } catch (...) {
            // 如果获取信息失败，继续执行删除
            userInfo = "UUID: " + uuid;
        }
        
        // 2. 删除用户记录
        // 由于外键约束和ON DELETE CASCADE，文章和评论会自动删除
        transPtr->execSqlSync("DELETE FROM users WHERE uuid = $1", uuid);
        
        // 3. 记录删除操作到日志
        try {
            transPtr->execSqlSync(
                "INSERT INTO admin_logs (action, admin_uuid, target_uuid, details) VALUES ($1, $2, $3, $4)",
                "DELETE_USER", adminUuid, uuid, userInfo
            );
        } catch (...) {
            // 日志记录失败不影响主要操作
            std::cerr << "无法记录用户删除日志: " << userInfo << std::endl;
        }
        
        // 提交事务
        transPtr->commit();
        
        Json::Value responseData;
        responseData["uuid"] = uuid;
        responseData["username"] = username;
        responseData["message"] = "用户已永久删除";
        
        auto resp = utils::HttpUtils::buildSuccessResponse(responseData);
        (*callbackPtr)(resp);
        
    } catch (const std::exception& e) {
        std::cerr << "删除用户错误: " << e.what() << std::endl;
        auto resp = utils::HttpUtils::buildServerErrorResponse(
            "删除用户失败: " + std::string(e.what())
        );
        (*callbackPtr)(resp);
    }
} 