/**
 * @file DemoWindow.cpp
 * @brief DemoWindow Window 组件包装类实现（手写，模拟 mmlc 生成的 .g.cpp 文件结构）。
 *
 * 本文件模拟 mmlc 从 DemoWindow.mml 生成的 DemoWindow.g.cpp，遵循
 * docs/04-precompiler.md §4.4.2 规定的生成代码契约。
 */
#include "DemoWindow.h"
#include <mine/core/TypeId.h>

// ── 命名空间别名 ──────────────────────────────────────────────────────────────
namespace core  = mine::core;
namespace math  = mine::math;
namespace paint = mine::paint;
namespace ui    = mine::ui;
namespace style = mine::ui::style;

namespace app {

// ── 构造 / 析构 ───────────────────────────────────────────────────────────────

DemoWindow::DemoWindow()
{
    // 配置窗口 pending 属性；UI 树在 setup() 中构建（因字体需等 Application 加载）
    _configure();
}

// ── 资源注入 + UI 树构建 ──────────────────────────────────────────────────────

void DemoWindow::setup(mine::text::FontFace* font)
{
    // 构建视觉树 → 安装绑定 → 安装状态机
    _build(font);
    _bind();
    _states();
}

// ── _configure：窗口 pending 属性（对应 MML window.xxx 属性）────────────────

void DemoWindow::_configure()
{
    // 对应 MML：window.title / window.size
    // 值暂存于 Impl::pending_* 字段，首次 win_.show() 时应用到原生窗口
    win_.set_title("MineFramework - 控件交互演示");
    win_.set_size({ 800.0f, 700.0f });
    // resizable=true / auto_position=true / kind=Normal 均为默认值，无需显式设置
}

// ── _build：视觉树构建（对应 MML 中的嵌套元素声明）───────────────────────────

void DemoWindow::_build(mine::text::FontFace* font)
{
    // ── 1. 根 Grid：2 行 × 1 列 ─────────────────────────────────────────────
    // Row 0：标题栏（固定 60px），Row 1：内容区（占满剩余空间）
    root_grid_.add_row(ui::RowDefinition{ ui::GridLength::pixel(60.0f) });
    root_grid_.add_row(ui::RowDefinition{ ui::GridLength::star() });

    // ── 2. 标题栏文字（Row 0，蓝色背景）──────────────────────────────────────
    header_text_.set_text("MineFramework 控件演示");
    header_text_.set_font_size(22.0f);
    header_text_.set_foreground(paint::Brush::solid_rgb(0xFFFFFF));
    header_text_.set_background(paint::Brush::solid_rgb(0x1565C0));
    header_text_.set_padding(math::Thickness{ 16.0f, 16.0f, 16.0f, 16.0f });
    if (font) { header_text_.set_font_face(font); }
    ui::Grid::set_row(header_text_, 0);
    root_grid_.add_child(&header_text_);

    // ── 3. 内容区垂直 StackPanel（Row 1）────────────────────────────────────
    ui::Grid::set_row(body_panel_, 1);
    root_grid_.add_child(&body_panel_);

    // ── 4. 交互说明副标题 ─────────────────────────────────────────────────
    subtitle_.set_text("点击按钮测试交互：Normal 颜色 → 按下 → Pressed 颜色");
    subtitle_.set_font_size(12.0f);
    subtitle_.set_foreground(paint::Brush::solid_rgb(0x9E9E9E));
    subtitle_.set_background(paint::Brush::solid(math::Color::Transparent));
    subtitle_.set_padding(math::Thickness{ 4.0f, 6.0f, 4.0f, 6.0f });
    subtitle_.set_margin(math::Thickness{ 16.0f, 8.0f, 16.0f, 0.0f });
    if (font) { subtitle_.set_font_face(font); }
    body_panel_.add_child(&subtitle_);

    // ── 5. 按钮行（水平 StackPanel）──────────────────────────────────────────
    btn_row_.set_orientation(ui::Orientation::Horizontal);
    btn_row_.set_margin(math::Thickness{ 16.0f, 16.0f, 16.0f, 0.0f });
    body_panel_.add_child(&btn_row_);

    // ── 5a. "计数 +1" 主操作按钮（蓝色）──────────────────────────────────────
    btn_count_.set_text("计数 +1");
    btn_count_.set_padding(math::Thickness{ 12.0f, 8.0f, 12.0f, 8.0f });
    btn_count_.set_foreground(paint::Brush::solid_rgb(0xFFFFFF));
    btn_count_.set_background(paint::Brush::solid_rgb(0x1976D2));
    btn_count_.set_background_hovered(paint::Brush::solid_rgb(0x1E88E5));
    btn_count_.set_background_pressed(paint::Brush::solid_rgb(0x0D47A1));
    btn_count_.set_border_color(paint::Brush::solid_rgb(0x0D47A1));
    btn_count_.set_margin(math::Thickness{ 0.0f, 0.0f, 10.0f, 0.0f });
    if (font) { btn_count_.set_font_face(font); }
    btn_count_.add_handler(ui::Button::ClickEvent(), &DemoWindow::s_on_click_count, this);
    btn_row_.add_child(&btn_count_);

    // ── 5b. "重  置" 辅助按钮（灰色）───────────────────────────────────────
    btn_reset_.set_text("重  置");
    btn_reset_.set_padding(math::Thickness{ 12.0f, 8.0f, 12.0f, 8.0f });
    btn_reset_.set_foreground(paint::Brush::solid_rgb(0xFFFFFF));
    btn_reset_.set_background(paint::Brush::solid_rgb(0x455A64));
    btn_reset_.set_background_hovered(paint::Brush::solid_rgb(0x546E7A));
    btn_reset_.set_background_pressed(paint::Brush::solid_rgb(0x263238));
    btn_reset_.set_border_color(paint::Brush::solid_rgb(0x263238));
    btn_reset_.set_margin(math::Thickness{ 0.0f, 0.0f, 10.0f, 0.0f });
    if (font) { btn_reset_.set_font_face(font); }
    btn_reset_.add_handler(ui::Button::ClickEvent(), &DemoWindow::s_on_click_reset, this);
    btn_row_.add_child(&btn_reset_);

    // ── 5c. "退  出" 危险操作按钮（红色）───────────────────────────────────
    btn_quit_.set_text("退  出");
    btn_quit_.set_padding(math::Thickness{ 12.0f, 8.0f, 12.0f, 8.0f });
    btn_quit_.set_foreground(paint::Brush::solid_rgb(0xFFFFFF));
    btn_quit_.set_background(paint::Brush::solid_rgb(0xC62828));
    btn_quit_.set_background_hovered(paint::Brush::solid_rgb(0xD32F2F));
    btn_quit_.set_background_pressed(paint::Brush::solid_rgb(0x7F0000));
    btn_quit_.set_border_color(paint::Brush::solid_rgb(0x7F0000));
    if (font) { btn_quit_.set_font_face(font); }
    btn_quit_.add_handler(ui::Button::ClickEvent(), &DemoWindow::s_on_click_quit, this);
    btn_row_.add_child(&btn_quit_);

    // ── 6. 计数状态标签 ──────────────────────────────────────────────────────
    status_label_.set_text("当前计数：0 次");
    status_label_.set_font_size(28.0f);
    status_label_.set_foreground(paint::Brush::solid_rgb(0xE8E8E8));
    status_label_.set_background(paint::Brush::solid(math::Color::Transparent));
    status_label_.set_padding(math::Thickness{ 4.0f, 4.0f, 4.0f, 4.0f });
    status_label_.set_margin(math::Thickness{ 16.0f, 24.0f, 16.0f, 0.0f });
    if (font) { status_label_.set_font_face(font); }
    body_panel_.add_child(&status_label_);

    // ── 7. Style 演示区 ───────────────────────────────────────────────────────

    // 构建绿色主题 Style（StyleSetter 优先级 20，不覆盖 Local 层 50 及以上）
    // Button 内置 Storyboard 读取 Hovered/PressedBackgroundProperty 驱动动画
    demo_style_
        .set_name("GreenButtonStyle")
        .set_target_type(core::TypeId::of<ui::Button>())
        .add_setter({ &ui::Button::BackgroundProperty,
                      core::Variant{ paint::Brush::solid_rgb(0x2E7D32) } })   // 深绿 Normal
        .add_setter({ &ui::Button::HoveredBackgroundProperty,
                      core::Variant{ paint::Brush::solid_rgb(0x43A047) } })   // 中绿 Hovered
        .add_setter({ &ui::Button::PressedBackgroundProperty,
                      core::Variant{ paint::Brush::solid_rgb(0x1B5E20) } })   // 墨绿 Pressed
        .add_setter({ &ui::Button::ForegroundProperty,
                      core::Variant{ paint::Brush::solid_rgb(0xFFFFFF) } })   // 白色文字
        .add_setter({ &ui::Button::PaddingProperty,
                      core::Variant{ math::Thickness{ 20.0f, 10.0f, 20.0f, 10.0f } } });

    // 区域分隔标题
    style_section_.set_text("── Style & 属性优先级演示 ──");
    style_section_.set_font_size(11.0f);
    style_section_.set_foreground(paint::Brush::solid_rgb(0x757575));
    style_section_.set_background(paint::Brush::solid_rgb(0xF0F0F0));
    style_section_.set_padding(math::Thickness{ 16.0f, 6.0f, 16.0f, 6.0f });
    style_section_.set_margin(math::Thickness{ 0.0f, 16.0f, 0.0f, 0.0f });
    if (font) { style_section_.set_font_face(font); }
    body_panel_.add_child(&style_section_);

    // 演示说明
    style_info_.set_text("下方绿色按钮颜色仅由 Style::add_setter(StyleSetter/20) 设置，未调用 set_background*");
    style_info_.set_font_size(11.0f);
    style_info_.set_foreground(paint::Brush::solid_rgb(0x757575));
    style_info_.set_background(paint::Brush::solid(math::Color::Transparent));
    style_info_.set_padding(math::Thickness{ 4.0f, 2.0f, 4.0f, 2.0f });
    style_info_.set_margin(math::Thickness{ 16.0f, 4.0f, 16.0f, 0.0f });
    if (font) { style_info_.set_font_face(font); }
    body_panel_.add_child(&style_info_);

    // Style 驱动的绿色按钮（demo_style_.apply() 写入 StyleSetter 层，不走 Local 层）
    btn_styled_.set_text("Style 驱动按钮（绿色）");
    btn_styled_.set_margin(math::Thickness{ 16.0f, 10.0f, 16.0f, 0.0f });
    if (font) { btn_styled_.set_font_face(font); }
    demo_style_.apply(btn_styled_);
    btn_styled_.add_handler(ui::Button::ClickEvent(), &DemoWindow::s_on_click_count, this);
    body_panel_.add_child(&btn_styled_);

    // ── 8. ControlTemplate 演示区 ────────────────────────────────────────────

    // 区域分隔标题
    tmpl_section_.set_text("── ControlTemplate（自定义视觉树）演示 ──");
    tmpl_section_.set_font_size(11.0f);
    tmpl_section_.set_foreground(paint::Brush::solid_rgb(0x757575));
    tmpl_section_.set_background(paint::Brush::solid_rgb(0xF0F0F0));
    tmpl_section_.set_padding(math::Thickness{ 16.0f, 6.0f, 16.0f, 6.0f });
    tmpl_section_.set_margin(math::Thickness{ 0.0f, 16.0f, 0.0f, 0.0f });
    if (font) { tmpl_section_.set_font_face(font); }
    body_panel_.add_child(&tmpl_section_);

    // 演示说明
    tmpl_info_.set_text("下方按钮用 set_template_root() 设置自定义模板根（Border + TextBlock 替换默认模板）");
    tmpl_info_.set_font_size(11.0f);
    tmpl_info_.set_foreground(paint::Brush::solid_rgb(0x757575));
    tmpl_info_.set_background(paint::Brush::solid(math::Color::Transparent));
    tmpl_info_.set_padding(math::Thickness{ 4.0f, 2.0f, 4.0f, 2.0f });
    tmpl_info_.set_margin(math::Thickness{ 16.0f, 4.0f, 16.0f, 0.0f });
    if (font) { tmpl_info_.set_font_face(font); }
    body_panel_.add_child(&tmpl_info_);

    // 构建自定义模板根（Border + TextBlock）
    // tmpl_label_ 是 DemoWindow 成员，生命周期覆盖模板根
    // Border 通过裸指针引用 tmpl_label_（不拥有），DemoWindow 析构时 tmpl_label_ 安全
    tmpl_label_.set_text("自定义 ControlTemplate 视觉树");
    tmpl_label_.set_font_size(13.0f);
    tmpl_label_.set_foreground(paint::Brush::solid_rgb(0x4A148C));
    tmpl_label_.set_background(paint::Brush::solid(math::Color::Transparent));
    tmpl_label_.set_padding(math::Thickness{ 16.0f, 8.0f, 16.0f, 8.0f });
    if (font) { tmpl_label_.set_font_face(font); }

    {
        // Border 作为模板根，通过 OwnedPtr 转移所有权给 btn_tmpl_
        auto border_root = core::make_owned<ui::Border>();
        border_root->set_border_thickness(math::Thickness::uniform(2.0f));
        border_root->set_border_color(paint::Brush::solid_rgb(0x7B1FA2));  // 深紫边框
        border_root->set_background(paint::Brush::solid_rgb(0xF3E5F5));   // 浅紫背景
        border_root->set_child(&tmpl_label_);  // Border 通过裸指针引用（不拥有）
        btn_tmpl_.set_template_root(std::move(border_root));
    }

    btn_tmpl_.set_margin(math::Thickness{ 16.0f, 10.0f, 16.0f, 0.0f });
    if (font) { btn_tmpl_.set_font_face(font); }
    btn_tmpl_.add_handler(ui::Button::ClickEvent(), &DemoWindow::s_on_click_count, this);
    body_panel_.add_child(&btn_tmpl_);

    // ── 9. 将根布局挂载到窗口 ────────────────────────────────────────────────
    // set_content 同时自动设置 InputRouter 路由根节点与默认键盘焦点
    win_.set_content(&root_grid_);
}

// ── _bind：属性绑定（当前演示为手动路由事件模式）────────────────────────────

void DemoWindow::_bind()
{
    // 待 mine.ui.binding 完整实现后，此处生成：
    //   status_label_.bind_text([this]{ return status_text_(); }, {prop_StatusText()});
    // 当前演示：通过 update_status_() 在事件处理中手动刷新文本。
}

// ── _states：组件级状态机（当前演示为空）────────────────────────────────────

void DemoWindow::_states()
{
    // 各 Button 的 Normal / Hovered / Pressed 状态由 Button 自身的内置 Storyboard 管理。
    // 若此组件有跨控件的协调状态（如整体加载中禁用所有按钮），则在此处注册。
}

// ── 属性实现 ──────────────────────────────────────────────────────────────────

void DemoWindow::set_status_text(const char* text)
{
    status_label_.set_text(text);
    win_.render();
}

// ── 内部状态刷新 ──────────────────────────────────────────────────────────────

void DemoWindow::update_status_()
{
    char buf[128];
    std::snprintf(buf, sizeof(buf), "当前计数：%d 次", click_count_);
    status_label_.set_text(buf);
    win_.render();
}

// ── 事件处理（静态路由桩）────────────────────────────────────────────────────

void DemoWindow::s_on_click_count(void* /*sender*/,
                                  mine::ui::RoutedEventArgs& /*args*/,
                                  void* user_data)
{
    auto* self = static_cast<DemoWindow*>(user_data);
    ++self->click_count_;
    self->update_status_();
}

void DemoWindow::s_on_click_reset(void* /*sender*/,
                                  mine::ui::RoutedEventArgs& /*args*/,
                                  void* user_data)
{
    auto* self = static_cast<DemoWindow*>(user_data);
    self->click_count_ = 0;
    self->update_status_();
}

void DemoWindow::s_on_click_quit(void* /*sender*/,
                                 mine::ui::RoutedEventArgs& /*args*/,
                                 void* user_data)
{
    auto* self = static_cast<DemoWindow*>(user_data);
    // 触发 closeRequested 信号 → Application::quit()
    if (self->on_close_requested_) {
        self->on_close_requested_();
    }
}

} // namespace app
