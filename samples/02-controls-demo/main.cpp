/**
 * @file main.cpp
 * @brief 02-controls-demo 示例：窗口 + 控件交互演示。
 *
 * 演示内容：
 *   1. mine::ui::app::Application 应用基类（封装平台初始化、设备、渲染器）
 *   2. mine::ui::Window 窗口（通过 Application::create_window() 创建）
 *   3. 自定义 DemoRoot（FrameworkElement 子类），手动管理控件绝对位置布局
 *   4. Button 控件（点击计数 / 重置 / 退出，Normal/Pressed 视觉反馈）
 *   5. TextBlock 控件（标题 / 副标题 / 状态计数，真实字体渲染）
 *   6. InputRouter 将平台鼠标事件路由到视觉树
 *   7. Button::ClickEvent 路由事件驱动应用状态更新
 *
 * 架构说明：
 *   DemoApp 继承 mine::ui::app::Application，平台宿主、图形设备、渲染器均由
 *   基类自动创建和管理。on_startup() 中创建窗口、加载字体、构建控件树。
 *   DemoApp 同时实现 IWindowEventSink，接收鼠标事件后转发给 InputRouter。
 */

// Win32 API：用于设置控制台 UTF-8 代码页（xmake 已通过 add_defines 定义宏，此处无需重复）
#include <windows.h>

#include <mine/ui/app/AppAll.h>         // Application + MINE_APPLICATION_MAIN

#include <mine/core/Memory.h>               // core::OwnedPtr
#include <mine/text/FontFace.h>

#include <mine/ui/window/Window.h>
#include <mine/ui/layout/FrameworkElement.h>
#include <mine/ui/controls/Button.h>
#include <mine/ui/controls/TextBlock.h>
#include <mine/ui/input/InputRouter.h>
#include <mine/ui/event/RoutedEventArgs.h>
#include <mine/platform/IWindow.h>
#include <mine/platform/WindowDesc.h>

#include <mine/math/Color.h>
#include <mine/math/Rect.h>
#include <mine/math/Thickness.h>

#include <cstdio>
#include <cmath>
#include <cstring>

// ── 命名空间别名 ──────────────────────────────────────────────────────────────

namespace platform = mine::platform;
namespace text     = mine::text;
namespace math     = mine::math;
namespace ui       = mine::ui;
namespace input    = mine::ui::input;
namespace core     = mine::core;

// ── Ripple 动画驱动：Win32 Timer 回调 ────────────────────────────────────────

// 前向声明（TimerProc 需要访问 DemoApp，DemoApp 定义在后面）
struct DemoApp;

/// 全局 DemoApp 实例指针，供 TimerProc 访问（由 on_startup 赋值，单实例保证）
static DemoApp* g_demo_app = nullptr;

/// Win32 Ripple 定时器 ID（任意非零值，与其他 SetTimer 区分）
static constexpr UINT_PTR kRippleTimerId = 42u;

/// Win32 TimerProc 回调：每 ~16ms 驱动一帧 ripple 渲染，无活跃 ripple 时自动停止
static VOID CALLBACK ripple_timer_proc(
    HWND /*hwnd*/, UINT /*msg*/, UINT_PTR /*id*/, DWORD /*time*/) noexcept;

// ── 演示根面板（自定义 FrameworkElement，绝对坐标布局）──────────────────────

/**
 * @brief 演示根面板。
 *
 * 继承 FrameworkElement 使其可作为 Window::set_content(UIElement*) 的内容根，
 * 并参与 Window 的两遍布局（Measure/Arrange）。
 *
 * 由于 Control（Button/TextBlock 等）继承自 UIElement 而非 FrameworkElement，
 * 无法通过 Panel::add_child(FrameworkElement*) 添加控件。
 * 本类改用 Visual::add_child(Visual*) 将控件纳入视觉树，
 * 在 arrange_override() 中通过 UIElement::arrange(Rect) 为每个控件分配位置。
 */
struct DemoRoot : public ui::FrameworkElement {

    // 控件成员（公开以便 DemoApp 中的回调直接访问）
    ui::TextBlock  header_text;   ///< 标题（蓝色背景全宽标题栏）
    ui::TextBlock  subtitle;      ///< 副标题说明文字
    ui::Button     btn_count;     ///< "计数 +1" 按钮
    ui::Button     btn_reset;     ///< "重  置" 按钮
    ui::Button     btn_quit;      ///< "退  出" 按钮
    ui::TextBlock  status_label;  ///< 显示当前点击计数的状态标签

    DemoRoot()
    {
        // 将控件纳入 Visual 视觉树（render_to_canvas 递归渲染 + hit_test 命中测试均依赖此步）
        Visual::add_child(&header_text);
        Visual::add_child(&subtitle);
        Visual::add_child(&btn_count);
        Visual::add_child(&btn_reset);
        Visual::add_child(&btn_quit);
        Visual::add_child(&status_label);
    }

protected:
    /**
     * @brief 测量阶段：测量各控件的期望尺寸，根面板自身充满全部可用空间。
     */
    math::Size measure_override(math::Size available) override
    {
        header_text.measure(available);
        subtitle.measure(available);
        btn_count.measure(available);
        btn_reset.measure(available);
        btn_quit.measure(available);
        status_label.measure(available);
        return available;
    }

    /**
     * @brief 排列阶段：为各控件分配绝对坐标矩形。
     *
     * 调用 UIElement::arrange(Rect) 设置每个控件的 bounds_rect，
     * 控件在 on_render() 中以 bounds_rect() 为绘制基准。
     */
    math::Size arrange_override(math::Size final_size) override
    {
        const float W      = final_size.width;
        const float margin = 16.0f;
        float y = 0.0f;

        // 标题栏：全宽蓝色背景，固定高 60px
        const float header_h = 60.0f;
        header_text.arrange(math::Rect{ 0.0f, y, W, header_h });
        y += header_h;

        // 副标题：两侧留边距，高 38px，上下各 8px 间距
        y += 8.0f;
        const float subtitle_h = 38.0f;
        subtitle.arrange(math::Rect{ margin, y, W - 2.0f * margin, subtitle_h });
        y += subtitle_h + 16.0f;

        // 按钮行：宽度由内容期望尺寸决定，高度对齐到三者最大值，间距 10px
        const float btn_gap     = 10.0f;
        const math::Size bsz_count = btn_count.desired_size();
        const math::Size bsz_reset = btn_reset.desired_size();
        const math::Size bsz_quit  = btn_quit.desired_size();
        const float btn_h = std::max({ bsz_count.height, bsz_reset.height, bsz_quit.height });
        float bx = margin;
        btn_count.arrange(math::Rect{ bx, y, bsz_count.width, btn_h });
        bx += bsz_count.width + btn_gap;
        btn_reset.arrange(math::Rect{ bx, y, bsz_reset.width, btn_h });
        bx += bsz_reset.width + btn_gap;
        btn_quit.arrange(math::Rect{ bx, y, bsz_quit.width, btn_h });
        y += btn_h + 24.0f;

        // 状态标签：两侧留边距，高 44px（大字体）
        const float status_h = 44.0f;
        status_label.arrange(math::Rect{ margin, y, W - 2.0f * margin, status_h });

        return final_size;
    }
};

// ── 演示应用主类 ──────────────────────────────────────────────────────────────

/**
 * @brief 控件演示应用。
 *
 * 继承 mine::ui::app::Application：平台宿主、图形设备、渲染器均由基类自动管理。
 * 同时实现 platform::IWindowEventSink 接收鼠标事件，转发给 InputRouter。
 */
struct DemoApp : public mine::ui::app::Application,
                 public platform::IWindowEventSink {

    // ── 输入路由器 ────────────────────────────────────────────────────────
    input::InputRouter  router;

    // ── UI 控件树根节点 ───────────────────────────────────────────────────
    DemoRoot            root;

    // ── 字体资源（生命周期与 DemoApp 一致，避免悬空指针）─────────────────────
    core::OwnedPtr<text::FontFace> font_face_;  ///< 已加载的字体，所有控件共享

    // ── 运行时状态 ────────────────────────────────────────────────────────
    int          click_count      = 0;       ///< 点击计数器
    ui::Window*  ui_win_          = nullptr; ///< 主窗口（由 on_startup 赋值）
    UINT_PTR     ripple_timer_id_ = 0;       ///< Ripple 动画驱动定时器 ID（0 = 未启动）

    // ── Application 生命周期扩展点 ───────────────────────────────────────

    /**
     * @brief 应用启动：设置控制台代码页，加载字体，创建窗口，构建 UI。
     */
    void on_startup(int /*argc*/, char** /*argv*/) override
    {
        // 设置全局实例指针（供 Ripple TimerProc 访问）
        g_demo_app = this;

        // 设置控制台输出为 UTF-8，避免中文日志乱码
        SetConsoleOutputCP(CP_UTF8);

        // 加载字体（优先微软雅黑以支持中文）。
        // 注意：font_face_ 是 DemoApp 成员，生命周期覆盖全部渲染周期，
        // 避免 on_startup 返回后局部变量被销毁导致后续渲染访问悬空指针。
        {
            const char* candidates[] = {
                "C:\\Windows\\Fonts\\msyh.ttc",    // 微软雅黑（Windows 10/11）
                "C:\\Windows\\Fonts\\msyh.ttf",    // 微软雅黑（Windows 7/8）
                "C:\\Windows\\Fonts\\simhei.ttf",  // 黑体（备用中文字体）
                "C:\\Windows\\Fonts\\segoeui.ttf", // Segoe UI（西文回退）
                "C:\\Windows\\Fonts\\arial.ttf",   // Arial（西文回退）
            };
            for (const char* path : candidates) {
                font_face_ = text::FontFace::load_from_file(path);
                if (font_face_) {
                    std::fprintf(stdout, "信息：已加载字体 %s\n", path);
                    break;
                }
            }
            if (!font_face_) {
                std::fprintf(stdout, "警告：未找到系统字体，将以占位线展示文字\n");
            }
            std::fflush(stdout);
        }

        // 创建主窗口（Application 内部自动创建交换链并订阅 Resized/DpiChanged）
        platform::WindowDesc desc{};
        desc.title         = "MineFramework - 控件交互演示";
        desc.size          = { 800.0f, 500.0f };
        desc.auto_position = true;
        desc.resizable     = true;
        desc.kind          = platform::WindowKind::Normal;

        ui_win_ = create_window(desc);
        set_main_window(ui_win_);

        // 构建 UI 控件树并连接事件
        build_ui(font_face_.get());

        // 订阅原生窗口输入事件（鼠标/键盘 → InputRouter）
        ui_win_->native_window().events().subscribe(this);

        // 将控件树挂载到窗口，触发首次布局 + 渲染
        ui_win_->set_content(&root);
        ui_win_->show();
    }

    void on_exit(int /*exit_code*/) override
    {
        // 停止 Ripple 定时器（防止应用退出后回调访问已释放内存）
        if (ripple_timer_id_ != 0) {
            KillTimer(nullptr, ripple_timer_id_);
            ripple_timer_id_ = 0;
        }
        g_demo_app = nullptr;

        // 取消输入事件订阅，防止窗口析构后回调访问已释放内存
        if (ui_win_) {
            ui_win_->native_window().events().unsubscribe(this);
        }
    }

    // ── IWindowEventSink 实现 ─────────────────────────────────────────────

    void on_window_event(platform::WindowEvent& event) override
    {
        using Kind = platform::WindowEventKind;
        switch (event.kind) {
        case Kind::MouseMove:
        case Kind::MouseDown:
        case Kind::MouseUp:
        case Kind::MouseWheel:
        case Kind::KeyDown:
        case Kind::KeyUp:
            router.on_window_event(event);
            if (ui_win_) {
                ui_win_->render();
                // 事件可能触发 visual_state 变化（悬停/按下等），进而启动动画
                // 若有活跃动画（ripple 或背景色过渡）且 timer 未启动，则启动渲染驱动器
                if (ripple_timer_id_ == 0 && (
                    root.btn_count.has_active_animation() ||
                    root.btn_reset.has_active_animation() ||
                    root.btn_quit.has_active_animation()))
                {
                    ripple_timer_id_ = SetTimer(nullptr, 0, 16, ripple_timer_proc);
                }
            }
            break;
        default:
            break;
        }
    }

    // ── 按钮点击回调 ──────────────────────────────────────────────────────

    static void on_click_count(void* /*sender*/,
                               ui::RoutedEventArgs& /*args*/,
                               void* user_data)
    {
        auto* app = static_cast<DemoApp*>(user_data);
        ++app->click_count;
        app->update_status();
    }

    static void on_click_reset(void* /*sender*/,
                               ui::RoutedEventArgs& /*args*/,
                               void* user_data)
    {
        auto* app = static_cast<DemoApp*>(user_data);
        app->click_count = 0;
        app->update_status();
    }

    static void on_click_quit(void* /*sender*/,
                              ui::RoutedEventArgs& /*args*/,
                              void* user_data)
    {
        // Application::quit() 请求退出主消息循环
        static_cast<DemoApp*>(user_data)->quit(0);
    }

    // ── 状态刷新 ──────────────────────────────────────────────────────────

    void update_status()
    {
        char buf[128];
        std::snprintf(buf, sizeof(buf), "当前计数：%d 次", click_count);
        root.status_label.set_text(buf);
        if (ui_win_) { ui_win_->render(); }
    }

    // ── UI 树构建 ─────────────────────────────────────────────────────────

    void build_ui(text::FontFace* font)
    {
        // ── 1. 标题文字（蓝色背景标题栏）────────────────────────────────────
        root.header_text.set_text("MineFramework 控件演示");
        root.header_text.set_font_size(22.0f);
        root.header_text.set_foreground(math::Color::from_rgb_u32(0xFFFFFF));
        root.header_text.set_background(math::Color::from_rgb_u32(0x1565C0));
        root.header_text.set_padding(math::Thickness{ 16.0f, 16.0f, 16.0f, 16.0f });
        if (font) { root.header_text.set_font_face(font); }

        // ── 2. 副标题说明 ─────────────────────────────────────────────────
        root.subtitle.set_text("点击按钮测试交互：Normal 颜色 → 按下 → Pressed 颜色");
        root.subtitle.set_font_size(12.0f);
        root.subtitle.set_foreground(math::Color::from_rgb_u32(0x9E9E9E));
        root.subtitle.set_background(math::Color::Transparent);
        root.subtitle.set_padding(math::Thickness{ 4.0f, 6.0f, 4.0f, 6.0f });
        if (font) { root.subtitle.set_font_face(font); }

        // ── 3. "计数 +1" 蓝色主操作按钮 ─────────────────────────────────
        root.btn_count.set_text("计数 +1");
        root.btn_count.set_padding(math::Thickness{ 12.0f, 8.0f, 12.0f, 8.0f });
        root.btn_count.set_foreground(math::Color::from_rgb_u32(0xFFFFFF));
        root.btn_count.set_background(math::Color::from_rgb_u32(0x1976D2));
        root.btn_count.set_background_pressed(math::Color::from_rgb_u32(0x0D47A1));
        root.btn_count.set_border_color(math::Color::from_rgb_u32(0x0D47A1));
        if (font) { root.btn_count.set_font_face(font); }
        root.btn_count.add_handler(ui::Button::ClickEvent(), &DemoApp::on_click_count, this);

        // ── 4. "重  置" 灰色辅助按钮 ─────────────────────────────────────
        root.btn_reset.set_text("重  置");
        root.btn_reset.set_padding(math::Thickness{ 12.0f, 8.0f, 12.0f, 8.0f });
        root.btn_reset.set_foreground(math::Color::from_rgb_u32(0xFFFFFF));
        root.btn_reset.set_background(math::Color::from_rgb_u32(0x455A64));
        root.btn_reset.set_background_pressed(math::Color::from_rgb_u32(0x263238));
        root.btn_reset.set_border_color(math::Color::from_rgb_u32(0x263238));
        if (font) { root.btn_reset.set_font_face(font); }
        root.btn_reset.add_handler(ui::Button::ClickEvent(), &DemoApp::on_click_reset, this);

        // ── 5. "退  出" 红色危险操作按钮 ─────────────────────────────────
        root.btn_quit.set_text("退  出");
        root.btn_quit.set_padding(math::Thickness{ 12.0f, 8.0f, 12.0f, 8.0f });
        root.btn_quit.set_foreground(math::Color::from_rgb_u32(0xFFFFFF));
        root.btn_quit.set_background(math::Color::from_rgb_u32(0xC62828));
        root.btn_quit.set_background_pressed(math::Color::from_rgb_u32(0x7F0000));
        root.btn_quit.set_border_color(math::Color::from_rgb_u32(0x7F0000));
        if (font) { root.btn_quit.set_font_face(font); }
        root.btn_quit.add_handler(ui::Button::ClickEvent(), &DemoApp::on_click_quit, this);

        // ── 6. 状态计数标签 ───────────────────────────────────────────────
        root.status_label.set_text("当前计数：0 次");
        root.status_label.set_font_size(28.0f);
        root.status_label.set_foreground(math::Color::from_rgb_u32(0xE8E8E8));
        root.status_label.set_background(math::Color::Transparent);
        root.status_label.set_padding(math::Thickness{ 4.0f, 4.0f, 4.0f, 4.0f });
        if (font) { root.status_label.set_font_face(font); }

        // ── 7. 连接输入路由器 ─────────────────────────────────────────────
        router.set_root(&root);
        router.set_keyboard_focus(&root);
    }
};

// ── Ripple TimerProc 实现（定义于 DemoApp 之后以访问完整类型）───────────────

static VOID CALLBACK ripple_timer_proc(
    HWND /*hwnd*/, UINT /*msg*/, UINT_PTR /*id*/, DWORD /*time*/) noexcept
{
    if (!g_demo_app) { return; }

    auto& root = g_demo_app->root;

    // 推进每个按钮的背景色过渡 Storyboard（~60fps，dt = 16ms）
    // Storyboard::tick 内部将插值色写入 BackgroundProperty 的 Animation 优先级槽，
    // PropertyMetadata.affects_render = true → 触发 invalidate_render（由下方 render 刷新）
    constexpr float kDt = 16.0f / 1000.0f;
    root.btn_count.tick_bg_animation(kDt);
    root.btn_reset.tick_bg_animation(kDt);
    root.btn_quit.tick_bg_animation(kDt);

    // 检查是否仍有活跃的动画（ripple 或背景色过渡 Storyboard）
    const bool any_active =
        root.btn_count.has_active_animation() ||
        root.btn_reset.has_active_animation() ||
        root.btn_quit.has_active_animation();

    if (any_active) {
        // 驱动当前帧渲染
        if (g_demo_app->ui_win_) {
            g_demo_app->ui_win_->render();
        }
    } else {
        // 所有动画已结束，停止定时器
        KillTimer(nullptr, g_demo_app->ripple_timer_id_);
        g_demo_app->ripple_timer_id_ = 0;
        // 触发最后一帧以确保最终状态正确显示
        if (g_demo_app->ui_win_) {
            g_demo_app->ui_win_->render();
        }
    }
}

// ── 进程入口 ──────────────────────────────────────────────────────────────────

MINE_APPLICATION_MAIN(DemoApp)
