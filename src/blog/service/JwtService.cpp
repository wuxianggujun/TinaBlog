#include "JwtService.hpp"
#include <ngx_core.h>
#include <nlohmann/json.hpp>
#include <openssl/hmac.h>
#include <openssl/evp.h>
#include <sstream>
#include <iomanip>
#include <ctime>
#include <algorithm>

using json = nlohmann::json;

// 获取单例实例
JwtService& JwtService::getInstance() {
    static JwtService instance;
    return instance;
}

// 初始化服务
bool JwtService::init(const std::string& secret_key, long token_expire_time) {
    if (secret_key.empty()) {
        ngx_log_error(NGX_LOG_ERR, ngx_cycle->log, 0, "JwtService: Empty secret key provided");
        return false;
    }
    
    secret_key_ = secret_key;
    token_expire_time_ = token_expire_time;
    initialized_ = true;
    
    ngx_log_error(NGX_LOG_INFO, ngx_cycle->log, 0, 
                "JwtService initialized with expiration time: %ld seconds", token_expire_time_);
    
    return true;
}

// 生成JWT令牌
std::string JwtService::generate(int user_id, 
                              const std::string& username,
                              const std::unordered_map<std::string, std::string>& additional_claims) 
{
    if (!initialized_) {
        ngx_log_error(NGX_LOG_ERR, ngx_cycle->log, 0, "JwtService: Not initialized");
        return "";
    }
    
    try {
        // 1. 创建头部和负载
        std::string header = encodeHeader();
        std::string payload = encodePayload(user_id, username, additional_claims);
        
        // 2. 编码头部和负载
        std::string headerPayload = header + "." + payload;
        
        // 3. 生成签名
        std::string signature = generateSignature(headerPayload);
        
        // 4. 组合JWT令牌
        std::string token = headerPayload + "." + signature;
        
        return token;
    } catch (const std::exception& e) {
        ngx_log_error(NGX_LOG_ERR, ngx_cycle->log, 0, 
                    "JwtService: Error generating token: %s", e.what());
        return "";
    }
}

// 验证JWT令牌
bool JwtService::verify(const std::string& token) {
    if (!initialized_) {
        ngx_log_error(NGX_LOG_ERR, ngx_cycle->log, 0, "JwtService: Not initialized");
        return false;
    }
    
    try {
        // 1. 解析令牌
        std::string header, payload, signature;
        if (!parseToken(token, header, payload, signature)) {
            ngx_log_error(NGX_LOG_ERR, ngx_cycle->log, 0, "JwtService: Invalid token format");
            return false;
        }
        
        // 2. 验证签名
        std::string headerPayload = header + "." + payload;
        std::string expectedSignature = generateSignature(headerPayload);
        
        if (signature != expectedSignature) {
            ngx_log_error(NGX_LOG_ERR, ngx_cycle->log, 0, "JwtService: Invalid signature");
            return false;
        }
        
        // 3. 解析负载
        auto claims = parsePayload(payload);
        
        // 4. 检查过期时间
        auto exp_it = claims.find("exp");
        if (exp_it == claims.end()) {
            ngx_log_error(NGX_LOG_ERR, ngx_cycle->log, 0, "JwtService: Token missing expiration time");
            return false;
        }
        
        long exp_time = std::stol(exp_it->second);
        long current_time = std::time(nullptr);
        
        if (current_time > exp_time) {
            ngx_log_error(NGX_LOG_ERR, ngx_cycle->log, 0, 
                        "JwtService: Token expired (exp: %ld, now: %ld)", exp_time, current_time);
            return false;
        }
        
        return true;
    } catch (const std::exception& e) {
        ngx_log_error(NGX_LOG_ERR, ngx_cycle->log, 0, 
                    "JwtService: Error verifying token: %s", e.what());
        return false;
    }
}

// 从JWT令牌中提取用户ID
std::optional<int> JwtService::getUserId(const std::string& token) {
    try {
        // 解析令牌
        std::string header, payload, signature;
        if (!parseToken(token, header, payload, signature)) {
            return std::nullopt;
        }
        
        // 解析负载
        auto claims = parsePayload(payload);
        
        // 查找用户ID
        auto uid_it = claims.find("user_id");
        if (uid_it == claims.end()) {
            return std::nullopt;
        }
        
        return std::stoi(uid_it->second);
    } catch (...) {
        return std::nullopt;
    }
}

// 从JWT令牌中提取用户名
std::optional<std::string> JwtService::getUsername(const std::string& token) {
    try {
        // 解析令牌
        std::string header, payload, signature;
        if (!parseToken(token, header, payload, signature)) {
            return std::nullopt;
        }
        
        // 解析负载
        auto claims = parsePayload(payload);
        
        // 查找用户名
        auto username_it = claims.find("username");
        if (username_it == claims.end()) {
            return std::nullopt;
        }
        
        return username_it->second;
    } catch (...) {
        return std::nullopt;
    }
}

// 从JWT令牌中提取声明值
std::optional<std::string> JwtService::getClaim(const std::string& token, const std::string& claim_name) {
    try {
        // 解析令牌
        std::string header, payload, signature;
        if (!parseToken(token, header, payload, signature)) {
            return std::nullopt;
        }
        
        // 解析负载
        auto claims = parsePayload(payload);
        
        // 查找声明
        auto claim_it = claims.find(claim_name);
        if (claim_it == claims.end()) {
            return std::nullopt;
        }
        
        return claim_it->second;
    } catch (...) {
        return std::nullopt;
    }
}

// 私有方法

// JWT头部编码
std::string JwtService::encodeHeader() {
    json header = {
        {"alg", "HS256"},
        {"typ", "JWT"}
    };
    
    std::string header_str = header.dump();
    std::vector<unsigned char> header_bytes(header_str.begin(), header_str.end());
    return base64UrlEncode(header_bytes);
}

// JWT负载编码
std::string JwtService::encodePayload(int user_id, 
                                   const std::string& username,
                                   const std::unordered_map<std::string, std::string>& additional_claims)
{
    // 当前时间
    long iat = std::time(nullptr);
    // 过期时间
    long exp = iat + token_expire_time_;
    
    // 创建基本负载
    json payload = {
        {"user_id", user_id},
        {"username", username},
        {"iat", iat},
        {"exp", exp}
    };
    
    // 添加额外声明
    for (const auto& claim : additional_claims) {
        payload[claim.first] = claim.second;
    }
    
    std::string payload_str = payload.dump();
    std::vector<unsigned char> payload_bytes(payload_str.begin(), payload_str.end());
    return base64UrlEncode(payload_bytes);
}

// 生成签名
std::string JwtService::generateSignature(const std::string& header_payload) {
    // 使用HMAC-SHA256算法生成签名
    unsigned int len = EVP_MAX_MD_SIZE;
    unsigned char result[EVP_MAX_MD_SIZE];
    
    HMAC(EVP_sha256(), 
        secret_key_.c_str(), secret_key_.length(),
        reinterpret_cast<const unsigned char*>(header_payload.c_str()), header_payload.length(),
        result, &len);
    
    std::vector<unsigned char> signature_bytes(result, result + len);
    return base64UrlEncode(signature_bytes);
}

// Base64 URL编码
std::string JwtService::base64UrlEncode(const std::vector<unsigned char>& input) {
    static const char* base64_chars = 
        "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    
    std::string encoded;
    int i = 0;
    int j = 0;
    unsigned char char_array_3[3];
    unsigned char char_array_4[4];
    
    for (unsigned char c : input) {
        char_array_3[i++] = c;
        if (i == 3) {
            char_array_4[0] = (char_array_3[0] & 0xfc) >> 2;
            char_array_4[1] = ((char_array_3[0] & 0x03) << 4) + ((char_array_3[1] & 0xf0) >> 4);
            char_array_4[2] = ((char_array_3[1] & 0x0f) << 2) + ((char_array_3[2] & 0xc0) >> 6);
            char_array_4[3] = char_array_3[2] & 0x3f;
            
            for (i = 0; i < 4; i++) {
                encoded += base64_chars[char_array_4[i]];
            }
            i = 0;
        }
    }
    
    if (i) {
        for (j = i; j < 3; j++) {
            char_array_3[j] = '\0';
        }
        
        char_array_4[0] = (char_array_3[0] & 0xfc) >> 2;
        char_array_4[1] = ((char_array_3[0] & 0x03) << 4) + ((char_array_3[1] & 0xf0) >> 4);
        char_array_4[2] = ((char_array_3[1] & 0x0f) << 2) + ((char_array_3[2] & 0xc0) >> 6);
        
        for (j = 0; j < i + 1; j++) {
            encoded += base64_chars[char_array_4[j]];
        }
        
        while (i++ < 3) {
            encoded += '=';
        }
    }
    
    // Base64 URL编码：替换字符，移除填充
    std::string url_encoded = encoded;
    std::replace(url_encoded.begin(), url_encoded.end(), '+', '-');
    std::replace(url_encoded.begin(), url_encoded.end(), '/', '_');
    url_encoded.erase(std::remove(url_encoded.begin(), url_encoded.end(), '='), url_encoded.end());
    
    return url_encoded;
}

// Base64 URL解码
std::vector<unsigned char> JwtService::base64UrlDecode(const std::string& input) {
    static const std::string base64_chars = 
        "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    
    // Base64 URL解码：替换字符，添加填充
    std::string base64_str = input;
    std::replace(base64_str.begin(), base64_str.end(), '-', '+');
    std::replace(base64_str.begin(), base64_str.end(), '_', '/');
    
    // 添加填充
    switch (base64_str.length() % 4) {
        case 0: break;
        case 2: base64_str += "=="; break;
        case 3: base64_str += "="; break;
        default: throw std::runtime_error("Illegal base64url string");
    }
    
    std::vector<unsigned char> decoded;
    int i = 0;
    int j = 0;
    int in_ = 0;
    unsigned char char_array_4[4], char_array_3[3];
    
    for (char c : base64_str) {
        if (c == '=') break;
        
        auto pos = base64_chars.find(c);
        if (pos == std::string::npos) continue;
        
        char_array_4[i++] = pos;
        if (i == 4) {
            char_array_3[0] = (char_array_4[0] << 2) + ((char_array_4[1] & 0x30) >> 4);
            char_array_3[1] = ((char_array_4[1] & 0xf) << 4) + ((char_array_4[2] & 0x3c) >> 2);
            char_array_3[2] = ((char_array_4[2] & 0x3) << 6) + char_array_4[3];
            
            for (i = 0; i < 3; i++) {
                decoded.push_back(char_array_3[i]);
            }
            i = 0;
        }
    }
    
    if (i) {
        for (j = 0; j < i; j++) {
            char_array_4[j] = base64_chars.find(char_array_4[j]);
        }
        
        char_array_3[0] = (char_array_4[0] << 2) + ((char_array_4[1] & 0x30) >> 4);
        char_array_3[1] = ((char_array_4[1] & 0xf) << 4) + ((char_array_4[2] & 0x3c) >> 2);
        
        for (j = 0; j < i - 1; j++) {
            decoded.push_back(char_array_3[j]);
        }
    }
    
    return decoded;
}

// 解析JWT令牌
bool JwtService::parseToken(const std::string& token, 
                          std::string& header,
                          std::string& payload,
                          std::string& signature) 
{
    // 按点分割令牌
    std::vector<std::string> parts;
    std::stringstream ss(token);
    std::string part;
    
    while (std::getline(ss, part, '.')) {
        parts.push_back(part);
    }
    
    // JWT应由三部分组成
    if (parts.size() != 3) {
        return false;
    }
    
    header = parts[0];
    payload = parts[1];
    signature = parts[2];
    
    return true;
}

// 解析JWT负载
std::unordered_map<std::string, std::string> JwtService::parsePayload(const std::string& payload) {
    // 解码Base64URL编码的负载
    auto payload_bytes = base64UrlDecode(payload);
    std::string payload_str(payload_bytes.begin(), payload_bytes.end());
    
    // 解析JSON
    json payload_json = json::parse(payload_str);
    
    // 转换为map
    std::unordered_map<std::string, std::string> claims;
    for (auto& el : payload_json.items()) {
        // 处理不同类型
        if (el.value().is_string()) {
            claims[el.key()] = el.value().get<std::string>();
        } else if (el.value().is_number_integer()) {
            claims[el.key()] = std::to_string(el.value().get<int>());
        } else if (el.value().is_number_unsigned()) {
            claims[el.key()] = std::to_string(el.value().get<unsigned int>());
        } else if (el.value().is_number_float()) {
            claims[el.key()] = std::to_string(el.value().get<double>());
        } else if (el.value().is_boolean()) {
            claims[el.key()] = el.value().get<bool>() ? "true" : "false";
        } else {
            // 其他类型转为字符串
            claims[el.key()] = el.value().dump();
        }
    }
    
    return claims;
} 