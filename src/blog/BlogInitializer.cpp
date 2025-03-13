//
// Created by wuxianggujun on 2025/3/14.
//

#include "BlogModule.hpp"
#include <iostream>

// 在全局作用域使用构造函数自动注册模块
namespace {
    struct BlogInitializer {
        BlogInitializer() {
            // 注册博客模块
            if (blog::BlogModule::registerModule()) {
                std::cout << "Successfully registered " 
                          << blog::BlogModule::getModuleName() 
                          << " (version " << blog::BlogModule::getModuleVersion() << ")" 
                          << std::endl;
            } else {
                std::cerr << "Failed to register " 
                          << blog::BlogModule::getModuleName() 
                          << std::endl;
            }
        }
    };

    // 创建一个静态的初始化器实例
    // 这样可以在程序启动时自动执行初始化代码
    static BlogInitializer initializer;
} 