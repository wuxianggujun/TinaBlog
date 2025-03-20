#pragma once

#include <string>
#include <nlohmann/json.hpp>

enum class ApiStatus {
    API_SUCCESS,
    API_ERROR,
    API_UNAUTHORIZED,
    API_NOT_FOUND,
    API_BAD_REQUEST
};

/**
 * API响应封装类
 * 用于统一API响应格式
 */
class ApiResponse {
public:
    // 创建成功响应
    static nlohmann::json success(const nlohmann::json& data = nullptr) {
        nlohmann::json response = {
            {"success", true},
            {"code", 200}
        };
        
        if (!data.is_null()) {
            response["data"] = data;
        }
        
        return response;
    }
    
    // 创建错误响应
    static nlohmann::json error(
        const std::string& message, 
        int code = 500, 
        const nlohmann::json& details = nullptr
    ) {
        nlohmann::json response = {
            {"success", false},
            {"code", code},
            {"message", message}
        };
        
        if (!details.is_null()) {
            response["details"] = details;
        }
        
        return response;
    }
    
    // 创建特定状态的响应
    static nlohmann::json status(
        ApiStatus status, 
        const std::string& message = "", 
        const nlohmann::json& data = nullptr
    ) {
        switch (status) {
            case ApiStatus::API_SUCCESS:
                return success(data);
                
            case ApiStatus::API_UNAUTHORIZED:
                return error(message.empty() ? "Unauthorized" : message, 401, data);
                
            case ApiStatus::API_NOT_FOUND:
                return error(message.empty() ? "Not found" : message, 404, data);
                
            case ApiStatus::API_BAD_REQUEST:
                return error(message.empty() ? "Bad request" : message, 400, data);
                
            case ApiStatus::API_ERROR:
            default:
                return error(message.empty() ? "Internal server error" : message, 500, data);
        }
    }
}; 