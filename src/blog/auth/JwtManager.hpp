#pragma once

// MSVC兼容性修复
#ifdef _MSC_VER
    #define NOMINMAX
    #define _SILENCE_CXX17_CODECVT_HEADER_DEPRECATION_WARNING
    #define _SILENCE_ALL_CXX17_DEPRECATION_WARNINGS
#endif

#include <string>
#include <chrono>
#include <exception>
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
     * 构造函数
     */
    JwtManager() {
        // 直接硬编码JWT配置
        m_secret = "wuxianggujun-tina-blog-3344207732";
        m_issuer = "tinablog";
        m_expireTime = 15 * 24 * 3600; // 15天
        
        LOG_INFO << "JwtManager初始化成功, 使用硬编码配置";
        LOG_INFO << "  - 密钥长度: " << m_secret.length();
        LOG_INFO << "  - 发行者: " << m_issuer;
        LOG_INFO << "  - 过期时间: " << m_expireTime << "秒";
    }

    /**
     * 生成JWT令牌
     */
    std::string generateToken(const std::string& userUuid, 
                            const std::string& username, 
                            bool isAdmin) const {
        auto now = std::chrono::system_clock::now();
        auto expireTime = now + std::chrono::seconds(m_expireTime);

        std::string isAdminStr = isAdmin ? "true" : "false";

        auto token = jwt::create()
            .set_issuer(m_issuer)
            .set_type("JWS")
            .set_issued_at(now)
            .set_expires_at(expireTime)
            .set_payload_claim("user_uuid", jwt::claim(userUuid))
            .set_payload_claim("username", jwt::claim(username))
            .set_payload_claim("is_admin", jwt::claim(isAdminStr))
            .sign(jwt::algorithm::hs256{m_secret});

        return token;
    }

    /**
     * 验证JWT令牌
     */
    bool verifyToken(const std::string& token, VerifyResult& result) const {
        try {
            LOG_INFO << "开始验证令牌";
            
            if (m_secret.empty()) {
                result.isValid = false;
                result.reason = "JWT密钥未配置";
                LOG_ERROR << "JWT密钥未配置";
                return false;
            }

            auto verifier = jwt::verify()
                .allow_algorithm(jwt::algorithm::hs256{m_secret})
                .with_issuer(m_issuer);

            auto decoded = jwt::decode(token);
            
            // 检查令牌是否过期
            auto now = std::chrono::system_clock::now();
            auto exp = decoded.get_expires_at();
            if (exp < now) {
                result.isValid = false;
                result.reason = "令牌已过期";
                LOG_WARN << "令牌已过期，过期时间: " << std::chrono::system_clock::to_time_t(exp);
                return false;
            }

            verifier.verify(decoded);

            result.isValid = true;
            result.userUuid = decoded.get_payload_claim("user_uuid").as_string();
            result.username = decoded.get_payload_claim("username").as_string();
            result.isAdmin = decoded.get_payload_claim("is_admin").as_string() == "true";
            result.reason = "验证成功";

            LOG_INFO << "令牌验证成功:";
            LOG_INFO << "  - 用户UUID: " << result.userUuid;
            LOG_INFO << "  - 用户名: " << result.username;
            LOG_INFO << "  - 是否管理员: " << (result.isAdmin ? "是" : "否");

            return true;
        } catch (const std::exception& e) {
            result.isValid = false;
            result.reason = std::string("验证失败: ") + e.what();
            LOG_ERROR << "验证失败: " << e.what();
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

private:
    // 存储JWT配置
    std::string m_secret;
    std::string m_issuer;
    int m_expireTime;
};
