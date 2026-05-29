/**
 * @file Frame.h
 * @brief Frame 导航容器控件。
 *
 * Frame 是路由出口控件，同时实现 INavigationService 接口。
 * 它管理页面历史栈，在路由切换时负责创建/销毁 Page、更新视觉树、
 * 触发 Page 生命周期回调，并广播导航事件给订阅者。
 *
 * 继承结构：
 *   UserControl（提供内容管理、测量/排列委托、视觉子树更新）
 *     └─ Frame（实现 INavigationService）
 *
 * 典型用法：
 * @code
 *   // 创建 Frame 并注册路由
 *   auto* frame = new Frame();
 *   frame->add_route("login", [](void*) -> Page* { return new LoginPage(); });
 *   frame->add_route("main",  [](void*) -> Page* { return new MainPage(); });
 *
 *   // 首次导航
 *   frame->navigate_to("login");
 *
 *   // 将 INavigationService 注册到 DI 容器
 *   container.bind<INavigationService>(frame);
 * @endcode
 */

#pragma once

#include <mine/nav/Api.h>
#include <mine/nav/INavigationService.h>
#include <mine/core/Pimpl.h>
#include <mine/ui/controls/UserControl.h>

namespace mine::nav {

/**
 * @brief 路由导航容器控件。
 *
 * Frame 将 INavigationService 的路由管理能力与 UI 控件的显示能力合并，
 * 应用层通过 navigate_to / go_back 切换当前展示的 Page。
 *
 * 设计要点：
 *   - 继承 UserControl，通过 set_content() 切换当前 Page 显示
 *   - 历史栈保存 OwnedPtr<Page>，Frame 持有所有历史页面的所有权
 *   - 导航时：询问旧页 on_navigate_away() → 通知旧页 on_navigated_from()
 *              → 切换内容 → 通知新页 on_navigated_to() → 广播事件
 *   - go_back 时弹出栈顶（销毁当前 Page），恢复显示历史栈中前一页
 */
// MSVC C4275：Frame（DLL 接口类）继承自 UserControl（非 DLL 接口基类）。
// UserControl 在 mine.ui.controls 模块中以 MINE_UI_CONTROLS_API 导出，
// 但 MSVC 静态分析认为两个模块的导出符号不完全匹配。
// 此处安全：Frame 和 UserControl 均在同一套框架 ABI 规范下编译。
#if defined(_MSC_VER)
#   pragma warning(push)
#   pragma warning(disable: 4275)  // 非 DLL 接口基类用于 DLL 接口类（Pimpl + 友元安全）
#endif
class MINE_NAV_API Frame : public mine::ui::UserControl,
                           public INavigationService {
#if defined(_MSC_VER)
#   pragma warning(pop)
#endif
public:
    Frame();
    ~Frame() override;

    // ── INavigationService ────────────────────────────────────────────────────

    void add_route(mine::core::StringView  name,
                   PageFactory             factory,
                   void*                   user_data = nullptr) noexcept override;

    [[nodiscard]] mine::core::Status navigate_to(
        mine::core::StringView route,
        mine::core::Variant    param = {}) noexcept override;

    bool go_back() noexcept override;

    [[nodiscard]] bool can_go_back() const noexcept override;

    [[nodiscard]] mine::core::StringView current_route() const noexcept override;

    [[nodiscard]] uint32_t subscribe(EventFn fn,
                                     void*   user_data) noexcept override;

    void unsubscribe(uint32_t token) noexcept override;

    // ── Frame 特有接口 ────────────────────────────────────────────────────────

    /**
     * @brief 获取当前正在显示的 Page 实例。
     *
     * @return 当前 Page 指针；历史栈为空时返回 nullptr
     */
    [[nodiscard]] mine::ui::Page* current_page() const noexcept;

    /**
     * @brief 获取历史栈深度（包含当前页面）。
     *
     * @return 历史栈条目数，0 表示尚未导航到任何页面
     */
    [[nodiscard]] size_t history_depth() const noexcept;

private:
    struct Impl;
    mine::core::Pimpl<Impl> p_;

    /// 内部辅助：向所有订阅者广播导航事件
    void broadcast_event(const NavigationEvent& ev) noexcept;

    /// 内部辅助：将新 Page 设为当前内容（更新视觉子树）
    void switch_content(mine::ui::Page* new_page) noexcept;
};

} // namespace mine::nav
