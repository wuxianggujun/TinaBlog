//
// Created by wuxianggujun on 2025/3/14.
//

#include "ModuleRegistrar.hpp"
#include <cstring>
#include <stdexcept>
#include <algorithm>

namespace blog {

ModuleRegistrar::ModuleRegistrar() {
    // 在构造函数中进行必要的初始化
}

ModuleRegistrar::~ModuleRegistrar() {
    // 析构函数中释放资源
    for (auto* module : modules_) {
        delete static_cast<ngx_module_t*>(module);
    }
    modules_.clear();
    moduleNames_.clear();
}

ModuleRegistrar& ModuleRegistrar::instance() {
    static ModuleRegistrar instance;
    return instance;
}

bool ModuleRegistrar::registerHttpModule(ngx_module_t* module) {
    if (!module) {
        return false;
    }

    // 检查是否已经注册过该模块
    auto it = std::find_if(modules_.begin(), modules_.end(), 
                         [module](const ngx_module_t* m) {
                             return std::strcmp(m->name, module->name) == 0;
                         });
    
    if (it != modules_.end()) {
        // 模块已存在，不重复注册
        return false;
    }

    // 将模块添加到注册列表中
    modules_.push_back(module);
    return true;
}

ngx_http_module_t* ModuleRegistrar::createHttpModuleContext(
    ngx_int_t (*preconfiguration)(ngx_conf_t *cf),
    ngx_int_t (*postconfiguration)(ngx_conf_t *cf),
    void* (*createMainConf)(ngx_conf_t *cf),
    char* (*initMainConf)(ngx_conf_t *cf, void *conf),
    void* (*createSrvConf)(ngx_conf_t *cf),
    char* (*mergeSrvConf)(ngx_conf_t *cf, void *prev, void *conf),
    void* (*createLocConf)(ngx_conf_t *cf),
    char* (*mergeLocConf)(ngx_conf_t *cf, void *prev, void *conf))
{
    // 分配内存
    auto* ctx = new ngx_http_module_t();
    
    // 初始化上下文
    std::memset(ctx, 0, sizeof(ngx_http_module_t));
    
    // 设置回调函数
    ctx->preconfiguration = preconfiguration;
    ctx->postconfiguration = postconfiguration;
    ctx->create_main_conf = createMainConf;
    ctx->init_main_conf = initMainConf;
    ctx->create_srv_conf = createSrvConf;
    ctx->merge_srv_conf = mergeSrvConf;
    ctx->create_loc_conf = createLocConf;
    ctx->merge_loc_conf = mergeLocConf;
    
    return ctx;
}

ngx_module_t* ModuleRegistrar::createHttpModule(
    const std::string& name,
    ngx_http_module_t* ctx,
    ngx_command_t* cmds,
    ngx_int_t (*initMaster)(ngx_log_t *log),
    ngx_int_t (*initModule)(ngx_cycle_t *cycle),
    ngx_int_t (*initProcess)(ngx_cycle_t *cycle),
    ngx_int_t (*initThread)(ngx_cycle_t *cycle),
    void (*exitThread)(ngx_cycle_t *cycle),
    void (*exitProcess)(ngx_cycle_t *cycle),
    void (*exitMaster)(ngx_cycle_t *cycle))
{
    if (name.empty() || !ctx) {
        throw std::invalid_argument("Module name and context are required");
    }
    
    // 分配内存
    auto* module = new ngx_module_t();
    
    // 初始化模块结构
    std::memset(module, 0, sizeof(ngx_module_t));
    
    // 处理模块名称（需要持久化存储）
    auto nameLen = name.length() + 1;  // +1 for null terminator
    auto moduleName = std::make_unique<char[]>(nameLen);
    std::strncpy(moduleName.get(), name.c_str(), nameLen);
    
    // 设置模块属性
    module->ctx_index = NGX_MODULE_UNSET_INDEX;
    module->index = NGX_MODULE_UNSET_INDEX;
    module->name = moduleName.get();
    module->version = NGX_MODULE_V1;
    module->signature = NGX_MODULE_SIGNATURE;
    module->ctx = ctx;
    module->commands = cmds;
    module->type = NGX_HTTP_MODULE;
    
    // 设置回调函数
    module->init_master = initMaster;
    module->init_module = initModule;
    module->init_process = initProcess;
    module->init_thread = initThread;
    module->exit_thread = exitThread;
    module->exit_process = exitProcess;
    module->exit_master = exitMaster;
    
    // 存储模块名称 - 通过单例实例访问非静态成员
    // 获取单例实例
    ModuleRegistrar& registrar = ModuleRegistrar::instance();
    
    // 将模块名称添加到单例实例的moduleNames_中
    registrar.moduleNames_.push_back(std::move(moduleName));
    
    return module;
}

const std::vector<ngx_module_t*>& ModuleRegistrar::getRegisteredModules() const {
    return modules_;
}

} // namespace blog 