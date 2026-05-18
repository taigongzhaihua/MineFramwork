/**
 * @file PlatformHost.h
 * @brief 平台应用宿主工厂函数声明（平台无关接口）。
 *
 * 此头文件声明 create_application_host()，但不含任何平台特定代码。
 * 实际实现由链接的平台后端库提供：
 *
 *   - mine.platform.win32  → create_application_host() 返回 Win32 宿主
 *   - mine.platform.macos  → 返回 Cocoa 宿主（未来）
 *   - mine.platform.linux  → 返回 Wayland/X11 宿主（未来）
 *
 * 应用代码只需：
 * @code
 *   #include <mine/platform/PlatformHost.h>
 *   auto host = mine::platform::create_application_host();
 * @endcode
 *
 * 链接哪个平台后端（通过构建系统 add_deps 配置），即使用对应实现，
 * 不需要在源码中出现任何平台 #ifdef 或平台特定 #include。
 */

#pragma once

#include <mine/core/Memory.h>
#include <mine/platform/IApplicationHost.h>

namespace mine::platform {

/**
 * @brief 创建当前平台的应用程序宿主实例。
 *
 * 声明在此，定义由链接的平台后端库（mine.platform.win32 等）提供。
 * 须在进程主线程调用，每进程只应调用一次。
 *
 * @return 持有 IApplicationHost 的 OwnedPtr（不会为空）
 */
core::OwnedPtr<IApplicationHost> create_application_host();

} // namespace mine::platform
