-- mine.ui.window 模块构建配置
-- 职责：Window 类（UI 窗口包装，桥接 platform::IWindow、GFX 渲染管线与视觉树）
-- 依赖：mine.core / mine.math
--       mine.platform.abi（IWindow / IApplicationHost / IWindowEventSink）
--       mine.gfx.rhi（IDevice / IQueue / ISwapchain / ITexture）
--       mine.paint（IRenderer / Canvas / Brush / DisplayList）
--       mine.ui.visual（UIElement / Visual — 视觉树与渲染树基类）
--       mine.ui.layout（FrameworkElement — 布局协议）

mine_module("mine.ui.window", {
    short_name = "ui.window",
    deps = {
        "mine.core",
        "mine.math",
        "mine.platform.abi",
        "mine.gfx.rhi",
        "mine.paint",
        "mine.ui.visual",
        "mine.ui.layout",
    },
})
