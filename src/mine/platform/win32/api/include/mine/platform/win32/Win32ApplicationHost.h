/**
 * @file Win32ApplicationHost.h
 * @brief Win32 平台应用程序宿主工厂函数声明。
 */

#pragma once

#include <mine/core/Memory.h>
#include <mine/platform/IApplicationHost.h>
#include <mine/platform/win32/Api.h>

namespace mine::platform::win32 {

/**
 * @brief 创建 Win32 平台应用程序宿主实例。
 *
 * 必须在进程主线程调用，且每进程只能调用一次。
 * 内部会调用 SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2)
 * 开启 per-monitor DPI v2 感知。
 *
 * @return 持有 IApplicationHost 的 OwnedPtr（不会为空）
 *
 * 典型用法：
 * @code
 *   auto host = mine::platform::win32::create_application_host();
 *   mine::platform::WindowDesc desc;
 *   desc.title = "Hello MineFramework";
 *   auto win = host->create_window(desc);
 *   win->show();
 *   return host->run();
 * @endcode
 */
MINE_PLATFORM_WIN32_API
core::OwnedPtr<IApplicationHost> create_application_host();

} // namespace mine::platform::win32
