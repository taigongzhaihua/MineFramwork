-- mine.paint：2D 绘制模块（L3 合成层入口）
-- 仅提供接口层（IRenderer / DisplayList / Brush / Canvas / Path 等），
-- 不依赖任何具体 RHI 后端。渲染器实现由 mine.paint.d3d11 等后端模块提供。
-- 依赖：mine.core / mine.containers / mine.math / mine.gfx.rhi

mine_module("mine.paint", {
    short_name = "paint",
    deps       = {"mine.core", "mine.containers", "mine.math", "mine.gfx.rhi"},
})
