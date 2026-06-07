-- mine.paint.d3d11：Direct3D 11 2D 渲染后端模块
-- 实现 IRenderer 接口，将 DisplayList 转换为 D3D11 GPU 绘制调用。
-- 依赖：mine.paint（IRenderer/DisplayList 接口）、mine.gfx.d3d11（着色器字节码）
--       mine.gfx.rhi（IDevice/IPipeline 抽象）、mine.text（字形光栅化）

if not is_plat("windows") then
    return
end

mine_module("mine.paint.d3d11", {
    short_name = "paint.d3d11",
    deps = {
        "mine.paint",       -- IRenderer / DisplayList / Brush / Canvas 等接口
        "mine.gfx.d3d11",   -- 预编译着色器字节码（ShaderBytecode.h）
        "mine.gfx.rhi",     -- IDevice / IPipeline / IBuffer / ITexture / ICommandList
        "mine.text",        -- FontFace 字形光栅化（GlyphAtlas 实现）
        "mine.core",        -- MINE_NEW / OwnedPtr / Span
        "mine.containers",  -- Vector
        "mine.math",        -- Vec2 / Transform2D / Color
    },
})
