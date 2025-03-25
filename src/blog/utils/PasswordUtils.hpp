#pragma once

#include <string>
#include <sodium.h>

namespace utils {

/**
 * 密码工具类
 * 基于libsodium库提供安全的密码哈希功能
 */
class PasswordUtils {
public:
    /**
     * 初始化libsodium库
     * 在使用任何密码函数前必须调用
     * 
     * @return 是否成功初始化
     */
    static bool initialize();

    /**
     * 生成密码哈希
     * 使用Argon2id算法（libsodium默认推荐）
     * 
     * @param password 明文密码
     * @return 哈希后的密码字符串，失败返回空字符串
     */
    static std::string hashPassword(const std::string& password);

    /**
     * 验证密码
     * 
     * @param password 用户输入的明文密码
     * @param hashedPassword 存储的哈希密码
     * @return 密码是否匹配
     */
    static bool verifyPassword(const std::string& password, const std::string& hashedPassword);

private:
    // 标记是否已初始化
    static bool m_initialized;
};

} // namespace utils 