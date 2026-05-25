mine_module("mine.ui.visual", {
    short_name = "ui.visual",
    deps       = {
        "mine.core",
        "mine.containers",
        "mine.math",
        "mine.paint",
        "mine.ui.property",
        "mine.ui.event",
        -- mine.ui.style 已移除：Control 已迁移至 mine.ui.layout，由 layout 承担 style 依赖
    }
})

-- visual.test 仍需测试 Control（通过转发头 visual/Control.h → layout/Control.h），
-- 因此额外依赖 mine.ui.layout 以获取 layout 头文件路径和 Control 实现
target("mine.ui.visual.test")
    add_deps("mine.ui.layout")
target_end()
