/**
 * @file RelayCommand.h
 * @brief RelayCommand —— 基于函数对象的 ICommand 实现。
 *
 * RelayCommand 以 Pimpl 模式实现 ICommand 接口，适用于跨 DLL 导出场景：
 *   - 接受任意可调用对象（lambda、函数指针）作为 execute/can_execute 逻辑
 *   - 内部用 core::Function<> 存储（SBO 32 字节，move-only）
 *   - can_execute_changed 通知采用 subscribe/unsubscribe + token 模式
 *   - 支持 notify_can_execute_changed() 手动触发通知
 *
 * 典型用法（构造时注入 lambda）：
 * @code
 *   // 无参命令（始终可执行）
 *   RelayCommand cmd{[this](const Variant&) { do_something(); }};
 *
 *   // 带 CanExecute 条件的命令
 *   RelayCommand cmd{
 *       [this](const Variant&) { save(); },
 *       [this](const Variant&) { return has_unsaved_changes_; }
 *   };
 *
 *   // 通知状态改变（让绑定的 UI 重新查询 can_execute）
 *   has_unsaved_changes_ = true;
 *   cmd.notify_can_execute_changed();
 * @endcode
 */

#pragma once

#include <mine/core/Function.h>
#include <mine/core/Pimpl.h>
#include <mine/core/Variant.h>
#include <mine/ui/event/Api.h>
#include <mine/ui/event/ICommand.h>

namespace mine::ui {

/**
 * @brief 基于函数对象的 ICommand 具体实现。
 *
 * 以 Pimpl 模式封装，公共头不暴露 Vector 等容器实现细节，
 * 确保跨 DLL 边界的 ABI 稳定性。
 */
class MINE_UI_EVENT_API RelayCommand final : public ICommand {
public:
    /// 执行函数类型（move-only，SBO 优化）
    using ExecuteFn    = core::Function<void(const core::Variant&)>;
    /// 可执行判断函数类型（move-only，SBO 优化）
    using CanExecuteFn = core::Function<bool(const core::Variant&)>;

    /**
     * @brief 构造 RelayCommand。
     *
     * @param execute     命令执行逻辑（必须非空）
     * @param can_execute 可执行判断逻辑（可为空，空时始终返回 true）
     */
    explicit RelayCommand(ExecuteFn    execute,
                          CanExecuteFn can_execute = {}) noexcept;

    ~RelayCommand() override;

    // move-only（Pimpl 语义）
    RelayCommand(RelayCommand&&) noexcept;
    RelayCommand& operator=(RelayCommand&&) noexcept;

    RelayCommand(const RelayCommand&)            = delete;
    RelayCommand& operator=(const RelayCommand&) = delete;

    // ── ICommand 接口实现 ─────────────────────────────────────────────────

    /**
     * @brief 调用构造时传入的 can_execute 函数。
     *
     * 若构造时 can_execute 为空，始终返回 true。
     */
    [[nodiscard]] bool can_execute(
        const core::Variant& param) const noexcept override;

    /**
     * @brief 调用构造时传入的 execute 函数。
     */
    void execute(const core::Variant& param) noexcept override;

    /**
     * @brief 订阅 can_execute 变化通知。
     * @return 订阅 token（传给 unsubscribe_can_execute_changed 取消）
     */
    [[nodiscard]] uint32_t subscribe_can_execute_changed(
        ChangedFn fn, void* user_data) noexcept override;

    /**
     * @brief 取消订阅。
     * @param token 之前 subscribe_can_execute_changed 返回的 token
     */
    void unsubscribe_can_execute_changed(uint32_t token) noexcept override;

    // ── 额外 API ──────────────────────────────────────────────────────────

    /**
     * @brief 手动触发 can_execute 变化通知。
     *
     * 当 ViewModel 内部状态变化导致 can_execute 结果改变时，
     * 调用此方法以通知已订阅的 UI 元素重新查询 can_execute()。
     */
    void notify_can_execute_changed() noexcept;

private:
    struct Impl;
    core::Pimpl<Impl> p_;
};

} // namespace mine::ui
