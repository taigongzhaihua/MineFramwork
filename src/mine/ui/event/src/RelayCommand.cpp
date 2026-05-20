/**
 * @file RelayCommand.cpp
 * @brief RelayCommand Pimpl 实现。
 *
 * Impl 结构体存储：
 *   - execute_fn_    : Function<void(const Variant&)>，命令执行逻辑
 *   - can_exec_fn_   : Function<bool(const Variant&)>，可执行判断（可为空）
 *   - subscribers_   : Vector<Sub>，can_execute_changed 订阅记录列表
 *   - next_token_    : uint32_t，下一次分配的 token 值（从 1 开始，0 表示无效）
 *
 * 订阅机制：
 *   subscribe_can_execute_changed() 分配递增 token，加入 subscribers_ 列表。
 *   unsubscribe_can_execute_changed(token) 线性扫描找到匹配记录，通过将其移至末尾并
 *   pop_back() 删除（无序删除，O(n) 但列表通常极短）。
 *   notify_can_execute_changed() 遍历 subscribers_ 并调用每个回调。
 *
 * 注意：
 *   notify_can_execute_changed() 在遍历期间不支持重入订阅/取消（M1.1 阶段足够）。
 */

#include <mine/ui/event/RelayCommand.h>

#include <utility>  // std::move, std::swap

#include <mine/core/Assert.h>
#include <mine/core/Allocator.h>
#include <mine/core/Pimpl.h>
#include <mine/containers/Vector.h>

namespace mine::ui {

// ────────────────────────────────────────────────────────────────────────────
// Impl 定义
// ────────────────────────────────────────────────────────────────────────────

struct RelayCommand::Impl {
    /// 命令执行逻辑（必须非空）
    ExecuteFn    execute_fn;
    /// 可执行判断逻辑（为空时 can_execute() 始终返回 true）
    CanExecuteFn can_exec_fn;

    /// can_execute_changed 的单个订阅记录
    struct Sub {
        ICommand::ChangedFn fn        = nullptr;  ///< 回调函数
        void*               user_data = nullptr;  ///< 用户数据
        uint32_t            token     = 0u;        ///< 分配的 token
    };

    /// 所有当前订阅记录（列表通常很短，线性扫描足够）
    containers::Vector<Sub> subscribers;

    /// 下一个 token 值（从 1 开始，0 保留为"无效"标记）
    uint32_t next_token = 1u;
};

// ────────────────────────────────────────────────────────────────────────────
// RelayCommand 实现
// ────────────────────────────────────────────────────────────────────────────

RelayCommand::RelayCommand(ExecuteFn    execute,
                            CanExecuteFn can_execute) noexcept
    : p_{core::make_pimpl<Impl>()}
{
    p_->execute_fn  = std::move(execute);
    p_->can_exec_fn = std::move(can_execute);
}

RelayCommand::~RelayCommand() = default;

RelayCommand::RelayCommand(RelayCommand&&) noexcept            = default;
RelayCommand& RelayCommand::operator=(RelayCommand&&) noexcept = default;

// ── ICommand 接口实现 ────────────────────────────────────────────────────────

bool RelayCommand::can_execute(const core::Variant& param) const noexcept {
    // 若未提供判断函数，始终返回 true
    if (!p_->can_exec_fn) {
        return true;
    }
    return p_->can_exec_fn(param);
}

void RelayCommand::execute(const core::Variant& param) noexcept {
    MINE_ASSERT_MSG(static_cast<bool>(p_->execute_fn),
        "RelayCommand::execute 调用时 execute_fn 为空");
    if (p_->execute_fn) {
        p_->execute_fn(param);
    }
}

uint32_t RelayCommand::subscribe_can_execute_changed(
    ChangedFn fn,
    void*     user_data) noexcept
{
    MINE_ASSERT(fn != nullptr);
    const uint32_t token = p_->next_token++;
    Impl::Sub sub;
    sub.fn        = fn;
    sub.user_data = user_data;
    sub.token     = token;
    p_->subscribers.push_back(sub);
    return token;
}

void RelayCommand::unsubscribe_can_execute_changed(uint32_t token) noexcept {
    auto& subs = p_->subscribers;
    for (size_t i = 0u; i < subs.size(); ++i) {
        if (subs[i].token == token) {
            // 将末尾元素覆盖当前位置，然后 pop（无序删除，O(n)，N 极小）
            subs[i] = subs[subs.size() - 1u];
            subs.pop_back();
            return;
        }
    }
    // token 不存在时静默忽略（可能是重复取消订阅）
}

// ── 额外 API ─────────────────────────────────────────────────────────────────

void RelayCommand::notify_can_execute_changed() noexcept {
    // 遍历并调用所有已订阅的回调
    // 注意：遍历期间不支持重入修改 subscribers 列表
    for (const Impl::Sub& sub : p_->subscribers) {
        sub.fn(this, sub.user_data);
    }
}

} // namespace mine::ui
