#pragma once
#include <mine/ui/controls/Api.h>
#include <mine/ui/visual/Control.h>
#include <mine/ui/event/RoutedEvent.h>
#include <mine/ui/property/DependencyProperty.h>
#include <mine/paint/Brush.h>
#include <mine/math/Rect.h>
#include <mine/containers/InlineString.h>
#include <mine/core/Memory.h>

namespace mine::paint { class Canvas; }
namespace mine::ui::input { class MouseEventArgs; }
namespace mine::ui::style { class Style; }

namespace mine::ui {

// 前向声明
class Border;
class StackPanel;
class TextBlock;

/**
 * @brief MD3 风格 CheckBox 勾选框控件（组合式架构 + VSM 状态机）。
 *
 * 视觉树（组合式装配，自内向外）：
 *   CheckBox
 *   └── StackPanel (Horizontal)
 *       ├── Border (icon_border_: 图标方框背景+边框+2px圆角)
 *       │   └── Border (state_border_: hover/press 半透明白蒙版)
 *       │       └── CheckMarkElement (勾号填充路径, 勾选时可见)
 *       └── TextBlock (label_: 伴随文字)
 *
 * 布局由 StackPanel 标准布局面板驱动，无需手动 on_arrange。
 *
 * 状态机驱动（VSM）：
 *   - Normal: 图标透明底+灰边框+无勾号
 *   - Hovered: 图标主色边框+8%白色蒙版（120ms 缓动）
 *   - Pressed: 图标主色边框+12%白色蒙版（60ms 缓动）
 *   - 勾选态通过 IsCheckedProperty 变更回调直接写入图标背景/边框/勾号色
 *     （Local P50 覆盖 StyleTrigger P30），取消勾选时 clear 让样式恢复。
 */
class MINE_UI_CONTROLS_API CheckBox : public Control {
public:
    // ── 路由事件 ───────────────────────────────────────────────────────────

    static const RoutedEvent& CheckedChangedEvent();

    // ── 依赖属性 ───────────────────────────────────────────────────────────

    /// 勾选状态（bool，默认 false）
    static const DependencyProperty& IsCheckedProperty;

    /// 图标背景画刷（勾选时 MD3 Primary #6750A4，未勾选时透明）
    static const DependencyProperty& IconBackgroundProperty;

    /// 图标边框画刷（勾选/悬停时 Primary，默认 #79747E）
    static const DependencyProperty& IconBorderBrushProperty;

    /// State Layer 蒙版画刷（hover 8%/press 12% 白，默认全透明）
    static const DependencyProperty& StateLayerBrushProperty;

    /// 勾号画刷（勾选时白色，未勾选时透明）
    static const DependencyProperty& CheckMarkBrushProperty;

    /// 文字前景画刷（默认 #1C1B1F）
    static const DependencyProperty& TextForegroundProperty;

    // ── 生命周期 ───────────────────────────────────────────────────────────

    CheckBox();
    ~CheckBox() override;

    CheckBox(const CheckBox&)            = delete;
    CheckBox& operator=(const CheckBox&) = delete;

    // ── 属性访问 ───────────────────────────────────────────────────────────

    [[nodiscard]] bool is_checked() const noexcept;
    void set_checked(bool checked);

    [[nodiscard]] core::StringView text() const noexcept;
    void set_text(core::StringView text);

    void set_font_face(void* font_face) noexcept;
    void set_font_size(float size_px) noexcept;

    // ── 鼠标事件路由桩 ─────────────────────────────────────────────────────

    static void on_mouse_enter_router(void*, RoutedEventArgs&, void*);
    static void on_mouse_leave_router(void*, RoutedEventArgs&, void*);
    static void on_mouse_down_router(void*, RoutedEventArgs&, void*);
    static void on_mouse_up_router  (void*, RoutedEventArgs&, void*);

protected:
    void on_measure(math::Size available) override;
    [[nodiscard]] ControlVisualState compute_visual_state() const noexcept override;

private:
    // ── 勾号元素（私有嵌套类，仅负责绘制 MD3 check 图标填充路径）─────────

    class CheckMarkElement;

    // ── IsChecked 变更回调 ─────────────────────────────────────────────────

    static void on_is_checked_changed(DependencyObject*         sender,
                                      const DependencyProperty& prop,
                                      const core::Variant&      old_v,
                                      const core::Variant&      new_v) noexcept;

    // ── 成员 ───────────────────────────────────────────────────────────────

    containers::InlineString text_;
    bool                     is_hovered_ = false;
    bool                     is_pressed_ = false;
    void*                    font_face_  = nullptr;
    float                    font_size_  = 14.0f;

    StackPanel*       layout_root_  = nullptr;  ///< 水平布局根
    Border*           icon_border_  = nullptr;  ///< 图标方框 Border
    Border*           state_border_ = nullptr;  ///< State Layer 蒙版 Border
    CheckMarkElement* check_mark_   = nullptr;  ///< 勾号元素
    TextBlock*        label_        = nullptr;  ///< 文字标签

    // 子元素所有权（生命周期由 OwnedPtr 管理，layout_root_ 由 set_inner_element 接管）
    core::OwnedPtr<StackPanel>       owned_root_;
    core::OwnedPtr<Border>           owned_icon_;
    core::OwnedPtr<Border>           owned_state_;
    core::OwnedPtr<CheckMarkElement> owned_check_;
    core::OwnedPtr<TextBlock>        owned_label_;
};

} // namespace mine::ui
