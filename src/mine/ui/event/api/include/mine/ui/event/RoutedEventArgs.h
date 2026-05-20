/**
 * @file RoutedEventArgs.h
 * @brief 路由事件参数基类。
 *
 * RoutedEventArgs 携带路由事件的上下文信息，是所有路由事件参数类的基类：
 *   - 标识所属路由事件（routed_event()）
 *   - 记录事件发起源（source / original_source）
 *   - 提供 Handled 标志以中止事件传播
 *
 * 特化事件（如键盘、鼠标事件）从此类继承并添加额外数据。
 *
 * 生命周期：
 *   RoutedEventArgs 通常在调用栈上构造，并以引用形式传递给 EventManager::raise()。
 *   不应在事件处理器完成后持有 RoutedEventArgs 的指针/引用。
 *
 * 示例：
 * @code
 *   RoutedEventArgs args{Button::ClickEvent};
 *   args.set_source(button_ptr);
 *   EventManager::raise(*button_element, args);
 * @endcode
 */

#pragma once

#include <mine/ui/event/Api.h>
#include <mine/ui/event/RoutedEvent.h>

namespace mine::ui {

/**
 * @brief 路由事件参数基类。
 *
 * 继承此类可定义具体事件参数类型（如 KeyEventArgs、MouseEventArgs 等）。
 * 基类保存路由事件描述符引用和传播控制状态。
 *
 * 拷贝语义：禁止拷贝（防止切片），允许子类通过引用传递。
 */
class MINE_UI_EVENT_API RoutedEventArgs {
public:
    /**
     * @brief 构造事件参数，绑定到指定路由事件。
     * @param event 此参数对应的路由事件描述符
     */
    explicit RoutedEventArgs(const RoutedEvent& event) noexcept;

    virtual ~RoutedEventArgs() = default;

    // 禁止拷贝（防止多态切片）
    RoutedEventArgs(const RoutedEventArgs&)            = delete;
    RoutedEventArgs& operator=(const RoutedEventArgs&) = delete;

    // ── 事件描述符 ────────────────────────────────────────────────────────

    /// 返回触发此参数对象的路由事件描述符
    [[nodiscard]] const RoutedEvent& routed_event() const noexcept;

    // ── 事件源 ────────────────────────────────────────────────────────────

    /**
     * @brief 当前传播层的事件源（随路由路径变化）。
     *
     * 在 WPF 语义中，source 在每个路由步骤中指向当前处理元素，
     * 而 original_source 始终保持最初触发事件的元素不变。
     * 当前 M1.1 阶段使用 void*；mine.ui.visual 就绪后类型会明确化。
     */
    [[nodiscard]] void* source() const noexcept;

    /**
     * @brief 最初触发事件的元素（在整个路由过程中不变）。
     */
    [[nodiscard]] void* original_source() const noexcept;

    /// 设置当前传播层的事件源
    void set_source(void* src) noexcept;

    /// 设置原始事件源（通常仅在 raise() 入口处设置一次）
    void set_original_source(void* src) noexcept;

    // ── 传播控制 ──────────────────────────────────────────────────────────

    /**
     * @brief 标记此事件已被处理，阻止后续传播。
     *
     * 事件处理器可将此值设为 true 以中止事件继续在路由路径上传播。
     * EventManager 在每个路由步骤开始前检查此标志。
     */
    [[nodiscard]] bool handled() const noexcept;

    /**
     * @brief 设置事件的 Handled 状态。
     * @param handled true 表示已处理并停止传播；false 恢复传播（罕见）
     */
    void set_handled(bool handled) noexcept;

private:
    /// 对应的路由事件描述符引用（全局生命周期，永远有效）
    const RoutedEvent* event_;
    /// 当前传播层的事件源
    void*              source_          = nullptr;
    /// 最初触发事件的源
    void*              original_source_ = nullptr;
    /// Handled 标志
    bool               handled_         = false;
};

} // namespace mine::ui
