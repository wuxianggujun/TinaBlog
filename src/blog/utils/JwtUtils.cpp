#include "JwtUtils.hpp"
#include <chrono>

JwtUtils& JwtUtils::getInstance() {
    static JwtUtils instance;
    return instance;
}

JwtUtils::JwtUtils() : secretKey_("your-256-bit-secret") {} // 默认密钥

void JwtUtils::setSecretKey(const std::string& key) {
    secretKey_ = key;
}

std::string JwtUtils::generateToken(const std::string& username, bool isAdmin) {
    auto now = std::chrono::system_clock::now();
    auto exp = now + std::chrono::hours(TOKEN_EXPIRE_HOURS);

    auto token = jwt::create()
        .set_issuer("blog_api")
        .set_type("JWS")
        .set_issued_at(now)
        .set_expires_at(exp)
        .set_payload_claim("username", jwt::claim(username));

    // 使用字符串来存储布尔值
    token.set_payload_claim("isAdmin", jwt::claim(std::to_string(isAdmin)));

    return token.sign(jwt::algorithm::hs256{secretKey_});
}

std::optional<json> JwtUtils::verifyToken(const std::string& token) {
    try {
        auto decoded = jwt::decode(token);
        auto verifier = jwt::verify()
            .allow_algorithm(jwt::algorithm::hs256{secretKey_})
            .with_issuer("blog_api");
            
        verifier.verify(decoded);
        
        json payload;
        payload["username"] = decoded.get_payload_claim("username").as_string();
        payload["isAdmin"] = decoded.get_payload_claim("isAdmin").as_string() == "1";
        
        return payload;
    } catch (const std::exception&) {
        return std::nullopt;
    }
} 