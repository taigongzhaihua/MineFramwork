-- mine.localization 模块构建配置
-- 任务 22：本地化模块（多语言资源字典、运行时语言切换、tr() 函数）

mine_module("mine.localization", {
    short_name = "localization",
    deps = {
        "mine.core",
        "mine.containers",
    },
})
