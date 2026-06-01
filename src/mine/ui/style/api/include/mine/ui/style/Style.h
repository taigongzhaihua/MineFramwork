#pragma once

#include <mine/ui/style/Api.h>
#include <mine/ui/style/StyleSetter.h>

#include <mine/core/StringView.h>
#include <mine/core/TypeId.h>
#include <mine/containers/InlineString.h>
#include <mine/containers/SmallVector.h>

namespace mine::ui {
class DependencyObject;
}  // namespace mine::ui

namespace mine::ui::style {

/**
 * @brief 样式对象：一组针对特定控件类型的属性 setter 集合。
 *
 * Style 支持两种工作模式（互斥选择）：
 *   1. **mmlc 生成路径**（apply_fn_ 非空）：调用 apply() 时直接执行 apply_fn_，
 *      apply_fn_ 由 mmlc 编译 `.mml` 文件时预生成，包含资源查找和订阅逻辑。
 *   2. **程序化路径**（apply_fn_ 为 nullptr）：调用 apply() 时遍历 setters_，
 *      对每个静态 setter 在 ValuePriority::StyleSetter (20) 写入属性值。
 *      DynamicResource setter（res_key 非空）在程序化路径下暂不处理。
 *
 * 优先级链（高 → 低）：
 *   Animation(60) > Local(50) > TemplateBind(40) > StyleTrigger(30)
 *   > StyleSetter(20) > Inherited(10) > Default(0)
 *
 * apply()：以 StyleSetter(20) 写入，不会覆盖 Local(50) 及以上的值。
 * apply_state()：以 StyleTrigger(30) 写入，可覆盖 StyleSetter(20) 的值，
 *               但不会覆盖 Local(50) 及以上的值。
 *
 * BasedOn 继承：
 *   apply() 先调用 base_->apply()，再应用本样式的 setter，
 *   因此本样式的 setter 覆盖父样式中同属性的 setter。
 *
 * 线程安全：不提供，调用方负责在 UI 线程使用。
 */
class MINE_UI_STYLE_API Style {
public:
    /// mmlc 生成的 apply 函数类型（接受 DependencyObject& 以解耦 mine.ui.visual）
    using ApplyFn = void (*)(ui::DependencyObject&);

    /// mmlc 生成的 apply 函数（nullptr = 使用程序化 setters_）
    ApplyFn apply_fn_{nullptr};

    // ── 生命周期 ──────────────────────────────────────────────────────────

    Style()                            = default;
    ~Style()                           = default;
    Style(const Style&)                = default;
    Style& operator=(const Style&)     = default;
    Style(Style&&) noexcept            = default;
    Style& operator=(Style&&) noexcept = default;

    // ── 查询 ──────────────────────────────────────────────────────────────

    /// 返回此样式适用的控件类型（注册时设置）
    [[nodiscard]] core::TypeId     target_type() const noexcept { return target_type_; }

    /// 返回父样式（BasedOn；可为 nullptr）
    [[nodiscard]] Style*           base()        const noexcept { return base_; }

    /// 返回样式名称（用于日志/调试，可为空）
    [[nodiscard]] core::StringView name()        const noexcept { return name_; }

    // ── 应用 ──────────────────────────────────────────────────────────────

    /**
     * @brief 将样式基线值应用到目标元素（StyleSetter 优先级）。
     *
     * 若有父样式（BasedOn），先应用父样式，再应用本样式（子样式覆盖同属性）。
     * 若 apply_fn_ 非空，则使用 mmlc 路径（调用 apply_fn_）；
     * 否则遍历 setters_，对静态 setter 以 ValuePriority::StyleSetter 写入。
     *
     * @param target 目标 DependencyObject（通常为 Control 实例）
     */
    void apply(ui::DependencyObject& target) const;

    /**
     * @brief 将指定视觉状态的 setter 应用到目标元素（StyleTrigger 优先级）。
     *
     * 查找 state_name 对应的 VisualStateSetters，依次以 ValuePriority::StyleTrigger
     * 写入各 setter 的静态值。若状态不存在，则为空操作。
     *
     * @param target     目标 DependencyObject
     * @param state_name 视觉状态名（如 "Hovered"、"Pressed"）
     */
    void apply_state(ui::DependencyObject& target, core::StringView state_name) const;

    /**
     * @brief 清除此样式所有状态 setter 曾写入的 StyleTrigger(30) 槽。
     *
     * 在 go_to_state 切换状态前调用，确保旧状态的 StyleTrigger 值不残留，
     * 使得过渡到无 setter 的状态（如 Normal）时属性能正确回退到 StyleSetter(20)。
     *
     * @param target 目标 DependencyObject
     */
    void clear_all_state_values(ui::DependencyObject& target) const;

    // ── 构建器接口（程序化构造；mmlc 路径不使用此接口）─────────────────────

    /// 设置样式名称
    Style& set_name(core::StringView name);

    /// 设置目标控件类型
    Style& set_target_type(core::TypeId type_id) noexcept;

    /// 设置父样式（BasedOn；传 nullptr 清除继承）
    Style& set_base(Style* base) noexcept;

    /// 追加一个基线属性 setter（静态值或 DynamicResource 键）
    Style& add_setter(StyleSetter setter);

    /// 追加一组视觉状态 setter
    Style& add_state_setters(VisualStateSetters state_setters);

private:
    /// 目标控件类型（用于 StyleRegistry 查找匹配样式）
    core::TypeId             target_type_{};
    /// 样式名称（InlineString，典型 < 24 字节）
    containers::InlineString name_;
    /// 父样式（BasedOn；弱引用，不拥有生命周期）
    Style*                   base_{nullptr};
    /// 基线属性 setter 列表（程序化路径）
    containers::SmallVector<StyleSetter, 16>       setters_;
    /// 视觉状态 setter 列表（程序化路径）
    containers::SmallVector<VisualStateSetters, 8> state_setters_;
};

}  // namespace mine::ui::style
