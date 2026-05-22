-- 01-vsm-animation 示例：VisualStateManager + Storyboard 属性动画演示
-- 演示鼠标悬停/按压触发 VSM 状态切换，动画平滑插值 DependencyProperty（Color）
-- 依赖：mine.platform.win32 + mine.gfx.d3d11 + mine.paint
--       mine.ui.property + mine.ui.style + mine.ui.animation
-- 仅 Windows 平台可用

if not is_plat("windows") then
    return
end

target("sample.01-vsm-animation")
    set_group("samples")
    set_kind("binary")
    set_default(false)
    set_languages("cxx20")
    set_warnings("allextra")

    -- 依赖项：
    --   mine.platform.win32  → Win32 窗口 + 消息循环
    --   mine.gfx.d3d11       → D3D11 后端（自动传递 mine.gfx.rhi）
    --   mine.paint           → 2D 绘制抽象（Canvas / IRenderer 等）
    --   mine.ui.property     → DependencyObject / DependencyProperty / ValuePriority
    --   mine.ui.style        → VisualStateManager / Style / StyleSetter
    --   mine.ui.animation    → Storyboard / EasingFunction / PropertyAnimation
    add_deps(
        "mine.platform.win32",
        "mine.gfx.d3d11",
        "mine.paint",
        "mine.ui.property",
        "mine.ui.style",
        "mine.ui.animation"
    )

    add_files("main.cpp")

    -- Win32 平台编译选项
    if is_plat("windows") then
        add_defines("UNICODE", "_UNICODE", "NOMINMAX", "WIN32_LEAN_AND_MEAN")
        add_cxflags("/utf-8", {tools = {"cl", "clang_cl"}})
        -- 使用 CONSOLE 子系统，xmake run 可直接启动并打印错误信息
    end
target_end()
