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
#include <mine/core/StringView.h>
#include <mine/core/Variant.h>
#include <mine/containers/Vector.h>
#include <mine/ui/binding/Api.h>
#include <mine/ui/binding/BindingMode.h>
#include <mine/ui/binding/PropertyDependency.h>

namespace mine::ui {

class DependencyObject;
class DependencyProperty;
struct Binding;
struct IConverter;
class INotifyPropertyChanged;
struct Binding;

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
     * @brief 传递给 converter.convert() 的参数字符串（可选）。
     *
     * 须为字符串字面量或长期存活的字符串（attach() 后内部只存储 StringView）。
     * 如未设置，则传空字符串给 converter。
     */
    core::StringView conv_param;

    /**
     * @brief 回退値（可选）。
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

    /**
     * @brief WPF 风格：按属性名一步建立 INPC → DependencyProperty 的绑定。
     *
     * 等价于 bind_inpc() 但无需手写 getter lambda。内部通过
     * INotifyPropertyChanged::get_property(prop_name) 自动读取源值。
     * ObservableObject 子类（通过 MINE_OBSERVABLE 宏声明属性）自动实现此接口。
     *
     * 用法示例：
     * @code
     *   // 无需任何 lambda，框架自动从属性名反射读取值
     *   BindingExpression::bind(expr, vm, "count", label, TextBlock::TextProperty);
     * @endcode
     *
     * @param out         待激活的 BindingExpression（须未 attach）
     * @param src         INotifyPropertyChanged 源对象（生命周期须覆盖 out）
     * @param prop_name   属性名（须与 MINE_OBSERVABLE 宏的 Name 参数完全一致）
     * @param target      目标 DependencyObject（生命周期须覆盖 out）
     * @param target_prop 目标属性描述符
     * @param mode        绑定方向，默认 OneWay
     */
    static void bind(
        BindingExpression&        out,
        INotifyPropertyChanged&   src,
        core::StringView          prop_name,
        DependencyObject&         target,
        const DependencyProperty& target_prop,
        BindingMode               mode = BindingMode::OneWay) noexcept;

    /**
     * @brief WPF 风格：从目标控件的 DataContext 自动解析源，按属性名建立绑定。
     *
     * 等价于 WPF 的 `{Binding PropName}` 语法，视图层完全不需要显式引用 ViewModel：
     * @code
     *   // 从 label 的 DataContext（由 Window::DataContextProperty inherits 传播而来）
     *   // 自动取出 INotifyPropertyChanged 指针，再按属性名建立绑定
     *   BindingExpression::bind(expr, "count", label, TextBlock::TextProperty);
     * @endcode
     *
     * 前提条件：
     *   1. `register_data_context_property()` 已被调用（由 mine.ui.window 静态初始化时完成）
     *   2. target 的 DataContextProperty 已持有 `INotifyPropertyChanged*` 值
     *      （Window::set_data_context() + inherits 传播 会自动完成这一步）
     *
     * @param out         待激活的 BindingExpression（须未 attach）
     * @param prop_name   属性名（须与 MINE_OBSERVABLE 宏的 Name 参数完全一致）
     * @param target      目标 DependencyObject（其 DataContext 将作为绑定源）
     * @param target_prop 目标属性描述符
     * @param mode        绑定方向，默认 OneWay
     */
    static void bind(
        BindingExpression&        out,
        core::StringView          prop_name,
        DependencyObject&         target,
        const DependencyProperty& target_prop,
        BindingMode               mode = BindingMode::OneWay) noexcept;

    /**
     * @brief WPF 风格：完整 Binding 描述符版，支持 converter/conv_param/fallback。
     *
     * 等价于 WPF 的 `element.SetBinding(prop, new Binding("Name") { Converter=..., Mode=... })`。
     *
     * @code
     *   BindingExpression::bind(expr, vm, ui::Binding{
     *       .prop_name  = "byte_count",
     *       .converter  = &bytes_to_human_readable,
     *       .conv_param = "MB",
     *       .fallback   = core::Variant{ "N/A" },
     *   }, label, TextBlock::TextProperty);
     * @endcode
     *
     * @param out         待激活的 BindingExpression（须未 attach）
     * @param src         INotifyPropertyChanged 源对象
     * @param binding     Binding 描述符（prop_name/mode/converter/conv_param/fallback）
     * @param target      目标 DependencyObject
     * @param target_prop 目标属性描述符
     */
    static void bind(
        BindingExpression&        out,
        INotifyPropertyChanged&   src,
        const Binding&            binding,
        DependencyObject&         target,
        const DependencyProperty& target_prop) noexcept;

    /**
     * @brief WPF 风格：从 DataContext 自动解析源 + 完整 Binding 描述符版。
     *
     * 等价于 `bind(out, prop_name, target, target_prop)` 简化版，
     * 但支持 converter/conv_param/fallback 完整配置。
     * 通常通过 FrameworkElement::set_binding(prop, Binding{...}) 调用。
     *
     * @param out         待激活的 BindingExpression（须未 attach）
     * @param binding     Binding 描述符（prop_name/mode/converter/conv_param/fallback）
     * @param target      目标 DependencyObject（其 DataContext 将作为绑定源）
     * @param target_prop 目标属性描述符
     */
    static void bind(
        BindingExpression&        out,
        const Binding&            binding,
        DependencyObject&         target,
        const DependencyProperty& target_prop) noexcept;

    /**
     * @brief 一步建立 DependencyProperty → DependencyProperty 的绑定（元素间 / 控件间绑定）。
     *
     * 等价于 WPF 的 ElementName 绑定（`{Binding ElementName=src, Path=Prop}`），
     * 但 C++ 化为直接传入源对象与源属性描述符，无需字符串路径与运行时反射。
     *
     * 各模式行为：
     *   - OneWay        ：source.source_prop 变更 → 写入 target.target_prop（TemplateBind 优先级）
     *   - OneTime       ：attach 时求值一次写入目标，随后不再订阅
     *   - TwoWay        ：双向同步；目标变更回写源（Local 优先级），内置防循环保护
     *   - OneWayToSource：仅 目标 → 源；attach 时把目标当前值回写源，随后订阅目标变更
     *
     * @code
     *   // 让 Border 的圆角随宿主 TextBox 的 CornerRadius 同步（OneWay）
     *   BindingExpression::bind_property(expr,
     *       textbox, TextBox::CornerRadiusProperty,
     *       border,  Border::CornerRadiusProperty);
     *
     *   // 滑块值与进度条值双向同步
     *   BindingExpression::bind_property(expr,
     *       slider,   Slider::ValueProperty,
     *       progress, ProgressBar::ValueProperty,
     *       BindingMode::TwoWay);
     * @endcode
     *
     * @param out         待激活的 BindingExpression（须未 attach）
     * @param source      源 DependencyObject（生命周期须覆盖 out）
     * @param source_prop 源属性描述符
     * @param target      目标 DependencyObject（生命周期须覆盖 out）
     * @param target_prop 目标属性描述符
     * @param mode        绑定方向，默认 OneWay
     * @param converter   可选值转换器（不拥有；生命周期须覆盖 out）
     * @param conv_param  传递给 converter 的参数字符串（可选）
     */
    static void bind_property(
        BindingExpression&        out,
        DependencyObject&         source,
        const DependencyProperty& source_prop,
        DependencyObject&         target,
        const DependencyProperty& target_prop,
        BindingMode               mode       = BindingMode::OneWay,
        IConverter*               converter  = nullptr,
        core::StringView          conv_param = {}) noexcept;

    /**
     * @brief 注入 DataContextProperty 描述符（由 mine.ui.window 静态初始化时调用）。
     *
     * 将 Window::DataContextProperty 的描述符指针传入 mine.ui.binding 层，
     * 使无 src 的 bind() 重载能在不产生循环依赖的前提下，
     * 从目标控件的 DataContext 属性中读取 INotifyPropertyChanged 指针。
     *
     * 调用时机：Window.cpp 的静态属性注册块，在 DataContextProperty 初始化之后立即调用。
     * 无需手动调用，框架自动完成注入。
     *
     * @param dp  DataContextProperty 的地址（静态生命周期，存储为裸指针安全）
     */
    static void register_data_context_property(const DependencyProperty* dp) noexcept;

    /**
     * @brief 将绑定的目标对象重定向到新地址（供 FrameworkElement 移动时内部调用）。
     *
     * 当包含绑定的 FrameworkElement 被移动构造/赋值后，新对象地址不同于旧地址。
     * FrameworkElement 的显式移动构造/赋值操作会对每个已 attach 的 BindingExpression
     * 调用此方法，确保 Impl::target_obj 指向新地址，避免悬空指针。
     *
     * 未 attach 状态下调用为空操作。
     *
     * @param new_target 新的目标 DependencyObject 地址
     */
    void retarget(DependencyObject& new_target) noexcept;

private:
    struct Impl;
    core::Pimpl<Impl> p_;
};

} // namespace mine::ui
