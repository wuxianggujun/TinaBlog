#pragma once

#include <string>
#include <optional>
#include <jwt-cpp/jwt.h>
#include <nlohmann/json.hpp>

using json = nlohmann::json;

class JwtUtils {
public:
    static JwtUtils& getInstance();

    // 生成JWT Token
    std::string generateToken(const std::string& username, bool isAdmin = false);

    // 验证并解析Token
    std::optional<json> verifyToken(const std::string& token);

    // 设置密钥
    void setSecretKey(const std::string& key);

private:
    JwtUtils();
    ~JwtUtils() = default;
    JwtUtils(const JwtUtils&) = delete;
    JwtUtils& operator=(const JwtUtils&) = delete;

    std::string secretKey_;
    const int TOKEN_EXPIRE_HOURS = 24; // Token有效期24小时
}; 