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

namespace blog {
namespace auth {

/**
 * JWT令牌管理器
 * 负责生成和验证JWT令牌
 */
class JwtManager {
public:
    /**
     * 构造函数
     * @param secret JWT签名密钥
     * @param issuer JWT发行者
     * @param tokenExpireTime 令牌过期时间(秒)
     */
    JwtManager(const std::string& secret, 
               const std::string& issuer = "tinablog", 
               int tokenExpireTime = 3600) 
        : m_secret(secret)
        , m_issuer(issuer)
        , m_tokenExpireTime(tokenExpireTime) 
    {
    }

    /**
     * 生成JWT令牌
     * @param userId 用户ID
     * @param username 用户名
     * @param isAdmin 是否管理员
     * @return JWT令牌
     */
    std::string generateToken(int userId, const std::string& username, bool isAdmin = false) {
        auto now = std::chrono::system_clock::now();
        auto expireTime = now + std::chrono::seconds(m_tokenExpireTime);
        
        // 将userId转为字符串
        std::string userIdStr = std::to_string(userId);
        
        // 将isAdmin转为字符串
        std::string isAdminStr = isAdmin ? "true" : "false";
        
        auto token = jwt::create()
            .set_issuer(m_issuer)
            .set_type("JWT")
            .set_issued_at(now)
            .set_expires_at(expireTime)
            .set_payload_claim("user_id", jwt::claim(userIdStr))
            .set_payload_claim("username", jwt::claim(username))
            .set_payload_claim("is_admin", jwt::claim(isAdminStr))
            .sign(jwt::algorithm::hs256{m_secret});
            
        return token;
    }
    
    /**
     * 验证JWT令牌
     * @param token JWT令牌
     * @return 验证是否成功
     */
    bool verifyToken(const std::string& token) {
        try {
            auto decoded = jwt::decode(token);
            auto verifier = jwt::verify()
                .allow_algorithm(jwt::algorithm::hs256{m_secret})
                .with_issuer(m_issuer);
                
            verifier.verify(decoded);
            return true;
        } catch (const std::exception& e) {
            return false;
        }
    }
    
    /**
     * 从请求中获取JWT令牌
     * @param req HTTP请求
     * @return JWT令牌，如果没有找到则返回空字符串
     */
    static std::string getTokenFromRequest(const drogon::HttpRequestPtr& req) {
        // 从Authorization头获取
        std::string authHeader = req->getHeader("Authorization");
        if (!authHeader.empty() && authHeader.substr(0, 7) == "Bearer ") {
            return authHeader.substr(7);
        }
        
        // 从Cookie获取
        std::string tokenCookie = req->getCookie("token");
        if (!tokenCookie.empty()) {
            return tokenCookie;
        }
        
        // 从查询参数获取
        std::string tokenParam = req->getParameter("token");
        if (!tokenParam.empty()) {
            return tokenParam;
        }
        
        return "";
    }
    
    /**
     * 从令牌中获取用户ID
     * @param token JWT令牌
     * @return 用户ID，如果令牌无效则返回-1
     */
    int getUserIdFromToken(const std::string& token) {
        try {
            auto decoded = jwt::decode(token);
            std::string userId = decoded.get_payload_claim("user_id").as_string();
            return std::stoi(userId);
        } catch (const std::exception& e) {
            return -1;
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
            std::string isAdmin = decoded.get_payload_claim("is_admin").as_string();
            return isAdmin == "true";
        } catch (const std::exception& e) {
            return false;
        }
    }

private:
    std::string m_secret;
    std::string m_issuer;
    int m_tokenExpireTime;
};

} // namespace auth
} // namespace blog 