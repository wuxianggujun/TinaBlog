//
// Created by wuxianggujun on 2025/3/14.
//

#ifndef TINA_BLOG_MODULE_REGISTRAR_HPP
#define TINA_BLOG_MODULE_REGISTRAR_HPP

#include "Nginx.hpp"
#include <vector>
#include <memory>
#include <functional>

namespace blog {

/**
 * @brief Nginx模块注册器
 * 
 * 该类提供了对Nginx模块进行注册和管理的功能，允许在不修改Nginx核心代码的情况下
 * 动态地向Nginx添加自定义模块。
 */
class ModuleRegistrar {
public:
    /**
     * @brief 获取ModuleRegistrar单例实例
     * @return ModuleRegistrar& 单例引用
     */
    static ModuleRegistrar& instance();

    /**
     * @brief 注册HTTP模块
     * @param module Nginx模块指针
     * @return 是否注册成功
     */
    bool registerHttpModule(ngx_module_t* module);

    /**
     * @brief 创建一个HTTP模块上下文
     * @param preconfiguration 预配置回调函数
     * @param postconfiguration 后配置回调函数
     * @param createMainConf 创建主配置回调函数
     * @param initMainConf 初始化主配置回调函数
     * @param createSrvConf 创建服务器配置回调函数
     * @param mergeSrvConf 合并服务器配置回调函数
     * @param createLocConf 创建位置配置回调函数
     * @param mergeLocConf 合并位置配置回调函数
     * @return 上下文对象指针，需要外部释放
     */
    static ngx_http_module_t* createHttpModuleContext(
        ngx_int_t (*preconfiguration)(ngx_conf_t *cf) = nullptr,
        ngx_int_t (*postconfiguration)(ngx_conf_t *cf) = nullptr,
        void* (*createMainConf)(ngx_conf_t *cf) = nullptr,
        char* (*initMainConf)(ngx_conf_t *cf, void *conf) = nullptr,
        void* (*createSrvConf)(ngx_conf_t *cf) = nullptr,
        char* (*mergeSrvConf)(ngx_conf_t *cf, void *prev, void *conf) = nullptr,
        void* (*createLocConf)(ngx_conf_t *cf) = nullptr,
        char* (*mergeLocConf)(ngx_conf_t *cf, void *prev, void *conf) = nullptr
    );

    /**
     * @brief 创建一个完整的HTTP模块
     * @param name 模块名称
     * @param ctx 模块上下文
     * @param cmds 模块命令
     * @param initMaster 初始化主进程回调
     * @param initModule 初始化模块回调
     * @param initProcess 初始化进程回调
     * @param initThread 初始化线程回调
     * @param exitThread 退出线程回调
     * @param exitProcess 退出进程回调
     * @param exitMaster 退出主进程回调
     * @return 模块对象指针，需要外部释放
     */
    static ngx_module_t* createHttpModule(
        const std::string& name,
        ngx_http_module_t* ctx,
        ngx_command_t* cmds,
        ngx_int_t (*initMaster)(ngx_log_t *log) = nullptr,
        ngx_int_t (*initModule)(ngx_cycle_t *cycle) = nullptr,
        ngx_int_t (*initProcess)(ngx_cycle_t *cycle) = nullptr,
        ngx_int_t (*initThread)(ngx_cycle_t *cycle) = nullptr,
        void (*exitThread)(ngx_cycle_t *cycle) = nullptr,
        void (*exitProcess)(ngx_cycle_t *cycle) = nullptr,
        void (*exitMaster)(ngx_cycle_t *cycle) = nullptr
    );

    /**
     * @brief 获取已注册的所有模块
     * @return 模块指针数组
     */
    const std::vector<ngx_module_t*>& getRegisteredModules() const;

    // 删除拷贝构造和赋值操作
    ModuleRegistrar(const ModuleRegistrar&) = delete;
    ModuleRegistrar& operator=(const ModuleRegistrar&) = delete;

private:
    // 私有构造函数，确保单例模式
    ModuleRegistrar();
    ~ModuleRegistrar();

    // 存储已注册的模块
    std::vector<ngx_module_t*> modules_;
    
    // 用于模块名称的存储
    std::vector<std::unique_ptr<char[]>> moduleNames_;
};

} // namespace blog

#endif // TINA_BLOG_MODULE_REGISTRAR_HPP 