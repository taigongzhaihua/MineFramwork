/**
 * @file WindowContext.cpp
 * @brief 进程级全局窗口上下文注册/获取实现。
 */

#include <mine/ui/window/WindowContext.h>

namespace mine::ui {

/// 进程级单例指针，由 Application::run() 在初始化图形设备后设置。
static IWindowContext* s_context = nullptr;

void set_application_window_context(IWindowContext* ctx) noexcept
{
    s_context = ctx;
}

IWindowContext* get_application_window_context() noexcept
{
    return s_context;
}

} // namespace mine::ui
