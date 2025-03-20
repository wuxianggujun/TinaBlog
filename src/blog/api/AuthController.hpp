#pragma once

#include "../NgxRequest.hpp"
#include "../BlogRoute.hpp"
#include "../service/UserService.hpp"
#include <nlohmann/json.hpp>
#include <string>

using json = nlohmann::json;

/**
 * @brief 认证控制器类，处理登录、注册等认证相关API
 */
class AuthController {
public:
    /**
     * @brief 获取AuthController单例
     * @return AuthController单例引用
     */
    static AuthController& getInstance();
    
    /**
     * @brief 注册路由
     * @param router BlogRoute实例引用
     */
    void registerRoutes(BlogRoute& router);
    
private:
    // 单例模式
    AuthController() = default;
    ~AuthController() = default;
    AuthController(const AuthController&) = delete;
    AuthController& operator=(const AuthController&) = delete;
    
    // API处理方法
    
    /**
     * @brief 处理登录请求
     * @param req NgxRequest对象
     * @param params 路由参数
     * @return ngx_int_t Nginx状态码
     */
    ngx_int_t handleLogin(NgxRequest& req, const NgxRouteParams& params);
    
    /**
     * @brief 处理注册请求
     * @param req NgxRequest对象
     * @param params 路由参数
     * @return ngx_int_t Nginx状态码
     */
    ngx_int_t handleRegister(NgxRequest& req, const NgxRouteParams& params);
    
    /**
     * @brief 处理注销请求
     * @param req NgxRequest对象
     * @param params 路由参数
     * @return ngx_int_t Nginx状态码
     */
    ngx_int_t handleLogout(NgxRequest& req, const NgxRouteParams& params);
    
    /**
     * @brief 处理获取当前用户信息请求
     * @param req NgxRequest对象
     * @param params 路由参数
     * @return ngx_int_t Nginx状态码
     */
    ngx_int_t handleGetUserInfo(NgxRequest& req, const NgxRouteParams& params);
    
    /**
     * @brief 处理更新用户信息请求
     * @param req NgxRequest对象
     * @param params 路由参数
     * @return ngx_int_t Nginx状态码
     */
    ngx_int_t handleUpdateUserInfo(NgxRequest& req, const NgxRouteParams& params);
}; 