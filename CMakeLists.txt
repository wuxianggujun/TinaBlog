cmake_minimum_required(VERSION 3.16)

# 先检查是否已有toolchain文件设置
if(NOT DEFINED CMAKE_TOOLCHAIN_FILE)
    # 尝试从环境变量查找VCPKG_ROOT
    if(DEFINED ENV{VCPKG_ROOT})
        set(VCPKG_ROOT "$ENV{VCPKG_ROOT}")
        message(STATUS "从环境变量中检测到VCPKG_ROOT: ${VCPKG_ROOT}")
    else()
        # 如果环境变量未设置，使用默认路径或常见路径
        if(WIN32)
            # Windows上常见路径
            if(EXISTS "C:/vcpkg")
                set(VCPKG_ROOT "C:/vcpkg")
            elseif(EXISTS "D:/vcpkg")
                set(VCPKG_ROOT "D:/vcpkg")
            elseif(EXISTS "$ENV{USERPROFILE}/vcpkg")
                set(VCPKG_ROOT "$ENV{USERPROFILE}/vcpkg")
            endif()
        else()
            # Linux/macOS常见路径
            if(EXISTS "$ENV{HOME}/vcpkg")
                set(VCPKG_ROOT "$ENV{HOME}/vcpkg")
            elseif(EXISTS "/usr/local/vcpkg")
                set(VCPKG_ROOT "/usr/local/vcpkg")
            endif()
        endif()
        
        if(DEFINED VCPKG_ROOT)
            message(STATUS "使用默认vcpkg路径: ${VCPKG_ROOT}")
        endif()
    endif()
    
    # 设置toolchain文件路径
    if(DEFINED VCPKG_ROOT)
        set(VCPKG_TOOLCHAIN_FILE "${VCPKG_ROOT}/scripts/buildsystems/vcpkg.cmake")
        if(EXISTS "${VCPKG_TOOLCHAIN_FILE}")
            set(CMAKE_TOOLCHAIN_FILE "${VCPKG_TOOLCHAIN_FILE}" CACHE STRING "vcpkg工具链文件路径")
            message(STATUS "vcpkg工具链文件: ${CMAKE_TOOLCHAIN_FILE}")
        else()
            message(FATAL_ERROR "未找到vcpkg工具链文件: ${VCPKG_TOOLCHAIN_FILE}")
        endif()
    else()
        message(FATAL_ERROR "未找到vcpkg路径，请设置VCPKG_ROOT环境变量或在命令行中指定CMAKE_TOOLCHAIN_FILE")
    endif()
endif()

# 项目名称和版本
project(TinaBlog VERSION 0.1.0 LANGUAGES CXX)

# C++标准设置
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# 添加UTF-8编码选项
if(MSVC)
    add_compile_options(/utf-8)
    # 禁用一些警告
    add_compile_options(/wd4819)  # 文件编码警告
    
    # 添加宏定义以解决picojson和MSVC兼容性问题
    add_definitions(-D_SILENCE_CXX17_CODECVT_HEADER_DEPRECATION_WARNING)
    add_definitions(-D_SILENCE_ALL_CXX17_DEPRECATION_WARNINGS)
    add_definitions(-DNOMINMAX)
endif()

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

# 查找依赖包
find_package(Drogon CONFIG REQUIRED)
find_package(libpqxx CONFIG REQUIRED)
find_package(jwt-cpp CONFIG REQUIRED)

# 包含目录
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/src)

# 源文件
file(GLOB_RECURSE SOURCES
    "src/blog/*.cpp"
    "src/blog/auth/*.cpp"
    "src/blog/controllers/*.cpp"
)

# 可执行文件
add_executable(TinaBlog ${SOURCES})

# 链接库
target_link_libraries(TinaBlog PRIVATE
    Drogon::Drogon
    libpqxx::pqxx
    jwt-cpp::jwt-cpp
)

# 安装规则
install(TARGETS TinaBlog DESTINATION bin)

# 复制前端资源文件
file(COPY ${CMAKE_CURRENT_SOURCE_DIR}/html DESTINATION ${CMAKE_CURRENT_BINARY_DIR})

# 输出信息
message(STATUS "TinaBlog配置完成")