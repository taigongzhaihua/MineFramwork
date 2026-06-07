/**
 * @file PaintTest.cpp
 * @brief mine.paint 模块单元测试。
 */

#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>

#include <mine/paint/Canvas.h>
#include <mine/paint/DisplayList.h>
#include <mine/core/StringView.h>
#include <mine/math/Vec2.h>

TEST_CASE("paint_Canvas_draw_text_长文本不会被512字节截断")
{
    mine::paint::Canvas canvas;

    mine::containers::InlineString long_text;
    for (int i = 0; i < 1200; ++i) {
        long_text.push_back('A');
    }

    // 此测试只验证 DisplayList 录制层，字体指针不会被解引用。
    void* dummy_font = reinterpret_cast<void*>(0x1);
    canvas.draw_text(
        mine::core::StringView{ long_text.data(), long_text.size() },
        mine::math::Vec2{ 10.0f, 20.0f },
        dummy_font,
        16.0f,
        mine::paint::Brush::solid_rgb(0xFFFFFF));

    mine::paint::DisplayList dl = canvas.end();
    REQUIRE(dl.text_run_count() == 1u);

    const auto runs = dl.text_runs();
    REQUIRE(runs.size() == 1u);
    CHECK(runs[0].utf8.size() == long_text.size());
    CHECK(mine::core::StringView{ runs[0].utf8.data(), runs[0].utf8.size() } ==
          mine::core::StringView{ long_text.data(), long_text.size() });
}
