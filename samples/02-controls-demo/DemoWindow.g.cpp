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
    set_size({ 800.0f, 1100.0f });
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

    // ── 5a. "计数 +1" 主操作按钮（蓝色）──────────────────────────────────────
    btn_count_.set_text("计数 +1");
    btn_count_.set_padding(math::Thickness{ 12.0f, 8.0f, 12.0f, 8.0f });
    btn_count_.set_foreground(paint::Brush::solid_rgb(0xFFFFFF));
    btn_count_.set_background(paint::Brush::solid_rgb(0x1976D2));
    btn_count_.set_margin(math::Thickness{ 0.0f, 0.0f, 10.0f, 0.0f });
    if (font) { btn_count_.set_font_face(font); }
    btn_count_.add_handler(ui::Button::ClickEvent(), &DemoWindowBase::s_on_click_count, this);
    btn_row_.add_child(&btn_count_);

    // ── 5b. "重  置" 辅助按钮（蓝灰色）──────────────────────────────────────
    btn_reset_.set_text("重  置");
    btn_reset_.set_padding(math::Thickness{ 12.0f, 8.0f, 12.0f, 8.0f });
    btn_reset_.set_foreground(paint::Brush::solid_rgb(0xFFFFFF));
    btn_reset_.set_background(paint::Brush::solid_rgb(0x455A64));
    btn_reset_.set_margin(math::Thickness{ 0.0f, 0.0f, 10.0f, 0.0f });
    if (font) { btn_reset_.set_font_face(font); }
    btn_reset_.add_handler(ui::Button::ClickEvent(), &DemoWindowBase::s_on_click_reset, this);
    btn_row_.add_child(&btn_reset_);

    // ── 5c. "退  出" 危险操作按钮（红色）────────────────────────────────────
    btn_quit_.set_text("退  出");
    btn_quit_.set_padding(math::Thickness{ 12.0f, 8.0f, 12.0f, 8.0f });
    btn_quit_.set_foreground(paint::Brush::solid_rgb(0xFFFFFF));
    btn_quit_.set_background(paint::Brush::solid_rgb(0xC62828));
    if (font) { btn_quit_.set_font_face(font); }
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

    // ── 8. TextBlock 功能演示区 ─────────────────────────────────────────────────

    // 区域分隔标题
    tb_section_.set_text("\u2500\u2500 TextBlock \u6392\u7248\u529f\u80fd\u6f14\u793a \u2500\u2500");
    tb_section_.set_font_size(11.0f);
    tb_section_.set_foreground(paint::Brush::solid_rgb(0x757575));
    tb_section_.set_background(paint::Brush::solid_rgb(0xF0F0F0));
    tb_section_.set_padding(math::Thickness{ 16.0f, 6.0f, 16.0f, 6.0f });
    tb_section_.set_margin(math::Thickness{ 0.0f, 16.0f, 0.0f, 0.0f });
    if (font) { tb_section_.set_font_face(font); }
    body_panel_.add_child(&tb_section_);

    
    // 8a. 自动换行（Col 0）
    tb_label_wrap_.set_text("自动换行  Wrap");
    tb_label_wrap_.set_font_size(10.0f);
    tb_label_wrap_.set_foreground(paint::Brush::solid_rgb(0x546E7A));
    tb_label_wrap_.set_background(paint::Brush::solid(math::Color::Transparent));
    tb_label_wrap_.set_padding(math::Thickness{ 4.0f, 8.0f, 4.0f, 2.0f });
    if (font) { tb_label_wrap_.set_font_face(font); }
    body_panel_.add_child(&tb_label_wrap_);

    tb_wrap_.set_text(
        "MineFramework \u662f\u4e00\u4e2a\u9ad8\u6027\u80fd C++ UI \u6846\u67b6\uff0c"
        "\u652f\u6301\u81ea\u52a8\u6362\u884c\u3001\u5b57\u7b26\u95f4\u8ddd\u3001"
        "\u884c\u9ad8\u8986\u76d6\u7b49\u5b8c\u6574\u6392\u7248\u529f\u80fd\u3002");
    tb_wrap_.set_font_size(12.0f);
    tb_wrap_.set_foreground(paint::Brush::solid_rgb(0x212121));
    tb_wrap_.set_background(paint::Brush::solid_rgb(0xF5F5F5));
    tb_wrap_.set_padding(math::Thickness{ 6.0f, 6.0f, 6.0f, 6.0f });
    tb_wrap_.set_text_wrapping(ui::TextWrapping::Wrap);
    if (font) { tb_wrap_.set_font_face(font); }
    body_panel_.add_child(&tb_label_wrap_);
    body_panel_.add_child(&tb_wrap_);

    // 8b. 省略号裁剪（Col 1）
    tb_label_ellipsis_.set_text("省略号  MaxLines=2");
    tb_label_ellipsis_.set_font_size(10.0f);
    tb_label_ellipsis_.set_foreground(paint::Brush::solid_rgb(0x546E7A));
    tb_label_ellipsis_.set_background(paint::Brush::solid(math::Color::Transparent));
    tb_label_ellipsis_.set_padding(math::Thickness{ 4.0f, 8.0f, 4.0f, 2.0f });
    if (font) { tb_label_ellipsis_.set_font_face(font); }
    ui::Grid::set_row(tb_label_ellipsis_, 0);
    ui::Grid::set_column(tb_label_ellipsis_, 1);
    body_panel_.add_child(&tb_label_ellipsis_);

    tb_ellipsis_.set_text(
        "\u7b2c\u4e00\u884c\uff1a\u8fd9\u662f\u4e00\u6bb5\u8d85\u957f\u7684\u6587\u5b57\uff0c"
        "\u7528\u6765\u6f14\u793a\u5f53\u5185\u5bb9\u8d85\u51fa\u53ef\u7528\u5bbd\u5ea6\u65f6"
        "\u4f1a\u81ea\u52a8\u8ffd\u52a0\u7701\u7565\u53f7\u88c1\u526a\u6548\u679c\u3002\n"
        "\u7b2c\u4e8c\u884c\uff1a\u540c\u6837\u662f\u4e00\u6bb5\u8d85\u957f\u7684\u5185\u5bb9\uff0c"
        "MaxLines=2 \u9650\u5236\u6700\u591a\u663e\u793a\u4e24\u884c\u3002\n"
        "\u7b2c\u4e09\u884c\uff1a\u8fd9\u884c\u5c06\u4e0d\u4f1a\u88ab\u663e\u793a\u3002");
    tb_ellipsis_.set_font_size(12.0f);
    tb_ellipsis_.set_foreground(paint::Brush::solid_rgb(0x212121));
    tb_ellipsis_.set_background(paint::Brush::solid_rgb(0xFFFDE7));
    tb_ellipsis_.set_padding(math::Thickness{ 6.0f, 6.0f, 6.0f, 6.0f });
    tb_ellipsis_.set_text_wrapping(ui::TextWrapping::Wrap);
    tb_ellipsis_.set_text_trimming(ui::TextTrimming::CharacterEllipsis);
    tb_ellipsis_.set_max_lines(2);
    if (font) { tb_ellipsis_.set_font_face(font); }
    body_panel_.add_child(&tb_ellipsis_);


    // ── Grid 2：文字对齐 L/C/R（2行×3列，标签跨3列）─────────────────────────────
    // Row 0（auto）：标签（colspan=3）；Row 1（*）：三列内容
    tb_align_grid_.add_row(ui::RowDefinition{ ui::GridLength::auto_() });
    tb_align_grid_.add_row(ui::RowDefinition{ ui::GridLength::star() });
    tb_align_grid_.add_column(ui::ColumnDefinition{ ui::GridLength::star() });
    tb_align_grid_.add_column(ui::ColumnDefinition{ ui::GridLength::star() });
    tb_align_grid_.add_column(ui::ColumnDefinition{ ui::GridLength::star() });
    tb_align_grid_.set_margin(math::Thickness{ 16.0f, 8.0f, 16.0f, 0.0f });

    // 8c. 对齐标签（Row 0，跨3列）
    tb_label_align_.set_text("\u5bf9\u9f50  L / C / R");
    tb_label_align_.set_font_size(10.0f);
    tb_label_align_.set_foreground(paint::Brush::solid_rgb(0x546E7A));
    tb_label_align_.set_background(paint::Brush::solid(math::Color::Transparent));
    tb_label_align_.set_padding(math::Thickness{ 4.0f, 8.0f, 4.0f, 2.0f });
    if (font) { tb_label_align_.set_font_face(font); }
    ui::Grid::set_row(tb_label_align_, 0);
    ui::Grid::set_column(tb_label_align_, 0);
    ui::Grid::set_column_span(tb_label_align_, 3);
    tb_align_grid_.add_child(&tb_label_align_);

    // 8c. 三列内容（Row 1）
    tb_align_left_.set_text("\u5de6\u5bf9\u9f50\nLeft");
    tb_align_left_.set_font_size(12.0f);
    tb_align_left_.set_foreground(paint::Brush::solid_rgb(0xFFFFFF));
    tb_align_left_.set_background(paint::Brush::solid_rgb(0x1565C0));
    tb_align_left_.set_padding(math::Thickness{ 6.0f, 6.0f, 6.0f, 6.0f });
    tb_align_left_.set_text_alignment(ui::TextAlignment::Left);
    if (font) { tb_align_left_.set_font_face(font); }
    ui::Grid::set_row(tb_align_left_, 1);
    ui::Grid::set_column(tb_align_left_, 0);
    tb_align_grid_.add_child(&tb_align_left_);

    tb_align_center_.set_text("\u5c45\u4e2d\u5bf9\u9f50\nCenter");
    tb_align_center_.set_font_size(12.0f);
    tb_align_center_.set_foreground(paint::Brush::solid_rgb(0xFFFFFF));
    tb_align_center_.set_background(paint::Brush::solid_rgb(0x2E7D32));
    tb_align_center_.set_padding(math::Thickness{ 6.0f, 6.0f, 6.0f, 6.0f });
    tb_align_center_.set_text_alignment(ui::TextAlignment::Center);
    if (font) { tb_align_center_.set_font_face(font); }
    ui::Grid::set_row(tb_align_center_, 1);
    ui::Grid::set_column(tb_align_center_, 1);
    tb_align_grid_.add_child(&tb_align_center_);

    tb_align_right_.set_text("\u53f3\u5bf9\u9f50\nRight");
    tb_align_right_.set_font_size(12.0f);
    tb_align_right_.set_foreground(paint::Brush::solid_rgb(0xFFFFFF));
    tb_align_right_.set_background(paint::Brush::solid_rgb(0x6A1B9A));
    tb_align_right_.set_padding(math::Thickness{ 6.0f, 6.0f, 6.0f, 6.0f });
    tb_align_right_.set_text_alignment(ui::TextAlignment::Right);
    if (font) { tb_align_right_.set_font_face(font); }
    ui::Grid::set_row(tb_align_right_, 1);
    ui::Grid::set_column(tb_align_right_, 2);
    tb_align_grid_.add_child(&tb_align_right_);

    body_panel_.add_child(&tb_align_grid_);

    // 8d. 字符间距标签（Row 0，Col 0）
    tb_label_spacing_.set_text("\u5b57\u7b26\u95f4\u8ddd  0 / 6 / 12px");
    tb_label_spacing_.set_font_size(10.0f);
    tb_label_spacing_.set_foreground(paint::Brush::solid_rgb(0x546E7A));
    tb_label_spacing_.set_background(paint::Brush::solid(math::Color::Transparent));
    tb_label_spacing_.set_padding(math::Thickness{ 4.0f, 8.0f, 4.0f, 2.0f });
    if (font) { tb_label_spacing_.set_font_face(font); }

    body_panel_.add_child(&tb_label_spacing_);

    // 8d. 字间距子 Grid（1行×3列：0px / 6px / 12px），放入 Grid3 Row 1 Col 0
    tb_spacing_grid_.add_column(ui::ColumnDefinition{ ui::GridLength::star() });
    tb_spacing_grid_.add_column(ui::ColumnDefinition{ ui::GridLength::star() });
    tb_spacing_grid_.add_column(ui::ColumnDefinition{ ui::GridLength::star() });
    tb_spacing_grid_.set_margin(math::Thickness{ 16.0f, 8.0f, 16.0f, 0.0f });
    const char* const kSpacingText =
        "汉字排版演示\n0123456789";

    tb_spacing_norm_.set_text(kSpacingText);
    tb_spacing_norm_.set_font_size(13.0f);
    tb_spacing_norm_.set_foreground(paint::Brush::solid_rgb(0x212121));
    tb_spacing_norm_.set_background(paint::Brush::solid_rgb(0xE3F2FD));
    tb_spacing_norm_.set_padding(math::Thickness{ 8.0f, 8.0f, 8.0f, 8.0f });
    tb_spacing_norm_.set_character_spacing(0.0f);
    if (font) { tb_spacing_norm_.set_font_face(font); }
    ui::Grid::set_column(tb_spacing_norm_, 0);
    tb_spacing_grid_.add_child(&tb_spacing_norm_);

    tb_spacing_2px_.set_text(kSpacingText);
    tb_spacing_2px_.set_font_size(13.0f);
    tb_spacing_2px_.set_foreground(paint::Brush::solid_rgb(0x212121));
    tb_spacing_2px_.set_background(paint::Brush::solid_rgb(0xE8F5E9));
    tb_spacing_2px_.set_padding(math::Thickness{ 8.0f, 8.0f, 8.0f, 8.0f });
    tb_spacing_2px_.set_character_spacing(6.0f);
    if (font) { tb_spacing_2px_.set_font_face(font); }
    ui::Grid::set_column(tb_spacing_2px_, 1);
    tb_spacing_grid_.add_child(&tb_spacing_2px_);

    tb_spacing_wide_.set_text(kSpacingText);
    tb_spacing_wide_.set_font_size(13.0f);
    tb_spacing_wide_.set_foreground(paint::Brush::solid_rgb(0x212121));
    tb_spacing_wide_.set_background(paint::Brush::solid_rgb(0xFCE4EC));
    tb_spacing_wide_.set_padding(math::Thickness{ 8.0f, 8.0f, 8.0f, 8.0f });
    tb_spacing_wide_.set_character_spacing(12.0f);
    if (font) { tb_spacing_wide_.set_font_face(font); }
    ui::Grid::set_column(tb_spacing_wide_, 2);
    tb_spacing_grid_.add_child(&tb_spacing_wide_);

    ui::Grid::set_row(tb_spacing_grid_, 1);
    ui::Grid::set_column(tb_spacing_grid_, 0);
    body_panel_.add_child(&tb_spacing_grid_);

    // 8e. 行距标签（Row 0，Col 1）
    tb_label_lineh_.set_text("\u884c\u8ddd  \u9ed8\u8ba4 vs 28px");
    tb_label_lineh_.set_font_size(10.0f);
    tb_label_lineh_.set_foreground(paint::Brush::solid_rgb(0x546E7A));
    tb_label_lineh_.set_background(paint::Brush::solid(math::Color::Transparent));
    tb_label_lineh_.set_padding(math::Thickness{ 4.0f, 8.0f, 4.0f, 2.0f });
    if (font) { tb_label_lineh_.set_font_face(font); }
    body_panel_.add_child(&tb_label_lineh_);
    // 8e. 行距子 Grid（1行×2列：默认行高 / 28px），放入 Grid3 Row 1 Col 1
    tb_lineh_grid_.add_column(ui::ColumnDefinition{ ui::GridLength::star() });
    tb_lineh_grid_.add_column(ui::ColumnDefinition{ ui::GridLength::star() });
tb_lineh_grid_.set_margin(math::Thickness{ 16.0f, 8.0f, 16.0f, 0.0f });
    const char* const kLineHText =
        "第一行文字\n第二行文字\n第三行文字\n第四行文字";

    tb_lineh_default_.set_text(kLineHText);
    tb_lineh_default_.set_font_size(12.0f);
    tb_lineh_default_.set_foreground(paint::Brush::solid_rgb(0x212121));
    tb_lineh_default_.set_background(paint::Brush::solid_rgb(0xECEFF1));
    tb_lineh_default_.set_padding(math::Thickness{ 8.0f, 8.0f, 8.0f, 8.0f });
    if (font) { tb_lineh_default_.set_font_face(font); }
    ui::Grid::set_column(tb_lineh_default_, 0);
    tb_lineh_grid_.add_child(&tb_lineh_default_);

    tb_lineh_.set_text(kLineHText);
    tb_lineh_.set_font_size(12.0f);
    tb_lineh_.set_foreground(paint::Brush::solid_rgb(0x212121));
    tb_lineh_.set_background(paint::Brush::solid_rgb(0xF3E5F5));
    tb_lineh_.set_padding(math::Thickness{ 8.0f, 8.0f, 8.0f, 8.0f });
    tb_lineh_.set_line_height(28.0f);
    if (font) { tb_lineh_.set_font_face(font); }
    ui::Grid::set_column(tb_lineh_, 1);
    tb_lineh_grid_.add_child(&tb_lineh_);

    body_panel_.add_child(&tb_lineh_grid_);

    // ── 9. TextBox 输入控件演示区 ──────────────────────────────────────────────────

    // 区域分隔标题
    textbox_section_.set_text("── TextBox 输入控件演示 ──");
    textbox_section_.set_font_size(11.0f);
    textbox_section_.set_foreground(paint::Brush::solid_rgb(0x757575));
    textbox_section_.set_background(paint::Brush::solid_rgb(0xF0F0F0));
    textbox_section_.set_padding(math::Thickness{ 16.0f, 6.0f, 16.0f, 6.0f });
    textbox_section_.set_margin(math::Thickness{ 0.0f, 16.0f, 0.0f, 0.0f });
    if (font) { textbox_section_.set_font_face(font); }
    body_panel_.add_child(&textbox_section_);

    // 操作提示说明
    textbox_hint_.set_text(
        "单击输入框获得焦点，可输入文字、"
        "Left/Right/Home/End 移动光标、"
        "Backspace/Delete 删除字符。光标 500ms 闪烁。");
    textbox_hint_.set_font_size(11.0f);
    textbox_hint_.set_foreground(paint::Brush::solid_rgb(0x757575));
    textbox_hint_.set_background(paint::Brush::solid(math::Color::Transparent));
    textbox_hint_.set_padding(math::Thickness{ 4.0f, 4.0f, 4.0f, 4.0f });
    textbox_hint_.set_margin(math::Thickness{ 16.0f, 4.0f, 16.0f, 0.0f });
    textbox_hint_.set_text_wrapping(ui::TextWrapping::Wrap);
    if (font) { textbox_hint_.set_font_face(font); }
    body_panel_.add_child(&textbox_hint_);

    // 普通输入框标签
    textbox_label_input_.set_text("普通输入框（点击后可输入）");
    textbox_label_input_.set_font_size(10.0f);
    textbox_label_input_.set_foreground(paint::Brush::solid_rgb(0x546E7A));
    textbox_label_input_.set_background(paint::Brush::solid(math::Color::Transparent));
    textbox_label_input_.set_padding(math::Thickness{ 4.0f, 8.0f, 4.0f, 2.0f });
    if (font) { textbox_label_input_.set_font_face(font); }
    body_panel_.add_child(&textbox_label_input_);

    // 普通输入框（带 Placeholder）
    textbox_input_.set_placeholder("请在此输入文字…");
    textbox_input_.set_margin(math::Thickness{ 16.0f, 0.0f, 16.0f, 0.0f });
    if (font) { textbox_input_.set_font_face(font); }
    body_panel_.add_child(&textbox_input_);

    // 只读输入框标签
    textbox_label_ro_.set_text("只读模式（IsReadOnly = true）");
    textbox_label_ro_.set_font_size(10.0f);
    textbox_label_ro_.set_foreground(paint::Brush::solid_rgb(0x546E7A));
    textbox_label_ro_.set_background(paint::Brush::solid(math::Color::Transparent));
    textbox_label_ro_.set_padding(math::Thickness{ 4.0f, 8.0f, 4.0f, 2.0f });
    if (font) { textbox_label_ro_.set_font_face(font); }
    body_panel_.add_child(&textbox_label_ro_);

    // 只读输入框（展示只读状态）
    textbox_readonly_.set_text("此内容不可编辑（只读模式）");
    textbox_readonly_.set_read_only(true);
    textbox_readonly_.set_margin(math::Thickness{ 16.0f, 0.0f, 16.0f, 16.0f });
    if (font) { textbox_readonly_.set_font_face(font); }
    body_panel_.add_child(&textbox_readonly_);
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

} // namespace app
