/**
 * @file NavigationEvent.h
 * @brief 导航事件类型定义。
 *
 * Frame 在每次路由切换时广播 NavigationEvent，订阅者可通过
 * INavigationService::subscribe() 注册回调。
 */

#pragma once

#include <cstdint>
#include <mine/nav/Api.h>
#include <mine/core/StringView.h>
#include <mine/core/Variant.h>

namespace mine::nav {

/**
 * @brief 导航事件类型枚举。
 */
enum class NavigationEventType : uint8_t {
    /// 即将导航：路由已确认，当前 Page 已通过 on_navigate_away()，
    /// 但 Page 尚未切换。可用于显示过渡动画或更新状态指示器。
    Navigating,

    /// 导航完成：新 Page 已加入视觉树，on_navigated_to() 已调用完毕。
    Navigated,

    /// 导航失败：指定路由名未在 Frame 中注册。
    NavigationFailed,

    /// 导航被取消：当前 Page 的 on_navigate_away() 返回 false，
    /// 路由切换被中止。
    NavigationCancelled,
};

/**
 * @brief 导航事件描述结构体。
 *
 * 由 Frame 在导航流程的各个阶段填充并传递给事件订阅者。
 * 生命周期：仅在回调期间有效，订阅者不得持有指针或引用到回调结束之后。
 *
 * @note 本结构体通过值传递（const 引用），不需要 DLL 导出修饰。
 *       其字段均为轻量类型（StringView 为非拥有视图，Variant* 为指针）。
 */
struct NavigationEvent {
    /// 事件类型
    NavigationEventType type;

    /// 目标路由名（对 NavigationFailed 仍为请求的路由名）
    mine::core::StringView route;

    /// 导航携带的参数（Navigating/Navigated/NavigationFailed 时有效）
    /// 指向 Frame 内部临时变量，生命周期仅在回调期间。
    const mine::core::Variant* parameter;
};

} // namespace mine::nav
