#pragma once

#include <drogon/HttpController.h>
#include <functional>

namespace api {
namespace v1 {

class AdminController : public drogon::HttpController<AdminController> {
public:
    METHOD_LIST_BEGIN
    ADD_METHOD_TO(AdminController::getUsers, "/admin/users", drogon::Get);
    ADD_METHOD_TO(AdminController::banUser, "/admin/users/{uuid}/ban", drogon::Post);
    ADD_METHOD_TO(AdminController::unbanUser, "/admin/users/{uuid}/unban", drogon::Post);
    ADD_METHOD_TO(AdminController::deleteUser, "/admin/users/{uuid}", drogon::Delete);
    METHOD_LIST_END

    // 获取所有用户
    void getUsers(const drogon::HttpRequestPtr& req, 
                 std::function<void(const drogon::HttpResponsePtr&)>&& callback) const;

    // 封禁用户
    void banUser(const drogon::HttpRequestPtr& req, 
               std::function<void(const drogon::HttpResponsePtr&)>&& callback,
               const std::string& uuid) const;

    // 解除封禁
    void unbanUser(const drogon::HttpRequestPtr& req, 
                 std::function<void(const drogon::HttpResponsePtr&)>&& callback,
                 const std::string& uuid) const;

    // 删除用户
    void deleteUser(const drogon::HttpRequestPtr& req, 
                  std::function<void(const drogon::HttpResponsePtr&)>&& callback,
                  const std::string& uuid) const;

private:
    // 验证请求中的管理员权限
    bool validateAdmin(const drogon::HttpRequestPtr& req, 
                      std::function<void(const drogon::HttpResponsePtr&)>& callback) const;
};

} // namespace v1
} // namespace api 