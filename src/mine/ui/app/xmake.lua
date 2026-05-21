-- mine.ui.app 模块构建配置
-- 职责：Application 基类（平台宿主 + 图形基础设施启动、主循环管理、窗口生命周期）
-- 依赖：mine.core / mine.containers
--       mine.platform.abi（IApplicationHost / IWindow / WindowDesc）
--       mine.gfx.rhi（IDevice / IQueue / Backend）
--       mine.paint（IRenderer）
--       mine.ui.window（Window — UI 窗口包装）

mine_module("mine.ui.app", {
    short_name = "ui.app",
    deps = {
        "mine.core",
        "mine.containers",
        "mine.platform.abi",
        "mine.gfx.rhi",
        "mine.paint",
        "mine.ui.window",
    },
})
