# MineFramework 第三方依赖管理
# 支持多种获取方式：系统库 > vcpkg > 网络下载（多镜像源）
# 设计目标：开源后方便不同网络环境的开发者构建

include(FetchContent)

# ============================================================================
# 配置选项
# ============================================================================

option(MINE_PREFER_SYSTEM_LIBS "优先使用系统已安装的库（find_package）" OFF)
option(MINE_USE_SHALLOW_CLONE "使用浅克隆加速下载（减少 80% 下载量）" ON)
option(MINE_OFFLINE_BUILD "离线构建模式（需要预下载 third_party/）" OFF)

# 配置 FetchContent 行为
set(FETCHCONTENT_QUIET OFF)
set(FETCHCONTENT_UPDATES_DISCONNECTED ON)

message(STATUS "")
message(STATUS "配置 MineFramework 第三方依赖")
message(STATUS "====================================================")
message(STATUS "优先使用系统库: ${MINE_PREFER_SYSTEM_LIBS}")
message(STATUS "浅克隆加速: ${MINE_USE_SHALLOW_CLONE}")
message(STATUS "离线构建: ${MINE_OFFLINE_BUILD}")

# ============================================================================
# 依赖获取函数（多方案支持）
# ============================================================================

# 通用依赖获取函数（多镜像源 + 离线支持）
function(mine_fetch_content name)
    set(options NO_SUBMODULES)
    set(oneValueArgs SYSTEM_PACKAGE GIT_TAG VALIDATE_FILE)
    set(multiValueArgs GIT_MIRRORS CMAKE_ARGS)
    cmake_parse_arguments(ARG "${options}" "${oneValueArgs}" "${multiValueArgs}" ${ARGN})
    
    # 处理子模块选项（FreeType 有 submodule 但 CMake 不需要）
    if(ARG_NO_SUBMODULES)
        set(GIT_SUBMODULES_CMD "GIT_SUBMODULES \"\"")
    else()
        set(GIT_SUBMODULES_CMD "")
    endif()

    if(ARG_VALIDATE_FILE STREQUAL "ARG_VALIDATE_FILE-NOTFOUND")
        set(ARG_VALIDATE_FILE "")
    endif()
    
    message(STATUS "配置 ${name}...")
    
    # 1. 尝试使用系统库
    if(ARG_SYSTEM_PACKAGE AND MINE_PREFER_SYSTEM_LIBS)
        find_package(${ARG_SYSTEM_PACKAGE} QUIET)
        if(${ARG_SYSTEM_PACKAGE}_FOUND)
            message(STATUS "      ✓ 使用系统库: ${ARG_SYSTEM_PACKAGE}")
            set(${name}_FOUND TRUE PARENT_SCOPE)
            return()
        endif()
    endif()
    
    # 2. 离线模式：使用预下载的源码
    if(MINE_OFFLINE_BUILD)
        set(local_source "${CMAKE_SOURCE_DIR}/third_party/${name}")
        if(EXISTS "${local_source}")
            FetchContent_Declare(
                ${name}
                SOURCE_DIR "${local_source}"
            )
            FetchContent_MakeAvailable(${name})
            if(ARG_VALIDATE_FILE AND NOT EXISTS "${local_source}/${ARG_VALIDATE_FILE}")
                message(FATAL_ERROR "      ✗ 本地源码校验失败: ${local_source}/${ARG_VALIDATE_FILE}")
            endif()
            message(STATUS "      ✓ 使用本地源码: third_party/${name}")
            return()
        else()
            message(FATAL_ERROR "      ✗ 离线模式但未找到: ${local_source}\n      请运行: scripts/download_deps.ps1")
        endif()
    endif()

    # 3. 在线模式：带校验的下载（用于镜像可能误指向的场景）
    if(ARG_VALIDATE_FILE)
        list(LENGTH ARG_GIT_MIRRORS mirror_count)
        if(mirror_count EQUAL 0)
            message(FATAL_ERROR "      ✗ ${name} 未配置镜像源")
        endif()

        if(ARG_CMAKE_ARGS)
            foreach(arg ${ARG_CMAKE_ARGS})
                set(${name}_${arg} ON CACHE BOOL "" FORCE)
            endforeach()
        endif()

        set(fetch_success FALSE)
        set(mirror_index 1)
        foreach(mirror_url ${ARG_GIT_MIRRORS})
            message(STATUS "      尝试镜像 [${mirror_index}/${mirror_count}]: ${mirror_url}")

            set(mirror_name "${name}_mirror_${mirror_index}")
            if(MINE_USE_SHALLOW_CLONE)
                FetchContent_Declare(
                    ${mirror_name}
                    GIT_REPOSITORY ${mirror_url}
                    GIT_TAG ${ARG_GIT_TAG}
                    GIT_SHALLOW TRUE
                    GIT_PROGRESS TRUE
                    ${GIT_SUBMODULES_CMD}
                )
            else()
                FetchContent_Declare(
                    ${mirror_name}
                    GIT_REPOSITORY ${mirror_url}
                    GIT_TAG ${ARG_GIT_TAG}
                    ${GIT_SUBMODULES_CMD}
                )
            endif()

            set(FETCHCONTENT_QUIET ON)
            FetchContent_Populate(${mirror_name})
            set(FETCHCONTENT_QUIET OFF)

            if(EXISTS "${${mirror_name}_SOURCE_DIR}/${ARG_VALIDATE_FILE}")
                add_subdirectory(${${mirror_name}_SOURCE_DIR} ${${mirror_name}_BINARY_DIR})
                set(${name}_SOURCE_DIR "${${mirror_name}_SOURCE_DIR}" PARENT_SCOPE)
                set(${name}_BINARY_DIR "${${mirror_name}_BINARY_DIR}" PARENT_SCOPE)
                set(fetch_success TRUE)
                message(STATUS "      ✓ ${name} 配置完成")
                break()
            endif()

            message(STATUS "      ✗ ${name} 校验失败，缺少: ${ARG_VALIDATE_FILE}")
            if(DEFINED ${mirror_name}_SOURCE_DIR AND EXISTS "${${mirror_name}_SOURCE_DIR}")
                file(REMOVE_RECURSE "${${mirror_name}_SOURCE_DIR}")
            endif()
            if(DEFINED ${mirror_name}_BINARY_DIR AND EXISTS "${${mirror_name}_BINARY_DIR}")
                file(REMOVE_RECURSE "${${mirror_name}_BINARY_DIR}")
            endif()

            math(EXPR mirror_index "${mirror_index} + 1")
        endforeach()

        if(NOT fetch_success)
            message(FATAL_ERROR 
                "\n===== ${name} 下载失败 =====\n"
                "所有镜像源均不可用。请尝试以下方案：\n"
                "\n方案 1: 使用 vcpkg 安装依赖\n"
                "  vcpkg install ${name}\n"
                "  cmake -DCMAKE_TOOLCHAIN_FILE=<vcpkg>/scripts/buildsystems/vcpkg.cmake ..\n"
                "\n方案 2: 离线构建\n"
                "  1. 运行: scripts/download_deps.ps1\n"
                "  2. 配置: cmake -DMINE_OFFLINE_BUILD=ON ..\n"
                "\n方案 3: 手动下载\n"
                "  git clone ${ARG_GIT_MIRRORS} third_party/${name}\n"
                "  cmake -DMINE_OFFLINE_BUILD=ON ..\n"
                "==============================\n"
            )
        endif()

        return()
    endif()
    
    # 3. 在线模式：多镜像源自动切换
    list(LENGTH ARG_GIT_MIRRORS mirror_count)
    if(mirror_count EQUAL 0)
        message(FATAL_ERROR "      ✗ ${name} 未配置镜像源")
    endif()
    
    set(fetch_success FALSE)
    set(mirror_index 1)
    
    foreach(mirror_url ${ARG_GIT_MIRRORS})
        message(STATUS "      尝试镜像 [${mirror_index}/${mirror_count}]: ${mirror_url}")
        
        # 清理之前失败的声明
        if(DEFINED ${name}_SOURCE_DIR)
            unset(${name}_SOURCE_DIR CACHE)
        endif()
        if(DEFINED ${name}_BINARY_DIR)
            unset(${name}_BINARY_DIR CACHE)
        endif()
        
        # 构建 FetchContent_Declare 参数
        if(MINE_USE_SHALLOW_CLONE)
            FetchContent_Declare(
                ${name}
                GIT_REPOSITORY ${mirror_url}
                GIT_TAG ${ARG_GIT_TAG}
                GIT_SHALLOW TRUE
                GIT_PROGRESS TRUE
                ${GIT_SUBMODULES_CMD}
            )
        else()
            FetchContent_Declare(
                ${name}
                GIT_REPOSITORY ${mirror_url}
                GIT_TAG ${ARG_GIT_TAG}
                ${GIT_SUBMODULES_CMD}
            )
        endif()
        
        # 设置 CMake 参数
        if(ARG_CMAKE_ARGS)
            foreach(arg ${ARG_CMAKE_ARGS})
                set(${name}_${arg} ON CACHE BOOL "" FORCE)
            endforeach()
        endif()
        
        # 尝试获取（抑制详细输出）
        set(FETCHCONTENT_QUIET ON)
        FetchContent_MakeAvailable(${name})
        set(FETCHCONTENT_QUIET OFF)
        
        # 检测是否成功（通过 SOURCE_DIR 存在性判断）
        FetchContent_GetProperties(${name})
        if(${name}_POPULATED OR EXISTS "${${name}_SOURCE_DIR}")
            # 将 Source/Binary 目录提升到父作用域，供子目录使用
            if(DEFINED ${name}_SOURCE_DIR)
                set(${name}_SOURCE_DIR "${${name}_SOURCE_DIR}" PARENT_SCOPE)
            endif()
            if(DEFINED ${name}_BINARY_DIR)
                set(${name}_BINARY_DIR "${${name}_BINARY_DIR}" PARENT_SCOPE)
            endif()
            set(fetch_success TRUE)
            message(STATUS "      ✓ ${name} 配置完成")
            break()
        endif()
        
        math(EXPR mirror_index "${mirror_index} + 1")
    endforeach()
    
    if(NOT fetch_success)
        message(FATAL_ERROR 
            "\n===== ${name} 下载失败 =====\n"
            "所有镜像源均不可用。请尝试以下方案：\n"
            "\n方案 1: 使用 vcpkg 安装依赖\n"
            "  vcpkg install ${name}\n"
            "  cmake -DCMAKE_TOOLCHAIN_FILE=<vcpkg>/scripts/buildsystems/vcpkg.cmake ..\n"
            "\n方案 2: 离线构建\n"
            "  1. 运行: scripts/download_deps.ps1\n"
            "  2. 配置: cmake -DMINE_OFFLINE_BUILD=ON ..\n"
            "\n方案 3: 手动下载\n"
            "  git clone ${ARG_GIT_MIRRORS} third_party/${name}\n"
            "  cmake -DMINE_OFFLINE_BUILD=ON ..\n"
            "==============================\n"
        )
    endif()
endfunction()

# ============================================================================
# 主函数：配置所有依赖
# ============================================================================

function(mine_setup_dependencies)
    # FreeType（字体渲染库）
    message(STATUS "[1/8] 配置 FreeType...")
    set(FT_DISABLE_ZLIB ON CACHE BOOL "" FORCE)
    set(FT_DISABLE_BZIP2 ON CACHE BOOL "" FORCE)
    set(FT_DISABLE_PNG ON CACHE BOOL "" FORCE)
    set(FT_DISABLE_HARFBUZZ ON CACHE BOOL "" FORCE)
    set(FT_DISABLE_BROTLI ON CACHE BOOL "" FORCE)
    
    mine_fetch_content(freetype
        SYSTEM_PACKAGE Freetype
        GIT_TAG "master"
        NO_SUBMODULES
        GIT_MIRRORS
            "https://gitcode.com/gh_mirrors/free/freetype.git"
            "https://github.com/freetype/freetype.git"
            "https://hub.njuu.cf/freetype/freetype.git"
    )

    if(FREETYPE_INCLUDE_DIRS)
        set(FREETYPE_INCLUDE_DIRS ${FREETYPE_INCLUDE_DIRS} PARENT_SCOPE)
    endif()
    if(FREETYPE_LIBRARIES)
        set(FREETYPE_LIBRARIES ${FREETYPE_LIBRARIES} PARENT_SCOPE)
    endif()
    
    if(TARGET freetype)
        if(freetype_SOURCE_DIR)
            set(FREETYPE_INCLUDE_DIRS "${freetype_SOURCE_DIR}/include" CACHE INTERNAL "FreeType 包含目录")
            set(FREETYPE_LIBRARIES freetype CACHE INTERNAL "FreeType 库")
        endif()
    endif()

    # HarfBuzz（文字整形库）
    message(STATUS "[2/8] 配置 HarfBuzz...")
    set(HB_HAVE_FREETYPE OFF CACHE BOOL "" FORCE)
    set(HB_BUILD_SUBSET OFF CACHE BOOL "" FORCE)
    set(HB_BUILD_UTILS OFF CACHE BOOL "" FORCE)
    
    mine_fetch_content(harfbuzz
        SYSTEM_PACKAGE harfbuzz
        GIT_TAG "main"
        GIT_MIRRORS
            "https://gitcode.com/gh_mirrors/ha/harfbuzz.git"
            "https://github.com/harfbuzz/harfbuzz.git"
            "https://hub.njuu.cf/harfbuzz/harfbuzz.git"
    )

    if(HARFBUZZ_INCLUDE_DIRS)
        set(HARFBUZZ_INCLUDE_DIRS ${HARFBUZZ_INCLUDE_DIRS} PARENT_SCOPE)
    endif()
    if(HARFBUZZ_LIBRARIES)
        set(HARFBUZZ_LIBRARIES ${HARFBUZZ_LIBRARIES} PARENT_SCOPE)
    endif()
    
    if(TARGET harfbuzz)
        if(harfbuzz_SOURCE_DIR)
            set(HARFBUZZ_INCLUDE_DIRS "${harfbuzz_SOURCE_DIR}/src" CACHE INTERNAL "HarfBuzz 包含目录")
            set(HARFBUZZ_LIBRARIES harfbuzz CACHE INTERNAL "HarfBuzz 库")
        endif()
    endif()

    # utf8cpp（UTF-8 编码处理）
    message(STATUS "[3/8] 配置 utf8cpp...")
    mine_fetch_content(utf8cpp
        SYSTEM_PACKAGE utf8cpp
        GIT_TAG "master"
        GIT_MIRRORS
            "https://gitcode.com/gh_mirrors/ut/utfcpp.git"
            "https://github.com/nemtrif/utfcpp.git"
            "https://hub.njuu.cf/nemtrif/utfcpp.git"
    )
    
    if(utf8cpp_POPULATED)
        set(UTF8CPP_INCLUDE_DIR ${utf8cpp_SOURCE_DIR}/source PARENT_SCOPE)
    endif()

    # stb（单头文件图像库）
    message(STATUS "[4/8] 配置 stb...")
    mine_fetch_content(stb
        GIT_TAG "master"
        GIT_MIRRORS
            "https://gitcode.com/GitHub_Trending/st/stb.git"
            "https://github.com/nothings/stb.git"
            "https://hub.njuu.cf/nothings/stb.git"
    )
    
    if(stb_POPULATED)
        set(STB_INCLUDE_DIR ${stb_SOURCE_DIR} PARENT_SCOPE)
    endif()

    # libpng（PNG 图像格式）
    message(STATUS "[5/8] 配置 libpng...")
    set(PNG_SHARED OFF CACHE BOOL "" FORCE)
    set(PNG_TESTS OFF CACHE BOOL "" FORCE)
    
    # libpng 需要 zlib 依赖
    find_package(ZLIB QUIET)
    if(NOT ZLIB_FOUND)
        message(STATUS "      下载 zlib...")
        mine_fetch_content(zlib
            GIT_TAG "master"
            GIT_MIRRORS
                "https://gitcode.com/gh_mirrors/zl/zlib.git"
                "https://github.com/madler/zlib.git"
                "https://hub.njuu.cf/madler/zlib.git"
        )
        if(TARGET zlib)
            set(ZLIB_LIBRARY zlib)
            set(ZLIB_INCLUDE_DIR ${zlib_SOURCE_DIR})
        endif()
    endif()
    
    mine_fetch_content(libpng
        SYSTEM_PACKAGE PNG
        GIT_TAG "libpng16"
        GIT_MIRRORS
            "https://gitcode.com/gh_mirrors/li/libpng.git"
            "https://github.com/pnggroup/libpng.git"
            "https://hub.njuu.cf/pnggroup/libpng.git"
    )
    
    if(TARGET png_static)
        if(libpng_SOURCE_DIR AND libpng_BINARY_DIR)
            set(LIBPNG_INCLUDE_DIRS "${libpng_SOURCE_DIR};${libpng_BINARY_DIR}" CACHE INTERNAL "libpng 包含目录")
            set(LIBPNG_LIBRARIES png_static CACHE INTERNAL "libpng 库")
        endif()
    endif()

    # SQLite（嵌入式数据库）
    message(STATUS "[6/8] 配置 SQLite...")
    
    # 优先尝试系统 SQLite
    if(MINE_PREFER_SYSTEM_LIBS)
        find_package(SQLite3 QUIET)
        if(SQLite3_FOUND)
            message(STATUS "      ✓ 使用系统库: SQLite3")
            set(SQLITE_INCLUDE_DIR ${SQLite3_INCLUDE_DIRS} PARENT_SCOPE)
            set(SQLITE_LIBRARIES ${SQLite3_LIBRARIES} PARENT_SCOPE)
        endif()
    endif()
    
    # 降级方案：使用 Git 仓库克隆
    if(NOT SQLite3_FOUND)
        mine_fetch_content(
            sqlite
            GIT_MIRRORS
                "https://gitcode.com/gh_mirrors/sq/sqlite.git"
                "https://github.com/sqlite/sqlite.git"
                "https://hub.njuu.cf/sqlite/sqlite.git"
            GIT_TAG "master"
        )
        
        # SQLite 源码在 Git 仓库中，但需要使用预处理后的 amalgamation 版本
        # 检查是否存在 sqlite3.c（amalgamation 文件）
        if(EXISTS "${sqlite_SOURCE_DIR}/sqlite3.c")
            add_library(sqlite3 STATIC ${sqlite_SOURCE_DIR}/sqlite3.c)
            target_include_directories(sqlite3 PUBLIC ${sqlite_SOURCE_DIR})
            target_compile_definitions(sqlite3 PRIVATE
                SQLITE_THREADSAFE=1
                SQLITE_ENABLE_COLUMN_METADATA
                SQLITE_ENABLE_FTS5
                SQLITE_ENABLE_JSON1
                SQLITE_ENABLE_RTREE
            )
            set(SQLITE_INCLUDE_DIR ${sqlite_SOURCE_DIR} PARENT_SCOPE)
            set(SQLITE_LIBRARIES sqlite3 PARENT_SCOPE)
        else()
            message(WARNING "SQLite amalgamation 文件未找到，请使用 vcpkg 或系统包管理器安装")
        endif()
    endif()

    # mbedTLS（加密库）
    if(MINE_BUILD_NETWORK)
        message(STATUS "[7/8] 配置 mbedTLS...")
        set(ENABLE_PROGRAMS OFF CACHE BOOL "" FORCE)
        set(ENABLE_TESTING OFF CACHE BOOL "" FORCE)
        set(MBEDTLS_FATAL_WARNINGS OFF CACHE BOOL "" FORCE)
        
        mine_fetch_content(mbedtls
            SYSTEM_PACKAGE MbedTLS
            GIT_TAG "master"
            GIT_MIRRORS
                "https://gitcode.com/gh_mirrors/mb/mbedtls.git"
                "https://github.com/Mbed-TLS/mbedtls.git"
                "https://hub.njuu.cf/Mbed-TLS/mbedtls.git"
        )
        
        if(TARGET mbedtls)
            set(MBEDTLS_INCLUDE_DIRS ${mbedtls_SOURCE_DIR}/include PARENT_SCOPE)
            set(MBEDTLS_LIBRARIES mbedtls mbedx509 mbedcrypto PARENT_SCOPE)
        endif()
    endif()

    # doctest（测试框架）
    if(MINE_BUILD_TESTS)
        message(STATUS "[8/8] 配置 doctest...")
        mine_fetch_content(doctest
            GIT_TAG "master"
            GIT_MIRRORS
                "https://gitcode.com/gh_mirrors/do/doctest.git"
                "https://github.com/doctest/doctest.git"
                "https://hub.njuu.cf/doctest/doctest.git"
        )
        
        # doctest 的源目录已经通过 mine_fetch_content 传播到当前作用域
        if(doctest_SOURCE_DIR AND EXISTS "${doctest_SOURCE_DIR}")
            set(DOCTEST_INCLUDE_DIR "${doctest_SOURCE_DIR}" CACHE INTERNAL "doctest 包含目录")
            message(STATUS "      DOCTEST_INCLUDE_DIR = ${DOCTEST_INCLUDE_DIR}")
        endif()
    endif()
    
    message(STATUS "")
    message(STATUS "✓ 所有依赖配置完成！")
    message(STATUS "====================================================")
endfunction()

# ============================================================================
# 辅助函数
# ============================================================================

# 为目标添加常用依赖
function(mine_add_common_dependencies target)
    if(TARGET utf8cpp)
        target_include_directories(${target} PRIVATE ${UTF8CPP_INCLUDE_DIR})
    endif()
endfunction()
