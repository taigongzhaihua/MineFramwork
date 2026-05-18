/**
 * @file IWindowEventSource.h
 * @brief 窗口事件分发器接口（被观察者）。
 */

#pragma once

#include <mine/platform/IWindowEventSink.h>

namespace mine::platform {

/**
 * @brief 窗口事件分发器接口。
 *
 * 持有一组 IWindowEventSink 注册表，当窗口状态发生变化时，
 * 依次通知所有已订阅的接收器。
 *
 * 与 IWindow::events() 配合使用：
 * @code
 *   window->events().subscribe(&my_sink);
 *   // ...
 *   window->events().unsubscribe(&my_sink);
 * @endcode
 */
class IWindowEventSource {
public:
    virtual ~IWindowEventSource() = default;

    /**
     * @brief 订阅窗口事件。
     * @param sink 事件接收器指针（不得为 nullptr，生命周期须长于订阅期）
     * @note  同一 sink 重复订阅不会产生重复通知。
     */
    virtual void subscribe(IWindowEventSink* sink) = 0;

    /**
     * @brief 取消订阅窗口事件。
     * @param sink 之前通过 subscribe 注册的接收器指针
     * @note  取消未订阅的 sink 为安全操作（静默忽略）。
     */
    virtual void unsubscribe(IWindowEventSink* sink) = 0;
};

} // namespace mine::platform
