#pragma once

#include <drogon/drogon.h>
#include <json/json.h>

namespace utils
{
    inline drogon::HttpResponsePtr createSuccessResponse(
        const std::string& message = "操作成功", const Json::Value& data = Json::Value(),
        drogon::HttpStatusCode statusCode = drogon::k200OK)
    {
        Json::Value json;
        json["status"] = "success";
        json["message"] = message;

        if (!data.isNull())
        {
            json["data"] = data;
        }

        auto resp = drogon::HttpResponse::newHttpJsonResponse(json);
        resp->setStatusCode(statusCode);
        return resp;
    }


    inline drogon::HttpResponsePtr createErrorResponse(
        const std::string& message,
        drogon::HttpStatusCode statusCode = drogon::k400BadRequest, int errorCode = 0)
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
