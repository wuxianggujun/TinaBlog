#include "HealthController.hpp"
#include "blog/utils/HttpUtils.hpp"

/**
 * 健康检查
 */
void HealthController::check(const drogon::HttpRequestPtr& req, 
                           std::function<void(const drogon::HttpResponsePtr&)>&& callback) const {
    try {
        // 获取数据库连接
        auto& dbManager = DbManager::getInstance();
        
        // 验证数据库连接
        if (dbManager.isConnected()) {
            // 数据库连接正常
            Json::Value data;
            data["database"] = "connected";
            auto resp = utils::createSuccessResponse("系统正常运行", data);
            callback(resp);
        } else {
            // 数据库连接异常
            auto resp = utils::createErrorResponse("数据库连接失败", drogon::k503ServiceUnavailable);
            callback(resp);
        }
    } catch (const std::exception& e) {
        // 发生异常
        auto resp = utils::createErrorResponse(std::string("健康检查异常: ") + e.what(), 
                                               drogon::k500InternalServerError);
        callback(resp);
    }
} 