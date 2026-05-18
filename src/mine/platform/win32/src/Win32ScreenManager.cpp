/**
 * @file Win32ScreenManager.cpp
 * @brief IScreenManager 的 Win32 实现。
 */

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <shellscalingapi.h>

#include "Win32ScreenManager.h"
#include "Win32Helpers.h"

namespace mine::platform::win32 {

// ── 内部结构体（仅用于枚举回调传参）────────────────────────────────────────

struct ScreenEnumData {
    std::vector<ScreenInfo>* screens;
};

// ── 私有方法 ──────────────────────────────────────────────────────────────────

BOOL CALLBACK Win32ScreenManager::monitor_enum_proc(
    HMONITOR hmonitor, HDC /*hdc*/, LPRECT /*lprect*/, LPARAM userdata) noexcept {

    auto* data = reinterpret_cast<ScreenEnumData*>(userdata);

    MONITORINFO mi{};
    mi.cbSize = sizeof(mi);
    if (!GetMonitorInfoW(hmonitor, &mi)) {
        return TRUE; // 跳过此显示器，继续枚举
    }

    // 查询显示器 DPI（需 Windows 8.1+）
    UINT dpi_x = 96, dpi_y = 96;
    GetDpiForMonitor(hmonitor, MDT_EFFECTIVE_DPI, &dpi_x, &dpi_y);

    ScreenInfo info{};
    // 物理像素坐标直接记录（调用方可根据需要换算）
    info.bounds    = rect_from_win32(mi.rcMonitor);
    info.work_area = rect_from_win32(mi.rcWork);
    info.dpi       = static_cast<float>(dpi_x);
    info.scale     = dpi_to_scale(info.dpi);
    info.is_primary = (mi.dwFlags & MONITORINFOF_PRIMARY) != 0;

    data->screens->push_back(info);
    return TRUE; // 继续枚举
}

std::vector<ScreenInfo> Win32ScreenManager::enumerate_screens() const {
    std::vector<ScreenInfo> screens;
    ScreenEnumData data{&screens};
    EnumDisplayMonitors(
        nullptr, nullptr,
        &Win32ScreenManager::monitor_enum_proc,
        reinterpret_cast<LPARAM>(&data));
    return screens;
}

// ── IScreenManager 接口实现 ───────────────────────────────────────────────────

int Win32ScreenManager::screen_count() const {
    return static_cast<int>(enumerate_screens().size());
}

ScreenInfo Win32ScreenManager::screen_at(int index) const {
    const auto screens = enumerate_screens();
    if (index < 0 || index >= static_cast<int>(screens.size())) {
        return {}; // 越界返回空结构体
    }
    return screens[static_cast<size_t>(index)];
}

ScreenInfo Win32ScreenManager::primary_screen() const {
    for (const auto& s : enumerate_screens()) {
        if (s.is_primary) {
            return s;
        }
    }
    return {};
}

ScreenInfo Win32ScreenManager::screen_for_point(math::Point pt) const {
    const POINT p{
        static_cast<LONG>(pt.x),
        static_cast<LONG>(pt.y),
    };
    const HMONITOR hm = MonitorFromPoint(p, MONITOR_DEFAULTTONEAREST);

    MONITORINFO mi{};
    mi.cbSize = sizeof(mi);
    if (!GetMonitorInfoW(hm, &mi)) {
        return primary_screen();
    }

    UINT dpi_x = 96, dpi_y = 96;
    GetDpiForMonitor(hm, MDT_EFFECTIVE_DPI, &dpi_x, &dpi_y);

    ScreenInfo info{};
    info.bounds    = rect_from_win32(mi.rcMonitor);
    info.work_area = rect_from_win32(mi.rcWork);
    info.dpi       = static_cast<float>(dpi_x);
    info.scale     = dpi_to_scale(info.dpi);
    info.is_primary = (mi.dwFlags & MONITORINFOF_PRIMARY) != 0;
    return info;
}

} // namespace mine::platform::win32
