-- mine.ui.layout 模块构建配置
-- 依赖：mine.core / mine.containers / mine.math
--       mine.ui.property（DependencyProperty、DependencyObject）
--       mine.ui.visual（UIElement、Visual — 布局协议基类）

mine_module("mine.ui.layout", {
    short_name = "ui.layout",
    deps = {
        "mine.core",
        "mine.containers",
        "mine.math",
        "mine.ui.property",
        "mine.ui.visual",
        "mine.ui.style",   -- Control 依赖 ControlTemplate / TemplateRegistry / VisualStateManager
    },
})
