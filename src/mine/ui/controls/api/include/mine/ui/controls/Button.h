/**
 * @file Button.h
 * @brief Button —— 基础按钮控件（M1.1 最小可用实现）。
 */

#pragma once

#include <mine/ui/controls/Api.h>
#include <mine/ui/visual/Control.h>
#include <mine/ui/event/RoutedEvent.h>
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
 */
class MINE_UI_CONTROLS_API Button : public Control {
public:
    static const RoutedEvent& ClickEvent();

    Button();
    ~Button() override;

    Button(const Button&)            = delete;
    Button& operator=(const Button&) = delete;
    Button(Button&&)                 = default;
    Button& operator=(Button&&)      = default;

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

protected:
    void on_measure(math::Size available_size) override;
    void on_render(paint::Canvas& canvas) override;
    [[nodiscard]] ControlVisualState compute_visual_state() const override;

private:
    static void on_mouse_down_router(void* sender, RoutedEventArgs& args, void* user_data);
    static void on_mouse_up_router(void* sender, RoutedEventArgs& args, void* user_data);

    void on_mouse_down(input::MouseEventArgs& args);
    void on_mouse_up(input::MouseEventArgs& args);
    void raise_click();

    containers::InlineString text_;
    bool                     is_enabled_       = true;
    bool                     is_pressed_       = false;
    math::Thickness          padding_          = math::Thickness::symmetric(12.0f, 8.0f);
    math::Color              foreground_       = math::Color::Black;
    math::Color              background_       = math::Color::from_rgb_u32(0xE6E6E6);
    math::Color              background_hover_ = math::Color::from_rgb_u32(0xDCDCDC);
    math::Color              background_press_ = math::Color::from_rgb_u32(0xC8C8C8);
    math::Color              border_color_     = math::Color::from_rgb_u32(0x707070);
};

} // namespace mine::ui
