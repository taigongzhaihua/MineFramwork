-- 00-blank-window 示例：创建一个空白 Win32 窗口
-- 仅 Windows 平台可用

if not is_plat("windows") then
    return
end

target("sample.00-blank-window")
    set_group("samples")
    set_kind("binary")
    set_default(false)
    set_languages("cxx20")
    set_warnings("allextra")

    -- 链接平台后端与 D3D11 图形后端（自动传递依赖：mine.gfx.rhi / mine.core / mine.math）
    add_deps("mine.platform.win32", "mine.gfx.d3d11")

    add_files("main.cpp")

    -- Win32 平台基础编译选项
    if is_plat("windows") then
        add_defines("UNICODE", "_UNICODE", "NOMINMAX", "WIN32_LEAN_AND_MEAN")
        add_cxflags("/utf-8", {tools = {"cl", "clang_cl"}})
        -- 开发阶段使用默认 CONSOLE 子系统，xmake run 可直接启动。
        -- 发布时若需隐藏控制台窗口，可改回：
        --   add_ldflags("/SUBSYSTEM:WINDOWS", "/ENTRY:mainCRTStartup", ...)
    end
target_end()
