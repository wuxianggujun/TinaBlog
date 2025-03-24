cmake_minimum_required(VERSION 3.20)

# 配置vcpkg支持 - 必须在project()命令之前设置toolchain文件
set(VCPKG_ROOT "D:/Programs/vcpkg" CACHE PATH "vcpkg根目录")
if (EXISTS "${VCPKG_ROOT}")
    set(CMAKE_TOOLCHAIN_FILE "${VCPKG_ROOT}/scripts/buildsystems/vcpkg.cmake"
            CACHE STRING "vcpkg工具链文件")
    message(STATUS "vcpkg集成已启用 - 工具链文件: ${CMAKE_TOOLCHAIN_FILE}")
else()
    message(FATAL_ERROR "未找到vcpkg! 请设置正确的VCPKG_ROOT路径或安装vcpkg。")
endif ()

project(TinaBlog LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 添加UTF-8编码选项
if(MSVC)
    add_compile_options(/utf-8)
    # 禁用一些警告
    add_compile_options(/wd4819)  # 文件编码警告
endif()

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

# 查找依赖包
find_package(Drogon CONFIG REQUIRED)
find_package(libpqxx CONFIG REQUIRED)
# 添加源文件
set(SOURCES
    src/blog/main.cpp
    src/blog/db/DbManager.cpp
)

# 添加头文件目录
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/src)

# 创建可执行文件
add_executable(${PROJECT_NAME} ${SOURCES})

# 链接库
target_link_libraries(${PROJECT_NAME} PRIVATE Drogon::Drogon)
target_link_libraries(${PROJECT_NAME} PRIVATE libpqxx::pqxx)

# 添加自定义目标来同步静态资源
add_custom_target(copy_static_resources ALL
    COMMAND ${CMAKE_COMMAND} -E copy_directory
    ${CMAKE_SOURCE_DIR}/html/blog/dist
    ${CMAKE_CURRENT_BINARY_DIR}/html/blog/dist
    COMMENT "Copying static resources to build directory"
)

# 确保在主目标之前复制静态资源
add_dependencies(${PROJECT_NAME} copy_static_resources)