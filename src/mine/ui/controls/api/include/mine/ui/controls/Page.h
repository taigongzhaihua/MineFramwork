/**
 * @file Page.h
 * @brief Page —— 导航系统页面基类（任务 17.3）。
 *
 * Page 在 UserControl 的基础上引入：
 *   - 导航生命周期虚函数：on_navigated_to / on_navigated_from / on_navigate_away；
 *   - 以上虚函数由 mine.nav 的 Frame 控件在导航操作期间调用；
 *   - 本任务仅完成接口定义（接口先行），Frame 在 F3.1 中实现。
 *
 * 继承关系：
 *   DependencyObject (mine.ui.property)
 *       └─ Visual (mine.ui.visual)
 *           └─ UIElement (mine.ui.visual)
 *               └─ FrameworkElement (mine.ui.visual)
 *                   └─ Control (mine.ui.visual)
 *                       └─ ContentControl (mine.ui.controls)
 *                           └─ UserControl (mine.ui.controls)
 *                               └─ Page (mine.ui.controls)
 *
 * 典型用法（mmlc 生成代码）：
 * @code
 *   class LoginPage : public Page {
 *   protected:
 *       void on_navigated_to(const core::Variant& param) noexcept override {
 *           // param 为 Frame::navigate_to() 传入的参数
 *       }
 *       bool on_navigate_away() noexcept override {
 *           // 有未保存数据时返回 false 可拦截导航
 *           return !has_unsaved_changes_;
 *       }
 *   };
 * @endcode
 */

#pragma once

#include <mine/ui/controls/Api.h>
#include <mine/ui/controls/UserControl.h>
#include <mine/core/Variant.h>

namespace mine::ui {

/**
 * @brief 导航系统页面基类。
 *
 * Page 是 mine.nav 的 Frame 控件所管理的导航单元。
 * 每个页面组件（MML 中的 page X : Page）对应一个 Page 子类实例。
 *
 * **导航生命周期顺序**（Frame 在导航时按顺序调用）：
 *   1. 离开当前页面前：旧页面 on_navigate_away() 返回 false → 导航被取消
 *   2. 旧页面 on_navigated_from()：离开通知，此时页面即将从 Frame 移除
 *   3. 新页面 on_navigated_to(param)：到达通知，此时页面已加入 Frame 视觉树
 *
 * **与 UserControl 生命周期的关系**：
 *   - on_loaded() / on_unloaded() 仍由视觉树管理（随 Frame 挂载/卸载触发）
 *   - on_navigated_to / on_navigated_from 由 Frame 在逻辑层显式调用，与视觉树无关
 *
 * @note Frame 控件（mine.nav）在 F3.1 中实现，本类仅提供接口定义。
 */

class MINE_UI_CONTROLS_API Page : public UserControl {
public:
    // ── 生命周期 ──────────────────────────────────────────────────────────

    Page();
    ~Page() override;

    Page(const Page&)            = delete;
    Page& operator=(const Page&) = delete;
    Page(Page&&)                 = default;
    Page& operator=(Page&&)      = default;

    // ── 导航生命周期虚函数（由 Frame 显式调用，子类可覆盖）────────────────
    //
    // 这些方法作为框架接口（Frame 调用）+ 子类扩展点（应用层覆盖）公开。
    // 虽然是 public，但应用层通常不应直接调用，而应通过 Frame 的导航操作触发。

    /**
     * @brief 导航到本页面后触发。
     *
     * 由 Frame 在完成页面切换、将本页面加入视觉树之后调用。
     * 子类可在此处根据导航参数初始化页面状态（如加载数据、设置标题等）。
     *
     * @param param 导航参数（由 Frame::navigate_to() 的调用方传入，可为空 Variant）
     *
     * 默认实现为空。
     */
    virtual void on_navigated_to(const core::Variant& param) noexcept;

    /**
     * @brief 离开本页面后触发。
     *
     * 由 Frame 在完成页面切换、将本页面从视觉树移除之前调用。
     * 子类可在此处保存页面状态、取消未完成的异步操作等。
     *
     * 默认实现为空。
     */
    virtual void on_navigated_from() noexcept;

    /**
     * @brief 询问本页面是否允许导航离开。
     *
     * 由 Frame 在执行导航操作之前调用（在 on_navigated_from 之前）。
     * 返回 false 时 Frame 将取消本次导航，页面保持不变。
     *
     * 典型用场景：存在未保存的表单数据，需要弹出确认对话框。
     *
     * @return true  —— 允许导航离开（默认）
     * @return false —— 拦截导航，保持当前页面
     *
     * 默认实现返回 true（始终允许）。
     */
    virtual bool on_navigate_away() noexcept;
};

} // namespace mine::ui
