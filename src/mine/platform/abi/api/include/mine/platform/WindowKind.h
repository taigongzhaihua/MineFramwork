/**
 * @file WindowKind.h
 * @brief 窗口类型枚举。
 */

#pragma once

namespace mine::platform {

/**
 * @brief 窗口类型，决定系统装饰与行为。
 */
enum class WindowKind : int {
    Normal,   ///< 普通应用窗口（含标题栏、任务栏条目）
    Tool,     ///< 工具窗口（小标题栏，不在任务栏中显示）
    Dialog,   ///< 模态对话框（对父窗口阻塞）
    Splash,   ///< 启动闪屏（无边框、无任务栏条目、无阴影）
    Popup,    ///< 弹出窗口（无边框、自动取消焦点时关闭）
};

} // namespace mine::platform
