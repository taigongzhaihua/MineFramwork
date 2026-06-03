# mine.paint 模块
# 2D 绘图、画刷、Path

mine_module("mine.paint"
    DEPENDENCIES 
        mine.core
        mine.math
        mine.gfx.rhi
    EXTERNAL_DEPS
        ${FREETYPE_LIBRARIES}
)

# 添加 FreeType 头文件
if(FREETYPE_INCLUDE_DIRS)
    target_include_directories(mine.paint PRIVATE ${FREETYPE_INCLUDE_DIRS})
endif()
