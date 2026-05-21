/**
 * @file VisualStateManager.cpp
 * @brief mine::ui::style::VisualStateManager 实现。
 *
 * 状态切换流程（go_to_state）：
 *   1. 目标状态须在 states_ 中已注册，否则返回 false
 *   2. 目标状态与当前状态相同时返回 false（无副作用）
 *   3. 更新 current_state_
 *   4. 若已关联 Style，调用 style_->apply_state() 以 StyleTrigger(30) 写入属性
 *   5. 返回 true
 *
 * M2 阶段说明：
 *   - add_transition 仅记录 from/to 对；切换时直接跳变（无动画）
 *   - use_transitions 参数保留但当前无实际效果（Task #17 补充 Storyboard 支持）
 */

#include <mine/ui/style/VisualStateManager.h>
#include <mine/ui/style/Style.h>

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
    // M2 阶段：仅记录 from/to，不包含动画配置（Task #17 扩展）
    VisualStateTransition tr;
    tr.from = containers::InlineString{from.data(), from.size()};
    tr.to   = containers::InlineString{to.data(), to.size()};
    transitions_.push_back(std::move(tr));
}

// ─────────────────────────────────────────────────────────────────────────────
// 状态切换
// ─────────────────────────────────────────────────────────────────────────────

bool VisualStateManager::go_to_state(core::StringView state_name,
                                     bool             /*use_transitions*/) noexcept
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

    // 3. 更新当前状态
    current_state_ = containers::InlineString{state_name.data(), state_name.size()};

    // 4. 若已关联样式，应用对应状态的属性 setter（StyleTrigger 优先级）
    //    owner_ 初始化时非空（由构造函数保证），此处无需防御检查
    if (style_ && owner_) {
        style_->apply_state(*owner_, state_name);
    }

    // 5. M2 阶段：有过渡记录时仍立即跳变，Task #17 补充 Storyboard 动画支持

    return true;
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
