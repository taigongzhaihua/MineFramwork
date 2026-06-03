# mine.platform.win32 模块
# Win32 平台实现

if(WIN32)
    mine_module("mine.platform.win32"
        DEPENDENCIES mine.platform.abi mine.diag
    )
    
    # 链接 Windows 系统库
    target_link_libraries(mine.platform.win32 PRIVATE
        user32
        gdi32
        shell32
        ole32
        oleaut32
        imm32
        dwmapi
        uxtheme
        d3d11
        dxgi
        dxguid
    )
endif()
