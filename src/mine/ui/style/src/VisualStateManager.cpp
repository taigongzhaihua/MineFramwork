/**
 * @file VisualStateManager.cpp
 * @brief mine::ui::style::VisualStateManager 实现。
 *
 * 状态切换流程（go_to_state）：
 *   1. 目标状态须在 states_ 中已注册，否则返回 false
 *   2. 目标状态与当前状态相同时返回 false（无副作用）
 *   3. 停止并清理当前所有活跃 Storyboard。
 *   4. 查找匹配的过渡记录（含 configure 字段）：
 *      - 若 use_transitions=true 且找到带动画的过渡：
 *          capture_from_values → apply_state → resolve_and_begin → 存入 active_storyboards_
 *      - 否则：直接 apply_state（立即跳变）
 *   5. 更新 current_state_，返回 true
 *
 * Task #17 新增：
 *   - add_transition（带 configure_fn）：附带 Storyboard 配置函数的过渡重载
 *   - tick_animations(dt)：每帧推进活跃 Storyboard，完成时自动 stop() + 移除
 */

#include <mine/ui/style/VisualStateManager.h>
#include <mine/ui/style/Style.h>
#include <mine/ui/animation/Storyboard.h>

namespace mine::ui::style {

// ─────────────────────────────────────────────────────────────────────────────
// 构造
// ─────────────────────────────────────────────────────────────────────────────

VisualStateManager::VisualStateManager(ui::DependencyObject& owner) noexcept
    : owner_(&owner)
{}

// ─────────────────────────────────────────────────────────────────────────────
// 状态注册
// ─────────────────────────────────────────────────────────────────────────────

void VisualStateManager::define_state(core::StringView name) noexcept
{
    // 重复注册同名状态：检查后跳过（幂等）
    for (const auto& s : states_) {
        if (s == name) {
            return;
        }
    }
    states_.push_back(containers::InlineString{name.data(), name.size()});
}

void VisualStateManager::add_transition(core::StringView from,
                                        core::StringView to) noexcept
{
    // 无动画过渡：切换时立即跳变
    VisualStateTransition tr;
    tr.from = containers::InlineString{from.data(), from.size()};
    tr.to   = containers::InlineString{to.data(), to.size()};
    // tr.configure 为空（默认构造）
    transitions_.push_back(std::move(tr));
}

void VisualStateManager::add_transition(
    core::StringView                              from,
    core::StringView                              to,
    core::Function<void(animation::Storyboard&)> configure_fn) noexcept
{
    // 带 Storyboard 配置函数的过渡：go_to_state 将创建 Storyboard 并播放动画
    VisualStateTransition tr;
    tr.from      = containers::InlineString{from.data(), from.size()};
    tr.to        = containers::InlineString{to.data(), to.size()};
    tr.configure = std::move(configure_fn);
    transitions_.push_back(std::move(tr));
}

// ─────────────────────────────────────────────────────────────────────────────
// 状态切换
// ─────────────────────────────────────────────────────────────────────────────

bool VisualStateManager::go_to_state(core::StringView state_name,
                                     bool             use_transitions) noexcept
{
    // 1. 目标状态必须是已注册的状态（防止拼写错误导致静默失败）
    bool found = false;
    for (const auto& s : states_) {
        if (s == state_name) {
            found = true;
            break;
        }
    }
    if (!found) {
        return false;
    }

    // 2. 若已是当前状态，无需切换
    if (current_state_ == state_name) {
        return false;
    }

    // 3. 停止并清理当前所有活跃 Storyboard（中断未完成的动画）
    for (auto& sb : active_storyboards_) {
        if (sb) {
            sb->stop();
        }
    }
    active_storyboards_.clear();

    // 4. 查找匹配的过渡记录（呈先精确 from 匹配，再匹配通配 "*"）
    const VisualStateTransition* matched_tr = nullptr;
    if (use_transitions) {
        const core::StringView cur_state_sv{current_state_.data(), current_state_.size()};

        // 优先查找具有 configure 的精确匹配（from == 当前状态）
        for (const auto& tr : transitions_) {
            if (tr.configure && (tr.to == state_name)) {
                const bool exact_from = (tr.from == cur_state_sv);
                if (exact_from) {
                    matched_tr = &tr;
                    break;
                }
            }
        }

        // 如未找到精确匹配，查找通配 "*" 匹配
        if (!matched_tr) {
            for (const auto& tr : transitions_) {
                if (tr.configure && (tr.to == state_name)) {
                    const bool wildcard_from = tr.from.empty() || (tr.from == "*");
                    if (wildcard_from) {
                        matched_tr = &tr;
                        break;
                    }
                }
            }
        }
    }

    if (matched_tr) {
        // 带动画的过渡路径：
        //   a. 创建 Storyboard 并调用 configure 注册要驱动的属性
        //   b. capture_from_values：在 StyleTrigger 写入前采样起始值
        //   c. apply_state：将新状态的 StyleTrigger 写入宿主属性
        //   d. resolve_and_begin：解析终止值，以 Animation 优先级写入起始值
        //   e. 加入 active_storyboards_
        auto sb = core::make_owned<animation::Storyboard>();
        matched_tr->configure(*sb);        // 注册动画目标和参数
        sb->capture_from_values();         // 采样起始值（StyleTrigger 写入前）

        // 应用新状态的 StyleTrigger
        if (style_ && owner_) {
            style_->apply_state(*owner_, state_name);
        }

        sb->resolve_and_begin();           // 解析终止值，写入 Animation=from
        active_storyboards_.push_back(std::move(sb));
    } else {
        // 无动画的过渡路径：直接应用新状态的 StyleTrigger（立即跳变）
        if (style_ && owner_) {
            style_->apply_state(*owner_, state_name);
        }
    }

    // 5. 更新当前状态
    current_state_ = containers::InlineString{state_name.data(), state_name.size()};

    return true;
}

bool VisualStateManager::tick_animations(float dt) noexcept
{
    if (active_storyboards_.empty()) {
        return false;
    }

    // 向前压缩策略：将未完成的 Storyboard 前移，完成的停止并允许析构
    // SmallVector 无 erase 方法，使用 write/read 双指针就地压缩
    const size_t n = active_storyboards_.size();
    size_t write = 0;

    for (size_t read = 0; read < n; ++read) {
        auto& sb = active_storyboards_[read];
        if (!sb) {
            continue; // 空指针跳过（正常情况下不应出现）
        }

        const bool done = sb->tick(dt);
        if (done) {
            sb->stop(); // 清除 Animation 优先级，属性退回到 StyleTrigger 层
            // active_storyboards_[read] 将在后续覆盖或 pop_back 时由 OwnedPtr 析构
        } else {
            // 未完成：前移到 write 位置（跳过已完成的槽）
            if (write != read) {
                active_storyboards_[write] = std::move(sb);
                // sb（原 read 位置）现为已 move-from 的空 OwnedPtr，pop_back 时安全析构
            }
            ++write;
        }
    }

    // 截断：移除已完成和已 move-from 的尾部元素（pop_back 会正确析构各 OwnedPtr）
    while (active_storyboards_.size() > write) {
        active_storyboards_.pop_back();
    }

    return !active_storyboards_.empty();
}

// ─────────────────────────────────────────────────────────────────────────────
// 查询
// ─────────────────────────────────────────────────────────────────────────────

core::StringView VisualStateManager::current_state() const noexcept
{
    return core::StringView{current_state_.data(), current_state_.size()};
}

bool VisualStateManager::has_state(core::StringView name) const noexcept
{
    for (const auto& s : states_) {
        if (s == name) {
            return true;
        }
    }
    return false;
}

bool VisualStateManager::has_transition(core::StringView from,
                                        core::StringView to) const noexcept
{
    for (const auto& tr : transitions_) {
        // from 为 "*" 或空字符串时视为通配符，匹配任意源状态
        const bool from_match = (tr.from == "*") ||
                                tr.from.empty()  ||
                                (tr.from == from);
        const bool to_match   = (tr.to == to);
        if (from_match && to_match) {
            return true;
        }
    }
    return false;
}

// ─────────────────────────────────────────────────────────────────────────────
// 关联样式
// ─────────────────────────────────────────────────────────────────────────────

void VisualStateManager::set_style(const Style* style) noexcept
{
    style_ = style;
}

const Style* VisualStateManager::attached_style() const noexcept
{
    return style_;
}

}  // namespace mine::ui::style
