#include "PasswordUtils.hpp"
#include <iostream>
#include <vector>

namespace utils {

// 静态成员初始化
bool PasswordUtils::m_initialized = false;

bool PasswordUtils::initialize() {
    if (m_initialized) {
        return true;
    }
    
    if (sodium_init() < 0) {
        std::cerr << "无法初始化libsodium库" << std::endl;
        return false;
    }
    
    m_initialized = true;
    return true;
}

std::string PasswordUtils::hashPassword(const std::string& password) {
    // 确保库已初始化
    if (!m_initialized && !initialize()) {
        return "";
    }
    
    // Argon2id的输出长度
    const size_t hashLen = crypto_pwhash_STRBYTES;
    
    // 分配足够存储哈希结果的空间
    std::vector<char> hashedPassword(hashLen);
    
    // 执行密码哈希
    if (crypto_pwhash_str(
            hashedPassword.data(),
            password.c_str(),
            password.length(),
            // 操作限制（计算强度，内存使用限制）
            crypto_pwhash_OPSLIMIT_INTERACTIVE,  // 针对普通登录的计算复杂度
            crypto_pwhash_MEMLIMIT_INTERACTIVE   // 内存限制
        ) != 0) {
        std::cerr << "密码哈希失败：内存不足" << std::endl;
        return "";
    }
    
    // 转换为字符串并返回
    return std::string(hashedPassword.data());
}

bool PasswordUtils::verifyPassword(const std::string& password, const std::string& hashedPassword) {
    // 确保库已初始化
    if (!m_initialized && !initialize()) {
        return false;
    }
    
    // 验证密码
    return crypto_pwhash_str_verify(
        hashedPassword.c_str(),
        password.c_str(),
        password.length()
    ) == 0;
}

} // namespace utils 