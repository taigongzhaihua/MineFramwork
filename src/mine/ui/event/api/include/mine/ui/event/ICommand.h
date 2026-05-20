/**
 * @file ICommand.h
 * @brief 命令接口 —— MVVM 命令模式的抽象定义。
 *
 * ICommand 提供命令的标准接口，遵循 WPF/MVVM 命令模式：
 *   - can_execute()：判断命令当前是否可执行（用于 UI 元素的 IsEnabled 绑定）
 *   - execute()：执行命令逻辑
 *   - subscribe_can_execute_changed()：订阅可执行状态变化通知（供 UI 重新查询 can_execute）
 *
 * 典型实现：RelayCommand（在同文件内提供）
 *
 * 使用示例（ViewModel 侧）：
 * @code
 *   class MyViewModel {
 *   public:
 *       MyViewModel()
 *           : save_command_{
 *               [this](const Variant&) { do_save(); },
 *               [this](const Variant&) { return is_dirty_; }
 *             }
 *       {}
 *
 *       ICommand* save_command() { return &save_command_; }
 *
 *   private:
 *       RelayCommand save_command_;
 *       bool is_dirty_ = false;
 *   };
 * @endcode
 */

#pragma once

#include <cstdint>
#include <mine/core/Variant.h>
#include <mine/ui/event/Api.h>

namespace mine::ui {

/**
 * @brief 命令抽象接口。
 *
 * UI 框架通过此接口与 ViewModel 的命令逻辑解耦：
 *   - UI 元素调用 can_execute() 决定是否启用
 *   - UI 元素在用户交互时调用 execute()
 *   - UI 元素订阅 can_execute_changed 以在状态变化时刷新
 */
class MINE_UI_EVENT_API ICommand {
public:
    virtual ~ICommand() = default;

    // ── 命令执行状态 ───────────────────────────────────────────────────────

    /**
     * @brief 判断当前参数下命令是否可执行。
     *
     * @param param 可选命令参数（无参数时传 Variant::null()）
     * @return true 命令可执行；false 命令被禁用
     */
    [[nodiscard]] virtual bool can_execute(
        const core::Variant& param) const noexcept = 0;

    /**
     * @brief 执行命令。
     *
     * 实现者应确保此函数不会抛出异常。
     * 建议在调用前先通过 can_execute() 检查是否可执行。
     *
     * @param param 可选命令参数（与 can_execute 保持一致）
     */
    virtual void execute(const core::Variant& param) noexcept = 0;

    // ── 可执行状态变化通知 ─────────────────────────────────────────────────

    /**
     * @brief 可执行状态变化通知处理器类型。
     * @param sender    触发通知的命令对象
     * @param user_data 注册时传入的用户自定义数据
     */
    using ChangedFn = void(*)(ICommand* sender, void* user_data);

    /**
     * @brief 订阅 can_execute 可能发生变化的通知。
     *
     * 每次 ViewModel 内部状态改变导致 can_execute() 结果可能变化时，
     * 命令实现应触发此通知，UI 框架重新调用 can_execute() 以刷新状态。
     *
     * @param fn        通知回调函数
     * @param user_data 回调时原样传回的用户数据
     * @return 订阅 token（用于取消订阅）
     */
    [[nodiscard]] virtual uint32_t subscribe_can_execute_changed(
        ChangedFn fn, void* user_data) noexcept = 0;

    /**
     * @brief 取消 can_execute 变化通知订阅。
     *
     * @param token subscribe_can_execute_changed() 返回的 token
     */
    virtual void unsubscribe_can_execute_changed(
        uint32_t token) noexcept = 0;
};

} // namespace mine::ui
