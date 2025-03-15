# ThirdPartyLibs.cmake - 统一的第三方库配置
# ===============================================================
# 该文件将负责所有第三方库的配置和集成
# 包括PCRE、ZLIB、OpenSSL、MySQL Connector C++等

# 定义一个函数来配置所有第三方库
function(configure_third_party_libs)
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

    # ==========================================================
    # PCRE 配置
    # ==========================================================
    # 临时将BUILD_SHARED_LIBS设置为OFF，确保PCRE以静态库方式构建
    set(_SAVED_BUILD_SHARED_LIBS ${BUILD_SHARED_LIBS})
    set(BUILD_SHARED_LIBS OFF CACHE BOOL "Build shared libraries" FORCE)
    
    set(PCRE_DIR "${CMAKE_SOURCE_DIR}/third_party/pcre")

    # 检查PCRE库是否存在
    set(PCRE_INCLUDE_DIRS ${PCRE_DIR}/include CACHE PATH "PCRE include directory")
    
    # 设置正确的库名称和路径 - 修改为绝对路径确保能找到
    set(PCRE_LIBRARIES_DEBUG "${CMAKE_BINARY_DIR}/third_party/pcre/Debug/pcre${CMAKE_STATIC_LIBRARY_SUFFIX}" CACHE FILEPATH "PCRE debug library")
    set(PCRE_LIBRARIES_RELEASE "${CMAKE_BINARY_DIR}/third_party/pcre/Release/pcre${CMAKE_STATIC_LIBRARY_SUFFIX}" CACHE FILEPATH "PCRE release library")
    
    # 另一种可能的路径
    if(NOT EXISTS "${PCRE_LIBRARIES_DEBUG}")
        set(PCRE_LIBRARIES_DEBUG "${CMAKE_BINARY_DIR}/lib/Debug/pcre${CMAKE_STATIC_LIBRARY_SUFFIX}" CACHE FILEPATH "PCRE debug library" FORCE)
    endif()
    
    if(NOT EXISTS "${PCRE_LIBRARIES_RELEASE}")
        set(PCRE_LIBRARIES_RELEASE "${CMAKE_BINARY_DIR}/lib/Release/pcre${CMAKE_STATIC_LIBRARY_SUFFIX}" CACHE FILEPATH "PCRE release library" FORCE)
    endif()
    
    # 检查是否需要先构建PCRE
    if(NOT EXISTS "${PCRE_LIBRARIES_DEBUG}" OR NOT EXISTS "${PCRE_LIBRARIES_RELEASE}")
        if(EXISTS "${PCRE_DIR}/CMakeLists.txt" AND NOT TARGET pcre)
            # 配置PCRE构建选项
            set(PCRE_STATIC_RUNTIME OFF CACHE BOOL "" FORCE)
            set(PCRE_BUILD_TESTS OFF CACHE BOOL "" FORCE)
            set(PCRE_BUILD_PCREGREP OFF CACHE BOOL "" FORCE)
            set(PCRE_BUILD_PCRECPP OFF CACHE BOOL "" FORCE)
            set(PCRE_STATIC ON CACHE BOOL "" FORCE)
            set(PCRE_BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE) # 确保PCRE以静态库形式构建
            set(PCRE_SUPPORT_UTF ON CACHE BOOL "" FORCE)
            set(PCRE_SUPPORT_UNICODE_PROPERTIES ON CACHE BOOL "" FORCE)
            set(PCRE_BUILD_PCRE8 ON CACHE BOOL "" FORCE)
            set(PCRE_BUILD_PCRE16 OFF CACHE BOOL "" FORCE)
            set(PCRE_BUILD_PCRE32 OFF CACHE BOOL "" FORCE)
            
            # 添加PCRE子项目
            message(STATUS "添加PCRE子项目进行构建...")
            add_subdirectory(${PCRE_DIR} ${CMAKE_BINARY_DIR}/third_party/pcre)
            
            # 设置库路径为目标路径
            set(PCRE_LIBRARIES_DEBUG "$<TARGET_FILE:pcre>" CACHE FILEPATH "PCRE debug library" FORCE)
            set(PCRE_LIBRARIES_RELEASE "$<TARGET_FILE:pcre>" CACHE FILEPATH "PCRE release library" FORCE)
        else()
            message(FATAL_ERROR "PCRE库文件未找到且无法构建PCRE。请检查third_party/pcre目录是否存在且包含CMakeLists.txt文件。")
        endif()
    endif()
    
    # 使用生成器表达式设置正确的库
    set(PCRE_LIBRARIES "$<$<CONFIG:Debug>:${PCRE_LIBRARIES_DEBUG}>$<$<NOT:$<CONFIG:Debug>>:${PCRE_LIBRARIES_RELEASE}>" CACHE STRING "PCRE libraries")
    
    set(NGX_HAVE_PCRE ON CACHE BOOL "Enable PCRE support")
    
    # 恢复BUILD_SHARED_LIBS的原始值
    set(BUILD_SHARED_LIBS ${_SAVED_BUILD_SHARED_LIBS} CACHE BOOL "Build shared libraries" FORCE)

    # ==========================================================
    # ZLIB 配置 - 强制使用动态库
    # ==========================================================
    # 确保为ZLIB动态库构建设置全局变量
    set(BUILD_SHARED_LIBS ON CACHE BOOL "Build shared libraries" FORCE)
    set(SKIP_INSTALL_ALL ON CACHE BOOL "" FORCE)
    
    set(ZLIB_DIR "${CMAKE_SOURCE_DIR}/third_party/zlib")

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

    # ==========================================================
    # OpenSSL 配置
    # ==========================================================
    set(OPENSSL_DIR "${CMAKE_SOURCE_DIR}/third_party/openssl-3.4")

    # 设置OpenSSL变量
    set(OPENSSL_INCLUDE_DIRS "${OPENSSL_DIR}/include" CACHE PATH "OpenSSL include directory")
    set(OPENSSL_LIBRARIES 
        "${OPENSSL_DIR}/lib/libssl.lib"
        "${OPENSSL_DIR}/lib/libcrypto.lib"
        CACHE PATH "OpenSSL libraries"
    )
    
    # 修改MySQL Connector C++ DLL搜索路径 - 仅保留实际存在的文件路径
    set(POSSIBLE_CONNECTOR_DLL_PATHS
        "${MYSQL_CONNECTOR_DIR}/lib64/mysqlcppconn-10-vs14.dll"
        "${MYSQL_CONNECTOR_DIR}/lib64/mysqlcppconnx-2-vs14.dll"
        "${MYSQL_CONNECTOR_DIR}/lib64/vs14/mysqlcppconn.lib"
        "${MYSQL_CONNECTOR_DIR}/lib64/vs14/mysqlcppconnx.lib"
        "${MYSQL_CONNECTOR_DIR}/lib64/vs14/mysqlcppconn-static.lib"
        "${MYSQL_CONNECTOR_DIR}/lib64/vs14/mysqlcppconnx-static.lib"
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

    # 为了确保其他组件能够找到OpenSSL，手动设置相关变量
    set(OPENSSL_FOUND TRUE CACHE BOOL "OpenSSL found")
    set(OPENSSL_INCLUDE_DIR ${OPENSSL_INCLUDE_DIRS} CACHE PATH "OpenSSL include directory")
    set(OPENSSL_SSL_LIBRARY "${OPENSSL_DIR}/lib/libssl.lib" CACHE PATH "OpenSSL SSL library")
    set(OPENSSL_CRYPTO_LIBRARY "${OPENSSL_DIR}/lib/libcrypto.lib" CACHE PATH "OpenSSL Crypto library")
    set(OPENSSL_ROOT_DIR ${OPENSSL_DIR} CACHE PATH "OpenSSL root directory")
    set(WITH_SSL ${OPENSSL_ROOT_DIR} CACHE PATH "SSL directory")

    set(NGX_HAVE_OPENSSL ON CACHE BOOL "Enable OpenSSL support")

    # ==========================================================
    # MySQL Connector/C++ 和 MySQL C API 配置
    # ==========================================================
    # 设置MySQL相关路径选项 - 明确区分不同目录的用途
    set(MYSQL_SERVER_DIR "D:/Program Files/MySQL/MySQL Server 9.2" CACHE PATH "MySQL Server安装路径")
    set(MYSQL_CONNECTOR_DIR "${CMAKE_SOURCE_DIR}/third_party/mysql-connector-c++-9.2.0-winx64" CACHE PATH "MySQL Connector预编译库路径")

    # 选项控制是否从源码构建 - 默认关闭，使用预编译库
    option(BUILD_MYSQL_CONNECTOR "构建MySQL Connector而不是使用预编译的" OFF)

    # 检查是否已设置MYSQL_DISABLE
    option(MYSQL_DISABLE "禁用MySQL功能" OFF)

    # 设置默认MYSQL_FOUND为FALSE
    set(MYSQL_FOUND FALSE CACHE BOOL "MySQL has been found" FORCE)

    if(NOT MYSQL_DISABLE)
        if(BUILD_MYSQL_CONNECTOR)
            message(STATUS "配置MySQL库...")
            
            # 设置MySQL Server的包含目录
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
            
            # 设置DLL搜索路径 - 只保留MySQL Server DLL，移除重复的Connector DLL
            set(POSSIBLE_MYSQL_DLL_PATHS
                # MySQL Server DLL
                "${MYSQL_SERVER_DIR}/lib/libmysql.dll"
            )
            
            # 检查文件是否存在并设置相应变量
            if(EXISTS "${MySQL_INCLUDE_DIRS}")
                set(MYSQL_HEADER_FOUND TRUE)
                message(STATUS "找到MySQL头文件目录")
                
                if(EXISTS "${MySQL_LIBRARIES}")
                    set(MYSQL_FOUND TRUE CACHE BOOL "MySQL has been found" FORCE)
                    add_definitions(-DHAVE_MYSQL=1)
                    message(STATUS "MySQL库配置成功")
                endif()
            endif()
            
            # 检查并设置DLL路径
            foreach(DLL_PATH ${POSSIBLE_MYSQL_DLL_PATHS})
                if(EXISTS "${DLL_PATH}")
                    set(MySQL_DLL "${DLL_PATH}" CACHE PATH "MySQL DLL文件" FORCE)
                    message(STATUS "找到MySQL DLL文件: ${DLL_PATH}")
                    break()
                endif()
            endforeach()
            
            # 检查并设置OpenSSL DLL路径
            foreach(DLL_PATH ${OPENSSL_DLL_PATHS})
                if(EXISTS "${DLL_PATH}")
                    if(DLL_PATH MATCHES "libcrypto")
                        set(OPENSSL_CRYPTO_DLL "${DLL_PATH}" CACHE PATH "OpenSSL Crypto DLL" FORCE)
                        message(STATUS "找到OpenSSL Crypto DLL: ${DLL_PATH}")
                    elseif(DLL_PATH MATCHES "libssl")
                        set(OPENSSL_SSL_DLL "${DLL_PATH}" CACHE PATH "OpenSSL SSL DLL" FORCE)
                        message(STATUS "找到OpenSSL SSL DLL: ${DLL_PATH}")
                    endif()
                endif()
            endforeach()
        endif()
    else()
        message(STATUS "MySQL功能已被禁用")
        add_definitions(-DHAVE_MYSQL=0)
    endif()

    # 将变量传递到父作用域
    set(MYSQL_SERVER_DIR ${MYSQL_SERVER_DIR} PARENT_SCOPE)
    set(MYSQL_CONNECTOR_DIR ${MYSQL_CONNECTOR_DIR} PARENT_SCOPE)
    set(MySQL_INCLUDE_DIRS ${MySQL_INCLUDE_DIRS} PARENT_SCOPE)
    set(MySQL_LIBRARIES ${MySQL_LIBRARIES} PARENT_SCOPE)
    set(MySQL_DLL ${MySQL_DLL} PARENT_SCOPE)
    set(MySQL_CONNECTOR_DLL ${MySQL_CONNECTOR_DLL} PARENT_SCOPE)
    set(COPY_MYSQL_CONNECTOR_DLL ${COPY_MYSQL_CONNECTOR_DLL} PARENT_SCOPE)
    set(MYSQL_FOUND ${MYSQL_FOUND} PARENT_SCOPE)
    set(OPENSSL_CRYPTO_DLL ${OPENSSL_CRYPTO_DLL} PARENT_SCOPE)
    set(OPENSSL_SSL_DLL ${OPENSSL_SSL_DLL} PARENT_SCOPE)

    # 将变量传递到父作用域
    set(PCRE_DIR ${PCRE_DIR} PARENT_SCOPE)
    set(PCRE_INCLUDE_DIRS ${PCRE_INCLUDE_DIRS} PARENT_SCOPE)
    set(PCRE_LIBRARIES_DEBUG ${PCRE_LIBRARIES_DEBUG} PARENT_SCOPE)
    set(PCRE_LIBRARIES_RELEASE ${PCRE_LIBRARIES_RELEASE} PARENT_SCOPE)
    set(PCRE_LIBRARIES ${PCRE_LIBRARIES} PARENT_SCOPE)
    set(NGX_HAVE_PCRE ${NGX_HAVE_PCRE} PARENT_SCOPE)

    set(ZLIB_DIR ${ZLIB_DIR} PARENT_SCOPE)
    set(ZLIB_INCLUDE_DIRS ${ZLIB_INCLUDE_DIRS} PARENT_SCOPE)
    set(ZLIB_LIBRARIES ${ZLIB_LIBRARIES} PARENT_SCOPE)
    set(ZLIB_INCLUDE_DIR ${ZLIB_INCLUDE_DIR} PARENT_SCOPE)
    set(ZLIB_LIBRARY ${ZLIB_LIBRARY} PARENT_SCOPE)
    set(ZLIB_FOUND ${ZLIB_FOUND} PARENT_SCOPE)
    set(NGX_HAVE_ZLIB ${NGX_HAVE_ZLIB} PARENT_SCOPE)

    set(OPENSSL_DIR ${OPENSSL_DIR} PARENT_SCOPE)
    set(OPENSSL_INCLUDE_DIRS ${OPENSSL_INCLUDE_DIRS} PARENT_SCOPE)
    set(OPENSSL_LIBRARIES ${OPENSSL_LIBRARIES} PARENT_SCOPE)
    set(OPENSSL_FOUND ${OPENSSL_FOUND} PARENT_SCOPE)
    set(OPENSSL_INCLUDE_DIR ${OPENSSL_INCLUDE_DIR} PARENT_SCOPE)
    set(OPENSSL_SSL_LIBRARY ${OPENSSL_SSL_LIBRARY} PARENT_SCOPE)
    set(OPENSSL_CRYPTO_LIBRARY ${OPENSSL_CRYPTO_LIBRARY} PARENT_SCOPE)
    set(OPENSSL_ROOT_DIR ${OPENSSL_ROOT_DIR} PARENT_SCOPE)
    set(WITH_SSL ${WITH_SSL} PARENT_SCOPE)
    set(NGX_HAVE_OPENSSL ${NGX_HAVE_OPENSSL} PARENT_SCOPE)

    # 导出所有变量到父作用域
    foreach(var ${EXPORTED_VARS})
        set(${var} ${${var}} PARENT_SCOPE)
    endforeach()
endfunction()

# 立即调用函数设置所有第三方库
configure_third_party_libs() 