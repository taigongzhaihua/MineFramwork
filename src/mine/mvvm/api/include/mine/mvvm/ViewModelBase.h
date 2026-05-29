/**
 * @file ViewModelBase.h
 * @brief ViewModelBase —— MVVM ViewModel 完整基类。
 *
 * ViewModelBase 在 ObservableObject（属性变更通知）的基础上，
 * 增加 ViewModel 生命周期虚方法钩子，供框架（如 mine.nav）在合适时机调用。
 *
 * 生命周期调用顺序（典型场景）：
 *   构造 → on_initialize() → [on_navigated_to()] →
 *   [on_appear()] → [on_disappear()] → [on_navigated_from()] →
 *   on_dispose() → 析构
 *
 * 典型用法（与 Page 配合 MVVM 模式）：
 * @code
 *   class LoginViewModel : public ViewModelBase {
 *       MINE_INJECT(LoginViewModel, IAuthService*);
 *   public:
 *       explicit LoginViewModel(IAuthService* auth) : auth_(auth) {}
 *       MINE_OBSERVABLE(mine::containers::InlineString, username, "")
 *       MINE_OBSERVABLE(mine::containers::InlineString, password, "")
 *   protected:
 *       void on_initialize() noexcept override { ... }
 *       void on_navigated_to(const mine::core::Variant& param) noexcept override { ... }
 *   private:
 *       IAuthService* auth_;
 *   };
 * @endcode
 *
 * @note ViewModelBase 本身不可拷贝、不可移动（继承自 ObservableObject）。
 */

#pragma once

#include <mine/core/Variant.h>
#include <mine/mvvm/Api.h>
#include <mine/mvvm/ObservableObject.h>

namespace mine::mvvm {

/**
 * @brief ViewModel 完整基类（属性通知 + 生命周期钩子）。
 *
 * 所有生命周期方法均有默认空实现，子类按需重写。
 * 框架层（mine.nav 的 Frame、mine.ui.controls 的 Page）负责在合适时机调用这些方法。
 */
class MINE_MVVM_API ViewModelBase : public ObservableObject {
public:
    ViewModelBase();
    ~ViewModelBase() override;

    // ── 框架调用接口（公开，由 Frame/Page 等框架组件调用）────────────────────

    /**
     * @brief 通知 ViewModel 完成初始化（首次绑定视图前调用，仅一次）。
     *
     * 内部转调 on_initialize()。由创建 ViewModel 的框架组件调用。
     */
    void initialize() noexcept;

    /**
     * @brief 通知 ViewModel 已导航到（每次导航到此视图时调用）。
     *
     * 内部转调 on_navigated_to(param)。由 mine.nav 的 Frame 调用。
     * @param param 导航参数（可为空 Variant）
     */
    void navigated_to(const mine::core::Variant& param) noexcept;

    /**
     * @brief 通知 ViewModel 即将离开（导航离开时调用）。
     *
     * 内部转调 on_navigated_from()。由 mine.nav 的 Frame 调用。
     */
    void navigated_from() noexcept;

    /**
     * @brief 通知 ViewModel 所属视图变为可见。
     *
     * 内部转调 on_appear()。适合恢复定时刷新、重新订阅事件等操作。
     */
    void appear() noexcept;

    /**
     * @brief 通知 ViewModel 所属视图变为不可见。
     *
     * 内部转调 on_disappear()。适合暂停定时刷新、取消不必要的订阅等操作。
     */
    void disappear() noexcept;

    /**
     * @brief 通知 ViewModel 即将销毁（析构前最后一次通知）。
     *
     * 内部转调 on_dispose()。适合释放非内存资源（文件句柄、网络连接等）。
     */
    void dispose() noexcept;

protected:
    // ── 生命周期虚方法（子类重写）────────────────────────────────────────────

    /**
     * @brief ViewModel 初始化钩子（仅触发一次）。
     *
     * 适合加载初始数据、注册一次性事件订阅等操作。
     * 默认实现为空，子类可重写。
     */
    virtual void on_initialize() noexcept;

    /**
     * @brief 导航到达钩子（每次导航到此视图时触发）。
     *
     * @param param 导航参数（可为空 Variant，类型由调用方约定）
     *
     * 适合根据导航参数刷新数据、恢复滚动位置等操作。
     * 默认实现为空，子类可重写。
     */
    virtual void on_navigated_to(const mine::core::Variant& param) noexcept;

    /**
     * @brief 导航离开钩子（离开此视图时触发）。
     *
     * 适合保存草稿、取消正在进行的异步操作等。
     * 默认实现为空，子类可重写。
     */
    virtual void on_navigated_from() noexcept;

    /**
     * @brief 视图可见钩子（视图进入可见状态时触发）。
     *
     * 适合恢复定时刷新、开始动画等。
     * 默认实现为空，子类可重写。
     */
    virtual void on_appear() noexcept;

    /**
     * @brief 视图不可见钩子（视图离开可见状态时触发）。
     *
     * 适合暂停不必要的刷新。
     * 默认实现为空，子类可重写。
     */
    virtual void on_disappear() noexcept;

    /**
     * @brief 销毁前钩子（析构前最后一次触发）。
     *
     * 适合关闭文件、断开网络连接等资源清理。
     * 注意：此时对象仍然有效，可安全访问成员。
     * 默认实现为空，子类可重写。
     */
    virtual void on_dispose() noexcept;
};

} // namespace mine::mvvm
