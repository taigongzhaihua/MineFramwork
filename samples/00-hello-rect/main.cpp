/**
 * @file main.cpp
 * @brief 00-hello-rect 示例：mine.paint Canvas 所有已实现填充命令的综合演示。
 *
 * 演示内容（2×2 网格布局）：
 *   左上：FillRect             — 纯色填充矩形（绿色）
 *   右上：FillRoundedRect      — 均匀圆角矩形（蓝色，radius=24px）
 *   左下：FillComplexRoundedRect — 四角独立椭圆半径（橙色，对角线不对称）
 *   右下：FillEllipse           — 椭圆填充（紫色）
 */

#include <mine/platform/PlatformAbi.h>
#include <mine/gfx/Gfx.h>
#include <mine/paint/Paint.h>

#include <cstdio>
#include <cmath>

// ── 渲染器 ───────────────────────────────────────────────────────────────────

/**
 * @brief Hello-Rect 渲染器，封装 RHI 设备 + Paint 渲染器 + 交换链。
 */
struct HelloRectRenderer : public mine::platform::IWindowEventSink {
    mine::core::OwnedPtr<mine::gfx::IDevice>     device;
    mine::core::OwnedPtr<mine::gfx::IQueue>       queue;
    mine::core::OwnedPtr<mine::gfx::ISwapchain>   swapchain;
    mine::core::OwnedPtr<mine::gfx::ICommandList> cmd_list;   ///< 用于背景清除
    mine::core::OwnedPtr<mine::paint::IRenderer>  paint_renderer;

    // IWindowEventSink：响应窗口尺寸变化事件
    void on_window_event(mine::platform::WindowEvent& event) override {
        if (event.kind == mine::platform::WindowEventKind::Resized) {
            const auto w = static_cast<uint32_t>(std::max(1.0f, event.new_size.width));
            const auto h = static_cast<uint32_t>(std::max(1.0f, event.new_size.height));

            // 等待 GPU 空闲后再 resize 交换链
            queue->wait_idle();
            swapchain->resize(w, h);

            // resize 后立即重新渲染一帧
            render();
        }
    }

    /**
     * @brief 渲染一帧：清屏 + 2×2 网格展示所有填充命令 + 呈现。
     */
    void render() {
        if (!device || !swapchain || !paint_renderer) return;

        mine::gfx::ITexture* back_buf = swapchain->current_render_target();
        if (!back_buf) return;

        const float w = static_cast<float>(swapchain->width());
        const float h = static_cast<float>(swapchain->height());

        // ── 1. 构建绘制命令列表 ────────────────────────────────────────────

        mine::paint::Canvas canvas;

        // 背景：深灰色覆盖整个视口
        canvas.fill_rect(
            mine::math::Rect{0.0f, 0.0f, w, h},
            mine::paint::Brush::solid(mine::math::Color{0.12f, 0.12f, 0.12f, 1.0f}));

        // 每个格子的尺寸（2×2 分割，留出 padding）
        const float pad   = 24.0f;   // 格子间距和边距
        const float cw    = (w - pad * 3.0f) * 0.5f;  // 格子宽度
        const float ch    = (h - pad * 3.0f) * 0.5f;  // 格子高度

        // 各格子左上角坐标
        const float x0 = pad;
        const float x1 = pad * 2.0f + cw;
        const float y0 = pad;
        const float y1 = pad * 2.0f + ch;

        // 形状尺寸（在格子内留出内边距）
        const float inner_pad = 24.0f;
        const float sw = cw - inner_pad * 2.0f;  // 形状宽度
        const float sh = ch - inner_pad * 2.0f;  // 形状高度

        // ── 左上：FillRect（纯色矩形，绿色）─────────────────────────────
        canvas.fill_rect(
            mine::math::Rect{x0 + inner_pad, y0 + inner_pad, sw, sh},
            mine::paint::Brush::solid(mine::math::Color{0.20f, 0.75f, 0.35f, 1.0f}));

        // ── 右上：FillRoundedRect（均匀圆角矩形，蓝色）───────────────────
        canvas.fill_rounded_rect(
            mine::math::RoundedRect{
                mine::math::Rect{x1 + inner_pad, y0 + inner_pad, sw, sh},
                24.0f   // 四角统一 24px 圆角半径
            },
            mine::paint::Brush::solid(mine::math::Color{0.20f, 0.45f, 0.90f, 1.0f}));

        // ── 左下：FillComplexRoundedRect（四角独立椭圆半径，橙色）────────
        canvas.fill_complex_rounded_rect(
            mine::math::ComplexRoundedRect{
                mine::math::Rect{x0 + inner_pad, y1 + inner_pad, sw, sh},
                mine::math::CornerRadii{
                    {40.0f, 40.0f},  // 左上角：大圆角
                    {8.0f,  8.0f},   // 右上角：小圆角
                    {40.0f, 40.0f},  // 右下角：大圆角
                    {8.0f,  8.0f}    // 左下角：小圆角
                }
            },
            mine::paint::Brush::solid(mine::math::Color{0.95f, 0.55f, 0.10f, 1.0f}));

        // ── 右下：FillEllipse（椭圆，紫色）───────────────────────────────
        const float ecx = x1 + inner_pad + sw * 0.5f;  // 椭圆中心 X
        const float ecy = y1 + inner_pad + sh * 0.5f;  // 椭圆中心 Y
        canvas.fill_ellipse(
            mine::math::Vec2{ecx, ecy},
            mine::math::Vec2{sw * 0.5f, sh * 0.5f},
            mine::paint::Brush::solid(mine::math::Color{0.65f, 0.25f, 0.90f, 1.0f}));

        // Canvas::end() 返回不可变的绘制命令列表
        mine::paint::DisplayList dl = canvas.end();

        // ── 2. 提交渲染 ────────────────────────────────────────────────────

        paint_renderer->begin_frame();
        paint_renderer->render(dl, back_buf);
        paint_renderer->end_frame();

        // ── 3. 呈现后缓冲 ──────────────────────────────────────────────────
        swapchain->present();
    }
};

// ── main ──────────────────────────────────────────────────────────────────────

int main(int /*argc*/, char* /*argv*/[])
{
    // 1. 创建应用宿主（Win32 平台实现，消息循环）
    auto host = mine::platform::create_application_host();

    // 2. 创建 D3D11 图形设备
    auto device = mine::gfx::create_device(mine::gfx::Backend::Auto);
    if (!device) {
        std::fprintf(stderr, "错误：图形设备创建失败（当前系统不支持 D3D11）\n");
        return 1;
    }

    // 3. 创建 2D 渲染器（基于 RHI，不依赖具体图形 API）
    auto paint_renderer = mine::paint::create_renderer(device.get());
    if (!paint_renderer) {
        std::fprintf(stderr, "错误：2D 渲染器创建失败（着色器编译失败？）\n");
        return 1;
    }

    // 4. 创建窗口
    mine::platform::WindowDesc win_desc{};
    win_desc.title         = "MineFramework - Canvas 填充命令演示（FillRect / FillRoundedRect / FillComplexRoundedRect / FillEllipse）";
    win_desc.size          = {800.0f, 600.0f};
    win_desc.auto_position = true;
    win_desc.resizable     = true;
    win_desc.kind          = mine::platform::WindowKind::Normal;

    auto window = host->create_window(win_desc);
    if (!window) {
        std::fprintf(stderr, "错误：窗口创建失败\n");
        return 1;
    }

    // 5. 构建渲染器
    HelloRectRenderer renderer;
    renderer.device         = std::move(device);
    renderer.paint_renderer = std::move(paint_renderer);

    renderer.queue    = renderer.device->create_queue(mine::gfx::QueueType::Graphics);
    renderer.cmd_list = renderer.device->create_command_list(mine::gfx::QueueType::Graphics);
    if (!renderer.queue || !renderer.cmd_list) {
        std::fprintf(stderr, "错误：命令队列或命令列表创建失败\n");
        return 1;
    }

    // 6. 创建交换链
    mine::gfx::SwapchainDesc sc_desc{};
    sc_desc.native_window = window->native_handle().ptr;
    sc_desc.width         = static_cast<uint32_t>(win_desc.size.width);
    sc_desc.height        = static_cast<uint32_t>(win_desc.size.height);
    sc_desc.image_count   = 2;
    sc_desc.format        = mine::gfx::PixelFormat::BGRA8_UNorm;
    sc_desc.vsync         = mine::gfx::Vsync::On;

    renderer.swapchain = renderer.device->create_swapchain(sc_desc);
    if (!renderer.swapchain) {
        std::fprintf(stderr, "错误：交换链创建失败\n");
        return 1;
    }

    // 7. 订阅窗口事件（Resized）
    window->events().subscribe(&renderer);

    // 8. 显示窗口
    window->show();

    // 9. 渲染第一帧
    renderer.render();

    // 10. 进入消息循环，直到窗口关闭
    const int exit_code = host->run();

    // 11. 清理
    window->events().unsubscribe(&renderer);

    return exit_code;
}
