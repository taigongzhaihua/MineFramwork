/**
 * @file Command.h
 * @brief MINE_COMMAND 宏 —— 一行声明可绑定命令属性。
 *
 * 宏展开示例：
 * @code
 *   // 声明（在 ViewModelBase 子类内部）：
 *   MINE_COMMAND(increment_cmd)
 *
 *   // 等价于：
 *   public:
 *       mine::ui::RelayCommand increment_cmd_;
 *   private:
 *       // 构造时自动注册 getter，使 get_property("increment_cmd") 返回 Variant{ICommand*}
 *       bool mine_cmd_reg_increment_cmd_{ (register_property_getter("increment_cmd",
 *           [this]() noexcept -> mine::core::Variant {
 *               return mine::core::Variant{ static_cast<mine::ui::ICommand*>(&increment_cmd_) };
 *           }), true) };
 *   public:
 * @endcode
 *
 * 用法示例（ViewModel 侧）：
 * @code
 *   class MyViewModel : public mine::mvvm::ViewModelBase {
 *   public:
 *       MINE_OBSERVABLE(bool, is_dirty, false)
 *       MINE_COMMAND(save_cmd)
 *
 *       MyViewModel()
 *           : save_cmd_{
 *               [this](const mine::core::Variant&) noexcept { do_save(); },
 *               [this](const mine::core::Variant&) noexcept -> bool { return is_dirty(); }
 *             }
 *       {}
 *   };
 * @endcode
 *
 * 视图层（View 侧）通过 Command 绑定将按钮连接到命令：
 * @code
 *   // 完全等价于 WPF 的 btn.SetBinding(Button.CommandProperty, new Binding("save_cmd"))
 *   save_btn_.set_binding(mine::ui::Button::CommandProperty, "save_cmd");
 * @endcode
 *
 * Button 收到 CommandProperty 变更后自动完成：
 *   1. 订阅 ICommand::can_execute_changed → 实时同步 is_enabled_ 禁用状态
 *   2. 用户点击时调用 ICommand::execute(CommandParameterProperty 当前值)
 * View 层完全无需手写 add_handler 路由桩或调用 vm.cmd.execute()。
 *
 * 注意事项：
 *   - 宏展开后命令字段名为 `Name##_`（末尾加下划线），在构造列表中按此名初始化
 *   - 命令成员初始化顺序早于 mine_cmd_reg_* 注册字段（由声明顺序保证），
 *     lambda 仅捕获 this，调用时 this->Name##_ 必然已完全构造，安全
 *   - 若命令状态改变（如 can_execute 结果变化），需手动调用 Name##_.notify_can_execute_changed()
 *     通知绑定的 Button 刷新 is_enabled_
 *   - 使用宏的类必须继承自 ObservableObject（或 ViewModelBase），
 *     以确保 register_property_getter() 方法可用
 */

#pragma once

#include <mine/core/Variant.h>
#include <mine/ui/event/ICommand.h>
#include <mine/ui/event/RelayCommand.h>

/**
 * @def MINE_COMMAND(Name)
 * @brief 在 ViewModelBase 子类内一行声明可绑定的命令属性。
 *
 * @param Name 命令名（遵循小写 snake_case 规范，字段名为 Name##_）
 *
 * 生成成员：
 *   - 公开字段：`mine::ui::RelayCommand Name##_`（在构造列表中注入 execute/can_execute）
 *   - 私有字段：`mine_cmd_reg_##Name##_`（哑元 bool，构造时自动注册命令属性 getter）
 *
 * 注册机制：
 *   `mine_cmd_reg_##Name##_` 的成员初始化器在对象构造期间（Name##_ 之后）调用
 *   ObservableObject::register_property_getter(#Name, getter)，
 *   将命令名映射到返回 `Variant{ICommand*}` 的 lambda，
 *   使 get_property(#Name) 可按名反射读取命令指针。
 *   视图层通过 set_binding(Button::CommandProperty, "cmd_name") 消费此能力。
 */
#define MINE_COMMAND(Name)                                                             \
public:                                                                                \
    mine::ui::RelayCommand Name##_;                                                    \
private:                                                                               \
    /* 构造时自动将命令指针 getter 注册到 ObservableObject 内部反射表 */               \
    bool mine_cmd_reg_##Name##_{ (this->register_property_getter(                     \
        #Name,                                                                         \
        [this]() noexcept -> mine::core::Variant {                                    \
            return mine::core::Variant{                                                \
                static_cast<mine::ui::ICommand*>(&(this->Name##_)) };                 \
        }), true) };                                                                   \
public:
