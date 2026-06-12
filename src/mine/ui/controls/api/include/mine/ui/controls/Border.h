/**
 * @file Border.h
 * @brief Border —— 单子元素边框容器（合格基元控件，外观完全由依赖属性驱动）。
 *
 * Border 是组合式外观架构（docs/22）中的核心基元控件：所有外观参数
 * （背景画刷 / 边框画刷 / 边框粗细 / 圆角半径）均声明为 DependencyProperty，
 * on_render 完全从 DP 读取值绘制。复合控件（Button/TextBox 等）可通过
 * bind_property 把宿主外观属性单向同步到 Border，从而把外框绘制从控件自身
 * 的 on_render 下沉到 Border，实现外观与逻辑分离。
 */

#pragma once

#include <mine/ui/controls/Api.h>
#include <mine/ui/controls/Primitive.h>
#include <mine/core/Memory.h>
#include <mine/math/Thickness.h>
#include <mine/math/CornerRadii.h>
#include <mine/paint/Brush.h>

namespace mine::paint { class Canvas; }

namespace mine::ui {

/**
 * @brief 边框容器控件（合格基元）。
 *
 * Border 持有一个可选子元素，负责：
 * 1. 从依赖属性读取背景、边框画刷、边框粗细、圆角半径并绘制
 * 2. 在内边框矩形中排列子元素
 *
 * 所有视觉属性均为 DependencyProperty（单一真相源），可被样式、动画、
 * bind_property 绑定统一驱动。
 */
class MINE_UI_CONTROLS_API Border : public Primitive {
public:
    // ── 依赖属性（外观真相源）────────────────────────────────────────────

    /// 背景画刷（Brush，默认透明）
    static const DependencyProperty& BackgroundProperty;
    /// 边框画刷（Brush，默认 #808080）
    static const DependencyProperty& BorderBrushProperty;
    /// 边框粗细（Thickness，默认四边 1dp）
    static const DependencyProperty& BorderThicknessProperty;
    /// 圆角半径（CornerRadii，默认四角直角 0；>0 时启用圆角描边与圆角背景）
    static const DependencyProperty& CornerRadiusProperty;

    // ── 生命周期 ──────────────────────────────────────────────────────────

    Border();
    ~Border() override;

    Border(const Border&)            = delete;
    Border& operator=(const Border&) = delete;
    Border(Border&&)                 = default;
    Border& operator=(Border&&)      = default;

    // ── 子元素 ────────────────────────────────────────────────────────────

    [[nodiscard]] UIElement* child() const noexcept;
    /// 设置子元素（裸指针，Border 不拥有所有权）
    void set_child(UIElement* child);
    /// 设置子元素并转移所有权（动态创建子元素时使用）
    void set_child(core::OwnedPtr<UIElement> child);

    // ── 外观访问器（读写 DependencyProperty）──────────────────────────────

    [[nodiscard]] math::Thickness border_thickness() const noexcept;
    void set_border_thickness(math::Thickness thickness);

    [[nodiscard]] paint::Brush border_brush() const noexcept;
    void set_border_brush(paint::Brush brush);

    [[nodiscard]] paint::Brush background() const noexcept;
    void set_background(paint::Brush brush);

    [[nodiscard]] math::CornerRadii corner_radius() const noexcept;
    void set_corner_radius(math::CornerRadii radii);

    // ── 兼容别名（旧 API border_color → border_brush）──────────────────────

    /// @deprecated 等价于 border_brush()，保留以兼容既有调用
    [[nodiscard]] paint::Brush border_color() const noexcept { return border_brush(); }
    /// @deprecated 等价于 set_border_brush()，保留以兼容既有调用
    void set_border_color(paint::Brush brush) { set_border_brush(brush); }

protected:
    void on_measure(math::Size available_size) override;
    void on_arrange(math::Rect final_rect) override;
    void on_render(paint::Canvas& canvas) override;

private:
    UIElement*                child_ = nullptr;
    core::OwnedPtr<UIElement>  owned_child_;  ///< 可选：动态创建子元素时的所有权
};

} // namespace mine::ui
