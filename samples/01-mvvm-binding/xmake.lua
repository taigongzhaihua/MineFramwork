-- samples/01-mvvm-binding 构建配置
-- 职责：演示 mine.mvvm（MINE_OBSERVABLE / RelayCommand / ViewModelBase）
--       与 mine.ui.binding（BindingExpression）在 UI 窗口中的协作

target("sample.01-mvvm-binding")
    set_group("samples")
    set_kind("binary")
    set_default(false)
    set_languages("cxx20")
    set_warnings("allextra")

    -- 直接依赖（传递依赖由 xmake 自动展开）
    add_deps(
        "mine.platform.win32",   -- 平台窗口 + 消息循环
        "mine.gfx.d3d11",        -- D3D11 渲染后端
        "mine.paint",            -- 2D 渲染器（Canvas / Brush）
        "mine.text",             -- 字体光栅化（FontFace）
        "mine.ui.app",           -- mine::ui::app::Application 应用基类
        "mine.ui.window",        -- mine::ui::Window（UI 窗口包装）
        "mine.ui.layout",        -- StackPanel / FrameworkElement
        "mine.ui.controls",      -- Button / TextBlock / Border
        "mine.ui.input",         -- InputRouter
        "mine.ui.animation",     -- Storyboard（VisualStateManager 依赖）
        "mine.ui.binding",       -- BindingExpression / PropertyDependency
        "mine.mvvm"              -- ViewModelBase / MINE_OBSERVABLE / RelayCommand
    )

    -- CounterViewModel.h 为纯头文件，无需加入编译列表
    add_files("CounterWindow.cpp", "main.cpp")

    -- Win32 字符集与编译选项
    add_defines("UNICODE", "_UNICODE", "NOMINMAX", "WIN32_LEAN_AND_MEAN")
    add_cxflags("/utf-8", { tools = { "cl", "clang_cl" } })
