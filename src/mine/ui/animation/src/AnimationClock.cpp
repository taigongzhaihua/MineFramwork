/**
 * @file AnimationClock.cpp
 * @brief AnimationClock 全局动画时钟实现。
 *
 * 使用"尾部覆盖删除"（swap-and-pop）维护活跃注册列表：
 *   - 删除一项时将尾部元素复制到当前位置，再 pop_back，O(1) 且避免 O(n) 移位；
 *   - 顺序不保证，但动画时钟不依赖顺序。
 */

#include <mine/ui/animation/AnimationClock.h>

namespace mine::ui::animation {

// ─────────────────────────────────────────────────────────────────────────────
// 单例访问
// ─────────────────────────────────────────────────────────────────────────────

AnimationClock& AnimationClock::instance() noexcept
{
    static AnimationClock s_instance;
    return s_instance;
}

// ─────────────────────────────────────────────────────────────────────────────
// 注册管理
// ─────────────────────────────────────────────────────────────────────────────

void AnimationClock::register_animation(void* handle, TickFn tick_fn) noexcept
{
    // 幂等：handle 已存在则更新 tick_fn，防止同一控件多次注册产生重复项
    for (Entry& e : entries_) {
        if (e.handle == handle) {
            e.tick_fn = tick_fn;
            return;
        }
    }
    entries_.push_back(Entry{handle, tick_fn});
}

void AnimationClock::unregister_animation(void* handle) noexcept
{
    for (int i = static_cast<int>(entries_.size()) - 1; i >= 0; --i) {
        if (entries_[static_cast<size_t>(i)].handle == handle) {
            // 尾部覆盖删除：O(1)，不保证顺序（动画时钟不需要顺序）
            entries_[static_cast<size_t>(i)] = entries_.back();
            entries_.pop_back();
            return;
        }
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// 驱动接口
// ─────────────────────────────────────────────────────────────────────────────

bool AnimationClock::tick_all(float dt) noexcept
{
    // 防止 tick_all 期间回调再次调用 tick_all
    if (ticking_) {
        return !entries_.empty();
    }
    ticking_ = true;

    // 遍历时调用每个 tick_fn；返回 false 的项立即用尾部覆盖并移除
    int i = 0;
    while (i < static_cast<int>(entries_.size())) {
        const Entry& e = entries_[static_cast<size_t>(i)];
        const bool still_active = e.tick_fn(e.handle, dt);
        if (!still_active) {
            // 动画完成：尾部覆盖删除（不递增 i，需重新检查当前位置）
            entries_[static_cast<size_t>(i)] = entries_.back();
            entries_.pop_back();
        } else {
            ++i;
        }
    }

    ticking_ = false;
    return !entries_.empty();
}

bool AnimationClock::has_active() const noexcept
{
    return !entries_.empty();
}

}  // namespace mine::ui::animation
