/**
 * @file MathTest.cpp
 * @brief mine.math 模块单元测试（doctest）。
 */

#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>

#include <mine/math/Math.h>

using namespace mine::math;

namespace {

[[nodiscard]] bool is_near(float lhs, float rhs, float epsilon = 1.0e-5f) noexcept {
    return nearly_equal(lhs, rhs, epsilon);
}

} // namespace

TEST_CASE("Math_Vec2_BasicArithmetic") {
    Vec2 a{3.0f, 4.0f};
    Vec2 b{1.0f, 2.0f};

    CHECK(a + b == Vec2{4.0f, 6.0f});
    CHECK(a - b == Vec2{2.0f, 2.0f});
    CHECK(a * 2.0f == Vec2{6.0f, 8.0f});
    CHECK(a / 2.0f == Vec2{1.5f, 2.0f});
    CHECK(a.dot(b) == doctest::Approx(11.0f));
    CHECK(a.cross(b) == doctest::Approx(2.0f));
    CHECK(a.length() == doctest::Approx(5.0f));
}

TEST_CASE("Math_Vec2_Normalized") {
    Vec2 v{3.0f, 4.0f};
    Vec2 n = v.normalized();
    CHECK(n.x == doctest::Approx(0.6f));
    CHECK(n.y == doctest::Approx(0.8f));
}

TEST_CASE("Math_Vec3_CrossProduct") {
    Vec3 x = Vec3::unit_x();
    Vec3 y = Vec3::unit_y();
    Vec3 z = x.cross(y);
    CHECK(z == Vec3::unit_z());
    CHECK(x.dot(y) == doctest::Approx(0.0f));
}

TEST_CASE("Math_Vec4_DotAndLength") {
    Vec4 v{1.0f, 2.0f, 2.0f, 1.0f};
    CHECK(v.dot(v) == doctest::Approx(10.0f));
    CHECK(v.length() == doctest::Approx(std::sqrt(10.0f)));
}

TEST_CASE("Math_Point_And_Size") {
    Point p{10.0f, 20.0f};
    Vec2 delta{2.0f, -5.0f};
    Size s{30.0f, 15.0f};

    CHECK(p + delta == Point{12.0f, 15.0f});
    CHECK((p + delta) - p == delta);
    CHECK(s.area() == doctest::Approx(450.0f));
    CHECK(!s.empty());
}

TEST_CASE("Math_Rect_Contains_Intersects_Intersection") {
    Rect a{0.0f, 0.0f, 10.0f, 8.0f};
    Rect b{5.0f, 3.0f, 10.0f, 6.0f};

    CHECK(a.contains(Point{1.0f, 1.0f}));
    CHECK_FALSE(a.contains(Point{11.0f, 1.0f}));
    CHECK(a.intersects(b));

    Rect inter = a.intersection(b);
    CHECK(inter == Rect{5.0f, 3.0f, 5.0f, 5.0f});
}

TEST_CASE("Math_Rect_Union_Inflate_Deflate") {
    Rect a{2.0f, 2.0f, 4.0f, 4.0f};
    Rect b{0.0f, 3.0f, 3.0f, 3.0f};

    CHECK(a.union_with(b) == Rect{0.0f, 2.0f, 6.0f, 4.0f});
    CHECK(a.inflated(1.0f, 2.0f) == Rect{1.0f, 0.0f, 6.0f, 8.0f});
    CHECK(a.deflated(1.0f, 1.0f) == Rect{3.0f, 3.0f, 2.0f, 2.0f});
}

TEST_CASE("Math_RoundedRect_Contains") {
    RoundedRect rr{Rect{0.0f, 0.0f, 10.0f, 10.0f}, 3.0f};

    CHECK(rr.contains(Point{5.0f, 5.0f}));
    CHECK(rr.contains(Point{1.0f, 1.0f}));
    CHECK_FALSE(rr.contains(Point{0.0f, 0.0f}));
}

TEST_CASE("Math_Mat3_Translation") {
    Mat3 translation = Mat3::translation(5.0f, -2.0f);
    Vec2 result = translation.transform_point({2.0f, 3.0f});
    CHECK(result == Vec2{7.0f, 1.0f});
}

TEST_CASE("Math_Mat3_Scaling_And_Vector") {
    Mat3 scaling = Mat3::scaling(2.0f, 3.0f);
    CHECK(scaling.transform_vector({4.0f, 5.0f}) == Vec2{8.0f, 15.0f});
}

TEST_CASE("Math_Mat3_Rotation") {
    Mat3 rotation = Mat3::rotation(radians(90.0f));
    Vec2 result = rotation.transform_vector({1.0f, 0.0f});
    CHECK(result.x == doctest::Approx(0.0f).epsilon(0.0001f));
    CHECK(result.y == doctest::Approx(1.0f).epsilon(0.0001f));
}

TEST_CASE("Math_Mat3_Multiplication_Order") {
    Mat3 combined = Mat3::translation(10.0f, 5.0f) * Mat3::scaling(2.0f, 2.0f);
    Vec2 result = combined.transform_point({1.0f, 1.0f});
    CHECK(result == Vec2{12.0f, 7.0f});
}

TEST_CASE("Math_Mat3_Inverse") {
    Mat3 matrix = Mat3::translation(4.0f, -3.0f) * Mat3::rotation(radians(45.0f));
    auto inverse = matrix.inverted();
    REQUIRE(inverse);

    Vec2 point{8.0f, 5.0f};
    Vec2 mapped = matrix.transform_point(point);
    Vec2 restored = inverse->transform_point(mapped);

    CHECK(restored.x == doctest::Approx(point.x).epsilon(0.0001f));
    CHECK(restored.y == doctest::Approx(point.y).epsilon(0.0001f));
}

TEST_CASE("Math_Mat3_Inverse_FailsForSingular") {
    Mat3 singular{
        1.0f, 2.0f, 3.0f,
        2.0f, 4.0f, 6.0f,
        0.0f, 0.0f, 0.0f,
    };
    auto inverse = singular.inverted();
    CHECK_FALSE(inverse.ok());
}

TEST_CASE("Math_Mat4_Translation_AndScaling") {
    Mat4 matrix = Mat4::translation(5.0f, -2.0f, 3.0f) * Mat4::scaling(2.0f, 3.0f, 4.0f);
    Vec3 point = matrix.transform_point({1.0f, 2.0f, 3.0f});

    CHECK(point.x == doctest::Approx(7.0f));
    CHECK(point.y == doctest::Approx(4.0f));
    CHECK(point.z == doctest::Approx(15.0f));
}

TEST_CASE("Math_Mat4_RotationZ") {
    Mat4 rotation = Mat4::rotation_z(radians(90.0f));
    Vec3 vector = rotation.transform_vector({1.0f, 0.0f, 0.0f});
    CHECK(vector.x == doctest::Approx(0.0f).epsilon(0.0001f));
    CHECK(vector.y == doctest::Approx(1.0f).epsilon(0.0001f));
    CHECK(vector.z == doctest::Approx(0.0f).epsilon(0.0001f));
}

TEST_CASE("Math_Color_FromRgbAndPack") {
    Color color = Color::from_rgb_u32(0x3366CCu);
    CHECK(color.r == doctest::Approx(0.2f));
    CHECK(color.g == doctest::Approx(0.4f));
    CHECK(color.b == doctest::Approx(0.8f));
    CHECK(color.a == doctest::Approx(1.0f));
    CHECK(color.to_rgba8() == 0x3366CCFFu);
}

TEST_CASE("Math_Color_Clamp_AndAlpha") {
    Color color{1.2f, -0.5f, 0.25f, 2.0f};
    Color clamped = color.clamped();
    CHECK(clamped == Color{1.0f, 0.0f, 0.25f, 1.0f});
    CHECK(clamped.with_alpha(0.5f) == Color{1.0f, 0.0f, 0.25f, 0.5f});
}

TEST_CASE("Math_Color_Premultiplied_AndBlend") {
    Color src{1.0f, 0.0f, 0.0f, 0.5f};
    Color dst{0.0f, 0.0f, 1.0f, 1.0f};

    CHECK(src.premultiplied() == Color{0.5f, 0.0f, 0.0f, 0.5f});

    Color blended = src.blend_over(dst);
    CHECK(blended.r == doctest::Approx(0.5f));
    CHECK(blended.g == doctest::Approx(0.0f));
    CHECK(blended.b == doctest::Approx(0.5f));
    CHECK(blended.a == doctest::Approx(1.0f));
}

TEST_CASE("Math_Transform2D_TranslationScaleComposition") {
    Transform2D transform = Transform2D::translation(10.0f, 5.0f) * Transform2D::scale(2.0f);
    Point result = transform.apply(Point{1.0f, 1.0f});
    CHECK(result == Point{12.0f, 7.0f});
}

TEST_CASE("Math_Transform2D_Rotation") {
    Transform2D transform = Transform2D::rotation(radians(90.0f));
    Point result = transform.apply(Point{1.0f, 0.0f});
    CHECK(result.x == doctest::Approx(0.0f).epsilon(0.0001f));
    CHECK(result.y == doctest::Approx(1.0f).epsilon(0.0001f));
}

TEST_CASE("Math_Transform2D_RotationAboutPivot") {
    Transform2D transform = Transform2D::rotation_about(radians(90.0f), Point{1.0f, 1.0f});
    Point result = transform.apply(Point{2.0f, 1.0f});
    CHECK(result.x == doctest::Approx(1.0f).epsilon(0.0001f));
    CHECK(result.y == doctest::Approx(2.0f).epsilon(0.0001f));
}

TEST_CASE("Math_Transform2D_Inverse") {
    Transform2D transform =
        Transform2D::translation(10.0f, -3.0f) *
        Transform2D::rotation(radians(30.0f)) *
        Transform2D::scale(2.0f, 4.0f);
    auto inverse = transform.inverted();
    REQUIRE(inverse);

    Point original{3.0f, 5.0f};
    Point transformed = transform.apply(original);
    Point restored = inverse->apply(transformed);

    CHECK(restored.x == doctest::Approx(original.x).epsilon(0.0001f));
    CHECK(restored.y == doctest::Approx(original.y).epsilon(0.0001f));
}

TEST_CASE("Math_Transform2D_ApplyRect") {
    Transform2D transform = Transform2D::translation(5.0f, -2.0f) * Transform2D::scale(2.0f, 3.0f);
    Rect result = transform.apply(Rect{1.0f, 2.0f, 4.0f, 5.0f});
    CHECK(result == Rect{7.0f, 4.0f, 8.0f, 15.0f});
}

TEST_CASE("Math_Thickness_Properties") {
    Thickness t{2.0f, 4.0f, 6.0f, 8.0f};
    CHECK(t.horizontal() == doctest::Approx(8.0f));   // left + right
    CHECK(t.vertical()   == doctest::Approx(12.0f));  // top + bottom
    CHECK_FALSE(t.is_uniform());
    CHECK_FALSE(t.is_zero());

    Thickness u = Thickness::uniform(3.0f);
    CHECK(u.is_uniform());
    CHECK(u.horizontal() == doctest::Approx(6.0f));

    Thickness s = Thickness::symmetric(5.0f, 2.0f);
    CHECK(s.left  == doctest::Approx(5.0f));
    CHECK(s.top   == doctest::Approx(2.0f));
    CHECK(s.right == doctest::Approx(5.0f));
    CHECK(s.bottom == doctest::Approx(2.0f));
}

TEST_CASE("Math_Rect_Deflate_Inflate_Thickness") {
    Rect r{10.0f, 10.0f, 100.0f, 80.0f};
    Thickness t{5.0f, 3.0f, 7.0f, 4.0f}; // left, top, right, bottom

    Rect deflated = r.deflated(t);
    CHECK(deflated == Rect{15.0f, 13.0f, 88.0f, 73.0f});

    Rect inflated = r.inflated(t);
    CHECK(inflated == Rect{5.0f, 7.0f, 112.0f, 87.0f});
}

TEST_CASE("Math_ComplexRoundedRect_InnerRect") {
    // 外圆角矩形 40x30，四角半径各不同（from_corners → rx==ry）
    ComplexRoundedRect outer{
        Rect{0.0f, 0.0f, 40.0f, 30.0f},
        CornerRadii::from_corners(10.0f, 8.0f, 6.0f, 4.0f)
    };
    Thickness border{2.0f, 3.0f, 2.0f, 3.0f}; // left=2, top=3, right=2, bottom=3

    ComplexRoundedRect inner = outer.inner_rect(border);

    // 内侧矩形
    CHECK(inner.rect == Rect{2.0f, 3.0f, 36.0f, 24.0f});
    // 内侧圆角：外侧减去对应边厚度
    CHECK(inner.radii.top_left.x    == doctest::Approx(8.0f));  // 10 - left=2
    CHECK(inner.radii.top_left.y    == doctest::Approx(7.0f));  // 10 - top=3
    CHECK(inner.radii.top_right.x   == doctest::Approx(6.0f));  // 8  - right=2
    CHECK(inner.radii.top_right.y   == doctest::Approx(5.0f));  // 8  - top=3
    CHECK(inner.radii.bottom_right.x == doctest::Approx(4.0f)); // 6  - right=2
    CHECK(inner.radii.bottom_right.y == doctest::Approx(3.0f)); // 6  - bottom=3
    CHECK(inner.radii.bottom_left.x  == doctest::Approx(2.0f)); // 4  - left=2
    CHECK(inner.radii.bottom_left.y  == doctest::Approx(1.0f)); // 4  - bottom=3
}

TEST_CASE("Math_ComplexRoundedRect_InnerRect_RadiusClampedToZero") {
    // 边框厚度超过圆角半径时，内侧圆角归零（而非负数）
    ComplexRoundedRect outer{
        Rect{0.0f, 0.0f, 20.0f, 20.0f},
        CornerRadii::uniform(3.0f)
    };
    ComplexRoundedRect inner = outer.inner_rect(Thickness::uniform(5.0f)); // 5 > 3

    CHECK(inner.radii.top_left.x    == doctest::Approx(0.0f));
    CHECK(inner.radii.top_right.y   == doctest::Approx(0.0f));
    CHECK(inner.radii.bottom_right.x == doctest::Approx(0.0f));
}

TEST_CASE("Math_CornerRadii_Uniform_And_Predicates") {
    // uniform(r) 四角相同
    CornerRadii all_same = CornerRadii::uniform(5.0f);
    CHECK(all_same.is_uniform());
    CHECK_FALSE(all_same.is_zero());
    CHECK(all_same.top_left  == Vec2{5.0f, 5.0f});
    CHECK(all_same.top_right == Vec2{5.0f, 5.0f});

    // is_zero
    CornerRadii all_zero{};
    CHECK(all_zero.is_zero());

    // from_corners 四角不同
    CornerRadii mixed = CornerRadii::from_corners(4.0f, 8.0f, 0.0f, 2.0f);
    CHECK_FALSE(mixed.is_uniform());
    CHECK(mixed.top_left    == Vec2{4.0f, 4.0f});
    CHECK(mixed.top_right   == Vec2{8.0f, 8.0f});
    CHECK(mixed.bottom_right == Vec2{0.0f, 0.0f});
    CHECK(mixed.bottom_left == Vec2{2.0f, 2.0f});
}

TEST_CASE("Math_ComplexRoundedRect_ClampRadii") {
    // top_left.x + top_right.x = 8 + 8 = 16 > 10 → 缩放因子 = 10/16 = 0.625
    ComplexRoundedRect crr{
        Rect{0.0f, 0.0f, 10.0f, 10.0f},
        CornerRadii::from_corners(8.0f, 8.0f, 0.0f, 0.0f)
    };
    // 预期所有角均缩放为 8*0.625 = 5
    CHECK(crr.radii.top_left.x   == doctest::Approx(5.0f));
    CHECK(crr.radii.top_right.x  == doctest::Approx(5.0f));
    // 未超限的角（0）保持不变
    CHECK(crr.radii.bottom_right.x == doctest::Approx(0.0f));
}

TEST_CASE("Math_ComplexRoundedRect_Contains_DifferentCorners") {
    // 左上 5x5 圆角，右上直角，右下 10x10 圆角，左下直角
    ComplexRoundedRect crr{
        Rect{0.0f, 0.0f, 20.0f, 20.0f},
        CornerRadii{
            {5.0f,  5.0f},   // top_left：圆角
            {0.0f,  0.0f},   // top_right：直角
            {10.0f, 10.0f},  // bottom_right：圆角
            {0.0f,  0.0f}    // bottom_left：直角
        }
    };

    // 中心点在内
    CHECK(crr.contains(Point{10.0f, 10.0f}));
    // 右上角 — 直角，点 (19.5, 0.5) 在内
    CHECK(crr.contains(Point{19.5f, 0.5f}));
    // 左下角 — 直角，点 (0.5, 19.5) 在内
    CHECK(crr.contains(Point{0.5f, 19.5f}));
    // 左上角圆角区域内侧（椭圆内）
    CHECK(crr.contains(Point{1.5f, 1.5f}));
    // 左上角圆角区域外侧（椭圆外）— (0,0) 极角处
    CHECK_FALSE(crr.contains(Point{0.0f, 0.0f}));
    // 右下角圆角区域内侧
    CHECK(crr.contains(Point{15.0f, 15.0f}));
    // 右下角圆角区域外侧（接近 (20,20) 极角处）
    CHECK_FALSE(crr.contains(Point{19.5f, 19.5f}));
}

TEST_CASE("Math_ComplexRoundedRect_Translated") {
    ComplexRoundedRect crr{
        Rect{0.0f, 0.0f, 10.0f, 10.0f},
        CornerRadii::from_corners(2.0f, 3.0f, 4.0f, 1.0f)
    };
    ComplexRoundedRect moved = crr.translated(Vec2{5.0f, -3.0f});
    CHECK(moved.rect == Rect{5.0f, -3.0f, 10.0f, 10.0f});
    // 圆角半径不变
    CHECK(moved.radii == crr.radii);
}

TEST_CASE("Math_Common_DegreesRadians") {
    const float clamped_low = clamp01(-1.0f);
    const float clamped_high = clamp01(2.0f);

    CHECK(is_near(radians(180.0f), 3.14159265f, 0.0001f));
    CHECK(is_near(degrees(3.14159265f), 180.0f, 0.001f));
    CHECK(clamped_low == doctest::Approx(0.0f));
    CHECK(clamped_high == doctest::Approx(1.0f));
    CHECK(lerp(10.0f, 20.0f, 0.25f) == doctest::Approx(12.5f));
}