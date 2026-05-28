/**
 * @file main.cpp
 * @brief 02-controls-demo 示例：窗口 + 控件交互演示。
 *
 * 演示内容：
 *   1. mine::ui::app::Application 应用基类（封装平台初始化、设备、渲染器）
 *   2. mine::ui::Window 窗口（通过 Application::create_window() 创建）
 *   3. Grid + StackPanel 布局容器，由框架自动完成两遍布局（Measure/Arrange）
 *   4. Button 控件（点击计数 / 重置 / 退出，Normal/Pressed 视觉反馈）
 *   5. TextBlock 控件（标题 / 副标题 / 状态计数，真实字体渲染）
 *   6. Button::ClickEvent 路由事件驱动应用状态更新
 *
 * 架构说明：
 *   DemoApp 仅继承 mine::ui::app::Application。平台实例化、图形设备、渲染器、
 *   鼠标/键盘输入路由、帧定时器和默认字体加载均由基类自动管理。
 *   Window 以值类型成员 ui_win_ 声明（pending 状态），on_startup() 中仅需
 *   set_title/set_size/show() 即可完成窗口创建与显示。
 */

// Win32 API：用于设置控制台 UTF-8 代码页（xmake 已通过 add_defines 定义宏，此处无需重复）
#include <windows.h>

#include <mine/ui/app/AppAll.h>         // Application + MINE_APPLICATION_MAIN

#include <mine/ui/window/Window.h>
#include <mine/ui/layout/LayoutAll.h>
// WindowDesc 和 WindowKind 由框架内部处理，无需在 sample 中 include
#include <mine/ui/controls/Button.h>
#include <mine/ui/controls/TextBlock.h>
#include <mine/ui/event/RoutedEventArgs.h>
#include <mine/ui/style/StyleAll.h>          // Style / VisualStateManager / ControlTemplate
#include <mine/ui/controls/Border.h>         // Border（ControlTemplate 演示用）

#include <mine/math/Color.h>
#include <mine/math/Rect.h>
#include <mine/math/Thickness.h>
#include <mine/paint/Brush.h>

#include <cstdio>
#include <cmath>
#include <cstring>

// ── 命名空间别名 ──────────────────────────────────────────────────────────────

namespace text     = mine::text;
namespace math     = mine::math;
namespace ui       = mine::ui;
namespace core     = mine::core;
namespace paint    = mine::paint;          // 画刷命名空间别名
namespace style    = mine::ui::style;      // Style / VSM / ControlTemplate 命名空间别名

// ── 演示应用主类 ──────────────────────────────────────────────────────────────

/**
 * @brief 控件演示应用。
 *
 * 仅继承 mine::ui::app::Application。平台实例化、图形设备、渲染器、
 * 鼠标/键盘输入路由、帧定时器和默认字体加载均由基类自动管理。
 */
struct DemoApp : public mine::ui::app::Application {

    // ── UI 布局树 ──────────────────────────────────────────────────────────
    ui::Grid        root_grid;    ///< 根 Grid（2行×1列：Row0=标题栏，Row1=内容区）
    ui::StackPanel  body_panel;   ///< 内容区垂直 StackPanel
    ui::StackPanel  btn_row;      ///< 按钮行水平 StackPanel
    ui::TextBlock   header_text;  ///< 标题（蓝色背景标题栏，Row 0）
    ui::TextBlock   subtitle;     ///< 副标题说明文字
    ui::Button      btn_count;    ///< "计数 +1" 按钮
    ui::Button      btn_reset;    ///< "重  置" 按钮
    ui::Button      btn_quit;     ///< "退  出" 按钮
    ui::TextBlock   status_label; ///< 显示当前点击计数的状态标签

    // ── Style 演示区（生命周期与 DemoApp 一致）────────────────────────────────
    ui::TextBlock   style_section_;   ///< "── Style 演示 ──" 区域分隔标题
    ui::TextBlock   style_info_;      ///< Style 演示说明文字
    ui::Button      btn_styled_;      ///< 颜色完全由 Style::add_setter 控制的绿色按钮
    style::Style    demo_style_;      ///< Style 对象（生命周期须覆盖所有对它的引用）

    // ── ControlTemplate 演示区（生命周期与 DemoApp 一致）─────────────────────
    ui::TextBlock   tmpl_section_;    ///< "── ControlTemplate 演示 ──" 区域分隔标题
    ui::TextBlock   tmpl_info_;       ///< ControlTemplate 演示说明文字
    ui::Button      btn_tmpl_;        ///< 使用自定义模板根（Border + TextBlock）的按钮
    ui::TextBlock   tmpl_label_;      ///< 模板根 Border 的子元素（DemoApp 管理生命周期）


    // ── 运行时状态 ────────────────────────────────────────────────────────
    int          click_count = 0;  ///< 点击计数器

    /// 主窗口（值成员，无参构造，pending 状态）
    /// 最后声明确保最先析构（渲染先停，后析构 UI 元素），由 show() 完成懒初始化
    ui::Window   ui_win_;

    // ── Application 生命周期扩展点 ───────────────────────────────────────

    /**
     * @brief 应用启动：设置控制台代码页，创建窗口，构建 UI。
     *
     * 字体加载、输入路由和帧定时器均由 Application 基类自动管理。
     */
    void on_startup(int /*argc*/, char** /*argv*/) override
    {
        // 设置控制台输出为 UTF-8，避免中文日志乱码
        SetConsoleOutputCP(CP_UTF8);

        // 配置窗口属性（pending 状态下缓存，首次 show() 时应用到原生窗口）
        ui_win_.set_title("MineFramework - 控件交互演示");
        ui_win_.set_size({ 800.0f, 700.0f });
        // auto_position=true / resizable=true / kind=Normal 均为默认值，无需显式设置

        // 构建 UI 控件树并连接事件（default_font() 由基类在此前自动加载）
        build_ui();

        // 将控件树挂载到窗口，触发首次布局 + 渲染
        // set_content 同时自动设置 InputRouter 的路由根节点和默认键盘焦点
        ui_win_.set_content(&root_grid);

        // 首次 show()：通过 IWindowContext 创建原生窗口、绑定图形资源，
        // 安装 tick_and_render 回调，自动成为主窗口
        ui_win_.show();
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
        status_label.set_text(buf);
        ui_win_.render();
    }

    // ── UI 树构建 ─────────────────────────────────────────────────────────

    void build_ui()
    {
        text::FontFace* font = default_font();  // 框架已在 on_startup 前自动加载默认系统字体
        // ── 1. 配置根 Grid：2行×1列 ─────────────────────────────────────────
        // Row 0: 标题栏（固定 60px），Row 1: 内容区（占剩余全部空间）
        root_grid.add_row(ui::RowDefinition{ ui::GridLength::pixel(60.0f) });
        root_grid.add_row(ui::RowDefinition{ ui::GridLength::star() });

        // ── 2. 标题文字（蓝色背景，Row 0，水平/垂直均 Stretch）──────────────
        header_text.set_text("MineFramework 控件演示");
        header_text.set_font_size(22.0f);
        header_text.set_foreground(paint::Brush::solid_rgb(0xFFFFFF));
        header_text.set_background(paint::Brush::solid_rgb(0x1565C0));
        header_text.set_padding(math::Thickness{ 16.0f, 16.0f, 16.0f, 16.0f });
        if (font) { header_text.set_font_face(font); }
        ui::Grid::set_row(header_text, 0);
        root_grid.add_child(&header_text);

        // ── 3. 内容区垂直 StackPanel（Row 1）───────────────────────────────
        ui::Grid::set_row(body_panel, 1);
        root_grid.add_child(&body_panel);

        // ── 4. 副标题说明 ─────────────────────────────────────────────────
        subtitle.set_text("点击按钮测试交互：Normal 颜色 → 按下 → Pressed 颜色");
        subtitle.set_font_size(12.0f);
        subtitle.set_foreground(paint::Brush::solid_rgb(0x9E9E9E));
        subtitle.set_background(paint::Brush::solid(math::Color::Transparent));
        subtitle.set_padding(math::Thickness{ 4.0f, 6.0f, 4.0f, 6.0f });
        subtitle.set_margin(math::Thickness{ 16.0f, 8.0f, 16.0f, 0.0f });
        if (font) { subtitle.set_font_face(font); }
        body_panel.add_child(&subtitle);

        // ── 5. 按钮行（水平 StackPanel）──────────────────────────────────
        btn_row.set_orientation(ui::Orientation::Horizontal);
        btn_row.set_margin(math::Thickness{ 16.0f, 16.0f, 16.0f, 0.0f });
        body_panel.add_child(&btn_row);

        // ── 5a. "计数 +1" 蓝色主操作按钮 ─────────────────────────────────
        btn_count.set_text("计数 +1");
        btn_count.set_padding(math::Thickness{ 12.0f, 8.0f, 12.0f, 8.0f });
        btn_count.set_foreground(paint::Brush::solid_rgb(0xFFFFFF));
        btn_count.set_background(paint::Brush::solid_rgb(0x1976D2));
        btn_count.set_background_hovered(paint::Brush::solid_rgb(0x1E88E5));  // Material Blue 400
        btn_count.set_background_pressed(paint::Brush::solid_rgb(0x0D47A1));
        btn_count.set_border_color(paint::Brush::solid_rgb(0x0D47A1));
        btn_count.set_margin(math::Thickness{ 0.0f, 0.0f, 10.0f, 0.0f });
        if (font) { btn_count.set_font_face(font); }
        btn_count.add_handler(ui::Button::ClickEvent(), &DemoApp::on_click_count, this);
        btn_row.add_child(&btn_count);

        // ── 5b. "重  置" 灰色辅助按钮 ─────────────────────────────────────
        btn_reset.set_text("重  置");
        btn_reset.set_padding(math::Thickness{ 12.0f, 8.0f, 12.0f, 8.0f });
        btn_reset.set_foreground(paint::Brush::solid_rgb(0xFFFFFF));
        btn_reset.set_background(paint::Brush::solid_rgb(0x455A64));
        btn_reset.set_background_hovered(paint::Brush::solid_rgb(0x546E7A));  // Blue Grey 600
        btn_reset.set_background_pressed(paint::Brush::solid_rgb(0x263238));
        btn_reset.set_border_color(paint::Brush::solid_rgb(0x263238));
        btn_reset.set_margin(math::Thickness{ 0.0f, 0.0f, 10.0f, 0.0f });
        if (font) { btn_reset.set_font_face(font); }
        btn_reset.add_handler(ui::Button::ClickEvent(), &DemoApp::on_click_reset, this);
        btn_row.add_child(&btn_reset);

        // ── 5c. "退  出" 红色危险操作按钮 ─────────────────────────────────
        btn_quit.set_text("退  出");
        btn_quit.set_padding(math::Thickness{ 12.0f, 8.0f, 12.0f, 8.0f });
        btn_quit.set_foreground(paint::Brush::solid_rgb(0xFFFFFF));
        btn_quit.set_background(paint::Brush::solid_rgb(0xC62828));
        btn_quit.set_background_hovered(paint::Brush::solid_rgb(0xD32F2F));  // Red 700
        btn_quit.set_background_pressed(paint::Brush::solid_rgb(0x7F0000));
        btn_quit.set_border_color(paint::Brush::solid_rgb(0x7F0000));
        if (font) { btn_quit.set_font_face(font); }
        btn_quit.add_handler(ui::Button::ClickEvent(), &DemoApp::on_click_quit, this);
        btn_row.add_child(&btn_quit);

        // ── 6. 状态计数标签 ───────────────────────────────────────────────
        status_label.set_text("当前计数：0 次");
        status_label.set_font_size(28.0f);
        status_label.set_foreground(paint::Brush::solid_rgb(0xE8E8E8));
        status_label.set_background(paint::Brush::solid(math::Color::Transparent));
        status_label.set_padding(math::Thickness{ 4.0f, 4.0f, 4.0f, 4.0f });
        status_label.set_margin(math::Thickness{ 16.0f, 24.0f, 16.0f, 0.0f });
        if (font) { status_label.set_font_face(font); }
        body_panel.add_child(&status_label);

        // ── 7. Style 程序化演示区 ────────────────────────────────────────────

        // 8a. 构建 Style（程序化路径）：通过 add_setter 设置绿色主题配色
        //     StyleSetter(20) 优先级，不覆盖 Local(50) 及以上的属性值
        //     Button 自带的 Storyboard 会读取 HoveredBackgroundProperty /
        //     PressedBackgroundProperty 的值作为动画终值，无需额外写入 Local
        demo_style_
            .set_name("GreenButtonStyle")
            .set_target_type(core::TypeId::of<ui::Button>())
            .add_setter({&ui::Button::BackgroundProperty,
                         core::Variant{paint::Brush::solid_rgb(0x2E7D32)}})   // 深绿 Normal
            .add_setter({&ui::Button::HoveredBackgroundProperty,
                         core::Variant{paint::Brush::solid_rgb(0x43A047)}})   // 中绿 Hovered
            .add_setter({&ui::Button::PressedBackgroundProperty,
                         core::Variant{paint::Brush::solid_rgb(0x1B5E20)}})   // 墨绿 Pressed
            .add_setter({&ui::Button::ForegroundProperty,
                         core::Variant{paint::Brush::solid_rgb(0xFFFFFF)}})   // 白色文字
            .add_setter({&ui::Button::PaddingProperty,
                         core::Variant{math::Thickness{20.0f, 10.0f, 20.0f, 10.0f}}});

        // 8b. 区域分隔标题
        style_section_.set_text("\u2500\u2500 Style & \u5c5e\u6027\u4f18\u5148\u7ea7\u6f14\u793a \u2500\u2500");
        style_section_.set_font_size(11.0f);
        style_section_.set_foreground(paint::Brush::solid_rgb(0x757575));
        style_section_.set_background(paint::Brush::solid_rgb(0xF0F0F0));
        style_section_.set_padding(math::Thickness{16.0f, 6.0f, 16.0f, 6.0f});
        style_section_.set_margin(math::Thickness{0.0f, 16.0f, 0.0f, 0.0f});
        if (font) { style_section_.set_font_face(font); }
        body_panel.add_child(&style_section_);

        // 8c. 说明文字
        style_info_.set_text("\u4e0b\u65b9\u7eff\u8272\u6309\u9215\u989c\u8272\u4ec5\u7531 Style::add_setter(StyleSetter/20) \u8bbe\u7f6e\uff0c\u672a\u8c03\u7528 set_background*");
        style_info_.set_font_size(11.0f);
        style_info_.set_foreground(paint::Brush::solid_rgb(0x757575));
        style_info_.set_background(paint::Brush::solid(math::Color::Transparent));
        style_info_.set_padding(math::Thickness{4.0f, 2.0f, 4.0f, 2.0f});
        style_info_.set_margin(math::Thickness{16.0f, 4.0f, 16.0f, 0.0f});
        if (font) { style_info_.set_font_face(font); }
        body_panel.add_child(&style_info_);

        // 8d. btn_styled_：通过 Style::apply 写入颜色，不调用 set_background*
        //     Button 自带 Storyboard 读取 StyleSetter(20) 写入的颜色值驱动动画
        btn_styled_.set_text("Style \u9a71\u52a8\u6309\u9215\uff08\u7eff\u8272\uff09");
        btn_styled_.set_margin(math::Thickness{16.0f, 10.0f, 16.0f, 0.0f});
        if (font) { btn_styled_.set_font_face(font); }
        demo_style_.apply(btn_styled_);
        btn_styled_.add_handler(ui::Button::ClickEvent(), &DemoApp::on_click_count, this);
        body_panel.add_child(&btn_styled_);

        // ── 9. ControlTemplate 演示区 ─────────────────────────────────────────

        // 9a. 区域分隔标题
        tmpl_section_.set_text("\u2500\u2500 ControlTemplate\uff08\u81ea\u5b9a\u4e49\u89c6\u89c9\u6811\uff09\u6f14\u793a \u2500\u2500");
        tmpl_section_.set_font_size(11.0f);
        tmpl_section_.set_foreground(paint::Brush::solid_rgb(0x757575));
        tmpl_section_.set_background(paint::Brush::solid_rgb(0xF0F0F0));
        tmpl_section_.set_padding(math::Thickness{16.0f, 6.0f, 16.0f, 6.0f});
        tmpl_section_.set_margin(math::Thickness{0.0f, 16.0f, 0.0f, 0.0f});
        if (font) { tmpl_section_.set_font_face(font); }
        body_panel.add_child(&tmpl_section_);

        // 9b. 说明文字
        tmpl_info_.set_text("\u4e0b\u65b9\u6309\u9215\u7528 set_template_root() \u8bbe\u7f6e\u81ea\u5b9a\u4e49\u6a21\u677f\u6839\uff08Border + TextBlock \u66ff\u6362\u9ed8\u8ba4\u6a21\u677f\uff09");
        tmpl_info_.set_font_size(11.0f);
        tmpl_info_.set_foreground(paint::Brush::solid_rgb(0x757575));
        tmpl_info_.set_background(paint::Brush::solid(math::Color::Transparent));
        tmpl_info_.set_padding(math::Thickness{4.0f, 2.0f, 4.0f, 2.0f});
        tmpl_info_.set_margin(math::Thickness{16.0f, 4.0f, 16.0f, 0.0f});
        if (font) { tmpl_info_.set_font_face(font); }
        body_panel.add_child(&tmpl_info_);

        // 9c. 构建自定义模板根（Border + TextBlock）
        //     tmpl_label_ 是 DemoApp 成员，生命周期覆盖模板根，Border 通过裸指针引用
        tmpl_label_.set_text("\u81ea\u5b9a\u4e49 ControlTemplate \u89c6\u89c9\u6811");
        tmpl_label_.set_font_size(13.0f);
        tmpl_label_.set_foreground(paint::Brush::solid_rgb(0x4A148C));
        tmpl_label_.set_background(paint::Brush::solid(math::Color::Transparent));
        tmpl_label_.set_padding(math::Thickness{16.0f, 8.0f, 16.0f, 8.0f});
        if (font) { tmpl_label_.set_font_face(font); }

        {
            // Border 作为模板根，Control 通过 OwnedPtr 拥有其生命周期
            auto border_root = core::make_owned<ui::Border>();
            border_root->set_border_thickness(math::Thickness::uniform(2.0f));
            border_root->set_border_color(paint::Brush::solid_rgb(0x7B1FA2));   // 深紫边框
            border_root->set_background(paint::Brush::solid_rgb(0xF3E5F5));    // 浅紫背景
            border_root->set_child(&tmpl_label_);  // Border 通过裸指针引用（不拥有）
            btn_tmpl_.set_template_root(std::move(border_root));
        }

        btn_tmpl_.set_margin(math::Thickness{16.0f, 10.0f, 16.0f, 0.0f});
        if (font) { btn_tmpl_.set_font_face(font); }
        btn_tmpl_.add_handler(ui::Button::ClickEvent(), &DemoApp::on_click_count, this);
        body_panel.add_child(&btn_tmpl_);
    }
};

// ── 进程入口 ──────────────────────────────────────────────────────────────────

MINE_APPLICATION_MAIN(DemoApp)
