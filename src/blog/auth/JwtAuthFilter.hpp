#pragma once

#include <drogon/HttpFilter.h>
#include "JwtManager.hpp"

namespace blog {
namespace auth {

/**
 * JWT认证过滤器
 * 用于验证请求中的JWT令牌
 */
class JwtAuthFilter : public drogon::HttpFilter<JwtAuthFilter, false> {
public:
    /**
     * 构造函数
     * @param secret JWT密钥
     */
    JwtAuthFilter(const std::string& secret) : m_jwtManager(secret) {}
    
    /**
     * 过滤器处理方法
     */
    virtual void doFilter(const drogon::HttpRequestPtr& req,
                         drogon::FilterCallback&& fcb,
                         drogon::FilterChainCallback&& fccb) override {
        // 从请求中获取令牌
        std::string token = JwtManager::getTokenFromRequest(req);
        
        // 没有令牌，返回未授权错误
        if (token.empty()) {
            Json::Value json;
            json["status"] = "error";
            json["message"] = "未授权的访问";
            auto resp = drogon::HttpResponse::newHttpJsonResponse(json);
            resp->setStatusCode(drogon::k401Unauthorized);
            fcb(resp);
            return;
        }
        
        // 验证令牌
        if (!m_jwtManager.verifyToken(token)) {
            // 令牌无效，返回未授权错误
            Json::Value json;
            json["status"] = "error";
            json["message"] = "令牌无效或已过期";
            auto resp = drogon::HttpResponse::newHttpJsonResponse(json);
            resp->setStatusCode(drogon::k401Unauthorized);
            fcb(resp);
            return;
        }
        
        // 将用户信息添加到请求属性中
        int userId = m_jwtManager.getUserIdFromToken(token);
        std::string username = m_jwtManager.getUsernameFromToken(token);
        bool isAdmin = m_jwtManager.isAdminFromToken(token);
        
        req->getAttributes()->insert("user_id", userId);
        req->getAttributes()->insert("username", username);
        req->getAttributes()->insert("is_admin", isAdmin);
        
        // 继续处理请求
        fccb();
    }
    
private:
    JwtManager m_jwtManager;
};

/**
 * 管理员认证过滤器
 * 用于验证用户是否具有管理员权限
 */
class AdminAuthFilter : public drogon::HttpFilter<AdminAuthFilter, false> {
public:
    /**
     * 构造函数
     * @param secret JWT密钥
     */
    AdminAuthFilter(const std::string& secret) : m_jwtManager(secret) {}
    
    /**
     * 过滤器处理方法
     */
    virtual void doFilter(const drogon::HttpRequestPtr& req,
                         drogon::FilterCallback&& fcb,
                         drogon::FilterChainCallback&& fccb) override {
        // 从请求中获取令牌
        std::string token = JwtManager::getTokenFromRequest(req);
        
        // 没有令牌，返回未授权错误
        if (token.empty()) {
            Json::Value json;
            json["status"] = "error";
            json["message"] = "未授权的访问";
            auto resp = drogon::HttpResponse::newHttpJsonResponse(json);
            resp->setStatusCode(drogon::k401Unauthorized);
            fcb(resp);
            return;
        }
        
        // 验证令牌
        if (!m_jwtManager.verifyToken(token)) {
            // 令牌无效，返回未授权错误
            Json::Value json;
            json["status"] = "error";
            json["message"] = "令牌无效或已过期";
            auto resp = drogon::HttpResponse::newHttpJsonResponse(json);
            resp->setStatusCode(drogon::k401Unauthorized);
            fcb(resp);
            return;
        }
        
        // 检查是否管理员
        if (!m_jwtManager.isAdminFromToken(token)) {
            // 不是管理员，返回禁止访问错误
            Json::Value json;
            json["status"] = "error";
            json["message"] = "需要管理员权限";
            auto resp = drogon::HttpResponse::newHttpJsonResponse(json);
            resp->setStatusCode(drogon::k403Forbidden);
            fcb(resp);
            return;
        }
        
        // 将用户信息添加到请求属性中
        int userId = m_jwtManager.getUserIdFromToken(token);
        std::string username = m_jwtManager.getUsernameFromToken(token);
        
        req->getAttributes()->insert("user_id", userId);
        req->getAttributes()->insert("username", username);
        req->getAttributes()->insert("is_admin", true);
        
        // 继续处理请求
        fccb();
    }
    
private:
    JwtManager m_jwtManager;
};

} // namespace auth
} // namespace blog 