# 01-mvvm-binding 示例
# MVVM 数据绑定演示

add_executable(sample.01-mvvm-binding
    main.cpp
)

target_link_libraries(sample.01-mvvm-binding PRIVATE
    mine.ui.app
    mine.ui.window
    mine.ui.controls
    mine.mvvm
    mine.platform.win32
    mine.gfx.d3d11
)

mine_set_compiler_flags(sample.01-mvvm-binding)

# 设置工作目录
set_target_properties(sample.01-mvvm-binding PROPERTIES
    VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
)
