#pragma once

#include <drogon/HttpFilter.h>
#include <drogon/drogon.h>
#include "JwtManager.hpp"
#include "blog/utils/HttpUtils.hpp"
#include "blog/utils/ErrorCode.hpp"

/**
 * JWT认证过滤器，用于验证请求中的JWT令牌
 */
class JwtAuthFilter : public drogon::HttpFilter<JwtAuthFilter>
{
public:
    /**
     * 构造函数
     */
    JwtAuthFilter();

    /**
     * 过滤器处理方法
     */
    virtual void doFilter(const drogon::HttpRequestPtr& req,
                          drogon::FilterCallback&& fcb,
                          drogon::FilterChainCallback&& fccb) override;

private:
    std::string m_jwtSecret;
    std::shared_ptr<JwtManager> m_jwtManager;
};

/**
 * 管理员认证过滤器，用于验证请求是否来自管理员
 */
class AdminAuthFilter : public drogon::HttpFilter<AdminAuthFilter>
{
public:
    /**
     * 构造函数
     */
    AdminAuthFilter();

    /**
     * 过滤器处理方法
     */
    virtual void doFilter(const drogon::HttpRequestPtr& req,
                          drogon::FilterCallback&& fcb,
                          drogon::FilterChainCallback&& fccb) override;

private:
    std::string m_jwtSecret;
    std::shared_ptr<JwtManager> m_jwtManager;
}; 
