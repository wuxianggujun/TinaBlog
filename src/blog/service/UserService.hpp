#pragma once

#include "../db/DbManager.hpp"
#include "../utils/JwtUtils.hpp"
#include <string>
#include <optional>
#include <nlohmann/json.hpp>

using json = nlohmann::json;

class UserService {
public:
    static UserService& getInstance();

    // 用户登录
    std::optional<std::string> login(const std::string& username, const std::string& password);

    // 验证用户密码
    bool verifyPassword(const std::string& username, const std::string& password);

    // 创建用户
    bool createUser(const std::string& username, const std::string& password,
                   const std::string& displayName, const std::string& email);

    // 更新用户信息
    bool updateUser(int userId, const json& userInfo);

    // 获取用户信息
    std::optional<json> getUserInfo(const std::string& username);

    // 删除用户
    bool deleteUser(int userId);

private:
    UserService() = default;
    ~UserService() = default;
    UserService(const UserService&) = delete;
    UserService& operator=(const UserService&) = delete;

    // 生成密码哈希
    std::string hashPassword(const std::string& password);
}; 