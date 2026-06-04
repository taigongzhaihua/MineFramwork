# mine.paint 模块
# 2D 绘图、画刷、Path

mine_module("mine.paint"
    DEPENDENCIES 
        mine.core
        mine.math
        mine.text
        mine.gfx.rhi
    EXTERNAL_DEPS
        ${FREETYPE_LIBRARIES}
)

# 添加 FreeType 头文件
if(FREETYPE_INCLUDE_DIRS)
    target_include_directories(mine.paint PRIVATE ${FREETYPE_INCLUDE_DIRS})
elseif(TARGET freetype)
    get_target_property(_freetype_source_dir freetype SOURCE_DIR)
    if(_freetype_source_dir)
        target_include_directories(mine.paint PRIVATE
            ${_freetype_source_dir}/include
        )
    endif()
endif()

# 确保能找到 mine.text 公开头文件
if(EXISTS ${CMAKE_SOURCE_DIR}/src/mine/text/api/include)
    target_include_directories(mine.paint PRIVATE
        ${CMAKE_SOURCE_DIR}/src/mine/text/api/include
    )
endif()
