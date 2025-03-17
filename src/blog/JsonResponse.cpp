//
// Created by wuxianggujun on 2025/3/14.
//

#include "JsonResponse.hpp"
#include "NgxLog.hpp"

// 发送NgxString JSON响应
ngx_int_t JsonResponse::send(ngx_http_request_t* r, const NgxString& jsonContent, ngx_uint_t status) {
    // 使用NgxResponse来处理响应
    return NgxResponse(r)
        .status(status)
        .contentType("application/json")
        .enableCors()
        .send(jsonContent);
}

// 发送JSON字符串响应
ngx_int_t JsonResponse::send(ngx_http_request_t* r, const std::string& jsonContent, ngx_uint_t status) {
    // 使用NgxResponse来处理响应
    return NgxResponse(r)
        .status(status)
        .contentType("application/json")
        .enableCors()
        .send(jsonContent);
}

// 发送JSON对象响应
ngx_int_t JsonResponse::send(ngx_http_request_t* r, const nlohmann::json& jsonObj, ngx_uint_t status) {
    try {
        // 将json对象转换为字符串并调用原来的函数
        std::string jsonContent = jsonObj.dump(2); // 缩进2个空格
        return send(r, jsonContent, status);
    }
    catch (const std::exception& e) {
        NgxLog logger(r);
        logger.error("JSON序列化失败: %s", e.what());
        
        // 返回错误响应
        nlohmann::json errorResponse = {
            {"success", false},
            {"error", {
                {"code", 500},
                {"message", "服务器内部错误：JSON序列化失败"}
            }}
        };
        
        std::string errorContent = errorResponse.dump();
        return NgxResponse(r)
            .status(NGX_HTTP_INTERNAL_SERVER_ERROR)
            .contentType("application/json")
            .enableCors()
            .send(errorContent);
    }
}

// 创建成功响应
nlohmann::json JsonResponse::success(const nlohmann::json& data) {
    nlohmann::json response;
    response["success"] = true;
    
    if (!data.is_null()) {
        response["data"] = data;
    }
    
    return response;
}

// 创建错误响应
nlohmann::json JsonResponse::error(const std::string& message, int code) {
    nlohmann::json response;
    response["success"] = false;
    response["error"]["code"] = code;
    response["error"]["message"] = message;
    
    return response;
} 