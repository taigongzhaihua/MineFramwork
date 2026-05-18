/**
 * @file WindowEvent.h
 * @brief 窗口事件类型定义。
 *
 * WindowEvent 采用带标签的扁平结构体：所有字段共存，
 * 调用方根据 kind 字段判断哪些有效，无需 union 或 std::variant。
 */

#pragma once

#include <mine/math/Size.h>
#include <mine/math/Point.h>
#include <mine/math/Rect.h>

namespace mine::platform {

/**
 * @brief 窗口事件类型。
 */
enum class WindowEventKind : int {
    Resized,      ///< 窗口客户区尺寸发生改变
    Moved,        ///< 窗口位置发生改变
    Closing,      ///< 用户请求关闭（可通过 cancel = true 阻止）
    Closed,       ///< 窗口已销毁（此后不得再调用 IWindow 方法）
    FocusGained,  ///< 窗口获得键盘焦点
    FocusLost,    ///< 窗口失去键盘焦点
    DpiChanged,   ///< 每英寸像素数改变（per-monitor DPI v2）
    Activated,    ///< 窗口切换为前台激活状态
    Deactivated,  ///< 窗口切换为后台非激活状态
};

/**
 * @brief 窗口事件数据，扁平结构。
 *
 * 根据 kind 字段读取对应数据字段：
 *   Resized     → new_size
 *   Moved       → new_position
 *   DpiChanged  → new_dpi, suggested_rect（系统建议的新窗口矩形）
 *   Closing     → 可置 cancel = true 阻止关闭
 *   其余字段均为零值，可忽略。
 */
struct WindowEvent {
    WindowEventKind kind{WindowEventKind::Closed};

    // Resized 时有效：客户区新尺寸（逻辑像素）
    math::Size new_size{};

    // Moved 时有效：屏幕坐标新位置（逻辑像素）
    math::Point new_position{};

    // DpiChanged 时有效
    float      new_dpi{96.0f};              ///< 新 DPI 值
    math::Rect suggested_rect{};            ///< 系统建议的新窗口矩形（屏幕坐标）

    // Closing 时可设置为 true 以取消关闭操作
    mutable bool cancel{false};
};

} // namespace mine::platform
