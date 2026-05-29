-- mine.di 模块构建配置
-- 依赖注入/IoC 容器，提供 ServiceCollection / ServiceProvider / Scope / MINE_INJECT 宏

mine_module("mine.di", {
    short_name = "di",
    deps = {
        "mine.core",
        "mine.containers",
    },
})
