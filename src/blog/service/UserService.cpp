#include "UserService.hpp"
#include <sstream>
#include <sodium.h>

UserService& UserService::getInstance() {
    static UserService instance;
    return instance;
}

std::optional<std::string> UserService::login(const std::string& username, const std::string& password) {
    try {
        auto& db = DbManager::getInstance();
        
        // 查询用户信息
        std::stringstream ss;
        ss << "SELECT id, username, password_hash, is_admin FROM authors WHERE username = '"
           << username << "' LIMIT 1";
           
        auto result = db.executeQuery(ss.str());
        auto row = result.fetchOne();
        
        if (!row) {
            return std::nullopt;  // 用户不存在
        }
        
        // 验证密码
        std::string storedHash = row[2].get<std::string>();
        if (crypto_pwhash_str_verify(
                storedHash.c_str(),
                password.c_str(),
                password.length()) != 0) {
            return std::nullopt;  // 密码错误
        }
        
        // 生成Token
        bool isAdmin = row[3].get<bool>();
        return JwtUtils::getInstance().generateToken(username, isAdmin);
        
    } catch (const std::exception&) {
        return std::nullopt;
    }
}

bool UserService::verifyPassword(const std::string& username, const std::string& password) {
    try {
        auto& db = DbManager::getInstance();
        
        std::stringstream ss;
        ss << "SELECT password_hash FROM authors WHERE username = '"
           << username << "' LIMIT 1";
           
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
        
        // 检查用户名是否已存在
        std::stringstream checkSs;
        checkSs << "SELECT COUNT(*) FROM authors WHERE username = '"
                << username << "' OR email = '" << email << "'";
                
        auto checkResult = db.executeQuery(checkSs.str());
        auto row = checkResult.fetchOne();
        
        if (row && row[0].get<int>() > 0) {
            return false;  // 用户名或邮箱已存在
        }
        
        // 生成密码哈希
        std::string passwordHash = hashPassword(password);
        
        // 插入新用户
        std::stringstream ss;
        ss << "INSERT INTO authors (username, display_name, email, password_hash) VALUES ('"
           << username << "', '"
           << displayName << "', '"
           << email << "', '"
           << passwordHash << "')";
           
        return db.executeUpdate(ss.str()) > 0;
        
    } catch (const std::exception&) {
        return false;
    }
}

std::optional<json> UserService::getUserInfo(const std::string& username) {
    try {
        auto& db = DbManager::getInstance();
        
        std::stringstream ss;
        ss << "SELECT id, username, display_name, email, bio, profile_image, is_admin "
           << "FROM authors WHERE username = '" << username << "' LIMIT 1";
           
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
        
        std::stringstream ss;
        ss << "UPDATE authors SET ";
        
        bool first = true;
        if (userInfo.contains("displayName")) {
            ss << "display_name = '" << userInfo["displayName"].get<std::string>() << "'";
            first = false;
        }
        if (userInfo.contains("email")) {
            if (!first) ss << ", ";
            ss << "email = '" << userInfo["email"].get<std::string>() << "'";
            first = false;
        }
        if (userInfo.contains("bio")) {
            if (!first) ss << ", ";
            ss << "bio = '" << userInfo["bio"].get<std::string>() << "'";
            first = false;
        }
        if (userInfo.contains("profileImage")) {
            if (!first) ss << ", ";
            ss << "profile_image = '" << userInfo["profileImage"].get<std::string>() << "'";
        }
        
        ss << " WHERE id = " << userId;
        
        return db.executeUpdate(ss.str()) > 0;
        
    } catch (const std::exception&) {
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