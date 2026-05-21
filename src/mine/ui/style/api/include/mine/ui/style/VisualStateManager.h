/**
 * @file VisualStateManager.h
 * @brief VisualStateManager —— 控件视觉状态管理器。
 *
 * VisualStateManager（VSM）管理控件的视觉状态机：
 *   - 状态注册（define_state）：声明控件拥有哪些视觉状态（Normal/Hovered/Pressed 等）
 *   - 过渡注册（add_transition）：声明从某状态切换到另一状态时的配置，
 *     可附带 Storyboard 配置函数实现平滑过渡动画
 *   - 状态切换（go_to_state）：切换到目标状态，并应用对应的样式状态 setter
 *   - 动画推进（tick_animations）：每帧推进活跃 Storyboard，完成后自动清理
 *
 * 与 Style 的关联：
 *   - VSM 可通过 set_style() 关联一个 Style 对象
 *   - go_to_state() 切换成功后，自动调用 style->apply_state(owner, state_name)
 *     将对应状态的属性 setter 以 StyleTrigger(30) 优先级写入宿主元素
 *
 * 与 Control 的集成：
 *   - Control::set_visual_state_manager() 存储 VSM 实例
 *   - Control::update_visual_state() 调用 vsm()->go_to_state() 驱动状态切换
 *   - 渲染循环每帧调用 vsm()->tick_animations(dt) 推进属性动画
 *
 * 循环依赖避免：
 *   - 构造函数接受 DependencyObject&（而非 Control&），
 *     使 mine.ui.style 仅依赖 mine.ui.property，不依赖 mine.ui.visual。
 *   - mine.ui.style 依赖 mine.ui.animation（Storyboard 类型）
 *
 * 线程安全：不提供，调用方须在 UI 线程使用。
 *
 * Task #17 新增：
 *   - VisualStateTransition 扩展 configure 字段（Storyboard 配置函数）
 *   - add_transition(from, to, configure_fn) 重载（附带动画配置）
 *   - tick_animations(dt) 推进活跃 Storyboard
 *   - active_storyboards_ 管理活跃动画实例生命周期
 */

#pragma once

#include <mine/ui/style/Api.h>
#include <mine/core/StringView.h>
#include <mine/core/Function.h>
#include <mine/core/Memory.h>
#include <mine/containers/InlineString.h>
#include <mine/containers/SmallVector.h>

// 前向声明：避免将 DependencyObject.h / Style.h / Storyboard.h 拉入依赖
namespace mine::ui {
class DependencyObject;
}  // namespace mine::ui

namespace mine::ui::style {
class Style;
}  // namespace mine::ui::style

namespace mine::ui::animation {
class Storyboard;
}  // namespace mine::ui::animation

namespace mine::ui::style {

/**
 * @brief 状态过渡描述。
 *
 * 记录从 from 状态到 to 状态的过渡：
 *   - from 为 "*" 或空字符串时匹配任意源状态（通配符）
 *   - to   必须是已注册的状态名
 *   - configure 为可选的 Storyboard 配置函数；为空时立即跳变（无动画）
 *
 * Task #17：追加 configure 字段，使 VisualStateTransition 成为 move-only 类型。
 * SmallVector 通过 push_back(std::move(t)) 追加 move-only 元素。
 */
struct VisualStateTransition {
    containers::InlineString from;  ///< 源状态名（"*" = 通配）
    containers::InlineString to;    ///< 目标状态名

    /// Storyboard 配置函数（可为空；空时立即跳变，不播放动画）
    /// 签名：void(animation::Storyboard& sb)
    /// SBO 32 字节约束：捕获列表不超过 4 个指针（64 位）
    core::Function<void(animation::Storyboard&)> configure;

    // move-only：configure 字段不可拷贝
    VisualStateTransition()                                      = default;
    VisualStateTransition(VisualStateTransition&&)               = default;
    VisualStateTransition& operator=(VisualStateTransition&&)    = default;
    VisualStateTransition(const VisualStateTransition&)          = delete;
    VisualStateTransition& operator=(const VisualStateTransition&) = delete;
};

/**
 * @brief 控件视觉状态管理器。
 *
 * 典型用法（由 mmlc 生成的 ControlTemplate::BuildFn 调用）：
 * @code
 *   VisualStateManager vsm{button};        // 绑定宿主控件
 *   vsm.define_state("Normal");
 *   vsm.define_state("Hovered");
 *   vsm.define_state("Pressed");
 *   vsm.add_transition("*", "Hovered");   // 任意 → Hovered
 *   vsm.add_transition("*", "Pressed");   // 任意 → Pressed
 *   vsm.set_style(&my_style);             // 关联样式（状态 setter）
 *   button.set_visual_state_manager(std::move(vsm));
 *
 *   // 控件事件中
 *   button.vsm()->go_to_state("Hovered"); // 切换状态并应用 setter
 * @endcode
 */
class MINE_UI_STYLE_API VisualStateManager {
public:
    // ── 生命周期 ──────────────────────────────────────────────────────────

    /**
     * @brief 构造 VSM，绑定宿主控件（存储为指针以支持移动语义）。
     *
     * @param owner 宿主依赖对象（通常为 Control 实例，转换为 DependencyObject&）
     */
    explicit VisualStateManager(ui::DependencyObject& owner) noexcept;

    ~VisualStateManager() = default;

    /// 不可拷贝：持有对宿主的引用，拷贝语义不安全
    VisualStateManager(const VisualStateManager&)            = delete;
    VisualStateManager& operator=(const VisualStateManager&) = delete;

    /// 可移动：内部使用指针存储 owner，移动时 owner 指针随之转移
    VisualStateManager(VisualStateManager&&) noexcept            = default;
    VisualStateManager& operator=(VisualStateManager&&) noexcept = default;

    // ── 状态注册 ──────────────────────────────────────────────────────────

    /**
     * @brief 注册一个视觉状态名。
     *
     * 重复注册同名状态为空操作。
     *
     * @param name 状态名（如 "Normal"、"Hovered"、"Pressed"）
     */
    void define_state(core::StringView name) noexcept;

    /**
     * @brief 注册状态过渡（无动画，立即跳变）。
     *
     * @param from 源状态名（"*" 表示从任意状态触发过渡）
     * @param to   目标状态名（须已通过 define_state 注册）
     */
    void add_transition(core::StringView from, core::StringView to) noexcept;

    /**
     * @brief 注册状态过渡（附带 Storyboard 动画配置）。
     *
     * go_to_state() 触发此过渡时，将：
     *   1. 调用 configure_fn(sb) 向 sb 注册要驱动的属性及参数
     *   2. 采样 from 值（capture_from_values）
     *   3. 应用新状态样式（StyleTrigger 写入）
     *   4. 解析 to 值并开始动画（resolve_and_begin）
     *   5. 将 Storyboard 加入 active_storyboards_，由 tick_animations 逐帧推进
     *
     * @param from         源状态名（"*" 表示从任意状态触发）
     * @param to           目标状态名
     * @param configure_fn Storyboard 配置函数（rvalue，move 进过渡记录）
     *
     * @note configure_fn 的捕获列表须满足 SBO 32 字节约束（最多 4 个指针）。
     */
    void add_transition(core::StringView from,
                        core::StringView to,
                        core::Function<void(animation::Storyboard&)> configure_fn) noexcept;

    // ── 状态切换 ──────────────────────────────────────────────────────────

    /**
     * @brief 切换到指定视觉状态。
     *
     * 流程（有配置函数的过渡，use_transitions=true）：
     *   1. 若 state_name 未注册，返回 false（无副作用）
     *   2. 若 state_name 已是当前状态，返回 false
     *   3. 停止并清理当前所有活跃 Storyboard（中断未完成动画）
     *   4. 查找匹配的过渡（精确 from → 通配 "*"），取第一个有 configure 的过渡
     *      - 若找到带动画的过渡：创建 Storyboard，调用 configure，
     *        capture_from_values，apply_state，resolve_and_begin，
     *        加入 active_storyboards_
     *      - 若未找到或 use_transitions=false：直接 apply_state（立即跳变）
     *   5. 更新 current_state_ = state_name
     *   6. 返回 true
     *
     * @param state_name      目标状态名
     * @param use_transitions 是否使用过渡动画（false 时立即跳变，跳过 Storyboard）
     * @return true  = 状态实际切换成功
     * @return false = 目标状态未注册 / 已是当前状态
     */
    bool go_to_state(core::StringView state_name,
                     bool             use_transitions = true) noexcept;

    /**
     * @brief 推进所有活跃 Storyboard 一帧。
     *
     * 完成的 Storyboard 将自动调用 stop()（清除 Animation 优先级）并从列表移除。
     * 建议由渲染循环或更新循环每帧调用。
     *
     * @param dt 本帧时间步长（秒），须 >= 0
     * @return true  = 仍有活跃 Storyboard 正在运行
     * @return false = 无活跃 Storyboard（所有动画已完成或未开始）
     */
    bool tick_animations(float dt) noexcept;

    // ── 查询 ──────────────────────────────────────────────────────────────

    /**
     * @brief 返回当前视觉状态名（初始为空字符串，切换后为最近一次成功切换的状态）。
     */
    [[nodiscard]] core::StringView current_state() const noexcept;

    /**
     * @brief 检查指定状态名是否已注册。
     */
    [[nodiscard]] bool has_state(core::StringView name) const noexcept;

    /**
     * @brief 检查从 from 到 to 是否存在已注册的过渡（含通配符 "*" 匹配）。
     */
    [[nodiscard]] bool has_transition(core::StringView from,
                                      core::StringView to) const noexcept;

    // ── 关联样式 ──────────────────────────────────────────────────────────

    /**
     * @brief 关联样式对象，供 go_to_state() 应用状态 setter。
     *
     * 设置为 nullptr 时，go_to_state() 仅更新 current_state_，
     * 不调用任何样式 setter。
     *
     * @param style 样式对象指针（非拥有；调用方负责生命周期）
     */
    void set_style(const Style* style) noexcept;

    /**
     * @brief 返回当前关联的样式对象（可为 nullptr）。
     */
    [[nodiscard]] const Style* attached_style() const noexcept;

private:
    /// 宿主控件（存为指针，支持移动；初始化后不应为 nullptr）
    ui::DependencyObject* owner_{nullptr};

    /// 关联样式（可选；nullptr = 不应用状态 setter）
    const Style* style_{nullptr};

    /// 已注册的状态名列表（define_state 时追加）
    containers::SmallVector<containers::InlineString, 8> states_;

    /// 已注册的过渡列表（add_transition 时追加；VisualStateTransition 为 move-only）
    containers::SmallVector<VisualStateTransition, 16> transitions_;

    /// 当前状态名（初始为空；go_to_state 成功后更新）
    containers::InlineString current_state_;

    /// 当前活跃的 Storyboard 列表（go_to_state 创建，tick_animations 推进，完成后移除）
    /// 使用 OwnedPtr 因 Storyboard 含 SmallVector，为 move-only 类型
    containers::SmallVector<core::OwnedPtr<animation::Storyboard>, 4> active_storyboards_;
};

}  // namespace mine::ui::style
