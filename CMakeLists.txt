cmake_minimum_required(VERSION 3.20)

# 配置vcpkg支持 - 必须在project()命令之前设置toolchain文件
set(VCPKG_ROOT "D:/Programs/vcpkg" CACHE PATH "vcpkg根目录")
if (EXISTS "${VCPKG_ROOT}")
    message(STATUS "找到vcpkg，正在配置...")
    set(CMAKE_TOOLCHAIN_FILE "${VCPKG_ROOT}/scripts/buildsystems/vcpkg.cmake"
            CACHE STRING "vcpkg工具链文件")
    message(STATUS "vcpkg集成已启用 - 工具链文件: ${CMAKE_TOOLCHAIN_FILE}")
endif ()

project(TinaBlog LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 设置vcpkg的查找路径优先级 
list(APPEND CMAKE_PREFIX_PATH "${VCPKG_ROOT}/installed/${VCPKG_TARGET_TRIPLET}")

# 设置cmake模块路径
list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

# 包含CPM.cmake依赖管理工具
include(cmake/CPM.cmake)

# 启用文件夹分组
set_property(GLOBAL PROPERTY USE_FOLDERS ON)

# 设置必要的目录结构
set(NGINX_ROOT_DIR ${CMAKE_SOURCE_DIR})

# 设置TinaBlog工作目录
set(NGINX_INSTALL_DIR ${CMAKE_BINARY_DIR}/dist)
set(NGINX_CONF_DIR ${NGINX_INSTALL_DIR}/conf)
set(NGINX_LOGS_DIR ${NGINX_INSTALL_DIR}/logs)
set(NGINX_TEMP_DIR ${NGINX_INSTALL_DIR}/temp)

# 包含自定义cmake模块
include(auto_group_files)

if (WIN32)
    add_compile_options(/utf-8)  # Windows 强制 UTF-8
else ()
    add_compile_options(-Wall -Wpedantic)  # Linux/Mac 使用无 BOM UTF-8
endif ()

# 加载统一的依赖管理系统
include(cmake/Dependencies.cmake)  # 包含所有依赖库管理

# 添加全局包含目录
include_directories(
    ${MySQL_INCLUDE_DIRS}           # MySQL Server头文件
    ${MYSQL_CONNECTOR_INCLUDE_DIRS} # MySQL Connector C++头文件
)

# 添加src作为子目录进行构建
add_subdirectory(src)

# 创建运行时目录
foreach (DIR ${NGINX_CONF_DIR} ${NGINX_LOGS_DIR} ${NGINX_TEMP_DIR})
    if (NOT EXISTS ${DIR})
        file(MAKE_DIRECTORY ${DIR})
    endif ()
endforeach ()

# 添加编译选项
if (MSVC)
    target_compile_definitions(${PROJECT_NAME} PRIVATE NOMINMAX)
    # 直接设置/MD或/MDd
    target_compile_options(${PROJECT_NAME} PRIVATE
            $<$<CONFIG:Debug>:/MDd>
            $<$<NOT:$<CONFIG:Debug>>:/MD>
    )
endif ()

# 输出构建信息
message(STATUS "构建TinaBlog - 高性能博客引擎")
message(STATUS "使用的编译器: ${CMAKE_CXX_COMPILER_ID} ${CMAKE_CXX_COMPILER_VERSION}")
message(STATUS "构建类型: ${CMAKE_BUILD_TYPE}")

message(STATUS "使用vcpkg管理依赖: ${VCPKG_ROOT}")
message(STATUS "vcpkg目标平台: ${VCPKG_TARGET_TRIPLET}")

# 提示如何运行
message(STATUS "配置完成。")
