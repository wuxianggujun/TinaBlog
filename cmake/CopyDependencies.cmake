# CopyDependencies.cmake - 动态库复制工具
# ===============================================================
# 该文件封装了所有与DLL复制相关的功能，提供简洁的接口

# 复制单个DLL文件到目标路径的函数
function(copy_dll_to_target TARGET_NAME DLL_PATH DLL_NAME REQUIRED)
    if(EXISTS "${DLL_PATH}")
        add_custom_command(TARGET ${TARGET_NAME} PRE_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${DLL_PATH}"
            "$<TARGET_FILE_DIR:${TARGET_NAME}>"
            COMMENT "复制${DLL_NAME}到输出目录: ${DLL_PATH}"
        )
        message(STATUS "已设置${DLL_NAME}复制命令: ${DLL_PATH}")
    elseif(${REQUIRED})
        message(WARNING "${DLL_NAME}文件不存在，无法设置复制命令: ${DLL_PATH}")
    endif()
endfunction()

# 复制所有依赖DLL到目标的主函数
function(copy_all_dependencies TARGET_NAME)
    # 检查并复制ZLIB DLL
    if(TARGET zlib)
        copy_dll_to_target(${TARGET_NAME} "$<TARGET_FILE:zlib>" "ZLIB动态库" FALSE)
    else()
        message(STATUS "找不到ZLIB目标，跳过复制ZLIB DLL")
    endif()
    
    # 检查并复制MySQL Server DLL
    set(MYSQL_DLL_PATH "${MYSQL_SERVER_DIR}/lib/libmysql.dll")
    copy_dll_to_target(${TARGET_NAME} "${MYSQL_DLL_PATH}" "MySQL运行时DLL" TRUE)
    
    # 检查并复制OpenSSL DLLs
    copy_dll_to_target(${TARGET_NAME} "${OPENSSL_CRYPTO_DLL}" "OpenSSL Crypto DLL" FALSE)
    copy_dll_to_target(${TARGET_NAME} "${OPENSSL_SSL_DLL}" "OpenSSL SSL DLL" FALSE)
    
    # 检查并复制MySQL Connector C++ DLL
    copy_dll_to_target(${TARGET_NAME} "${MySQL_CONNECTOR_DLL}" "MySQL Connector C++ DLL" FALSE)
endfunction()

# 创建copy_all_dlls目标，用于手动复制所有依赖到项目根目录
function(create_copy_dlls_target)
    # 添加一个新的自定义目标，用于手动复制所有DLL文件到项目根目录
    add_custom_target(copy_all_dlls
        COMMENT "手动复制所有需要的DLL文件到项目根目录"
    )
    
    # 复制ZLIB DLL
    if(TARGET zlib)
        add_custom_command(TARGET copy_all_dlls POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "$<TARGET_FILE:zlib>"
            "${CMAKE_SOURCE_DIR}"
            COMMENT "复制ZLIB动态库到项目根目录"
        )
    endif()
    
    # 复制MySQL DLL
    set(MYSQL_DLL_PATH "${MYSQL_SERVER_DIR}/lib/libmysql.dll")
    if(EXISTS "${MYSQL_DLL_PATH}")
        add_custom_command(TARGET copy_all_dlls POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${MYSQL_DLL_PATH}"
            "${CMAKE_SOURCE_DIR}"
            COMMENT "复制MySQL动态库到项目根目录"
        )
    else()
        message(WARNING "无法为copy_all_dlls目标设置MySQL DLL复制命令，文件不存在: ${MYSQL_DLL_PATH}")
    endif()
    
    # 复制OpenSSL DLLs
    if(EXISTS "${OPENSSL_CRYPTO_DLL}")
        add_custom_command(TARGET copy_all_dlls POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${OPENSSL_CRYPTO_DLL}"
            "${CMAKE_SOURCE_DIR}"
            COMMENT "复制OpenSSL Crypto动态库到项目根目录"
        )
    endif()
    
    if(EXISTS "${OPENSSL_SSL_DLL}")
        add_custom_command(TARGET copy_all_dlls POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${OPENSSL_SSL_DLL}"
            "${CMAKE_SOURCE_DIR}"
            COMMENT "复制OpenSSL SSL动态库到项目根目录"
        )
    endif()
    
    # 复制MySQL Connector C++ DLL
    if(EXISTS "${MySQL_CONNECTOR_DLL}")
        add_custom_command(TARGET copy_all_dlls POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${MySQL_CONNECTOR_DLL}"
            "${CMAKE_SOURCE_DIR}"
            COMMENT "复制MySQL Connector C++ DLL到项目根目录"
        )
    endif()
endfunction()

# 创建清理DLL的目标
function(create_clean_dlls_target)
    add_custom_target(clean_dll_files
        COMMAND ${CMAKE_COMMAND} -E echo "清理项目根目录下的DLL文件..."
        COMMAND ${CMAKE_COMMAND} -E remove -f 
            ${CMAKE_SOURCE_DIR}/zlibd1.dll
            ${CMAKE_SOURCE_DIR}/zlib1.dll
            ${CMAKE_SOURCE_DIR}/libmysql.dll
            ${CMAKE_SOURCE_DIR}/libcrypto-3-x64.dll
            ${CMAKE_SOURCE_DIR}/libssl-3-x64.dll
            ${CMAKE_SOURCE_DIR}/mysqlcppconn-9-vs14.dll
            ${CMAKE_SOURCE_DIR}/mysqlcppconn-10-vs14.dll
            ${CMAKE_SOURCE_DIR}/mysqlcppconn.dll
        COMMENT "清理项目根目录下的DLL文件"
    )
endfunction() 