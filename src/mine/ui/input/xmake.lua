-- mine.ui.input 模块构建配置
-- 依赖：mine.core / mine.containers / mine.math
--       mine.ui.event（路由事件、EventManager）
--       mine.ui.visual（UIElement 命中测试）
--       mine.platform.abi（WindowEvent、IWindowEventSink 接口）

mine_module("mine.ui.input", {
    short_name = "ui.input",
    deps = {
        "mine.core",
        "mine.containers",
        "mine.math",
        "mine.ui.event",
        "mine.ui.visual",
        "mine.platform.abi",
    },
})
