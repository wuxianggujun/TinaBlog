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
    ADD_METHOD_TO(AuthController::registerUser, "/api/auth/register", drogon::Post);
    // 添加验证和登出接口
    ADD_METHOD_TO(AuthController::verifyToken, "/api/auth/verify", drogon::Get, "JwtAuthFilter");
    ADD_METHOD_TO(AuthController::logout, "/api/auth/logout", drogon::Post, "JwtAuthFilter");
    // 修改个人信息
    ADD_METHOD_TO(AuthController::updateProfile, "/api/auth/profile", drogon::Put, "JwtAuthFilter");
    // 修改密码
    ADD_METHOD_TO(AuthController::changePassword, "/api/auth/change-password", drogon::Post, "JwtAuthFilter");
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
    
    /**
     * 注册用户
     * @param req 请求对象
     * @param callback 回调函数
     */
    void registerUser(const drogon::HttpRequestPtr& req, 
                     std::function<void(const drogon::HttpResponsePtr&)>&& callback) const;
    
    /**
     * 验证token
     * @param req 请求对象
     * @param callback 回调函数
     */
    void verifyToken(const drogon::HttpRequestPtr& req, 
                    std::function<void(const drogon::HttpResponsePtr&)>&& callback) const;
                    
    /**
     * 用户登出
     * @param req 请求对象
     * @param callback 回调函数
     */
    void logout(const drogon::HttpRequestPtr& req, 
                std::function<void(const drogon::HttpResponsePtr&)>&& callback) const;
    
    /**
     * 更新用户个人信息
     * @param req 请求对象
     * @param callback 回调函数
     */
    void updateProfile(const drogon::HttpRequestPtr& req, 
                      std::function<void(const drogon::HttpResponsePtr&)>&& callback) const;
                      
    /**
     * 修改用户密码
     * @param req 请求对象
     * @param callback 回调函数
     */
    void changePassword(const drogon::HttpRequestPtr& req, 
                       std::function<void(const drogon::HttpResponsePtr&)>&& callback) const;

private:
    std::string m_jwtSecret;
    std::shared_ptr<DbManager> m_dbManager;
    std::shared_ptr<JwtManager> m_jwtManager;
    
    /**
     * 验证密码
     * @param password 明文密码
     * @param hashedPassword 哈希后的密码
     * @return 是否验证通过
     */
    bool verifyPassword(const std::string& password, const std::string& hashedPassword) const;
};
