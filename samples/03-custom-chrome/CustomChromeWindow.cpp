/**
 * @file CustomChromeWindow.cpp
 * @brief 自定义无边框标题栏演示窗口实现。
 *
 * 关键设计决策（WindowChrome + 拖拽）：
 *
 * 1. IsCustomChrome = true
 *    WM_NCCALCSIZE 将整个窗口区域（含 NC 区）划为客户区，
 *    框架自行渲染标题栏，系统不再绘制默认边框/标题。
 *
 * 2. CaptionHeight = 0
 *    WM_NCHITTEST 不自动对标题栏高度范围返回 HTCAPTION。
 *    若 CaptionHeight > 0，系统会将该区域的鼠标按下转为窗口拖拽，
 *    导致标题栏中的按钮无法收到 WM_LBUTTONDOWN，无法点击。
 *
 * 3. 手动拖拽（drag_region_ MouseDownEvent）：
 *    在拖拽区 TextBlock 上注册 MouseDownEvent；
 *    鼠标左键按下时调用 ReleaseCapture() 释放框架鼠标捕获，
 *    再通过 PostMessageW(hwnd, WM_NCLBUTTONDOWN, HTCAPTION, 0)
 *    通知系统"用户在 NC 标题栏区域按下了鼠标"，系统接管后续拖拽。
 *
 * 4. ResizeBorderThickness = 6px
 *    平台层在 WM_NCHITTEST 处理中，对窗口边框 6px 范围内返回
 *    HTLEFT / HTRIGHT / HTTOP / HTBOTTOM / HT*CORNER，
 *    系统据此处理拖拽调整窗口大小。
 */

#include "CustomChromeWindow.h"

#include <windows.h>

#include <mine/platform/IWindow.h>
#include <mine/platform/WindowCornerPreference.h>
#include <mine/ui/input/InputEvents.h>
#include <mine/ui/input/MouseEventArgs.h>
#include <mine/ui/input/MouseButton.h>

// ── 命名空间别名 ──────────────────────────────────────────────────────────────
namespace math  = mine::math;
namespace paint = mine::paint;
namespace ui    = mine::ui;
namespace input = mine::ui::input;

namespace app {

// ── 构造 / 析构 ───────────────────────────────────────────────────────────────

CustomChromeWindow::CustomChromeWindow()
{
    set_title("MineFramework \xe2\x80\x94 \xe8\x87\xaa\xe5\xae\x9a\xe4\xb9\x89\xe6\xa0\x87\xe9\xa2\x98\xe6\xa0\x8f\xe6\xbc\x94\xe7\xa4\xba");
    set_size({ 820.0f, 560.0f });

    // 启用自定义 Chrome：隐藏系统标题栏，整个窗口区域划为客户区
    set_custom_chrome(true);

    // CaptionHeight = 0：不使用系统 HTCAPTION 区域
    // （非零值会使标题栏按钮无法点击，见文件头注释）
    set_caption_height(0.0f);

    // 保留系统 resize 边框热区（6px，四边和四角）
    set_resize_border_thickness(math::Thickness::uniform(6.0f));

    // Windows 11 圆角（旧系统自动忽略此设置）
    set_corner_preference(mine::platform::WindowCornerPreference::Round);
}

CustomChromeWindow::~CustomChromeWindow()
{
    if (!is_closed()) close();
}

// ── 公开接口 ──────────────────────────────────────────────────────────────────

void CustomChromeWindow::setup(mine::text::FontFace* font)
{
    build_(font);
}

// ── 视觉树构建 ────────────────────────────────────────────────────────────────

void CustomChromeWindow::build_(mine::text::FontFace* font)
{
    // ── 根 Grid：两行 ─────────────────────────────────────────────────────
    root_grid_.add_row(ui::RowDefinition{ ui::GridLength::pixel(48.0f) });  // 标题栏固定高度
    root_grid_.add_row(ui::RowDefinition{ ui::GridLength::star() });         // 内容区弹性填充

    // ─────────────────────────────────────────────────────────────────────
    // 标题栏背景 Border（Grid/StackPanel 无 set_background，通过 Border 包裹实现）
    // ─────────────────────────────────────────────────────────────────────
    title_bar_border_.set_background(paint::Brush::solid_rgb(0x1F1F1F));
    title_bar_border_.set_border_thickness(math::Thickness::uniform(0.0f));
    title_bar_border_.set_child(&title_bar_grid_);

    // 两列：左侧拖拽区弹性填充（1*），右侧按钮面板自动宽度（Auto）
    title_bar_grid_.add_column(ui::ColumnDefinition{ ui::GridLength::star()  });
    title_bar_grid_.add_column(ui::ColumnDefinition{ ui::GridLength::auto_() });

    // ── 拖拽区域（drag_region_）──────────────────────────────────────────
    // 显示应用图标（◆）和窗口标题；鼠标左键按下时触发系统窗口拖拽。
    // UTF-8 字节序列：◆ = E2 97 86，— = E2 80 94
    drag_region_.set_text(
        "\xe2\x97\x86  MineFramework \xe2\x80\x94 "
        "\xe8\x87\xaa\xe5\xae\x9a\xe4\xb9\x89"
        "\xe6\xa0\x87\xe9\xa2\x98\xe6\xa0\x8f"
        "\xe6\xbc\x94\xe7\xa4\xba");
    drag_region_.set_font_size(13.0f);
    drag_region_.set_foreground(paint::Brush::solid_rgb(0xD0D0D0));
    drag_region_.set_padding(math::Thickness{ 14.0f, 0.0f, 0.0f, 0.0f });
    drag_region_.set_vertical_alignment(ui::VerticalAlignment::Center);
    if (font) drag_region_.set_font_face(font);
    // 注册鼠标按下事件：左键按下 → ReleaseCapture + WM_NCLBUTTONDOWN(HTCAPTION)
    drag_region_.add_handler(
        input::MouseDownEvent(),
        &CustomChromeWindow::s_on_drag_mouse_down,
        this);
    ui::Grid::set_column(drag_region_, 0);
    title_bar_grid_.add_child(&drag_region_);

    // ── 控制按钮面板（btn_panel_）────────────────────────────────────────
    btn_panel_.set_orientation(ui::Orientation::Horizontal);
    ui::Grid::set_column(btn_panel_, 1);

    // 配置标题栏按钮公共外观（透明底色、悬停高亮、充满标题栏高度）
    auto setup_chrome_btn = [&](ui::Button& btn,
                                const char* label,
                                uint32_t    hover_bg,
                                uint32_t    pressed_bg) {
        btn.set_text(label);
        btn.set_font_size(12.0f);
        btn.set_foreground(paint::Brush::solid_rgb(0xCCCCCC));
        // 默认背景完全透明（标题栏深灰底色透出）
        btn.set_background(paint::Brush::solid_rgba(0x00000000));
        btn.set_background_hovered(paint::Brush::solid_rgb(hover_bg));
        btn.set_background_pressed(paint::Brush::solid_rgb(pressed_bg));
        // 水平内边距 20px 保证点击面积；垂直 0 使按钮 Stretch 充满 48px 标题栏
        btn.set_padding(math::Thickness{ 20.0f, 0.0f, 20.0f, 0.0f });
        btn.set_vertical_alignment(ui::VerticalAlignment::Stretch);
        if (font) btn.set_font_face(font);
    };

    // 最小化按钮（─，U+2500 BOX DRAWINGS LIGHT HORIZONTAL）
    setup_chrome_btn(btn_minimize_, "\xe2\x94\x80", 0x3A3A3A, 0x2A2A2A);
    btn_minimize_.add_handler(
        ui::Button::ClickEvent(),
        &CustomChromeWindow::s_on_minimize_click,
        this);
    btn_panel_.add_child(&btn_minimize_);

    // 最大化/还原按钮（□，U+25A1 WHITE SQUARE）
    setup_chrome_btn(btn_maximize_, "\xe2\x96\xa1", 0x3A3A3A, 0x2A2A2A);
    btn_maximize_.add_handler(
        ui::Button::ClickEvent(),
        &CustomChromeWindow::s_on_maximize_click,
        this);
    btn_panel_.add_child(&btn_maximize_);

    // 关闭按钮（✕，U+2715）：悬停变 Windows 红（#C42B1C）
    setup_chrome_btn(btn_close_, "\xe2\x9c\x95", 0xC42B1C, 0x9B2213);
    btn_close_.add_handler(
        ui::Button::ClickEvent(),
        &CustomChromeWindow::s_on_close_click,
        this);
    btn_panel_.add_child(&btn_close_);

    title_bar_grid_.add_child(&btn_panel_);

    // 将标题栏 Border（含 title_bar_grid_）挂入根 Grid 第 0 行
    ui::Grid::set_row(title_bar_border_, 0);
    root_grid_.add_child(&title_bar_border_);

    // ─────────────────────────────────────────────────────────────────────
    // 内容区（content_panel_）
    // ─────────────────────────────────────────────────────────────────────
    content_panel_.set_orientation(ui::Orientation::Vertical);

    // 演示区标题（较大字号，白色）
    desc_title_.set_text(
        "WindowChrome "
        "\xe8\x87\xaa\xe5\xae\x9a\xe4\xb9\x89"
        "\xe6\xa0\x87\xe9\xa2\x98\xe6\xa0\x8f"
        "\xe6\xbc\x94\xe7\xa4\xba");
    desc_title_.set_font_size(20.0f);
    desc_title_.set_foreground(paint::Brush::solid_rgb(0xF0F0F0));
    desc_title_.set_padding(math::Thickness{ 32.0f, 32.0f, 32.0f, 20.0f });
    if (font) desc_title_.set_font_face(font);
    content_panel_.add_child(&desc_title_);

    // 特性说明条目（灰色，较小字号）
    // 使用 UTF-8 字节序列确保跨编译环境的字符集兼容性
    const char* lines[5] = {
        // ● IsCustomChrome = true：隐藏系统标题栏，整个窗口区域划为客户区
        "\xe2\x97\x8f  IsCustomChrome = true\xef\xbc\x9a"
        "\xe9\x9a\x90\xe8\x97\x8f\xe7\xb3\xbb\xe7\xbb\x9f\xe6\xa0\x87\xe9\xa2\x98\xe6\xa0\x8f\xef\xbc\x8c"
        "\xe6\x95\xb4\xe4\xb8\xaa\xe7\xaa\x97\xe5\x8f\xa3\xe5\x8c\xba\xe5\x9f\x9f\xe5\x88\x92\xe4\xb8\xba\xe5\xae\xa2\xe6\x88\xb7\xe5\x8c\xba",

        // ● CaptionHeight = 0：系统不接管拖拽，由应用层手动发送 WM_NCLBUTTONDOWN
        "\xe2\x97\x8f  CaptionHeight = 0\xef\xbc\x9a"
        "\xe7\xb3\xbb\xe7\xbb\x9f\xe4\xb8\x8d\xe6\x8e\xa5\xe7\xae\xa1\xe6\x8b\x96\xe6\x8b\xbd\xef\xbc\x8c"
        "\xe7\x94\xb1\xe5\xba\x94\xe7\x94\xa8\xe5\xb1\x82\xe6\x89\x8b\xe5\x8a\xa8\xe5\x8f\x91\xe9\x80\x81 WM_NCLBUTTONDOWN",

        // ● ResizeBorderThickness = 6px：系统自动处理四边/四角 resize 命中测试
        "\xe2\x97\x8f  ResizeBorderThickness = 6px\xef\xbc\x9a"
        "\xe7\xb3\xbb\xe7\xbb\x9f\xe8\x87\xaa\xe5\x8a\xa8\xe5\xa4\x84\xe7\x90\x86\xe5\x9b\x9b\xe8\xbe\xb9/"
        "\xe5\x9b\x9b\xe8\xa7\x92 resize \xe5\x91\xbd\xe4\xb8\xad\xe6\xb5\x8b\xe8\xaf\x95",

        // ● CornerPreference = Round：Windows 11 圆角（旧系统自动忽略）
        "\xe2\x97\x8f  CornerPreference = Round\xef\xbc\x9a"
        "Windows 11 \xe5\x9c\x86\xe8\xa7\x92\xef\xbc\x88\xe6\x97\xa7\xe7\xb3\xbb\xe7\xbb\x9f\xe8\x87\xaa\xe5\x8a\xa8\xe5\xbf\xbd\xe7\x95\xa5\xef\xbc\x89",

        // ● 拖拽：点击上方标题文字区域并拖动即可移动窗口
        "\xe2\x97\x8f  "
        "\xe6\x8b\x96\xe6\x8b\xbd\xef\xbc\x9a"
        "\xe7\x82\xb9\xe5\x87\xbb\xe4\xb8\x8a\xe6\x96\xb9\xe6\xa0\x87\xe9\xa2\x98\xe6\x96\x87\xe5\xad\x97\xe5\x8c\xba\xe5\x9f\x9f\xe5\xb9\xb6\xe6\x8b\x96\xe5\x8a\xa8\xe5\x8d\xb3\xe5\x8f\xaf\xe7\xa7\xbb\xe5\x8a\xa8\xe7\xaa\x97\xe5\x8f\xa3",
    };

    for (int i = 0; i < 5; ++i) {
        desc_lines_[i].set_text(lines[i]);
        desc_lines_[i].set_font_size(13.5f);
        desc_lines_[i].set_foreground(paint::Brush::solid_rgb(0xA0A0A0));
        desc_lines_[i].set_padding(math::Thickness{ 32.0f, 6.0f, 32.0f, 6.0f });
        if (font) desc_lines_[i].set_font_face(font);
        content_panel_.add_child(&desc_lines_[i]);
    }

    // 内容区背景 Border
    content_border_.set_background(paint::Brush::solid_rgb(0x191919));
    content_border_.set_border_thickness(math::Thickness::uniform(0.0f));
    content_border_.set_child(&content_panel_);

    // 将内容区 Border（含 content_panel_）挂入根 Grid 第 1 行
    ui::Grid::set_row(content_border_, 1);
    root_grid_.add_child(&content_border_);

    // 设置根内容元素
    set_content(&root_grid_);
}

// ── 静态事件处理函数 ──────────────────────────────────────────────────────────

void CustomChromeWindow::s_on_drag_mouse_down(
    void* /*sender*/, ui::RoutedEventArgs& args, void* ud)
{
    // 仅响应鼠标左键（右键不触发拖拽）
    auto& margs = static_cast<input::MouseEventArgs&>(args);
    if (margs.button() != input::MouseButton::Left) {
        return;
    }

    auto* self = static_cast<CustomChromeWindow*>(ud);
    auto hwnd  = static_cast<HWND>(self->native_window().native_handle().ptr);

    // 释放框架内部鼠标捕获后，由系统接管拖拽操作。
    // 等价于 WPF 中的 DragMove()，不会触发额外的 WM_MOUSEMOVE 消息。
    ::ReleaseCapture();
    ::PostMessageW(hwnd, WM_NCLBUTTONDOWN, HTCAPTION, 0);

    // 标记事件已处理，阻止进一步冒泡
    args.set_handled(true);
}

void CustomChromeWindow::s_on_minimize_click(
    void* /*sender*/, ui::RoutedEventArgs& /*args*/, void* ud)
{
    auto* self = static_cast<CustomChromeWindow*>(ud);
    auto hwnd  = static_cast<HWND>(self->native_window().native_handle().ptr);
    ::ShowWindow(hwnd, SW_MINIMIZE);
}

void CustomChromeWindow::s_on_maximize_click(
    void* /*sender*/, ui::RoutedEventArgs& /*args*/, void* ud)
{
    auto* self = static_cast<CustomChromeWindow*>(ud);
    auto hwnd  = static_cast<HWND>(self->native_window().native_handle().ptr);
    // IsZoomed 检查当前是否处于最大化状态，据此切换
    ::ShowWindow(hwnd, ::IsZoomed(hwnd) ? SW_RESTORE : SW_MAXIMIZE);
}

void CustomChromeWindow::s_on_close_click(
    void* /*sender*/, ui::RoutedEventArgs& /*args*/, void* ud)
{
    auto* self = static_cast<CustomChromeWindow*>(ud);
    // close() 触发 Closing → Closed 事件链；
    // Application 监听主窗口的 Closed 事件，自动调用 quit(0)
    self->close();
}

} // namespace app
