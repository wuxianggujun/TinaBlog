#pragma once
#include <drogon/HttpController.h>
#include "blog/db/DbManager.hpp"
#include "blog/auth/JwtManager.hpp"
#include "blog/auth/JwtAuthFilter.hpp"

/**
 * 认证控制器
 * 处理登录、注册、登出等认证相关功能
 */
class AuthController : public drogon::HttpController<AuthController>
{
public:
    METHOD_LIST_BEGIN
    // 设置API路径前缀
    ADD_METHOD_TO(AuthController::login, "/api/auth/login", drogon::Post);
    // 使用JwtAuthFilter过滤器保护getUserInfo接口 - 参照示例简化
    ADD_METHOD_TO(AuthController::getUserInfo, "/api/auth/info", drogon::Get, "JwtAuthFilter");
    METHOD_LIST_END
    
    /**
     * 用户登录
     * @param req 请求对象
     * @param callback 回调函数
     */
    void login(const drogon::HttpRequestPtr& req, 
               std::function<void(const drogon::HttpResponsePtr&)>&& callback) const;
    
    /**
     * 获取当前登录用户信息
     * @param req 请求对象
     * @param callback 回调函数
     */
    void getUserInfo(const drogon::HttpRequestPtr& req, 
                    std::function<void(const drogon::HttpResponsePtr&)>&& callback) const;
                    
    /**
     * 构造函数
     */
    AuthController();
    
private:
    std::string m_jwtSecret;
};
