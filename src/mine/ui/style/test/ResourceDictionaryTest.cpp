/**
 * @file ResourceDictionaryTest.cpp
 * @brief mine.ui.style - ResourceDictionary 单元测试。
 *
 * 覆盖场景：
 *   1. 本层写入与查找（set / find_local / find）
 *   2. 未命中时返回空 Variant（has_value() == false）
 *   3. 树形查找 - 父层命中（set_parent + find）
 *   4. 树形查找 - 合并层命中（merge + find）
 *   5. 本层优先于合并层
 *   6. 合并层优先于父层
 *   7. clear_merged 后合并层不参与查找
 *   8. 变更通知 - subscribe / unsubscribe（set_dynamic 触发回调）
 *   9. unsubscribe 后不再收到通知
 *  10. resource_changed 广播（on_resource_changed / off_resource_changed）
 *  11. set_parent 使父层变更传播到子层 subscribe() 订阅者
 *  12. 析构时自动断开父层订阅（无悬空引用）
 *  13. notify_resource_changed 手动广播
 *  14. set() 不触发通知（仅 set_dynamic 触发）
 */

#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>

#include <mine/ui/style/StyleAll.h>

using namespace mine::ui::style;
using namespace mine::core;

// ────────────────────────────────────────────────────────────────────────────
// 辅助：从 Variant 中取 int（假设存储了 int）
// ────────────────────────────────────────────────────────────────────────────

static int variant_int(const Variant& v) {
    return v.get<int>();
}

// ────────────────────────────────────────────────────────────────────────────
// 测试用例
// ────────────────────────────────────────────────────────────────────────────

TEST_CASE("ResourceDictionary - 本层写入与查找") {
    ResourceDictionary dict;
    dict.set("color", Variant{42});

    SUBCASE("find_local 命中") {
        auto v = dict.find_local("color");
        CHECK(v.has_value());
        CHECK(variant_int(v) == 42);
    }

    SUBCASE("find 命中") {
        auto v = dict.find("color");
        CHECK(v.has_value());
        CHECK(variant_int(v) == 42);
    }

    SUBCASE("未命中时返回空 Variant") {
        auto v = dict.find("not_exist");
        CHECK_FALSE(v.has_value());
    }

    SUBCASE("find_local 未命中时返回空 Variant") {
        auto v = dict.find_local("not_exist");
        CHECK_FALSE(v.has_value());
    }
}

TEST_CASE("ResourceDictionary - 覆盖写入") {
    ResourceDictionary dict;
    dict.set("x", Variant{1});
    dict.set("x", Variant{2});

    auto v = dict.find("x");
    CHECK(v.has_value());
    CHECK(variant_int(v) == 2);
}

TEST_CASE("ResourceDictionary - 树形查找：父层命中") {
    ResourceDictionary parent;
    ResourceDictionary child;

    parent.set("theme", Variant{100});
    child.set_parent(&parent);

    // 子层本层无命中，应向上查父层
    auto v = child.find("theme");
    CHECK(v.has_value());
    CHECK(variant_int(v) == 100);

    // find_local 不向上查
    auto vl = child.find_local("theme");
    CHECK_FALSE(vl.has_value());
}

TEST_CASE("ResourceDictionary - 树形查找：合并层命中") {
    ResourceDictionary merged_dict;
    ResourceDictionary dict;

    merged_dict.set("accent", Variant{55});
    dict.merge(merged_dict);

    auto v = dict.find("accent");
    CHECK(v.has_value());
    CHECK(variant_int(v) == 55);

    // find_local 不查合并层
    auto vl = dict.find_local("accent");
    CHECK_FALSE(vl.has_value());
}

TEST_CASE("ResourceDictionary - 本层优先于合并层") {
    ResourceDictionary merged_dict;
    ResourceDictionary dict;

    merged_dict.set("color", Variant{1});
    dict.set("color", Variant{2});
    dict.merge(merged_dict);

    auto v = dict.find("color");
    CHECK(v.has_value());
    CHECK(variant_int(v) == 2);  // 本层值优先
}

TEST_CASE("ResourceDictionary - 合并层优先于父层") {
    ResourceDictionary parent;
    ResourceDictionary merged_dict;
    ResourceDictionary dict;

    parent.set("key", Variant{10});
    merged_dict.set("key", Variant{20});

    dict.set_parent(&parent);
    dict.merge(merged_dict);

    auto v = dict.find("key");
    CHECK(v.has_value());
    CHECK(variant_int(v) == 20);  // 合并层优先
}

TEST_CASE("ResourceDictionary - clear_merged 后合并层不参与查找") {
    ResourceDictionary merged_dict;
    ResourceDictionary dict;

    merged_dict.set("k", Variant{99});
    dict.merge(merged_dict);

    CHECK(dict.find("k").has_value());

    dict.clear_merged();
    CHECK_FALSE(dict.find("k").has_value());
}

TEST_CASE("ResourceDictionary - set 不触发订阅通知") {
    ResourceDictionary dict;
    int call_count = 0;
    dict.subscribe("v", [&call_count](const Variant&) noexcept { ++call_count; });

    dict.set("v", Variant{1});
    CHECK(call_count == 0);  // set() 不触发通知
}

TEST_CASE("ResourceDictionary - set_dynamic 触发 subscribe 回调") {
    ResourceDictionary dict;
    int last_val = -1;

    dict.subscribe("color", [&last_val](const Variant& v) noexcept {
        last_val = v.get<int>();
    });

    dict.set_dynamic("color", Variant{7});
    CHECK(last_val == 7);

    dict.set_dynamic("color", Variant{42});
    CHECK(last_val == 42);
}

TEST_CASE("ResourceDictionary - unsubscribe 后不再收到通知") {
    ResourceDictionary dict;
    int call_count = 0;

    HandlerId id = dict.subscribe("k", [&call_count](const Variant&) noexcept {
        ++call_count;
    });

    dict.set_dynamic("k", Variant{1});
    CHECK(call_count == 1);

    dict.unsubscribe(id);
    dict.set_dynamic("k", Variant{2});
    CHECK(call_count == 1);  // 取消后不再触发
}

TEST_CASE("ResourceDictionary - resource_changed 广播") {
    ResourceDictionary dict;
    int changed_count  = 0;
    StringView last_key{};

    HandlerId id = dict.on_resource_changed([&](StringView key) noexcept {
        ++changed_count;
        last_key = key;
    });

    dict.set_dynamic("accent", Variant{5});
    CHECK(changed_count == 1);
    CHECK(last_key == "accent");

    dict.off_resource_changed(id);
    dict.set_dynamic("accent", Variant{6});
    CHECK(changed_count == 1);  // 取消后不再触发
}

TEST_CASE("ResourceDictionary - set_parent 使父层变更传播到子层订阅者") {
    ResourceDictionary parent;
    ResourceDictionary child;

    child.set_parent(&parent);

    int last_val = -1;
    child.subscribe("theme_color", [&last_val](const Variant& v) noexcept {
        last_val = v.get<int>();
    });

    // 父层动态写入 → 应传播到子层订阅者
    parent.set_dynamic("theme_color", Variant{123});
    CHECK(last_val == 123);
}

TEST_CASE("ResourceDictionary - 子层本层覆盖时父层变更不触发子层订阅者") {
    ResourceDictionary parent;
    ResourceDictionary child;

    // 子层本层覆盖了 "color"
    child.set("color", Variant{1});
    child.set_parent(&parent);

    int call_count = 0;
    child.subscribe("color", [&call_count](const Variant&) noexcept { ++call_count; });

    // 父层动态变更 "color"，但子层本层有覆盖，不应触发子层订阅者
    parent.set_dynamic("color", Variant{99});
    CHECK(call_count == 0);
}

TEST_CASE("ResourceDictionary - resource_changed 从父层向下传播") {
    ResourceDictionary parent;
    ResourceDictionary child;

    child.set_parent(&parent);

    int child_changed = 0;
    child.on_resource_changed([&child_changed](StringView) noexcept { ++child_changed; });

    parent.set_dynamic("k", Variant{1});
    CHECK(child_changed == 1);  // 父层变更通过 resource_changed 向下传播
}

TEST_CASE("ResourceDictionary - notify_resource_changed 手动广播") {
    ResourceDictionary dict;
    int call_count  = 0;
    StringView last_key{};

    dict.on_resource_changed([&](StringView key) noexcept {
        ++call_count;
        last_key = key;
    });

    // 手动广播（主题切换场景：merge 后手动通知）
    dict.merge(dict);  // 仅为了展示流程，实际无意义
    dict.notify_resource_changed("*");
    CHECK(call_count == 1);
    CHECK(last_key == "*");
}

TEST_CASE("ResourceDictionary - 析构时自动断开父层订阅") {
    ResourceDictionary parent;

    {
        ResourceDictionary child;
        child.set_parent(&parent);

        // 订阅父层广播，观察子层是否注册了订阅
        // 此时父层应有一个来自 child 的内部订阅
        parent.set_dynamic("k", Variant{1});
        // （child 析构后，父层的订阅应被自动取消）
    }

    // child 析构后，父层再次广播不应崩溃（悬空引用测试）
    int crash_guard = 0;
    parent.on_resource_changed([&crash_guard](StringView) noexcept { ++crash_guard; });
    parent.set_dynamic("k", Variant{2});
    CHECK(crash_guard == 1);  // 只有手动注册的那个回调触发，不崩溃
}

TEST_CASE("ResourceDictionary - 多层树形查找（三层）") {
    ResourceDictionary root;
    ResourceDictionary mid;
    ResourceDictionary leaf;

    root.set("r", Variant{1});
    mid.set("m", Variant{2});
    leaf.set("l", Variant{3});

    mid.set_parent(&root);
    leaf.set_parent(&mid);

    // leaf 可以查到所有层的键
    CHECK(leaf.find("l").has_value());
    CHECK(variant_int(leaf.find("l")) == 3);

    CHECK(leaf.find("m").has_value());
    CHECK(variant_int(leaf.find("m")) == 2);

    CHECK(leaf.find("r").has_value());
    CHECK(variant_int(leaf.find("r")) == 1);

    // 不存在的键
    CHECK_FALSE(leaf.find("x").has_value());
}
