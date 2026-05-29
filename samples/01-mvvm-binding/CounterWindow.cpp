/**
 * @file CounterWindow.cpp
 * @brief CounterWindow —— MVVM 计数器演示窗口实现。
 *
 * 实现分为两部分：
 *   1. build_()：视觉树构建（控件布局、外观配置、事件注册）
 *   2. bind_()：数据绑定激活（BindingExpression 配置与 attach）
 *
 * 关键路径（点击 [+1]）：
 *   s_on_click_inc → vm_.increment_cmd_.execute({})
 *     → CounterViewModel::do_increment_()
 *       → set_count(count+1)              [MINE_OBSERVABLE 自动发出 "count" 通知]
 *       → update_display_()
 *         → set_count_text(new_str)        [发出 "count_text" 通知]
 *           → BindingExpression 回调
 *             → count_bind_.re_evaluate()
 *               → count_label_.set_value(TextProperty, new_variant, TemplateBind)
 *                 → TextBlock::on_text_changed() → 更新内部文字字段
 *         → set_hint_text(new_hint)        [发出 "hint_text" 通知 → 同上路径]
 *     → render()                           [重绘窗口，将新文字绘制到屏幕]
 */

#include "CounterWindow.h"

// ── 命名空间别名 ──────────────────────────────────────────────────────────────
namespace core  = mine::core;
namespace math  = mine::math;
namespace paint = mine::paint;
namespace ui    = mine::ui;

namespace app {

// ── 构造 / 析构 ───────────────────────────────────────────────────────────────

CounterWindow::CounterWindow()
{
    // 配置窗口标题和初始尺寸（在 show() 时应用到原生窗口）
    set_title("MineFramework — MVVM + 数据绑定演示");
    set_size({ 700.0f, 460.0f });
}

CounterWindow::~CounterWindow()
{
    // 先关闭窗口（停止渲染循环，释放 swapchain），再析构数据成员
    if (!is_closed()) close();
}

// ── 资源注入 + UI 树构建 ──────────────────────────────────────────────────────

void CounterWindow::setup(mine::text::FontFace* font)
{
    build_(font);   // 步骤 1：构建视觉树
    bind_();        // 步骤 2：激活数据绑定
}

// ── 视觉树构建 ────────────────────────────────────────────────────────────────

void CounterWindow::build_(mine::text::FontFace* font)
{
    // ── 根面板：垂直 StackPanel ─────────────────────────────────────────────────

    body_panel_.set_orientation(ui::Orientation::Vertical);

    // ── 标题栏 ─────────────────────────────────────────────────────────────────

    header_label_.set_text("MVVM + 数据绑定演示 — MineFramework");
    header_label_.set_font_size(18.0f);
    header_label_.set_foreground(paint::Brush::solid_rgb(0xFFFFFF));
    header_label_.set_background(paint::Brush::solid_rgb(0x1A237E));   // 深靛蓝
    header_label_.set_padding(math::Thickness{ 20.0f, 14.0f, 20.0f, 14.0f });
    if (font) { header_label_.set_font_face(font); }
    body_panel_.add_child(&header_label_);

    // ── 主计数显示（绑定到 vm_.count_text）────────────────────────────────────
    //
    // 初始文字将在 bind_() 中由 BindingExpression 首次求值写入，
    // 此处设置 fallback 风格（深色背景区域）
    //
    count_label_.set_text("当前计数：0");                               // 初始文字（bind 后被覆盖）
    count_label_.set_font_size(42.0f);
    count_label_.set_foreground(paint::Brush::solid_rgb(0xE8EAF6));    // 浅靛蓝文字
    count_label_.set_background(paint::Brush::solid_rgb(0x283593));    // 靛蓝背景
    count_label_.set_padding(math::Thickness{ 20.0f, 28.0f, 20.0f, 28.0f });
    count_label_.set_margin(math::Thickness{ 0.0f, 0.0f, 0.0f, 0.0f });
    if (font) { count_label_.set_font_face(font); }
    body_panel_.add_child(&count_label_);

    // ── 提示文字（绑定到 vm_.hint_text）──────────────────────────────────────

    hint_label_.set_text("点击下方按钮改变计数");                        // 初始文字（bind 后被覆盖）
    hint_label_.set_font_size(13.0f);
    hint_label_.set_foreground(paint::Brush::solid_rgb(0x9FA8DA));     // 灰蓝色
    hint_label_.set_background(paint::Brush::solid_rgb(0x283593));    // 与计数背景一致
    hint_label_.set_padding(math::Thickness{ 20.0f, 0.0f, 20.0f, 16.0f });
    if (font) { hint_label_.set_font_face(font); }
    body_panel_.add_child(&hint_label_);

    // ── 按钮行 ────────────────────────────────────────────────────────────────

    btn_row_.set_orientation(ui::Orientation::Horizontal);
    btn_row_.set_margin(math::Thickness{ 20.0f, 20.0f, 20.0f, 0.0f });
    body_panel_.add_child(&btn_row_);

    // [+1] 蓝色按钮
    btn_inc_.set_text("+1  计数");
    btn_inc_.set_font_size(15.0f);
    btn_inc_.set_padding(math::Thickness{ 20.0f, 12.0f, 20.0f, 12.0f });
    btn_inc_.set_foreground(paint::Brush::solid_rgb(0xFFFFFF));
    btn_inc_.set_background(paint::Brush::solid_rgb(0x1565C0));
    btn_inc_.set_background_hovered(paint::Brush::solid_rgb(0x1976D2));
    btn_inc_.set_background_pressed(paint::Brush::solid_rgb(0x0D47A1));
    btn_inc_.set_border_color(paint::Brush::solid_rgb(0x0D47A1));
    btn_inc_.set_margin(math::Thickness{ 0.0f, 0.0f, 12.0f, 0.0f });
    if (font) { btn_inc_.set_font_face(font); }
    btn_inc_.add_handler(ui::Button::ClickEvent(), &CounterWindow::s_on_click_inc, this);
    btn_row_.add_child(&btn_inc_);

    // [-1] 灰色按钮
    btn_dec_.set_text("-1  计数");
    btn_dec_.set_font_size(15.0f);
    btn_dec_.set_padding(math::Thickness{ 20.0f, 12.0f, 20.0f, 12.0f });
    btn_dec_.set_foreground(paint::Brush::solid_rgb(0xFFFFFF));
    btn_dec_.set_background(paint::Brush::solid_rgb(0x455A64));
    btn_dec_.set_background_hovered(paint::Brush::solid_rgb(0x546E7A));
    btn_dec_.set_background_pressed(paint::Brush::solid_rgb(0x263238));
    btn_dec_.set_border_color(paint::Brush::solid_rgb(0x263238));
    btn_dec_.set_margin(math::Thickness{ 0.0f, 0.0f, 12.0f, 0.0f });
    if (font) { btn_dec_.set_font_face(font); }
    btn_dec_.add_handler(ui::Button::ClickEvent(), &CounterWindow::s_on_click_dec, this);
    btn_row_.add_child(&btn_dec_);

    // [重置] 橙色按钮
    btn_reset_.set_text("重  置");
    btn_reset_.set_font_size(15.0f);
    btn_reset_.set_padding(math::Thickness{ 20.0f, 12.0f, 20.0f, 12.0f });
    btn_reset_.set_foreground(paint::Brush::solid_rgb(0xFFFFFF));
    btn_reset_.set_background(paint::Brush::solid_rgb(0xE65100));
    btn_reset_.set_background_hovered(paint::Brush::solid_rgb(0xF57C00));
    btn_reset_.set_background_pressed(paint::Brush::solid_rgb(0xBF360C));
    btn_reset_.set_border_color(paint::Brush::solid_rgb(0xBF360C));
    btn_reset_.set_margin(math::Thickness{ 0.0f, 0.0f, 12.0f, 0.0f });
    if (font) { btn_reset_.set_font_face(font); }
    btn_reset_.add_handler(ui::Button::ClickEvent(), &CounterWindow::s_on_click_reset, this);
    btn_row_.add_child(&btn_reset_);

    // [退出] 红色按钮
    btn_quit_.set_text("退  出");
    btn_quit_.set_font_size(15.0f);
    btn_quit_.set_padding(math::Thickness{ 20.0f, 12.0f, 20.0f, 12.0f });
    btn_quit_.set_foreground(paint::Brush::solid_rgb(0xFFFFFF));
    btn_quit_.set_background(paint::Brush::solid_rgb(0xC62828));
    btn_quit_.set_background_hovered(paint::Brush::solid_rgb(0xD32F2F));
    btn_quit_.set_background_pressed(paint::Brush::solid_rgb(0x7F0000));
    btn_quit_.set_border_color(paint::Brush::solid_rgb(0x7F0000));
    if (font) { btn_quit_.set_font_face(font); }
    btn_quit_.add_handler(ui::Button::ClickEvent(), &CounterWindow::s_on_click_quit, this);
    btn_row_.add_child(&btn_quit_);

    // ── 挂载根面板到窗口内容区 ───────────────────────────────────────────────

    set_content(&body_panel_);
}

// ── 数据绑定激活 ──────────────────────────────────────────────────────────────

void CounterWindow::bind_()
{
    // ── 绑定 1：vm_.count_text → count_label_.TextProperty ────────────────────
    //
    // getter：每次触发时从 ViewModel 读取 count_text（InlineString），
    //         包装为 core::Variant 返回给绑定引擎。
    // deps：  订阅 vm_ 的 "count_text" 属性变更（通过 INotifyPropertyChanged）。
    // 效果：  vm_.set_count_text(x) → 绑定自动更新 count_label_ 的文字。
    //
    count_bind_.getter = [this]() noexcept -> core::Variant {
        // 从 ViewModel 读取当前 count_text，复制到 Variant（InlineString 可拷贝）
        return core::Variant{ vm_.count_text() };
    };
    count_bind_.deps.push_back(
        ui::PropertyDependency::from_inpc(vm_, "count_text"));
    count_bind_.mode = ui::BindingMode::OneWay;
    // attach：立即求值一次（写入当前值），并开始订阅后续变更
    count_bind_.attach(count_label_, ui::TextBlock::TextProperty);

    // ── 绑定 2：vm_.hint_text → hint_label_.TextProperty ─────────────────────

    hint_bind_.getter = [this]() noexcept -> core::Variant {
        return core::Variant{ vm_.hint_text() };
    };
    hint_bind_.deps.push_back(
        ui::PropertyDependency::from_inpc(vm_, "hint_text"));
    hint_bind_.mode = ui::BindingMode::OneWay;
    hint_bind_.attach(hint_label_, ui::TextBlock::TextProperty);
}

// ── 事件处理：静态路由桩 ──────────────────────────────────────────────────────

void CounterWindow::s_on_click_inc(void* /*sender*/,
                                    ui::RoutedEventArgs& /*args*/,
                                    void* user_data)
{
    auto* self = static_cast<CounterWindow*>(user_data);
    // 执行命令（ViewModel 内部更新属性 → 绑定链自动同步到 UI）
    self->vm_.increment_cmd_.execute({});
    // 显式重绘（确保当前帧立即更新；未来框架层应自动调度）
    self->render();
}

void CounterWindow::s_on_click_dec(void* /*sender*/,
                                    ui::RoutedEventArgs& /*args*/,
                                    void* user_data)
{
    auto* self = static_cast<CounterWindow*>(user_data);
    self->vm_.decrement_cmd_.execute({});
    self->render();
}

void CounterWindow::s_on_click_reset(void* /*sender*/,
                                      ui::RoutedEventArgs& /*args*/,
                                      void* user_data)
{
    auto* self = static_cast<CounterWindow*>(user_data);
    self->vm_.reset_cmd_.execute({});
    self->render();
}

void CounterWindow::s_on_click_quit(void* /*sender*/,
                                     ui::RoutedEventArgs& /*args*/,
                                     void* user_data)
{
    auto* self = static_cast<CounterWindow*>(user_data);
    // 触发关闭请求信号（App 层监听后调用 quit()）
    if (self->on_close_requested_) {
        self->on_close_requested_();
    }
}

} // namespace app
