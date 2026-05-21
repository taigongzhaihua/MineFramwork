/**
 * @file AnimationTest.cpp
 * @brief mine.ui.animation 模块单元测试。
 *
 * 测试覆盖：
 *   1. Duration：工厂函数、转换、算术、比较
 *   2. 标量缓动函数：端点值、单调性、帧序列关键值（Linear/Quad/Cubic/Quart/Quint）
 *   3. 超调型缓动函数：Back/Elastic/Bounce 输出范围及特征
 *   4. Expo/Sine：边界值及近似正确性
 *   5. CubicBezier：线性退化、CSS ease 预置、帧序列值
 *   6. SpringEasing：欠阻尼振荡、临界阻尼无振荡、is_settled、retarget 不丢速度
 */

#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>

#include <mine/ui/animation/AnimationAll.h>

using namespace mine::ui::animation;

// ════════════════════════════════════════════════════════════════════════════
// Duration 测试
// ════════════════════════════════════════════════════════════════════════════

TEST_CASE("animation_Duration_毫秒工厂转换正确")
{
    Duration d = Duration::milliseconds(120.0f);
    CHECK(d.to_milliseconds() == doctest::Approx(120.0f));
    CHECK(d.to_seconds()      == doctest::Approx(0.12f));
}

TEST_CASE("animation_Duration_秒工厂转换正确")
{
    Duration d = Duration::seconds(1.5f);
    CHECK(d.to_seconds()      == doctest::Approx(1.5f));
    CHECK(d.to_milliseconds() == doctest::Approx(1500.0f));
}

TEST_CASE("animation_Duration_默认构造为零时长")
{
    Duration d;
    CHECK(d.is_zero());
    CHECK(d.to_seconds() == doctest::Approx(0.0f));
}

TEST_CASE("animation_Duration_算术运算正确")
{
    Duration a = Duration::milliseconds(100.0f);
    Duration b = Duration::milliseconds(50.0f);
    CHECK((a + b).to_milliseconds() == doctest::Approx(150.0f));
    CHECK((a - b).to_milliseconds() == doctest::Approx(50.0f));
    CHECK((a * 2.0f).to_milliseconds() == doctest::Approx(200.0f));
}

TEST_CASE("animation_Duration_比较运算正确")
{
    Duration a = Duration::milliseconds(100.0f);
    Duration b = Duration::milliseconds(200.0f);
    CHECK(a < b);
    CHECK(b > a);
    CHECK(a <= a);
    CHECK(a == a);
    CHECK(a != b);
}

// ════════════════════════════════════════════════════════════════════════════
// 线性缓动
// ════════════════════════════════════════════════════════════════════════════

TEST_CASE("animation_Linear_端点与中点正确")
{
    CHECK(linear(0.0f) == doctest::Approx(0.0f));
    CHECK(linear(0.5f) == doctest::Approx(0.5f));
    CHECK(linear(1.0f) == doctest::Approx(1.0f));
}

TEST_CASE("animation_Linear_函数指针常量可用")
{
    // 验证全局函数指针常量能正常调用
    CHECK(Linear(0.5f) == doctest::Approx(0.5f));
}

// ════════════════════════════════════════════════════════════════════════════
// Quad 缓动
// ════════════════════════════════════════════════════════════════════════════

TEST_CASE("animation_QuadEaseIn_端点及帧序列值正确")
{
    CHECK(quad_ease_in(0.0f) == doctest::Approx(0.0f));
    CHECK(quad_ease_in(1.0f) == doctest::Approx(1.0f));
    // t=0.5 时，In：0.5² = 0.25（比 Linear 慢）
    CHECK(quad_ease_in(0.5f) == doctest::Approx(0.25f));
    // In 曲线在前半段比 Linear 慢
    CHECK(quad_ease_in(0.5f) < 0.5f);
}

TEST_CASE("animation_QuadEaseOut_端点及帧序列值正确")
{
    CHECK(quad_ease_out(0.0f) == doctest::Approx(0.0f));
    CHECK(quad_ease_out(1.0f) == doctest::Approx(1.0f));
    // t=0.5 时，Out：0.5*(2-0.5) = 0.75（比 Linear 快）
    CHECK(quad_ease_out(0.5f) == doctest::Approx(0.75f));
    // Out 曲线在前半段比 Linear 快
    CHECK(quad_ease_out(0.5f) > 0.5f);
}

TEST_CASE("animation_QuadEaseInOut_对称性验证")
{
    CHECK(quad_ease_in_out(0.0f) == doctest::Approx(0.0f));
    CHECK(quad_ease_in_out(1.0f) == doctest::Approx(1.0f));
    // 中点值应为 0.5（关于 (0.5,0.5) 点对称）
    CHECK(quad_ease_in_out(0.5f) == doctest::Approx(0.5f));
    // 前半段比 Linear 慢（加速段）
    CHECK(quad_ease_in_out(0.25f) < 0.25f);
    // 后半段比 Linear 快（减速段）
    CHECK(quad_ease_in_out(0.75f) > 0.75f);
}

// ════════════════════════════════════════════════════════════════════════════
// Cubic 缓动
// ════════════════════════════════════════════════════════════════════════════

TEST_CASE("animation_CubicEaseIn_端点及帧序列值正确")
{
    CHECK(cubic_ease_in(0.0f) == doctest::Approx(0.0f));
    CHECK(cubic_ease_in(1.0f) == doctest::Approx(1.0f));
    // t=0.5：0.5³ = 0.125
    CHECK(cubic_ease_in(0.5f) == doctest::Approx(0.125f));
    // Cubic In 比 Quad In 在中点时更慢
    CHECK(cubic_ease_in(0.5f) < quad_ease_in(0.5f));
}

TEST_CASE("animation_CubicEaseOut_端点及帧序列值正确")
{
    CHECK(cubic_ease_out(0.0f) == doctest::Approx(0.0f));
    CHECK(cubic_ease_out(1.0f) == doctest::Approx(1.0f));
    // t=0.5：1 + (0.5-1)³ = 1 + (-0.5)³ = 1 - 0.125 = 0.875
    CHECK(cubic_ease_out(0.5f) == doctest::Approx(0.875f));
    // Cubic Out 比 Quad Out 在中点时更快
    CHECK(cubic_ease_out(0.5f) > quad_ease_out(0.5f));
}

TEST_CASE("animation_CubicEaseInOut_对称性验证")
{
    CHECK(cubic_ease_in_out(0.0f) == doctest::Approx(0.0f));
    CHECK(cubic_ease_in_out(1.0f) == doctest::Approx(1.0f));
    CHECK(cubic_ease_in_out(0.5f) == doctest::Approx(0.5f));
}

TEST_CASE("animation_CubicEaseOut_函数指针常量可用")
{
    // 验证 CubicEaseOut 常量
    CHECK(CubicEaseOut(0.5f) == doctest::Approx(0.875f));
    CHECK(CubicEaseIn(0.5f)  == doctest::Approx(0.125f));
}

// ════════════════════════════════════════════════════════════════════════════
// Quart 和 Quint 缓动
// ════════════════════════════════════════════════════════════════════════════

TEST_CASE("animation_Quart_端点值正确")
{
    CHECK(quart_ease_in(0.0f)  == doctest::Approx(0.0f));
    CHECK(quart_ease_in(1.0f)  == doctest::Approx(1.0f));
    CHECK(quart_ease_out(0.0f) == doctest::Approx(0.0f));
    CHECK(quart_ease_out(1.0f) == doctest::Approx(1.0f));
}

TEST_CASE("animation_Quart_In_比Cubic更慢")
{
    // Quart In 在中点时比 Cubic In 更慢（更强的加速）
    CHECK(quart_ease_in(0.5f) < cubic_ease_in(0.5f));
    // Quart Out 在中点时比 Cubic Out 更快
    CHECK(quart_ease_out(0.5f) > cubic_ease_out(0.5f));
}

TEST_CASE("animation_Quint_帧序列值正确")
{
    // t=0.5：0.5^5 = 0.03125
    CHECK(quint_ease_in(0.5f)  == doctest::Approx(0.03125f));
    // t=0.5：1 + (0.5-1)^5 = 1 - 0.03125 = 0.96875
    CHECK(quint_ease_out(0.5f) == doctest::Approx(0.96875f));
}

// ════════════════════════════════════════════════════════════════════════════
// Expo 缓动（使用近似比较，精度 1e-4）
// ════════════════════════════════════════════════════════════════════════════

TEST_CASE("animation_ExpoEaseIn_边界及单调性")
{
    CHECK(expo_ease_in(0.0f) == doctest::Approx(0.0f));
    CHECK(expo_ease_in(1.0f) == doctest::Approx(1.0f));
    // 单调递增
    CHECK(expo_ease_in(0.3f) < expo_ease_in(0.5f));
    CHECK(expo_ease_in(0.5f) < expo_ease_in(0.7f));
    // In 系列：中点值远小于 0.5
    CHECK(expo_ease_in(0.5f) < 0.1f);
}

TEST_CASE("animation_ExpoEaseOut_边界及单调性")
{
    CHECK(expo_ease_out(0.0f) == doctest::Approx(0.0f));
    CHECK(expo_ease_out(1.0f) == doctest::Approx(1.0f));
    // Out 系列：中点值远大于 0.5
    CHECK(expo_ease_out(0.5f) > 0.9f);
}

// ════════════════════════════════════════════════════════════════════════════
// Sine 缓动
// ════════════════════════════════════════════════════════════════════════════

TEST_CASE("animation_SineEaseOut_近似值正确")
{
    // SineEaseOut(t) = sin(t * π/2)
    // t=1 → sin(π/2) = 1
    CHECK(sine_ease_out(1.0f) == doctest::Approx(1.0f).epsilon(1e-5f));
    CHECK(sine_ease_out(0.0f) == doctest::Approx(0.0f).epsilon(1e-5f));
    // 在中点比 Linear 更快（Out 特征）
    CHECK(sine_ease_out(0.5f) > 0.5f);
}

// ════════════════════════════════════════════════════════════════════════════
// Back 缓动（超调验证）
// ════════════════════════════════════════════════════════════════════════════

TEST_CASE("animation_BackEaseOut_端点及超调验证")
{
    CHECK(back_ease_out(0.0f) == doctest::Approx(0.0f));
    CHECK(back_ease_out(1.0f) == doctest::Approx(1.0f));
    // Back Out：在到达 1.0 之前会先超过 1.0（超调），然后回落
    // 在大约 t=0.7~0.9 时会超过 1.0
    bool has_overshoot = false;
    for (int i = 60; i <= 95; ++i) {
        if (back_ease_out(static_cast<float>(i) / 100.0f) > 1.0f) {
            has_overshoot = true;
            break;
        }
    }
    CHECK(has_overshoot);
}

TEST_CASE("animation_BackEaseIn_端点及超调验证")
{
    CHECK(back_ease_in(0.0f) == doctest::Approx(0.0f));
    CHECK(back_ease_in(1.0f) == doctest::Approx(1.0f));
    // Back In：在起始时会先向负方向运动（下冲）
    bool has_undershoot = false;
    for (int i = 1; i <= 20; ++i) {
        if (back_ease_in(static_cast<float>(i) / 100.0f) < 0.0f) {
            has_undershoot = true;
            break;
        }
    }
    CHECK(has_undershoot);
}

// ════════════════════════════════════════════════════════════════════════════
// Elastic 缓动
// ════════════════════════════════════════════════════════════════════════════

TEST_CASE("animation_ElasticEaseOut_端点及振荡特征")
{
    CHECK(elastic_ease_out(0.0f) == doctest::Approx(0.0f));
    CHECK(elastic_ease_out(1.0f) == doctest::Approx(1.0f));
    // Elastic Out 会在结尾振荡（出现超过 1.0 再回落的值）
    bool has_overshoot = false;
    for (int i = 1; i <= 80; ++i) {
        if (elastic_ease_out(static_cast<float>(i) / 100.0f) > 1.0f) {
            has_overshoot = true;
            break;
        }
    }
    CHECK(has_overshoot);
}

// ════════════════════════════════════════════════════════════════════════════
// Bounce 缓动
// ════════════════════════════════════════════════════════════════════════════

TEST_CASE("animation_BounceEaseOut_端点及输出始终非负")
{
    CHECK(bounce_ease_out(0.0f) == doctest::Approx(0.0f));
    CHECK(bounce_ease_out(1.0f) == doctest::Approx(1.0f));
    // Bounce Out 输出始终在 [0, 1] 内（不超调）
    for (int i = 0; i <= 100; ++i) {
        const float v = bounce_ease_out(static_cast<float>(i) / 100.0f);
        CHECK(v >= -0.001f);
        CHECK(v <= 1.001f);
    }
}

TEST_CASE("animation_BounceEaseOut_第二弹跳值正确")
{
    // BounceEaseOut 在 t ∈ [1/2.75, 2/2.75) 时：v = 7.5625*(t-1.5/2.75)^2 + 0.75
    // 中点约 t=0.5454：v ≈ 7.5625*(0.5454-0.5454)^2 + 0.75 = 0.75
    CHECK(bounce_ease_out(2.0f / 2.75f * 0.5f + 1.0f / 2.75f * 0.5f)
          >= 0.74f);
}

// ════════════════════════════════════════════════════════════════════════════
// CubicBezier 测试
// ════════════════════════════════════════════════════════════════════════════

TEST_CASE("animation_CubicBezier_线性退化为Linear")
{
    // 控制点在对角线上 → 等价线性缓动
    CubicBezier cb{ 0.0f, 0.0f, 1.0f, 1.0f };
    // 端点
    CHECK(cb.solve(0.0f) == doctest::Approx(0.0f).epsilon(1e-4f));
    CHECK(cb.solve(1.0f) == doctest::Approx(1.0f).epsilon(1e-4f));
    // 中点
    CHECK(cb.solve(0.5f) == doctest::Approx(0.5f).epsilon(0.01f));
}

TEST_CASE("animation_CubicBezier_端点始终为零和一")
{
    CubicBezier ease = CubicBezier::Ease;
    CHECK(ease.solve(0.0f) == doctest::Approx(0.0f));
    CHECK(ease.solve(1.0f) == doctest::Approx(1.0f));
}

TEST_CASE("animation_CubicBezier_EaseOut前半段比Linear快")
{
    // ease-out：{0.0, 0.0, 0.58, 1.0} 前半段比 Linear 快
    CubicBezier ease_out = CubicBezier::EaseOut;
    CHECK(ease_out.solve(0.3f) > 0.3f);
}

TEST_CASE("animation_CubicBezier_EaseIn前半段比Linear慢")
{
    // ease-in：{0.42, 0.0, 1.0, 1.0} 前半段比 Linear 慢
    CubicBezier ease_in = CubicBezier::EaseIn;
    CHECK(ease_in.solve(0.3f) < 0.3f);
}

TEST_CASE("animation_CubicBezier_静态预置常量可用")
{
    // 验证所有静态预置常量的端点
    CHECK(CubicBezier::Ease.solve(0.0f)      == doctest::Approx(0.0f));
    CHECK(CubicBezier::Ease.solve(1.0f)      == doctest::Approx(1.0f));
    CHECK(CubicBezier::EaseIn.solve(0.0f)    == doctest::Approx(0.0f));
    CHECK(CubicBezier::EaseIn.solve(1.0f)    == doctest::Approx(1.0f));
    CHECK(CubicBezier::EaseOut.solve(0.0f)   == doctest::Approx(0.0f));
    CHECK(CubicBezier::EaseOut.solve(1.0f)   == doctest::Approx(1.0f));
    CHECK(CubicBezier::EaseInOut.solve(0.0f) == doctest::Approx(0.0f));
    CHECK(CubicBezier::EaseInOut.solve(1.0f) == doctest::Approx(1.0f));
}

// ════════════════════════════════════════════════════════════════════════════
// SpringEasing 测试
// ════════════════════════════════════════════════════════════════════════════

TEST_CASE("animation_SpringEasing_初始位置在起点")
{
    SpringEasing spring{ 1.0f, 200.0f, 20.0f };
    spring.reset(0.0f, 1.0f, 0.0f);
    CHECK(spring.current() == doctest::Approx(0.0f));
    CHECK(spring.target()  == doctest::Approx(1.0f));
}

TEST_CASE("animation_SpringEasing_步进后向目标移动")
{
    SpringEasing spring{ 1.0f, 200.0f, 20.0f };
    spring.reset(0.0f, 1.0f, 0.0f);
    // 第一次步进后位置应大于 0（向目标方向移动）
    float v = spring.step(1.0f / 60.0f);
    CHECK(v > 0.0f);
    CHECK(v < 1.0f);
}

TEST_CASE("animation_SpringEasing_欠阻尼最终稳定在目标")
{
    // 欠阻尼弹簧（ζ < 1）会振荡但最终稳定
    SpringEasing spring{ 1.0f, 200.0f, 10.0f };  // 阻尼比较小，振荡明显
    spring.reset(0.0f, 1.0f, 0.0f);

    // 最多模拟 5 秒（300 帧 @60fps），应该稳定
    constexpr float dt = 1.0f / 60.0f;
    bool settled = false;
    for (int i = 0; i < 300; ++i) {
        spring.step(dt);
        if (spring.is_settled(0.01f, 0.01f)) {
            settled = true;
            break;
        }
    }
    CHECK(settled);
    CHECK(spring.current() == doctest::Approx(1.0f).epsilon(0.01f));
}

TEST_CASE("animation_SpringEasing_临界阻尼无振荡最快到达")
{
    // 临界阻尼：ζ = damping / (2 * sqrt(k * m)) = 1
    // 对于 m=1, k=100：ζ=1 → damping = 2*sqrt(100) = 20
    SpringEasing critical{ 1.0f, 100.0f, 20.0f };  // ζ ≈ 1.0
    critical.reset(0.0f, 1.0f, 0.0f);

    // 检查阻尼比接近 1（临界）
    CHECK(critical.damping_ratio() == doctest::Approx(1.0f).epsilon(0.01f));

    // 模拟 60 帧，应无过冲（位置不超过目标的 1% 以上）
    constexpr float dt = 1.0f / 60.0f;
    float max_overshoot = 0.0f;
    for (int i = 0; i < 60; ++i) {
        float v = critical.step(dt);
        if (v > 1.0f) {
            max_overshoot = v - 1.0f;
        }
    }
    // 临界阻尼不应有明显过冲
    CHECK(max_overshoot < 0.01f);
}

TEST_CASE("animation_SpringEasing_retarget保持当前速度")
{
    SpringEasing spring{ 1.0f, 200.0f, 20.0f };
    spring.reset(0.0f, 1.0f, 0.0f);

    // 步进一段时间获取速度
    constexpr float dt = 1.0f / 60.0f;
    for (int i = 0; i < 10; ++i) {
        spring.step(dt);
    }
    float vel_before = spring.velocity();
    float pos_before = spring.current();

    // 改变目标
    spring.retarget(2.0f);

    // 速度和位置应保持不变
    CHECK(spring.velocity() == doctest::Approx(vel_before));
    CHECK(spring.current()  == doctest::Approx(pos_before));
    CHECK(spring.target()   == doctest::Approx(2.0f));
}

TEST_CASE("animation_SpringEasing_is_settled零位移零速度时为真")
{
    SpringEasing spring{ 1.0f, 200.0f, 20.0f };
    spring.reset(1.0f, 1.0f, 0.0f);  // 起点 == 目标，速度为 0
    CHECK(spring.is_settled());
}

TEST_CASE("animation_SpringEasing_参数查询正确")
{
    SpringEasing spring{ 2.0f, 300.0f, 15.0f };
    CHECK(spring.mass()      == doctest::Approx(2.0f));
    CHECK(spring.stiffness() == doctest::Approx(300.0f));
    CHECK(spring.damping()   == doctest::Approx(15.0f));

    // 自然频率 = sqrt(300/2) = sqrt(150) ≈ 12.247
    CHECK(spring.natural_frequency() == doctest::Approx(std::sqrt(300.0f / 2.0f)).epsilon(1e-4f));

    // 阻尼比 = 15 / (2 * sqrt(300 * 2)) ≈ 0.306
    const float expected_zeta = 15.0f / (2.0f * std::sqrt(300.0f * 2.0f));
    CHECK(spring.damping_ratio() == doctest::Approx(expected_zeta).epsilon(1e-4f));
}

TEST_CASE("animation_SpringEasing_帧序列单调移向目标（过阻尼）")
{
    // 过阻尼：ζ > 1，无振荡，单调向目标移动
    // m=1, k=100, damping=30 → ζ = 30/(2*10) = 1.5
    SpringEasing overdamped{ 1.0f, 100.0f, 30.0f };
    overdamped.reset(0.0f, 1.0f, 0.0f);

    CHECK(overdamped.damping_ratio() > 1.0f);

    constexpr float dt = 1.0f / 60.0f;
    float prev = overdamped.current();
    bool monotonic = true;
    for (int i = 0; i < 120; ++i) {
        float curr = overdamped.step(dt);
        if (curr < prev - 0.001f) {
            // 过阻尼不应出现回退（超调）
            monotonic = false;
            break;
        }
        prev = curr;
    }
    CHECK(monotonic);
}

TEST_CASE("animation_SpringEasing_estimated_settle_time大于零")
{
    SpringEasing spring{ 1.0f, 200.0f, 20.0f };
    CHECK(spring.estimated_settle_time() > 0.0f);
}

// ════════════════════════════════════════════════════════════════════════════
// 伞形头文件可用性验证
// ════════════════════════════════════════════════════════════════════════════

TEST_CASE("animation_AnimationAll_伞形头文件包含所有类型")
{
    // 验证通过 AnimationAll.h 可访问所有核心类型
    Duration d = Duration::milliseconds(100.0f);
    CHECK(!d.is_zero());

    float v = CubicEaseOut(0.5f);
    CHECK(v > 0.0f);

    CubicBezier cb = CubicBezier::Ease;
    CHECK(cb.solve(0.0f) == doctest::Approx(0.0f));

    SpringEasing spring;
    spring.reset(0.0f, 1.0f);
    CHECK(spring.target() == doctest::Approx(1.0f));
}
