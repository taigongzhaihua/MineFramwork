/**
 * @file Page.cpp
 * @brief Page 导航系统页面基类实现（任务 17.3）。
 *
 * 实现要点：
 *   - 构造/析构均为默认实现（所有成员由 UserControl 基类负责）。
 *   - 三个导航虚函数提供默认空实现，子类根据需要覆盖：
 *       - on_navigated_to：默认空操作
 *       - on_navigated_from：默认空操作
 *       - on_navigate_away：默认返回 true（允许导航离开）
 *   - Frame 控件（mine.nav）在 F3.1 中实现，届时将调用这些虚函数。
 */

#include <mine/ui/controls/Page.h>
#include <mine/core/Variant.h>

namespace mine::ui {

// ============================================================================
// 生命周期
// ============================================================================

Page::Page()  = default;
Page::~Page() = default;

// ============================================================================
// 导航生命周期虚函数默认实现
// ============================================================================

void Page::on_navigated_to(const core::Variant& /*param*/) noexcept
{
    // 默认空实现：子类覆盖以处理导航到达事件（如加载数据、设置标题）
}

void Page::on_navigated_from() noexcept
{
    // 默认空实现：子类覆盖以处理离开事件（如保存状态、取消异步操作）
}

bool Page::on_navigate_away() noexcept
{
    // 默认返回 true：允许 Frame 执行导航离开
    // 子类覆盖：当存在未保存数据时返回 false 可拦截导航
    return true;
}

} // namespace mine::ui
