# MineFramework 第三方依赖管理（使用国内镜像）
# 所有第三方库优先从 Gitee/清华镜像获取，确保国内访问速度

include(FetchContent)

# 设置 FetchContent 全局策略
set(FETCHCONTENT_QUIET OFF CACHE BOOL "" FORCE)
set(FETCHCONTENT_UPDATES_DISCONNECTED ON CACHE BOOL "" FORCE)

# 主依赖配置函数
function(mine_setup_dependencies)
    message(STATUS "")
    message(STATUS "==================================================")
    message(STATUS "配置 MineFramework 第三方依赖（使用国内镜像）")
    message(STATUS "==================================================")
    
    # ===== 1. doctest（测试框架）=====
    if(MINE_BUILD_TESTS)
        message(STATUS "[1/8] 配置 doctest...")
        FetchContent_Declare(
            doctest
            GIT_REPOSITORY https://gitee.com/mirrors/doctest.git
            GIT_TAG master
            GIT_SHALLOW TRUE
            GIT_PROGRESS TRUE
        )
        FetchContent_MakeAvailable(doctest)
        set(DOCTEST_INCLUDE_DIR ${doctest_SOURCE_DIR} PARENT_SCOPE)
        message(STATUS "      ✓ doctest 配置完成")
    endif()
    
    # ===== 2. FreeType（字体渲染）=====
    message(STATUS "[2/8] 配置 FreeType...")
    set(FT_DISABLE_ZLIB ON CACHE BOOL "" FORCE)
    set(FT_DISABLE_BZIP2 ON CACHE BOOL "" FORCE)
    set(FT_DISABLE_PNG ON CACHE BOOL "" FORCE)
    set(FT_DISABLE_HARFBUZZ ON CACHE BOOL "" FORCE)
    set(FT_DISABLE_BROTLI ON CACHE BOOL "" FORCE)
    
    FetchContent_Declare(
        freetype
        GIT_REPOSITORY https://gitee.com/mirrors/freetype.git
        GIT_TAG master
        GIT_SHALLOW TRUE
        GIT_PROGRESS TRUE
    )
    FetchContent_MakeAvailable(freetype)
    set(FREETYPE_INCLUDE_DIRS ${freetype_SOURCE_DIR}/include PARENT_SCOPE)
    set(FREETYPE_LIBRARIES freetype PARENT_SCOPE)
    message(STATUS "      ✓ FreeType 配置完成")
    
    # ===== 3. HarfBuzz（文本整形）=====
    message(STATUS "[3/8] 配置 HarfBuzz...")
    set(HB_HAVE_FREETYPE OFF CACHE BOOL "" FORCE)
    set(HB_BUILD_SUBSET OFF CACHE BOOL "" FORCE)
    set(HB_BUILD_UTILS OFF CACHE BOOL "" FORCE)
    
    FetchContent_Declare(
        harfbuzz
        GIT_REPOSITORY https://gitee.com/mirrors/harfbuzz.git
        GIT_TAG main
        GIT_SHALLOW TRUE
        GIT_PROGRESS TRUE
    )
    FetchContent_MakeAvailable(harfbuzz)
    set(HARFBUZZ_INCLUDE_DIRS ${harfbuzz_SOURCE_DIR}/src PARENT_SCOPE)
    set(HARFBUZZ_LIBRARIES harfbuzz PARENT_SCOPE)
    message(STATUS "      ✓ HarfBuzz 配置完成")
    
    # ===== 4. utf8cpp（UTF-8 处理）=====
    message(STATUS "[4/8] 配置 utf8cpp...")
    FetchContent_Declare(
        utf8cpp
        GIT_REPOSITORY https://gitee.com/mirrors/utfcpp.git
        GIT_TAG master
        GIT_SHALLOW TRUE
        GIT_PROGRESS TRUE
    )
    FetchContent_MakeAvailable(utf8cpp)
    set(UTF8CPP_INCLUDE_DIR ${utf8cpp_SOURCE_DIR}/source PARENT_SCOPE)
    message(STATUS "      ✓ utf8cpp 配置完成")
    
    # ===== 5. stb（STB 单头文件库）=====
    message(STATUS "[5/8] 配置 stb...")
    FetchContent_Declare(
        stb
        GIT_REPOSITORY https://gitee.com/mirrors/stb.git
        GIT_TAG master
        GIT_SHALLOW TRUE
        GIT_PROGRESS TRUE
    )
    FetchContent_MakeAvailable(stb)
    set(STB_INCLUDE_DIR ${stb_SOURCE_DIR} PARENT_SCOPE)
    message(STATUS "      ✓ stb 配置完成")
    
    # ===== 6. libpng（PNG 支持）=====
    message(STATUS "[6/8] 配置 libpng...")
    set(PNG_SHARED OFF CACHE BOOL "" FORCE)
    set(PNG_TESTS OFF CACHE BOOL "" FORCE)
    set(PNG_BUILD_ZLIB ON CACHE BOOL "" FORCE)
    
    FetchContent_Declare(
        libpng
        GIT_REPOSITORY https://gitee.com/mirrors/libpng.git
        GIT_TAG libpng16
        GIT_SHALLOW TRUE
        GIT_PROGRESS TRUE
    )
    FetchContent_MakeAvailable(libpng)
    set(LIBPNG_INCLUDE_DIRS ${libpng_SOURCE_DIR} ${libpng_BINARY_DIR} PARENT_SCOPE)
    set(LIBPNG_LIBRARIES png_static PARENT_SCOPE)
    message(STATUS "      ✓ libpng 配置完成")
    
    # ===== 7. SQLite（数据库）=====
    message(STATUS "[7/8] 配置 SQLite...")
    FetchContent_Declare(
        sqlite
        URL https://mirrors.tuna.tsinghua.edu.cn/sqlite/2024/sqlite-amalgamation-3460100.zip
        DOWNLOAD_EXTRACT_TIMESTAMP TRUE
    )
    FetchContent_MakeAvailable(sqlite)
    
    # 创建 SQLite 静态库目标
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
    message(STATUS "      ✓ SQLite 配置完成")
    
    # ===== 8. mbedTLS（加密库）=====
    if(MINE_BUILD_NETWORK)
        message(STATUS "[8/8] 配置 mbedTLS...")
        set(ENABLE_PROGRAMS OFF CACHE BOOL "" FORCE)
        set(ENABLE_TESTING OFF CACHE BOOL "" FORCE)
        set(MBEDTLS_FATAL_WARNINGS OFF CACHE BOOL "" FORCE)
        
        FetchContent_Declare(
            mbedtls
            GIT_REPOSITORY https://gitee.com/mirrors/mbedtls.git
            GIT_TAG master
            GIT_SHALLOW TRUE
            GIT_PROGRESS TRUE
        )
        FetchContent_MakeAvailable(mbedtls)
        set(MBEDTLS_INCLUDE_DIRS ${mbedtls_SOURCE_DIR}/include PARENT_SCOPE)
        set(MBEDTLS_LIBRARIES mbedtls mbedx509 mbedcrypto PARENT_SCOPE)
        message(STATUS "      ✓ mbedTLS 配置完成")
    endif()
    
    message(STATUS "==================================================")
    message(STATUS "✓ 所有第三方依赖配置完成")
    message(STATUS "==================================================")
    message(STATUS "")
endfunction()

# 辅助函数：为目标添加常用依赖
function(mine_add_common_dependencies target)
    # UTF-8 支持
    if(TARGET utf8cpp)
        target_include_directories(${target} PRIVATE ${UTF8CPP_INCLUDE_DIR})
    endif()
endfunction()
