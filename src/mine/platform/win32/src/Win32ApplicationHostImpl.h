/**
 * @file Win32ApplicationHostImpl.h
 * @brief IApplicationHost 的 Win32 内部实现。
 */

#pragma once

#include <mine/platform/IApplicationHost.h>
#include <mine/core/Memory.h>
#include "Win32WindowClass.h"
#include "Win32ScreenManager.h"
#include "Win32Clipboard.h"
#include "Win32IMEService.h"
#include "Win32Window.h"

#include <vector>

namespace mine::platform::win32 {

/**
 * @brief IApplicationHost 的 Win32 具体实现。
 *
 * 管理 Win32 消息循环、窗口生命周期及平台服务实例。
 * 每进程只能存在一个实例，通过 create_application_host() 工厂函数创建。
 */
class Win32ApplicationHostImpl final : public IApplicationHost {
public:
    Win32ApplicationHostImpl();
    ~Win32ApplicationHostImpl() override;

    // 禁止拷贝和移动（单例保证）
    Win32ApplicationHostImpl(const Win32ApplicationHostImpl&)            = delete;
    Win32ApplicationHostImpl& operator=(const Win32ApplicationHostImpl&) = delete;

    // ── IApplicationHost 接口实现 ─────────────────────────────────────────────

    int  run()                                          override;
    void quit(int exit_code = 0)                        override;
    [[nodiscard]] core::OwnedPtr<IWindow> create_window(
        const WindowDesc& desc)                         override;
    [[nodiscard]] IClipboard&     clipboard() override;
    [[nodiscard]] IScreenManager& screens()   override;
    [[nodiscard]] IMEService&     ime()       override;

private:
    /// 当某个 Win32Window 被销毁时调用（从追踪列表中移除）
    void on_window_destroyed(Win32Window* window);

private:
    /// 当前存活的所有窗口（弱引用：不拥有生命周期，生命周期由调用方占有）
    std::vector<Win32Window*> alive_windows_;

    /// 平台服务实例
    Win32ScreenManager screen_manager_;
    Win32Clipboard     clipboard_;
    Win32IMEService    ime_service_;

    /// run() 的退出码（由 quit() 设置）
    int exit_code_{0};
};

} // namespace mine::platform::win32
