/**
 * @file Win32ScreenManager.h
 * @brief IScreenManager 的 Win32 实现。
 */

#pragma once

#include <mine/platform/IScreenManager.h>
#include <vector>

namespace mine::platform::win32 {

/**
 * @brief 基于 EnumDisplayMonitors 的多显示器管理实现。
 *
 * 每次查询前重新枚举显示器信息（以反映热插拔变化）。
 */
class Win32ScreenManager final : public IScreenManager {
public:
    Win32ScreenManager() = default;
    ~Win32ScreenManager() override = default;

    [[nodiscard]] int         screen_count()                   const override;
    [[nodiscard]] ScreenInfo  screen_at(int index)             const override;
    [[nodiscard]] ScreenInfo  primary_screen()                 const override;
    [[nodiscard]] ScreenInfo  screen_for_point(math::Point pt) const override;

private:
    /// 枚举当前所有显示器并返回信息列表
    [[nodiscard]] std::vector<ScreenInfo> enumerate_screens() const;

    /// EnumDisplayMonitors 的回调函数
    static BOOL CALLBACK monitor_enum_proc(
        HMONITOR hmonitor, HDC hdc, LPRECT lprect, LPARAM userdata) noexcept;
};

} // namespace mine::platform::win32
