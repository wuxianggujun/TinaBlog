#pragma once

// MSVC兼容性修复
#ifdef _MSC_VER
    #define NOMINMAX
    #define _SILENCE_CXX17_CODECVT_HEADER_DEPRECATION_WARNING
    #define _SILENCE_ALL_CXX17_DEPRECATION_WARNINGS
#endif

#include <string>
#include <chrono>
#include <jwt-cpp/jwt.h>
#include <drogon/drogon.h>
#include <json/json.h>

/**
 * JWT管理器类，用于处理JWT令牌的生成和验证
 */
class JwtManager {
public:
    /**
     * 验证结果结构体
     */
    struct VerifyResult {
        bool isValid;
        std::string userUuid;
        std::string username;
        bool isAdmin;
        std::string reason;
    };

    /**
     * 从请求中获取token
     */
    static std::string getTokenFromRequest(const drogon::HttpRequestPtr& req) {
        // 首先从Cookie中获取
        auto cookies = req->getCookies();
        auto it = cookies.find("token");
        if (it != cookies.end()) {
            return it->second;
        }

        // 然后从Authorization头中获取
        std::string authHeader = req->getHeader("Authorization");
        if (!authHeader.empty() && authHeader.substr(0, 7) == "Bearer ") {
            return authHeader.substr(7);
        }

        return "";
    }

    /**
     * 获取JWT配置
     */
    static Json::Value getJwtConfig() {
        auto& app = drogon::app();
        const auto& config = app.getCustomConfig();
        return config["jwt"];
    }

    /**
     * 获取JWT密钥
     */
    static std::string getJwtSecret() {
        const auto& jwtConfig = getJwtConfig();
        std::string secret = jwtConfig["secret"].asString();
        if (secret.empty()) {
            LOG_ERROR << "警告: JWT密钥未配置或为空，将使用默认密钥";
            secret = "wuxianggujun-tina-blog-3344207732";
        }
        return secret;
    }

    /**
     * 获取JWT发行者
     */
    static std::string getJwtIssuer() {
        const auto& jwtConfig = getJwtConfig();
        return jwtConfig["issuer"].asString();
    }

    /**
     * 获取JWT过期时间
     */
    static int getJwtExpireTime() {
        const auto& jwtConfig = getJwtConfig();
        return jwtConfig["expire_time"].asInt();
    }

    /**
     * 生成JWT令牌
     */
    std::string generateToken(const std::string& userUuid, 
                            const std::string& username, 
                            bool isAdmin) const {
        auto now = std::chrono::system_clock::now();
        auto expireTime = now + std::chrono::seconds(getJwtExpireTime());

        std::string isAdminStr = isAdmin ? "true" : "false";

        auto token = jwt::create()
            .set_issuer(getJwtIssuer())
            .set_type("JWS")
            .set_issued_at(now)
            .set_expires_at(expireTime)
            .set_payload_claim("user_uuid", jwt::claim(userUuid))
            .set_payload_claim("username", jwt::claim(username))
            .set_payload_claim("is_admin", jwt::claim(isAdminStr))
            .sign(jwt::algorithm::hs256{getJwtSecret()});

        return token;
    }

    /**
     * 验证JWT令牌
     */
    bool verifyToken(const std::string& token, VerifyResult& result) const {
        try {
            auto verifier = jwt::verify()
                .allow_algorithm(jwt::algorithm::hs256{getJwtSecret()})
                .with_issuer(getJwtIssuer());

            auto decoded = jwt::decode(token);
            verifier.verify(decoded);

            result.isValid = true;
            result.userUuid = decoded.get_payload_claim("user_uuid").as_string();
            result.username = decoded.get_payload_claim("username").as_string();
            result.isAdmin = decoded.get_payload_claim("is_admin").as_string() == "true";
            result.reason = "验证成功";

            return true;
        } catch (const std::exception& e) {
            result.isValid = false;
            result.reason = std::string("验证失败: ") + e.what();
            return false;
        }
    }

    /**
     * 从令牌中获取用户UUID
     * @param token JWT令牌
     * @return 用户UUID，如果令牌无效则返回空字符串
     */
    std::string getUserUuidFromToken(const std::string& token) {
        try {
            auto decoded = jwt::decode(token);
            return decoded.get_payload_claim("user_uuid").as_string();
        } catch (const std::exception& e) {
            return "";
        }
    }
    
    /**
     * 从令牌中获取用户名
     * @param token JWT令牌
     * @return 用户名，如果令牌无效则返回空字符串
     */
    std::string getUsernameFromToken(const std::string& token) {
        try {
            auto decoded = jwt::decode(token);
            return decoded.get_payload_claim("username").as_string();
        } catch (const std::exception& e) {
            return "";
        }
    }
    
    /**
     * 检查令牌中的用户是否管理员
     * @param token JWT令牌
     * @return 是否管理员，如果令牌无效则返回false
     */
    bool isAdminFromToken(const std::string& token) {
        try {
            auto decoded = jwt::decode(token);
            return decoded.get_payload_claim("is_admin").as_string() == "true";
        } catch (const std::exception& e) {
            return false;
        }
    }
};
