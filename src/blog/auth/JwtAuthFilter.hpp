#pragma once

#include <drogon/HttpFilter.h>
#include <drogon/drogon.h>
#include "JwtManager.hpp"

// 简化过滤器，参照示例
class JwtAuthFilter : public drogon::HttpFilter<JwtAuthFilter>
{
public:
    /**
     * 构造函数
     */
    JwtAuthFilter() 
        : m_jwtManager(getJwtSecret())
    {
    }

    /**
     * 获取JWT密钥
     */
    static std::string getJwtSecret()
    {
        std::string secret = "your-secret-key-change-this-in-production";
        try {
            auto& config = drogon::app().getCustomConfig();
            if (config.isMember("jwt_secret")) {
                secret = config["jwt_secret"].asString();
            }
        } catch (...) {
            // 使用默认密钥
        }
        return secret;
    }

    /**
     * 过滤器处理方法
     */
    virtual void doFilter(const drogon::HttpRequestPtr& req,
                          drogon::FilterCallback&& fcb,
                          drogon::FilterChainCallback&& fccb) override
    {
        // 从请求中获取令牌
        std::string token = JwtManager::getTokenFromRequest(req);

        // 没有令牌，返回未授权错误
        if (token.empty())
        {
            Json::Value json;
            json["status"] = "error";
            json["message"] = "未授权的访问";
            auto resp = drogon::HttpResponse::newHttpJsonResponse(json);
            resp->setStatusCode(drogon::k401Unauthorized);
            fcb(resp);
            return;
        }

        // 验证令牌
        if (!m_jwtManager.verifyToken(token))
        {
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

// 同样简化管理员过滤器
class AdminAuthFilter : public drogon::HttpFilter<AdminAuthFilter>
{
public:
    /**
     * 构造函数
     */
    AdminAuthFilter() 
        : m_jwtManager(JwtAuthFilter::getJwtSecret())
    {
    }

    /**
     * 过滤器处理方法
     */
    virtual void doFilter(const drogon::HttpRequestPtr& req,
                          drogon::FilterCallback&& fcb,
                          drogon::FilterChainCallback&& fccb) override
    {
        // 从请求中获取令牌
        std::string token = JwtManager::getTokenFromRequest(req);

        // 没有令牌，返回未授权错误
        if (token.empty())
        {
            Json::Value json;
            json["status"] = "error";
            json["message"] = "未授权的访问";
            auto resp = drogon::HttpResponse::newHttpJsonResponse(json);
            resp->setStatusCode(drogon::k401Unauthorized);
            fcb(resp);
            return;
        }

        // 验证令牌
        if (!m_jwtManager.verifyToken(token))
        {
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
        if (!m_jwtManager.isAdminFromToken(token))
        {
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
