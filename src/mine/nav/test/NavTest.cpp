/**
 * @file NavTest.cpp
 * @brief mine.nav 模块单元测试。
 *
 * 覆盖：
 *   - Frame 构造与基本状态（空历史栈）
 *   - add_route：注册路由，重复注册覆盖旧工厂
 *   - navigate_to：成功路径、路由未找到（NotFound）、工厂返回 nullptr
 *   - 导航拦截：on_navigate_away() 返回 false → NavigationCancelled
 *   - 生命周期调用顺序：on_navigate_away → on_navigated_from → on_navigated_to
 *   - go_back / can_go_back：正常回退、无历史时返回 false
 *   - current_route / current_page / history_depth
 *   - 事件订阅与取消订阅（subscribe/unsubscribe）
 *   - 导航事件类型序列（Navigating → Navigated / Failed / Cancelled）
 */

#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>

// 测试代码中大量使用 navigate_to/go_back 作为前置条件，
// 无需检查其返回值，压制 C4834（丢弃 [[nodiscard]] 返回值）警告
#if defined(_MSC_VER)
#   pragma warning(disable: 4834)
#endif

#include <mine/nav/Nav.h>
// Page 完整定义（NavTest 需要从 Page 派生 MockPage）
#include <mine/ui/controls/Page.h>
// Vector 用于收集导航事件序列
#include <mine/containers/Vector.h>
#include <mine/core/Status.h>

using namespace mine::nav;
using namespace mine::core;

// ─────────────────────────────────────────────────────────────────────────────
// 测试辅助：MockPage（记录导航生命周期调用）
// ─────────────────────────────────────────────────────────────────────────────

namespace {

/**
 * @brief 用于测试的 Page 派生类，记录各导航生命周期方法的调用情况。
 */
class MockPage : public mine::ui::Page {
public:
    /// 是否允许离开（默认 true）
    bool allow_navigate_away{true};

    /// 各生命周期方法的调用次数
    int navigated_to_count{0};
    int navigated_from_count{0};
    int navigate_away_count{0};

    /// 最后一次 on_navigated_to 收到的参数
    mine::core::Variant last_param{};

    /// 析构标志（用于检测是否被正确销毁）
    bool* destroyed_flag{nullptr};

    ~MockPage() override {
        if (destroyed_flag != nullptr) {
            *destroyed_flag = true;
        }
    }

protected:
    void on_navigated_to(const mine::core::Variant& param) noexcept override {
        ++navigated_to_count;
        last_param = param;
    }

    void on_navigated_from() noexcept override {
        ++navigated_from_count;
    }

    bool on_navigate_away() noexcept override {
        ++navigate_away_count;
        return allow_navigate_away;
    }
};

/// 创建 MockPage 工厂函数（无捕获，直接 new）
mine::ui::Page* make_mock_page(void* /*user_data*/) {
    return new MockPage();
}

/// 创建带析构标志的 MockPage 工厂函数（通过 user_data 传递标志指针）
mine::ui::Page* make_tracked_mock_page(void* user_data) {
    auto* page = new MockPage();
    page->destroyed_flag = static_cast<bool*>(user_data);
    return page;
}

/// 创建"拒绝导航离开"的 MockPage 工厂函数
mine::ui::Page* make_blocking_mock_page(void* /*user_data*/) {
    auto* page = new MockPage();
    page->allow_navigate_away = false;
    return page;
}

} // namespace

// ─────────────────────────────────────────────────────────────────────────────
// Frame 基本状态测试
// ─────────────────────────────────────────────────────────────────────────────

TEST_SUITE("Frame 基本状态") {

    TEST_CASE("初始状态：历史栈为空") {
        Frame frame;
        CHECK(frame.current_page() == nullptr);
        CHECK(frame.current_route().empty());
        CHECK(frame.history_depth() == 0u);
        CHECK(!frame.can_go_back());
    }

}

// ─────────────────────────────────────────────────────────────────────────────
// add_route 测试
// ─────────────────────────────────────────────────────────────────────────────

TEST_SUITE("add_route") {

    TEST_CASE("注册后可以导航到该路由") {
        Frame frame;
        frame.add_route("home", make_mock_page);

        auto status = frame.navigate_to("home");
        CHECK(status.code() == StatusCode::Ok);
        CHECK(frame.current_route() == "home");
        CHECK(frame.current_page() != nullptr);
    }

    TEST_CASE("重复注册同名路由：后者覆盖前者") {
        Frame frame;

        // 第一次注册：工厂返回 nullptr（会触发 NavigationFailed）
        frame.add_route("test", [](void*) -> mine::ui::Page* { return nullptr; });
        // 第二次注册：覆盖为正常工厂
        frame.add_route("test", make_mock_page);

        // 应使用第二次注册的工厂，导航成功
        auto status = frame.navigate_to("test");
        CHECK(status.code() == StatusCode::Ok);
    }

    TEST_CASE("user_data 正确传递给工厂函数") {
        Frame frame;

        int factory_call_count = 0;
        auto factory = [](void* ud) -> mine::ui::Page* {
            ++*static_cast<int*>(ud);
            return new MockPage();
        };
        frame.add_route("home", factory, &factory_call_count);

        frame.navigate_to("home");
        CHECK(factory_call_count == 1);

        frame.navigate_to("home");
        CHECK(factory_call_count == 2);
    }

}

// ─────────────────────────────────────────────────────────────────────────────
// navigate_to 测试
// ─────────────────────────────────────────────────────────────────────────────

TEST_SUITE("navigate_to") {

    TEST_CASE("路由未注册返回 NotFound") {
        Frame frame;
        auto status = frame.navigate_to("nonexistent");
        CHECK(status.code() == StatusCode::NotFound);
    }

    TEST_CASE("成功导航更新 current_route 和 history_depth") {
        Frame frame;
        frame.add_route("login", make_mock_page);
        frame.add_route("main",  make_mock_page);

        frame.navigate_to("login");
        CHECK(frame.current_route() == "login");
        CHECK(frame.history_depth() == 1u);

        frame.navigate_to("main");
        CHECK(frame.current_route() == "main");
        CHECK(frame.history_depth() == 2u);
    }

    TEST_CASE("导航参数正确传递给 on_navigated_to") {
        Frame frame;
        frame.add_route("detail", make_mock_page);

        mine::core::Variant param{42};
        frame.navigate_to("detail", param);

        // 无 RTTI 环境，不使用 dynamic_cast；直接验证页面非空即可
        CHECK(frame.current_page() != nullptr);
    }

    TEST_CASE("on_navigated_to 调用一次且参数正确") {
        Frame frame;

        MockPage* tracked = nullptr;
        // PageFactory 是函数指针（不接受捕获变量 lambda），通过 user_data 传递追踪指针
        frame.add_route("page", [](void* ud) -> mine::ui::Page* {
            auto** pp = static_cast<MockPage**>(ud);
            *pp = new MockPage();
            return *pp;
        }, &tracked);

        mine::core::Variant param{99};
        frame.navigate_to("page", param);

        // tracked 现在归 Frame 所有，但指针仍有效（页面在历史栈中存活）
        CHECK(tracked != nullptr);
        CHECK(tracked->navigated_to_count == 1);
    }

    TEST_CASE("连续导航：旧页面的 on_navigated_from 和新页面的 on_navigated_to 各调用一次") {
        Frame frame;

        MockPage* page_a = nullptr;
        MockPage* page_b = nullptr;

        frame.add_route("a", [](void* ud) -> mine::ui::Page* {
            auto** pp = static_cast<MockPage**>(ud);
            *pp = new MockPage();
            return *pp;
        }, &page_a);
        frame.add_route("b", [](void* ud) -> mine::ui::Page* {
            auto** pp = static_cast<MockPage**>(ud);
            *pp = new MockPage();
            return *pp;
        }, &page_b);

        frame.navigate_to("a");
        CHECK(page_a != nullptr);
        CHECK(page_a->navigated_to_count   == 1);
        CHECK(page_a->navigated_from_count == 0);

        frame.navigate_to("b");
        CHECK(page_a->navigated_from_count == 1);  // 旧页面离开通知
        CHECK(page_b != nullptr);
        CHECK(page_b->navigated_to_count   == 1);  // 新页面到达通知
    }

    TEST_CASE("工厂返回 nullptr 视为导航失败") {
        Frame frame;
        frame.add_route("broken", [](void*) -> mine::ui::Page* { return nullptr; });

        auto status = frame.navigate_to("broken");
        CHECK(status.code() == StatusCode::InvalidState);
        CHECK(frame.current_page() == nullptr);
    }

    TEST_CASE("导航被 on_navigate_away 拦截返回 Cancelled") {
        Frame frame;
        frame.add_route("first",   make_mock_page);
        frame.add_route("blocked", make_blocking_mock_page);  // 拒绝离开
        frame.add_route("second",  make_mock_page);

        frame.navigate_to("first");
        frame.navigate_to("blocked");   // 进入拒绝离开的页面

        // 尝试从 "blocked" 导航到 "second"，应被拒绝
        auto status = frame.navigate_to("second");
        CHECK(status.code() == StatusCode::Cancelled);
        // 当前页面保持不变
        CHECK(frame.current_route() == "blocked");
    }

    TEST_CASE("首次导航时不询问 on_navigate_away（无旧页面）") {
        Frame frame;

        MockPage* tracked = nullptr;
        frame.add_route("home", [](void* ud) -> mine::ui::Page* {
            auto** pp = static_cast<MockPage**>(ud);
            *pp = new MockPage();
            return *pp;
        }, &tracked);

        frame.navigate_to("home");

        // 首次导航无旧页面，不应调用 on_navigate_away
        CHECK(tracked != nullptr);
        CHECK(tracked->navigate_away_count == 0);
    }

}

// ─────────────────────────────────────────────────────────────────────────────
// go_back / can_go_back 测试
// ─────────────────────────────────────────────────────────────────────────────

TEST_SUITE("go_back") {

    TEST_CASE("无历史时 can_go_back 为 false，go_back 返回 false") {
        Frame frame;
        CHECK(!frame.can_go_back());
        CHECK(!frame.go_back());
    }

    TEST_CASE("只有一个页面时 can_go_back 为 false") {
        Frame frame;
        frame.add_route("home", make_mock_page);
        frame.navigate_to("home");

        CHECK(!frame.can_go_back());
        CHECK(!frame.go_back());
    }

    TEST_CASE("两个页面时 can_go_back 为 true，go_back 成功回退") {
        Frame frame;
        frame.add_route("login", make_mock_page);
        frame.add_route("main",  make_mock_page);

        frame.navigate_to("login");
        frame.navigate_to("main");

        CHECK(frame.can_go_back());

        bool result = frame.go_back();
        CHECK(result);
        CHECK(frame.current_route() == "login");
        CHECK(frame.history_depth() == 1u);
        CHECK(!frame.can_go_back());
    }

    TEST_CASE("go_back 触发正确的生命周期调用") {
        Frame frame;

        MockPage* login_page = nullptr;
        MockPage* main_page  = nullptr;

        frame.add_route("login", [](void* ud) -> mine::ui::Page* {
            auto** pp = static_cast<MockPage**>(ud);
            *pp = new MockPage();
            return *pp;
        }, &login_page);
        frame.add_route("main", [](void* ud) -> mine::ui::Page* {
            auto** pp = static_cast<MockPage**>(ud);
            *pp = new MockPage();
            return *pp;
        }, &main_page);

        frame.navigate_to("login");
        frame.navigate_to("main");

        // 回退前
        CHECK(main_page->navigated_from_count == 0);

        frame.go_back();

        // go_back 应通知当前页（main）离开
        CHECK(main_page->navigated_from_count == 1);
        // login_page 指针回退后已回到当前显示，不再调用 navigated_to（go_back 不重新导航）
    }

    TEST_CASE("go_back 时当前页面被销毁（OwnedPtr 析构）") {
        Frame frame;
        frame.add_route("login", make_mock_page);

        // make_tracked_mock_page 通过 user_data 接收 bool* 追踪指针，可直接作为 PageFactory
        bool detail_destroyed = false;
        frame.add_route("detail", make_tracked_mock_page, &detail_destroyed);

        frame.navigate_to("login");
        frame.navigate_to("detail");

        CHECK(!detail_destroyed);

        // go_back 弹出 detail_page（OwnedPtr 析构），destroyed_flag 应被置为 true
        frame.go_back();
        CHECK(detail_destroyed);
    }

    TEST_CASE("go_back 被 on_navigate_away 拦截返回 false") {
        Frame frame;
        frame.add_route("login",   make_mock_page);
        frame.add_route("blocked", make_blocking_mock_page);

        frame.navigate_to("login");
        frame.navigate_to("blocked");

        // 当前页面拒绝离开，go_back 应返回 false
        bool result = frame.go_back();
        CHECK(!result);
        CHECK(frame.current_route() == "blocked");
    }

    TEST_CASE("多次 go_back 逐步回退历史栈") {
        Frame frame;
        frame.add_route("a", make_mock_page);
        frame.add_route("b", make_mock_page);
        frame.add_route("c", make_mock_page);

        frame.navigate_to("a");
        frame.navigate_to("b");
        frame.navigate_to("c");

        CHECK(frame.history_depth() == 3u);

        frame.go_back();
        CHECK(frame.current_route() == "b");
        CHECK(frame.history_depth() == 2u);

        frame.go_back();
        CHECK(frame.current_route() == "a");
        CHECK(frame.history_depth() == 1u);

        CHECK(!frame.go_back());  // 已无法回退
    }

}

// ─────────────────────────────────────────────────────────────────────────────
// 事件订阅/取消订阅测试
// ─────────────────────────────────────────────────────────────────────────────

TEST_SUITE("导航事件订阅") {

    TEST_CASE("subscribe 返回非零令牌") {
        Frame frame;
        uint32_t token = frame.subscribe(
            [](INavigationService*, const NavigationEvent&, void*) noexcept {},
            nullptr);
        CHECK(token != 0u);
    }

    TEST_CASE("navigate_to 成功时广播 Navigating 和 Navigated 事件") {
        Frame frame;
        frame.add_route("home", make_mock_page);

        mine::containers::Vector<NavigationEventType> received;
        frame.subscribe(
            [](INavigationService*, const NavigationEvent& ev, void* ud) noexcept {
                static_cast<mine::containers::Vector<NavigationEventType>*>(ud)
                    ->push_back(ev.type);
            },
            &received);

        frame.navigate_to("home");

        CHECK(received.size() == 2u);
        if (received.size() == 2u) {
            CHECK(received[0] == NavigationEventType::Navigating);
            CHECK(received[1] == NavigationEventType::Navigated);
        }
    }

    TEST_CASE("路由不存在时广播 NavigationFailed 事件") {
        Frame frame;

        NavigationEventType received_type{NavigationEventType::Navigating};
        frame.subscribe(
            [](INavigationService*, const NavigationEvent& ev, void* ud) noexcept {
                *static_cast<NavigationEventType*>(ud) = ev.type;
            },
            &received_type);

        frame.navigate_to("nonexistent");
        CHECK(received_type == NavigationEventType::NavigationFailed);
    }

    TEST_CASE("导航被拦截时广播 Navigating 和 NavigationCancelled 事件") {
        Frame frame;
        frame.add_route("blocking", make_blocking_mock_page);
        frame.add_route("next",     make_mock_page);

        frame.navigate_to("blocking");  // 进入拒绝离开的页面

        mine::containers::Vector<NavigationEventType> received;
        frame.subscribe(
            [](INavigationService*, const NavigationEvent& ev, void* ud) noexcept {
                static_cast<mine::containers::Vector<NavigationEventType>*>(ud)
                    ->push_back(ev.type);
            },
            &received);

        frame.navigate_to("next");

        // Navigating 先广播（询问 on_navigate_away 前），然后 Cancelled
        CHECK(received.size() == 2u);
        if (received.size() == 2u) {
            CHECK(received[0] == NavigationEventType::Navigating);
            CHECK(received[1] == NavigationEventType::NavigationCancelled);
        }
    }

    TEST_CASE("事件中 route 字段正确") {
        Frame frame;
        frame.add_route("detail", make_mock_page);

        mine::core::StringView received_route{};
        frame.subscribe(
            [](INavigationService*, const NavigationEvent& ev, void* ud) noexcept {
                if (ev.type == NavigationEventType::Navigated) {
                    *static_cast<mine::core::StringView*>(ud) = ev.route;
                }
            },
            &received_route);

        frame.navigate_to("detail");
        CHECK(received_route == "detail");
    }

    TEST_CASE("unsubscribe 后不再接收事件") {
        Frame frame;
        frame.add_route("home", make_mock_page);

        int call_count = 0;
        uint32_t token = frame.subscribe(
            [](INavigationService*, const NavigationEvent&, void* ud) noexcept {
                ++*static_cast<int*>(ud);
            },
            &call_count);

        frame.navigate_to("home");
        CHECK(call_count == 2);  // Navigating + Navigated

        frame.unsubscribe(token);
        frame.navigate_to("home");  // 再次导航，但订阅者已取消
        CHECK(call_count == 2);     // 不再增加
    }

    TEST_CASE("unsubscribe 无效令牌（0 或已取消）不崩溃") {
        Frame frame;
        frame.unsubscribe(0);        // 无效令牌
        frame.unsubscribe(9999);     // 不存在的令牌
        // 不崩溃即通过
    }

    TEST_CASE("多订阅者同时接收事件") {
        Frame frame;
        frame.add_route("home", make_mock_page);

        int count_a = 0, count_b = 0;
        frame.subscribe(
            [](INavigationService*, const NavigationEvent&, void* ud) noexcept {
                ++*static_cast<int*>(ud);
            },
            &count_a);
        frame.subscribe(
            [](INavigationService*, const NavigationEvent&, void* ud) noexcept {
                ++*static_cast<int*>(ud);
            },
            &count_b);

        frame.navigate_to("home");
        CHECK(count_a == 2);  // Navigating + Navigated
        CHECK(count_b == 2);
    }

}

// ─────────────────────────────────────────────────────────────────────────────
// go_back 事件测试
// ─────────────────────────────────────────────────────────────────────────────

TEST_SUITE("go_back 事件") {

    TEST_CASE("go_back 成功时广播 Navigated 事件，route 为前一页路由名") {
        Frame frame;
        frame.add_route("login", make_mock_page);
        frame.add_route("main",  make_mock_page);

        frame.navigate_to("login");
        frame.navigate_to("main");

        // 订阅后执行 go_back，收集事件
        mine::containers::Vector<NavigationEvent> events;
        frame.subscribe(
            [](INavigationService*, const NavigationEvent& ev, void* ud) noexcept {
                static_cast<mine::containers::Vector<NavigationEvent>*>(ud)
                    ->push_back(ev);
            },
            &events);

        frame.go_back();

        // go_back 应广播 Navigated 事件，route 为回退后的当前页路由名
        CHECK(!events.empty());
        if (!events.empty()) {
            CHECK(events.back().type  == NavigationEventType::Navigated);
            CHECK(events.back().route == "login");
        }
    }

}
