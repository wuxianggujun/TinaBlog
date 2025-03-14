# ThirdPartyLibs.cmake - 统一的第三方库配置
# ===============================================================
# 该文件将负责所有第三方库的配置和集成
# 包括PCRE、ZLIB、OpenSSL、MySQL Connector C++等

include(Utilities)

# 全局配置函数
function(configure_global_build_options)
    # 设置全局变量 - 共享库路径设置
    set(THIRD_PARTY_DIR "${CMAKE_SOURCE_DIR}/third_party" PARENT_SCOPE)

    # 设置全局编译选项以确保一致性
    if(MSVC)
        # 统一所有库使用动态运行时库/MD，匹配OpenSSL的编译方式
        foreach(flag_var
            CMAKE_C_FLAGS CMAKE_C_FLAGS_DEBUG CMAKE_C_FLAGS_RELEASE
            CMAKE_CXX_FLAGS CMAKE_CXX_FLAGS_DEBUG CMAKE_CXX_FLAGS_RELEASE)
            string(REGEX REPLACE "/MT" "/MD" ${flag_var} "${${flag_var}}")
            set(${flag_var} "${${flag_var}}" PARENT_SCOPE)
        endforeach()
        
        # 确保在Debug模式中使用/MDd
        set(CMAKE_C_FLAGS_DEBUG "${CMAKE_C_FLAGS_DEBUG} /MDd" PARENT_SCOPE)
        set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} /MDd" PARENT_SCOPE)
    endif()
endfunction()

# PCRE 配置函数
function(configure_pcre)
    # 选项控制是否禁用PCRE
    option(PCRE_DISABLE "禁用PCRE功能" OFF)
    
    # 如果PCRE功能已禁用，直接返回
    if(PCRE_DISABLE)
        message(STATUS "PCRE功能已被禁用")
        set(NGX_HAVE_PCRE OFF CACHE BOOL "Enable PCRE support" FORCE)
        return()
    endif()

    # 临时保存原来的BUILD_SHARED_LIBS设置
    set(_SAVED_BUILD_SHARED_LIBS ${BUILD_SHARED_LIBS})
    # 强制设置为静态库构建，确保不会生成DLL
    set(BUILD_SHARED_LIBS OFF CACHE BOOL "Build shared libraries" FORCE)

    # 设置PCRE源码目录路径
    set(PCRE_DIR "${CMAKE_SOURCE_DIR}/third_party/pcre" CACHE PATH "PCRE源码目录路径")
    set(PCRE_DIR ${PCRE_DIR} PARENT_SCOPE)
    
    # 检查源码目录是否存在
    if(NOT EXISTS "${PCRE_DIR}")
        message(WARNING "PCRE源码目录不存在: ${PCRE_DIR}")
        message(STATUS "请在CMake GUI中设置正确的PCRE_DIR路径或使用命令行参数 -DPCRE_DIR=\"路径\"")
        set(NGX_HAVE_PCRE OFF CACHE BOOL "Enable PCRE support" FORCE)
        return()
    endif()

    # 检查是否有CMakeLists.txt文件，表明可以构建
    if(NOT EXISTS "${PCRE_DIR}/CMakeLists.txt")
        message(WARNING "PCRE源码目录中没有CMakeLists.txt文件: ${PCRE_DIR}")
        message(STATUS "请确保PCRE源码是完整的，且包含CMakeLists.txt文件")
        set(NGX_HAVE_PCRE OFF CACHE BOOL "Enable PCRE support" FORCE)
        return()
    endif()
    
    # 设置包含目录
    set(PCRE_INCLUDE_DIRS "${PCRE_DIR}/include" CACHE PATH "PCRE include directory")
    
    # 尝试从源码构建PCRE
    if(NOT TARGET pcre)
        message(STATUS "尝试从源码构建PCRE静态库...")
        
        # 配置PCRE构建选项 - 确保所有与静态/动态库相关的选项都正确设置
        set(PCRE_STATIC_RUNTIME OFF CACHE BOOL "" FORCE)
        set(PCRE_BUILD_TESTS OFF CACHE BOOL "" FORCE)
        set(PCRE_BUILD_PCREGREP OFF CACHE BOOL "" FORCE)
        set(PCRE_BUILD_PCRECPP OFF CACHE BOOL "" FORCE)
        set(PCRE_STATIC ON CACHE BOOL "" FORCE)  # 强制为静态库
        set(PCRE_BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)  # 明确禁用共享库
        set(PCRE_SUPPORT_UTF ON CACHE BOOL "" FORCE)
        set(PCRE_SUPPORT_UNICODE_PROPERTIES ON CACHE BOOL "" FORCE)
        set(PCRE_BUILD_PCRE8 ON CACHE BOOL "" FORCE)
        set(PCRE_BUILD_PCRE16 OFF CACHE BOOL "" FORCE)
        set(PCRE_BUILD_PCRE32 OFF CACHE BOOL "" FORCE)
        
        # 清理可能存在的之前构建的文件
        file(REMOVE_RECURSE "${CMAKE_BINARY_DIR}/third_party/pcre")
        
        # 添加PCRE子项目
        add_subdirectory(${PCRE_DIR} ${CMAKE_BINARY_DIR}/third_party/pcre)
        
        # 检查是否成功创建目标
        if(TARGET pcre)
            message(STATUS "成功创建PCRE目标")
            
            # 确保我们链接到静态库，但不设置IMPORTED_GLOBAL属性（因为这不是导入目标）
            if(MSVC)
                set_target_properties(pcre PROPERTIES 
                    STATIC_LIBRARY_FLAGS "/IGNORE:4099"
                )
            endif()
            
            # 使用生成器表达式获取正确的库文件路径
            set(PCRE_LIBRARIES_DEBUG "$<TARGET_FILE:pcre>" CACHE FILEPATH "PCRE debug library" FORCE)
            set(PCRE_LIBRARIES_RELEASE "$<TARGET_FILE:pcre>" CACHE FILEPATH "PCRE release library" FORCE)
            set(PCRE_LIBRARIES "$<TARGET_FILE:pcre>" CACHE FILEPATH "PCRE libraries" FORCE)
            set(NGX_HAVE_PCRE ON CACHE BOOL "Enable PCRE support" FORCE)
        else()
            message(WARNING "从源码构建PCRE失败，未能创建pcre目标")
            set(NGX_HAVE_PCRE OFF CACHE BOOL "Enable PCRE support" FORCE)
        endif()
    else()
        message(STATUS "PCRE目标已存在，直接使用")
        
        # 确保我们链接到静态库，但不设置IMPORTED_GLOBAL属性（因为这不是导入目标）
        if(MSVC)
            set_target_properties(pcre PROPERTIES 
                STATIC_LIBRARY_FLAGS "/IGNORE:4099"
            )
        endif()
        
        set(PCRE_LIBRARIES_DEBUG "$<TARGET_FILE:pcre>" CACHE FILEPATH "PCRE debug library" FORCE)
        set(PCRE_LIBRARIES_RELEASE "$<TARGET_FILE:pcre>" CACHE FILEPATH "PCRE release library" FORCE)
        set(PCRE_LIBRARIES "$<TARGET_FILE:pcre>" CACHE FILEPATH "PCRE libraries" FORCE)
        set(NGX_HAVE_PCRE ON CACHE BOOL "Enable PCRE support" FORCE)
    endif()
    
    # 导出变量到父作用域
    set(PCRE_INCLUDE_DIRS ${PCRE_INCLUDE_DIRS} PARENT_SCOPE)
    set(PCRE_LIBRARIES_DEBUG ${PCRE_LIBRARIES_DEBUG} PARENT_SCOPE)
    set(PCRE_LIBRARIES_RELEASE ${PCRE_LIBRARIES_RELEASE} PARENT_SCOPE)
    set(PCRE_LIBRARIES ${PCRE_LIBRARIES} PARENT_SCOPE)
    set(NGX_HAVE_PCRE ${NGX_HAVE_PCRE} PARENT_SCOPE)
    
    # 恢复原来的BUILD_SHARED_LIBS设置
    set(BUILD_SHARED_LIBS ${_SAVED_BUILD_SHARED_LIBS} CACHE BOOL "Build shared libraries" FORCE)
endfunction()

# ZLIB 配置函数
function(configure_zlib)
    # 确保为ZLIB动态库构建设置全局变量
    set(BUILD_SHARED_LIBS ON CACHE BOOL "Build shared libraries" FORCE)
    set(SKIP_INSTALL_ALL ON CACHE BOOL "" FORCE)
    
    set(ZLIB_DIR "${CMAKE_SOURCE_DIR}/third_party/zlib")
    set(ZLIB_DIR ${ZLIB_DIR} PARENT_SCOPE)

    # 设置包含目录
    set(ZLIB_INCLUDE_DIRS 
        ${ZLIB_DIR}
        ${CMAKE_BINARY_DIR}/third_party/zlib
        CACHE PATH "ZLIB include directories"
    )

    # 强制设置为动态库构建
    set(ZLIB_BUILD_SHARED_LIBS ON CACHE BOOL "Build shared library" FORCE)
    
    # 检查ZLIB源码是否存在并且目标尚未定义
    if(EXISTS "${ZLIB_DIR}/CMakeLists.txt" AND NOT TARGET zlib AND NOT TARGET zlibstatic)
        message(STATUS "从源码开始构建ZLIB动态库...")
        
        # 使用唯一的二进制目录，避免冲突
        set(ZLIB_BINARY_DIR "${CMAKE_BINARY_DIR}/third_party/_zlib_build")
        file(MAKE_DIRECTORY ${ZLIB_BINARY_DIR})
        
        # 添加ZLIB子项目
        add_subdirectory(${ZLIB_DIR} ${ZLIB_BINARY_DIR})
        
        # 确认动态库是否成功构建
        if(TARGET zlib)
            message(STATUS "ZLIB动态库目标已创建成功")
            set(ZLIB_LIBRARIES zlib CACHE STRING "ZLIB libraries" FORCE)
        else()
            message(WARNING "未成功创建ZLIB动态库目标，请检查ZLIB源码")
        endif()
    else()
        if(TARGET zlib)
            message(STATUS "ZLIB动态库目标已存在，直接使用")
            set(ZLIB_LIBRARIES zlib CACHE STRING "ZLIB libraries" FORCE)
        else()
            message(WARNING "ZLIB源码目录有问题或目标已被定义但不是动态库")
        endif()
    endif()

    # 为其他组件设置ZLIB变量
    set(ZLIB_FOUND TRUE CACHE BOOL "ZLIB has been found")
    set(ZLIB_INCLUDE_DIR ${ZLIB_INCLUDE_DIRS} CACHE PATH "ZLIB include directory")
    if(TARGET zlib)
        set(ZLIB_LIBRARY "$<TARGET_FILE:zlib>" CACHE PATH "ZLIB library")
    endif()

    set(NGX_HAVE_ZLIB ON CACHE BOOL "Enable ZLIB support")
    
    # 导出变量到父作用域
    set(ZLIB_INCLUDE_DIRS ${ZLIB_INCLUDE_DIRS} PARENT_SCOPE)
    set(ZLIB_LIBRARIES ${ZLIB_LIBRARIES} PARENT_SCOPE)
    set(ZLIB_INCLUDE_DIR ${ZLIB_INCLUDE_DIR} PARENT_SCOPE)
    set(ZLIB_LIBRARY ${ZLIB_LIBRARY} PARENT_SCOPE)
    set(ZLIB_FOUND ${ZLIB_FOUND} PARENT_SCOPE)
    set(NGX_HAVE_ZLIB ${NGX_HAVE_ZLIB} PARENT_SCOPE)
endfunction()

# OpenSSL 配置函数
function(configure_openssl)
    set(OPENSSL_DIR "${CMAKE_SOURCE_DIR}/third_party/openssl-3.4")
    set(OPENSSL_DIR ${OPENSSL_DIR} PARENT_SCOPE)

    # 设置OpenSSL变量
    set(OPENSSL_INCLUDE_DIRS "${OPENSSL_DIR}/include" CACHE PATH "OpenSSL include directory")
    set(OPENSSL_LIBRARIES 
        "${OPENSSL_DIR}/lib/libssl.lib"
        "${OPENSSL_DIR}/lib/libcrypto.lib"
        CACHE PATH "OpenSSL libraries"
    )
    
    # 检查是否有MySQL Connector提供的OpenSSL DLL
    if(DEFINED MYSQL_CONNECTOR_DIR)
        # 设置OpenSSL DLL搜索路径 - 仅保留实际存在的路径
        set(OPENSSL_DLL_PATHS
            # MySQL Connector中的OpenSSL DLL
            "${MYSQL_CONNECTOR_DIR}/lib64/libcrypto-3-x64.dll"
            "${MYSQL_CONNECTOR_DIR}/lib64/libssl-3-x64.dll"
        )

        # 设置OpenSSL DLL路径
        foreach(DLL_PATH ${OPENSSL_DLL_PATHS})
            file(TO_CMAKE_PATH "${DLL_PATH}" DLL_PATH_CMAKE)
            if(EXISTS "${DLL_PATH_CMAKE}")
                if(DLL_PATH_CMAKE MATCHES "libcrypto")
                    set(OPENSSL_CRYPTO_DLL "${DLL_PATH_CMAKE}" CACHE FILEPATH "OpenSSL libcrypto DLL文件路径" FORCE)
                    message(STATUS "找到OpenSSL Crypto DLL: ${DLL_PATH_CMAKE}")
                elseif(DLL_PATH_CMAKE MATCHES "libssl")
                    set(OPENSSL_SSL_DLL "${DLL_PATH_CMAKE}" CACHE FILEPATH "OpenSSL libssl DLL文件路径" FORCE)
                    message(STATUS "找到OpenSSL SSL DLL: ${DLL_PATH_CMAKE}")
                endif()
            endif()
        endforeach()
    endif()

    # 为了确保其他组件能够找到OpenSSL，手动设置相关变量
    set(OPENSSL_FOUND TRUE CACHE BOOL "OpenSSL found")
    set(OPENSSL_INCLUDE_DIR ${OPENSSL_INCLUDE_DIRS} CACHE PATH "OpenSSL include directory")
    set(OPENSSL_SSL_LIBRARY "${OPENSSL_DIR}/lib/libssl.lib" CACHE PATH "OpenSSL SSL library")
    set(OPENSSL_CRYPTO_LIBRARY "${OPENSSL_DIR}/lib/libcrypto.lib" CACHE PATH "OpenSSL Crypto library")
    set(OPENSSL_ROOT_DIR ${OPENSSL_DIR} CACHE PATH "OpenSSL root directory")
    set(WITH_SSL ${OPENSSL_ROOT_DIR} CACHE PATH "SSL directory")

    set(NGX_HAVE_OPENSSL ON CACHE BOOL "Enable OpenSSL support")
    
    # 导出变量到父作用域
    set(OPENSSL_INCLUDE_DIRS ${OPENSSL_INCLUDE_DIRS} PARENT_SCOPE)
    set(OPENSSL_LIBRARIES ${OPENSSL_LIBRARIES} PARENT_SCOPE)
    set(OPENSSL_FOUND ${OPENSSL_FOUND} PARENT_SCOPE)
    set(OPENSSL_INCLUDE_DIR ${OPENSSL_INCLUDE_DIR} PARENT_SCOPE)
    set(OPENSSL_SSL_LIBRARY ${OPENSSL_SSL_LIBRARY} PARENT_SCOPE)
    set(OPENSSL_CRYPTO_LIBRARY ${OPENSSL_CRYPTO_LIBRARY} PARENT_SCOPE)
    set(OPENSSL_ROOT_DIR ${OPENSSL_ROOT_DIR} PARENT_SCOPE)
    set(WITH_SSL ${WITH_SSL} PARENT_SCOPE)
    set(NGX_HAVE_OPENSSL ${NGX_HAVE_OPENSSL} PARENT_SCOPE)
    set(OPENSSL_CRYPTO_DLL ${OPENSSL_CRYPTO_DLL} PARENT_SCOPE)
    set(OPENSSL_SSL_DLL ${OPENSSL_SSL_DLL} PARENT_SCOPE)
endfunction()

# MySQL 配置函数 - 包括Server和Connector
function(configure_mysql)
    # 设置MySQL相关路径选项 - 明确区分不同目录的用途
    set(MYSQL_SERVER_DIR "D:/Program Files/MySQL/MySQL Server 9.2" CACHE PATH "MySQL Server安装路径")
    set(MYSQL_CONNECTOR_DIR "${CMAKE_SOURCE_DIR}/third_party/mysql-connector-c++-9.2.0-winx64" CACHE PATH "MySQL Connector预编译库路径")
    
    # 导出目录变量到父作用域
    set(MYSQL_SERVER_DIR ${MYSQL_SERVER_DIR} PARENT_SCOPE)
    set(MYSQL_CONNECTOR_DIR ${MYSQL_CONNECTOR_DIR} PARENT_SCOPE)
    
    # 选项控制是否禁用MySQL
    option(MYSQL_DISABLE "禁用MySQL功能" OFF)
    
    # 如果MySQL功能已禁用，直接返回
    if(MYSQL_DISABLE)
        message(STATUS "MySQL功能已被禁用")
        add_definitions(-DHAVE_MYSQL=0)
        return()
    endif()
    
    # 设置MySQL Server相关变量
    set(MySQL_INCLUDE_DIRS 
        "${MYSQL_SERVER_DIR}/include"           # MySQL Server头文件
        "${MYSQL_CONNECTOR_DIR}/include"        # MySQL Connector头文件
        CACHE PATH "MySQL包含目录"
    )
    
    # 设置MySQL Server的库文件路径
    set(MySQL_LIBRARIES 
        "${MYSQL_SERVER_DIR}/lib/libmysql.lib"  # MySQL Server库文件
        CACHE PATH "MySQL库文件"
    )
    
    # 检查MySQL Server DLL
    set(MYSQL_DLL_PATH "${MYSQL_SERVER_DIR}/lib/libmysql.dll")
    if(EXISTS "${MYSQL_DLL_PATH}")
        set(MySQL_DLL "${MYSQL_DLL_PATH}" CACHE PATH "MySQL DLL文件" FORCE)
        message(STATUS "找到MySQL DLL文件: ${MYSQL_DLL_PATH}")
    else()
        message(WARNING "MySQL DLL文件不存在: ${MYSQL_DLL_PATH}")
    endif()
    
    # 检查MySQL Connector C++ DLL
    set(POSSIBLE_CONNECTOR_DLL_PATHS
        "${MYSQL_CONNECTOR_DIR}/lib64/mysqlcppconn-10-vs14.dll"
    )
    
    foreach(DLL_PATH ${POSSIBLE_CONNECTOR_DLL_PATHS})
        file(TO_CMAKE_PATH "${DLL_PATH}" DLL_PATH_CMAKE)
        if(EXISTS "${DLL_PATH_CMAKE}")
            set(MySQL_CONNECTOR_DLL "${DLL_PATH_CMAKE}" CACHE PATH "MySQL Connector/C++ DLL" FORCE)
            message(STATUS "找到MySQL Connector C++ DLL文件: ${DLL_PATH_CMAKE}")
            set(COPY_MYSQL_CONNECTOR_DLL ON CACHE BOOL "复制MySQL Connector DLL到输出目录" FORCE)
            break()
        endif()
    endforeach()
    
    # 检查MySQL头文件和库是否存在
    # 修复检查：分别检查每个路径而不是整个列表
    set(MYSQL_HEADER_FOUND TRUE)
    set(MYSQL_SERVER_INCLUDE "${MYSQL_SERVER_DIR}/include")
    set(MYSQL_CONNECTOR_INCLUDE "${MYSQL_CONNECTOR_DIR}/include")
    
    if(NOT EXISTS "${MYSQL_SERVER_INCLUDE}")
        message(WARNING "MySQL Server头文件目录不存在: ${MYSQL_SERVER_INCLUDE}")
        set(MYSQL_HEADER_FOUND FALSE)
    endif()
    
    if(NOT EXISTS "${MYSQL_CONNECTOR_INCLUDE}")
        message(WARNING "MySQL Connector头文件目录不存在: ${MYSQL_CONNECTOR_INCLUDE}")
        set(MYSQL_HEADER_FOUND FALSE)
    endif()
    
    set(MYSQL_LIB_PATH "${MYSQL_SERVER_DIR}/lib/libmysql.lib")
    if(NOT EXISTS "${MYSQL_LIB_PATH}")
        message(WARNING "MySQL库文件不存在: ${MYSQL_LIB_PATH}")
        set(MYSQL_FOUND FALSE CACHE BOOL "MySQL has not been found" FORCE)
    elseif(NOT MYSQL_HEADER_FOUND)
        message(WARNING "MySQL某些头文件目录不存在，请检查路径")
        set(MYSQL_FOUND FALSE CACHE BOOL "MySQL has not been found" FORCE)
    else()
        set(MYSQL_FOUND TRUE CACHE BOOL "MySQL has been found" FORCE)
        add_definitions(-DHAVE_MYSQL=1)
        message(STATUS "MySQL库配置成功")
    endif()
    
    # 导出变量到父作用域
    set(MySQL_INCLUDE_DIRS ${MySQL_INCLUDE_DIRS} PARENT_SCOPE)
    set(MySQL_LIBRARIES ${MySQL_LIBRARIES} PARENT_SCOPE)
    set(MySQL_DLL ${MySQL_DLL} PARENT_SCOPE)
    set(MySQL_CONNECTOR_DLL ${MySQL_CONNECTOR_DLL} PARENT_SCOPE)
    set(COPY_MYSQL_CONNECTOR_DLL ${COPY_MYSQL_CONNECTOR_DLL} PARENT_SCOPE)
    set(MYSQL_FOUND ${MYSQL_FOUND} PARENT_SCOPE)
endfunction()

# 创建一个总的函数来配置和初始化所有第三方库
function(configure_third_party_libs)
    # 配置全局构建选项
    configure_global_build_options()
    
    # 配置各个库
    configure_pcre()
    configure_zlib()
    configure_mysql()  # 必须在OpenSSL之前配置，因为MySQL Connector可能提供OpenSSL DLL
    configure_openssl()
    
    # 打印配置状态
    message(STATUS "===== 第三方库配置完成 =====")
    message(STATUS "PCRE: ${PCRE_DIR}")
    message(STATUS "ZLIB: ${ZLIB_DIR}")
    message(STATUS "OpenSSL: ${OPENSSL_DIR}")
    message(STATUS "MySQL Server: ${MYSQL_SERVER_DIR}")
    message(STATUS "MySQL Connector: ${MYSQL_CONNECTOR_DIR}")
    message(STATUS "============================")
endfunction()

# 立即调用函数设置所有第三方库
configure_third_party_libs() 