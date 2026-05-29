/**
 * @file INavigationService.h
 * @brief 导航服务抽象接口。
 *
 * 应用层通过此接口执行路由切换，无需依赖具体的 Frame 控件类型。
 * Frame 实现了此接口，应用层可将接口指针注册到 DI 容器，
 * 使 ViewModel 通过注入获得导航能力。
 *
 * 使用示例：
 * @code
 *   // 注册路由（无捕获 lambda 自动转为函数指针）
 *   frame.add_route("login",  [](void*) -> Page* { return new LoginPage(); });
 *   frame.add_route("main",   [](void*) -> Page* { return new MainPage(); });
 *
 *   // 带 DI 的工厂（通过 user_data 传递 ServiceProvider）
 *   frame.add_route("detail", [](void* ud) -> Page* {
 *       auto* sp = static_cast<ServiceProvider*>(ud);
 *       return sp->resolve<DetailPage>();
 *   }, &service_provider);
 *
 *   // 导航
 *   frame.navigate_to("main", my_param);
 * @endcode
 */

#pragma once

#include <cstdint>
#include <mine/nav/Api.h>
#include <mine/nav/NavigationEvent.h>
#include <mine/core/StringView.h>
#include <mine/core/Variant.h>
#include <mine/core/Status.h>

// 前向声明（避免循环依赖）
namespace mine::ui {
class Page;
} // namespace mine::ui

namespace mine::nav {

/**
 * @brief 导航服务抽象接口。
 *
 * 路由注册与页面切换的核心接口，由 Frame 实现。
 * 通过 DI 容器注入给 ViewModel 或其他需要导航能力的组件。
 */
class MINE_NAV_API INavigationService {
public:
    /**
     * @brief 页面工厂函数指针类型。
     *
     * 使用函数指针 + void* user_data 模式，而非 SBO Function，
     * 避免 32 字节 SBO 限制，支持持有大型闭包（如 ServiceProvider 指针）的工厂。
     *
     * @param user_data 注册路由时传入的用户数据指针，可为 nullptr
     * @return          新构造的 Page 派生实例，所有权转移给 Frame
     */
    using PageFactory = mine::ui::Page* (*)(void* user_data);

    /**
     * @brief 导航事件回调函数指针类型。
     *
     * @param sender    触发事件的 INavigationService 实例
     * @param event     本次导航事件详情
     * @param user_data 订阅时传入的用户数据指针
     */
    using EventFn = void (*)(INavigationService* sender,
                             const NavigationEvent& event,
                             void* user_data);

    virtual ~INavigationService() = default;

    // ── 路由管理 ─────────────────────────────────────────────────────────────

    /**
     * @brief 注册一条命名路由。
     *
     * 重复注册同名路由将覆盖旧工厂（最后注册的生效）。
     *
     * @param name       路由名称（区分大小写，建议用 "/" 分隔的路径风格）
     * @param factory    页面工厂函数指针
     * @param user_data  传递给 factory 的不透明用户数据，可为 nullptr
     */
    virtual void add_route(mine::core::StringView  name,
                           PageFactory             factory,
                           void*                   user_data = nullptr) noexcept = 0;

    // ── 导航操作 ─────────────────────────────────────────────────────────────

    /**
     * @brief 导航到指定路由。
     *
     * 导航流程：
     *  1. 查找路由，未找到则广播 NavigationFailed 并返回 Status::NotFound
     *  2. 广播 Navigating 事件
     *  3. 询问当前 Page：on_navigate_away()，返回 false 则广播
     *     NavigationCancelled 并返回 Status::Cancelled
     *  4. 通知当前 Page：on_navigated_from()
     *  5. 通过工厂创建新 Page，压入历史栈
     *  6. 切换内容（视觉树更新）
     *  7. 通知新 Page：on_navigated_to(param)
     *  8. 广播 Navigated 事件
     *
     * @param route  已注册的路由名称
     * @param param  传递给目标 Page::on_navigated_to() 的参数（可为空 Variant）
     * @return       Status::Ok         导航成功
     *               Status::NotFound   路由未注册
     *               Status::Cancelled  当前 Page 拒绝离开
     */
    [[nodiscard]] virtual mine::core::Status navigate_to(
        mine::core::StringView route,
        mine::core::Variant    param = {}) noexcept = 0;

    /**
     * @brief 回退到上一个历史页面。
     *
     * 回退流程：
     *  1. can_go_back() 为 false 时直接返回 false
     *  2. 询问当前 Page：on_navigate_away()，返回 false 则返回 false
     *  3. 通知当前 Page：on_navigated_from()
     *  4. 销毁当前 Page（弹出历史栈顶）
     *  5. 恢复显示前一 Page（视觉树更新）
     *  6. 广播 Navigated 事件（route 为前一页路由名）
     *
     * @return true  成功回退；false 无法回退或被当前 Page 拒绝
     */
    virtual bool go_back() noexcept = 0;

    /**
     * @brief 查询是否可以回退。
     *
     * @return true 历史栈中有两个及以上页面（有前一页可回退到）
     */
    [[nodiscard]] virtual bool can_go_back() const noexcept = 0;

    /**
     * @brief 获取当前显示的路由名称。
     *
     * @return 当前路由名；历史栈为空时返回空 StringView
     */
    [[nodiscard]] virtual mine::core::StringView current_route() const noexcept = 0;

    // ── 事件订阅 ─────────────────────────────────────────────────────────────

    /**
     * @brief 订阅导航事件。
     *
     * 每次路由切换（Navigating/Navigated/NavigationFailed/NavigationCancelled）
     * 均会调用已注册的所有回调。
     *
     * @param fn        事件回调函数指针，不得为 nullptr
     * @param user_data 传递给回调的不透明用户数据，可为 nullptr
     * @return          订阅令牌，用于 unsubscribe() 取消订阅；返回 0 表示注册失败
     */
    [[nodiscard]] virtual uint32_t subscribe(EventFn fn,
                                             void*   user_data) noexcept = 0;

    /**
     * @brief 取消订阅。
     *
     * 传入 subscribe() 返回的令牌，无效令牌（0 或已取消）会被忽略。
     *
     * @param token subscribe() 返回的订阅令牌
     */
    virtual void unsubscribe(uint32_t token) noexcept = 0;
};

} // namespace mine::nav
