---
-- mine.gfx.rhi：图形渲染接口抽象层（纯头文件）
--
-- 声明 IDevice / ISwapchain / ICommandList / IQueue 等纯虚接口；
-- 不依赖任何具体图形 API，后端实现见 mine.gfx.d3d11 / d3d12 / vulkan。
---

mine_module("mine.gfx.rhi", {
    short_name = "gfx.rhi",
    deps       = { "mine.core", "mine.math" },
})
