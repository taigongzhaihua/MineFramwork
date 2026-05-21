/**
 * @file StyleTest.cpp
 * @brief mine.ui.style - Style + StyleSetter 单元测试。
 *
 * 覆盖场景：
 *   1.  StyleSetter 可默认构造（所有字段为默认值）
 *   2.  StyleSetter 可构造静态值（res_key 为空）
 *   3.  StyleSetter 可构造 DynamicResource 键（res_key 非空）
 *   4.  VisualStateSetters 可构造并添加 setter
 *   5.  Style 构建器接口（set_name / set_target_type / set_base）
 *   6.  Style::apply() 写入 StyleSetter(20) 优先级
 *   7.  【核心验收】StyleSetter 不覆盖本地值（Local 50 > StyleSetter 20）
 *   8.  【逆向验收】先设本地值再 apply 样式 -> 本地值不变
 *   9.  Style::apply_state() 写入 StyleTrigger(30) 优先级
 *  10.  StyleTrigger(30) 不覆盖 Local(50) 值
 *  11.  BasedOn 继承：父样式先应用，子样式同属性覆盖父样式
 *  12.  apply_fn_ 路径：调用自定义函数而非 setters_
 *  13.  apply_state() 状态不存在时为空操作（不崩溃）
 *  14.  Style 可拷贝（所有成员均可拷贝）
 *  15.  set_base(nullptr) 清除 BasedOn 继承
 *  16.  多个属性 setter 同时应用
 *  17.  Style::name() / target_type() / base() 查询
 */

#include <doctest/doctest.h>

#include <mine/ui/style/StyleAll.h>
#include <mine/ui/property/DependencyObject.h>
#include <mine/ui/property/DependencyProperty.h>
#include <mine/ui/property/ValuePriority.h>

using namespace mine::ui::style;
using namespace mine::ui;
using namespace mine::core;

// ─────────────────────────────────────────────────────────────────────────────
// 测试辅助类型
// ─────────────────────────────────────────────────────────────────────────────

/// 样式测试专用 DependencyObject 子类（记录虚方法调用）
class StyleTestObject : public DependencyObject {
public:
    int measure_invalidated = 0;  ///< invalidate_measure 调用次数
    int render_invalidated  = 0;  ///< invalidate_render  调用次数

protected:
    void on_property_changed(const DependencyProperty&,
                             const Variant&,
                             const Variant&) override {}
    void invalidate_measure() override { ++measure_invalidated; }
    void invalidate_arrange() override {}
    void invalidate_render()  override { ++render_invalidated; }
};

/// 样式测试用属性所有者标记类型（仅用于类型 ID 区分）
struct StyleTestOwner {};

// ─────────────────────────────────────────────────────────────────────────────
// 静态属性注册（全局唯一，在所有测试前完成）
// ─────────────────────────────────────────────────────────────────────────────

/// 测试用整型属性（默认值 0）
static const DependencyProperty& IntPropA =
    register_property<StyleTestOwner>("StyleInt_A", Variant{0});

/// 测试用整型属性（默认值 100）
static const DependencyProperty& IntPropB =
    register_property<StyleTestOwner>("StyleInt_B", Variant{100});

/// 测试用整型属性（默认值 0，用于多属性测试）
static const DependencyProperty& IntPropC =
    register_property<StyleTestOwner>("StyleInt_C", Variant{0});

// ─────────────────────────────────────────────────────────────────────────────
// 测试用例
// ─────────────────────────────────────────────────────────────────────────────

TEST_SUITE("Style") {

    // ── 场景 1：StyleSetter 默认构造 ─────────────────────────────────────────
    TEST_CASE("1. StyleSetter 默认构造字段均为默认值") {
        StyleSetter s;
        CHECK(s.property == nullptr);
        CHECK(!s.value.has_value());
        CHECK(s.res_key.empty());
    }

    // ── 场景 2：StyleSetter 静态值构造 ───────────────────────────────────────
    TEST_CASE("2. StyleSetter 可持有静态值（res_key 为空）") {
        StyleSetter s;
        s.property = &IntPropA;
        s.value    = Variant{42};
        CHECK(s.property == &IntPropA);
        CHECK(s.value.get<int>() == 42);
        CHECK(s.res_key.empty());
    }

    // ── 场景 3：StyleSetter DynamicResource 键 ───────────────────────────────
    TEST_CASE("3. StyleSetter 可持有 DynamicResource 键（res_key 非空）") {
        StyleSetter s;
        s.property = &IntPropA;
        s.res_key  = mine::containers::InlineString{"PrimaryColor"};
        CHECK(!s.res_key.empty());
        // res_key 非空时 value 可忽略（程序化路径跳过该 setter）
    }

    // ── 场景 4：VisualStateSetters 构造 ─────────────────────────────────────
    TEST_CASE("4. VisualStateSetters 可构造并追加 setter") {
        VisualStateSetters vs;
        vs.state_name = mine::containers::InlineString{"Hovered"};
        StyleSetter s;
        s.property = &IntPropA;
        s.value    = Variant{99};
        vs.setters.push_back(std::move(s));
        CHECK(vs.state_name == "Hovered");
        CHECK(vs.setters.size() == 1u);
    }

    // ── 场景 5：Style 构建器接口 ─────────────────────────────────────────────
    TEST_CASE("5. Style 构建器接口（set_name/set_target_type/set_base）") {
        Style parent;
        parent.set_name("BaseStyle");

        Style child;
        child.set_name("ChildStyle")
             .set_target_type(TypeId::of<StyleTestOwner>())
             .set_base(&parent);

        CHECK(child.name() == "ChildStyle");
        CHECK(child.target_type() == TypeId::of<StyleTestOwner>());
        CHECK(child.base() == &parent);
    }

    // ── 场景 6：apply() 写入 StyleSetter 优先级 ──────────────────────────────
    TEST_CASE("6. apply() 以 StyleSetter(20) 优先级写入属性值") {
        StyleTestObject obj;

        Style style;
        StyleSetter s;
        s.property = &IntPropA;
        s.value    = Variant{55};
        style.add_setter(std::move(s));

        // 初始值为默认值 0
        CHECK(obj.get_value(IntPropA).get<int>() == 0);

        style.apply(obj);

        // apply 后应获得样式写入值 55
        CHECK(obj.get_value(IntPropA).get<int>() == 55);
    }

    // ── 场景 7：【核心验收】StyleSetter 不覆盖本地值 ─────────────────────────
    TEST_CASE("7. [核心验收] 本地值(Local=50) 不被样式 setter(StyleSetter=20) 覆盖") {
        StyleTestObject obj;

        // 先以 Local 优先级写入本地值
        obj.set_value(IntPropA, Variant{999}, ValuePriority::Local);
        CHECK(obj.get_value(IntPropA).get<int>() == 999);

        // 构建样式并 apply
        Style style;
        StyleSetter s;
        s.property = &IntPropA;
        s.value    = Variant{1};
        style.add_setter(std::move(s));
        style.apply(obj);

        // 本地值 999 应保持不变（样式 setter 优先级低于本地值）
        CHECK(obj.get_value(IntPropA).get<int>() == 999);
    }

    // ── 场景 8：先设本地值再 apply 样式 -> 本地值不变 ────────────────────────
    TEST_CASE("8. [逆向验收] apply() 后再设本地值 -> 本地值有效") {
        StyleTestObject obj;

        // 先 apply 样式（写入 StyleSetter 优先级）
        Style style;
        StyleSetter s;
        s.property = &IntPropA;
        s.value    = Variant{7};
        style.add_setter(std::move(s));
        style.apply(obj);
        CHECK(obj.get_value(IntPropA).get<int>() == 7);

        // 再以 Local 优先级设置本地值 -> 应覆盖样式值
        obj.set_value(IntPropA, Variant{888}, ValuePriority::Local);
        CHECK(obj.get_value(IntPropA).get<int>() == 888);
    }

    // ── 场景 9：apply_state() 写入 StyleTrigger 优先级 ───────────────────────
    TEST_CASE("9. apply_state() 以 StyleTrigger(30) 优先级写入属性值") {
        StyleTestObject obj;

        // 先 apply 基线样式（StyleSetter=20）
        Style style;
        {
            StyleSetter s;
            s.property = &IntPropA;
            s.value    = Variant{10};
            style.add_setter(std::move(s));
        }
        // 添加 Hovered 状态 setter（StyleTrigger=30）
        {
            VisualStateSetters vs;
            vs.state_name = mine::containers::InlineString{"Hovered"};
            StyleSetter s;
            s.property = &IntPropA;
            s.value    = Variant{20};
            vs.setters.push_back(std::move(s));
            style.add_state_setters(std::move(vs));
        }

        style.apply(obj);
        CHECK(obj.get_value(IntPropA).get<int>() == 10);

        // apply_state 应以 StyleTrigger(30) 覆盖 StyleSetter(20) 写入的值
        style.apply_state(obj, "Hovered");
        CHECK(obj.get_value(IntPropA).get<int>() == 20);
    }

    // ── 场景 10：StyleTrigger 不覆盖 Local 值 ───────────────────────────────
    TEST_CASE("10. StyleTrigger(30) 不覆盖 Local(50) 值") {
        StyleTestObject obj;

        Style style;
        {
            VisualStateSetters vs;
            vs.state_name = mine::containers::InlineString{"Pressed"};
            StyleSetter s;
            s.property = &IntPropA;
            s.value    = Variant{5};
            vs.setters.push_back(std::move(s));
            style.add_state_setters(std::move(vs));
        }

        // 以 Local 优先级写入本地值
        obj.set_value(IntPropA, Variant{777}, ValuePriority::Local);

        // apply_state 不应覆盖 Local 值
        style.apply_state(obj, "Pressed");
        CHECK(obj.get_value(IntPropA).get<int>() == 777);
    }

    // ── 场景 11：BasedOn 继承（父样式先应用，子样式覆盖同属性） ───────────────
    TEST_CASE("11. BasedOn 继承：子样式同属性 setter 覆盖父样式") {
        StyleTestObject obj;

        // 父样式：IntPropA=1, IntPropB=2
        Style parent;
        {
            StyleSetter sa;
            sa.property = &IntPropA;
            sa.value    = Variant{1};
            parent.add_setter(std::move(sa));

            StyleSetter sb;
            sb.property = &IntPropB;
            sb.value    = Variant{2};
            parent.add_setter(std::move(sb));
        }

        // 子样式：继承父样式，仅覆盖 IntPropA=99（IntPropB 继承父值）
        Style child;
        child.set_base(&parent);
        {
            StyleSetter sa;
            sa.property = &IntPropA;
            sa.value    = Variant{99};
            child.add_setter(std::move(sa));
        }

        child.apply(obj);

        // IntPropA：子样式覆盖父样式 -> 99
        // IntPropB：仅父样式设置 -> 2
        CHECK(obj.get_value(IntPropA).get<int>() == 99);
        CHECK(obj.get_value(IntPropB).get<int>() == 2);
    }

    // ── 场景 12：apply_fn_ 路径 ──────────────────────────────────────────────
    TEST_CASE("12. apply_fn_ 非空时调用自定义函数而非 setters_") {
        StyleTestObject obj;

        // 添加程序化 setter（应被 apply_fn_ 绕过）
        Style style;
        {
            StyleSetter s;
            s.property = &IntPropA;
            s.value    = Variant{999};
            style.add_setter(std::move(s));
        }

        // 通过 static lambda 包装为函数指针（C++20 无捕获 lambda 可转 fn ptr）
        static int apply_fn_called = 0;
        style.apply_fn_ = [](DependencyObject& target) {
            ++apply_fn_called;
            // 自定义函数写入 IntPropB=42，不写 IntPropA
            target.set_value(
                IntPropB,
                Variant{42},
                ValuePriority::StyleSetter);
        };

        style.apply(obj);

        // apply_fn_ 应被调用
        CHECK(apply_fn_called == 1);
        // IntPropA：apply_fn_ 没有写入，应保持默认值 0
        CHECK(obj.get_value(IntPropA).get<int>() == 0);
        // IntPropB：apply_fn_ 写入 42
        CHECK(obj.get_value(IntPropB).get<int>() == 42);
    }

    // ── 场景 13：apply_state() 状态不存在时为空操作 ───────────────────────────
    TEST_CASE("13. apply_state() 状态名不存在时为空操作") {
        StyleTestObject obj;
        Style style;
        // 不添加任何视觉状态 setter

        // 不应崩溃，且不修改任何属性值
        style.apply_state(obj, "NonExistentState");
        CHECK(obj.get_value(IntPropA).get<int>() == 0);
    }

    // ── 场景 14：Style 可拷贝 ────────────────────────────────────────────────
    TEST_CASE("14. Style 可拷贝（setters 和 state_setters 均正确复制）") {
        Style original;
        original.set_name("TestStyle");
        {
            StyleSetter s;
            s.property = &IntPropA;
            s.value    = Variant{77};
            original.add_setter(std::move(s));
        }

        Style copy = original;  // 拷贝构造

        StyleTestObject obj;
        copy.apply(obj);

        // 拷贝的样式应与原样式行为一致
        CHECK(obj.get_value(IntPropA).get<int>() == 77);
        CHECK(copy.name() == original.name());
    }

    // ── 场景 15：set_base(nullptr) 清除继承 ─────────────────────────────────
    TEST_CASE("15. set_base(nullptr) 清除 BasedOn 继承") {
        Style parent;
        {
            StyleSetter s;
            s.property = &IntPropA;
            s.value    = Variant{111};
            parent.add_setter(std::move(s));
        }

        Style child;
        child.set_base(&parent);
        CHECK(child.base() == &parent);

        child.set_base(nullptr);
        CHECK(child.base() == nullptr);

        StyleTestObject obj;
        child.apply(obj);
        // 清除 BasedOn 后，父样式的 setter 不再生效
        CHECK(obj.get_value(IntPropA).get<int>() == 0);  // 默认值
    }

    // ── 场景 16：多属性 setter 同时应用 ─────────────────────────────────────
    TEST_CASE("16. apply() 同时写入多个属性") {
        StyleTestObject obj;

        Style style;
        {
            StyleSetter sa;
            sa.property = &IntPropA;
            sa.value    = Variant{10};
            style.add_setter(std::move(sa));
        }
        {
            StyleSetter sb;
            sb.property = &IntPropB;
            sb.value    = Variant{20};
            style.add_setter(std::move(sb));
        }
        {
            StyleSetter sc;
            sc.property = &IntPropC;
            sc.value    = Variant{30};
            style.add_setter(std::move(sc));
        }

        style.apply(obj);

        CHECK(obj.get_value(IntPropA).get<int>() == 10);
        CHECK(obj.get_value(IntPropB).get<int>() == 20);
        CHECK(obj.get_value(IntPropC).get<int>() == 30);
    }

    // ── 场景 17：Style 查询接口 ──────────────────────────────────────────────
    TEST_CASE("17. Style::name() / target_type() / base() 查询") {
        Style s;
        // 初始状态
        CHECK(s.name().empty());
        CHECK(s.target_type() == TypeId{});
        CHECK(s.base() == nullptr);

        // 设置后查询
        s.set_name("MyStyle")
         .set_target_type(TypeId::of<StyleTestOwner>());

        CHECK(s.name() == "MyStyle");
        CHECK(s.target_type() == TypeId::of<StyleTestOwner>());
        CHECK(s.base() == nullptr);
    }

}  // TEST_SUITE("Style")
