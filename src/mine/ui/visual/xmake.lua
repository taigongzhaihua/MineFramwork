mine_module("mine.ui.visual", {
    short_name = "ui.visual",
    deps       = {
        "mine.core",
        "mine.containers",
        "mine.math",
        "mine.paint",
        "mine.ui.property",
        "mine.ui.event",
        "mine.ui.style",  -- Control 依赖 ControlTemplate / TemplateRegistry（任务 #12）
    }
})
