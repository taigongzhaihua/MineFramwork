/**
 * @file AnimationClock.h
 * @brief AnimationClock —— 全局动画时钟，集中管理所有活跃动画的 tick 驱动。
 *
 * 控件（如 Button）启动动画时向 AnimationClock 注册一个静态 tick 回调；
 * 应用层（或框架层渲染调度器）定时调用 tick_all(dt) 推进全部活跃动画并触发重绘。
 * 回调返回 false（表示动画已完成）后自动从列表中移除，无需手动注销。
 *
 * 生命周期规则：
 *   - 控件析构时 **必须** 调用 unregister_animation(this)，防止悬空指针。
 *   - 同一 handle 重复注册视为刷新（幂等），不会产生重复项。
 *   - tick_all 可在 tick 回调内部安全调用 register/unregister（防重入保护）。
 *
 * 典型用法：
 * @code
 *   // 控件启动动画时注册（this 作为唯一 handle）
 *   AnimationClock::instance().register_animation(this, &MyControl::s_tick);
 *
 *   // 应用层定时驱动（~60fps）
 *   const bool any = AnimationClock::instance().tick_all(16.0f / 1000.0f);
 *
 *   // 查询是否还有活跃动画（用于决定是否继续驱动定时器）
 *   if (!AnimationClock::instance().has_active()) { stop_timer(); }
 * @endcode
 */

#pragma once

#include <mine/ui/animation/Api.h>
#include <mine/containers/SmallVector.h>

namespace mine::ui::animation {

/**
 * @brief 全局动画时钟（Meyer's Singleton，非线程安全，仅在主线程使用）。
 *
 * 使用函数指针（而非 std::function）以避免堆分配，与框架其余部分
 * （路由事件回调、模板 changed 回调）保持一致的低开销风格。
 */
class MINE_UI_ANIMATION_API AnimationClock {
public:
    /**
     * @brief tick 回调函数指针类型。
     *
     * @param user_data 注册时传入的 handle（通常是控件的 this 指针）
     * @param dt        帧时长（秒），必须 >= 0
     * @return true     仍有活跃动画，下一帧继续回调
     * @return false    所有动画已完成，AnimationClock 将自动移除此注册项
     */
    using TickFn = bool (*)(void* user_data, float dt) noexcept;

    // ── 单例访问 ──────────────────────────────────────────────────────────────

    /// 获取全局唯一实例（主线程调用，无线程安全要求）
    [[nodiscard]] static AnimationClock& instance() noexcept;

    // ── 注册管理 ──────────────────────────────────────────────────────────────

    /**
     * @brief 注册动画 tick 回调（幂等：handle 已存在则更新 tick_fn）。
     *
     * @param handle  唯一标识符（通常传入控件的 this 指针，不能为 nullptr）
     * @param tick_fn tick 回调（不能为 nullptr）
     */
    void register_animation(void* handle, TickFn tick_fn) noexcept;

    /**
     * @brief 注销 tick 回调（控件析构时必须调用，防止悬空指针）。
     *
     * 若 handle 不存在则静默忽略（幂等）。
     *
     * @param handle 注册时使用的标识符
     */
    void unregister_animation(void* handle) noexcept;

    // ── 驱动接口 ──────────────────────────────────────────────────────────────

    /**
     * @brief 推进所有活跃动画一帧，自动移除回调返回 false 的项。
     *
     * 内部防止重入（tick_all 期间调用 register/unregister 是安全的，
     * 但新注册的项从下一帧才开始 tick，unregister 立即生效）。
     *
     * @param dt 帧时长（秒）
     * @return true  仍有未完成的动画（需要继续驱动）
     * @return false 所有动画均已完成（可停止定时器）
     */
    bool tick_all(float dt) noexcept;

    /**
     * @brief 查询是否存在活跃动画。
     *
     * @return true  有动画正在播放（应继续驱动定时器）
     * @return false 无活跃动画（可停止定时器）
     */
    [[nodiscard]] bool has_active() const noexcept;

private:
    AnimationClock() noexcept = default;

    /// 注册项：handle（唯一标识） + tick 回调
    struct Entry {
        void*  handle  = nullptr;
        TickFn tick_fn = nullptr;
    };

    /// 注册列表（内联容量 8，覆盖大多数场景）
    containers::SmallVector<Entry, 8> entries_;

    /// tick_all 重入防护（防止回调内部再次调用 tick_all）
    bool ticking_ = false;
};

}  // namespace mine::ui::animation
