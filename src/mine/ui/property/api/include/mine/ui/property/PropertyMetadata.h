/**
 * @file PropertyMetadata.h
 * @brief 依赖属性元数据，描述属性的行为特征和变更回调。
 *
 * PropertyMetadata 在调用 register_property() 时传入，描述该属性：
 *   - 变更后是否需要触发布局/绘制失效
 *   - 是否沿可视化树向下继承
 *   - 变更时的自定义回调函数
 */

#pragma once

#include <mine/core/Variant.h>

namespace mine::ui {

// 前向声明，避免循环包含
class DependencyObject;
class DependencyProperty;

/**
 * @brief 属性变更回调函数指针类型。
 *
 * 参数说明：
 *   @param obj       发生属性变更的 DependencyObject 实例
 *   @param prop      发生变更的属性描述符
 *   @param old_value 变更前的有效值
 *   @param new_value 变更后的有效值
 *
 * 注意：此回调运行在 UI 线程，不得在回调内部再次调用同属性的 set_value()
 * （会被防递归保护忽略）。
 */
using PropertyChangedFn = void (*)(DependencyObject*       obj,
                                   const DependencyProperty& prop,
                                   const mine::core::Variant& old_value,
                                   const mine::core::Variant& new_value);

/**
 * @brief 依赖属性元数据。
 *
 * 使用 C++20 指定初始化语法（designated initializers）构造：
 * @code
 *   PropertyMetadata meta{
 *       .affects_measure = true,
 *       .changed = &MyClass::s_on_content_changed
 *   };
 * @endcode
 */
struct PropertyMetadata {
    /// 属性变更后是否触发 DependencyObject::invalidate_measure()
    bool              affects_measure = false;
    /// 属性变更后是否触发 DependencyObject::invalidate_arrange()
    bool              affects_arrange = false;
    /// 属性变更后是否触发 DependencyObject::invalidate_render()
    bool              affects_render  = false;
    /// 属性值是否沿可视化树向下继承（如 FontSize、TextColor）
    bool              inherits        = false;
    /// 属性变更回调（nullptr 表示不需要自定义回调），在 on_property_changed() 之后调用
    PropertyChangedFn changed         = nullptr;
};

} // namespace mine::ui
