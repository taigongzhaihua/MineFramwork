# 00-blank-window 示例
# 空白窗口演示

add_executable(sample.00-blank-window
    main.cpp
)

target_link_libraries(sample.00-blank-window PRIVATE
    mine.ui.app
    mine.ui.window
    mine.platform.win32
    mine.gfx.d3d11
)

mine_set_compiler_flags(sample.00-blank-window)

set_target_properties(sample.00-blank-window PROPERTIES
    VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
)
