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

#include "blog/db/DbManager.hpp"
#include "blog/auth/JwtManager.hpp"
#include "blog/auth/JwtAuthFilter.hpp"
#include "blog/controllers/AuthController.hpp"
#include "blog/controllers/HealthController.hpp"
#include <drogon/drogon.h>
#include <json/json.h>

// 定义JWT密钥（在实际应用中应从配置文件或环境变量获取）
const std::string JWT_SECRET = "wuxianggujun-tina-blog-3344207732";

// 定义一个全局指针，用于在信号处理程序中访问DbManager
static DbManager* g_dbManager = nullptr;

// 信号处理函数，用于优雅地处理程序终止
void signalHandler(int signum) {
    std::cout << "收到信号 " << signum << "，正在关闭数据库连接..." << std::endl;
    
    // 关闭数据库连接
    if (g_dbManager) {
        g_dbManager->close();
    }
    
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
        
        // 关闭当前连接，重新连接到blog数据库
        dbManager.close();
        
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
                std::string testQuery = "SELECT 1";
                auto result = dbManager.executeQuery(testQuery);
                if (!result.empty()) {
                    std::cout << "测试查询执行成功，连接有效" << std::endl;
                } else {
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
    
    // 设置Vue编译后的dist目录作为文档根目录
    const std::string distPath = "./html/blog/dist";
    std::cout << "设置静态资源目录: " << distPath << std::endl;
    drogon::app().setDocumentRoot(distPath);
    
    // 设置静态文件缓存时间
    drogon::app().setStaticFilesCacheTime(0); // 开发环境禁用缓存
    
    // 配置默认首页
    drogon::app().registerHandler("/", 
        [distPath](const drogon::HttpRequestPtr& req, 
           std::function<void(const drogon::HttpResponsePtr&)>&& callback) {
            // 返回首页index.html
            auto resp = drogon::HttpResponse::newFileResponse(distPath + "/index.html");
            callback(resp);
        });
    
    // 配置处理前端路由的通配符路由
    drogon::app().registerHandler("/{path}", 
        [distPath](const drogon::HttpRequestPtr& req, 
           std::function<void(const drogon::HttpResponsePtr&)>&& callback, 
           const std::string& path) {
            // 对于任何路径请求，都返回index.html以支持前端路由
            auto resp = drogon::HttpResponse::newFileResponse(distPath + "/index.html");
            callback(resp);
        }, {drogon::Get});
    
    // 设置JWT密钥作为应用程序全局配置
    Json::Value config;
    config["jwt_secret"] = JWT_SECRET;
    drogon::app().loadConfigJson(config);
    
    // Drogon会自动发现并注册继承自HttpController的控制器类
    // 不需要手动注册控制器
    
    // 启动Drogon
    std::cout << "启动Web服务器，监听 http://localhost:8080" << std::endl;
    drogon::app().run();
    
    // 应用结束，关闭数据库连接
    std::cout << "应用程序正常结束，关闭数据库连接..." << std::endl;
    dbManager.close();
    
    return 0;
} 