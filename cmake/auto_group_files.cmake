# 遍历指定目录及其子目录，并将文件按目录分组
function(AUTO_GROUP_FILES)
    set(options)
    set(oneValueArgs TARGET BASE_DIR)
    set(multiValueArgs FILES)
    cmake_parse_arguments(AGF "${options}" "${oneValueArgs}" "${multiValueArgs}" ${ARGN})

    foreach(FILE ${AGF_FILES})
        # 确保文件路径是绝对路径
        string(FIND ${FILE} "/" str_index)
        if(str_index EQUAL -1)
            # 放root下
            source_group("" FILES "${FILE}")
            continue()
        endif()

        get_filename_component(ABS_PATH ${FILE} ABSOLUTE)
        file(RELATIVE_PATH REL_PATH ${AGF_BASE_DIR} ${ABS_PATH})
        string(REPLACE "/" "\\" GROUP_PATH ${REL_PATH})

        # 提取文件所在目录作为组名
        get_filename_component(GROUP_NAME ${GROUP_PATH} PATH)

        # 排除根目录的文件
        if(GROUP_NAME STREQUAL "")
            set(GROUP_NAME "")
        endif()

        # 将文件添加到组中
        source_group("${GROUP_NAME}" FILES "${FILE}")
    endforeach()
endfunction()
