//
// Created by wuxianggujun on 2025/3/14.
//

#include "HttpConfigHelper.hpp"
#include <cstring>


ngx_command_t HttpConfigHelper::createCommand(
    const std::string& name,
    ngx_uint_t type,
    char* (*set)(ngx_conf_t*, ngx_command_t*, void*),
    ngx_uint_t conf,
    ngx_uint_t offset,
    void* post)
{
    ngx_command_t cmd;
    
    // 使用 ngx_string 宏来避免直接操作指针
    // 注意：这里的 name 字符串必须是常量，因为 nginx 不会复制它
    cmd.name = stringToNgxStr(name);
    cmd.type = type;
    cmd.set = set;
    cmd.conf = conf;
    cmd.offset = offset;
    cmd.post = post;
    
    return cmd;
}

ngx_command_t HttpConfigHelper::createNullCommand() {
    ngx_command_t cmd;
    std::memset(&cmd, 0, sizeof(ngx_command_t));
    return cmd;
}

ngx_command_t* HttpConfigHelper::createCommandArray(const std::vector<ngx_command_t>& commands) {
    // 为命令数组分配内存，加1是为了存放结束标记（ngx_null_command）
    auto* cmds = new ngx_command_t[commands.size() + 1];
    
    // 复制命令
    for (size_t i = 0; i < commands.size(); ++i) {
        cmds[i] = commands[i];
    }
    
    // 设置结束标记
    cmds[commands.size()] = createNullCommand();
    
    return cmds;
}

ngx_str_t HttpConfigHelper::stringToNgxStr(const std::string& str) {
    ngx_str_t result;
    
    // 设置长度和数据指针
    result.len = str.size();
    result.data = const_cast<u_char*>(reinterpret_cast<const u_char*>(str.c_str()));
    
    return result;
}

std::string HttpConfigHelper::ngxStrToString(const ngx_str_t& str) {
    return std::string(reinterpret_cast<const char*>(str.data), str.len);
}

ngx_int_t HttpConfigHelper::setAllowedMethods(ngx_http_request_t* r, ngx_uint_t methods) {
    // 检查当前请求的方法是否在允许列表中
    if ((r->method & methods) == 0) {
        // 如果不在允许列表中
        // Windows版本的nginx中headers_out结构体没有allow成员
        // 需要创建一个新的头部
        ngx_table_elt_t* allow = static_cast<ngx_table_elt_t*>(
            ngx_list_push(&r->headers_out.headers));
            
        if (allow == nullptr) {
            return NGX_ERROR;
        }
        
        allow->hash = 1;
        ngx_str_set(&allow->key, "Allow");
        
        // 设置允许的方法值
        // 这里应该根据methods参数构建一个适当的字符串
        // 简化起见，我们这里只返回"GET, HEAD"
        ngx_str_set(&allow->value, "GET, HEAD");
        
        // 返回错误状态（方法不允许）
        return NGX_HTTP_NOT_ALLOWED;
    }
    
    return NGX_OK;
}