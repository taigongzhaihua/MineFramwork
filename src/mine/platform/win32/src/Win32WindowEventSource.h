/**
 * @file Win32WindowEventSource.h
 * @brief IWindowEventSource 的 Win32 内部实现。
 *
 * 使用 std::vector 存储订阅列表（仅在内部 .cpp/.h 中使用，不暴露到公共头）。
 */

#pragma once

#include <mine/platform/IWindowEventSource.h>
#include <mine/platform/WindowEvent.h>
#include <vector>

namespace mine::platform::win32 {

/**
 * @brief 窗口事件分发器的 Win32 具体实现。
 *
 * 维护一组 IWindowEventSink 指针（不拥有生命周期），
 * fire() 时依次通知所有已订阅的接收器。
 *
 * 注意：不支持在 fire() 调用期间修改订阅列表（禁止在回调中调用 subscribe/unsubscribe）。
 */
class Win32WindowEventSource final : public IWindowEventSource {
public:
    Win32WindowEventSource() = default;
    ~Win32WindowEventSource() override = default;

    // 禁止拷贝和移动（地址稳定性）
    Win32WindowEventSource(const Win32WindowEventSource&)            = delete;
    Win32WindowEventSource& operator=(const Win32WindowEventSource&) = delete;

    void subscribe(IWindowEventSink* sink) override {
        if (!sink) {
            return;
        }
        // 防止重复订阅
        for (auto* s : sinks_) {
            if (s == sink) {
                return;
            }
        }
        sinks_.push_back(sink);
    }

    void unsubscribe(IWindowEventSink* sink) override {
        for (auto it = sinks_.begin(); it != sinks_.end(); ++it) {
            if (*it == sink) {
                sinks_.erase(it);
                return;
            }
        }
    }

    /**
     * @brief 向所有订阅者分发事件。
     * @param event 事件数据（可被订阅者修改，如 cancel 标志）
     */
    void fire(WindowEvent& event) {
        // 快照副本：防止回调中修改 sinks_ 导致迭代器失效
        const auto snapshot = sinks_;
        for (auto* sink : snapshot) {
            sink->on_window_event(event);
        }
    }

private:
    std::vector<IWindowEventSink*> sinks_;
};

} // namespace mine::platform::win32
