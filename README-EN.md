# NGINX CMake Build Project

<div align="right">
  <a href="README.md">English</a> | <a href="README.zh.md">中文</a>
</div>

<picture>
  <source media="(prefers-color-scheme: dark)" srcset="https://github.com/user-attachments/assets/9335b488-ffcc-4157-8364-2370a0b70ad0">
  <source media="(prefers-color-scheme: light)" srcset="https://github.com/user-attachments/assets/3a7eeb08-1133-47f5-859c-fad4f5a6a013">
  <img alt="NGINX Banner">
</picture>

## Project Introduction

This project provides a solution for building and debugging Nginx on Windows platform using CMake. With CMake, you can build and debug the project directly without complex dependency installation and third-party library configuration.

## Reference Resources

This project references the implementation of [LiYoumu/nginx-cmake](https://github.com/LiYoumu/nginx-cmake), with some improvements and adjustments made on this basis.

## Features

- Uses CMake build system to simplify the compilation process on Windows
- Automatically handles PCRE, ZLIB, and OpenSSL dependencies
- Added `daemon off;` and `master_process off;` in conf/nginx.conf for easier debugging
- Automatically creates temporary directories and log folders required for Nginx

## Supported Platforms

Currently only tested on Windows platform, recommended to use Visual Studio 2019 or higher.

## Build Steps

1. Clone the repository
```bash
git clone https://github.com/your-username/nginx-cmake.git
cd nginx-cmake
```

2. Create a build directory
```bash
mkdir build
cd build
```

3. Configure the project
```bash
cmake .. -G "Visual Studio 16 2019" -A x64
```

4. Build the project
```bash
cmake --build . --config Release
```
Or open the generated solution file in Visual Studio and build directly.

## Debugging Notes

Since `daemon off;` and `master_process off;` configurations have been added to conf/nginx.conf, you can set nginx as the startup project in Visual Studio and start debugging directly.

## Directory Structure
```text
nginx-cmake/
├── CMakeLists.txt # Main CMake configuration file
├── cmake/ # CMake modules and tools
│ ├── CMakeLists.txt
│ └── auto_group_files.cmake
├── src/ # Source code
│ └── CMakeLists.txt # Build configuration for src directory
├── third_party/ # Third-party dependencies
│ ├── pcre/
│ ├── zlib/
│ ├── openssl-3.4/
│ └── CMakeLists.txt # Third-party library configuration
└── conf/ # Configuration directory
└── nginx.conf # Configured with daemon off and master_process off

```


## Related Links

- [NGINX Official Website](https://nginx.org/)
- [LiYoumu/nginx-cmake](https://github.com/LiYoumu/nginx-cmake)