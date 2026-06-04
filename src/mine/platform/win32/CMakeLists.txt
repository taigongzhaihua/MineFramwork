# mine.platform.win32 模块
# Win32 平台实现

if(WIN32)
    mine_module("mine.platform.win32"
        DEPENDENCIES mine.platform.abi mine.diag
    )
    
    # 定义 Unicode 字符集和 Windows 版本（需要 Win10 1703+ API）
    target_compile_definitions(mine.platform.win32 PRIVATE
        UNICODE
        _UNICODE
        _WIN32_WINNT=0x0A02
        WINVER=0x0A02
    )

    # 防止未使用参数警告在 /WX 下变成错误
    target_compile_options(mine.platform.win32 PRIVATE /WX- /wd4100)
    
    # 链接 Windows 系统库
    target_link_libraries(mine.platform.win32 PRIVATE
        user32
        gdi32
        shell32
        shcore
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
