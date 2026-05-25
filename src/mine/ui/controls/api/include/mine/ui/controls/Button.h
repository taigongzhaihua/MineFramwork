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
#include <mine/core/Memory.h>

#include <chrono>

namespace mine::paint { class Canvas; }
namespace mine::ui::input { class MouseEventArgs; }
namespace mine::ui::animation { class Storyboard; }

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
     * @brief 当前渲染背景色属性（Variant 存储 math::Color，由 Storyboard 动画驱动）。
     *
     * 视觉状态切换时，on_visual_state_changed 通过 Storyboard::animate_dp_to 在此属性
     * 上写入插值色（Animation 优先级=60），on_render 读取该属性作为填充色，从而实现
     * Normal / Hovered / Pressed 之间的平滑过渡。动画结束后值保留在 Animation 槽。
     */
    static const DependencyProperty& BackgroundProperty;

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
     * @brief 查询当前是否有需要持续渲染的动画（ripple 或背景色过渡）。
     * @return 任一动画仍在播放中则返回 true，全部结束后返回 false。
     */
    [[nodiscard]] bool has_active_animation() const noexcept;

    /**
     * @brief 推进背景色过渡动画（Storyboard::tick）。
     *
     * 由外部渲染驱动定时器（Win32 SetTimer）每帧调用。tick 内部写入 BackgroundProperty
     * 的 Animation 优先级插值色，触发 affects_render 回调，再经 ui_win_->render()
     * 刷新屏幕。
     *
     * @param dt 帧时长（秒），通常为 0.016（约 60fps）
     */
    void tick_bg_animation(float dt);

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
    void on_visual_state_changed(ControlVisualState old_state,
                                 ControlVisualState new_state) override;

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
    static void on_mouse_enter_router(void* sender, RoutedEventArgs& args, void* user_data);
    static void on_mouse_leave_router(void* sender, RoutedEventArgs& args, void* user_data);

    void on_mouse_down(input::MouseEventArgs& args);
    void on_mouse_up(input::MouseEventArgs& args);
    void on_mouse_enter();
    void on_mouse_leave();
    void raise_click();

    // ── 成员变量 ───────────────────────────────────────────────────────────

    containers::InlineString text_;
    bool                     is_enabled_       = true;
    bool                     is_hovered_       = false;  ///< 鼠标正悬停在按鈕上
    bool                     is_pressed_       = false;
    // Material Design 3 Filled Button 默认尺寸（水平 24dp，垂直 10dp）
    math::Thickness          padding_          = math::Thickness::symmetric(24.0f, 10.0f);
    // MD3 On Primary（白色文字覆盖在 Primary 背景上）
    math::Color              foreground_       = math::Color::White;
    // MD3 Primary #6750A4
    math::Color              background_       = math::Color::from_rgb_u32(0x6750A4);
    // MD3 Hovered = Primary + OnPrimary * 8%  ≈ #735BAC
    math::Color              background_hover_ = math::Color{0.452f, 0.369f, 0.672f, 1.0f};
    // MD3 Pressed = Primary + OnPrimary * 12% ≈ #7A65AF
    math::Color              background_press_ = math::Color{0.476f, 0.396f, 0.686f, 1.0f};
    // MD3 Filled Button 无外边框
    math::Color              border_color_     = math::Color::Transparent;
    void*                    font_face_        = nullptr;
    float                    font_size_px_     = 14.0f;

    /// MD3 Ripple 涟漪动画状态（鼠标按下时触发）
    struct RippleState {
        float  center_x  = 0.0f;  ///< 涟漪圆心 X（相对于 bounds_rect 左上角）
        float  center_y  = 0.0f;  ///< 涟漪圆心 Y（相对于 bounds_rect 左上角）
        std::chrono::steady_clock::time_point start; ///< 动画触发时刻
        bool   active    = false;  ///< 是否正在播放
    };
    RippleState              ripple_;           ///< 当前涟漪状态

    /// MD3 背景色过渡动画（Storyboard 驱动 BackgroundProperty 在 Animation 优先级插值）
    core::OwnedPtr<animation::Storyboard> bg_storyboard_;
};

} // namespace mine::ui
