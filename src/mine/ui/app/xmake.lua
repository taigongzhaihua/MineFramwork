-- mine.ui.app 模块构建配置
-- 职责：Application 基类（平台宿主 + 图形基础设施启动、主循环管理、窗口生命周期）
-- 依赖：mine.core / mine.containers
--       mine.platform.abi（IApplicationHost / IWindow / WindowDesc）
--       mine.gfx.rhi（IDevice / IQueue / Backend）
--       mine.paint（IRenderer）
--       mine.ui.window（Window — UI 窗口包装）
--       mine.ui.style（ResourceDictionary — 主题资源字典，Task #14）
--       mine.ui.animation（AnimationClock::tick_all — Application::tick_and_render 驱动）
--       mine.text（FontFace — Application::default_font 默认系统字体加载）

mine_module("mine.ui.app", {
    short_name = "ui.app",
    deps = {
        "mine.core",
        "mine.containers",
        "mine.platform.abi",
        "mine.gfx.rhi",
        "mine.paint",
        "mine.paint.d3d11",    -- create_renderer 工厂实现（D3D11 渲染后端）
        "mine.ui.window",
        "mine.ui.style",       -- 主题资源字典（Application::set_theme / global_resources）
        "mine.ui.animation",   -- AnimationClock::tick_all（Application::tick_and_render 驱动）
        "mine.text",           -- FontFace（Application::default_font 默认系统字体）
    },
})
