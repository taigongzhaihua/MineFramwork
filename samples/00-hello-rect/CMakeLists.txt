# 00-hello-rect 示例
# 矩形绘制演示

add_executable(sample.00-hello-rect
    main.cpp
)

target_link_libraries(sample.00-hello-rect PRIVATE
    mine.paint
    mine.platform.win32
    mine.gfx.d3d11
)

mine_set_compiler_flags(sample.00-hello-rect)

set_target_properties(sample.00-hello-rect PROPERTIES
    VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
)
