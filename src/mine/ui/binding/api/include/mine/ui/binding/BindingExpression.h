/**
 * @file BindingExpression.h
 * @brief 运行时数据绑定表达式：将源属性绑定到目标 DependencyProperty。
 *
 * BindingExpression 是绑定系统的核心运行时对象，由 mmlc 生成代码或用户手动构造。
 * 它描述：
 *   - 如何从源读取值（getter）
 *   - 如何向源写回值（setter，TwoWay 时）
 *   - 依赖哪些源属性（deps，用于订阅变更通知）
 *   - 绑定方向、值转换器、回退值
 *
 * 生命周期：
 *   1. 构造并填充配置字段（getter/setter/deps/mode/converter/fallback）
 *   2. 调用 attach() 激活绑定，配置字段被消费（移动到内部实现）
 *   3. BindingExpression 存活期间绑定持续有效
 *   4. 销毁（或调用 detach()）时自动取消所有依赖订阅
 *
 * M1.1 实现 OneWay 和 OneTime；
 * M2 实现 TwoWay 和 OneWayToSource（需补充 setter 调用路径）。
 *
 * 示例（手动构造，DependencyObject 来源）：
 * @code
 *   BindingExpression expr;
 *   expr.getter = [&src]() noexcept -> core::Variant {
 *       return src.get_value(WidthProp);
 *   };
 *   expr.deps.push_back(PropertyDependency::from_dep(src, WidthProp));
 *   expr.mode = BindingMode::OneWay;
 *   expr.attach(target, HeightProp);
 *   // expr 必须保持存活以维持绑定
 * @endcode
 */

#pragma once

#include <mine/core/Function.h>
#include <mine/core/Pimpl.h>
#include <mine/core/Variant.h>
#include <mine/containers/Vector.h>
#include <mine/ui/binding/Api.h>
#include <mine/ui/binding/BindingMode.h>
#include <mine/ui/binding/PropertyDependency.h>

namespace mine::ui {

class DependencyObject;
class DependencyProperty;
struct IConverter;
struct INotifyPropertyChanged;

/**
 * @brief 运行时绑定表达式。
 *
 * 持有绑定所需的全部运行时状态：getter/setter 闭包、依赖订阅列表、
 * 目标引用、模式、转换器和回退值。
 *
 * 使用 PIMPL 模式隐藏实现细节，保证 ABI 稳定（订阅记录列表等内部细节不暴露）。
 */
class MINE_UI_BINDING_API BindingExpression {
public:
    /// getter 类型：无参数、返回 Variant 的可调用对象（noexcept）
    using Getter = core::Function<core::Variant()>;
    /// setter 类型：接收 const Variant&、返回 void 的可调用对象（TwoWay 时，M2）
    using Setter = core::Function<void(const core::Variant&)>;

    // ── 配置字段（在调用 attach() 之前设置）────────────────────────────────

    /**
     * @brief 源值求值函数（必填）。
     *
     * 通常为捕获了源对象引用的 lambda。
     * attach() 后将被移动到内部实现，此字段变为空。
     */
    Getter getter;

    /**
     * @brief 值回写函数（TwoWay 时填写，M2 实现）。
     *
     * M1.1 中此字段不会被调用（即使填写了也会被忽略）。
     * attach() 后将被移动到内部实现。
     */
    Setter setter;

    /**
     * @brief 依赖源属性列表（必填，至少包含一个依赖项）。
     *
     * 列表中每个 PropertyDependency 描述一个需要订阅的源属性。
     * 任意一个依赖源发生变更时，绑定系统重新求值并更新目标。
     * attach() 后此列表被清空（依赖已转移到内部订阅记录）。
     */
    containers::Vector<PropertyDependency> deps;

    /// 绑定方向，默认为 OneWay
    BindingMode mode = BindingMode::OneWay;

    /**
     * @brief 值转换器（可选）。
     *
     * 非 nullptr 时，getter 的返回值先经过 convert() 转换，再写入目标。
     * 不拥有此指针，调用方须保证其生命周期覆盖 BindingExpression。
     */
    IConverter* converter = nullptr;

    /**
     * @brief 回退值（可选）。
     *
     * 当 getter 返回空 Variant（或 converter 返回空 Variant）时，
     * 使用此值写入目标。
     */
    core::Variant fallback;

    // ── 生命周期 ─────────────────────────────────────────────────────────────

    BindingExpression() noexcept;
    ~BindingExpression();

    /// 移动构造（转移绑定所有权，包括激活状态和所有订阅）
    BindingExpression(BindingExpression&&) noexcept;
    BindingExpression& operator=(BindingExpression&&) noexcept;

    BindingExpression(const BindingExpression&)            = delete;
    BindingExpression& operator=(const BindingExpression&) = delete;

    // ── 激活 / 停用 ───────────────────────────────────────────────────────────

    /**
     * @brief 将绑定附加到目标属性，开始监听依赖源并更新目标。
     *
     * 调用此方法后：
     *   1. getter/setter/deps 字段被移动到内部实现（调用后字段为空）
     *   2. 每个 dep 被订阅（PropertyChanged 或 INPC 回调）
     *   3. 立即求值一次并以 ValuePriority::TemplateBind 写入目标
     *   4. OneTime 模式：写入后立即取消所有订阅
     *
     * 前提条件：
     *   - getter 非空
     *   - target 的生命周期须覆盖此 BindingExpression
     *   - 如果 is_attached() 已为 true，先调用 detach() 再 attach()
     *
     * @param target       目标 DependencyObject
     * @param target_prop  目标属性描述符
     */
    void attach(DependencyObject& target, const DependencyProperty& target_prop) noexcept;

    /**
     * @brief 分离绑定：取消所有依赖订阅，不修改目标属性的当前值。
     *
     * 分离后可重新填充 getter/setter/deps 并再次 attach()。
     */
    void detach() noexcept;

    /**
     * @brief 手动触发一次重新求值，更新目标属性值。
     *
     * 正常情况下不需要手动调用；源属性变更时自动触发。
     * 仅在已 attach() 后才有效，未 attach 时为空操作。
     */
    void evaluate() noexcept;

    /// 检查绑定是否已激活（已调用 attach() 且未 detach()）
    [[nodiscard]] bool is_attached() const noexcept;

    // ── 便捷工厂 ─────────────────────────────────────────────────────────────

    /**
     * @brief 一步建立 INPC → DependencyProperty 的 OneWay 绑定。
     *
     * 等价于手动设置 getter/deps/mode 后调用 attach()，但更简洁：
     * @code
     *   BindingExpression::bind_inpc(expr,
     *       vm, "Name",
     *       [&vm]() noexcept -> core::Variant { return core::Variant{vm.name()}; },
     *       label, TextBlock::TextProperty);
     * @endcode
     *
     * @param out        待激活的 BindingExpression（须未 attach）
     * @param src        INotifyPropertyChanged 源对象（生命周期须覆盖 out）
     * @param prop_name  要监听的属性名（须为字符串字面量或长期存活的字符串）
     * @param getter     源值求值 lambda
     * @param target     目标 DependencyObject（生命周期须覆盖 out）
     * @param target_prop 目标属性描述符
     * @param mode       绑定方向，默认 OneWay
     */
    static void bind_inpc(
        BindingExpression&        out,
        INotifyPropertyChanged&   src,
        core::StringView          prop_name,
        Getter                    getter,
        DependencyObject&         target,
        const DependencyProperty& target_prop,
        BindingMode               mode = BindingMode::OneWay) noexcept;

private:
    struct Impl;
    core::Pimpl<Impl> p_;
};

} // namespace mine::ui
