#pragma once

#include <drogon/HttpResponse.h>
#include <drogon/HttpRequest.h>
#include <json/json.h>
#include "ErrorCode.hpp"

namespace utils
{
    /**
     * 创建成功响应
     * @param message 成功消息
     * @param data 响应数据
     * @return HTTP响应
     */
    inline drogon::HttpResponsePtr createSuccessResponse(const std::string& message, const Json::Value& data = Json::Value())
    {
        Json::Value response;
        response["status"] = "success";
        response["code"] = static_cast<int>(ErrorCode::SUCCESS);
        response["message"] = message;
        if (!data.isNull())
        {
            response["data"] = data;
        }
        
        // 使用 StreamWriterBuilder 确保中文字符不被转义
        Json::StreamWriterBuilder builder;
        builder["indentation"] = "";  // 不添加缩进
        builder["emitUTF8"] = true;   // 使用UTF-8编码
        std::string jsonStr = Json::writeString(builder, response);
        
        auto resp = drogon::HttpResponse::newHttpResponse();
        resp->setContentTypeCode(drogon::CT_APPLICATION_JSON);
        resp->addHeader("Content-Type", "application/json; charset=utf-8");
        resp->setBody(jsonStr);
        return resp;
    }

    /**
     * 创建错误响应
     * @param code 错误码
     * @param customMessage 自定义错误消息（可选）
     * @return HTTP响应
     */
    inline drogon::HttpResponsePtr createErrorResponse(ErrorCode code, const std::string& customMessage = "")
    {
        Json::Value response;
        response["status"] = "error";
        response["code"] = static_cast<int>(code);
        response["message"] = customMessage.empty() ? getErrorMessage(code) : customMessage;
        
        // 使用 StreamWriterBuilder 确保中文字符不被转义
        Json::StreamWriterBuilder builder;
        builder["indentation"] = "";  // 不添加缩进
        builder["emitUTF8"] = true;   // 使用UTF-8编码
        std::string jsonStr = Json::writeString(builder, response);
        
        auto resp = drogon::HttpResponse::newHttpResponse();
        resp->setContentTypeCode(drogon::CT_APPLICATION_JSON);
        resp->addHeader("Content-Type", "application/json; charset=utf-8");
        resp->setBody(jsonStr);
        return resp;
    }
}
