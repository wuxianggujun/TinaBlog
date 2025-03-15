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
    
    # 修改MySQL Connector C++ DLL搜索路径
    set(POSSIBLE_CONNECTOR_DLL_PATHS
        "${MYSQL_PREBUILT_DIR}/lib64/vs14/mysqlcppconn-9-vs14.dll"
        "${MYSQL_PREBUILT_DIR}/lib64/vs14/mysqlcppconn.dll"
        "${MYSQL_PREBUILT_DIR}/lib64/mysqlcppconn-9-vs14.dll"
        "${MYSQL_PREBUILT_DIR}/lib64/mysqlcppconn.dll"
        "${MYSQL_PREBUILT_DIR}/lib/vs14/mysqlcppconn-9-vs14.dll"
        "${MYSQL_PREBUILT_DIR}/lib/vs14/mysqlcppconn.dll"
        "${MYSQL_PREBUILT_DIR}/lib/mysqlcppconn-9-vs14.dll"
        "${MYSQL_PREBUILT_DIR}/lib/mysqlcppconn.dll"
        "${MYSQL_PREBUILT_DIR}/bin/mysqlcppconn-9-vs14.dll"
        "${MYSQL_PREBUILT_DIR}/bin/mysqlcppconn.dll"
    )

    foreach(DLL_PATH ${POSSIBLE_CONNECTOR_DLL_PATHS})
        file(TO_CMAKE_PATH "${DLL_PATH}" DLL_PATH_CMAKE)
        if(EXISTS "${DLL_PATH_CMAKE}")
            set(MySQL_DLL "${DLL_PATH_CMAKE}" CACHE PATH "MySQL Connector/C++ DLL" FORCE)
            message(STATUS "找到MySQL Connector C++ DLL文件: ${DLL_PATH_CMAKE}")
            set(COPY_MYSQL_DLL ON CACHE BOOL "复制MySQL DLL到输出目录" FORCE)
            break()
        endif()
    endforeach()

    # 修改OpenSSL DLL搜索路径
    set(OPENSSL_DLL_PATHS
        # 预编译库目录下的OpenSSL DLL
        "${MYSQL_PREBUILT_DIR}/lib64/vs14/libcrypto-3-x64.dll"
        "${MYSQL_PREBUILT_DIR}/lib64/vs14/libssl-3-x64.dll"
        "${MYSQL_PREBUILT_DIR}/lib64/libcrypto-3-x64.dll"
        "${MYSQL_PREBUILT_DIR}/lib64/libssl-3-x64.dll"
        "${MYSQL_PREBUILT_DIR}/lib/vs14/libcrypto-3-x64.dll"
        "${MYSQL_PREBUILT_DIR}/lib/vs14/libssl-3-x64.dll"
        "${MYSQL_PREBUILT_DIR}/lib/libcrypto-3-x64.dll"
        "${MYSQL_PREBUILT_DIR}/lib/libssl-3-x64.dll"
        "${MYSQL_PREBUILT_DIR}/bin/libcrypto-3-x64.dll"
        "${MYSQL_PREBUILT_DIR}/bin/libssl-3-x64.dll"
        
        # 如果预编译库目录没有，再查找其他位置
        "${CMAKE_SOURCE_DIR}/third_party/openssl-3.4/bin/libcrypto-3-x64.dll"
        "${CMAKE_SOURCE_DIR}/third_party/openssl-3.4/bin/libssl-3-x64.dll"
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
    # 设置MySQL相关路径选项
    set(MYSQL_PREBUILT_DIR "${CMAKE_SOURCE_DIR}/third_party/mysql-connector-c++-9.2.0-winx64" CACHE PATH "预编译MySQL Connector/C++路径")
    set(MYSQL_SERVER_DIR "${CMAKE_SOURCE_DIR}/third_party/mysql-connector-c++-9.2.0-winx64" CACHE PATH "MySQL Server安装路径")
    set(MYSQL_CONNECTOR_DIR "${CMAKE_SOURCE_DIR}/third_party/mysql-connector-c++-9.2.0-winx64" CACHE PATH "MySQL Connector C++安装路径")

    # 选项控制是否从源码构建 - 默认关闭，使用预编译库
    option(BUILD_MYSQL_CONNECTOR "构建MySQL Connector而不是使用预编译的" OFF)

    # 检查是否已设置MYSQL_DISABLE
    option(MYSQL_DISABLE "禁用MySQL功能" OFF)

    # 设置默认MYSQL_FOUND为FALSE - 在全局范围设置
    set(MYSQL_FOUND FALSE CACHE BOOL "MySQL has been found" FORCE)

    if(NOT MYSQL_DISABLE)
        if(BUILD_MYSQL_CONNECTOR AND EXISTS "${CMAKE_SOURCE_DIR}/third_party/mysql-connector-cpp/CMakeLists.txt")
            # 保留原有从源码构建的代码
            message(STATUS "从源码构建MySQL Connector/C++")
            # ... 原有从源码构建的代码 ...
        else()
            # 使用预编译的MySQL
            message(STATUS "配置MySQL库...")
            
            # 首先检查MySQL Server安装目录的C API头文件
            set(MYSQL_C_API_INCLUDE_DIR "${MYSQL_SERVER_DIR}/include" CACHE PATH "MySQL C API包含目录" FORCE)
            file(TO_CMAKE_PATH "${MYSQL_C_API_INCLUDE_DIR}" MYSQL_C_API_INCLUDE_DIR_CMAKE)
            
            set(MYSQL_C_API_HEADER "${MYSQL_C_API_INCLUDE_DIR_CMAKE}/mysql/mysql.h")
            
            if(EXISTS "${MYSQL_C_API_HEADER}")
                message(STATUS "找到MySQL C API头文件: ${MYSQL_C_API_HEADER}")
                set(MYSQL_HEADER_FOUND TRUE)
                set(MySQL_INCLUDE_DIR "${MYSQL_C_API_INCLUDE_DIR}" CACHE PATH "MySQL包含目录" FORCE)
                
                # 设置库文件路径 - 首先在MySQL Server目录查找libmysqlclient
                if(MSVC)
                    set(POSSIBLE_C_API_LIB_PATHS
                        "${MYSQL_SERVER_DIR}/lib/libmysql.lib"
                        "${MYSQL_SERVER_DIR}/lib/mysqlclient.lib"
                        "${MYSQL_SERVER_DIR}/lib/libmysqlclient.lib"
                        "${MYSQL_SERVER_DIR}/lib64/libmysql.lib"
                        "${MYSQL_SERVER_DIR}/lib64/mysqlclient.lib"
                        "${MYSQL_SERVER_DIR}/lib64/libmysqlclient.lib"
                    )
                    
                    foreach(LIB_PATH ${POSSIBLE_C_API_LIB_PATHS})
                        file(TO_CMAKE_PATH "${LIB_PATH}" LIB_PATH_CMAKE)
                        if(EXISTS "${LIB_PATH_CMAKE}")
                            set(MySQL_LIBRARIES "${LIB_PATH_CMAKE}" CACHE PATH "MySQL库文件" FORCE)
                            message(STATUS "找到MySQL C API库文件: ${LIB_PATH_CMAKE}")
                            break()
                        endif()
                    endforeach()
                    
                    # 修改DLL路径搜索顺序，优先使用预编译库目录
                    set(POSSIBLE_C_API_DLL_PATHS
                        "${MYSQL_PREBUILT_DIR}/lib64/vs14/libmysql.dll"
                        "${MYSQL_PREBUILT_DIR}/lib64/libmysql.dll"
                        "${MYSQL_PREBUILT_DIR}/lib/vs14/libmysql.dll"
                        "${MYSQL_PREBUILT_DIR}/lib/libmysql.dll"
                        "${MYSQL_PREBUILT_DIR}/bin/libmysql.dll"
                    )
                    
                    foreach(DLL_PATH ${POSSIBLE_C_API_DLL_PATHS})
                        file(TO_CMAKE_PATH "${DLL_PATH}" DLL_PATH_CMAKE)
                        if(EXISTS "${DLL_PATH_CMAKE}")
                            set(MySQL_DLL "${DLL_PATH_CMAKE}" CACHE PATH "MySQL DLL文件" FORCE)
                            message(STATUS "找到MySQL C API DLL文件: ${DLL_PATH_CMAKE}")
                            set(COPY_MYSQL_DLL ON CACHE BOOL "复制MySQL DLL到输出目录" FORCE)
                            break()
                        endif()
                    endforeach()
                endif()
            else()
                # 如果没有找到C API头文件，尝试找Connector/C++的头文件
                message(STATUS "未找到MySQL C API头文件，尝试使用MySQL Connector/C++...")
                
                # 首先尝试从项目中的预编译库目录查找
                set(FOUND_IN_PREBUILT FALSE)
                if(EXISTS "${MYSQL_PREBUILT_DIR}")
                    message(STATUS "检查预编译库目录: ${MYSQL_PREBUILT_DIR}")
                    
                    # 设置包含目录
                    set(MySQL_INCLUDE_DIR "${MYSQL_PREBUILT_DIR}/include" CACHE PATH "MySQL包含目录" FORCE)
                    file(TO_CMAKE_PATH "${MySQL_INCLUDE_DIR}" MySQL_INCLUDE_DIR_CMAKE)
                    
                    # 检查头文件是否存在
                    set(JDBC_API_HEADER "${MySQL_INCLUDE_DIR_CMAKE}/jdbc/mysql_connection.h")
                    
                    if(EXISTS "${JDBC_API_HEADER}")
                        message(STATUS "在预编译库中找到MySQL JDBC API头文件")
                        set(MYSQL_HEADER_FOUND TRUE)
                        set(FOUND_IN_PREBUILT TRUE)
                        
                        # 设置库文件路径 - 检查不同可能的库文件名
                        if(MSVC)
                            set(POSSIBLE_LIB_NAMES 
                                "mysqlcppconn.lib"
                                "mysqlcppconn-static.lib"
                                "mysqlcppconn8.lib"
                            )
                            
                            foreach(LIB_NAME ${POSSIBLE_LIB_NAMES})
                                set(LIB_PATH "${MYSQL_PREBUILT_DIR}/lib64/vs14/${LIB_NAME}")
                                if(EXISTS "${LIB_PATH}")
                                    set(MySQL_LIBRARIES "${LIB_PATH}" CACHE PATH "MySQL Connector/C++ libraries" FORCE)
                                    message(STATUS "找到MySQL库文件: ${LIB_PATH}")
                                    break()
                                endif()
                            endforeach()
                            
                            # 修改DLL路径搜索顺序，优先使用预编译库目录
                            set(POSSIBLE_C_API_DLL_PATHS
                                "${MYSQL_PREBUILT_DIR}/lib64/vs14/libmysql.dll"
                                "${MYSQL_PREBUILT_DIR}/lib64/libmysql.dll"
                                "${MYSQL_PREBUILT_DIR}/lib/vs14/libmysql.dll"
                                "${MYSQL_PREBUILT_DIR}/lib/libmysql.dll"
                                "${MYSQL_PREBUILT_DIR}/bin/libmysql.dll"
                            )
                            
                            foreach(DLL_PATH ${POSSIBLE_C_API_DLL_PATHS})
                                file(TO_CMAKE_PATH "${DLL_PATH}" DLL_PATH_CMAKE)
                                if(EXISTS "${DLL_PATH_CMAKE}")
                                    set(MySQL_DLL "${DLL_PATH_CMAKE}" CACHE PATH "MySQL Connector/C++ DLL" FORCE)
                                    message(STATUS "找到MySQL DLL文件: ${DLL_PATH_CMAKE}")
                                    set(COPY_MYSQL_DLL ON CACHE BOOL "复制MySQL DLL到输出目录" FORCE)
                                    break()
                                endif()
                            endforeach()
                        endif()
                    endif()
                endif()
                
                # 如果项目中没有找到预编译库，则尝试系统安装的MySQL
                if(NOT FOUND_IN_PREBUILT)
                    message(STATUS "在系统安装的MySQL目录查找库文件")
                    
                    # 首先尝试Connector目录的头文件
                    set(MySQL_INCLUDE_DIR "${MYSQL_CONNECTOR_DIR}/include" CACHE PATH "MySQL包含目录" FORCE)
                    file(TO_CMAKE_PATH "${MySQL_INCLUDE_DIR}" MySQL_INCLUDE_DIR_CMAKE)
                    
                    # 检查头文件是否存在
                    set(JDBC_API_HEADER "${MySQL_INCLUDE_DIR_CMAKE}/jdbc/mysql_connection.h")
                    
                    if(NOT EXISTS "${JDBC_API_HEADER}")
                        # 如果Connector目录没有头文件，尝试Server目录
                        set(MySQL_INCLUDE_DIR "${MYSQL_SERVER_DIR}/include" CACHE PATH "MySQL包含目录" FORCE)
                        file(TO_CMAKE_PATH "${MySQL_INCLUDE_DIR}" MySQL_INCLUDE_DIR_CMAKE)
                        set(JDBC_API_HEADER "${MySQL_INCLUDE_DIR_CMAKE}/jdbc/mysql_connection.h")
                    endif()
                    
                    if(EXISTS "${JDBC_API_HEADER}")
                        message(STATUS "找到MySQL JDBC API头文件: ${JDBC_API_HEADER}")
                        set(MYSQL_HEADER_FOUND TRUE)
                        
                        # 设置库文件路径 - 首先在Connector目录查找
                        if(MSVC)
                            set(POSSIBLE_LIB_PATHS
                                "${MYSQL_CONNECTOR_DIR}/lib64/vs14/mysqlcppconn.lib"
                                "${MYSQL_CONNECTOR_DIR}/lib64/mysqlcppconn.lib"
                                "${MYSQL_CONNECTOR_DIR}/lib/vs14/mysqlcppconn.lib"
                                "${MYSQL_CONNECTOR_DIR}/lib/mysqlcppconn.lib"
                                "${MYSQL_SERVER_DIR}/lib64/vs14/mysqlcppconn.lib"
                                "${MYSQL_SERVER_DIR}/lib64/mysqlcppconn.lib"
                                "${MYSQL_SERVER_DIR}/lib/vs14/mysqlcppconn.lib"
                                "${MYSQL_SERVER_DIR}/lib/mysqlcppconn.lib"
                            )
                            
                            foreach(LIB_PATH ${POSSIBLE_LIB_PATHS})
                                file(TO_CMAKE_PATH "${LIB_PATH}" LIB_PATH_CMAKE)
                                if(EXISTS "${LIB_PATH_CMAKE}")
                                    set(MySQL_LIBRARIES "${LIB_PATH_CMAKE}" CACHE PATH "MySQL Connector/C++ libraries" FORCE)
                                    message(STATUS "找到MySQL库文件: ${LIB_PATH_CMAKE}")
                                    break()
                                endif()
                            endforeach()
                            
                            # 修改DLL路径搜索顺序，优先使用预编译库目录
                            set(POSSIBLE_C_API_DLL_PATHS
                                "${MYSQL_PREBUILT_DIR}/lib64/vs14/libmysql.dll"
                                "${MYSQL_PREBUILT_DIR}/lib64/libmysql.dll"
                                "${MYSQL_PREBUILT_DIR}/lib/vs14/libmysql.dll"
                                "${MYSQL_PREBUILT_DIR}/lib/libmysql.dll"
                                "${MYSQL_PREBUILT_DIR}/bin/libmysql.dll"
                            )
                            
                            foreach(DLL_PATH ${POSSIBLE_C_API_DLL_PATHS})
                                file(TO_CMAKE_PATH "${DLL_PATH}" DLL_PATH_CMAKE)
                                if(EXISTS "${DLL_PATH_CMAKE}")
                                    set(MySQL_DLL "${DLL_PATH_CMAKE}" CACHE PATH "MySQL Connector/C++ DLL" FORCE)
                                    message(STATUS "找到MySQL DLL文件: ${DLL_PATH_CMAKE}")
                                    set(COPY_MYSQL_DLL ON CACHE BOOL "复制MySQL DLL到输出目录" FORCE)
                                    break()
                                endif()
                            endforeach()
                        endif()
                    else()
                        message(WARNING "未在任何目录找到MySQL头文件")
                        set(MYSQL_HEADER_FOUND FALSE)
                    endif()
                endif()
            endif()
            
            # 设置最终的包含目录
            set(MySQL_INCLUDE_DIRS "${MySQL_INCLUDE_DIR}" CACHE PATH "MySQL包含目录" FORCE)
            message(STATUS "MySQL包含目录: ${MySQL_INCLUDE_DIRS}")
            
            # 确定最终的MYSQL_FOUND状态
            if(MYSQL_HEADER_FOUND AND DEFINED MySQL_LIBRARIES AND EXISTS "${MySQL_LIBRARIES}")
                set(MYSQL_FOUND TRUE CACHE BOOL "MySQL has been found" FORCE)
                add_definitions(-DHAVE_MYSQL=1)
                message(STATUS "添加编译定义 -DHAVE_MYSQL=1")
                message(STATUS "MySQL库配置成功")
            else()
                if(NOT MYSQL_HEADER_FOUND)
                    message(WARNING "没有找到标准的MySQL头文件，可能会遇到编译问题")
                endif()
                if(NOT DEFINED MySQL_LIBRARIES OR NOT EXISTS "${MySQL_LIBRARIES}")
                    message(WARNING "未找到有效的MySQL库文件")
                endif()
                set(MYSQL_FOUND FALSE CACHE BOOL "MySQL has been found" FORCE)
                add_definitions(-DHAVE_MYSQL=0)
                message(STATUS "添加编译定义 -DHAVE_MYSQL=0")
            endif()
        endif()
    else()
        message(STATUS "MySQL功能已被禁用")
        add_definitions(-DHAVE_MYSQL=0)
        message(STATUS "添加编译定义 -DHAVE_MYSQL=0")
    endif()

    # 将MySQL相关变量传递到父作用域
    set(MySQL_INCLUDE_DIRS ${MySQL_INCLUDE_DIRS} PARENT_SCOPE)
    if(DEFINED MySQL_DLL)
        set(MySQL_DLL ${MySQL_DLL} PARENT_SCOPE)
    endif()
    if(DEFINED MySQL_LIBRARIES)
        set(MySQL_LIBRARIES ${MySQL_LIBRARIES} PARENT_SCOPE)
    endif()
    # 明确设置MYSQL_FOUND到父作用域
    set(MYSQL_FOUND ${MYSQL_FOUND} PARENT_SCOPE)

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
    set(OPENSSL_CRYPTO_DLL ${OPENSSL_CRYPTO_DLL} PARENT_SCOPE)
    set(OPENSSL_SSL_DLL ${OPENSSL_SSL_DLL} PARENT_SCOPE)

    set(MYSQL_DIR ${CMAKE_SOURCE_DIR}/third_party/mysql-connector-cpp PARENT_SCOPE)
    set(MySQL_INCLUDE_DIRS ${MySQL_INCLUDE_DIRS} PARENT_SCOPE)
    set(MySQL_LIBRARIES ${MySQL_LIBRARIES} PARENT_SCOPE)
    if(DEFINED MySQL_DLL)
        set(MySQL_DLL ${MySQL_DLL} PARENT_SCOPE)
    endif()
    set(MYSQL_FOUND ${MYSQL_FOUND} PARENT_SCOPE)
    set(BUILD_MYSQL_CONNECTOR ${BUILD_MYSQL_CONNECTOR} PARENT_SCOPE)

    # 导出所有变量到父作用域
    foreach(var ${EXPORTED_VARS})
        set(${var} ${${var}} PARENT_SCOPE)
    endforeach()
endfunction()

# 立即调用函数设置所有第三方库
configure_third_party_libs() 