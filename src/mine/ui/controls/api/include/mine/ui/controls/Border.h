/**
 * @file Border.h
 * @brief Border —— 单子元素边框容器（M1.1 最小可用实现）。
 */

#pragma once

#include <mine/ui/controls/Api.h>
#include <mine/ui/visual/Control.h>
#include <mine/math/Color.h>
#include <mine/math/Thickness.h>

namespace mine::paint { class Canvas; }

namespace mine::ui {

/**
 * @brief 边框容器控件。
 *
 * Border 持有一个可选子元素，负责：
 * 1. 绘制背景和边框
 * 2. 在内边框矩形中排列子元素
 */
class MINE_UI_CONTROLS_API Border : public Control {
public:
    Border();
    ~Border() override;

    Border(const Border&)            = delete;
    Border& operator=(const Border&) = delete;
    Border(Border&&)                 = default;
    Border& operator=(Border&&)      = default;

    [[nodiscard]] UIElement* child() const noexcept;
    void set_child(UIElement* child);

    [[nodiscard]] math::Thickness border_thickness() const noexcept;
    void set_border_thickness(math::Thickness thickness);

    [[nodiscard]] math::Color border_color() const noexcept;
    void set_border_color(math::Color color);

    [[nodiscard]] math::Color background() const noexcept;
    void set_background(math::Color color);

protected:
    void on_measure(math::Size available_size) override;
    void on_arrange(math::Rect final_rect) override;
    void on_render(paint::Canvas& canvas) override;

private:
    UIElement*      child_            = nullptr;
    math::Thickness border_thickness_ = math::Thickness::uniform(1.0f);
    math::Color     border_color_     = math::Color::from_rgb_u32(0x808080);
    math::Color     background_       = math::Color::Transparent;
};

} // namespace mine::ui
