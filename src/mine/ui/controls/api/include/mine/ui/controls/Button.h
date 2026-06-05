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
 * 动画架构（F3 范式——三层分离）：
 *   背景色过渡由 VisualStateManager 的 Storyboard 管理：go_to_state() 先通过
 *   StyleTrigger(P4) 写入终值，再以 animate_dp()（仅声明路径）插值到该终值；
 *   Ripple 涟漪动画直接由 AnimationClock 驱动。两者共用 anim_tick_callback，
 *   在 on_visual_state_changed 中注册到 AnimationClock，tick 返回 false 时自动注销。
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
#include <mine/math/CornerRadii.h>
#include <mine/core/Memory.h>

namespace mine::paint { class Canvas; }
namespace mine::ui::input { class MouseEventArgs; }
namespace mine::ui::style { class Style; }

// 前向声明（完整定义仅在 Button.cpp 中引入）
namespace mine::ui { class ICommand; class ContentPresenter; class Border; }

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
     * @brief 背景画刷属性（Variant 存储 paint::Brush）。
     *
     * Default(P0) = MD3 Primary #6750A4；默认样式以 StyleSetter(P5) 写入基线值；
     * Hovered/Pressed/Disabled 状态由 StyleTrigger(P4) 写入终值，VSM Storyboard
     * 以 Animation(P1) 进行插值过渡；on_render 通过 get_value 读取最高优先级层。
     * 用户调用 set_background() 写入 Local(P2)，高于 StyleTrigger，故不受状态影响。
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
     * @brief 命令属性（Variant 存储 ICommand*）。
     *
     * 等价于 WPF 的 Button.CommandProperty。
     * 通过 set_binding(CommandProperty, "cmd_name") 按名绑定 ViewModel 命令；
     * 设置后 Button 自动：
     *   1. 订阅 ICommand::can_execute_changed → 实时刷新 is_enabled_ 状态
     *   2. 在 Click 时调用 ICommand::execute(CommandParameterProperty 的当前值)
     * 不拥有命令指针，调用方须保证命令生命周期覆盖 Button。
     */
    static const DependencyProperty& CommandProperty;

    /**
     * @brief 命令参数属性（Variant，传递给 ICommand::execute / can_execute）。
     *
     * 默认为空 Variant（等价于无参数命令）。
     * 可通过 set_binding(CommandParameterProperty, "prop_name") 动态绑定。
     */
    static const DependencyProperty& CommandParameterProperty;

    /**
     * @brief 圆角半径属性（CornerRadii，四角独立）。
     *
     * 取值规则：
     *   - 各角 rx/ry 均为负数（默认 CornerRadii::uniform(-1)）：自动胶囊形，r = height / 2。
     *   - 各角 rx/ry 均为非负数：用户指定的固定圆角（可四角各异）。
     *
     * 影响：渲染背景/边框圆角、Ripple 裁剪区域、Visual 级命中测试边界。
     */
    static const DependencyProperty& CornerRadiusProperty;

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

    /**
     * @brief 读取 CommandProperty 中当前绑定的命令指针（未绑定时返回 nullptr）。
     */
    [[nodiscard]] ICommand* command() const noexcept;

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

    /**
     * @brief 设置 VSM 使用的样式（覆盖内置的 default_button_style()）。
     *
     * 在模板构建前（首次 measure 之前）调用：build_fn_ 将用此样式
     * 完成 P5 基线值写入（apply）和 VSM P4 状态值配置（apply_state）。
     * 在模板构建后调用：直接更新已有 VSM 的样式引用（立即生效）。
     * 传入 nullptr 将回退到 default_button_style()。
     */
    void set_vsm_style(style::Style* style) noexcept;
    [[nodiscard]] style::Style* vsm_style() const noexcept;

    /// 读取 BorderColorProperty
    [[nodiscard]] paint::Brush border_color() const noexcept;
    /// 写入 BorderColorProperty Local 槽
    void set_border_color(paint::Brush brush);

    /**
     * @brief 读取 CornerRadiusProperty 当前生效值。
     * @return 各角圆角半径（rx < 0 表示自动胶囊）。
     */
    [[nodiscard]] math::CornerRadii corner_radii() const noexcept;

    /**
     * @brief 设置四角独立圆角（写入 Local 槽）。
     * @param radii 四角圆角半径，各角 rx < 0 时该角使用自动胶囊。
     */
    void set_corner_radii(math::CornerRadii radii);

    /**
     * @brief 设置四角相同的圆角（快捷方法）。
     * @param radius 负数恢复自动胶囊（-1.0f），非负数为固定半径。
     */
    void set_corner_radius(float radius);

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
    void on_arrange(math::Rect final_rect) override;
    [[nodiscard]] ControlVisualState compute_visual_state() const override;
    /**
     * @brief 视觉状态切换钩子——注册 AnimationClock 驱动 VSM 动画。
     *
     * 背景色过渡完全由 VSM Storyboard 管理，此钩子只负责将 anim_tick_callback
     * 注册到 AnimationClock（幂等）；Ripple 涟漪的停止/启动在 on_mouse_down 中控制。
     */
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

    /// BorderColorProperty 变更时按是否透明推导内部 Border 的边框粗细
    /// （透明 → 0 不占布局；非透明 → 2dp）；边框画刷本身经 bind_property 同步
    static void on_border_color_changed(DependencyObject*         sender,
                                        const DependencyProperty& prop,
                                        const core::Variant&      old_v,
                                        const core::Variant&      new_v) noexcept;

    /**
     * @brief CommandProperty 变更回调。
     *
     * 取消旧命令的 can_execute_changed 订阅，订阅新命令，
     * 并立即按 can_execute() 结果刷新 is_enabled_。
     */
    static void on_command_changed(DependencyObject*         sender,
                                   const DependencyProperty& prop,
                                   const core::Variant&      old_v,
                                   const core::Variant&      new_v) noexcept;

    /**
     * @brief ICommand::can_execute_changed 回调（函数指针类型，传入 subscribe_can_execute_changed）。
     *
     * 每当 ViewModel 调用 notify_can_execute_changed() 时触发，
     * 重新查询 can_execute() 并调用 set_enabled() 同步按钮禁用状态。
     */
    static void on_can_execute_changed(ICommand* sender, void* user_data) noexcept;

    // ── 辅助函数 ────────────────────────────────────────────────────────────

    /**
     * @brief 交互覆盖层内部元素（组合式视觉树中介于 Border 背景与 ContentPresenter
     *        文字之间的一层），负责绘制 MD3 State Layer、Ripple 涟漪与边框描边。
     *
     * 作为 Button 的私有嵌套类，可访问 Button 的交互状态成员（ripple_/is_hovered_ 等）。
     * 完整定义在 Button.cpp 中。
     */
    class InteractionLayer;

    /**
     * @brief 绘制交互覆盖层内容（由 InteractionLayer::on_render 转发调用）。
     *
     * 在背景之上、文字之下绘制：State Layer 半透明叠加、圆角边框描边、Ripple 涟漪。
     * 全部裁剪到按钮圆角形状。
     *
     * @param canvas 画布
     * @param rect   覆盖层局部矩形（等于按钮内容矩形）
     */
    void render_interaction(paint::Canvas& canvas, const math::Rect& rect);

    /**
     * @brief 根据 CornerRadiusProperty 和当前高度计算实际渲染的 CornerRadii。
     * @param height 按钮当前高度（bounds_rect().height）
     * @return 完全解析后的四角半径（所有角 >= 0）
     */
    [[nodiscard]] math::CornerRadii compute_radii(float height) const noexcept;

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
    uint32_t                 cmd_token_    = 0;        ///< can_execute_changed 订阅 token（0 表示未订阅）
    /// 内边距缓存（由 on_padding_changed 从 PaddingProperty 同步）
    math::Thickness          padding_      = math::Thickness::symmetric(24.0f, 10.0f);
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

    /// MD3 State Layer 缓动状态：Hover/Press 半透明蒙版的 alpha 做淡入淡出，
    /// 避免状态切换时整层蒙版瞬时突现/突隐（仅当背景为 Local 色、背景色动画无变化时启用）。
    struct StateLayerState {
        float current_alpha = 0.0f;  ///< 当前渲染用 alpha（由 tick 每帧朝 target 缓动）
        float target_alpha  = 0.0f;  ///< 目标 alpha（Hover=0.08 / Press=0.12 / 其他=0）
    };
    StateLayerState state_layer_;  ///< 当前 State Layer 蒙版缓动状态

    ContentPresenter*        content_part_           = nullptr;  ///< 文字层（组合式视觉树最内层，bind_property 目标）
    Border*                  border_part_            = nullptr;  ///< 背景/圆角基元层（组合式视觉树最底层，圆角同步目标）
    style::Style*            vsm_style_              = nullptr;  ///< 用户指定的 VSM 样式（nullptr 则用 default_button_style）
};

} // namespace mine::ui
