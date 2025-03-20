#pragma once

#include "../Nginx.hpp"
#include <string>
#include <unordered_map>
#include <optional>
#include <memory>
#include <vector>
#include <chrono>

/**
 * @brief JWT服务类，负责生成和验证JWT令牌
 * 
 * 此类使用单例模式，确保在整个应用中只有一个实例
 */
class JwtService {
public:
    /**
     * @brief 获取JwtService单例
     * @return JwtService单例引用
     */
    static JwtService& getInstance();

    /**
     * @brief 初始化服务
     * @param secret_key JWT签名密钥
     * @param token_expire_time 令牌过期时间（秒）
     * @return 是否初始化成功
     */
    bool init(const std::string& secret_key, long token_expire_time = 86400); // 默认24小时过期

    /**
     * @brief 生成JWT令牌
     * @param user_id 用户ID
     * @param username 用户名
     * @param additional_claims 额外的声明数据
     * @return 成功则返回JWT令牌，失败则返回空字符串
     */
    std::string generate(int user_id, 
                       const std::string& username,
                       const std::unordered_map<std::string, std::string>& additional_claims = {});

    /**
     * @brief 验证JWT令牌
     * @param token JWT令牌
     * @return 令牌是否有效
     */
    bool verify(const std::string& token);

    /**
     * @brief 从JWT令牌中提取用户ID
     * @param token JWT令牌
     * @return 成功则返回用户ID，失败则返回std::nullopt
     */
    std::optional<int> getUserId(const std::string& token);

    /**
     * @brief 从JWT令牌中提取用户名
     * @param token JWT令牌
     * @return 成功则返回用户名，失败则返回std::nullopt
     */
    std::optional<std::string> getUsername(const std::string& token);

    /**
     * @brief 从JWT令牌中提取声明值
     * @param token JWT令牌
     * @param claim_name 声明名称
     * @return 成功则返回声明值，失败则返回std::nullopt
     */
    std::optional<std::string> getClaim(const std::string& token, const std::string& claim_name);

private:
    // 单例模式，禁止外部创建实例
    JwtService() = default;
    ~JwtService() = default;
    JwtService(const JwtService&) = delete;
    JwtService& operator=(const JwtService&) = delete;

    // JWT头部编码
    std::string encodeHeader();
    
    // JWT负载编码
    std::string encodePayload(int user_id, 
                            const std::string& username,
                            const std::unordered_map<std::string, std::string>& additional_claims);
    
    // 生成签名
    std::string generateSignature(const std::string& header_payload);
    
    // Base64 URL编码
    std::string base64UrlEncode(const std::vector<unsigned char>& input);
    
    // Base64 URL解码
    std::vector<unsigned char> base64UrlDecode(const std::string& input);
    
    // 解析JWT令牌
    bool parseToken(const std::string& token, 
                  std::string& header,
                  std::string& payload,
                  std::string& signature);
    
    // 解析JWT负载
    std::unordered_map<std::string, std::string> parsePayload(const std::string& payload);

    // 成员变量
    std::string secret_key_;                 // 签名密钥
    long token_expire_time_ = 86400;         // 令牌过期时间（秒）
    bool initialized_ = false;               // 是否已初始化
}; 