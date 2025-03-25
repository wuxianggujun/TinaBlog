#pragma once

#include <drogon/HttpResponse.h>
#include <json/json.h>

namespace utils
{
    /**
     * 创建成功响应
     * @param message 成功消息
     * @param data 响应数据
     * @return HTTP响应对象
     */
    inline drogon::HttpResponsePtr createSuccessResponse(const std::string& message, const Json::Value& data = Json::Value())
    {
        Json::Value json;
        json["status"] = "success";
        json["message"] = message;
        if (!data.isNull())
        {
            json["data"] = data;
        }
        return drogon::HttpResponse::newHttpJsonResponse(json);
    }

    /**
     * 创建错误响应
     * @param message 错误消息
     * @param statusCode HTTP状态码
     * @param errorCode 错误代码（可选）
     * @return HTTP响应对象
     */
    inline drogon::HttpResponsePtr createErrorResponse(const std::string& message, 
                                                     drogon::HttpStatusCode statusCode,
                                                     int errorCode = 0)
    {
        Json::Value json;
        json["status"] = "error";
        json["message"] = message;
        if (errorCode != 0)
        {
            json["code"] = errorCode;
        }
        auto resp = drogon::HttpResponse::newHttpJsonResponse(json);
        resp->setStatusCode(statusCode);
        return resp;
    }
}
