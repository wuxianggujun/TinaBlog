# 在文件开头添加提示，建议使用Visual Studio生成器
# 提示：推荐使用Visual Studio生成器而不是Ninja，以便正确处理Debug/Release配置
# 示例：cmake -G "Visual Studio 17 2022" -A x64 ..

cmake_minimum_required(VERSION 3.20)

project(nginx LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 设置cmake模块路径
list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

# 包含CPM.cmake依赖管理工具
include(cmake/CPM.cmake)

# 设置项目选项
option(BUILD_TESTS "构建测试程序" OFF)
option(BUILD_MYSQL_CONNECTOR "从源码构建MySQL Connector/C++" OFF)
option(MYSQL_DISABLE "禁用MySQL功能" OFF)

# 使用统一的第三方库配置
# ThirdPartyLibs.cmake中已经包含了对configure_third_party_libs的调用
include(cmake/ThirdPartyLibs.cmake)

# 使用CPM添加本地Boost依赖
CPMAddPackage(
  NAME Boost
  VERSION 1.87.0
  URL ${CMAKE_SOURCE_DIR}/third_party/boost/boost-1.87.0-cmake.tar.xz
  URL_HASH SHA256=7da75f171837577a52bbf217e17f8ea576c7c246e4594d617bfde7fafd408be5
  OPTIONS "BOOST_ENABLE_CMAKE ON" "BOOST_SKIP_INSTALL_RULES ON"
          "BUILD_SHARED_LIBS OFF" "BOOST_INCLUDE_LIBRARIES container\\\;exception\\\;utility\\\;tti"
)

# 启用文件夹分组
set_property(GLOBAL PROPERTY USE_FOLDERS ON)

# 设置必要的目录结构
set(NGINX_ROOT_DIR ${CMAKE_SOURCE_DIR})

# 设置nginx工作目录
set(NGINX_INSTALL_DIR ${CMAKE_BINARY_DIR}/dist)
set(NGINX_CONF_DIR ${NGINX_INSTALL_DIR}/conf)
set(NGINX_LOGS_DIR ${NGINX_INSTALL_DIR}/logs)
set(NGINX_TEMP_DIR ${NGINX_INSTALL_DIR}/temp)

# 包含自定义cmake模块
include(auto_group_files)

if(WIN32)
    add_compile_options(/utf-8)  # Windows 强制 UTF-8
else()
    add_compile_options(-Wall -Wpedantic)  # Linux/Mac 使用无 BOM UTF-8
endif()

# 添加全局定义
add_compile_definitions(
    $<$<BOOL:${NGX_HAVE_PCRE}>:NGX_HAVE_PCRE=1>
    $<$<BOOL:${NGX_HAVE_ZLIB}>:NGX_HAVE_ZLIB=1>
    $<$<BOOL:${NGX_HAVE_OPENSSL}>:NGX_HAVE_OPENSSL=1>
)

# 显示MySQL库信息
message(STATUS "MySQL头文件目录: ${MySQL_INCLUDE_DIRS}")
message(STATUS "MySQL库文件: ${MySQL_LIBRARIES}")
message(STATUS "MySQL是否找到: ${MYSQL_FOUND}")

# 添加src作为子目录进行构建
add_subdirectory(src)

# 创建运行时目录
foreach(DIR ${NGINX_CONF_DIR} ${NGINX_LOGS_DIR} ${NGINX_TEMP_DIR})
    if(NOT EXISTS ${DIR})
        file(MAKE_DIRECTORY ${DIR})
    endif()
endforeach()

# 添加编译选项
if(MSVC)
    target_compile_definitions(${PROJECT_NAME} PRIVATE NOMINMAX)
    # 直接设置/MD或/MDd
    target_compile_options(${PROJECT_NAME} PRIVATE
        $<$<CONFIG:Debug>:/MDd>
        $<$<NOT:$<CONFIG:Debug>>:/MD>
    )
endif()

# 输出构建信息
message(STATUS "构建TinaBlog - 高性能博客引擎")
message(STATUS "使用的编译器: ${CMAKE_CXX_COMPILER_ID} ${CMAKE_CXX_COMPILER_VERSION}")
message(STATUS "构建类型: ${CMAKE_BUILD_TYPE}")
message(STATUS "构建测试: ${BUILD_TESTS}")
message(STATUS "从源码构建MySQL: ${BUILD_MYSQL_CONNECTOR}")
message(STATUS "禁用MySQL: ${MYSQL_DISABLE}")
message(STATUS "使用PCRE: ${NGX_HAVE_PCRE}")
message(STATUS "使用ZLIB: ${NGX_HAVE_ZLIB}")
message(STATUS "使用OpenSSL: ${NGX_HAVE_OPENSSL}")

# 提示如何运行
message(STATUS "配置完成。")
