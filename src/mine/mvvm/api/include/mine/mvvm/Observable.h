/**
 * @file Observable.h
 * @brief MINE_OBSERVABLE 宏 —— 一行声明可观察属性。
 *
 * 宏展开示例：
 * @code
 *   // 声明（在类内部）：
 *   MINE_OBSERVABLE(int, count, 0)
 *
 *   // 等价于：
 *   private:
 *       int mine_prop_count_{ 0 };
 *   public:
 *       [[nodiscard]] const int& count() const noexcept { return mine_prop_count_; }
 *       void set_count(int value) noexcept {
 *           this->template set<int>(mine_prop_count_, std::move(value), "count");
 *       }
 * @endcode
 *
 * 要求：使用宏的类必须继承自 ObservableObject（或 ViewModelBase），
 * 以确保 set() 方法可用。
 *
 * @note 宏生成的 setter 命名为 `set_<Name>`，getter 命名为 `<Name>`（const&）。
 *       属性名字符串（用于通知）与 Name 参数完全一致。
 */

#pragma once

#include <utility>

/**
 * @def MINE_OBSERVABLE(Type, Name, ...)
 * @brief 在 ObservableObject 子类内一行声明可观察属性。
 *
 * @param Type 属性类型（可含模板参数，如 mine::containers::InlineString）
 * @param Name 属性名（遵循小写 snake_case 规范）
 * @param ...  属性初始值（传给字段的默认初始化表达式）
 *
 * 生成成员：
 *   - 私有字段：`mine_prop_<Name>_`（类型为 Type）
 *   - 公开 getter：`const Type& <Name>() const noexcept`
 *   - 公开 setter：`void set_<Name>(Type value) noexcept`
 *     setter 通过 ObservableObject::set() 实现，值不变时不触发通知
 */
#define MINE_OBSERVABLE(Type, Name, ...)                                          \
private:                                                                          \
    Type mine_prop_##Name##_{ __VA_ARGS__ };                                      \
public:                                                                           \
    [[nodiscard]] const Type& Name() const noexcept {                             \
        return mine_prop_##Name##_;                                               \
    }                                                                             \
    void set_##Name(Type value) noexcept {                                        \
        this->template set<Type>(mine_prop_##Name##_, ::std::move(value), #Name); \
    }
