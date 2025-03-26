#include <iostream>
#include <thread>
#include <csignal>

// 只在Windows平台上包含Windows头文件
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX  // 防止Windows头文件定义min/max宏
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN  // 减少包含内容
#endif
#include <Windows.h>
#endif

#include <fstream>

#include "blog/db/DbManager.hpp"
#include "blog/auth/JwtManager.hpp"
#include "blog/auth/JwtAuthFilter.hpp"
#include "blog/controllers/AuthController.hpp"
#include "blog/controllers/HealthController.hpp"
#include "blog/controllers/PostController.hpp"
#include <drogon/drogon.h>
#include <json/json.h>

// 定义JWT密钥（在实际应用中应从配置文件或环境变量获取）
const std::string JWT_SECRET = "wuxianggujun-tina-blog-3344207732";

// 定义一个全局指针，用于在信号处理程序中访问DbManager
static DbManager* g_dbManager = nullptr;

// 获取当前工作目录的函数
std::string getCurrentWorkingDir() {
    try {
        // 尝试使用C++17标准库
        return std::filesystem::current_path().string();
    } catch(const std::exception& e) {
        // 如果C++17方法失败，使用平台特定方法
        char buffer[1024];
#ifdef _WIN32
        if (_getcwd(buffer, sizeof(buffer)) != NULL) {
            return std::string(buffer);
        }
#else
        if (getcwd(buffer, sizeof(buffer)) != NULL) {
            return std::string(buffer);
        }
#endif
        std::cerr << "获取当前工作目录失败: " << e.what() << std::endl;
        return "未知目录";
    }
}

// 检查文件或目录是否存在
bool fileExists(const std::string& path) {
    try {
        return std::filesystem::exists(path);
    } catch(const std::exception& e) {
        std::cerr << "检查文件路径失败: " << e.what() << std::endl;
        return false;
    }
}


// 信号处理函数，用于优雅地处理程序终止
void signalHandler(int signum) {
    std::cout << "收到信号 " << signum << "，正在关闭应用..." << std::endl;
    
    // 通知Drogon框架停止
    drogon::app().quit();
    
    // 退出程序
    exit(signum);
}

// 设置控制台编码为UTF-8（仅Windows平台）
static void setConsoleUTF8() {
#ifdef _WIN32
    // 设置控制台代码页为UTF-8
    SetConsoleOutputCP(CP_UTF8);
    SetConsoleCP(CP_UTF8);
#endif
}

int main()
{
    setConsoleUTF8();

        
    // 输出当前工作目录
    std::string currentDir = getCurrentWorkingDir();
    std::cout << "当前工作目录: " << currentDir << std::endl;
    
    
    // 设置信号处理
    std::signal(SIGINT, signalHandler);
    std::signal(SIGTERM, signalHandler);
    
    std::cout << "TinaBlog 启动中..." << std::endl;
    
    // 初始化数据库管理器
    DbManager& dbManager = DbManager::getInstance();
    g_dbManager = &dbManager;  // 保存全局指针用于信号处理
    
    
    // 使用环境变量（如果存在）或默认值
    std::string host = "postgres-db";
    std::string port =  "5432";
    std::string user =  "postgres";
    std::string password =  "postgres";  // 修改为postgres默认密码
    std::string defaultDb = "postgres";
    
    std::cout << "数据库连接信息：" << std::endl;
    std::cout << "  主机: " << host << std::endl;
    std::cout << "  端口: " << port << std::endl;
    std::cout << "  用户: " << user << std::endl;
    std::cout << "  数据库: " << defaultDb << std::endl;
    

    // 首先连接到默认数据库postgres
    std::string connStr = "host=" + host + " port=" + port + " user=" + user + " password=" + password + " dbname=" + defaultDb;
    
    if (dbManager.initialize(connStr)) {
        std::cout << "连接到默认数据库成功" << std::endl;
        
        // 使用环境变量中的或默认的blog数据库名
        std::string blogDbName = "blog";
        std::cout << "  数据库: " << blogDbName << std::endl;

        // 检查blog数据库是否存在，如果不存在则创建
        if (!dbManager.databaseExists(blogDbName)) {
            std::cout << blogDbName << "数据库不存在，正在创建..." << std::endl;
            if (!dbManager.createDatabase(blogDbName)) {
                std::cerr << "创建" << blogDbName << "数据库失败" << std::endl;
                return 1;
            }
        } else {
            std::cout << blogDbName << "数据库已存在" << std::endl;
        }
        
        // 简单等待一会儿确保连接完全关闭
        std::this_thread::sleep_for(std::chrono::seconds(1));
        std::cout << "正在连接到blog数据库..." << std::endl;
        
        // 重新连接到blog数据库
        connStr = "host=" + host + " port=" + port + " user=" + user + " password=" + password + " dbname=" + blogDbName;
        if (dbManager.initialize(connStr)) {
            std::cout << "连接到blog数据库成功" << std::endl;
            
            // 测试连接是否可用
            if (dbManager.isConnected()) {
                std::cout << "已成功验证与blog数据库的连接" << std::endl;
            } else {
                std::cerr << "无法验证与blog数据库的连接，可能存在问题" << std::endl;
                return 1;
            }
            
            // 尝试执行一个简单查询以进一步验证连接
            try {
                bool connectionOk = false;
                
                // 使用新的异步查询接口
                dbManager.executeQuery(
                    "SELECT 1",
                    [&connectionOk](const drogon::orm::Result& result) {
                        connectionOk = (result.size() > 0);
                        std::cout << "测试查询执行成功，连接有效" << std::endl;
                    },
                    [](const drogon::orm::DrogonDbException& e) {
                        std::cerr << "测试查询执行失败: " << e.base().what() << std::endl;
                    }
                );
                
                // 给异步查询一些时间完成
                std::this_thread::sleep_for(std::chrono::milliseconds(500));
                
                if (!connectionOk) {
                    // 可能异步查询没有足够时间完成，我们也可以尝试同步查询
                    auto result = dbManager.execSyncQuery("SELECT 1");
                    if (result.size() > 0) {
                        std::cout << "同步测试查询执行成功，连接有效" << std::endl;
                        connectionOk = true;
                    }
                }
                
                if (!connectionOk) {
                    std::cerr << "测试查询执行失败，连接可能无效" << std::endl;
                    return 1;
                }
            } catch (const std::exception& e) {
                std::cerr << "执行测试查询时发生异常: " << e.what() << std::endl;
                return 1;
            }
            
            // 创建表结构
            if (dbManager.createTables()) {
                std::cout << "数据表创建成功" << std::endl;
            } else {
                std::cerr << "数据表创建失败" << std::endl;
                return 1;
            }
        } else {
            std::cerr << "无法连接到blog数据库" << std::endl;
            return 1;
        }
    } else {
        std::cerr << "数据库连接失败" << std::endl;
        return 1;
    }
    
    // 配置Drogon服务器
    drogon::app().addListener("0.0.0.0", 8080);
    
    // 记录JWT配置信息（现在已经硬编码在JwtManager类中）
    std::cout << "JWT配置信息：" << std::endl;
    std::cout << "  - 密钥长度: " << JWT_SECRET.length() << std::endl;
    std::cout << "  - 发行者: tinablog" << std::endl;
    std::cout << "  - 过期时间: 1296000秒" << std::endl;
    
    // 设置文档根目录
    const std::string distPath = "./html/blog";
    std::cout << "设置静态资源目录: " << distPath << std::endl;

    // 检查静态资源目录是否存在
    std::string absoluteDistPath = currentDir + "/" + distPath;
    if (fileExists(distPath)) {
        std::cout << "静态资源相对路径存在: " << distPath << std::endl;
    } else {
        std::cout << "警告: 相对路径不存在: " << distPath << std::endl;
    }
    
    if (fileExists(absoluteDistPath)) {
        std::cout << "静态资源绝对路径存在: " << absoluteDistPath << std::endl;
    } else {
        std::cout << "警告: 绝对路径不存在: " << absoluteDistPath << std::endl;
        
        // 尝试列出当前目录下的内容
        std::cout << "当前目录内容:" << std::endl;
        try {
            for (const auto& entry : std::filesystem::directory_iterator(currentDir)) {
                std::cout << "  " << entry.path().string() << std::endl;
            }
        } catch (const std::exception& e) {
            std::cerr << "  无法列出目录内容: " << e.what() << std::endl;
        }
    }
    
    
    drogon::app().setDocumentRoot(distPath);
    
    // 设置静态文件缓存时间
    drogon::app().setStaticFilesCacheTime(0); // 开发环境禁用缓存
    
    // 输出静态资源目录内容
    std::cout << "静态资源目录 (" << distPath << ") 内容:" << std::endl;
    try {
        for (const auto& entry : std::filesystem::directory_iterator(distPath)) {
            std::cout << "  " << entry.path().filename().string();
            if (std::filesystem::is_directory(entry.path())) {
                std::cout << "/";
            }
            std::cout << std::endl;
            
            // 如果是assets目录，列出其内容
            if (std::filesystem::is_directory(entry.path()) && 
                entry.path().filename().string() == "assets") {
                std::cout << "    assets/ 目录内容:" << std::endl;
                try {
                    for (const auto& assetEntry : std::filesystem::directory_iterator(entry.path())) {
                        std::cout << "      " << assetEntry.path().filename().string() << std::endl;
                    }
                } catch (const std::exception& e) {
                    std::cerr << "      无法列出assets目录内容: " << e.what() << std::endl;
                }
            }
        }
    } catch (const std::exception& e) {
        std::cerr << "  无法列出静态资源目录内容: " << e.what() << std::endl;
    }
    
    // 添加静态资源路径监听
    drogon::app().registerHandler("/assets/{path}", 
        [distPath](const drogon::HttpRequestPtr& req, 
           std::function<void(const drogon::HttpResponsePtr&)>&& callback,
           const std::string& path) {
            std::string assetPath = distPath + "/assets/" + path;
            std::cout << "请求静态资源: " << assetPath << std::endl;
            if (fileExists(assetPath)) {
                auto resp = drogon::HttpResponse::newFileResponse(assetPath);
                callback(resp);
            } else {
                std::cout << "静态资源不存在: " << assetPath << std::endl;
                auto resp = drogon::HttpResponse::newNotFoundResponse();
                callback(resp);
            }
        });
    
    // 配置默认首页
    drogon::app().registerHandler("/", 
        [distPath, currentDir](const drogon::HttpRequestPtr& req, 
           std::function<void(const drogon::HttpResponsePtr&)>&& callback) {
            std::string indexPath = distPath + "/index.html";
            // 检查index.html是否存在
            if (fileExists(indexPath)) {
                std::cout << "请求 /: index.html 存在于 " << indexPath << std::endl;
            } else {
                std::cout << "警告 - 请求 /: index.html 不存在于 " << indexPath << std::endl;
            }
            // 返回首页index.html
            auto resp = drogon::HttpResponse::newFileResponse(indexPath);
            callback(resp);
        });
    
    // 配置处理前端路由的通配符路由
    drogon::app().registerHandler("/{path}", 
        [distPath](const drogon::HttpRequestPtr& req, 
           std::function<void(const drogon::HttpResponsePtr&)>&& callback, 
           const std::string& path) {
            std::string indexPath = distPath + "/index.html";
            std::cout << "请求路径: /" << path << " -> 将返回 index.html" << std::endl;
            // 对于任何路径请求，都返回index.html以支持前端路由
            auto resp = drogon::HttpResponse::newFileResponse(indexPath);
            callback(resp);
        }, {drogon::Get});
    
    // Drogon会自动发现并注册继承自HttpController的控制器类
    // 不需要手动注册控制器
    
    // 启动Drogon
    std::cout << "启动Web服务器，监听 http://localhost:8080" << std::endl;
    drogon::app().run();
    
    // 应用结束
    std::cout << "应用程序正常结束" << std::endl;
    
    return 0;
} 