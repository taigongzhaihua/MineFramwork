/**
 * @file main.cpp
 * @brief 01-vsm-animation 示例：VisualStateManager + Storyboard 属性动画演示。
 *
 * 演示内容：
 *   - 注册 Color 类型 DependencyProperty（AnimDemo_ButtonBgColor）
 *   - 配置 VisualStateManager 三状态（Normal / Hovered / Pressed）
 *   - 为每个目标状态注册带动画的过渡（Storyboard::animate_dp_to）
 *   - 通过 PeekMessage 非阻塞循环实现每帧 tick + 渲染
 *   - 鼠标悬停 / 按压触发状态切换，动画平滑过渡背景颜色
 *
 * 交互方式：
 *   - 鼠标移入按钮  → Normal → Hovered（180ms，CubicEaseOut，深灰 → 蓝色）
 *   - 鼠标移出按钮  → Hovered → Normal（300ms，CubicEaseOut，蓝色 → 深灰）
 *   - 鼠标左键按下  → Pressed（100ms，Linear，蓝色 → 深蓝）
 *   - 鼠标左键松开  → 根据光标位置切换到 Hovered 或 Normal
 *
 * 渲染：底部三个小方块指示当前激活状态（活跃者有白色描边）。
 * 中途再次触发状态切换时，动画从当前插值颜色续接新动画（无颜色跳变）。
 */

#ifndef WIN32_LEAN_AND_MEAN
#  define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#  define NOMINMAX
#endif
#include <Windows.h>

#include <mine/platform/PlatformAbi.h>
#include <mine/platform/win32/Win32ApplicationHost.h>
#include <mine/gfx/Gfx.h>
#include <mine/paint/Paint.h>

#include <mine/ui/property/DependencyObject.h>
#include <mine/ui/property/DependencyProperty.h>
#include <mine/ui/property/ValuePriority.h>
#include <mine/ui/style/VisualStateManager.h>
#include <mine/ui/animation/AnimationAll.h>

#include <mine/math/Color.h>
#include <mine/core/Variant.h>

#include <algorithm>
#include <cstdio>

// ── 命名空间导入 ──────────────────────────────────────────────────────────────
using namespace mine;
using namespace mine::core;
using namespace mine::math;
using namespace mine::paint;
using namespace mine::ui;
using namespace mine::ui::style;
using namespace mine::ui::animation;

// ── 颜色常量（线性空间）──────────────────────────────────────────────────────
// 窗口背景：极深灰
static constexpr Color kBgColor      {0.09f, 0.09f, 0.11f, 1.0f};
// Normal 状态：深灰
static constexpr Color kColorNormal  {0.20f, 0.20f, 0.23f, 1.0f};
// Hovered 状态：蓝色
static constexpr Color kColorHovered {0.14f, 0.38f, 0.82f, 1.0f};
// Pressed 状态：深蓝
static constexpr Color kColorPressed {0.07f, 0.22f, 0.56f, 1.0f};

// ── 最小 DependencyObject 子类（承载 DP 属性） ──────────────────────────────
class ButtonState final : public DependencyObject {
protected:
    void on_property_changed(const DependencyProperty&,
                             const Variant&, const Variant&) override {}
    void invalidate_measure() override {}
    void invalidate_render()  override {}
};

// ── 全局属性注册（静态单例，线程安全由调用方在 main 单线程阶段保证）──────────
static const DependencyProperty& bg_color_prop() {
    static const DependencyProperty* p = nullptr;
    if (!p) {
        // 以匿名 TypeId 注册（示例中不需要类型检查）
        p = &register_property(
            "AnimDemo_ButtonBgColor",
            TypeId{}, TypeId{},
            Variant{kColorNormal}
        );
    }
    return *p;
}

// ── 主演示结构体 ─────────────────────────────────────────────────────────────
struct VsmAnimDemo : public mine::platform::IWindowEventSink {

    // ── RHI / 渲染 ───────────────────────────────────────────────────────────
    OwnedPtr<gfx::IDevice>     device;
    OwnedPtr<gfx::IQueue>      queue;
    OwnedPtr<gfx::ICommandList>cmd_list;   // 保留，resize 前 wait_idle 用
    OwnedPtr<gfx::ISwapchain>  swapchain;
    OwnedPtr<IRenderer>        paint_renderer;

    // DPI 缩放因子（物理像素 / 逻辑像素）
    float dpi_scale_{1.0f};

    // ── VSM / 状态 ───────────────────────────────────────────────────────────
    ButtonState       btn_state;
    VisualStateManager vsm{btn_state};
    bool mouse_is_pressed{false};

    // ── 按钮逻辑像素矩形（用于鼠标碰撞检测，事件坐标为逻辑像素）────────────
    math::Rect btn_logical{};

    // ── 控制 ─────────────────────────────────────────────────────────────────
    bool should_quit{false};

    // ── 高精度计时 ────────────────────────────────────────────────────────────
    LARGE_INTEGER perf_freq{};
    LARGE_INTEGER last_time{};

    // ── VSM 初始化 ────────────────────────────────────────────────────────────
    void setup_vsm() {
        // 定义三个视觉状态
        vsm.define_state("Normal");
        vsm.define_state("Hovered");
        vsm.define_state("Pressed");

        // * → Normal：300ms，CubicEaseOut（淡出蓝色，回归深灰）
        vsm.add_transition("*", "Normal",
            Function<void(Storyboard&)>{
                [this](Storyboard& sb) {
                    sb.animate_dp_to(
                        btn_state, bg_color_prop(),
                        Variant{kColorNormal},
                        Duration::milliseconds(300.0f),
                        CubicEaseOut);
                }
            });

        // * → Hovered：180ms，CubicEaseOut（快速入场蓝色）
        vsm.add_transition("*", "Hovered",
            Function<void(Storyboard&)>{
                [this](Storyboard& sb) {
                    sb.animate_dp_to(
                        btn_state, bg_color_prop(),
                        Variant{kColorHovered},
                        Duration::milliseconds(180.0f),
                        CubicEaseOut);
                }
            });

        // * → Pressed：100ms，Linear（迅速暗化到深蓝，强调按压感）
        vsm.add_transition("*", "Pressed",
            Function<void(Storyboard&)>{
                [this](Storyboard& sb) {
                    sb.animate_dp_to(
                        btn_state, bg_color_prop(),
                        Variant{kColorPressed},
                        Duration::milliseconds(100.0f),
                        Linear);
                }
            });

        // 设置初始属性值并立即跳变到 Normal（use_transitions=false，无动画）
        btn_state.set_value(bg_color_prop(), Variant{kColorNormal}, ValuePriority::Local);
        vsm.go_to_state("Normal", false);
    }

    // ── IWindowEventSink ─────────────────────────────────────────────────────
    void on_window_event(mine::platform::WindowEvent& event) override {
        switch (event.kind) {
        case mine::platform::WindowEventKind::Closing:
            should_quit = true;
            break;

        case mine::platform::WindowEventKind::DpiChanged: {
            // DPI 变化：重新计算物理交换链尺寸
            const float old_scale = dpi_scale_;
            dpi_scale_ = event.new_dpi / 96.0f;
            paint_renderer->set_dpi_scale(dpi_scale_);

            // 根据旧物理尺寸换算逻辑尺寸，再以新 DPI 算出新物理尺寸
            const float log_w = static_cast<float>(swapchain->width())  / old_scale;
            const float log_h = static_cast<float>(swapchain->height()) / old_scale;
            const auto phys_w = static_cast<uint32_t>(
                std::max(1.0f, log_w * dpi_scale_));
            const auto phys_h = static_cast<uint32_t>(
                std::max(1.0f, log_h * dpi_scale_));
            queue->wait_idle();
            swapchain->resize(phys_w, phys_h);
            render();
            break;
        }

        case mine::platform::WindowEventKind::Resized: {
            // Resized.new_size 为逻辑像素，乘以 dpi_scale_ 得物理像素
            const auto w = static_cast<uint32_t>(
                std::max(1.0f, event.new_size.width  * dpi_scale_));
            const auto h = static_cast<uint32_t>(
                std::max(1.0f, event.new_size.height * dpi_scale_));
            queue->wait_idle();
            swapchain->resize(w, h);
            render();
            break;
        }

        case mine::platform::WindowEventKind::MouseMove:
            handle_mouse_move(event.mouse_x, event.mouse_y);
            break;

        case mine::platform::WindowEventKind::MouseDown:
            // 仅响应左键（button index 0）且光标在按钮内
            if (event.mouse_button == 0 &&
                hit_test(event.mouse_x, event.mouse_y)) {
                mouse_is_pressed = true;
                vsm.go_to_state("Pressed");
            }
            break;

        case mine::platform::WindowEventKind::MouseUp:
            if (event.mouse_button == 0) {
                mouse_is_pressed = false;
                // 松开时依据光标位置决定回到 Hovered 还是 Normal
                if (hit_test(event.mouse_x, event.mouse_y)) {
                    vsm.go_to_state("Hovered");
                } else {
                    vsm.go_to_state("Normal");
                }
            }
            break;

        default:
            break;
        }
    }

    // ── 碰撞检测（逻辑像素坐标）──────────────────────────────────────────────
    [[nodiscard]] bool hit_test(float x, float y) const noexcept {
        return x >= btn_logical.x
            && x <= btn_logical.x + btn_logical.width
            && y >= btn_logical.y
            && y <= btn_logical.y + btn_logical.height;
    }

    // ── 鼠标移动处理（未按下时更新 Hovered/Normal）──────────────────────────
    void handle_mouse_move(float x, float y) {
        if (mouse_is_pressed) return;  // 按压期间不因移动改变状态
        const StringView cur = vsm.current_state();
        if (hit_test(x, y)) {
            if (cur != StringView{"Hovered"}) {
                vsm.go_to_state("Hovered");
            }
        } else {
            if (cur != StringView{"Normal"}) {
                vsm.go_to_state("Normal");
            }
        }
    }

    // ── 动画驱动（每帧调用）──────────────────────────────────────────────────
    void tick(float dt) {
        vsm.tick_animations(dt);
    }

    // ── 渲染 ─────────────────────────────────────────────────────────────────
    void render() {
        if (!swapchain || !paint_renderer) return;

        gfx::ITexture* back_buf = swapchain->current_render_target();
        if (!back_buf) return;

        // 物理像素尺寸（Canvas 坐标系使用物理像素）
        const float W = static_cast<float>(swapchain->width());
        const float H = static_cast<float>(swapchain->height());
        // 逻辑像素尺寸（用于布局计算，最终乘以 dpi_scale_ 得物理像素）
        const float logW = W / dpi_scale_;
        const float logH = H / dpi_scale_;
        const float s    = dpi_scale_;  // 缩写，方便后续乘法

        // 按钮逻辑矩形（240×80 逻辑像素，居中）
        constexpr float kBtnLogW = 240.0f;
        constexpr float kBtnLogH = 80.0f;
        btn_logical = {
            (logW - kBtnLogW) * 0.5f,
            (logH - kBtnLogH) * 0.5f,
            kBtnLogW,
            kBtnLogH
        };

        // 从 DP 读取动画当前插值颜色（Animation 优先级 > Local）
        const Variant& bg_var = btn_state.get_value(bg_color_prop());
        const Color    bg_color = bg_var.has<Color>() ? bg_var.get<Color>() : kColorNormal;

        // ── 构建 Canvas 绘制命令（物理像素坐标）──────────────────────────────
        Canvas canvas;

        // 1. 窗口背景
        canvas.fill_rect(
            math::Rect{0.0f, 0.0f, W, H},
            Brush::solid(kBgColor));

        // 2. 按钮投影（偏移 2×3 逻辑像素的半透明暗矩形）
        const math::Rect shadow_phys = {
            btn_logical.x * s + 2.0f * s,
            btn_logical.y * s + 3.0f * s,
            btn_logical.width  * s,
            btn_logical.height * s
        };
        canvas.fill_rounded_rect(
            {shadow_phys, 12.0f * s},
            Brush::solid(Color{0.0f, 0.0f, 0.0f, 0.30f}));

        // 3. 按钮主体（动画插值背景色）
        const math::Rect btn_phys = {
            btn_logical.x      * s,
            btn_logical.y      * s,
            btn_logical.width  * s,
            btn_logical.height * s
        };
        canvas.fill_rounded_rect(
            {btn_phys, 12.0f * s},
            Brush::solid(bg_color));

        // 4. 按钮描边（细白边，增加层次感）
        canvas.stroke_rounded_rect(
            {btn_phys, 12.0f * s},
            Brush::solid(Color{1.0f, 1.0f, 1.0f, 0.12f}),
            Pen{1.5f * s});

        // ── 状态指示器（按钮下方 32 逻辑像素，三个小方块）──────────────────
        // 方块大小 16×16 逻辑像素，相邻间距 24 逻辑像素
        constexpr float kIndSize = 16.0f;   // 逻辑像素
        constexpr float kIndGap  = 24.0f;   // 指示器间隔（逻辑像素）

        // 指示器行的垂直位置（逻辑像素，按钮底部下方 28 逻辑像素）
        const float ind_log_y  = btn_logical.y + btn_logical.height + 28.0f;
        // 三个指示器以窗口水平中心为基准排列
        const float ind_log_cx = logW * 0.5f;
        const float ind_log_x_normal  = ind_log_cx - kIndGap - kIndSize * 0.5f;
        const float ind_log_x_hovered = ind_log_cx - kIndSize * 0.5f;
        const float ind_log_x_pressed = ind_log_cx + kIndGap - kIndSize * 0.5f;

        const StringView cur_state = vsm.current_state();

        // 绘制单个状态指示器（活跃时全不透明 + 白色描边）
        auto draw_indicator = [&](float log_x, Color ic, StringView state_name) {
            const bool active = (cur_state == state_name);
            ic.a = active ? 1.0f : 0.22f;

            const math::Rect ind_phys = {
                log_x * s, ind_log_y * s,
                kIndSize * s, kIndSize * s
            };
            canvas.fill_rounded_rect({ind_phys, 4.0f * s}, Brush::solid(ic));

            if (active) {
                // 活跃指示器绘制白色描边以突出显示
                canvas.stroke_rounded_rect(
                    {ind_phys, 4.0f * s},
                    Brush::solid(Color{1.0f, 1.0f, 1.0f, 0.65f}),
                    Pen{1.5f * s});
            }
        };

        draw_indicator(ind_log_x_normal,  kColorNormal,  StringView{"Normal"});
        draw_indicator(ind_log_x_hovered, kColorHovered, StringView{"Hovered"});
        draw_indicator(ind_log_x_pressed, kColorPressed, StringView{"Pressed"});

        // ── 提交渲染 ──────────────────────────────────────────────────────────
        mine::paint::DisplayList dl = canvas.end();
        paint_renderer->begin_frame();
        paint_renderer->render(dl, back_buf);
        paint_renderer->end_frame();
        swapchain->present();
    }
};

// ── main ──────────────────────────────────────────────────────────────────────
int main(int /*argc*/, char* /*argv*/[])
{
    // 1. 触发静态属性注册（确保在单线程 main 阶段完成）
    (void)bg_color_prop();

    // 2. 创建平台应用宿主（Win32 消息循环）
    auto host = mine::platform::create_application_host();

    // 3. 创建 D3D11 图形设备
    auto device = mine::gfx::create_device(mine::gfx::Backend::Auto);
    if (!device) {
        std::fprintf(stderr, "错误：图形设备创建失败\n");
        return 1;
    }

    // 4. 创建 2D 渲染器
    auto paint_renderer = mine::paint::create_renderer(device.get());
    if (!paint_renderer) {
        std::fprintf(stderr, "错误：Paint 渲染器创建失败\n");
        return 1;
    }

    // 5. 创建窗口（逻辑像素尺寸）
    mine::platform::WindowDesc win_desc{};
    win_desc.title         = "MineFramework - VSM 属性动画演示 (01-vsm-animation)";
    win_desc.size          = {600.0f, 400.0f};
    win_desc.auto_position = true;
    win_desc.resizable     = true;
    win_desc.kind          = mine::platform::WindowKind::Normal;

    auto window = host->create_window(win_desc);
    if (!window) {
        std::fprintf(stderr, "错误：窗口创建失败\n");
        return 1;
    }

    // 6. 初始化演示结构体
    VsmAnimDemo demo;
    demo.device         = std::move(device);
    demo.paint_renderer = std::move(paint_renderer);

    // 创建命令队列（resize 时 wait_idle 使用）
    demo.queue    = demo.device->create_queue(mine::gfx::QueueType::Graphics);
    demo.cmd_list = demo.device->create_command_list(mine::gfx::QueueType::Graphics);
    if (!demo.queue || !demo.cmd_list) {
        std::fprintf(stderr, "错误：命令队列或命令列表创建失败\n");
        return 1;
    }

    // 创建交换链（初始以逻辑尺寸建立，后续根据 DPI 校正）
    mine::gfx::SwapchainDesc sc_desc{};
    sc_desc.native_window = window->native_handle().ptr;
    sc_desc.width         = static_cast<uint32_t>(win_desc.size.width);
    sc_desc.height        = static_cast<uint32_t>(win_desc.size.height);
    sc_desc.image_count   = 2;
    sc_desc.format        = mine::gfx::PixelFormat::BGRA8_UNorm_sRGB;
    sc_desc.vsync         = mine::gfx::Vsync::On;

    demo.swapchain = demo.device->create_swapchain(sc_desc);
    if (!demo.swapchain) {
        std::fprintf(stderr, "错误：交换链创建失败\n");
        return 1;
    }

    // 7. 根据窗口实际 DPI 校正交换链尺寸
    {
        const float dpi_scale = window->dpi() / 96.0f;
        demo.dpi_scale_ = dpi_scale;
        demo.paint_renderer->set_dpi_scale(dpi_scale);

        if (dpi_scale > 1.001f) {
            // 以物理像素重建交换链
            const auto phys_w = static_cast<uint32_t>(win_desc.size.width  * dpi_scale);
            const auto phys_h = static_cast<uint32_t>(win_desc.size.height * dpi_scale);
            demo.swapchain->resize(phys_w, phys_h);
        }
    }

    // 8. 初始化 VSM（注册状态 + 过渡 + 设置初始值）
    demo.setup_vsm();

    // 9. 订阅窗口事件
    window->events().subscribe(&demo);

    // 10. 显示窗口并渲染首帧
    window->show();
    demo.render();

    // 11. 初始化高精度计时器
    ::QueryPerformanceFrequency(&demo.perf_freq);
    ::QueryPerformanceCounter(&demo.last_time);

    // 12. 自定义帧循环（PeekMessage 非阻塞模式，支持连续动画帧）
    //
    //  与 host->run() 的阻塞式 GetMessage 循环不同，此循环每帧：
    //    a. 消费消息队列中所有待处理消息（不阻塞）
    //    b. 计算帧间 dt（QueryPerformanceCounter 高精度）
    //    c. 驱动 VSM 动画 tick(dt)
    //    d. 渲染并呈现帧
    //
    while (!demo.should_quit) {
        // 处理所有排队的 Windows 消息
        MSG msg;
        while (::PeekMessageW(&msg, nullptr, 0, 0, PM_REMOVE)) {
            if (msg.message == WM_QUIT) {
                demo.should_quit = true;
                break;
            }
            ::TranslateMessage(&msg);
            ::DispatchMessageW(&msg);
        }

        if (!demo.should_quit) {
            // 计算帧间隔时间（秒）
            LARGE_INTEGER now;
            ::QueryPerformanceCounter(&now);
            float dt = static_cast<float>(
                now.QuadPart - demo.last_time.QuadPart)
                / static_cast<float>(demo.perf_freq.QuadPart);
            demo.last_time = now;

            // 限制最大 dt（防止后台切换或调试暂停后一次性推进过大）
            dt = std::min(dt, 0.1f);

            // 驱动动画 + 渲染
            demo.tick(dt);
            demo.render();
        }
    }

    // 13. 清理资源
    window->events().unsubscribe(&demo);
    if (demo.queue) demo.queue->wait_idle();

    return 0;
}
