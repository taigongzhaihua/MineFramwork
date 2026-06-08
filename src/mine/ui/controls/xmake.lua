-- mine.ui.controls 模块构建配置
-- M1.1 任务 #23：基础控件（TextBlock / Button / Border）
-- 复用 mine.ui.layout 中的 StackPanel / Grid
-- M1.2 任务 #15：添加 mine.ui.style 依赖（TemplateRegistry 用于 ContentPresenter/Button 默认模板注册）

mine_module("mine.ui.controls", {
    short_name = "ui.controls",
    deps = {
        "mine.core",
        "mine.containers",
        "mine.math",
        "mine.paint",
        "mine.text",          -- TextBlock::on_measure 调用 FontFace::measure_text() 需要
        "mine.ui.event",
        "mine.ui.input",
        "mine.ui.property",
        "mine.ui.visual",
        "mine.ui.layout",
        "mine.ui.style",      -- ContentPresenter 和 Button 模板注册依赖 TemplateRegistry
        "mine.ui.animation",  -- Button 背景色过渡动画（Storyboard + EasingFunction）
        "mine.ui.window",     -- TextBox IME 支持需要 Window::ime() 访问 IMEService
        "mine.platform.abi",  -- IMEService 头文件依赖
    },
    packages    = {"harfbuzz"},  -- TextBox/TextBlock 包含 FontFace.h（含 hb.h）
})
