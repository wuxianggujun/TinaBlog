#pragma once
#include <drogon/HttpController.h>
#include "blog/db/DbManager.hpp"

namespace blog {
namespace controllers {

/**
 * 健康检查控制器
 * 用于检查系统各组件状态
 */
class HealthController : public drogon::HttpController<HealthController>
{
public:
    METHOD_LIST_BEGIN
    // 健康检查接口
    ADD_METHOD_TO(HealthController::check, "/api/health", drogon::Get);
    METHOD_LIST_END
    
    /**
     * 健康检查
     * @param req 请求对象
     * @param callback 回调函数
     */
    void check(const drogon::HttpRequestPtr& req, 
               std::function<void(const drogon::HttpResponsePtr&)>&& callback) const;
};

} // namespace controllers
} // namespace blog 