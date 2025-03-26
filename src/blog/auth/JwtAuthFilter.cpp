#include "JwtAuthFilter.hpp"
#include <drogon/HttpResponse.h>
#include <drogon/HttpAppFramework.h>
#include "blog/utils/ErrorCode.hpp"
#include "blog/utils/HttpUtils.hpp"

JwtAuthFilter::JwtAuthFilter() {
    // 创建JWT管理器
    m_jwtManager = std::make_shared<JwtManager>();
}

void JwtAuthFilter::doFilter(const drogon::HttpRequestPtr &req,
                           drogon::FilterCallback &&fcb,
                           drogon::FilterChainCallback &&fccb) {
    LOG_INFO << "JWT认证过滤器开始处理请求: " << req->getPath();
    
    try {
        // 尝试从请求头中获取Bearer令牌
        std::string token;
        std::string authHeader = req->getHeader("Authorization");
        
        LOG_INFO << "Authorization头: " << (authHeader.empty() ? "未提供" : "已提供");
        
        // 检查Authorization头
        if (!authHeader.empty() && authHeader.find("Bearer ") == 0) {
            token = authHeader.substr(7); // 跳过"Bearer "
            LOG_INFO << "从Authorization头中获取到令牌, 长度: " << token.length();
        } else {
            // 尝试从Cookie中获取令牌
            const auto& cookies = req->getCookies();
            auto it = cookies.find("token");
            if (it != cookies.end()) {
                token = it->second;
                LOG_INFO << "从Cookie中获取到令牌, 长度: " << token.length();
            } else {
                LOG_INFO << "未在Cookie中找到令牌";
            }
        }
        
        // 检查令牌是否提供
        if (token.empty()) {
            LOG_WARN << "无法从请求中获取认证令牌";
            auto resp = utils::createErrorResponse(utils::ErrorCode::UNAUTHORIZED);
            fcb(resp);
            return;
        }
        
        // 验证令牌
        JwtManager::VerifyResult result;
        if (!m_jwtManager->verifyToken(token, result)) {
            LOG_WARN << "令牌验证失败: " << result.reason;
            auto resp = utils::createErrorResponse(utils::ErrorCode::UNAUTHORIZED);
            fcb(resp);
            return;
        }
        
        LOG_INFO << "令牌验证成功, 用户: " << result.username;
        
        // 将用户信息添加到请求属性中
        req->getAttributes()->insert("user_uuid", result.userUuid);
        req->getAttributes()->insert("username", result.username);
        req->getAttributes()->insert("is_admin", result.isAdmin);
        
        // 继续处理
        LOG_INFO << "JWT验证通过，继续处理请求";
        fccb();
    } catch (const std::exception &e) {
        LOG_ERROR << "JWT认证过滤器异常: " << e.what();
        auto resp = utils::createErrorResponse(utils::ErrorCode::INTERNAL_ERROR, "服务器内部错误");
        fcb(resp);
    }
}

// AdminAuthFilter实现
AdminAuthFilter::AdminAuthFilter() {
    // 创建JWT管理器
    m_jwtManager = std::make_shared<JwtManager>();
}

void AdminAuthFilter::doFilter(const drogon::HttpRequestPtr &req,
                             drogon::FilterCallback &&fcb,
                             drogon::FilterChainCallback &&fccb) {
    LOG_INFO << "管理员认证过滤器开始处理请求: " << req->getPath();
    
    try {
        // 尝试从请求头中获取Bearer令牌
        std::string token;
        std::string authHeader = req->getHeader("Authorization");
        
        LOG_INFO << "Authorization头: " << (authHeader.empty() ? "未提供" : "已提供");
        
        // 检查Authorization头
        if (!authHeader.empty() && authHeader.find("Bearer ") == 0) {
            token = authHeader.substr(7); // 跳过"Bearer "
            LOG_INFO << "从Authorization头中获取到令牌, 长度: " << token.length();
        } else {
            // 尝试从Cookie中获取令牌
            const auto& cookies = req->getCookies();
            auto it = cookies.find("token");
            if (it != cookies.end()) {
                token = it->second;
                LOG_INFO << "从Cookie中获取到令牌, 长度: " << token.length();
            } else {
                LOG_INFO << "未在Cookie中找到令牌";
            }
        }
        
        // 检查令牌是否提供
        if (token.empty()) {
            LOG_WARN << "无法从请求中获取认证令牌";
            auto resp = utils::createErrorResponse(utils::ErrorCode::UNAUTHORIZED);
            fcb(resp);
            return;
        }
        
        // 验证令牌
        JwtManager::VerifyResult result;
        if (!m_jwtManager->verifyToken(token, result)) {
            LOG_WARN << "令牌验证失败: " << result.reason;
            auto resp = utils::createErrorResponse(utils::ErrorCode::UNAUTHORIZED);
            fcb(resp);
            return;
        }
        
        // 检查是否具有管理员权限
        if (!result.isAdmin) {
            LOG_WARN << "用户 " << result.username << " 不是管理员，拒绝访问";
            auto resp = utils::createErrorResponse(utils::ErrorCode::FORBIDDEN, "需要管理员权限");
            fcb(resp);
            return;
        }
        
        LOG_INFO << "管理员令牌验证成功, 用户: " << result.username;
        
        // 将用户信息添加到请求属性中
        req->getAttributes()->insert("user_uuid", result.userUuid);
        req->getAttributes()->insert("username", result.username);
        req->getAttributes()->insert("is_admin", true);
        
        // 继续处理
        LOG_INFO << "管理员验证通过，继续处理请求";
        fccb();
    } catch (const std::exception &e) {
        LOG_ERROR << "管理员认证过滤器异常: " << e.what();
        auto resp = utils::createErrorResponse(utils::ErrorCode::INTERNAL_ERROR, "服务器内部错误");
        fcb(resp);
    }
} 