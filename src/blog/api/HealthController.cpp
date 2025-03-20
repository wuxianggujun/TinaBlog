#include "HealthController.hpp"
#include "../NgxLog.hpp"
#include <ctime>
#include <stdexcept>

// 获取单例实例
HealthController& HealthController::getInstance() {
    static HealthController instance;
    return instance;
}

// 注册路由
void HealthController::registerRoutes(BlogRoute& router) {
    // 健康检查API
    router.register_route("/api/health", HttpMethod::GET_METHOD, 
        [this](NgxRequest& req, const NgxRouteParams& params) -> ngx_int_t {
            return this->handleHealthCheck(req, params);
        });
}

// 处理健康检查请求
ngx_int_t HealthController::handleHealthCheck(NgxRequest& req, const NgxRouteParams& params) {
    try {
        // 检查数据库连接
        bool dbConnected = DbManager::getInstance().isConnected();
        
        // 获取当前系统负载信息（在实际系统中可能需要从OS获取）
        double systemLoad = 0.0; // 简化示例
        
        // 获取当前内存使用情况（在实际系统中可能需要从OS获取）
        size_t memoryUsage = 0; // 简化示例
        
        // 构建响应
        json response = {
            {"status", dbConnected ? "healthy" : "unhealthy"},
            {"components", {
                {"database", {
                    {"status", dbConnected ? "connected" : "disconnected"},
                    {"message", dbConnected ? "Database connection is healthy" : "Database connection failed"}
                }},
                {"server", {
                    {"status", "running"},
                    {"uptime", 0}, // 在实际系统中应获取真实的启动时间
                    {"load", systemLoad},
                    {"memory_usage", memoryUsage}
                }}
            }},
            {"timestamp", std::time(nullptr)}
        };
        
        return req.send_json(response.dump());
    }
    catch (const std::exception& e) {
        req.get_log().error("Health check error: %s", e.what());
        return req.send_error(NGX_HTTP_INTERNAL_SERVER_ERROR, "Internal server error");
    }
} 