-- mine.config 模块构建配置
-- 任务 21：分层配置模块（默认/文件/环境变量三层，支持 JSON/TOML 加载）

mine_module("mine.config", {
    short_name = "config",
    deps = {
        "mine.core",
        "mine.containers",
        -- 不依赖 mine.io（尚未完整实现），直接使用 C 标准库 FILE* 读文件
    },
})
