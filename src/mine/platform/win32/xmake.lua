-- mine.platform.win32：Win32 平台窗口后端实现
-- 仅 Windows 平台编译，依赖 mine.platform.abi、mine.core、mine.math

if not is_plat("windows") then
    return
end

mine_module("mine.platform.win32", {
    short_name = "platform.win32",
    deps = {"mine.platform.abi", "mine.core", "mine.math"},
})

-- mine_module 不直接支持 syslinks，追加一次 target 块补充系统链接库
target("mine.platform.win32")
    -- Win32 API 所需系统库
    add_syslinks(
        "user32",    -- 窗口、消息循环、DPI 相关 API
        "gdi32",     -- 图形设备接口（位图、字体等）
        "ole32",     -- 剪贴板（OLE 剪贴板格式）
        "shell32",   -- ShellExecute、SHELLAPI 等
        "dwmapi",    -- 桌面窗口管理器（毛玻璃效果、DWM 合成等）
        "imm32",     -- 输入法管理器
        "shcore"     -- GetDpiForMonitor、SetProcessDpiAwareness 等
    )
    -- 添加公共 API 头文件目录（工厂函数声明）
    add_includedirs("api/include", {public = true})
    add_headerfiles("api/include/(**.h)")
target_end()
