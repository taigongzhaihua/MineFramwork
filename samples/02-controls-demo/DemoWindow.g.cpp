/**
 * @file DemoWindow.g.cpp
 * @brief DemoWindowBase 实现 —— 由 mmlc 从 DemoWindow.mml 自动生成，请勿手动修改。
 *
 * 本文件模拟 mmlc 从 DemoWindow.mml 生成的 .g.cpp，遵循
 * docs/04-precompiler.md §4.4.2 规定的生成代码契约。
 *
 * 析构安全保证：~DemoWindowBase() 第一句调用 close()。
 * 视觉树调用继承自 Window 的方法（set_content / render），无 win_. 前缀。
 */
#include "DemoWindow.g.h"
#include <mine/core/TypeId.h>
#include <mine/core/Memory.h>
#include <mine/ui/controls/ContentPresenter.h>
#include <mine/ui/animation/Storyboard.h>
#include <mine/ui/animation/EasingFunction.h>
#include <mine/ui/animation/Duration.h>

// ── 命名空间别名 ──────────────────────────────────────────────────────────────
namespace core  = mine::core;
namespace math  = mine::math;
namespace paint = mine::paint;
namespace ui    = mine::ui;
namespace style = mine::ui::style;
namespace anim  = mine::ui::animation;

// ── ControlTemplate BuildFn（文件作用域，供 tmpl_green_ / tmpl_orange_ 使用）──────────────────

/// 绿色圆角模板：ContentPresenter（"content" Part）直接作为模板根
/// 背景由 Button::on_render 通过 BackgroundProperty DP 绘制（VSM 驱动颜色过渡）
/// 圆角边框由 Button::on_render 通过 BorderColorProperty DP 绘制（与背景同形）
static void s_build_green_template(mine::ui::DependencyObject& target)
{
    auto& btn     = static_cast<ui::Button&>(target);
    auto  content = core::make_owned<ui::ContentPresenter>();

    // "content" Part 名称与 Button::on_apply_template 中 find_template_child 一致
    content->set_template_name("content");

    // TemplateBind：宿主 Content/Padding 属性自动同步到 ContentPresenter
    btn.bind_template(*content,
                      ui::ContentPresenter::ContentProperty,
                      ui::ContentControl::ContentProperty);
    btn.bind_template(*content,
                      ui::ContentPresenter::PaddingProperty,
                      ui::Button::PaddingProperty);

    // 安装 VSM（绿色状态机）
    // 背景色和边框色均通过 Button::BackgroundProperty / BorderColorProperty 驱动
    // Button::on_render 负责绘制圆角背景和圆角边框，与 VSM 动画完全兼容
    static style::Style s_green_style = []() {
        style::Style s;
        s.set_name("GreenTemplate_Style")
         .set_target_type(core::TypeId::of<ui::Button>())
         .add_setter({ &ui::Button::BackgroundProperty,
                       core::Variant{ paint::Brush::solid_rgb(0x1B5E20) } })  // 深绿 Normal(P5)
         .add_setter({ &ui::Button::ForegroundProperty,
                       core::Variant{ paint::Brush::solid_rgb(0xFFFFFF) } })  // 白色文字
         .add_setter({ &ui::Button::BorderColorProperty,
                       core::Variant{ paint::Brush::solid_rgb(0x4CAF50) } }); // 亮绿边框(P5)
        {
            style::VisualStateSetters vs;
            vs.state_name = "Hovered";
            vs.setters.push_back({ &ui::Button::BackgroundProperty,
                core::Variant{ paint::Brush::solid_rgb(0x2E7D32) } });  // 悬停：略亮绿
            s.add_state_setters(std::move(vs));
        }
        {
            style::VisualStateSetters vs;
            vs.state_name = "Pressed";
            vs.setters.push_back({ &ui::Button::BackgroundProperty,
                core::Variant{ paint::Brush::solid_rgb(0x388E3C) } });  // 按下：更亮绿
            s.add_state_setters(std::move(vs));
        }
        return s;
    }();

    btn.set_vsm_style(&s_green_style);  // 使用绿色样式驱动 VSM

    style::VisualStateManager vsm{ btn };
    vsm.define_state("Normal");
    vsm.define_state("Hovered");
    vsm.define_state("Pressed");
    vsm.define_state("Disabled");
    auto* btn_ptr = &btn;
    vsm.add_transition("*", "Hovered", [btn_ptr](anim::Storyboard& sb) {
        sb.animate_dp(*btn_ptr, ui::Button::BackgroundProperty,
                      anim::Duration::milliseconds(120.0f), anim::QuadEaseOut);
    });
    vsm.add_transition("*", "Normal", [btn_ptr](anim::Storyboard& sb) {
        sb.animate_dp(*btn_ptr, ui::Button::BackgroundProperty,
                      anim::Duration::milliseconds(100.0f), anim::QuadEaseOut);
    });
    vsm.add_transition("*", "Pressed", [btn_ptr](anim::Storyboard& sb) {
        sb.animate_dp(*btn_ptr, ui::Button::BackgroundProperty,
                      anim::Duration::milliseconds(60.0f), anim::QuadEaseIn);
    });
    vsm.set_style(&s_green_style);
    btn.set_visual_state_manager(std::move(vsm));
    s_green_style.apply(btn);  // 写入 P5 基线値（含 BorderColorProperty）

    btn.set_template_root(std::move(content));
}

/// 橙色圆角模板：ContentPresenter（"content" Part）直接作为模板根
/// 背景由 Button::on_render 通过 BackgroundProperty DP 绘制（VSM 驱动颜色过渡）
/// 圆角边框由 Button::on_render 通过 BorderColorProperty DP 绘制（与背景同形）
static void s_build_orange_template(mine::ui::DependencyObject& target)
{
    auto& btn     = static_cast<ui::Button&>(target);
    auto  content = core::make_owned<ui::ContentPresenter>();

    content->set_template_name("content");

    btn.bind_template(*content,
                      ui::ContentPresenter::ContentProperty,
                      ui::ContentControl::ContentProperty);
    btn.bind_template(*content,
                      ui::ContentPresenter::PaddingProperty,
                      ui::Button::PaddingProperty);

    static style::Style s_orange_style = []() {
        style::Style s;
        s.set_name("OrangeTemplate_Style")
         .set_target_type(core::TypeId::of<ui::Button>())
         .add_setter({ &ui::Button::BackgroundProperty,
                       core::Variant{ paint::Brush::solid_rgb(0xBF360C) } })  // 深橙 Normal(P5)
         .add_setter({ &ui::Button::ForegroundProperty,
                       core::Variant{ paint::Brush::solid_rgb(0xFFFFFF) } })  // 白色文字
         .add_setter({ &ui::Button::BorderColorProperty,
                       core::Variant{ paint::Brush::solid_rgb(0xFF7043) } }); // 亮橙边框(P5)
        {
            style::VisualStateSetters vs;
            vs.state_name = "Hovered";
            vs.setters.push_back({ &ui::Button::BackgroundProperty,
                core::Variant{ paint::Brush::solid_rgb(0xE64A19) } });  // 悬停：略亮橙
            s.add_state_setters(std::move(vs));
        }
        {
            style::VisualStateSetters vs;
            vs.state_name = "Pressed";
            vs.setters.push_back({ &ui::Button::BackgroundProperty,
                core::Variant{ paint::Brush::solid_rgb(0xFF5722) } });  // 按下：更亮橙
            s.add_state_setters(std::move(vs));
        }
        return s;
    }();

    btn.set_vsm_style(&s_orange_style);

    style::VisualStateManager vsm{ btn };
    vsm.define_state("Normal");
    vsm.define_state("Hovered");
    vsm.define_state("Pressed");
    vsm.define_state("Disabled");
    auto* btn_ptr = &btn;
    vsm.add_transition("*", "Hovered", [btn_ptr](anim::Storyboard& sb) {
        sb.animate_dp(*btn_ptr, ui::Button::BackgroundProperty,
                      anim::Duration::milliseconds(120.0f), anim::QuadEaseOut);
    });
    vsm.add_transition("*", "Normal", [btn_ptr](anim::Storyboard& sb) {
        sb.animate_dp(*btn_ptr, ui::Button::BackgroundProperty,
                      anim::Duration::milliseconds(100.0f), anim::QuadEaseOut);
    });
    vsm.add_transition("*", "Pressed", [btn_ptr](anim::Storyboard& sb) {
        sb.animate_dp(*btn_ptr, ui::Button::BackgroundProperty,
                      anim::Duration::milliseconds(60.0f), anim::QuadEaseIn);
    });
    vsm.set_style(&s_orange_style);
    btn.set_visual_state_manager(std::move(vsm));
    s_orange_style.apply(btn);

    btn.set_template_root(std::move(content));
}

namespace app {

// ── 构造 / 析构 ───────────────────────────────────────────────────────────────

DemoWindowBase::DemoWindowBase()
{
    // 配置窗口 pending 属性；UI 树在 setup() 中构建（因字体需等 Application 加载）
    _configure();
}

DemoWindowBase::~DemoWindowBase()
{
    // 第一句调用 close()：停止渲染循环，释放 swapchain，解注册 IWindowContext。
    // 之后 C++ 按声明逆序析构数据成员（各 UI 元素），最后析构基类 Window（no-op）。
    if (!is_closed()) close();
}

// ── 资源注入 + UI 树构建 ──────────────────────────────────────────────────────

void DemoWindowBase::setup(mine::text::FontFace* font)
{
    _build(font);
    _bind();
    _states();
}

// ── _configure：窗口 pending 属性（对应 MML window.xxx 属性）────────────────

void DemoWindowBase::_configure()
{
    // 直接调用继承自 Window 的方法（无 win_. 前缀）
    // 值暂存于 Impl::pending_* 字段，首次 show() 时应用到原生窗口
    set_title("MineFramework - 控件交互演示");
    set_size({ 800.0f, 700.0f });
}

// ── _build：视觉树构建（对应 MML 中的嵌套元素声明）───────────────────────────

void DemoWindowBase::_build(mine::text::FontFace* font)
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

    // ── 5a. "计数 +1" 主操作按鈕（蓝色）───────────────────────────────────────────────────
    btn_count_.set_text("计数 +1");
    btn_count_.set_padding(math::Thickness{ 12.0f, 8.0f, 12.0f, 8.0f });
    btn_count_.set_foreground(paint::Brush::solid_rgb(0xFFFFFF));
    // 注：set_background 写入 Local(P2)，优先级高于 StyleTrigger(P4)，
    // 因此 Hover/Pressed 状态色被遁盖（按鈕颜色不随状态变化）。
    // 若需 Hover/Pressed 颜色动画，应改用 Style(P5+P4) 驱动（参见下方绿色按鈕示例）。
    btn_count_.set_background(paint::Brush::solid_rgb(0x1976D2));
    btn_count_.set_border_color(paint::Brush::solid_rgb(0x0D47A1));
    btn_count_.set_margin(math::Thickness{ 0.0f, 0.0f, 10.0f, 0.0f });
    if (font) { btn_count_.set_font_face(font); }
    // 点击 → 调用虚方法 on_count_clicked()（多态分派到 code-behind）
    btn_count_.add_handler(ui::Button::ClickEvent(), &DemoWindowBase::s_on_click_count, this);
    btn_row_.add_child(&btn_count_);

    // ── 5b. "重  置" 辅助按钮（灰色）───────────────────────────────────────
    btn_reset_.set_text("重  置");
    btn_reset_.set_padding(math::Thickness{ 12.0f, 8.0f, 12.0f, 8.0f });
    btn_reset_.set_foreground(paint::Brush::solid_rgb(0xFFFFFF));
    btn_reset_.set_background(paint::Brush::solid_rgb(0x455A64));  // Local(P2)，遁盖 P4 状态色
    btn_reset_.set_border_color(paint::Brush::solid_rgb(0x263238));
    btn_reset_.set_margin(math::Thickness{ 0.0f, 0.0f, 10.0f, 0.0f });
    if (font) { btn_reset_.set_font_face(font); }
    // 点击 → 调用虚方法 on_reset_clicked()（多态分派到 code-behind）
    btn_reset_.add_handler(ui::Button::ClickEvent(), &DemoWindowBase::s_on_click_reset, this);
    btn_row_.add_child(&btn_reset_);

    // ── 5c. "退  出" 危险操作按钮（红色）───────────────────────────────────
    btn_quit_.set_text("退  出");
    btn_quit_.set_padding(math::Thickness{ 12.0f, 8.0f, 12.0f, 8.0f });
    btn_quit_.set_foreground(paint::Brush::solid_rgb(0xFFFFFF));
    btn_quit_.set_background(paint::Brush::solid_rgb(0xC62828));  // Local(P2)，遁盖 P4 状态色
    btn_quit_.set_border_color(paint::Brush::solid_rgb(0x7F0000));
    if (font) { btn_quit_.set_font_face(font); }
    // 点击 → 触发 closeRequested 信号（MML 声明式绑定，无需 code-behind method）
    btn_quit_.add_handler(ui::Button::ClickEvent(), &DemoWindowBase::s_on_click_quit, this);
    btn_row_.add_child(&btn_quit_);

    // ── 6. 计数状态标签（#id=status_label_，通过 protected 引用暴露）────────
    status_label_s_.set_text("当前计数：0 次");
    status_label_s_.set_font_size(28.0f);
    status_label_s_.set_foreground(paint::Brush::solid_rgb(0xE8E8E8));
    status_label_s_.set_background(paint::Brush::solid(math::Color::Transparent));
    status_label_s_.set_padding(math::Thickness{ 4.0f, 4.0f, 4.0f, 4.0f });
    status_label_s_.set_margin(math::Thickness{ 16.0f, 24.0f, 16.0f, 0.0f });
    if (font) { status_label_s_.set_font_face(font); }
    body_panel_.add_child(&status_label_s_);

    // ── 7. Style 演示区 ───────────────────────────────────────────────────────

    // 构建绿色主题 Style（三层分离范式）：
    //   P5 StyleSetter  = Normal 状态基线外观値（apply 写入）
    //   P4 VisualStateSetters = Hovered / Pressed 状态终値（go_to_state 自动写入）
    demo_style_
        .set_name("GreenButtonStyle")
        .set_target_type(core::TypeId::of<ui::Button>())
        .add_setter({ &ui::Button::BackgroundProperty,
                      core::Variant{ paint::Brush::solid_rgb(0x2E7D32) } })   // 深综Normal(P5)
        .add_setter({ &ui::Button::ForegroundProperty,
                      core::Variant{ paint::Brush::solid_rgb(0xFFFFFF) } })   // 白色文字(P5)
        .add_setter({ &ui::Button::PaddingProperty,
                      core::Variant{ math::Thickness{ 20.0f, 10.0f, 20.0f, 10.0f } } });

    // P4 StyleTrigger：Hovered 状态终値（中综Hover）
    {
        style::VisualStateSetters vs;
        vs.state_name = "Hovered";
        vs.setters.push_back({ &ui::Button::BackgroundProperty,
            core::Variant{ paint::Brush::solid_rgb(0x43A047) } });
        demo_style_.add_state_setters(std::move(vs));
    }
    // P4 StyleTrigger：Pressed 状态终値（墨综Pressed）
    {
        style::VisualStateSetters vs;
        vs.state_name = "Pressed";
        vs.setters.push_back({ &ui::Button::BackgroundProperty,
            core::Variant{ paint::Brush::solid_rgb(0x1B5E20) } });
        demo_style_.add_state_setters(std::move(vs));
    }

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
    style_info_.set_text(
        "绿色按鈕用 Style 驱动："
        "StyleSetter(P5) 写 Normal 基线色，"
        "VisualStateSetters(P4) 写 Hovered/Pressed 终値");
    style_info_.set_font_size(11.0f);
    style_info_.set_foreground(paint::Brush::solid_rgb(0x757575));
    style_info_.set_background(paint::Brush::solid(math::Color::Transparent));
    style_info_.set_padding(math::Thickness{ 4.0f, 2.0f, 4.0f, 2.0f });
    style_info_.set_margin(math::Thickness{ 16.0f, 4.0f, 16.0f, 0.0f });
    if (font) { style_info_.set_font_face(font); }
    body_panel_.add_child(&style_info_);

    // Style 驱动的绿色按钮（set_vsm_style 在首次 measure 时自动完成两步工作：
    //   1. demo_style_.apply() 写入 P5 基线绿色；
    //   2. VSM 连接到 demo_style_，悬停/按下时 apply_state() 触发 P4 绿色状态色）
    // 点击也触发计数（演示 Style 不影响功能）
    btn_styled_.set_text("Style 驱动按钮（绿色）");
    btn_styled_.set_margin(math::Thickness{ 16.0f, 10.0f, 16.0f, 0.0f });
    if (font) { btn_styled_.set_font_face(font); }
    btn_styled_.set_vsm_style(&demo_style_);   // 勿再单独调用 demo_style_.apply()
    btn_styled_.add_handler(ui::Button::ClickEvent(), &DemoWindowBase::s_on_click_count, this);
    body_panel_.add_child(&btn_styled_);

    // ── 8. ControlTemplate 演示区（WPF 风格：BuildFn + 命名 Part + set_control_template 切换）────

    // 区域分隔标题
    tmpl_section_.set_text("── ControlTemplate（BuildFn + Part 命名 + 动态切换）演示 ──");
    tmpl_section_.set_font_size(11.0f);
    tmpl_section_.set_foreground(paint::Brush::solid_rgb(0x757575));
    tmpl_section_.set_background(paint::Brush::solid_rgb(0xF0F0F0));
    tmpl_section_.set_padding(math::Thickness{ 16.0f, 6.0f, 16.0f, 6.0f });
    tmpl_section_.set_margin(math::Thickness{ 0.0f, 16.0f, 0.0f, 0.0f });
    if (font) { tmpl_section_.set_font_face(font); }
    body_panel_.add_child(&tmpl_section_);

    // 演示说明
    tmpl_info_.set_text(
        "下方按钮用 ControlTemplate（BuildFn + set_template_name 命名）。"
        "点击「切换模板」后 on_apply_template() 自动重新绑定同名 Part \"content\"，"
        "按钮功能（计数）透明延续。");
    tmpl_info_.set_font_size(11.0f);
    tmpl_info_.set_foreground(paint::Brush::solid_rgb(0x757575));
    tmpl_info_.set_background(paint::Brush::solid(math::Color::Transparent));
    tmpl_info_.set_padding(math::Thickness{ 4.0f, 2.0f, 4.0f, 2.0f });
    tmpl_info_.set_margin(math::Thickness{ 16.0f, 4.0f, 16.0f, 0.0f });
    if (font) { tmpl_info_.set_font_face(font); }
    body_panel_.add_child(&tmpl_info_);

    // 配置两个 ControlTemplate（BuildFn 为文件作用域 static 函数，无捕获，可隐式转换为函数指针）
    tmpl_green_.build_fn_  = &s_build_green_template;
    tmpl_orange_.build_fn_ = &s_build_orange_template;

    // btn_tmpl_：演示按钮，初始使用绿色模板
    btn_tmpl_.set_text("ControlTemplate 演示按钮（点击计数）");
    btn_tmpl_.set_padding(math::Thickness{ 16.0f, 10.0f, 16.0f, 10.0f });
    if (font) { btn_tmpl_.set_font_face(font); }
    btn_tmpl_.set_margin(math::Thickness{ 16.0f, 10.0f, 16.0f, 0.0f });
    // 通过 TemplateProperty DP 设置模板，on_template_dp_changed 会在下次 measure 时
    // 调用 build_fn_ 构建视觉树，并触发 on_apply_template() 绑定 "content" Part
    btn_tmpl_.set_control_template(&tmpl_green_);
    btn_tmpl_.add_handler(ui::Button::ClickEvent(), &DemoWindowBase::s_on_click_count, this);
    body_panel_.add_child(&btn_tmpl_);

    // btn_switch_tmpl_：切换模板按钮（本身使用默认模板，演示 StyleSetter+VSM 路径）
    //
    // 注意：此前用 set_background(Local P50) 设置深灰色，导致 Local 优先级 > StyleTrigger，
    // VSM 状态颜色被遮盖，Hover/Pressed 颜色始终无效。
    // 修复：改用 switch_style_（StyleSetter P20 + StateSetters P30），确保 VSM 可正常驱动颜色。
    switch_style_
        .set_name("SwitchButtonStyle")
        .set_target_type(core::TypeId::of<ui::Button>())
        .add_setter({ &ui::Button::BackgroundProperty,
                      core::Variant{ paint::Brush::solid_rgb(0x37474F) } })   // 深蓝灰 Normal(P20)
        .add_setter({ &ui::Button::ForegroundProperty,
                      core::Variant{ paint::Brush::solid_rgb(0xFFFFFF) } })   // 白色文字(P20)
        .add_setter({ &ui::Button::PaddingProperty,
                      core::Variant{ math::Thickness{ 12.0f, 8.0f, 12.0f, 8.0f } } });

    // P30 StyleTrigger：Hovered 状态终値（略亮蓝灰）
    {
        style::VisualStateSetters vs;
        vs.state_name = "Hovered";
        vs.setters.push_back({ &ui::Button::BackgroundProperty,
            core::Variant{ paint::Brush::solid_rgb(0x546E7A) } });
        switch_style_.add_state_setters(std::move(vs));
    }
    // P30 StyleTrigger：Pressed 状态终値（极深蓝灰）
    {
        style::VisualStateSetters vs;
        vs.state_name = "Pressed";
        vs.setters.push_back({ &ui::Button::BackgroundProperty,
            core::Variant{ paint::Brush::solid_rgb(0x263238) } });
        switch_style_.add_state_setters(std::move(vs));
    }

    btn_switch_tmpl_.set_text("切换模板（绿色 \u2194 橙色）");
    if (font) { btn_switch_tmpl_.set_font_face(font); }
    btn_switch_tmpl_.set_margin(math::Thickness{ 16.0f, 8.0f, 16.0f, 16.0f });
    btn_switch_tmpl_.set_vsm_style(&switch_style_);   // 用样式驱动颜色，避免 Local 遮盖 VSM 状态色
    btn_switch_tmpl_.add_handler(ui::Button::ClickEvent(), &DemoWindowBase::s_on_switch_tmpl, this);
    body_panel_.add_child(&btn_switch_tmpl_);

    // ── 9. 将根布局挂载到窗口 ────────────────────────────────────────────────
    // 直接调用继承自 Window 的 set_content()（无 win_. 前缀）
    // set_content 同时自动设置 InputRouter 路由根节点与默认键盘焦点
    set_content(&root_grid_);
}

// ── _bind：属性绑定（当前演示为手动路由事件模式）────────────────────────────

void DemoWindowBase::_bind()
{
    // 待 mine.ui.binding 完整实现后，此处生成：
    //   status_label_s_.bind_text([this]{ return status_text_(); }, {prop_StatusText()});
    // 当前演示：通过 code-behind 在事件处理中手动更新 status_label_ 并调用 render()。
}

// ── _states：组件级状态机（当前演示为空）────────────────────────────────────

void DemoWindowBase::_states()
{
    // 各 Button 的 Normal / Hovered / Pressed 状态由 Button 自身的内置 Storyboard 管理。
    // 若此组件有跨控件的协调状态（如整体加载中禁用所有按钮），则在此处注册。
}

// ── 事件处理（静态路由桩，通过虚方法多态分派到 code-behind）──────────────────

void DemoWindowBase::s_on_click_count(void* /*sender*/,
                                      mine::ui::RoutedEventArgs& /*args*/,
                                      void* user_data)
{
    // 调用纯虚 method → 多态分派到 DemoWindow::on_count_clicked()
    static_cast<DemoWindowBase*>(user_data)->on_count_clicked();
}

void DemoWindowBase::s_on_click_reset(void* /*sender*/,
                                      mine::ui::RoutedEventArgs& /*args*/,
                                      void* user_data)
{
    // 调用纯虚 method → 多态分派到 DemoWindow::on_reset_clicked()
    static_cast<DemoWindowBase*>(user_data)->on_reset_clicked();
}

void DemoWindowBase::s_on_click_quit(void* /*sender*/,
                                     mine::ui::RoutedEventArgs& /*args*/,
                                     void* user_data)
{
    // 触发 closeRequested 信号（MML 声明式绑定，无需 method）
    static_cast<DemoWindowBase*>(user_data)->emit_close_requested();
}

void DemoWindowBase::s_on_switch_tmpl(void* /*sender*/,
                                      mine::ui::RoutedEventArgs& /*args*/,
                                      void* user_data)
{
    auto* self = static_cast<DemoWindowBase*>(user_data);
    // 切换模板标志
    self->tmpl_is_green_ = !self->tmpl_is_green_;
    // set_control_template() 写入 TemplateProperty Local 槽，
    // on_template_dp_changed 清除旧模板根 → 下次 measure 时按新模板重建视觉树
    // → on_apply_template() 重新 find_template_child("content") 绑定同名 Part
    self->btn_tmpl_.set_control_template(
        self->tmpl_is_green_ ? &self->tmpl_green_ : &self->tmpl_orange_);
    self->render();
}

} // namespace app
