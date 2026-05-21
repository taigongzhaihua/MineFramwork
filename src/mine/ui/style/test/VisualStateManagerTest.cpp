/**
 * @file VisualStateManagerTest.cpp
 * @brief mine.ui.style - VisualStateManager 单元测试。
 *
 * 覆盖场景：
 *   1.  初始状态为空字符串
 *   2.  define_state 注册状态后，has_state 返回 true
 *   3.  go_to_state 目标未注册时返回 false，current_state 不变
 *   4.  go_to_state 首次切换成功，返回 true，current_state 更新
 *   5.  go_to_state 目标已是当前状态时返回 false
 *   6.  【核心验收】Normal → Hovered 正确触发
 *   7.  define_state 重复注册为幂等（不重复添加）
 *   8.  add_transition / has_transition 通配符 "*" 匹配
 *   9.  has_transition 精确 from 匹配
 *  10.  未注册的过渡 has_transition 返回 false
 *  11.  go_to_state 关联样式时调用 apply_state（StyleTrigger 写入）
 *  12.  set_style(nullptr) 后 go_to_state 不崩溃
 *  13.  attached_style() 返回设置的样式指针
 *  14.  VisualStateManager 可移动（set_visual_state_manager 场景）
 *  15.  多个状态依次切换，current_state 始终正确
 */

#include <doctest/doctest.h>

#include <mine/ui/style/StyleAll.h>
#include <mine/ui/property/DependencyObject.h>
#include <mine/ui/property/DependencyProperty.h>
#include <mine/ui/property/ValuePriority.h>
// Task #17：VSM 动画集成测试所需
#include <mine/ui/animation/AnimationAll.h>

using namespace mine::ui::style;
using namespace mine::ui;
using namespace mine::core;

// ─────────────────────────────────────────────────────────────────────────────
// 测试辅助类型
// ─────────────────────────────────────────────────────────────────────────────

/// VSM 测试专用 DependencyObject 子类（最小实现）
class VsmTestObject : public DependencyObject {
protected:
    void on_property_changed(const DependencyProperty&,
                             const Variant&,
                             const Variant&) override {}
    void invalidate_measure() override {}
    void invalidate_render()  override {}
};

/// 注册 VSM 测试专用依赖属性（全局唯一；使用完整 4 参数 API 避免 TOwner 推导）
static const DependencyProperty& vsm_test_prop()
{
    static const DependencyProperty* prop = nullptr;
    if (!prop) {
        prop = &register_property(
            "VSM_TestProp",
            TypeId{},   // owner_type（测试用，不绑定具体类型）
            TypeId{},   // value_type
            Variant{0}  // 默认值为 0
        );
    }
    return *prop;
}

/// 注册 VSM 动画测试专用 float 依赖属性（全局唯一；默认值 0.0f）
static const DependencyProperty& vsm_float_prop()
{
    static const DependencyProperty* prop = nullptr;
    if (!prop) {
        prop = &register_property(
            "VSM_AnimFloatProp",
            TypeId{},
            TypeId{},
            Variant{0.0f}
        );
    }
    return *prop;
}

/// 构造一个含 "Normal" 和 "Hovered" 两个状态的 VSM
static VisualStateManager make_two_state_vsm(VsmTestObject& obj)
{
    VisualStateManager vsm{obj};
    vsm.define_state("Normal");
    vsm.define_state("Hovered");
    return vsm;
}

// ─────────────────────────────────────────────────────────────────────────────
// 测试用例
// ─────────────────────────────────────────────────────────────────────────────

TEST_SUITE("VisualStateManager - 初始化与状态查询") {

    TEST_CASE("1. 初始 current_state 为空字符串") {
        VsmTestObject obj;
        VisualStateManager vsm{obj};
        CHECK(vsm.current_state().empty());
    }

    TEST_CASE("2. define_state 后 has_state 返回 true") {
        VsmTestObject obj;
        VisualStateManager vsm{obj};
        vsm.define_state("Normal");
        vsm.define_state("Hovered");
        CHECK(vsm.has_state("Normal"));
        CHECK(vsm.has_state("Hovered"));
        CHECK_FALSE(vsm.has_state("Pressed"));  // 未注册
    }

    TEST_CASE("3. go_to_state 目标未注册时返回 false，current_state 不变") {
        VsmTestObject obj;
        VisualStateManager vsm{obj};
        vsm.define_state("Normal");
        const bool result = vsm.go_to_state("Hovered");  // Hovered 未注册
        CHECK_FALSE(result);
        CHECK(vsm.current_state().empty());  // 未发生切换
    }

    TEST_CASE("4. go_to_state 首次切换成功") {
        VsmTestObject obj;
        auto vsm = make_two_state_vsm(obj);
        const bool result = vsm.go_to_state("Hovered");
        CHECK(result);
        CHECK(vsm.current_state() == "Hovered");
    }

    TEST_CASE("5. go_to_state 目标已是当前状态时返回 false") {
        VsmTestObject obj;
        auto vsm = make_two_state_vsm(obj);
        vsm.go_to_state("Normal");           // 首次切换到 Normal
        const bool result = vsm.go_to_state("Normal");  // 再次切换到 Normal
        CHECK_FALSE(result);
        CHECK(vsm.current_state() == "Normal");
    }

}  // TEST_SUITE

TEST_SUITE("VisualStateManager - 核心验收") {

    TEST_CASE("6. 【核心验收】Normal → Hovered 正确触发") {
        VsmTestObject obj;
        VisualStateManager vsm{obj};
        vsm.define_state("Normal");
        vsm.define_state("Hovered");
        vsm.define_state("Pressed");
        vsm.define_state("Disabled");

        // 先切换到 Normal（初始状态）
        CHECK(vsm.go_to_state("Normal"));
        CHECK(vsm.current_state() == "Normal");

        // Normal → Hovered
        CHECK(vsm.go_to_state("Hovered"));
        CHECK(vsm.current_state() == "Hovered");

        // Hovered → Normal
        CHECK(vsm.go_to_state("Normal"));
        CHECK(vsm.current_state() == "Normal");
    }

}  // TEST_SUITE

TEST_SUITE("VisualStateManager - 状态定义约束") {

    TEST_CASE("7. define_state 重复注册为幂等") {
        VsmTestObject obj;
        VisualStateManager vsm{obj};
        vsm.define_state("Normal");
        vsm.define_state("Normal");  // 重复
        vsm.define_state("Normal");  // 再次重复
        // has_state 应仍为 true（未重复添加导致崩溃）
        CHECK(vsm.has_state("Normal"));
        // 验证切换仍正常（状态只有一份）
        vsm.define_state("Hovered");
        CHECK(vsm.go_to_state("Normal"));
        CHECK(vsm.go_to_state("Hovered"));
    }

}  // TEST_SUITE

TEST_SUITE("VisualStateManager - 过渡注册与查询") {

    TEST_CASE("8. add_transition 通配符 \"*\" 匹配任意源状态") {
        VsmTestObject obj;
        VisualStateManager vsm{obj};
        vsm.define_state("Normal");
        vsm.define_state("Hovered");
        vsm.add_transition("*", "Hovered");
        // "*" 应匹配任意源状态（包括 "Normal"）
        CHECK(vsm.has_transition("Normal",  "Hovered"));
        CHECK(vsm.has_transition("Pressed", "Hovered"));
        CHECK(vsm.has_transition("",        "Hovered"));
    }

    TEST_CASE("9. add_transition 精确 from 匹配") {
        VsmTestObject obj;
        VisualStateManager vsm{obj};
        vsm.define_state("Normal");
        vsm.define_state("Hovered");
        vsm.add_transition("Normal", "Hovered");
        CHECK(vsm.has_transition("Normal",  "Hovered"));
        // "Pressed" → "Hovered" 未注册，不应匹配
        CHECK_FALSE(vsm.has_transition("Pressed", "Hovered"));
    }

    TEST_CASE("10. 未注册的过渡 has_transition 返回 false") {
        VsmTestObject obj;
        VisualStateManager vsm{obj};
        vsm.define_state("Normal");
        vsm.define_state("Hovered");
        // 未调用 add_transition
        CHECK_FALSE(vsm.has_transition("Normal", "Hovered"));
    }

}  // TEST_SUITE

TEST_SUITE("VisualStateManager - 样式 setter 集成") {

    TEST_CASE("11. go_to_state 关联样式时调用 apply_state（StyleTrigger 写入）") {
        // 准备：注册测试属性（全局唯一）
        const DependencyProperty& prop = vsm_test_prop();

        // 构造带有 "Hovered" 状态 setter 的样式
        StyleSetter setter;
        setter.property = &prop;
        setter.value    = Variant{999};  // Hovered 状态下值为 999

        VisualStateSetters state_setters;
        state_setters.state_name = "Hovered";  // InlineString 支持 operator=(StringView)
        state_setters.setters.push_back(setter);

        Style style;
        style.add_state_setters(std::move(state_setters));

        // 创建宿主对象（初始值为默认 0）
        VsmTestObject obj;
        CHECK(obj.get_value(prop).get<int>() == 0);  // 初始值

        // 构造 VSM 并关联样式
        VisualStateManager vsm{obj};
        vsm.define_state("Normal");
        vsm.define_state("Hovered");
        vsm.set_style(&style);

        // 切换到 Hovered：样式 setter 应以 StyleTrigger(30) 写入
        vsm.go_to_state("Normal");
        CHECK(vsm.go_to_state("Hovered"));

        // 验证属性值已被 StyleTrigger(30) 写入
        CHECK(obj.get_value(prop).get<int>() == 999);
    }

    TEST_CASE("12. set_style(nullptr) 后 go_to_state 不崩溃") {
        VsmTestObject obj;
        auto vsm = make_two_state_vsm(obj);
        vsm.set_style(nullptr);
        // 无样式关联时，go_to_state 仅更新 current_state_，不崩溃
        CHECK(vsm.go_to_state("Hovered"));
        CHECK(vsm.current_state() == "Hovered");
    }

    TEST_CASE("13. attached_style() 返回设置的样式指针") {
        VsmTestObject obj;
        VisualStateManager vsm{obj};
        CHECK(vsm.attached_style() == nullptr);  // 初始为 nullptr

        Style style;
        vsm.set_style(&style);
        CHECK(vsm.attached_style() == &style);

        vsm.set_style(nullptr);
        CHECK(vsm.attached_style() == nullptr);
    }

}  // TEST_SUITE

TEST_SUITE("VisualStateManager - 移动语义") {

    TEST_CASE("14. VisualStateManager 可移动（保留状态与过渡）") {
        VsmTestObject obj;
        VisualStateManager vsm1{obj};
        vsm1.define_state("Normal");
        vsm1.define_state("Hovered");
        vsm1.add_transition("*", "Hovered");
        vsm1.go_to_state("Normal");

        // 移动构造
        VisualStateManager vsm2{std::move(vsm1)};
        CHECK(vsm2.has_state("Normal"));
        CHECK(vsm2.has_state("Hovered"));
        CHECK(vsm2.has_transition("*", "Hovered"));
        CHECK(vsm2.current_state() == "Normal");

        // 移动后的 VSM 可继续使用
        CHECK(vsm2.go_to_state("Hovered"));
        CHECK(vsm2.current_state() == "Hovered");
    }

}  // TEST_SUITE

TEST_SUITE("VisualStateManager - 多状态切换") {

    TEST_CASE("15. 多个状态依次切换，current_state 始终正确") {
        VsmTestObject obj;
        VisualStateManager vsm{obj};
        vsm.define_state("Normal");
        vsm.define_state("Hovered");
        vsm.define_state("Pressed");
        vsm.define_state("Focused");
        vsm.define_state("Disabled");

        // Normal → Hovered → Pressed → Focused → Disabled → Normal
        vsm.go_to_state("Normal");
        CHECK(vsm.current_state() == "Normal");

        vsm.go_to_state("Hovered");
        CHECK(vsm.current_state() == "Hovered");

        vsm.go_to_state("Pressed");
        CHECK(vsm.current_state() == "Pressed");

        vsm.go_to_state("Focused");
        CHECK(vsm.current_state() == "Focused");

        vsm.go_to_state("Disabled");
        CHECK(vsm.current_state() == "Disabled");

        vsm.go_to_state("Normal");
        CHECK(vsm.current_state() == "Normal");
    }

}  // TEST_SUITE

// ════════════════════════════════════════════════════════════════════════════
// Task #17：VisualStateManager 动画集成测试
// ════════════════════════════════════════════════════════════════════════════

TEST_SUITE("VisualStateManager - Task #17 动画集成") {

    TEST_CASE("16. add_transition 带 configure_fn 后 has_transition 返回 true")
    {
        VsmTestObject obj;
        const DependencyProperty& prop = vsm_float_prop();

        VisualStateManager vsm{obj};
        vsm.define_state("Normal");
        vsm.define_state("Hovered");

        // 传入 configure_fn 重载
        vsm.add_transition("Normal", "Hovered",
            Function<void(animation::Storyboard&)>{
                [&obj, &prop](animation::Storyboard& sb) {
                    sb.animate_dp_to(obj, prop,
                                     Variant{1.0f},
                                     animation::Duration::milliseconds(100.0f));
                }
            });

        // 有 configure_fn 的过渡应被 has_transition 识别
        CHECK(vsm.has_transition("Normal", "Hovered"));
        CHECK_FALSE(vsm.has_transition("Hovered", "Normal")); // 未注册
    }

    TEST_CASE("17. go_to_state 带动画过渡时 Animation 优先级写入 from 值")
    {
        VsmTestObject obj;
        const DependencyProperty& prop = vsm_float_prop();

        // 设置 Local 层为 0.0f（from）
        obj.set_value(prop, Variant{0.0f}, ValuePriority::Local);

        VisualStateManager vsm{obj};
        vsm.define_state("Normal");
        vsm.define_state("Hovered");

        // 注册带动画的过渡：Normal → Hovered（to=1.0f，100ms）
        vsm.add_transition("Normal", "Hovered",
            Function<void(animation::Storyboard&)>{
                [&obj, &prop](animation::Storyboard& sb) {
                    sb.animate_dp_to(obj, prop,
                                     Variant{1.0f},
                                     animation::Duration::milliseconds(100.0f));
                }
            });

        // 先进入 Normal，再切换到 Hovered（触发动画）
        vsm.go_to_state("Normal");
        vsm.go_to_state("Hovered");

        // resolve_and_begin 应已向 Animation 层写入 from = 0.0f
        CHECK(obj.has_value(prop, ValuePriority::Animation));
        const Variant& val = obj.get_value(prop);
        CHECK(val.has<float>());
        CHECK(val.get<float>() == doctest::Approx(0.0f));  // Animation 层覆盖 Local
    }

    TEST_CASE("18. tick_animations 推进动画后值在 from-to 之间")
    {
        VsmTestObject obj;
        const DependencyProperty& prop = vsm_float_prop();

        obj.set_value(prop, Variant{0.0f}, ValuePriority::Local);

        VisualStateManager vsm{obj};
        vsm.define_state("Normal");
        vsm.define_state("Hovered");

        vsm.add_transition("Normal", "Hovered",
            Function<void(animation::Storyboard&)>{
                [&obj, &prop](animation::Storyboard& sb) {
                    sb.animate_dp_to(obj, prop,
                                     Variant{1.0f},
                                     animation::Duration::milliseconds(100.0f),
                                     animation::Linear);
                }
            });

        vsm.go_to_state("Normal");
        vsm.go_to_state("Hovered");  // 动画开始

        // 推进 50ms = 50% → 仍有活跃动画 → 返回 true
        bool still_active = vsm.tick_animations(0.05f);
        CHECK(still_active);

        // 中间值应约为 0.5f（Linear 插值）
        const Variant& val = obj.get_value(prop);
        CHECK(val.has<float>());
        CHECK(val.get<float>() == doctest::Approx(0.5f).epsilon(0.02f));
    }

    TEST_CASE("19. tick_animations 全部完成后返回 false")
    {
        VsmTestObject obj;
        const DependencyProperty& prop = vsm_float_prop();

        obj.set_value(prop, Variant{0.0f}, ValuePriority::Local);

        VisualStateManager vsm{obj};
        vsm.define_state("Normal");
        vsm.define_state("Hovered");

        vsm.add_transition("Normal", "Hovered",
            Function<void(animation::Storyboard&)>{
                [&obj, &prop](animation::Storyboard& sb) {
                    sb.animate_dp_to(obj, prop,
                                     Variant{1.0f},
                                     animation::Duration::milliseconds(100.0f));
                }
            });

        vsm.go_to_state("Normal");
        vsm.go_to_state("Hovered");

        // 一次性推进超过整个 duration（200ms > 100ms）
        bool still_active = vsm.tick_animations(0.2f);
        CHECK_FALSE(still_active);  // 所有动画已完成，无活跃动画
    }

    TEST_CASE("20. go_to_state 无 configure_fn 时直接 apply_state（立即跳变）")
    {
        VsmTestObject obj;
        const DependencyProperty& prop = vsm_float_prop();

        // 构建带 "Hovered" setter 的样式（setter 值 = 2.0f）
        StyleSetter setter;
        setter.property = &prop;
        setter.value    = Variant{2.0f};

        VisualStateSetters state_setters;
        state_setters.state_name = "Hovered";
        state_setters.setters.push_back(setter);

        Style style;
        style.add_state_setters(std::move(state_setters));

        // 注意：不设置 Local 层（Local=50 > StyleTrigger=30，会覆盖 apply_state 写入的值）
        // 默认值（Default 层）低于 StyleTrigger，因此 apply_state 写入后 get_value 可见

        VisualStateManager vsm{obj};
        vsm.define_state("Normal");
        vsm.define_state("Hovered");
        vsm.set_style(&style);

        // 仅用 add_transition（无 configure_fn）注册普通过渡
        vsm.add_transition("Normal", "Hovered");

        vsm.go_to_state("Normal");
        vsm.go_to_state("Hovered");  // 应立即跳变，无动画

        // Animation 层不应存在（立即跳变，无 Storyboard 启动）
        CHECK_FALSE(obj.has_value(prop, ValuePriority::Animation));

        // StyleTrigger 层已写入（apply_state 触发）
        CHECK(obj.has_value(prop, ValuePriority::StyleTrigger));
        CHECK(obj.get_value(prop).get<float>() == doctest::Approx(2.0f));
    }

    TEST_CASE("21. go_to_state 中断活跃动画并启动新动画")
    {
        VsmTestObject obj;
        const DependencyProperty& prop = vsm_float_prop();

        obj.set_value(prop, Variant{0.0f}, ValuePriority::Local);

        VisualStateManager vsm{obj};
        vsm.define_state("Normal");
        vsm.define_state("Hovered");
        vsm.define_state("Pressed");

        auto make_anim_fn = [&obj, &prop](float to_val) {
            return Function<void(animation::Storyboard&)>{
                [&obj, &prop, to_val](animation::Storyboard& sb) {
                    sb.animate_dp_to(obj, prop,
                                     Variant{to_val},
                                     animation::Duration::milliseconds(500.0f));
                }
            };
        };

        vsm.add_transition("Normal",  "Hovered", make_anim_fn(1.0f));
        vsm.add_transition("Hovered", "Pressed", make_anim_fn(0.5f));

        vsm.go_to_state("Normal");
        vsm.go_to_state("Hovered");  // 动画1：Normal→Hovered（500ms）

        // 推进 100ms（动画1 进行中）
        CHECK(vsm.tick_animations(0.1f));  // 仍活跃

        // 中途切换到 Pressed（应停止动画1，启动动画2）
        vsm.go_to_state("Pressed");

        // 动画2 刚启动，Animation 优先级应从新的 from 开始
        CHECK(obj.has_value(prop, ValuePriority::Animation));
        CHECK(vsm.current_state() == "Pressed");
    }

    TEST_CASE("22. tick_animations 无活跃动画时返回 false")
    {
        VsmTestObject obj;
        VisualStateManager vsm{obj};
        vsm.define_state("Normal");

        // 无任何动画
        bool still_active = vsm.tick_animations(0.016f);
        CHECK_FALSE(still_active);
    }

}  // TEST_SUITE（Task #17 动画集成）
