# NGINX CMake 构建项目

<div align="right">
  <a href="README-EN.md">English</a> | <a href="README.md">中文</a>
</div>

<picture>
  <source media="(prefers-color-scheme: dark)" srcset="https://github.com/user-attachments/assets/9335b488-ffcc-4157-8364-2370a0b70ad0">
  <source media="(prefers-color-scheme: light)" srcset="https://github.com/user-attachments/assets/3a7eeb08-1133-47f5-859c-fad4f5a6a013">
  <img alt="NGINX Banner">
</picture>

## 项目简介

本项目提供了使用CMake在Windows平台下构建和调试Nginx的解决方案。通过使用CMake，无需复杂的依赖安装处理和相关三方库配置，直接构建工程并进行调试。

## 参考资源

本项目参考了 [LiYoumu/nginx-cmake](https://github.com/LiYoumu/nginx-cmake) 项目的实现方式，在此基础上进行了一些改进和调整。

## 特性

- 使用CMake构建系统，简化Windows平台下的编译过程
- 自动处理PCRE、ZLIB和OpenSSL依赖
- 已在conf/nginx.conf中添加了`daemon off;`和`master_process off;`配置，便于调试
- 自动创建Nginx运行所需的临时目录和日志文件夹

## 支持平台

目前仅在Windows平台下测试通过，推荐使用Visual Studio 2019及以上版本。

## 构建步骤

1. 克隆仓库
```bash
git clone https://github.com/your-username/nginx-cmake.git
cd nginx-cmake
```

2. 创建构建目录
```bash
mkdir build
cd build
```

3. 配置项目
```bash
cmake .. -G "Visual Studio 16 2019" -A x64
```

4. 构建项目
```bash
cmake --build . --config Release
```
或者直接在Visual Studio中打开生成的解决方案文件并构建。

## 调试说明

由于在conf/nginx.conf中已添加了`daemon off;`和`master_process off;`配置，可以直接在Visual Studio中设置nginx为启动项目，然后启动调试。

## 目录结构
```text
nginx-cmake/
├── CMakeLists.txt # 主CMake配置文件
├── cmake/ # CMake模块和工具
│ ├── CMakeLists.txt
│ └── auto_group_files.cmake
├── src/ # 源代码
│ └── CMakeLists.txt # src目录的构建配置
├── third_party/ # 第三方依赖库
│ ├── pcre/
│ ├── zlib/
│ ├── openssl-3.4/
│ └── CMakeLists.txt # 第三方库配置
└── conf/ # 配置文件目录
└── nginx.conf # 已配置daemon off和master_process off

```

## 相关链接

- [NGINX官方网站](https://nginx.org/)
- [LiYoumu/nginx-cmake](https://github.com/LiYoumu/nginx-cmake)