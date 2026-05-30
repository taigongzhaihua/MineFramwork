mine_module("mine.ui.visual", {
    short_name = "ui.visual",
    deps       = {
        "mine.core",
        "mine.containers",
        "mine.math",
        "mine.paint",
        "mine.ui.property",
        "mine.ui.binding",  -- FrameworkElement 内置绑定存储（set_binding / clear_all_bindings）
        "mine.ui.event",
        "mine.ui.style",   -- Control 依赖 ControlTemplate / TemplateRegistry / VisualStateManager
    }
})
