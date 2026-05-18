/**
 * @file IScreenManager.h
 * @brief 多显示器管理接口。
 */

#pragma once

#include <mine/platform/ScreenInfo.h>
#include <mine/math/Point.h>

namespace mine::platform {

/**
 * @brief 多显示器管理接口，提供显示器枚举与查询能力。
 *
 * 在 DPI 改变或显示器热插拔时，信息会被刷新。
 * 所有方法均应在主线程调用。
 */
class IScreenManager {
public:
    virtual ~IScreenManager() = default;

    /// 当前已连接的显示器数量
    [[nodiscard]] virtual int screen_count() const = 0;

    /**
     * @brief 获取指定索引的显示器信息。
     * @param index 从 0 开始，须 < screen_count()
     */
    [[nodiscard]] virtual ScreenInfo screen_at(int index) const = 0;

    /// 获取主显示器信息
    [[nodiscard]] virtual ScreenInfo primary_screen() const = 0;

    /**
     * @brief 获取包含指定逻辑坐标点的显示器信息。
     * @param p 虚拟桌面逻辑坐标
     * @return 最近的显示器信息（点不在任何显示器内时返回最近的一个）
     */
    [[nodiscard]] virtual ScreenInfo screen_for_point(math::Point p) const = 0;
};

} // namespace mine::platform
