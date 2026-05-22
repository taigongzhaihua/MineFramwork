/**
 * @file Button.h
 * @brief Button —— 基础按钮控件（M1.1 最小可用实现）。
 */

#pragma once

#include <mine/ui/controls/Api.h>
#include <mine/ui/visual/Control.h>
#include <mine/ui/event/RoutedEvent.h>
#include <mine/ui/property/DependencyProperty.h>
#include <mine/containers/InlineString.h>
#include <mine/math/Color.h>
#include <mine/math/Thickness.h>

namespace mine::paint { class Canvas; }
namespace mine::ui::input { class MouseEventArgs; }

namespace mine::ui {

/**
 * @brief 可点击按钮控件。
 *
 * 当前阶段提供：
 * 1. 文本与基础按钮外观
 * 2. 鼠标按下/释放状态切换
 * 3. Click 路由事件
 * 4. 样式/模板/视觉状态挂点（与 mine.ui.style 对接）
 * 5. ContentProperty / PaddingProperty（DependencyProperty，供 ControlTemplate 绑定）
 */
class MINE_UI_CONTROLS_API Button : public Control {
public:
    // ── 路由事件 ───────────────────────────────────────────────────────────

    static const RoutedEvent& ClickEvent();

    // ── 依赖属性 ───────────────────────────────────────────────────────────

    /**
     * @brief 内容属性（Variant 存储 containers::InlineString，即按钮文字）。
     *
     * 与 ContentPresenter::ContentProperty 通过 bind_template 连接，
     * 使 ContentPresenter 能够在模板树中渲染按钮文字。
     */
    static const DependencyProperty& ContentProperty;

    /**
     * @brief 内边距属性（Variant 存储 math::Thickness）。
     *
     * 默认值：{12, 8, 12, 8}（水平 12px，垂直 8px）。
     */
    static const DependencyProperty& PaddingProperty;

    // ── 生命周期 ───────────────────────────────────────────────────────────

    Button();
    ~Button() override;

    Button(const Button&)            = delete;
    Button& operator=(const Button&) = delete;
    Button(Button&&)                 = default;
    Button& operator=(Button&&)      = default;

    // ── 属性访问 ───────────────────────────────────────────────────────────

    [[nodiscard]] core::StringView text() const noexcept;
    void set_text(core::StringView text);

    [[nodiscard]] bool is_enabled() const noexcept;
    void set_enabled(bool enabled) noexcept;

    [[nodiscard]] math::Thickness padding() const noexcept;
    void set_padding(math::Thickness padding);

    [[nodiscard]] math::Color foreground() const noexcept;
    void set_foreground(math::Color color);

    [[nodiscard]] math::Color background() const noexcept;
    void set_background(math::Color color);

    [[nodiscard]] math::Color background_hovered() const noexcept;
    void set_background_hovered(math::Color color);

    [[nodiscard]] math::Color background_pressed() const noexcept;
    void set_background_pressed(math::Color color);

    [[nodiscard]] math::Color border_color() const noexcept;
    void set_border_color(math::Color color);

    /**
     * @brief 设置文字渲染字体（同步传播到模板树内的 ContentPresenter）。
     * @param font_face 字体对象指针（nullptr 时 ContentPresenter 回退到占位线）
     */
    void set_font_face(void* font_face) noexcept;

    /**
     * @brief 设置文字渲染字号（逻辑像素，默认 14.0f）。
     * @param size_px 字号
     */
    void set_font_size(float size_px) noexcept;

protected:
    void on_measure(math::Size available_size) override;
    void on_render(paint::Canvas& canvas) override;
    [[nodiscard]] ControlVisualState compute_visual_state() const override;

private:
    // ── 依赖属性变更回调 ───────────────────────────────────────────────────

    /**
     * @brief ContentProperty 变更时同步 text_ 成员缓存。
     */
    static void on_content_changed(DependencyObject*         sender,
                                   const DependencyProperty& prop,
                                   const core::Variant&      old_v,
                                   const core::Variant&      new_v) noexcept;

    /**
     * @brief PaddingProperty 变更时同步 padding_ 成员缓存。
     */
    static void on_padding_changed(DependencyObject*         sender,
                                   const DependencyProperty& prop,
                                   const core::Variant&      old_v,
                                   const core::Variant&      new_v) noexcept;

    // ── 鼠标事件路由 ───────────────────────────────────────────────────────

    static void on_mouse_down_router(void* sender, RoutedEventArgs& args, void* user_data);
    static void on_mouse_up_router(void* sender, RoutedEventArgs& args, void* user_data);

    void on_mouse_down(input::MouseEventArgs& args);
    void on_mouse_up(input::MouseEventArgs& args);
    void raise_click();

    // ── 成员变量 ───────────────────────────────────────────────────────────

    containers::InlineString text_;
    bool                     is_enabled_       = true;
    bool                     is_pressed_       = false;
    math::Thickness          padding_          = math::Thickness::symmetric(12.0f, 8.0f);
    math::Color              foreground_       = math::Color::Black;
    math::Color              background_       = math::Color::from_rgb_u32(0xE6E6E6);
    math::Color              background_hover_ = math::Color::from_rgb_u32(0xDCDCDC);
    math::Color              background_press_ = math::Color::from_rgb_u32(0xC8C8C8);
    math::Color              border_color_     = math::Color::from_rgb_u32(0x707070);
    void*                    font_face_        = nullptr;
    float                    font_size_px_     = 14.0f;
};

} // namespace mine::ui
