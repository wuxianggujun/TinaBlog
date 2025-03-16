# Dependencies.cmake - 统一的依赖库管理
# ===============================================================
# 该文件集中管理所有外部依赖库，区分本地库和vcpkg库

# 定义区分本地库和vcpkg库的集合
set(VCPKG_LIBS
    "PCRE2"     # 正则表达式库
    "ZLIB"      # 压缩库
    "OpenSSL"   # SSL库
    "MySQL-Connector-C++"  # MySQL Connector C++
)

set(LOCAL_LIBS
    "MySQL"     # MySQL Server本地安装
)

# 配置编译选项的函数 - 从Utilities.cmake整合
function(configure_compiler_options TARGET_NAME)
    if(MSVC)
        target_compile_options(${TARGET_NAME} PRIVATE
            /W3
            /wd4996  # 禁止"deprecated"警告
            /wd4055  # 禁止类型转换警告 
            /wd4152  # 禁止函数指针转换警告
            /wd4204  # 非标准扩展警告
        )
        
        target_link_options(${TARGET_NAME} PRIVATE
            /NODEFAULTLIB:libcmt.lib
            /NODEFAULTLIB:libcmtd.lib
            /NODEFAULTLIB:msvcrt.lib
            /ignore:4217
            /ignore:4286
        )
    else()
        target_compile_options(${TARGET_NAME} PRIVATE
            -Wall
            -Wextra
            -Wno-unused-parameter
        )
    endif()
endfunction()

# 统一的依赖查找函数
function(find_all_dependencies)
    message(STATUS "----- 开始查找所有依赖库 -----")
    
    # 查找所有vcpkg安装的库
    find_package(ZLIB REQUIRED)
    find_package(OpenSSL REQUIRED)
    find_package(pcre2 CONFIG REQUIRED)
    find_package(unofficial-mysql-connector-cpp CONFIG REQUIRED)
    
    # 查找本地安装的MySQL
    find_mysql_server()
    
    # 设置MySQL Connector C++头文件目录
    if(unofficial-mysql-connector-cpp_FOUND)
        set(MYSQL_CONNECTOR_INCLUDE_DIRS 
            "${_VCPKG_INSTALLED_DIR}/${VCPKG_TARGET_TRIPLET}/include/mysql-cppconn-8"
            CACHE PATH "MySQL Connector C++包含目录" FORCE
        )
        message(STATUS "MySQL Connector C++已找到")
    else()
        message(FATAL_ERROR "未找到MySQL Connector C++，请使用vcpkg安装: vcpkg install mysql-connector-cpp:x64-windows")
    endif()
    
    # 导出MySQL Connector C++变量到父作用域
    set(MYSQL_CONNECTOR_INCLUDE_DIRS ${MYSQL_CONNECTOR_INCLUDE_DIRS} PARENT_SCOPE)
    
    # 输出所有找到的库信息
    message(STATUS "----- 依赖库查找结果 -----")
    message(STATUS "PCRE2: ${pcre2_VERSION}")
    message(STATUS "ZLIB: ${ZLIB_VERSION_STRING}")
    message(STATUS "OpenSSL: ${OPENSSL_VERSION}")
    message(STATUS "MySQL: ${MYSQL_SERVER_DIR}")
    message(STATUS "MySQL Connector C++: ${_VCPKG_INSTALLED_DIR}/${VCPKG_TARGET_TRIPLET}/include/mysql-cppconn-8")
    message(STATUS "---------------------------")
endfunction()

# MySQL Server配置函数
function(find_mysql_server)
    # 设置MySQL相关路径选项
    set(MYSQL_SERVER_DIR "D:/Program Files/MySQL/MySQL Server 9.2" CACHE PATH "MySQL Server安装路径")
    
    # 导出目录变量到父作用域
    set(MYSQL_SERVER_DIR ${MYSQL_SERVER_DIR} PARENT_SCOPE)
    
    # 设置MySQL Server相关变量
    set(MySQL_INCLUDE_DIRS 
        "${MYSQL_SERVER_DIR}/include"
        CACHE PATH "MySQL包含目录"
    )
    
    # 设置MySQL Server的库文件路径
    set(MySQL_LIBRARIES 
        "${MYSQL_SERVER_DIR}/lib/libmysql.lib"
        CACHE PATH "MySQL库文件"
    )
    
    # 检查MySQL头文件和库是否存在
    if(NOT EXISTS "${MYSQL_SERVER_DIR}/include")
        message(WARNING "MySQL Server头文件目录不存在: ${MYSQL_SERVER_DIR}/include")
        set(MYSQL_FOUND FALSE CACHE BOOL "MySQL has not been found" FORCE)
    elseif(NOT EXISTS "${MYSQL_SERVER_DIR}/lib/libmysql.lib")
        message(WARNING "MySQL库文件不存在: ${MYSQL_SERVER_DIR}/lib/libmysql.lib")
        set(MYSQL_FOUND FALSE CACHE BOOL "MySQL has not been found" FORCE)
    else()
        set(MYSQL_FOUND TRUE CACHE BOOL "MySQL has been found" FORCE)
        message(STATUS "MySQL Server库配置成功")
    endif()
    
    # 导出变量到父作用域
    set(MySQL_INCLUDE_DIRS ${MySQL_INCLUDE_DIRS} PARENT_SCOPE)
    set(MySQL_LIBRARIES ${MySQL_LIBRARIES} PARENT_SCOPE)
    set(MYSQL_FOUND ${MYSQL_FOUND} PARENT_SCOPE)
endfunction()

# 配置运行时DLL复制
function(configure_dll_dependencies TARGET_NAME)
    # 添加复制依赖库到输出目录的逻辑
    add_custom_command(TARGET ${TARGET_NAME} POST_BUILD
        # 复制MySQL Server DLL
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${MYSQL_SERVER_DIR}/lib/libmysql.dll"
            "${CMAKE_SOURCE_DIR}/libmysql.dll"
        # 复制vcpkg库文件
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${_VCPKG_INSTALLED_DIR}/${VCPKG_TARGET_TRIPLET}/bin/zlib1.dll"
            "${CMAKE_SOURCE_DIR}/zlib1.dll"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${_VCPKG_INSTALLED_DIR}/${VCPKG_TARGET_TRIPLET}/bin/pcre2-8.dll"
            "${CMAKE_SOURCE_DIR}/pcre2-8.dll"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${_VCPKG_INSTALLED_DIR}/${VCPKG_TARGET_TRIPLET}/bin/libcrypto-3-x64.dll" 
            "${CMAKE_SOURCE_DIR}/libcrypto-3-x64.dll"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${_VCPKG_INSTALLED_DIR}/${VCPKG_TARGET_TRIPLET}/bin/libssl-3-x64.dll"
            "${CMAKE_SOURCE_DIR}/libssl-3-x64.dll"
        COMMENT "复制运行时依赖DLL到输出目录..."
    )
    
    # 查找MySQL Connector C++ DLL
    file(GLOB MYSQL_CPP_DLL_FILES
        "${_VCPKG_INSTALLED_DIR}/${VCPKG_TARGET_TRIPLET}/bin/mysqlcppconn*.dll"
    )
    
    foreach(DLL_FILE ${MYSQL_CPP_DLL_FILES})
        get_filename_component(DLL_FILENAME ${DLL_FILE} NAME)
        add_custom_command(TARGET ${TARGET_NAME} POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                ${DLL_FILE}
                "${CMAKE_SOURCE_DIR}/${DLL_FILENAME}"
            COMMENT "复制MySQL Connector C++ DLL: ${DLL_FILENAME}"
        )
    endforeach()
    
    # 复制Protobuf DLL (MySQL Connector C++依赖库)
    file(GLOB PROTOBUF_DLL_FILES
        "${_VCPKG_INSTALLED_DIR}/${VCPKG_TARGET_TRIPLET}/bin/libprotobuf*.dll"
    )
    
    foreach(DLL_FILE ${PROTOBUF_DLL_FILES})
        get_filename_component(DLL_FILENAME ${DLL_FILE} NAME)
        add_custom_command(TARGET ${TARGET_NAME} POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                ${DLL_FILE}
                "${CMAKE_SOURCE_DIR}/${DLL_FILENAME}"
            COMMENT "复制Protobuf DLL (MySQL Connector C++依赖): ${DLL_FILENAME}"
        )
    endforeach()
    
    # 复制LZ4 DLL (另一个MySQL Connector C++依赖)
    if(EXISTS "${_VCPKG_INSTALLED_DIR}/${VCPKG_TARGET_TRIPLET}/bin/lz4.dll")
        add_custom_command(TARGET ${TARGET_NAME} POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "${_VCPKG_INSTALLED_DIR}/${VCPKG_TARGET_TRIPLET}/bin/lz4.dll"
                "${CMAKE_SOURCE_DIR}/lz4.dll"
            COMMENT "复制LZ4 DLL (MySQL Connector C++依赖)"
        )
    endif()
    
    # 复制其他可能的依赖DLL
    set(POSSIBLE_DEPS 
        "zstd.dll"      # ZSTD压缩库
        "xxhash.dll"    # Hash库
        "fmt.dll"       # 格式化库
    )
    
    foreach(DEP_DLL ${POSSIBLE_DEPS})
        if(EXISTS "${_VCPKG_INSTALLED_DIR}/${VCPKG_TARGET_TRIPLET}/bin/${DEP_DLL}")
            add_custom_command(TARGET ${TARGET_NAME} POST_BUILD
                COMMAND ${CMAKE_COMMAND} -E copy_if_different
                    "${_VCPKG_INSTALLED_DIR}/${VCPKG_TARGET_TRIPLET}/bin/${DEP_DLL}"
                    "${CMAKE_SOURCE_DIR}/${DEP_DLL}"
                COMMENT "复制${DEP_DLL} (可能的MySQL Connector C++依赖)"
            )
        endif()
    endforeach()
    
    # 复制vcpkg bin目录中所有包含"mysql"的DLL
    # 这是为了确保捕获所有MySQL相关的DLL
    file(GLOB MYSQL_RELATED_DLLS
        "${_VCPKG_INSTALLED_DIR}/${VCPKG_TARGET_TRIPLET}/bin/*mysql*.dll"
    )
    
    foreach(DLL_FILE ${MYSQL_RELATED_DLLS})
        get_filename_component(DLL_FILENAME ${DLL_FILE} NAME)
        if(NOT "${DLL_FILENAME}" MATCHES "mysqlcppconn") # 避免重复复制已处理的文件
            add_custom_command(TARGET ${TARGET_NAME} POST_BUILD
                COMMAND ${CMAKE_COMMAND} -E copy_if_different
                    ${DLL_FILE}
                    "${CMAKE_SOURCE_DIR}/${DLL_FILENAME}"
                COMMENT "复制MySQL相关DLL: ${DLL_FILENAME}"
            )
        endif()
    endforeach()
endfunction()

# 链接所有依赖库到目标
function(link_all_dependencies TARGET_NAME)
    target_link_libraries(${TARGET_NAME}
        PRIVATE
        # 系统库 - Windows平台
        $<$<PLATFORM_ID:Windows>:kernel32.lib>
        $<$<PLATFORM_ID:Windows>:user32.lib>
        $<$<PLATFORM_ID:Windows>:advapi32.lib>
        $<$<PLATFORM_ID:Windows>:ws2_32.lib>
        $<$<PLATFORM_ID:Windows>:gdi32.lib>
        $<$<PLATFORM_ID:Windows>:crypt32.lib>
        # SSL库
        OpenSSL::SSL
        OpenSSL::Crypto
        # MySQL Server库 - 本地安装
        ${MySQL_LIBRARIES}
        # MySQL Connector C++ - vcpkg安装
        unofficial::mysql-connector-cpp::connector
        # PCRE库 - 只使用vcpkg的PCRE2
        PCRE2::8BIT
        # zlib库 - 通过vcpkg安装
        ZLIB::ZLIB
    )
    
    # 添加通用编译定义
    target_compile_definitions(${TARGET_NAME} PRIVATE 
        # 添加PCRE2支持
        $<$<TARGET_EXISTS:PCRE2::8BIT>:NGX_PCRE2=1>
        $<$<TARGET_EXISTS:PCRE2::8BIT>:NGX_HAVE_PCRE=1>
    )
endfunction()

# 设置全局编译选项
function(configure_global_options)
    if(MSVC)
        # 统一所有库使用动态运行时库/MD
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

# 初始化所有依赖配置
configure_global_options()
find_all_dependencies() 