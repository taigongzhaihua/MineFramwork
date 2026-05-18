-- mine.paint：2D 绘制模块（L3 合成层入口）
-- 依赖：mine.core / mine.containers / mine.math / mine.gfx.rhi
-- 不直接依赖任何具体 RHI 后端，后端实现由 IRenderer 注入。

mine_module("mine.paint", {
    short_name = "paint",
    deps       = {"mine.core", "mine.containers", "mine.math", "mine.gfx.rhi"},
})
