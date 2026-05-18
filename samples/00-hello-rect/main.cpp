/**
 * @file main.cpp
 * @brief 00-hello-rect 示例：mine.paint Canvas SDF 填充 + 描边命令综合演示。
 *
 * 演示内容（2×5 网格布局，左列填充/右列描边）：
 *   行1 左：FillRect                      — 纯色填充矩形（绿色）
 *   行1 右：StrokeRect                    — 矩形描边（绿色，线宽 4px）
 *   行2 左：FillRoundedRect               — 均匀圆角矩形填充（蓝色，radius=20px）
 *   行2 右：StrokeRoundedRect             — 均匀圆角矩形描边（蓝色，线宽 4px）
 *   行3 左：FillComplexRoundedRect        — 四角独立圆角矩形填充（橙色）
 *   行3 右：StrokeComplexRoundedRect      — 四角独立圆角矩形描边（橙色，线宽 4px）
 *   行4 左：FillEllipse                   — 椭圆填充（紫色）
 *   行4 右：StrokeEllipse                 — 椭圆描边（紫色，线宽 4px）
 *   行5 左：StrokeBorderedRoundedRect     — 四边不等宽 + 四角独立圆角内侧描边（红色）
 *   行5 右：StrokeBorderedRoundedRect     — 均匀圆角 + 仅上下边框（青色，上下各12px）
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
     * @brief 渲染一帧：清屏 + 2×5 网格展示填充、描边和四边不等宽描边命令 + 呈现。
     */
    void render() {
        if (!device || !swapchain || !paint_renderer) return;

        mine::gfx::ITexture* back_buf = swapchain->current_render_target();
        if (!back_buf) return;

        const float W = static_cast<float>(swapchain->width());
        const float H = static_cast<float>(swapchain->height());

        // ── 1. 构建绘制命令列表 ────────────────────────────────────────────

        mine::paint::Canvas canvas;

        // 背景：深灰色覆盖整个视口
        canvas.fill_rect(
            mine::math::Rect{0.0f, 0.0f, W, H},
            mine::paint::Brush::solid(mine::math::Color{0.12f, 0.12f, 0.12f, 1.0f}));

        // 2×5 网格参数（2列 × 5行）
        const float outer_pad = 20.0f;
        const float gap       = 10.0f;
        const float cw        = (W - outer_pad * 2.0f - gap) * 0.5f;
        const float ch        = (H - outer_pad * 2.0f - gap * 4.0f) / 5.0f;

        const float col0 = outer_pad;
        const float col1 = outer_pad + cw + gap;

        const float row0 = outer_pad;
        const float row1 = outer_pad + ch + gap;
        const float row2 = outer_pad + (ch + gap) * 2.0f;
        const float row3 = outer_pad + (ch + gap) * 3.0f;
        const float row4 = outer_pad + (ch + gap) * 4.0f;

        const float ip = 14.0f;
        const float sw = cw - ip * 2.0f;
        const float sh = ch - ip * 2.0f;

        // 颜色定义
        const mine::math::Color col_green  {0.20f, 0.75f, 0.35f, 1.0f};
        const mine::math::Color col_blue   {0.20f, 0.45f, 0.90f, 1.0f};
        const mine::math::Color col_orange {0.95f, 0.55f, 0.10f, 1.0f};
        const mine::math::Color col_purple {0.65f, 0.25f, 0.90f, 1.0f};
        const mine::math::Color col_red    {0.90f, 0.20f, 0.20f, 1.0f};
        const mine::math::Color col_cyan   {0.15f, 0.80f, 0.85f, 1.0f};

        const mine::paint::Pen pen{.width = 4.0f, .miter_limit = 4.0f};

        // ── 行1 左：FillRect ────────────────────────────────────────────
        canvas.fill_rect(
            mine::math::Rect{col0 + ip, row0 + ip, sw, sh},
            mine::paint::Brush::solid(col_green));

        // ── 行1 右：StrokeRect ──────────────────────────────────────────
        canvas.stroke_rect(
            mine::math::Rect{col1 + ip, row0 + ip, sw, sh},
            mine::paint::Brush::solid(col_green), pen);

        // ── 行2 左：FillRoundedRect ─────────────────────────────────────
        canvas.fill_rounded_rect(
            mine::math::RoundedRect{mine::math::Rect{col0 + ip, row1 + ip, sw, sh}, 20.0f},
            mine::paint::Brush::solid(col_blue));

        // ── 行2 右：StrokeRoundedRect ───────────────────────────────────
        canvas.stroke_rounded_rect(
            mine::math::RoundedRect{mine::math::Rect{col1 + ip, row1 + ip, sw, sh}, 20.0f},
            mine::paint::Brush::solid(col_blue), pen);

        // ── 行3 左：FillComplexRoundedRect ─────────────────────────────
        const mine::math::CornerRadii radii{
            {36.0f, 36.0f},  // 左上角
            {8.0f,  8.0f},   // 右上角
            {36.0f, 36.0f},  // 右下角
            {8.0f,  8.0f}    // 左下角
        };
        canvas.fill_complex_rounded_rect(
            mine::math::ComplexRoundedRect{mine::math::Rect{col0 + ip, row2 + ip, sw, sh}, radii},
            mine::paint::Brush::solid(col_orange));

        // ── 行3 右：StrokeComplexRoundedRect ───────────────────────────
        canvas.stroke_complex_rounded_rect(
            mine::math::ComplexRoundedRect{mine::math::Rect{col1 + ip, row2 + ip, sw, sh}, radii},
            mine::paint::Brush::solid(col_orange), pen);

        // ── 行4 左：FillEllipse ─────────────────────────────────────────
        canvas.fill_ellipse(
            mine::math::Vec2{col0 + ip + sw * 0.5f, row3 + ip + sh * 0.5f},
            mine::math::Vec2{sw * 0.5f, sh * 0.5f},
            mine::paint::Brush::solid(col_purple));

        // ── 行4 右：StrokeEllipse ───────────────────────────────────────
        canvas.stroke_ellipse(
            mine::math::Vec2{col1 + ip + sw * 0.5f, row3 + ip + sh * 0.5f},
            mine::math::Vec2{sw * 0.5f, sh * 0.5f},
            mine::paint::Brush::solid(col_purple), pen);

        // ── 行5 左：StrokeBorderedRoundedRect（四边不等宽 + 四角独立圆角）──
        canvas.stroke_bordered_rounded_rect(
            mine::math::Rect{col0 + ip, row4 + ip, sw, sh},
            mine::paint::Brush::solid(col_red),
            mine::math::Thickness{8.0f, 4.0f, 2.0f, 16.0f},  // left=8, top=4, right=2, bottom=16
            mine::math::CornerRadii{
                {30.0f, 30.0f},  // 左上角
                {10.0f, 10.0f},  // 右上角
                {20.0f, 20.0f},  // 右下角
                {0.0f,  0.0f}    // 左下角（直角）
            });

        // ── 行5 右：StrokeBorderedRoundedRect（均匀圆角 + 仅上下边框各12px）──
        canvas.stroke_bordered_rounded_rect(
            mine::math::Rect{col1 + ip, row4 + ip, sw, sh},
            mine::paint::Brush::solid(col_cyan),
            mine::math::Thickness::symmetric(0.0f, 12.0f),  // 仅上下各 12px（symmetric: horizontal=0, vertical=12）
            mine::math::CornerRadii::uniform(16.0f));

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
    win_desc.title         = "MineFramework - Canvas SDF 渲染演示（Fill / Stroke × Rect / RoundedRect / ComplexRoundedRect / Ellipse）";
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
