/**
 * @file main.cpp
 * @brief 00-hello-rect 示例：mine.paint 绘制矩形的最小闭环演示。
 *
 * 演示内容：
 *   1. mine::gfx::create_device()：创建 D3D11 图形设备
 *   2. mine::paint::create_renderer()：创建基于 RHI 的 2D 渲染器
 *   3. Canvas → DisplayList → IRenderer：录制绘制命令并渲染到交换链后缓冲
 *   4. Brush::solid()：纯色填充矩形
 *
 * 验收标准（M0.3 任务 #14）：窗口中央显示一个红色矩形，背景为深灰色。
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
     * @brief 渲染一帧：清屏 + 画矩形 + 呈现。
     */
    void render() {
        if (!device || !swapchain || !paint_renderer) return;

        mine::gfx::ITexture* back_buf = swapchain->current_render_target();
        if (!back_buf) return;

        const float w = static_cast<float>(swapchain->width());
        const float h = static_cast<float>(swapchain->height());

        // ── 1. 构建绘制命令列表 ────────────────────────────────────────────

        mine::paint::Canvas canvas;

        // 背景矩形：覆盖整个视口（深灰色，不透明）
        canvas.fill_rect(
            mine::math::Rect{0.0f, 0.0f, w, h},
            mine::paint::Brush::solid(mine::math::Color{0.15f, 0.15f, 0.15f, 1.0f}));

        // 前景矩形：居中显示红色矩形（宽 300，高 200）
        const float rect_w = 300.0f;
        const float rect_h = 200.0f;
        const float rect_x = (w - rect_w) * 0.5f;
        const float rect_y = (h - rect_h) * 0.5f;

        canvas.fill_rect(
            mine::math::Rect{rect_x, rect_y, rect_w, rect_h},
            mine::paint::Brush::solid(mine::math::Color{0.85f, 0.15f, 0.10f, 1.0f}));

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
    win_desc.title         = "MineFramework - Hello Rect（M0.3 Paint 验证）";
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
