# mine.text 模块
# Unicode、文本处理

mine_module("mine.text"
    DEPENDENCIES mine.core
    EXTERNAL_DEPS utf8cpp freetype harfbuzz
)

# FreeType 头文件会出现在公共接口中，需要对下游可见
if(TARGET freetype)
    FetchContent_GetProperties(freetype)
    if(freetype_SOURCE_DIR)
        target_include_directories(mine.text PUBLIC
            ${freetype_SOURCE_DIR}/include
        )
    endif()
endif()

# HarfBuzz 头文件会出现在公共接口中，需要对下游可见
if(TARGET harfbuzz)
    FetchContent_GetProperties(harfbuzz)
    if(harfbuzz_SOURCE_DIR)
        target_include_directories(mine.text PUBLIC
            ${harfbuzz_SOURCE_DIR}/src
        )
    endif()
endif()
