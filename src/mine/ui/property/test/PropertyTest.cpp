/**
 * @file PropertyTest.cpp
 * @brief mine.ui.property 模块单元测试。
 *
 * 测试覆盖：
 *   - DependencyProperty 注册与查询
 *   - DependencyObject get/set/clear_value（含优先级覆盖）
 *   - 属性变更通知（on_property_changed 虚方法 + 外部订阅）
 *   - 防递归保护（通知回调内再次 set_value 被忽略）
 *   - 附加属性注册与使用
 *   - clear_value 后退回默认值
 */

#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>

#include <mine/ui/property/Property.h>

using namespace mine::ui;
using namespace mine::core;

// ────────────────────────────────────────────────────────────────────────────
// 测试辅助类型
// ────────────────────────────────────────────────────────────────────────────

/**
 * @brief 测试用 DependencyObject 子类，记录 on_property_changed 调用信息。
 */
class TestObject : public DependencyObject {
public:
    /// 记录最近一次属性变更通知信息
    struct ChangeRecord {
        const DependencyProperty* prop      = nullptr;
        Variant                   old_value;
        Variant                   new_value;
    };

    int          changed_count = 0;  ///< on_property_changed 被调用次数
    ChangeRecord last_change;        ///< 最后一次变更记录

    /// 记录 invalidate_measure 调用次数
    int measure_invalidated = 0;
    int arrange_invalidated = 0;
    int render_invalidated  = 0;

protected:
    void on_property_changed(const DependencyProperty& prop,
                             const Variant&            old_v,
                             const Variant&            new_v) override {
        ++changed_count;
        last_change = {&prop, old_v, new_v};
    }

    void invalidate_measure() override { ++measure_invalidated; }
    void invalidate_arrange() override { ++arrange_invalidated; }
    void invalidate_render()  override { ++render_invalidated; }
};

// ────────────────────────────────────────────────────────────────────────────
// 静态属性注册（全局，在测试运行前完成）
// ────────────────────────────────────────────────────────────────────────────

/// 测试用普通属性（默认值 42，affects_measure = true）
static const DependencyProperty& WidthProp =
    register_property("Width",
                      TypeId::of<TestObject>(),
                      TypeId::of<int>(),
                      Variant{42},
                      PropertyMetadata{.affects_measure = true});

/// 测试用普通属性（默认值为空 Variant，affects_render = true）
static const DependencyProperty& LabelProp =
    register_property("Label",
                      TypeId::of<TestObject>(),
                      TypeId::of<int>(),
                      Variant{},
                      PropertyMetadata{.affects_render = true});

/// 附加属性（用于测试 is_attached()）
static const DependencyProperty& ColumnProp =
    register_attached_property("Column",
                               TypeId::of<TestObject>(),
                               TypeId::of<int>(),
                               Variant{0});

// ────────────────────────────────────────────────────────────────────────────
// DependencyProperty 注册
// ────────────────────────────────────────────────────────────────────────────

TEST_CASE("ui_property_Register_普通属性_基本属性正确") {
    CHECK(WidthProp.name()   == StringView{"Width"});
    CHECK(WidthProp.owner_type() == TypeId::of<TestObject>());
    CHECK(WidthProp.value_type() == TypeId::of<int>());
    CHECK(WidthProp.is_attached() == false);
    CHECK(WidthProp.metadata().affects_measure == true);
    CHECK(WidthProp.metadata().affects_arrange == false);
    CHECK(WidthProp.default_value().has<int>());
    CHECK(WidthProp.default_value().get<int>() == 42);
}

TEST_CASE("ui_property_Register_附加属性_IsAttachedTrue") {
    CHECK(ColumnProp.is_attached() == true);
    CHECK(ColumnProp.default_value().has<int>());
    CHECK(ColumnProp.default_value().get<int>() == 0);
}

TEST_CASE("ui_property_DependencyProperty_身份比较_地址即身份") {
    CHECK(WidthProp == WidthProp);
    CHECK(WidthProp != LabelProp);
    CHECK(&WidthProp != &LabelProp);
}

// ────────────────────────────────────────────────────────────────────────────
// DependencyObject get_value / set_value
// ────────────────────────────────────────────────────────────────────────────

TEST_CASE("ui_property_GetValue_无本地值_返回属性默认值") {
    TestObject obj;
    const Variant& v = obj.get_value(WidthProp);
    REQUIRE(v.has<int>());
    CHECK(v.get<int>() == 42);
}

TEST_CASE("ui_property_SetValue_写入本地值_GetValue返回新值") {
    TestObject obj;
    obj.set_value(WidthProp, Variant{100});
    const Variant& v = obj.get_value(WidthProp);
    REQUIRE(v.has<int>());
    CHECK(v.get<int>() == 100);
}

TEST_CASE("ui_property_SetValue_触发OnPropertyChanged") {
    TestObject obj;
    obj.set_value(WidthProp, Variant{99});

    CHECK(obj.changed_count == 1);
    REQUIRE(obj.last_change.prop == &WidthProp);
    // 变更前值应为属性默认值 42
    REQUIRE(obj.last_change.old_value.has<int>());
    CHECK(obj.last_change.old_value.get<int>() == 42);
    // 变更后值应为新写入的 99
    REQUIRE(obj.last_change.new_value.has<int>());
    CHECK(obj.last_change.new_value.get<int>() == 99);
}

TEST_CASE("ui_property_SetValue_affects_measure_触发InvalidateMeasure") {
    TestObject obj;
    obj.set_value(WidthProp, Variant{50});  // WidthProp.affects_measure = true
    CHECK(obj.measure_invalidated == 1);
    CHECK(obj.arrange_invalidated == 0);
    CHECK(obj.render_invalidated  == 0);
}

TEST_CASE("ui_property_SetValue_affects_render_触发InvalidateRender") {
    TestObject obj;
    obj.set_value(LabelProp, Variant{1});  // LabelProp.affects_render = true
    CHECK(obj.render_invalidated  == 1);
    CHECK(obj.measure_invalidated == 0);
}

// ────────────────────────────────────────────────────────────────────────────
// 值优先级覆盖
// ────────────────────────────────────────────────────────────────────────────

TEST_CASE("ui_property_ValuePriority_高优先级覆盖低优先级") {
    TestObject obj;

    // 先写入 StyleSetter 优先级（低）
    obj.set_value(WidthProp, Variant{10}, ValuePriority::StyleSetter);
    CHECK(obj.get_value(WidthProp).get<int>() == 10);

    // 再写入 Local 优先级（高）→ 生效值应变为 20
    obj.set_value(WidthProp, Variant{20}, ValuePriority::Local);
    CHECK(obj.get_value(WidthProp).get<int>() == 20);
}

TEST_CASE("ui_property_ValuePriority_低优先级不覆盖高优先级") {
    TestObject obj;

    // 先写入 Local 优先级（高）
    obj.set_value(WidthProp, Variant{20}, ValuePriority::Local);
    int changed_before = obj.changed_count;

    // 再写入 StyleSetter 优先级（低）→ 生效值不变，不触发通知
    obj.set_value(WidthProp, Variant{5}, ValuePriority::StyleSetter);
    CHECK(obj.get_value(WidthProp).get<int>() == 20);
    CHECK(obj.changed_count == changed_before);  // 无新通知
}

TEST_CASE("ui_property_ValuePriority_动画值最高优先级") {
    TestObject obj;
    obj.set_value(WidthProp, Variant{100}, ValuePriority::Local);
    obj.set_value(WidthProp, Variant{200}, ValuePriority::Animation);

    CHECK(obj.get_value(WidthProp).get<int>() == 200);
}

// ────────────────────────────────────────────────────────────────────────────
// clear_value
// ────────────────────────────────────────────────────────────────────────────

TEST_CASE("ui_property_ClearValue_清除生效槽_退回到下一优先级") {
    TestObject obj;
    obj.set_value(WidthProp, Variant{10}, ValuePriority::StyleSetter);
    obj.set_value(WidthProp, Variant{20}, ValuePriority::Local);

    // 清除 Local（最高优先级）→ 应退回到 StyleSetter 的值
    obj.clear_value(WidthProp, ValuePriority::Local);
    CHECK(obj.get_value(WidthProp).get<int>() == 10);
}

TEST_CASE("ui_property_ClearValue_清除所有槽_退回到默认值") {
    TestObject obj;
    obj.set_value(WidthProp, Variant{99});
    obj.clear_value(WidthProp, ValuePriority::Local);

    // 无任何有效槽 → 返回属性默认值 42
    const Variant& v = obj.get_value(WidthProp);
    REQUIRE(v.has<int>());
    CHECK(v.get<int>() == 42);
}

TEST_CASE("ui_property_ClearValue_清除不存在的槽_空操作无通知") {
    TestObject obj;
    int before = obj.changed_count;
    obj.clear_value(WidthProp, ValuePriority::StyleSetter);  // 未设置此优先级
    CHECK(obj.changed_count == before);
}

// ────────────────────────────────────────────────────────────────────────────
// has_value
// ────────────────────────────────────────────────────────────────────────────

TEST_CASE("ui_property_HasValue_未设置_返回false") {
    TestObject obj;
    CHECK(obj.has_value(WidthProp) == false);
}

TEST_CASE("ui_property_HasValue_已设置本地值_返回true") {
    TestObject obj;
    obj.set_value(WidthProp, Variant{5});
    CHECK(obj.has_value(WidthProp, ValuePriority::Local) == true);
}

// ────────────────────────────────────────────────────────────────────────────
// 属性变更事件订阅
// ────────────────────────────────────────────────────────────────────────────

TEST_CASE("ui_property_Subscribe_收到属性变更通知") {
    TestObject obj;

    int  call_count = 0;

    // 注册订阅（每次 WidthProp 变更就递增 call_count）
    uint32_t token = obj.subscribe_property_changed(
        [](DependencyObject*, const DependencyProperty& p,
           const Variant&, const Variant&, void* ud) {
            if (p == WidthProp) {
                ++*static_cast<int*>(ud);
            }
        },
        &call_count);
    CHECK(token != 0u);

    obj.set_value(WidthProp, Variant{7});
    CHECK(call_count == 1);

    obj.unsubscribe_property_changed(token);
    obj.set_value(WidthProp, Variant{8});
    CHECK(call_count == 1);  // 取消订阅后不再回调
}

// ────────────────────────────────────────────────────────────────────────────
// 防递归保护
// ────────────────────────────────────────────────────────────────────────────

TEST_CASE("ui_property_SetValue_回调内递归调用SetValue_被忽略") {
    TestObject obj;

    // 在 on_property_changed 期间再次 set_value 会被防递归保护忽略
    int  recursive_depth = 0;

    // 订阅：每次收到通知时尝试再次 set_value（应被忽略）
    [[maybe_unused]] uint32_t token2 = obj.subscribe_property_changed(
        [](DependencyObject* sender,
           const DependencyProperty& p,
           const Variant&,
           const Variant& /*new_v*/,
           void* ud) {
            int& depth = *static_cast<int*>(ud);
            ++depth;
            if (depth == 1) {
                // 尝试递归调用（应被防递归标志阻止）
                sender->set_value(p, Variant{999});
            }
        },
        &recursive_depth);

    obj.set_value(WidthProp, Variant{55});

    // on_property_changed 应只被调用一次（初始那次）
    CHECK(obj.changed_count == 1);
    // 最终生效值应为 55（递归的 999 被忽略）
    CHECK(obj.get_value(WidthProp).get<int>() == 55);
    CHECK(recursive_depth == 1);
}

// ────────────────────────────────────────────────────────────────────────────
// 附加属性：可被任意 DependencyObject 持有
// ────────────────────────────────────────────────────────────────────────────

TEST_CASE("ui_property_AttachedProperty_可被任意对象持有") {
    TestObject obj;

    // 初始为默认值 0
    CHECK(obj.get_value(ColumnProp).get<int>() == 0);

    // 设置附加属性
    obj.set_value(ColumnProp, Variant{3});
    CHECK(obj.get_value(ColumnProp).get<int>() == 3);
}
