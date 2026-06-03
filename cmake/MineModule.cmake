# MineFramework 模块定义函数
# 对应 xmake/rules/mine_module.lua

# 定义 mine 模块的统一函数
# 参数：
#   NAME - 模块全名，如 mine.core
#   SHORT_NAME - 短名，如 core（用于选项检查）
#   SOURCES - 源文件列表或模式
#   PUBLIC_HEADERS - 公开头文件目录
#   PRIVATE_HEADERS - 私有头文件目录
#   DEPENDENCIES - 依赖的其他 mine 模块
#   EXTERNAL_DEPS - 外部依赖（如 freetype）
#   TEST_SOURCES - 测试源文件
#   API_ONLY - 是否仅接口库（纯头文件）
function(mine_add_module)
    # 解析参数
    set(options API_ONLY)
    set(oneValueArgs NAME SHORT_NAME)
    set(multiValueArgs 
        SOURCES 
        PUBLIC_HEADERS 
        PRIVATE_HEADERS 
        DEPENDENCIES 
        EXTERNAL_DEPS
        TEST_SOURCES
        COMPILE_DEFINITIONS
    )
    cmake_parse_arguments(ARG "${options}" "${oneValueArgs}" "${multiValueArgs}" ${ARGN})
    
    # 检查必需参数
    if(NOT ARG_NAME)
        message(FATAL_ERROR "mine_add_module: 缺少 NAME 参数")
    endif()
    
    # 如果未提供 SHORT_NAME，从 NAME 提取
    if(NOT ARG_SHORT_NAME)
        string(REPLACE "mine." "" ARG_SHORT_NAME "${ARG_NAME}")
        string(REPLACE "." "_" ARG_SHORT_NAME "${ARG_SHORT_NAME}")
    endif()
    
    # 检查模块是否启用（基于选项）
    string(TOUPPER "${ARG_SHORT_NAME}" SHORT_NAME_UPPER)
    string(REPLACE "." "_" SHORT_NAME_UPPER "${SHORT_NAME_UPPER}")
    
    if(DEFINED MINE_MODULE_${SHORT_NAME_UPPER})
        if(NOT MINE_MODULE_${SHORT_NAME_UPPER})
            message(STATUS "跳过模块 ${ARG_NAME}（已禁用）")
            return()
        endif()
    endif()
    
    message(STATUS "配置模块: ${ARG_NAME}")
    
    # 创建主目标
    if(ARG_API_ONLY)
        # 纯接口库
        add_library(${ARG_NAME} INTERFACE)
        
        # 添加接口头文件目录
        if(ARG_PUBLIC_HEADERS)
            foreach(header_dir ${ARG_PUBLIC_HEADERS})
                target_include_directories(${ARG_NAME} INTERFACE
                    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/${header_dir}>
                    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
                )
            endforeach()
        endif()
        
        # 接口库依赖
        if(ARG_DEPENDENCIES)
            target_link_libraries(${ARG_NAME} INTERFACE ${ARG_DEPENDENCIES})
        endif()
        
    else()
        # 实现库
        # 收集源文件
        set(SOURCE_FILES)
        if(ARG_SOURCES)
            foreach(source_pattern ${ARG_SOURCES})
                # 如果是 glob 模式（包含 *），展开
                if(source_pattern MATCHES "\\*")
                    file(GLOB_RECURSE matched_sources 
                        CONFIGURE_DEPENDS
                        ${CMAKE_CURRENT_SOURCE_DIR}/${source_pattern}
                    )
                    list(APPEND SOURCE_FILES ${matched_sources})
                else()
                    list(APPEND SOURCE_FILES ${CMAKE_CURRENT_SOURCE_DIR}/${source_pattern})
                endif()
            endforeach()
        endif()
        
        # 如果没有指定源文件，自动查找 src 目录
        if(NOT SOURCE_FILES)
            file(GLOB_RECURSE SOURCE_FILES 
                CONFIGURE_DEPENDS
                ${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp
                ${CMAKE_CURRENT_SOURCE_DIR}/src/*.c
            )
        endif()
        
        # 创建库目标
        add_library(${ARG_NAME} ${MINE_LIBRARY_TYPE} ${SOURCE_FILES})
        
        # 设置输出名称（去掉 mine. 前缀）
        string(REPLACE "mine." "" output_name "${ARG_NAME}")
        set_target_properties(${ARG_NAME} PROPERTIES
            OUTPUT_NAME ${output_name}
            POSITION_INDEPENDENT_CODE ON
        )
        
        # 添加公开头文件目录
        if(ARG_PUBLIC_HEADERS)
            foreach(header_dir ${ARG_PUBLIC_HEADERS})
                target_include_directories(${ARG_NAME} PUBLIC
                    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/${header_dir}>
                    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
                )
            endforeach()
        else()
            # 默认使用 api/include
            if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/api/include)
                target_include_directories(${ARG_NAME} PUBLIC
                    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/api/include>
                    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
                )
            endif()
        endif()
        
        # 添加私有头文件目录
        if(ARG_PRIVATE_HEADERS)
            foreach(header_dir ${ARG_PRIVATE_HEADERS})
                target_include_directories(${ARG_NAME} PRIVATE
                    ${CMAKE_CURRENT_SOURCE_DIR}/${header_dir}
                )
            endforeach()
        else()
            # 默认使用 src
            if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/src)
                target_include_directories(${ARG_NAME} PRIVATE
                    ${CMAKE_CURRENT_SOURCE_DIR}/src
                )
            endif()
        endif()
        
        # 添加依赖
        if(ARG_DEPENDENCIES)
            target_link_libraries(${ARG_NAME} PUBLIC ${ARG_DEPENDENCIES})
        endif()
        
        if(ARG_EXTERNAL_DEPS)
            target_link_libraries(${ARG_NAME} PRIVATE ${ARG_EXTERNAL_DEPS})
        endif()
        
        # 添加编译定义
        if(ARG_COMPILE_DEFINITIONS)
            target_compile_definitions(${ARG_NAME} PRIVATE ${ARG_COMPILE_DEFINITIONS})
        endif()
        
        # 应用通用编译器标志
        mine_set_compiler_flags(${ARG_NAME})
        
        # 导出符号定义
        if(MINE_BUILD_SHARED)
            string(TOUPPER "${ARG_NAME}" MODULE_UPPER)
            string(REPLACE "." "_" MODULE_UPPER "${MODULE_UPPER}")
            target_compile_definitions(${ARG_NAME} PRIVATE
                ${MODULE_UPPER}_EXPORTS=1
            )
        endif()
    endif()
    
    # 创建 API 子目标（接口库）
    set(api_target "${ARG_NAME}.api")
    if(NOT TARGET ${api_target})
        add_library(${api_target} INTERFACE)
        
        # API 目标继承公开头文件和依赖
        if(ARG_PUBLIC_HEADERS)
            foreach(header_dir ${ARG_PUBLIC_HEADERS})
                target_include_directories(${api_target} INTERFACE
                    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/${header_dir}>
                    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
                )
            endforeach()
        else()
            if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/api/include)
                target_include_directories(${api_target} INTERFACE
                    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/api/include>
                    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
                )
            endif()
        endif()
        
        # API 目标依赖其他 API 目标
        if(ARG_DEPENDENCIES)
            set(api_deps)
            foreach(dep ${ARG_DEPENDENCIES})
                if(TARGET ${dep}.api)
                    list(APPEND api_deps ${dep}.api)
                endif()
            endforeach()
            if(api_deps)
                target_link_libraries(${api_target} INTERFACE ${api_deps})
            endif()
        endif()
    endif()
    
    # 单元测试
    if(MINE_BUILD_TESTS AND ARG_TEST_SOURCES)
        set(test_target "${ARG_NAME}.test")
        
        # 收集测试源文件
        set(TEST_FILES)
        foreach(test_pattern ${ARG_TEST_SOURCES})
            if(test_pattern MATCHES "\\*")
                file(GLOB_RECURSE matched_tests 
                    CONFIGURE_DEPENDS
                    ${CMAKE_CURRENT_SOURCE_DIR}/${test_pattern}
                )
                list(APPEND TEST_FILES ${matched_tests})
            else()
                list(APPEND TEST_FILES ${CMAKE_CURRENT_SOURCE_DIR}/${test_pattern})
            endif()
        endforeach()
        
        # 如果没有指定测试文件，自动查找 test 目录
        if(NOT TEST_FILES)
            file(GLOB_RECURSE TEST_FILES 
                CONFIGURE_DEPENDS
                ${CMAKE_CURRENT_SOURCE_DIR}/test/*.cpp
            )
        endif()
        
        if(TEST_FILES)
            add_executable(${test_target} ${TEST_FILES})
            target_link_libraries(${test_target} PRIVATE ${ARG_NAME})
            
            # 链接 doctest
            if(DOCTEST_INCLUDE_DIR)
                target_include_directories(${test_target} PRIVATE ${DOCTEST_INCLUDE_DIR})
            endif()
            
            # 应用编译器标志
            mine_set_compiler_flags(${test_target})
            
            # 添加到 CTest
            add_test(NAME ${test_target} COMMAND ${test_target})
            
            # 设置测试属性
            set_tests_properties(${test_target} PROPERTIES
                WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
            )
        endif()
    endif()
    
    # 安装规则
    if(NOT ARG_API_ONLY)
        # 安装库文件
        install(TARGETS ${ARG_NAME}
            EXPORT MineFrameworkTargets
            LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
            ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
            RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
        )
        
        # 安装公开头文件
        if(ARG_PUBLIC_HEADERS)
            foreach(header_dir ${ARG_PUBLIC_HEADERS})
                install(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/${header_dir}/
                    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
                    FILES_MATCHING PATTERN "*.h" PATTERN "*.hpp"
                )
            endforeach()
        else()
            if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/api/include)
                install(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/api/include/
                    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
                    FILES_MATCHING PATTERN "*.h" PATTERN "*.hpp"
                )
            endif()
        endif()
    endif()
    
    # 安装 API 目标
    install(TARGETS ${api_target}
        EXPORT MineFrameworkTargets
    )
endfunction()

# 简化版本：仅指定模块名，使用默认约定
function(mine_module NAME)
    mine_add_module(
        NAME ${NAME}
        SOURCES src/*.cpp
        PUBLIC_HEADERS api/include
        PRIVATE_HEADERS src
        TEST_SOURCES test/*.cpp
        ${ARGN}
    )
endfunction()
