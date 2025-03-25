#pragma once

#include <drogon/HttpResponse.h>
#include <json/json.h>
#include "ErrorCode.hpp"

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
        json["code"] = 200;
        json["message"] = message;
        if (!data.isNull())
        {
            json["data"] = data;
        }
        
        // 设置JSON编码选项
        Json::StreamWriterBuilder builder;
        builder["indentation"] = "";  // 不缩进
        builder["emitUTF8"] = true;   // 使用UTF-8编码
        std::string output = Json::writeString(builder, json);
        
        auto resp = drogon::HttpResponse::newHttpResponse();
        resp->setBody(output);
        resp->setContentTypeCode(drogon::CT_APPLICATION_JSON);
        resp->setStatusCode(drogon::k200OK);  // 统一使用200状态码
        return resp;
    }

    /**
     * 创建错误响应（使用错误码）
     * @param errorCode 错误码
     * @param customMessage 自定义错误消息（可选）
     * @return HTTP响应对象
     */
    inline drogon::HttpResponsePtr createErrorResponse(ErrorCode errorCode, 
                                                     const std::string& customMessage = "")
    {
        Json::Value json;
        json["status"] = "error";
        json["code"] = static_cast<int>(errorCode);
        json["message"] = customMessage.empty() ? getErrorMessage(errorCode) : customMessage;
        
        // 设置JSON编码选项
        Json::StreamWriterBuilder builder;
        builder["indentation"] = "";  // 不缩进
        builder["emitUTF8"] = true;   // 使用UTF-8编码
        std::string output = Json::writeString(builder, json);
        
        auto resp = drogon::HttpResponse::newHttpResponse();
        resp->setBody(output);
        resp->setContentTypeCode(drogon::CT_APPLICATION_JSON);
        resp->setStatusCode(drogon::k200OK);  // 统一使用200状态码
        return resp;
    }

    /**
     * 创建错误响应（使用自定义消息）
     * @param message 错误消息
     * @return HTTP响应对象
     */
    inline drogon::HttpResponsePtr createErrorResponse(const std::string& message)
    {
        Json::Value json;
        json["status"] = "error";
        json["code"] = static_cast<int>(ErrorCode::SYSTEM_ERROR);  // 使用系统错误作为默认错误码
        json["message"] = message;
        
        // 设置JSON编码选项
        Json::StreamWriterBuilder builder;
        builder["indentation"] = "";  // 不缩进
        builder["emitUTF8"] = true;   // 使用UTF-8编码
        std::string output = Json::writeString(builder, json);
        
        auto resp = drogon::HttpResponse::newHttpResponse();
        resp->setBody(output);
        resp->setContentTypeCode(drogon::CT_APPLICATION_JSON);
        resp->setStatusCode(drogon::k200OK);  // 统一使用200状态码
        return resp;
    }
}
