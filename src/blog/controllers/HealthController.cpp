#include "HealthController.hpp"


/**
 * 健康检查
 */
void HealthController::check(const drogon::HttpRequestPtr& req, 
                           std::function<void(const drogon::HttpResponsePtr&)>&& callback) const {
    Json::Value json;
    
    try {
        // 获取数据库连接
        auto& dbManager = DbManager::getInstance();
        
        // 验证数据库连接
        if (dbManager.isConnected()) {
            // 数据库连接正常
            json["status"] = "ok";
            json["database"] = "connected";
            auto resp = drogon::HttpResponse::newHttpJsonResponse(json);
            resp->setStatusCode(drogon::k200OK);
            callback(resp);
        } else {
            // 数据库连接异常
            json["status"] = "error";
            json["database"] = "disconnected";
            auto resp = drogon::HttpResponse::newHttpJsonResponse(json);
            resp->setStatusCode(drogon::k503ServiceUnavailable);
            callback(resp);
        }
    } catch (const std::exception& e) {
        // 发生异常
        json["status"] = "error";
        json["message"] = e.what();
        auto resp = drogon::HttpResponse::newHttpJsonResponse(json);
        resp->setStatusCode(drogon::k500InternalServerError);
        callback(resp);
    }
} 