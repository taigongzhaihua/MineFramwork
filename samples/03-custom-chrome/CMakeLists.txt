# 03-custom-chrome 示例
# 自定义窗口样式演示

# 自动收集所有源文件
file(GLOB SAMPLE_SOURCES *.cpp)

add_executable(sample.03-custom-chrome ${SAMPLE_SOURCES})

target_link_libraries(sample.03-custom-chrome PRIVATE
    mine.ui.app
    mine.ui.window
    mine.ui.controls
    mine.platform.win32
    mine.gfx.d3d11
)

mine_set_compiler_flags(sample.03-custom-chrome)

set_target_properties(sample.03-custom-chrome PROPERTIES
    VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
)
