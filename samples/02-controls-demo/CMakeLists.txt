# 02-controls-demo 示例
# 控件演示

add_executable(sample.02-controls-demo
    main.cpp
)

target_link_libraries(sample.02-controls-demo PRIVATE
    mine.ui.app
    mine.ui.window
    mine.ui.controls
    mine.ui.style
    mine.platform.win32
    mine.gfx.d3d11
)

mine_set_compiler_flags(sample.02-controls-demo)

set_target_properties(sample.02-controls-demo PROPERTIES
    VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
)
