#pragma once

#include "../NgxRequest.hpp"
#include "../BlogRoute.hpp"
#include "../db/DbManager.hpp"
#include <nlohmann/json.hpp>
#include <string>

using json = nlohmann::json;

/**
 * @brief 健康检查控制器类，负责提供系统健康状态信息
 */
class HealthController {
public:
    /**
     * @brief 获取HealthController单例
     * @return HealthController单例引用
     */
    static HealthController& getInstance();
    
    /**
     * @brief 注册路由
     * @param router BlogRoute实例引用
     */
    void registerRoutes(BlogRoute& router);
    
private:
    // 单例模式
    HealthController() = default;
    ~HealthController() = default;
    HealthController(const HealthController&) = delete;
    HealthController& operator=(const HealthController&) = delete;
    
    // API处理方法
    
    /**
     * @brief 处理健康检查请求
     * @param req NgxRequest对象
     * @param params 路由参数
     * @return ngx_int_t Nginx状态码
     */
    ngx_int_t handleHealthCheck(NgxRequest& req, const NgxRouteParams& params);
}; 