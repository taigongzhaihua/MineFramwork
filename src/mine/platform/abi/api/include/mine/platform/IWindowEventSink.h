/**
 * @file IWindowEventSink.h
 * @brief 窗口事件消费者接口（观察者）。
 */

#pragma once

#include <mine/platform/WindowEvent.h>

namespace mine::platform {

/**
 * @brief 窗口事件接收器接口。
 *
 * 实现此接口并通过 IWindow::events().subscribe(sink) 注册，
 * 即可接收该窗口产生的所有 WindowEvent。
 *
 * 注意：
 *   - 回调发生在窗口所在平台线程（通常为主线程）。
 *   - 不得在回调中销毁注册的 sink，否则行为未定义。
 */
class IWindowEventSink {
public:
    virtual ~IWindowEventSink() = default;

    /**
     * @brief 窗口事件回调。
     * @param event 事件数据（可修改 event.cancel 以取消 Closing 事件）
     */
    virtual void on_window_event(WindowEvent& event) = 0;
};

} // namespace mine::platform
