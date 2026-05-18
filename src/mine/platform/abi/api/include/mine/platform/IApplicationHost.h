/**
 * @file IApplicationHost.h
 * @brief 进程级应用程序宿主接口。
 */

#pragma once

#include <mine/core/Memory.h>
#include <mine/platform/IWindow.h>
#include <mine/platform/WindowDesc.h>
#include <mine/platform/IClipboard.h>
#include <mine/platform/IScreenManager.h>
#include <mine/platform/IMEService.h>

namespace mine::platform {

/**
 * @brief 进程级应用程序宿主接口。
 *
 * IApplicationHost 管理主消息/事件循环与核心平台服务：
 *   - 窗口创建与生命周期
 *   - 剪贴板访问
 *   - 显示器枚举
 *   - IME 服务（L4 阶段完整实现）
 *
 * 通过平台后端工厂函数创建（如 create_win32_application_host()），
 * 以 OwnedPtr<IApplicationHost> 返回，由调用方拥有生命周期。
 *
 * 使用模式：
 * @code
 *   auto host = create_win32_application_host();
 *   WindowDesc desc;
 *   desc.title = "Hello";
 *   auto win = host->create_window(desc);
 *   win->show();
 *   return host->run(); // 进入消息循环，返回退出码
 * @endcode
 */
class IApplicationHost {
public:
    virtual ~IApplicationHost() = default;

    /**
     * @brief 进入平台主消息循环，阻塞直到调用 quit()。
     * @return 退出码（通常为 0 表示正常退出）
     */
    virtual int run() = 0;

    /**
     * @brief 请求退出消息循环。
     * @param exit_code 传递给 run() 的返回值
     * @note  可在任意线程调用（线程安全）
     */
    virtual void quit(int exit_code = 0) = 0;

    /**
     * @brief 创建一个新窗口。
     * @param desc 窗口创建参数
     * @return 成功时返回持有 IWindow 的 OwnedPtr；失败时返回空指针
     */
    [[nodiscard]] virtual core::OwnedPtr<IWindow> create_window(
        const WindowDesc& desc) = 0;

    /// 获取系统剪贴板接口（生命周期与宿主相同）
    [[nodiscard]] virtual IClipboard& clipboard() = 0;

    /// 获取多显示器管理接口（生命周期与宿主相同）
    [[nodiscard]] virtual IScreenManager& screens() = 0;

    /// 获取 IME 服务接口（生命周期与宿主相同）
    [[nodiscard]] virtual IMEService& ime() = 0;
};

} // namespace mine::platform
