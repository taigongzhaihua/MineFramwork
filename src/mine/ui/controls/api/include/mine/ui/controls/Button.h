/**
 * @file Button.h
 * @brief Button —— 可点击按钮控件。
 *
 * 实现 MD3 Filled Button 规范，支持：
 *   - Normal / Hovered / Pressed / Disabled 四种视觉状态平滑过渡
 *   - 鼠标按下时 Ripple 涟漪动画
 *   - Click 路由事件（Bubble 策略）
 *   - ContentProperty / PaddingProperty / BackgroundProperty 等多个 DependencyProperty
 *   - 背景色、悬停色、按下色、前景色、边框色均通过 DependencyProperty 驱动
 *
 * 动画架构（F2 阶段）：
 *   背景色过渡 Storyboard 与 Ripple 均注册到 AnimationClock，
 *   由应用层（或未来框架层）统一调用 AnimationClock::instance().tick_all(dt) 驱动，
 *   控件自身不再暴露任何 tick/has_active 接口。
 */

#pragma once

#include <mine/ui/controls/Api.h>
#include <mine/ui/controls/ContentControl.h>
#include <mine/ui/event/RoutedEvent.h>
#include <mine/ui/property/DependencyProperty.h>
#include <mine/containers/InlineString.h>
#include <mine/paint/Brush.h>
#include <mine/math/Color.h>
#include <mine/math/Thickness.h>
#include <mine/core/Memory.h>

namespace mine::paint { class Canvas; }
namespace mine::ui::input { class MouseEventArgs; }
namespace mine::ui::animation { class Storyboard; }

namespace mine::ui {

/**
 * @brief 可点击按钮控件（MD3 Filled Button）。
 *
 * 所有可视属性均通过 DependencyProperty 管理（单一真相源）。
 * 动画 tick 由 AnimationClock 统一驱动，控件不暴露定时器或 tick 接口。
 */
class MINE_UI_CONTROLS_API Button : public ContentControl {
public:
    // ── 路由事件 ───────────────────────────────────────────────────────────

    static const RoutedEvent& ClickEvent();

    // ── 依赖属性 ───────────────────────────────────────────────────────────

    /**
     * @brief 内容属性（继承自 ContentControl::ContentProperty）。
     *
     * 与 ContentPresenter::ContentProperty 通过 bind_template 连接。
     * 向后兼容：Button::ContentProperty == ContentControl::ContentProperty。
     */
    using ContentControl::ContentProperty;

    /**
     * @brief 当前渲染背景画刷（由 Storyboard 在 Animation 槽写入插值画刷）。
     *
     * Default = MD3 Primary Brush::solid(#6750A4)。用户设定的 Normal 态画刷通过 set_background()
     * 写入 Local 槽；Storyboard 在 Animation 槽（优先级 60）覆盖，on_render 读取最高优先级。
     */
    static const DependencyProperty& BackgroundProperty;

    /**
     * @brief 内边距属性（Variant 存储 Thickness）。
     *
     * 默认：水平 24dp，垂直 10dp（MD3 Filled Button 规范）。
     */
    static const DependencyProperty& PaddingProperty;

    /**
     * @brief 前景画刷（文字颜色，Variant 存储 paint::Brush）。
     *
     * Default = Brush::solid(White)（MD3 On Primary）。
     * 变更时自动传播到模板树中的 ContentPresenter。
     */
    static const DependencyProperty& ForegroundProperty;

    /**
     * @brief 边框画刷（Variant 存储 paint::Brush）。
     *
     * Default = Brush::solid(Transparent)（MD3 Filled Button 无外边框）。
     */
    static const DependencyProperty& BorderColorProperty;

    /**
     * @brief Hovered 状态目标背景画刷（Variant 存储 paint::Brush）。
     *
     * Default = Brush::solid(MD3 Primary + OnPrimary * 8%，≈ #735BAC)。
     * set_background_hovered() 写入 Local 槽；on_visual_state_changed 读取作为过渡终值。
     */
    static const DependencyProperty& HoveredBackgroundProperty;

    /**
     * @brief Pressed 状态目标背景画刷（Variant 存储 paint::Brush）。
     *
     * Default = Brush::solid(MD3 Primary + OnPrimary * 12%，≈ #7A65AF)。
     * set_background_pressed() 写入 Local 槽；on_visual_state_changed 读取作为过渡终值。
     */
    static const DependencyProperty& PressedBackgroundProperty;

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

    /// 读取 ForegroundProperty（最高优先级生效值）
    [[nodiscard]] paint::Brush foreground() const noexcept;
    /// 写入 ForegroundProperty Local 槽，并传播到 ContentPresenter
    void set_foreground(paint::Brush brush);

    /// 读取 BackgroundProperty（当前渲染画刷，含动画插值）
    [[nodiscard]] paint::Brush background() const noexcept;
    /// 写入 BackgroundProperty Local 槽（同时停止进行中的 Storyboard）
    void set_background(paint::Brush brush);

    /// 读取 HoveredBackgroundProperty
    [[nodiscard]] paint::Brush background_hovered() const noexcept;
    /// 写入 HoveredBackgroundProperty Local 槽
    void set_background_hovered(paint::Brush brush);

    /// 读取 PressedBackgroundProperty
    [[nodiscard]] paint::Brush background_pressed() const noexcept;
    /// 写入 PressedBackgroundProperty Local 槽
    void set_background_pressed(paint::Brush brush);

    /// 读取 BorderColorProperty
    [[nodiscard]] paint::Brush border_color() const noexcept;
    /// 写入 BorderColorProperty Local 槽
    void set_border_color(paint::Brush brush);

    /**
     * @brief 设置文字渲染字体（同步传播到 ContentPresenter）。
     * @param font_face 字体对象指针（nullptr 时 ContentPresenter 回退到占位线）
     */
    void set_font_face(void* font_face) noexcept;

    /**
     * @brief 设置文字渲染字号（逻辑像素，默认 14.0f）。
     */
    void set_font_size(float size_px) noexcept;

protected:
    void on_measure(math::Size available_size) override;
    void on_render(paint::Canvas& canvas) override;
    UIElement* hit_test(math::Point p) override;
    [[nodiscard]] ControlVisualState compute_visual_state() const override;
    void on_visual_state_changed(ControlVisualState old_state,
                                 ControlVisualState new_state) override;

private:
    // ── 内容变更钩子 ──────────────────────────────────────────────────────

    /// 重写 ContentControl::on_content_changed，同步 text_ 成员缓存
    void on_content_changed(const core::Variant& old_v,
                             const core::Variant& new_v) noexcept override;

    // ── 依赖属性变更回调 ───────────────────────────────────────────────────

    /// PaddingProperty 变更时同步 padding_ 成员缓存
    static void on_padding_changed(DependencyObject*         sender,
                                   const DependencyProperty& prop,
                                   const core::Variant&      old_v,
                                   const core::Variant&      new_v) noexcept;

    /// ForegroundProperty 变更时将新颜色推送到 ContentPresenter
    static void on_foreground_changed(DependencyObject*         sender,
                                      const DependencyProperty& prop,
                                      const core::Variant&      old_v,
                                      const core::Variant&      new_v) noexcept;

    // ── 动画 tick 回调（注册到 AnimationClock）────────────────────────────

    /**
     * @brief AnimationClock tick 回调（推进 Ripple + Storyboard）。
     *
     * @param user_data 注册时的 handle（即 Button* this）
     * @param dt        帧时长（秒）
     * @return true  仍有活跃动画，AnimationClock 继续下一帧回调
     * @return false 全部动画完成，AnimationClock 自动移除此注册项
     */
    static bool anim_tick_callback(void* user_data, float dt) noexcept;

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
    bool                     is_enabled_   = true;
    bool                     is_hovered_   = false;   ///< 鼠标正悬停在按钮上
    bool                     is_pressed_   = false;
    /// 内边距缓存（由 on_padding_changed 从 PaddingProperty 同步）
    math::Thickness          padding_      = math::Thickness::symmetric(24.0f, 10.0f);
    /// Normal 态目标背景画刷（用户通过 set_background() 设定的语义値）
    /// F2 临时保留：用于 on_visual_state_changed 中 Normal 态目标画刷查找，
    /// F3 将改为从 BackgroundProperty Local 槽读取（配合 StyleSetter）
    paint::Brush             background_   = paint::Brush::solid_rgb(0x6750A4);
    void*                    font_face_    = nullptr;
    float                    font_size_px_ = 14.0f;

    /// MD3 Ripple 涟漪动画状态（鼠标按下时触发，由 AnimationClock 驱动 tick）
    struct RippleState {
        float center_x   = 0.0f;  ///< 涟漪圆心 X（相对于 bounds_rect 左上角）
        float center_y   = 0.0f;  ///< 涟漪圆心 Y（相对于 bounds_rect 左上角）
        float elapsed_ms = 0.0f;  ///< 动画已播放时长（毫秒），由 tick 每帧累加
        bool  active     = false; ///< 是否正在播放
    };
    RippleState ripple_;  ///< 当前涟漪状态

    /// MD3 背景色过渡 Storyboard（在 AnimationClock tick 中推进）
    core::OwnedPtr<animation::Storyboard> bg_storyboard_;
};

} // namespace mine::ui
