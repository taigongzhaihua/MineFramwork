/**
 * @file main.cpp
 * @brief 00-blank-window 示例：平台抽象 + D3D11 RHI 清屏演示。
 *
 * 演示内容：
 *   1. IApplicationHost + IWindow：平台无关窗口创建与消息循环
 *   2. mine::gfx::create_device()：链接时注入 D3D11 后端
 *   3. ISwapchain / ICommandList / IQueue：渲染管线基础对象
 *   4. clear_render_target()：将后缓冲清为纯蓝色
 *   5. Resized 事件处理：交换链跟随窗口尺寸变化
 *
 * 验收标准（M0.2 任务 #12）：窗口背景显示蓝色，resize 后依然保持蓝色。
 */

#include <mine/platform/PlatformAbi.h>
#include <mine/gfx/Gfx.h>

// MessageBoxA — GUI 子系统无控制台，用弹窗报错
#ifndef WIN32_LEAN_AND_MEAN
#  define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#  define NOMINMAX
#endif
#include <windows.h>

#include <cstdio>
#include <cmath>     // std::max

// ── 渲染器：封装 RHI 对象，提供单帧清屏 + 呈现 ───────────────────────────

/**
 * @brief 简易渲染器，持有 RHI 设备/交换链/命令列表等对象。
 *
 * 生命周期须短于窗口（销毁顺序：renderer → window）。
 */
struct BlueRenderer : public mine::platform::IWindowEventSink {
    mine::core::OwnedPtr<mine::gfx::IDevice>      device;
    mine::core::OwnedPtr<mine::gfx::IQueue>        queue;
    mine::core::OwnedPtr<mine::gfx::ISwapchain>    swapchain;
    mine::core::OwnedPtr<mine::gfx::ICommandList>  cmd_list;

    // IWindowEventSink 接口实现
    void on_window_event(mine::platform::WindowEvent& event) override {
        if (event.kind == mine::platform::WindowEventKind::Resized) {
            // 获取新的客户区物理尺寸（Resized 事件中 new_size 是逻辑像素，
            // 此处暂时直接使用，因示例窗口 DPI=100% 时二者相等）
            const auto w = static_cast<uint32_t>(std::max(1.0f, event.new_size.width));
            const auto h = static_cast<uint32_t>(std::max(1.0f, event.new_size.height));

            // 等待 GPU 完成当前帧，然后 resize 交换链
            queue->wait_idle();
            swapchain->resize(w, h);

            // resize 后立即重新渲染一帧，保持蓝色背景
            render();
        }
    }

    /// 渲染一帧：清屏为蓝色并呈现
    void render() {
        if (!device || !swapchain || !queue || !cmd_list) {
            return;
        }

        mine::gfx::ITexture* back_buf = swapchain->current_render_target();
        if (!back_buf) {
            return;
        }

        // 开始录制
        cmd_list->begin();

        // 设置渲染目标为后缓冲
        cmd_list->set_render_target(back_buf);

        // 设置视口（覆盖整个后缓冲区域）
        mine::gfx::Viewport vp{};
        vp.x         = 0.0f;
        vp.y         = 0.0f;
        vp.width     = static_cast<float>(swapchain->width());
        vp.height    = static_cast<float>(swapchain->height());
        vp.min_depth = 0.0f;
        vp.max_depth = 1.0f;
        cmd_list->set_viewport(vp);

        // 清屏为纯蓝色（线性 sRGB 空间，R=0.0, G=0.4, B=1.0, A=1.0）
        constexpr mine::gfx::Color4f blue{0.0f, 0.4f, 1.0f, 1.0f};
        cmd_list->clear_render_target(back_buf, blue);

        // 结束录制
        cmd_list->end();

        // 提交到 GPU 执行
        queue->submit(cmd_list.get());

        // 呈现后缓冲（Vsync=On，等待垂直同步）
        swapchain->present();
    }
};

// ── main ──────────────────────────────────────────────────────────────────────

int main(int /*argc*/, char* /*argv*/[])
{
    // 1. 创建应用宿主（实际实现由链接的平台后端库决定）
    auto host = mine::platform::create_application_host();

    // 2. 创建 D3D11 图形设备（实现由链接的 mine.gfx.d3d11 库注入）
    auto device = mine::gfx::create_device(mine::gfx::Backend::Auto);
    if (!device) {
        MessageBoxA(NULL, "图形设备创建失败（当前系统不支持 D3D11）", "MineFramework 错误", MB_ICONERROR);
        return 1;
    }

    // 3. 描述窗口
    mine::platform::WindowDesc win_desc{};
    win_desc.title         = "MineFramework - 蓝色清屏（M0.2 RHI 验证）";
    win_desc.size          = {800.0f, 600.0f};
    win_desc.auto_position = true;
    win_desc.resizable     = true;
    win_desc.kind          = mine::platform::WindowKind::Normal;

    // 4. 创建窗口
    auto window = host->create_window(win_desc);
    if (!window) {
        MessageBoxA(NULL, "窗口创建失败", "MineFramework 错误", MB_ICONERROR);
        return 1;
    }

    // 5. 构建渲染器，使用逻辑尺寸初始化（简化起见不计 DPI 缩放）
    BlueRenderer renderer;
    renderer.device = std::move(device);

    // 创建 Graphics 命令队列
    renderer.queue    = renderer.device->create_queue(mine::gfx::QueueType::Graphics);
    renderer.cmd_list = renderer.device->create_command_list(mine::gfx::QueueType::Graphics);
    if (!renderer.queue || !renderer.cmd_list) {
        MessageBoxA(NULL, "命令队列或命令列表创建失败", "MineFramework 错误", MB_ICONERROR);
        return 1;
    }

    // 创建交换链（绑定到窗口原生句柄）
    mine::gfx::SwapchainDesc sc_desc{};
    sc_desc.native_window = window->native_handle().ptr;
    sc_desc.width         = static_cast<uint32_t>(win_desc.size.width);
    sc_desc.height        = static_cast<uint32_t>(win_desc.size.height);
    sc_desc.image_count   = 2;
    sc_desc.format        = mine::gfx::PixelFormat::BGRA8_UNorm_sRGB;
    sc_desc.vsync         = mine::gfx::Vsync::On;

    renderer.swapchain = renderer.device->create_swapchain(sc_desc);
    if (!renderer.swapchain) {
        MessageBoxA(NULL, "交换链创建失败（DXGI/D3D11 错误）", "MineFramework 错误", MB_ICONERROR);
        return 1;
    }

    // 6. 订阅窗口事件（接收 Resized 通知以同步交换链尺寸）
    window->events().subscribe(&renderer);

    // 7. 显示窗口
    window->show();

    // 8. 渲染第一帧（清屏蓝色）
    renderer.render();

    // 9. 进入消息循环，直到窗口关闭
    const int exit_code = host->run();

    // 10. 清理（先取消订阅，再销毁渲染器）
    window->events().unsubscribe(&renderer);

    return exit_code;
}
