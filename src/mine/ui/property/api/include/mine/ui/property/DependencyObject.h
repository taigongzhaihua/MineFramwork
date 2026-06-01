/**
 * @file DependencyObject.h
 * @brief 依赖属性值存储与变更通知的基类。
 *
 * DependencyObject 为所有具有依赖属性的 UI 对象提供统一的值存储、
 * 优先级覆盖和变更通知机制。所有 UI 元素（Visual、Control 等）均
 * 直接或间接继承自 DependencyObject。
 *
 * 核心功能：
 *   - get_value()：按优先级返回当前生效值（无值时返回属性默认值）
 *   - set_value()：在指定优先级写入属性值，触发变更通知
 *   - clear_value()：清除指定优先级的值，回退到下一优先级
 *   - subscribe_property_changed()：订阅任意属性变更事件
 *
 * 线程安全：
 *   所有属性读写必须在拥有此对象的 UI 线程执行。
 *
 * 内存布局：
 *   使用 PIMPL 模式隐藏实现细节，保证 ABI 稳定性。实现内部使用
 *   SmallVector<ValueSlot, 16> 存储有效值，大多数控件无需堆分配。
 */

#pragma once

#include <cstdint>
#include <mine/core/Pimpl.h>
#include <mine/core/Variant.h>
#include <mine/ui/property/Api.h>
#include <mine/ui/property/DependencyProperty.h>
#include <mine/ui/property/ValuePriority.h>

namespace mine::ui {

/**
 * @brief 属性变更事件订阅回调函数类型。
 *
 * 订阅后，每当此 DependencyObject 上任意属性的生效值发生变更时回调。
 *
 * 参数：
 *   @param sender    触发变更的 DependencyObject 实例
 *   @param prop      发生变更的属性描述符（可与特定 DependencyProperty 比较）
 *   @param old_value 变更前的生效值
 *   @param new_value 变更后的生效值
 *   @param user_data 订阅时传入的用户自定义数据指针
 */
using PropertyChangedCallbackFn = void (*)(DependencyObject*       sender,
                                           const DependencyProperty& prop,
                                           const core::Variant&     old_value,
                                           const core::Variant&     new_value,
                                           void*                    user_data);

/**
 * @brief 依赖属性值存储与通知基类。
 *
 * 子类继承此类并通过 get_value()/set_value() 访问依赖属性，
 * 同时可覆盖 on_property_changed() 及 invalidate_* 方法以响应属性变更。
 */
class MINE_UI_PROPERTY_API DependencyObject {
public:
    DependencyObject();
    virtual ~DependencyObject();

    // 禁止拷贝（DependencyObject 具有身份，不可复制）
    DependencyObject(const DependencyObject&)            = delete;
    DependencyObject& operator=(const DependencyObject&) = delete;

    // 允许移动（子类需显式支持）
    DependencyObject(DependencyObject&&)            = default;
    DependencyObject& operator=(DependencyObject&&) = default;

    // ── 属性值读写 ────────────────────────────────────────────────────────

    /**
     * @brief 读取属性当前生效值。
     *
     * 返回所有有效槽中优先级最高的值；若无任何有效槽，则返回属性的
     * PropertyMetadata.default_value（即 DependencyProperty::default_value()）。
     *
     * @param prop 要读取的属性描述符
     * @return     当前生效的属性值（引用可能指向内部槽或属性默认值）
     */
    [[nodiscard]] const core::Variant& get_value(const DependencyProperty& prop) const noexcept;

    /**
     * @brief 读取属性在指定优先级上限及以下的生效值。
     *
     * 返回所有 priority <= max_priority 的槽中优先级最高的值；
     * 若无满足条件的有效槽，则返回属性的默认值。
     *
     * 此方法主要供动画系统在 StyleTrigger 写入后读取动画终止值时使用，
     * 避免被 Local(P50) 等更高优先级的值遮盖。
     *
     * @param prop         要读取的属性描述符
     * @param max_priority 允许读取的最高优先级（含）
     * @return             满足优先级限制的生效属性值
     */
    [[nodiscard]] const core::Variant& get_value(const DependencyProperty& prop,
                                                  ValuePriority max_priority) const noexcept;

    /**
     * @brief 在指定优先级写入属性值。
     *
     * 若此优先级的槽已存在，则更新其值；否则创建新槽。
     * 若新写入导致生效值发生变更，则触发属性变更通知：
     *   1. 调用 on_property_changed()（虚方法，子类可覆盖）
     *   2. 根据 PropertyMetadata 调用 invalidate_measure/arrange/render()
     *   3. 调用 PropertyMetadata.changed 回调（如已设置）
     *   4. 回调所有通过 subscribe_property_changed() 注册的订阅者
     *
     * @param prop     目标属性描述符
     * @param value    要写入的值
     * @param priority 值优先级（默认为本地值 ValuePriority::Local）
     */
    void set_value(const DependencyProperty& prop,
                   core::Variant             value,
                   ValuePriority             priority = ValuePriority::Local);

    /**
     * @brief 清除指定优先级的属性值槽。
     *
     * 若清除后生效值发生变更（退回到下一优先级的值或默认值），
     * 则触发属性变更通知。若该优先级无有效槽，则为空操作。
     *
     * @param prop     目标属性描述符
     * @param priority 要清除的优先级（默认为本地值 ValuePriority::Local）
     */
    void clear_value(const DependencyProperty& prop,
                     ValuePriority             priority = ValuePriority::Local);

    /**
     * @brief 检查指定优先级是否存在有效值槽。
     *
     * @param prop     目标属性描述符
     * @param priority 要检查的优先级（默认为本地值）
     * @return         true 表示存在该优先级的有效值槽
     */
    [[nodiscard]] bool has_value(const DependencyProperty& prop,
                                 ValuePriority priority = ValuePriority::Local) const noexcept;

    // ── 属性变更事件订阅 ──────────────────────────────────────────────────

    /**
     * @brief 订阅此对象上任意属性的变更事件。
     *
     * 每当任意属性的生效值发生变更时，callback 均会被调用。
     * 返回的订阅 token 可传入 unsubscribe_property_changed() 取消订阅。
     *
     * @param callback  回调函数指针（不可为 nullptr）
     * @param user_data 用户自定义数据，原样传回回调
     * @return          订阅 token（非零；零表示订阅失败）
     */
    [[nodiscard]] uint32_t subscribe_property_changed(PropertyChangedCallbackFn callback,
                                                      void*                     user_data = nullptr);

    /**
     * @brief 取消属性变更事件订阅。
     *
     * @param token subscribe_property_changed() 返回的订阅 token
     */
    void unsubscribe_property_changed(uint32_t token);

protected:
    /**
     * @brief 属性生效值发生变更时的虚方法通知（在订阅者回调之前调用）。
     *
     * 子类可覆盖此方法响应具体属性变更，例如更新内部状态或触发重绘。
     * 默认实现为空。
     *
     * @param prop      发生变更的属性
     * @param old_value 变更前的生效值
     * @param new_value 变更后的生效值
     */
    virtual void on_property_changed(const DependencyProperty& prop,
                                     const core::Variant&      old_value,
                                     const core::Variant&      new_value);

    /**
     * @brief 标记布局测量失效（属性 affects_measure = true 时自动调用）。
     * 子类（如 UIElement）应覆盖此方法通知布局系统。
     */
    virtual void invalidate_measure();

    /**
     * @brief 标记布局排列失效（属性 affects_arrange = true 时自动调用）。
     */
    virtual void invalidate_arrange();

    /**
     * @brief 标记渲染失效（属性 affects_render = true 时自动调用）。
     */
    virtual void invalidate_render();

private:
    struct Impl;
    core::Pimpl<Impl> p_;
};

} // namespace mine::ui
