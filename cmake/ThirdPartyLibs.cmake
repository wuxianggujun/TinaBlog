# ThirdPartyLibs.cmake - 统一的第三方库配置
# ===============================================================
# 该文件将负责所有第三方库的配置和集成
# 包括OpenSSL等

include(MySQL) # 包含MySQL专用配置文件

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

# PCRE2 配置函数 - 使用vcpkg安装的库
function(configure_pcre)
    # 在主CMakeLists.txt中已经查找过PCRE2
    if(TARGET PCRE2::8BIT)
        message(STATUS "使用PCRE2库")
        set(NGX_HAVE_PCRE ON CACHE BOOL "Enable PCRE support" FORCE)
        set(NGX_PCRE2 ON CACHE BOOL "Use PCRE2 library" FORCE)
    else()
        message(FATAL_ERROR "未找到PCRE2库，请确保已通过vcpkg安装并且find_package(pcre2)成功")
    endif()
    
    # 导出变量到父作用域
    set(NGX_HAVE_PCRE ${NGX_HAVE_PCRE} PARENT_SCOPE)
    set(NGX_PCRE2 ${NGX_PCRE2} PARENT_SCOPE)
endfunction()

# OpenSSL 配置函数 - 只使用vcpkg安装的库
function(configure_openssl)
    # 在主CMakeLists.txt中已经查找过OpenSSL
    if(NOT OpenSSL_FOUND)
        message(FATAL_ERROR "未找到OpenSSL，请确保已通过vcpkg安装并且find_package(OpenSSL)成功")
    endif()
    
    message(STATUS "使用OpenSSL库 - 版本: ${OPENSSL_VERSION}")
    set(NGX_HAVE_OPENSSL ON CACHE BOOL "Enable OpenSSL support" FORCE)
    
    # 尝试查找DLL文件路径
    get_target_property(SSL_DLL_PATH OpenSSL::SSL IMPORTED_LOCATION_RELEASE)
    get_target_property(CRYPTO_DLL_PATH OpenSSL::Crypto IMPORTED_LOCATION_RELEASE)
    
    if(SSL_DLL_PATH)
        set(OPENSSL_SSL_DLL ${SSL_DLL_PATH} PARENT_SCOPE)
        message(STATUS "OpenSSL SSL DLL: ${SSL_DLL_PATH}")
    endif()
    
    if(CRYPTO_DLL_PATH)
        set(OPENSSL_CRYPTO_DLL ${CRYPTO_DLL_PATH} PARENT_SCOPE)
        message(STATUS "OpenSSL Crypto DLL: ${CRYPTO_DLL_PATH}")
    endif()
    
    # 导出变量到父作用域
    set(NGX_HAVE_OPENSSL ${NGX_HAVE_OPENSSL} PARENT_SCOPE)
    set(OPENSSL_CRYPTO_DLL ${OPENSSL_CRYPTO_DLL} PARENT_SCOPE)
    set(OPENSSL_SSL_DLL ${OPENSSL_SSL_DLL} PARENT_SCOPE)
endfunction()

# ZLIB 配置函数 - 使用vcpkg安装的库
function(configure_zlib)
    # 在主CMakeLists.txt中已经查找过ZLIB
    if(NOT ZLIB_FOUND)
        message(FATAL_ERROR "未找到ZLIB，请确保已通过vcpkg安装并且find_package(ZLIB)成功")
    endif()
    
    message(STATUS "使用ZLIB库 - 版本: ${ZLIB_VERSION_STRING}")
    set(NGX_HAVE_ZLIB ON CACHE BOOL "Enable ZLIB support" FORCE)
    
    # 导出变量到父作用域
    set(NGX_HAVE_ZLIB ${NGX_HAVE_ZLIB} PARENT_SCOPE)
endfunction()

# 创建一个总的函数来配置和初始化所有第三方库
function(configure_third_party_libs)
    
    # 配置全局构建选项
    configure_global_build_options()
    
    # 配置各个库
    configure_pcre()
    configure_openssl()
    configure_zlib()
    # MySQL相关配置已移至MySQL.cmake
    
    # 打印配置状态
    message(STATUS "===== 第三方库配置完成 =====")
    message(STATUS "使用vcpkg管理依赖库: ${VCPKG_ROOT}")
    message(STATUS "PCRE2: 使用vcpkg安装版本")
    message(STATUS "OpenSSL: 使用vcpkg安装版本 ${OPENSSL_VERSION}")
    message(STATUS "ZLIB: 使用vcpkg安装版本 ${ZLIB_VERSION_STRING}")
    message(STATUS "============================")
endfunction()

# 立即调用函数设置所有第三方库
configure_third_party_libs() 