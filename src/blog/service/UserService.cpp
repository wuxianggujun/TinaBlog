#include "UserService.hpp"
#include <sstream>
#include <sodium.h>
#include <mutex>
#include <algorithm>
#include "../NgxLog.hpp"

UserService& UserService::getInstance() {
    static UserService instance;
    return instance;
}

std::optional<std::string> UserService::login(const std::string& username, const std::string& password) {
    try {
        // 使用参数转义，防止SQL注入
        std::string escapedUsername = username;
        // 简单替换单引号 - 修复std::replace使用方式
        for (size_t pos = 0; pos < escapedUsername.length(); ++pos) {
            if (escapedUsername[pos] == '\'') {
                escapedUsername[pos] = '\\';
                escapedUsername.insert(pos + 1, 1, '\'');
                ++pos; // 跳过新插入的字符
            }
        }
        
        // 获取日志实例
        NgxLog log;
        log.debug("执行登录查询: %s", escapedUsername.c_str());
        
        // 组装SQL查询 - 避免在锁中构建
        std::stringstream ss;
        ss << "SELECT id, username, password_hash, is_admin FROM authors WHERE username = '"
           << escapedUsername << "' LIMIT 1";
        std::string sql = ss.str();
        
        // 仅在必要的数据库操作部分使用互斥锁
        static std::mutex login_mutex;
        mysqlx::Row row;
        
        {
            std::lock_guard<std::mutex> lock(login_mutex);
            auto& db = DbManager::getInstance();
            auto result = db.executeQuery(sql);
            if (result.count() > 0) {
                row = result.fetchOne();
            }
        }
        
        // 处理查询结果 - 锁外进行
        if (!row) {
            log.debug("登录失败: 用户 %s 不存在", escapedUsername.c_str());
            return std::nullopt;  // 用户不存在
        }
        
        // 验证密码
        std::string storedHash = row[2].get<std::string>();
        if (crypto_pwhash_str_verify(
                storedHash.c_str(),
                password.c_str(),
                password.length()) != 0) {
            log.debug("登录失败: 用户 %s 密码错误", escapedUsername.c_str());
            return std::nullopt;  // 密码错误
        }
        
        // 获取用户ID和管理员状态
        int userId = row[0].get<int>();
        bool isAdmin = row[3].get<bool>();
        
        log.debug("登录成功: 用户 %s (ID: %d, 管理员: %s)", 
                 escapedUsername.c_str(), userId, isAdmin ? "是" : "否");
        
        // 生成Token
        return JwtUtils::getInstance().generateToken(username, isAdmin);
        
    } catch (const std::exception& e) {
        NgxLog log;
        log.error("登录过程中出现异常: %s", e.what());
        throw std::runtime_error(std::string("登录失败: ") + e.what());
    }
}

bool UserService::verifyPassword(const std::string& username, const std::string& password) {
    try {
        auto& db = DbManager::getInstance();
        
        // 转义用户名
        std::string escapedUsername = username;
        for (size_t pos = 0; pos < escapedUsername.length(); ++pos) {
            if (escapedUsername[pos] == '\'') {
                escapedUsername[pos] = '\\';
                escapedUsername.insert(pos + 1, 1, '\'');
                ++pos;
            }
        }
        
        std::stringstream ss;
        ss << "SELECT password_hash FROM authors WHERE username = '"
           << escapedUsername << "' LIMIT 1";
           
        auto result = db.executeQuery(ss.str());
        auto row = result.fetchOne();
        
        if (!row) {
            return false;  // 用户不存在
        }
        
        std::string storedHash = row[0].get<std::string>();
        return crypto_pwhash_str_verify(
            storedHash.c_str(),
            password.c_str(),
            password.length()) == 0;
        
    } catch (const std::exception&) {
        return false;
    }
}

bool UserService::createUser(const std::string& username, const std::string& password,
                           const std::string& displayName, const std::string& email) {
    try {
        auto& db = DbManager::getInstance();
        
        // 转义所有字段
        std::string escapedUsername = username;
        std::string escapedDisplayName = displayName;
        std::string escapedEmail = email;
        
        // 转义函数
        auto escapeSQL = [](std::string& str) {
            for (size_t pos = 0; pos < str.length(); ++pos) {
                if (str[pos] == '\'') {
                    str[pos] = '\\';
                    str.insert(pos + 1, 1, '\'');
                    ++pos;
                }
            }
        };
        
        escapeSQL(escapedUsername);
        escapeSQL(escapedDisplayName);
        escapeSQL(escapedEmail);
        
        // 检查用户名是否已存在
        std::stringstream checkSs;
        checkSs << "SELECT COUNT(*) FROM authors WHERE username = '"
                << escapedUsername << "' OR email = '" << escapedEmail << "'";
                
        auto checkResult = db.executeQuery(checkSs.str());
        auto row = checkResult.fetchOne();
        
        if (row && row[0].get<int>() > 0) {
            return false;  // 用户名或邮箱已存在
        }
        
        // 生成密码哈希
        std::string passwordHash = hashPassword(password);
        
        // 转义密码哈希
        std::string escapedPasswordHash = passwordHash;
        escapeSQL(escapedPasswordHash);
        
        // 插入新用户
        std::stringstream ss;
        ss << "INSERT INTO authors (username, display_name, email, password_hash) VALUES ('"
           << escapedUsername << "', '"
           << escapedDisplayName << "', '"
           << escapedEmail << "', '"
           << escapedPasswordHash << "')";
           
        return db.executeUpdate(ss.str()) > 0;
        
    } catch (const std::exception&) {
        return false;
    }
}

std::optional<json> UserService::getUserInfo(const std::string& username) {
    try {
        auto& db = DbManager::getInstance();
        
        // 转义用户名
        std::string escapedUsername = username;
        for (size_t pos = 0; pos < escapedUsername.length(); ++pos) {
            if (escapedUsername[pos] == '\'') {
                escapedUsername[pos] = '\\';
                escapedUsername.insert(pos + 1, 1, '\'');
                ++pos;
            }
        }
        
        std::stringstream ss;
        ss << "SELECT id, username, display_name, email, bio, profile_image, is_admin "
           << "FROM authors WHERE username = '" << escapedUsername << "' LIMIT 1";
           
        auto result = db.executeQuery(ss.str());
        auto row = result.fetchOne();
        
        if (!row) {
            return std::nullopt;
        }
        
        json userInfo = {
            {"id", row[0].get<int>()},
            {"username", row[1].get<std::string>()},
            {"displayName", row[2].get<std::string>()},
            {"email", row[3].get<std::string>()},
            {"bio", row[4].isNull() ? "" : row[4].get<std::string>()},
            {"profileImage", row[5].isNull() ? "" : row[5].get<std::string>()},
            {"isAdmin", row[6].get<bool>()}
        };
        
        return userInfo;
        
    } catch (const std::exception&) {
        return std::nullopt;
    }
}

bool UserService::updateUser(int userId, const json& userInfo) {
    try {
        auto& db = DbManager::getInstance();
        
        // 转义函数
        auto escapeSQL = [](std::string& str) {
            for (size_t pos = 0; pos < str.length(); ++pos) {
                if (str[pos] == '\'') {
                    str[pos] = '\\';
                    str.insert(pos + 1, 1, '\'');
                    ++pos;
                }
            }
        };
        
        std::stringstream ss;
        ss << "UPDATE authors SET ";
        
        bool first = true;
        if (userInfo.contains("displayName")) {
            std::string displayName = userInfo["displayName"].get<std::string>();
            escapeSQL(displayName);
            ss << "display_name = '" << displayName << "'";
            first = false;
        }
        if (userInfo.contains("email")) {
            std::string email = userInfo["email"].get<std::string>();
            escapeSQL(email);
            if (!first) ss << ", ";
            ss << "email = '" << email << "'";
            first = false;
        }
        if (userInfo.contains("bio")) {
            std::string bio = userInfo["bio"].get<std::string>();
            escapeSQL(bio);
            if (!first) ss << ", ";
            ss << "bio = '" << bio << "'";
            first = false;
        }
        if (userInfo.contains("profileImage")) {
            std::string profileImage = userInfo["profileImage"].get<std::string>();
            escapeSQL(profileImage);
            if (!first) ss << ", ";
            ss << "profile_image = '" << profileImage << "'";
        }
        
        ss << " WHERE id = " << userId;
        
        return db.executeUpdate(ss.str()) > 0;
        
    } catch (const std::exception& e) {
        NgxLog log;
        log.error("更新用户信息时出错: %s", e.what());
        return false;
    }
}

bool UserService::deleteUser(int userId) {
    try {
        auto& db = DbManager::getInstance();
        
        std::stringstream ss;
        ss << "DELETE FROM authors WHERE id = " << userId;
        
        return db.executeUpdate(ss.str()) > 0;
        
    } catch (const std::exception&) {
        return false;
    }
}

std::string UserService::hashPassword(const std::string& password) {
    unsigned char hash[crypto_pwhash_STRBYTES];
    
    if (crypto_pwhash_str(
        (char*)hash,
        password.c_str(),
        password.length(),
        crypto_pwhash_OPSLIMIT_INTERACTIVE,
        crypto_pwhash_MEMLIMIT_INTERACTIVE) != 0) {
        throw std::runtime_error("Failed to hash password");
    }
    
    return std::string((char*)hash);
} 