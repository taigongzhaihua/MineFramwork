/**
 * @file ObservableObject.h
 * @brief ObservableObject —— 属性变更通知基类。
 *
 * ObservableObject 是所有需要支持 UI 数据绑定的对象的轻量基类，
 * 实现了 INotifyPropertyChanged 接口，并提供辅助方法：
 *
 *   - `set(field, value, name)` — 在值变更时自动触发通知（模板）
 *   - `raise(name)` — 手动触发指定属性的变更通知（用于计算属性）
 *
 * 典型用法：
 * @code
 *   class Person : public ObservableObject {
 *   public:
 *       MINE_OBSERVABLE(mine::containers::InlineString, name, "")
 *       MINE_OBSERVABLE(int, age, 0)
 *
 *       // 计算属性（依赖 name 和 age，手动 raise）
 *       mine::containers::InlineString display_name() const noexcept {
 *           return name();  // 简化示例
 *       }
 *
 *   private:
 *       void refresh_display() noexcept {
 *           raise("display_name");
 *       }
 *   };
 * @endcode
 *
 * 注意：ObservableObject 不可拷贝、不可移动（订阅者持有此对象指针）。
 */

#pragma once

#include <type_traits>
#include <utility>

#include <mine/core/Function.h>
#include <mine/core/Pimpl.h>
#include <mine/core/StringView.h>
#include <mine/core/Variant.h>
#include <mine/mvvm/Api.h>
#include <mine/ui/binding/INotifyPropertyChanged.h>

namespace mine::mvvm {

/**
 * @brief 属性变更通知基类。
 *
 * 实现 INotifyPropertyChanged 接口，管理订阅者列表，
 * 并提供模板辅助方法 set<T>() 和 raise()。
 *
 * 不可拷贝、不可移动：订阅者通过裸指针引用此对象，
 * 所有权应通过 OwnedPtr<T> 或 new 管理。
 */
class MINE_MVVM_API ObservableObject : public mine::ui::INotifyPropertyChanged {
public:
    ObservableObject();
    ~ObservableObject() override;

    ObservableObject(const ObservableObject&)            = delete;
    ObservableObject& operator=(const ObservableObject&) = delete;
    ObservableObject(ObservableObject&&)                 = delete;
    ObservableObject& operator=(ObservableObject&&)      = delete;

    // ── INotifyPropertyChanged 实现 ────────────────────────────────────────

    /**
     * @brief 订阅属性变更事件。
     *
     * @param fn        变更时调用的回调
     * @param user_data 透传给回调的用户数据
     * @return 订阅令牌（用于取消订阅）
     */
    [[nodiscard]] uint32_t subscribe_property_changed(
        ChangedFn fn, void* user_data) noexcept override;

    /**
     * @brief 取消属性变更订阅。
     *
     * @param token subscribe_property_changed 返回的令牌
     */
    void unsubscribe_property_changed(uint32_t token) noexcept override;

    /**
     * @brief 按属性名读取当前值（属性反射接口）。
     *
     * 重写 INotifyPropertyChanged::get_property()。
     * MINE_OBSERVABLE 宏在对象构造时自动通过 register_property_getter()
     * 将每个属性的 getter 注册到内部查找表，无需手动实现。
     *
     * @param name 属性名（须与 MINE_OBSERVABLE 宏的 Name 参数完全一致）
     * @return 属性当前值；属性未注册时返回空 Variant
     */
    [[nodiscard]] core::Variant get_property(core::StringView name) const noexcept override;

protected:
    /**
     * @brief 属性 setter 辅助方法：值变更时自动触发通知。
     *
     * 若 T 支持 `operator==`，则仅在值实际变化时触发通知；
     * 否则无条件触发。
     *
     * @tparam T   属性类型（须满足 MoveAssignable）
     * @param field 存储属性值的字段（引用）
     * @param value 新值（按值传入，支持移动语义）
     * @param name  属性名（传给变更通知）
     * @return true 值已变更并触发通知；false 值未变更（未触发通知）
     */
    template<class T>
    bool set(T& field, T value, mine::core::StringView name) noexcept {
        // 若 T 支持 == 比较，仅在值不同时才通知
        if constexpr (requires(const T& a, const T& b) {
            { a == b } -> std::convertible_to<bool>;
        }) {
            if (field == value) return false;
        }
        field = std::move(value);
        raise(name);
        return true;
    }

    /**
     * @brief 手动触发属性变更通知。
     *
     * 适用于计算属性（由其他属性派生而来，无自身字段）。
     *
     * @param name 属性名（空字符串表示所有属性均已变更）
     */
    void raise(mine::core::StringView name) noexcept;

    /**
     * @brief 注册属性 getter 到内部反射表。
     *
     * 由 MINE_OBSERVABLE 宏在成员初始化器中自动调用，无需手动使用。
     * 注册后，get_property(name) 将调用对应 getter 返回属性当前值。
     *
     * @param name   属性名（须与 MINE_OBSERVABLE 宏的 Name 参数完全一致）
     * @param getter 返回属性当前值的无参函数对象
     */
    void register_property_getter(
        mine::core::StringView                        name,
        mine::core::Function<mine::core::Variant()>   getter) noexcept;

private:
    struct Impl;
    mine::core::Pimpl<Impl> p_;
};

} // namespace mine::mvvm
