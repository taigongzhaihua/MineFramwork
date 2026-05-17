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

TEST_CASE("Math_Common_DegreesRadians") {
    const float clamped_low = clamp01(-1.0f);
    const float clamped_high = clamp01(2.0f);

    CHECK(is_near(radians(180.0f), 3.14159265f, 0.0001f));
    CHECK(is_near(degrees(3.14159265f), 180.0f, 0.001f));
    CHECK(clamped_low == doctest::Approx(0.0f));
    CHECK(clamped_high == doctest::Approx(1.0f));
    CHECK(lerp(10.0f, 20.0f, 0.25f) == doctest::Approx(12.5f));
}