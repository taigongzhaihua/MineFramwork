-- mine.diag 模块构建配置
-- 诊断基础设施：Logger（日志接口/全局调度器）、MINE_ASSERT 等

mine_module("mine.diag", {
    short_name = "diag",
    deps = { "mine.core" },
})