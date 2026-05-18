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

    -- 链接平台后端（自动传递依赖：mine.platform.abi / mine.core / mine.math）
    add_deps("mine.platform.win32")

    add_files("main.cpp")

    -- Win32 GUI 子系统（不显示控制台窗口）
    if is_plat("windows") then
        add_defines("UNICODE", "_UNICODE", "NOMINMAX", "WIN32_LEAN_AND_MEAN")
        add_cxflags("/utf-8", {tools = {"cl", "clang_cl"}})
        add_ldflags("/SUBSYSTEM:WINDOWS", {tools = {"link", "lld-link"}})
    end
target_end()
