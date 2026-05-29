-- mine.logging 模块构建配置
-- 任务 23：日志 sink 模块（ConsoleSink + FileSink，对接 mine.diag::Logger）

mine_module("mine.logging", {
    short_name = "logging",
    deps = {
        "mine.core",
        "mine.diag",
        -- 不依赖 mine.io（尚未完整实现），文件 I/O 直接使用 C 标准库 FILE*
    },
})
