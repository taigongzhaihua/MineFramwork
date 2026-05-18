/**
 * @file WindowDesc.h
 * @brief 窗口创建参数描述结构体。
 */

#pragma once

#include <mine/core/StringView.h>
#include <mine/math/Size.h>
#include <mine/math/Point.h>
#include <mine/platform/WindowKind.h>

namespace mine::platform {

/**
 * @brief 窗口创建参数，传递给 IApplicationHost::create_window()。
 *
 * 所有尺寸/坐标均为逻辑像素（系统 DPI 缩放后的值）。
 */
struct WindowDesc {
    /// 窗口标题（UTF-8）
    core::StringView title{};

    /// 初始客户区尺寸（逻辑像素）
    math::Size size{800.0f, 600.0f};

    /// 初始屏幕位置（逻辑像素，左上角）
    /// 仅当 auto_position = false 时生效
    math::Point position{};

    /// 是否由系统决定初始位置（默认 true）
    bool auto_position{true};

    /// 窗口是否可由用户调整大小
    bool resizable{true};

    /// 是否隐藏系统标题栏与边框（自定义标题栏模式）
    bool frameless{false};

    /// 是否开启窗口背景透明（需合成器支持）
    bool transparent{false};

    /// 是否始终显示在其他窗口之上
    bool always_on_top{false};

    /// 创建后是否保持隐藏（不立即调用 show()）
    bool startup_hidden{false};

    /// 窗口类型
    WindowKind kind{WindowKind::Normal};

    /// 父窗口（用于 Dialog 类型，nullptr 表示无父窗口）
    void* parent_native_handle{nullptr};
};

} // namespace mine::platform
