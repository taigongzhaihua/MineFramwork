-- mine.ui.style 模块构建配置
-- F2.1 任务 #10：ResourceDictionary（树形作用域、resource_changed 信号）

mine_module("mine.ui.style", {
    short_name = "ui.style",
    deps = {
        "mine.core",
        "mine.containers",
    },
})
