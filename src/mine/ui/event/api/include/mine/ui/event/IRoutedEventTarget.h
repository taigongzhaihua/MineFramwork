/**
 * @file IRoutedEventTarget.h
 * @brief 路由事件目标接口 —— 路由系统与可视化树的抽象边界。
 *
 * IRoutedEventTarget 使 EventManager 的路由算法独立于具体的 UIElement 实现：
 *   - 当前阶段（M1.1）：用于测试和事件系统基础验证
 *   - 后续阶段（mine.ui.visual）：UIElement 将实现此接口
 *
 * 设计原则：
 *   - 不引入 UIElement 等具体类型的依赖（在 mine.ui.visual 存在之前保持解耦）
 *   - 事件处理器以函数指针 + 用户数据形式存储（避免 std::function 开销）
 *   - 接口极简：仅暴露路由所需的两个操作（获取父节点、触发处理器列表）
 */

#pragma once

#include <mine/ui/event/RoutedEventArgs.h>

namespace mine::ui {

/// 路由事件处理器函数指针类型
/// @param sender    当前处理元素（与 invoke_handlers 的 this 对应）
/// @param args      路由事件参数（可读写 handled 等状态）
/// @param user_data 注册时传入的用户自定义数据
using RoutedEventHandlerFn = void(*)(void* sender,
                                     RoutedEventArgs& args,
                                     void* user_data);

/**
 * @brief 路由事件目标接口。
 *
 * 实现此接口的对象可以参与路由事件系统：
 *   - 提供父节点指针（供 EventManager 遍历路由路径）
 *   - 管理并触发本元素上注册的事件处理器列表
 */
class IRoutedEventTarget {
public:
    virtual ~IRoutedEventTarget() = default;

    /**
     * @brief 返回可视化树中的父节点，若当前为根节点则返回 nullptr。
     *
     * EventManager 依赖此方法向上遍历路由路径（Bubble 策略）
     * 或收集完整路径再反向遍历（Tunnel 策略）。
     */
    [[nodiscard]] virtual IRoutedEventTarget* parent_target() const noexcept = 0;

    /**
     * @brief 触发此节点上注册到指定路由事件的所有处理器。
     *
     * 实现者应：
     *   1. 遍历内部已注册的处理器列表
     *   2. 对列表中匹配 event 的每个处理器调用回调
     *   3. 每次调用后检查 args.handled()，若为 true 可提前停止本层处理
     *
     * @param event 当前派发的路由事件描述符
     * @param args  路由事件参数（可被处理器修改 handled 状态等）
     */
    virtual void invoke_handlers(const RoutedEvent& event,
                                  RoutedEventArgs&   args) noexcept = 0;
};

} // namespace mine::ui
