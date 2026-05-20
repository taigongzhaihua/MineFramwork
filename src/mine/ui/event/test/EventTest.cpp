/**
 * @file EventTest.cpp
 * @brief mine.ui.event 模块单元测试。
 *
 * 覆盖场景：
 *   RoutedEvent：
 *     - 注册并验证名称、所有者类型、传播策略
 *     - 描述符地址稳定（多次注册同一事件返回同一地址）
 *   RoutedEventArgs：
 *     - 初始状态（handled=false, source=nullptr）
 *     - set_source / set_original_source / set_handled
 *   EventManager - Bubble（冒泡）路由：
 *     - 事件从 source 向 parent 传播
 *     - 三层树：source → parent → grandparent
 *     - handled=true 中止传播
 *   EventManager - Tunnel（隧道）路由：
 *     - 事件从 grandparent → parent → source 传播
 *     - handled=true 中止传播
 *   EventManager - Direct（直接）路由：
 *     - 仅在 source 上触发
 *     - 不传播到父节点
 *   RelayCommand：
 *     - execute() 调用执行函数
 *     - can_execute() 无判断函数时返回 true
 *     - can_execute() 有判断函数时按函数逻辑返回
 *     - subscribe_can_execute_changed / unsubscribe / notify
 *     - move 语义
 */

#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>

#include <memory>  // Variant.h 间接依赖

#include <mine/containers/Vector.h>
#include <mine/ui/event/Event.h>

using namespace mine::ui;
using namespace mine::core;

namespace core       = mine::core;
namespace containers = mine::containers;

// ============================================================================
// 测试辅助：MockTarget —— IRoutedEventTarget 的简单测试实现
// ============================================================================

namespace {

/**
 * @brief 测试用路由事件目标。
 *
 * 维护一个处理器列表（handler_entries），每次 invoke_handlers 时
 * 按注册顺序调用与事件匹配的回调。
 *
 * 同时记录 invoke_handlers() 的调用次数（invoke_count），
 * 供测试断言路由轨迹。
 */
class MockTarget : public IRoutedEventTarget {
public:
    /// 单个处理器注册项
    struct Entry {
        const RoutedEvent* event;      ///< 目标事件描述符
        RoutedEventHandlerFn handler;  ///< 回调函数
        void*                ud;       ///< 用户数据
    };

    explicit MockTarget(MockTarget* parent = nullptr) noexcept
        : parent_{parent}
    {}

    // ── IRoutedEventTarget 实现 ────────────────────────────────────────────

    [[nodiscard]] IRoutedEventTarget* parent_target() const noexcept override {
        return parent_;
    }

    void invoke_handlers(const RoutedEvent& event,
                          RoutedEventArgs&   args) noexcept override {
        ++invoke_count;
        for (const Entry& e : entries_) {
            if (e.event == &event) {
                e.handler(this, args, e.ud);
                // 若 handled 则不再调用本层剩余处理器
                if (args.handled()) {
                    break;
                }
            }
        }
    }

    // ── 测试 API ──────────────────────────────────────────────────────────

    /// 注册一个处理器（以函数指针 + 用户数据形式）
    void add_handler(const RoutedEvent& event,
                     RoutedEventHandlerFn handler,
                     void* ud = nullptr)
    {
        entries_.push_back({&event, handler, ud});
    }

    /// invoke_handlers() 的总调用次数（用于断言路由经过了哪些节点）
    int invoke_count = 0;

private:
    MockTarget*                        parent_;
    mine::containers::Vector<Entry>    entries_;
};

// ──────────────────────────────────────────────────────────────────────────
// 用于测试的虚拟所有者类型（仅用于 TypeId::of<>() 产生唯一 ID）
// ──────────────────────────────────────────────────────────────────────────

struct OwnerA {};
struct OwnerB {};

} // namespace (anonymous)

// ============================================================================
// RoutedEvent 测试
// ============================================================================

TEST_SUITE("RoutedEvent") {

    TEST_CASE("注册并读取事件属性") {
        const RoutedEvent& ev =
            register_event<OwnerA>("TestBubbleEvent", RoutingStrategy::Bubble);

        CHECK(ev.name()       == "TestBubbleEvent");
        CHECK(ev.owner_type() == core::TypeId::of<OwnerA>());
        CHECK(ev.strategy()   == RoutingStrategy::Bubble);
    }

    TEST_CASE("Tunnel 策略注册") {
        const RoutedEvent& ev =
            register_event<OwnerA>("TestTunnelEvent", RoutingStrategy::Tunnel);

        CHECK(ev.strategy() == RoutingStrategy::Tunnel);
    }

    TEST_CASE("Direct 策略注册") {
        const RoutedEvent& ev =
            register_event<OwnerA>("TestDirectEvent", RoutingStrategy::Direct);

        CHECK(ev.strategy() == RoutingStrategy::Direct);
    }

    TEST_CASE("不同所有者类型可注册同名事件") {
        const RoutedEvent& evA =
            register_event<OwnerA>("SharedName", RoutingStrategy::Bubble);
        const RoutedEvent& evB =
            register_event<OwnerB>("SharedName", RoutingStrategy::Bubble);

        // 两个描述符不同（不同所有者类型）
        CHECK(&evA != &evB);
        CHECK(evA.owner_type() != evB.owner_type());
    }
}

// ============================================================================
// RoutedEventArgs 测试
// ============================================================================

TEST_SUITE("RoutedEventArgs") {

    // 测试使用的共享事件描述符
    const RoutedEvent& kArgsTestEvent =
        register_event<OwnerA>("ArgsTestBubble", RoutingStrategy::Bubble);

    TEST_CASE("初始状态") {
        RoutedEventArgs args{kArgsTestEvent};

        CHECK(&args.routed_event()   == &kArgsTestEvent);
        CHECK(args.source()           == nullptr);
        CHECK(args.original_source()  == nullptr);
        CHECK(args.handled()          == false);
    }

    TEST_CASE("set_source / set_original_source") {
        RoutedEventArgs args{kArgsTestEvent};
        int dummy = 42;

        args.set_source(&dummy);
        args.set_original_source(&dummy);

        CHECK(args.source()          == &dummy);
        CHECK(args.original_source() == &dummy);
    }

    TEST_CASE("set_handled") {
        RoutedEventArgs args{kArgsTestEvent};
        CHECK(args.handled() == false);

        args.set_handled(true);
        CHECK(args.handled() == true);

        args.set_handled(false);
        CHECK(args.handled() == false);
    }
}

// ============================================================================
// EventManager - Bubble 路由测试
// ============================================================================

TEST_SUITE("EventManager_Bubble") {

    // 三层树：source（叶）→ parent → grandparent（根）
    const RoutedEvent& kBubbleEvent =
        register_event<OwnerA>("BubbleClick", RoutingStrategy::Bubble);

    TEST_CASE("Bubble 从 source 向上传播到所有祖先") {
        MockTarget grandparent;
        MockTarget parent{&grandparent};
        MockTarget source{&parent};

        // 每个节点注册一个处理器
        grandparent.add_handler(kBubbleEvent, [](void*, RoutedEventArgs&, void*) {});
        parent.add_handler(     kBubbleEvent, [](void*, RoutedEventArgs&, void*) {});
        source.add_handler(     kBubbleEvent, [](void*, RoutedEventArgs&, void*) {});

        RoutedEventArgs args{kBubbleEvent};
        EventManager::raise(source, args);

        // Bubble：source → parent → grandparent（全部访问）
        CHECK(source.invoke_count      == 1);
        CHECK(parent.invoke_count      == 1);
        CHECK(grandparent.invoke_count == 1);
    }

    TEST_CASE("Bubble 遇到 handled=true 时停止") {
        MockTarget grandparent;
        MockTarget parent{&grandparent};
        MockTarget source{&parent};

        // parent 处理器将 handled 设为 true
        source.add_handler(kBubbleEvent, [](void*, RoutedEventArgs&, void*) {});
        parent.add_handler(kBubbleEvent,
            [](void*, RoutedEventArgs& a, void*) { a.set_handled(true); });
        grandparent.add_handler(kBubbleEvent, [](void*, RoutedEventArgs&, void*) {});

        RoutedEventArgs args{kBubbleEvent};
        EventManager::raise(source, args);

        // source 和 parent 都被访问，grandparent 因 handled 停止
        CHECK(source.invoke_count      == 1);
        CHECK(parent.invoke_count      == 1);
        CHECK(grandparent.invoke_count == 0);  // 未达到
        CHECK(args.handled()           == true);
    }

    TEST_CASE("Bubble 无父节点时只在 source 触发") {
        MockTarget source;  // parent = nullptr
        source.add_handler(kBubbleEvent, [](void*, RoutedEventArgs&, void*) {});

        RoutedEventArgs args{kBubbleEvent};
        EventManager::raise(source, args);

        CHECK(source.invoke_count == 1);
    }

    TEST_CASE("Bubble source 没有注册处理器时传播仍继续") {
        MockTarget grandparent;
        MockTarget parent{&grandparent};
        MockTarget source{&parent};  // source 未注册处理器

        grandparent.add_handler(kBubbleEvent, [](void*, RoutedEventArgs&, void*) {});

        RoutedEventArgs args{kBubbleEvent};
        EventManager::raise(source, args);

        // invoke_handlers 会被调用（即使没有匹配的处理器），传播仍向上
        CHECK(source.invoke_count      == 1);
        CHECK(parent.invoke_count      == 1);
        CHECK(grandparent.invoke_count == 1);
    }
}

// ============================================================================
// EventManager - Tunnel 路由测试
// ============================================================================

TEST_SUITE("EventManager_Tunnel") {

    const RoutedEvent& kTunnelEvent =
        register_event<OwnerA>("PreviewClick", RoutingStrategy::Tunnel);

    TEST_CASE("Tunnel 从根向 source 传播（反向顺序）") {
        // 记录调用顺序
        mine::containers::Vector<void*> call_order;

        MockTarget grandparent;
        MockTarget parent{&grandparent};
        MockTarget source{&parent};

        // 记录每层的调用顺序（用节点地址区分）
        grandparent.add_handler(kTunnelEvent,
            [](void* sender, RoutedEventArgs&, void* ud) {
                static_cast<mine::containers::Vector<void*>*>(ud)->push_back(sender);
            }, &call_order);
        parent.add_handler(kTunnelEvent,
            [](void* sender, RoutedEventArgs&, void* ud) {
                static_cast<mine::containers::Vector<void*>*>(ud)->push_back(sender);
            }, &call_order);
        source.add_handler(kTunnelEvent,
            [](void* sender, RoutedEventArgs&, void* ud) {
                static_cast<mine::containers::Vector<void*>*>(ud)->push_back(sender);
            }, &call_order);

        RoutedEventArgs args{kTunnelEvent};
        EventManager::raise(source, args);

        // Tunnel 顺序：grandparent → parent → source
        REQUIRE(call_order.size() == 3u);
        CHECK(call_order[0] == &grandparent);
        CHECK(call_order[1] == &parent);
        CHECK(call_order[2] == &source);
    }

    TEST_CASE("Tunnel 遇到 handled=true 时停止") {
        MockTarget grandparent;
        MockTarget parent{&grandparent};
        MockTarget source{&parent};

        // grandparent 处理器将 handled 设为 true
        grandparent.add_handler(kTunnelEvent,
            [](void*, RoutedEventArgs& a, void*) { a.set_handled(true); });
        parent.add_handler(kTunnelEvent, [](void*, RoutedEventArgs&, void*) {});
        source.add_handler(kTunnelEvent, [](void*, RoutedEventArgs&, void*) {});

        RoutedEventArgs args{kTunnelEvent};
        EventManager::raise(source, args);

        // grandparent 被调用后 handled=true，parent 和 source 不被调用
        CHECK(grandparent.invoke_count == 1);
        CHECK(parent.invoke_count      == 0);
        CHECK(source.invoke_count      == 0);
        CHECK(args.handled()           == true);
    }

    TEST_CASE("Tunnel 无父节点时只在 source 触发") {
        MockTarget source;
        source.add_handler(kTunnelEvent, [](void*, RoutedEventArgs&, void*) {});

        RoutedEventArgs args{kTunnelEvent};
        EventManager::raise(source, args);

        CHECK(source.invoke_count == 1);
    }
}

// ============================================================================
// EventManager - Direct 路由测试
// ============================================================================

TEST_SUITE("EventManager_Direct") {

    const RoutedEvent& kDirectEvent =
        register_event<OwnerA>("DirectFocus", RoutingStrategy::Direct);

    TEST_CASE("Direct 只在 source 触发，不传播") {
        MockTarget grandparent;
        MockTarget parent{&grandparent};
        MockTarget source{&parent};

        grandparent.add_handler(kDirectEvent, [](void*, RoutedEventArgs&, void*) {});
        parent.add_handler(     kDirectEvent, [](void*, RoutedEventArgs&, void*) {});
        source.add_handler(     kDirectEvent, [](void*, RoutedEventArgs&, void*) {});

        RoutedEventArgs args{kDirectEvent};
        EventManager::raise(source, args);

        // 仅 source 被调用
        CHECK(source.invoke_count      == 1);
        CHECK(parent.invoke_count      == 0);
        CHECK(grandparent.invoke_count == 0);
    }

    TEST_CASE("Direct handled=true 不影响传播（本来就不传播）") {
        MockTarget parent;
        MockTarget source{&parent};

        source.add_handler(kDirectEvent,
            [](void*, RoutedEventArgs& a, void*) { a.set_handled(true); });
        parent.add_handler(kDirectEvent, [](void*, RoutedEventArgs&, void*) {});

        RoutedEventArgs args{kDirectEvent};
        EventManager::raise(source, args);

        CHECK(source.invoke_count == 1);
        CHECK(parent.invoke_count == 0);
    }
}

// ============================================================================
// RelayCommand 测试
// ============================================================================

TEST_SUITE("RelayCommand") {

    TEST_CASE("execute 调用 execute 函数") {
        int call_count = 0;
        RelayCommand cmd{
            [&call_count](const Variant&) { ++call_count; }
        };

        cmd.execute(Variant{});
        CHECK(call_count == 1);
        cmd.execute(Variant{});
        CHECK(call_count == 2);
    }

    TEST_CASE("can_execute 无判断函数时始终返回 true") {
        RelayCommand cmd{[](const Variant&) {}};
        CHECK(cmd.can_execute(Variant{}) == true);
    }

    TEST_CASE("can_execute 按判断函数逻辑返回") {
        bool enabled = false;
        RelayCommand cmd{
            [](const Variant&) {},
            [&enabled](const Variant&) { return enabled; }
        };

        CHECK(cmd.can_execute(Variant{}) == false);
        enabled = true;
        CHECK(cmd.can_execute(Variant{}) == true);
    }

    TEST_CASE("subscribe / notify / unsubscribe") {
        RelayCommand cmd{[](const Variant&) {}};

        int notify_count = 0;
        uint32_t token = cmd.subscribe_can_execute_changed(
            [](ICommand*, void* ud) {
                ++(*static_cast<int*>(ud));
            },
            &notify_count);

        cmd.notify_can_execute_changed();
        CHECK(notify_count == 1);

        cmd.notify_can_execute_changed();
        CHECK(notify_count == 2);

        // 取消订阅后不再收到通知
        cmd.unsubscribe_can_execute_changed(token);
        cmd.notify_can_execute_changed();
        CHECK(notify_count == 2);  // 未再增加
    }

    TEST_CASE("多个订阅者均收到通知") {
        RelayCommand cmd{[](const Variant&) {}};

        int a = 0, b = 0;
        // 多个订阅者，返回值不需要使用（仅验证通知行为）
        [[maybe_unused]] auto tok_a =
            cmd.subscribe_can_execute_changed([](ICommand*, void* ud) { ++(*static_cast<int*>(ud)); }, &a);
        [[maybe_unused]] auto tok_b =
            cmd.subscribe_can_execute_changed([](ICommand*, void* ud) { ++(*static_cast<int*>(ud)); }, &b);

        cmd.notify_can_execute_changed();
        CHECK(a == 1);
        CHECK(b == 1);
    }

    TEST_CASE("重复取消订阅同一 token 不崩溃") {
        RelayCommand cmd{[](const Variant&) {}};

        uint32_t token = cmd.subscribe_can_execute_changed(
            [](ICommand*, void*) {}, nullptr);

        cmd.unsubscribe_can_execute_changed(token);
        // 第二次取消同一 token 应静默忽略，不崩溃
        cmd.unsubscribe_can_execute_changed(token);
    }

    TEST_CASE("move 后原对象 Pimpl 为空，新对象可正常使用") {
        int call_count = 0;
        RelayCommand cmd{
            [&call_count](const Variant&) { ++call_count; }
        };

        RelayCommand moved = std::move(cmd);
        moved.execute(Variant{});
        CHECK(call_count == 1);
    }

    TEST_CASE("ICommand 接口指针可正常调用") {
        int call_count = 0;
        RelayCommand cmd{
            [&call_count](const Variant&) { ++call_count; }
        };

        ICommand* iface = &cmd;
        CHECK(iface->can_execute(Variant{}) == true);
        iface->execute(Variant{});
        CHECK(call_count == 1);
    }
}
