/**
 * @file INotifyPropertyChanged.h
 * @brief 属性变更通知接口（等价于 C# INotifyPropertyChanged）。
 *
 * ViewModel 层的对象实现此接口，以允许 UI 绑定系统订阅属性变更事件。
 * 配合 BindingExpression 的 PropertyDependency 使用：
 *   - ViewModel 在属性 setter 内调用通知方法
 *   - BindingExpression 订阅通知，在指定属性变更时重新求值
 *
 * 设计原则：
 *   - 不依赖 RTTI 或异常
 *   - 回调函数为原始函数指针 + void* user_data（避免 std::function）
 *   - 通知时传递属性名，订阅方可按名过滤（空名字符串表示所有属性）
 */

#pragma once

#include <cstdint>
#include <mine/core/StringView.h>
#include <mine/ui/binding/Api.h>

namespace mine::ui {

/**
 * @brief 属性变更通知接口。
 *
 * 所有需要与 UI 绑定系统交互的 ViewModel 均应实现此接口。
 * 实现类（通常为 ViewModelBase）负责维护订阅者列表并在属性变更时广播通知。
 *
 * 线程安全：
 *   属性变更通知和订阅操作应在同一线程执行（UI 线程）。
 */
class MINE_UI_BINDING_API INotifyPropertyChanged {
public:
    virtual ~INotifyPropertyChanged() = default;

    /**
     * @brief 属性变更回调函数类型。
     *
     * @param sender    触发通知的 INotifyPropertyChanged 实例
     * @param prop_name 发生变更的属性名（与注册时的属性名一致）
     * @param user_data 订阅时传入的用户数据指针
     */
    using ChangedFn = void (*)(INotifyPropertyChanged* sender,
                               core::StringView         prop_name,
                               void*                    user_data);

    /**
     * @brief 订阅属性变更事件。
     *
     * @param fn        属性变更时调用的回调函数
     * @param user_data 透传给回调的用户数据指针（可为 nullptr）
     * @return 订阅令牌（用于 unsubscribe_property_changed 取消订阅）
     */
    [[nodiscard]] virtual uint32_t subscribe_property_changed(
        ChangedFn fn, void* user_data) noexcept = 0;

    /**
     * @brief 取消属性变更事件订阅。
     *
     * @param token subscribe_property_changed 返回的令牌；无效令牌时为空操作
     */
    virtual void unsubscribe_property_changed(uint32_t token) noexcept = 0;
};

} // namespace mine::ui
