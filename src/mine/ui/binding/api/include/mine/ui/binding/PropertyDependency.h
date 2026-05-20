/**
 * @file PropertyDependency.h
 * @brief 属性依赖项描述符，标识 BindingExpression 所依赖的源属性。
 *
 * 当源属性发生变更时，BindingExpression 会重新调用 getter 并更新目标。
 *
 * 支持两类来源：
 *   1. DependencyObject + DependencyProperty（UI 元素间绑定）
 *   2. INotifyPropertyChanged + 属性名（ViewModel → UI 绑定）
 *
 * 用法：
 * @code
 *   // 来自 DependencyObject 属性
 *   PropertyDependency dep1 = PropertyDependency::from_dep(source_obj, WidthProp);
 *
 *   // 来自 ViewModel 属性
 *   PropertyDependency dep2 = PropertyDependency::from_inpc(view_model, "Name");
 * @endcode
 */

#pragma once

#include <cstdint>
#include <mine/core/StringView.h>
#include <mine/ui/binding/Api.h>

namespace mine::ui {

class DependencyObject;
class DependencyProperty;
class INotifyPropertyChanged;

/**
 * @brief 属性依赖项描述符。
 *
 * 轻量值类型，仅持有指针，不拥有所指对象。
 * 调用方须确保源对象的生命周期覆盖 BindingExpression 的生命周期。
 */
struct PropertyDependency {
    /// 来源类型标识
    enum class Kind : uint8_t {
        DependencyProperty, ///< 来自 DependencyObject 的 DependencyProperty
        Inpc,               ///< 来自 INotifyPropertyChanged 的命名属性
    };

    Kind kind = Kind::DependencyProperty;

    // ── DependencyProperty 来源字段（kind == DependencyProperty 时有效）──

    /// 源 DependencyObject（不拥有，须由调用方保证生命周期）
    DependencyObject*         dep_obj  = nullptr;
    /// 源 DependencyProperty 描述符（不拥有，全局静态生命周期）
    const DependencyProperty* dep_prop = nullptr;

    // ── INotifyPropertyChanged 来源字段（kind == Inpc 时有效）────────────

    /// 源 INotifyPropertyChanged 对象（不拥有，须由调用方保证生命周期）
    INotifyPropertyChanged* inpc_src  = nullptr;
    /// 需要监听的属性名（必须指向稳定字符串，如字符串字面量）
    core::StringView        inpc_name;

    // ── 便捷工厂函数 ─────────────────────────────────────────────────────

    /**
     * @brief 从 DependencyObject + DependencyProperty 构造依赖项。
     *
     * @param obj  源对象（生命周期须覆盖绑定）
     * @param prop 源属性描述符（通常为静态全局变量）
     */
    [[nodiscard]] static PropertyDependency from_dep(
        DependencyObject&        obj,
        const DependencyProperty& prop) noexcept
    {
        PropertyDependency d;
        d.kind     = Kind::DependencyProperty;
        d.dep_obj  = &obj;
        d.dep_prop = &prop;
        return d;
    }

    /**
     * @brief 从 INotifyPropertyChanged 对象 + 属性名构造依赖项。
     *
     * @param inpc ViewModel 对象（生命周期须覆盖绑定）
     * @param name 属性名字符串（须为稳定的字符串字面量或长期存活的字符串）
     */
    [[nodiscard]] static PropertyDependency from_inpc(
        INotifyPropertyChanged& inpc,
        core::StringView        name) noexcept
    {
        PropertyDependency d;
        d.kind      = Kind::Inpc;
        d.inpc_src  = &inpc;
        d.inpc_name = name;
        return d;
    }
};

} // namespace mine::ui
