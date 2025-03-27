#pragma once

#include <string>
#include <unordered_map>

namespace utils {

// 错误码枚举
enum class ErrorCode {
    // 成功状态 (0)
    SUCCESS = 0,
    
    // 认证相关错误 (1000-1999)
    USER_NOT_FOUND = 1000,
    PASSWORD_ERROR = 1001,
    TOKEN_INVALID = 1002,
    TOKEN_EXPIRED = 1003,
    UNAUTHORIZED = 1004,
    FORBIDDEN = 1005,
    
    // 用户相关错误 (2000-2999)
    USERNAME_EXISTS = 2000,
    EMAIL_EXISTS = 2001,
    INVALID_USERNAME = 2002,
    INVALID_EMAIL = 2003,
    INVALID_PASSWORD = 2004,
    
    // 数据库相关错误 (3000-3999)
    DB_CONNECTION_ERROR = 3000,
    DB_QUERY_ERROR = 3001,
    DB_INSERT_ERROR = 3002,
    DB_UPDATE_ERROR = 3003,
    DB_DELETE_ERROR = 3004,
    
    // 系统相关错误 (4000-4999)
    SYSTEM_ERROR = 4000,
    INVALID_REQUEST = 4001,
    INVALID_PARAMETER = 4002,
    SERVER_ERROR = 4003,
    RESOURCE_NOT_FOUND = 4004,
    INTERNAL_ERROR = 500
};

// 错误信息映射
inline const std::unordered_map<ErrorCode, std::string> ERROR_MESSAGES = {
    {ErrorCode::SUCCESS, "操作成功"},
    {ErrorCode::USER_NOT_FOUND, "用户不存在"},
    {ErrorCode::PASSWORD_ERROR, "密码错误"},
    {ErrorCode::TOKEN_INVALID, "令牌无效"},
    {ErrorCode::TOKEN_EXPIRED, "令牌已过期"},
    {ErrorCode::UNAUTHORIZED, "未授权的访问"},
    {ErrorCode::FORBIDDEN, "禁止访问"},
    
    {ErrorCode::USERNAME_EXISTS, "用户名已存在"},
    {ErrorCode::EMAIL_EXISTS, "邮箱已被注册"},
    {ErrorCode::INVALID_USERNAME, "无效的用户名"},
    {ErrorCode::INVALID_EMAIL, "无效的邮箱地址"},
    {ErrorCode::INVALID_PASSWORD, "无效的密码"},
    
    {ErrorCode::DB_CONNECTION_ERROR, "数据库连接失败"},
    {ErrorCode::DB_QUERY_ERROR, "数据库查询失败"},
    {ErrorCode::DB_INSERT_ERROR, "数据库插入失败"},
    {ErrorCode::DB_UPDATE_ERROR, "数据库更新失败"},
    {ErrorCode::DB_DELETE_ERROR, "数据库删除失败"},
    
    {ErrorCode::SYSTEM_ERROR, "系统错误"},
    {ErrorCode::INVALID_REQUEST, "无效的请求"},
    {ErrorCode::INVALID_PARAMETER, "无效的参数"},
    {ErrorCode::SERVER_ERROR, "服务器内部错误"},
    {ErrorCode::RESOURCE_NOT_FOUND, "资源不存在"},
    {ErrorCode::INTERNAL_ERROR, "内部错误"}
};

// 获取错误信息
inline std::string getErrorMessage(ErrorCode code) {
    auto it = ERROR_MESSAGES.find(code);
    return it != ERROR_MESSAGES.end() ? it->second : "未知错误";
}

} // namespace utils 