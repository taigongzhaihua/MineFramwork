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
        "mine.ui.event",
        "mine.ui.input",
        "mine.ui.property",
        "mine.ui.visual",
        "mine.ui.layout",
        "mine.ui.style",  -- ContentPresenter 和 Button 模板注册依赖 TemplateRegistry
    },
})
