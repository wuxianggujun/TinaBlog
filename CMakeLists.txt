cmake_minimum_required(VERSION 3.20)

project(nginx LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)

# 设置cmake模块路径
list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

# 包含CPM.cmake依赖管理工具
include(cmake/CPM.cmake)

# 使用CPM添加本地Boost依赖
CPMAddPackage(
  NAME Boost
  VERSION 1.87.0
  URL ${CMAKE_SOURCE_DIR}/third_party/boost/boost-1.87.0-cmake.tar.xz
  URL_HASH SHA256=7da75f171837577a52bbf217e17f8ea576c7c246e4594d617bfde7fafd408be5
  OPTIONS "BOOST_ENABLE_CMAKE ON" "BOOST_SKIP_INSTALL_RULES ON"
          "BUILD_SHARED_LIBS OFF" "BOOST_INCLUDE_LIBRARIES container\\\;asio"
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

# 设置第三方库路径
set(NGINX_THIRD_PARTY_DIR ${NGINX_ROOT_DIR}/third_party)
set(NGINX_PCRE_DIR ${NGINX_THIRD_PARTY_DIR}/pcre)
set(NGINX_ZLIB_DIR ${NGINX_THIRD_PARTY_DIR}/zlib)
set(NGINX_OPENSSL_DIR ${NGINX_THIRD_PARTY_DIR}/openssl-3.4)

# 将主项目变量传递给子项目
set(PCRE_DIR ${NGINX_PCRE_DIR})
set(ZLIB_DIR ${NGINX_ZLIB_DIR})
set(OPENSSL_DIR ${NGINX_OPENSSL_DIR})

# 包含自定义cmake模块
include(auto_group_files)

# 先添加third_party子目录，让它处理所有的库依赖
add_subdirectory(third_party)

if(WIN32)
    add_compile_options(/utf-8)  # Windows 强制 UTF-8 BOM
else()
    add_compile_options(-Wall -Wpedantic)  # Linux/Mac 使用无 BOM UTF-8
endif()

# 添加全局定义
add_compile_definitions(
    $<$<BOOL:${NGX_HAVE_PCRE}>:NGX_HAVE_PCRE=1>
    $<$<BOOL:${NGX_HAVE_ZLIB}>:NGX_HAVE_ZLIB=1>
    $<$<BOOL:${NGX_HAVE_OPENSSL}>:NGX_HAVE_OPENSSL=1>
)

# 添加src作为子目录进行构建
add_subdirectory(src)

file(GLOB_RECURSE nginx_HEADERS "${NGINX_ROOT_DIR}/src/*.h")

# 创建运行时目录
foreach(DIR ${NGINX_CONF_DIR} ${NGINX_LOGS_DIR} ${NGINX_TEMP_DIR})
    if(NOT EXISTS ${DIR})
        file(MAKE_DIRECTORY ${DIR})
    endif()
endforeach()

# 欢迎页生成
set(WELCOME_PAGE ${NGINX_CONF_DIR}/html/index.html)
if(NOT EXISTS ${WELCOME_PAGE})
    file(MAKE_DIRECTORY ${NGINX_CONF_DIR}/html)
    file(WRITE ${WELCOME_PAGE} "<html><body><h1>Welcome to nginx!</h1></body></html>")
endif()

# 添加编译选项
if(MSVC)
    # 直接设置/MD或/MDd
    target_compile_options(${PROJECT_NAME} PRIVATE
        $<$<CONFIG:Debug>:/MDd>
        $<$<NOT:$<CONFIG:Debug>>:/MD>
    )
endif()
