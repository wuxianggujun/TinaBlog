#include "HealthController.hpp"
#include "blog/utils/HttpUtils.hpp"
#include "blog/utils/ErrorCode.hpp"

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
            // 尝试执行简单查询验证数据库
            dbManager.executeQuery(
                "SELECT 1",
                [callback=std::move(callback)](const drogon::orm::Result& result) {
                    // 数据库连接正常
                    Json::Value data;
                    data["database"] = "connected";
                    auto resp = utils::createSuccessResponse("系统正常运行", data);
                    callback(resp);
                },
                [callback=std::move(callback)](const drogon::orm::DrogonDbException& e) {
                    // 数据库查询异常
                    auto resp = utils::createErrorResponse(
                        utils::ErrorCode::DB_QUERY_ERROR,
                        "数据库查询失败: " + std::string(e.base().what())
                    );
                    callback(resp);
                });
        } else {
            // 数据库连接异常
            auto resp = utils::createErrorResponse(
                utils::ErrorCode::DB_CONNECTION_ERROR,
                "数据库连接失败"
            );
            callback(resp);
        }
    } catch (const std::exception& e) {
        // 发生异常
        auto resp = utils::createErrorResponse(
            utils::ErrorCode::SYSTEM_ERROR,
            std::string("健康检查异常: ") + e.what()
        );
        callback(resp);
    }
} 