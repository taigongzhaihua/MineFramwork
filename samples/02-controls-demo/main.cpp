/**
 * @file main.cpp
 * @brief 02-controls-demo 示例：窗口 + 控件交互演示。
 *
 * 演示内容：
 *   1. mine::ui::Window 窗口（Platform → GFX → Paint → UI 完整渲染栈）
 *   2. 自定义 DemoRoot（FrameworkElement 子类），手动管理控件绝对位置布局
 *   3. Button 控件（点击计数 / 重置 / 退出，Normal/Pressed 视觉反馈）
 *   4. TextBlock 控件（标题 / 副标题 / 状态计数，真实字体渲染）
 *   5. InputRouter 将平台鼠标事件路由到视觉树
 *   6. Button::ClickEvent 路由事件驱动应用状态更新
 *
 * 架构说明：
 *   当前 mine.ui 实现中 Control 继承自 UIElement（非 FrameworkElement），
 *   因此无法直接通过 Panel::add_child(FrameworkElement*) 添加控件。
 *   DemoRoot 通过 Visual::add_child(Visual*) 将控件纳入视觉树，
 *   并在 arrange_override(Size) 中为每个控件调用 UIElement::arrange(Rect)，
 *   从而实现绝对坐标布局。
 *
 * 渲染时机：
 *   - Resized / DpiChanged：由 mine::ui::Window 内部自动处理（布局+渲染）。
 *   - 鼠标事件（悬停/按压/点击）：DemoApp → InputRouter 处理后手动调用 render()。
 */

// Win32 API：用于设置控制台 UTF-8 代码页，必须放在其他包含之前
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>

#include <mine/platform/PlatformAbi.h>
#include <mine/gfx/Gfx.h>
#include <mine/paint/Paint.h>
#include <mine/text/FontFace.h>

#include <mine/ui/window/Window.h>
#include <mine/ui/layout/FrameworkElement.h>
#include <mine/ui/controls/Button.h>
#include <mine/ui/controls/TextBlock.h>
#include <mine/ui/input/InputRouter.h>
#include <mine/ui/event/RoutedEventArgs.h>

#include <mine/math/Color.h>
#include <mine/math/Rect.h>
#include <mine/math/Thickness.h>

#include <cstdio>
#include <cmath>
#include <cstring>

// ── 命名空间别名 ──────────────────────────────────────────────────────────────

namespace platform = mine::platform;
namespace gfx      = mine::gfx;
namespace paint    = mine::paint;
namespace text     = mine::text;
namespace math     = mine::math;
namespace ui       = mine::ui;
namespace input    = mine::ui::input;
namespace core     = mine::core;

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

        // 按钮行：三个按钮横向排列，宽 120px、高 42px、间距 10px
        const float btn_w   = 120.0f;
        const float btn_h   =  42.0f;
        const float btn_gap =  10.0f;
        float bx = margin;
        btn_count.arrange(math::Rect{ bx, y, btn_w, btn_h });
        bx += btn_w + btn_gap;
        btn_reset.arrange(math::Rect{ bx, y, btn_w, btn_h });
        bx += btn_w + btn_gap;
        btn_quit.arrange(math::Rect{ bx, y, btn_w, btn_h });
        y += btn_h + 24.0f;

        // 状态标签：两侧留边距，高 44px（大字体）
        const float status_h = 44.0f;
        status_label.arrange(math::Rect{ margin, y, W - 2.0f * margin, status_h });

        return final_size;
    }
};

// ── 演示应用主类 ──────────────────────────────────────────────────────────────

/**
 * @brief 控件演示应用，持有 DemoRoot（UI 控件树）并响应窗口输入事件。
 *
 * 生命周期须短于 mine::ui::Window 与 platform::IWindow。
 * 析构前须从原生窗口取消事件订阅（native_win->events().unsubscribe(this)）。
 */
struct DemoApp : public platform::IWindowEventSink {

    // ── 应用层引用（非拥有，生命周期由 main 保证）────────────────────────
    ui::Window*                 ui_window = nullptr;  ///< UI 窗口（render() 接口）
    platform::IApplicationHost* app_host  = nullptr;  ///< 应用宿主（quit() 接口）

    // ── 输入路由器 ────────────────────────────────────────────────────────
    input::InputRouter          router;

    // ── UI 控件树根节点 ───────────────────────────────────────────────────
    DemoRoot                    root;

    // ── 运行时状态 ────────────────────────────────────────────────────────
    int click_count = 0;  ///< 点击计数器

    // ── IWindowEventSink 实现 ─────────────────────────────────────────────

    /**
     * @brief 接收原生窗口事件，将鼠标/键盘事件转发给 InputRouter，
     *        处理完毕后触发一次 UI 重绘。
     *
     * 注：Resized / DpiChanged 由 mine::ui::Window 内部自动处理，无需在此响应。
     */
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
            // 1. 命中测试 + 路由事件派发
            router.on_window_event(event);
            // 2. 刷新视觉状态（按压/悬停颜色变化）
            if (ui_window) {
                ui_window->render();
            }
            break;
        default:
            break;
        }
    }

    // ── 按钮点击回调（静态函数指针 + user_data 模式）─────────────────────

    /**
     * @brief "计数 +1" 按钮点击：递增计数器并刷新状态标签。
     */
    static void on_click_count(void* /*sender*/,
                               ui::RoutedEventArgs& /*args*/,
                               void* user_data)
    {
        auto* app = static_cast<DemoApp*>(user_data);
        ++app->click_count;
        app->update_status();
    }

    /**
     * @brief "重  置" 按钮点击：将计数归零并刷新状态标签。
     */
    static void on_click_reset(void* /*sender*/,
                               ui::RoutedEventArgs& /*args*/,
                               void* user_data)
    {
        auto* app = static_cast<DemoApp*>(user_data);
        app->click_count = 0;
        app->update_status();
    }

    /**
     * @brief "退  出" 按钮点击：请求应用宿主退出消息循环。
     */
    static void on_click_quit(void* /*sender*/,
                              ui::RoutedEventArgs& /*args*/,
                              void* user_data)
    {
        auto* app = static_cast<DemoApp*>(user_data);
        if (app->app_host) {
            app->app_host->quit(0);
        }
    }

    // ── 状态刷新 ──────────────────────────────────────────────────────────

    /**
     * @brief 将当前点击计数写入状态标签，并触发重绘。
     */
    void update_status()
    {
        char buf[128];
        std::snprintf(buf, sizeof(buf), "当前计数：%d 次", click_count);
        root.status_label.set_text(buf);
        if (ui_window) {
            ui_window->render();
        }
    }

    // ── UI 树构建 ─────────────────────────────────────────────────────────

    /**
     * @brief 初始化各控件的外观属性并连接事件处理器。
     *
     * @param window   UI 窗口（render() 接口）
     * @param host     应用宿主（quit() 接口）
     * @param font     已加载的字体面（nullptr 时文字回退到占位线）
     */
    void build_ui(ui::Window*                 window,
                  platform::IApplicationHost* host,
                  text::FontFace*             font)
    {
        ui_window = window;
        app_host  = host;

        // ── 1. 标题文字（蓝色背景标题栏）────────────────────────────────────
        root.header_text.set_text("MineFramework 控件演示");
        root.header_text.set_font_size(22.0f);
        root.header_text.set_foreground(math::Color::from_rgb_u32(0xFFFFFF));
        root.header_text.set_background(math::Color::from_rgb_u32(0x1565C0));
        root.header_text.set_padding(math::Thickness{ 16.0f, 16.0f, 16.0f, 16.0f });
        if (font) {
            root.header_text.set_font_face(font);
        }

        // ── 2. 副标题说明 ─────────────────────────────────────────────────
        root.subtitle.set_text("点击按钮测试交互：Normal 颜色 → 按下 → Pressed 颜色");
        root.subtitle.set_font_size(12.0f);
        root.subtitle.set_foreground(math::Color::from_rgb_u32(0x9E9E9E));
        root.subtitle.set_background(math::Color::Transparent);
        root.subtitle.set_padding(math::Thickness{ 4.0f, 6.0f, 4.0f, 6.0f });
        if (font) {
            root.subtitle.set_font_face(font);
        }

        // ── 3. "计数 +1" 蓝色主操作按钮 ─────────────────────────────────
        root.btn_count.set_text("计数 +1");
        root.btn_count.set_padding(math::Thickness{ 12.0f, 8.0f, 12.0f, 8.0f });
        root.btn_count.set_foreground(math::Color::from_rgb_u32(0xFFFFFF));
        root.btn_count.set_background(math::Color::from_rgb_u32(0x1976D2));
        root.btn_count.set_background_pressed(math::Color::from_rgb_u32(0x0D47A1));
        root.btn_count.set_border_color(math::Color::from_rgb_u32(0x0D47A1));
        root.btn_count.add_handler(ui::Button::ClickEvent(), &DemoApp::on_click_count, this);

        // ── 4. "重  置" 灰色辅助按钮 ─────────────────────────────────────
        root.btn_reset.set_text("重  置");
        root.btn_reset.set_padding(math::Thickness{ 12.0f, 8.0f, 12.0f, 8.0f });
        root.btn_reset.set_foreground(math::Color::from_rgb_u32(0xFFFFFF));
        root.btn_reset.set_background(math::Color::from_rgb_u32(0x455A64));
        root.btn_reset.set_background_pressed(math::Color::from_rgb_u32(0x263238));
        root.btn_reset.set_border_color(math::Color::from_rgb_u32(0x263238));
        root.btn_reset.add_handler(ui::Button::ClickEvent(), &DemoApp::on_click_reset, this);

        // ── 5. "退  出" 红色危险操作按钮 ─────────────────────────────────
        root.btn_quit.set_text("退  出");
        root.btn_quit.set_padding(math::Thickness{ 12.0f, 8.0f, 12.0f, 8.0f });
        root.btn_quit.set_foreground(math::Color::from_rgb_u32(0xFFFFFF));
        root.btn_quit.set_background(math::Color::from_rgb_u32(0xC62828));
        root.btn_quit.set_background_pressed(math::Color::from_rgb_u32(0x7F0000));
        root.btn_quit.set_border_color(math::Color::from_rgb_u32(0x7F0000));
        root.btn_quit.add_handler(ui::Button::ClickEvent(), &DemoApp::on_click_quit, this);

        // ── 6. 状态计数标签 ───────────────────────────────────────────────
        root.status_label.set_text("当前计数：0 次");
        root.status_label.set_font_size(28.0f);
        root.status_label.set_foreground(math::Color::from_rgb_u32(0xE8E8E8));
        root.status_label.set_background(math::Color::Transparent);
        root.status_label.set_padding(math::Thickness{ 4.0f, 4.0f, 4.0f, 4.0f });
        if (font) {
            root.status_label.set_font_face(font);
        }

        // ── 7. 连接输入路由器 ─────────────────────────────────────────────
        // DemoRoot 是 FrameworkElement → UIElement，InputRouter::set_root 接受 UIElement*
        router.set_root(&root);
        router.set_keyboard_focus(&root);
    }
};

// ── main ──────────────────────────────────────────────────────────────────────

int main(int /*argc*/, char* /*argv*/[])
{
    // 0. 设置控制台输出为 UTF-8，避免中文日志乱码
    //    （/utf-8 编译标志保证字符串字面量为 UTF-8，但控制台默认代码页为 GBK）
    SetConsoleOutputCP(CP_UTF8);

    // 1. 创建应用宿主（Win32 实现由链接库注入）
    auto host = mine::platform::create_application_host();
    if (!host) {
        std::fprintf(stderr, "错误：应用宿主创建失败\n");
        return 1;
    }

    // 2. 创建 D3D11 图形设备
    auto device = gfx::create_device(gfx::Backend::Auto);
    if (!device) {
        std::fprintf(stderr, "错误：图形设备创建失败（当前系统不支持 D3D11）\n");
        return 1;
    }

    // 3. 创建图形命令队列
    auto queue = device->create_queue(gfx::QueueType::Graphics);
    if (!queue) {
        std::fprintf(stderr, "错误：图形命令队列创建失败\n");
        return 1;
    }

    // 4. 创建 2D Paint 渲染器（D3D11 后端，由链接库注入）
    auto renderer = paint::create_renderer(device.get());
    if (!renderer) {
        std::fprintf(stderr, "错误：Paint 渲染器创建失败\n");
        return 1;
    }

    // 5. 加载字体（优先微软雅黑以支持中文，回退到西文字体，最终回退为占位线渲染）
    core::OwnedPtr<text::FontFace> font_face;
    {
        // 候选字体路径（Windows 系统字体目录）
        // 注意：msyh.ttc 为 Windows 10/11 格式，msyh.ttf 为 Windows 7/8 格式
        const char* candidates[] = {
            "C:\\Windows\\Fonts\\msyh.ttc",    // 微软雅黑（Windows 10/11，支持中文）
            "C:\\Windows\\Fonts\\msyh.ttf",    // 微软雅黑（Windows 7/8，支持中文）
            "C:\\Windows\\Fonts\\simhei.ttf",  // 黑体（备用中文字体）
            "C:\\Windows\\Fonts\\segoeui.ttf", // Segoe UI（西文回退）
            "C:\\Windows\\Fonts\\arial.ttf",   // Arial（西文回退）
        };
        for (const char* path : candidates) {
            font_face = text::FontFace::load_from_file(path);
            if (font_face) {
                std::fprintf(stdout, "信息：已加载字体 %s\n", path);
                break;
            }
        }
        if (!font_face) {
            std::fprintf(stdout, "警告：未找到系统字体，TextBlock 将以占位线展示文字\n");
        }
        std::fflush(stdout);  // 确保初始化日志立即可见（stdout 重定向时默认块缓冲）
    }

    // 6. 创建原生平台窗口
    platform::WindowDesc win_desc{};
    win_desc.title         = "MineFramework - 控件交互演示";
    win_desc.size          = { 800.0f, 500.0f };
    win_desc.auto_position = true;
    win_desc.resizable     = true;
    win_desc.kind          = platform::WindowKind::Normal;

    auto native_win = host->create_window(win_desc);
    if (!native_win) {
        std::fprintf(stderr, "错误：窗口创建失败\n");
        return 1;
    }

    // 7. 创建 UI 窗口（内部自动创建交换链并订阅 Resized/DpiChanged 事件）
    ui::Window ui_window{ *native_win, *device, *queue, *renderer };

    // 8. 构建演示应用（控件树 + 事件路由）
    DemoApp demo_app;
    demo_app.build_ui(&ui_window, host.get(), font_face.get());

    // 9. 订阅原生窗口事件（接收鼠标/键盘输入，触发渲染）
    native_win->events().subscribe(&demo_app);

    // 10. 将控件树挂载到 UI 窗口，立即触发布局 + 渲染
    // DemoRoot 继承 FrameworkElement → UIElement，符合 set_content(UIElement*) 要求
    ui_window.set_content(&demo_app.root);

    // 11. 显示窗口
    ui_window.show();

    // 12. 进入平台消息循环（阻塞至 quit() 被调用或窗口关闭）
    const int exit_code = host->run();

    // 13. 清理：取消输入事件订阅（UI 窗口自身的订阅在 ui_window 析构时清理）
    native_win->events().unsubscribe(&demo_app);

    return exit_code;
}
