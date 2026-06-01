/**
 * @file LayoutTest.cpp
 * @brief mine.ui.layout 模块单元测试。
 *
 * 覆盖场景：
 *   FrameworkElement — 基本布局属性：
 *     - Width/Height/MinWidth/MaxWidth/MinHeight/MaxHeight 默认值
 *     - Margin 默认值为零厚度
 *     - HorizontalAlignment / VerticalAlignment 默认为 Stretch
 *   FrameworkElement — Measure 协议：
 *     - 无约束时 desired_size == {0, 0}（叶子元素默认 measure_override 返回零）
 *     - 设置明确 Width/Height 后 desired_size 等于 Width/Height（无 Margin）
 *     - Margin 会增大 desired_size
 *   FrameworkElement — Arrange 协议：
 *     - Stretch 对齐：bounds_rect 充满 slot（减去 Margin）
 *     - Left/Center/Right 对齐：bounds_rect 水平位置正确
 *     - Top/Center/Bottom 对齐：bounds_rect 垂直位置正确
 *     - Margin 正确偏移 bounds_rect
 *   StackPanel — 垂直排列：
 *     - 两个子元素 desired_size 累加（高度方向）
 *     - Arrange 后各子元素 bounds_rect.y 依次排列
 *   StackPanel — 水平排列：
 *     - 两个子元素 desired_size 累加（宽度方向）
 *     - Arrange 后各子元素 bounds_rect.x 依次排列
 *   Grid — 基本像素行列：
 *     - 1行1列，Pixel，子元素 arrange 后 bounds_rect 正确
 *   Grid — Auto 行列：
 *     - 行/列尺寸等于子元素期望尺寸
 *   Grid — Star 行列：
 *     - 两列 1* 1*，各占可用宽度的一半
 *   Grid — 跨行列（RowSpan/ColumnSpan）：
 *     - 子元素 slot 宽度 = 两列合并宽度之和
 *   Panel — 子节点管理：
 *     - add_child / children_count / child_at
 *     - remove_child 正确移除
 *     - 重复 add_child 不重复添加
 */

#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>

#include <mine/ui/layout/LayoutAll.h>

using namespace mine::ui;
using namespace mine::math;

// ============================================================================
// 测试辅助：可指定 desired_size 的叶子元素
// ============================================================================

namespace {

/**
 * @brief 测试用叶子 FrameworkElement，measure_override 返回预设尺寸。
 */
class FixedLeaf : public FrameworkElement {
public:
    explicit FixedLeaf(float w = 0.0f, float h = 0.0f)
        : fixed_w_{w}, fixed_h_{h}
    {}

protected:
    Size measure_override(Size /*available*/) override
    {
        return {fixed_w_, fixed_h_};
    }

private:
    float fixed_w_;
    float fixed_h_;
};

} // anonymous namespace

// ============================================================================
// FrameworkElement — 默认属性值
// ============================================================================

TEST_CASE("layout_FE_默认属性值") {
    FrameworkElement fe;

    // Width/Height 默认为 NaN（表示 Auto）
    CHECK(std::isnan(fe.width()));
    CHECK(std::isnan(fe.height()));

    // Min 默认 0，Max 默认正无穷
    CHECK(fe.min_width()  == doctest::Approx(0.0f));
    CHECK(fe.max_width()  == std::numeric_limits<float>::infinity());
    CHECK(fe.min_height() == doctest::Approx(0.0f));
    CHECK(fe.max_height() == std::numeric_limits<float>::infinity());

    // Margin 默认全零
    const Thickness m = fe.margin();
    CHECK(m.left   == doctest::Approx(0.0f));
    CHECK(m.top    == doctest::Approx(0.0f));
    CHECK(m.right  == doctest::Approx(0.0f));
    CHECK(m.bottom == doctest::Approx(0.0f));

    // 对齐默认 Stretch
    CHECK(fe.horizontal_alignment() == HorizontalAlignment::Stretch);
    CHECK(fe.vertical_alignment()   == VerticalAlignment::Stretch);
}

// ============================================================================
// FrameworkElement — Measure 协议
// ============================================================================

TEST_CASE("layout_FE_Measure_叶子默认期望零尺寸") {
    FrameworkElement fe;
    fe.measure({800.0f, 600.0f});
    const Size ds = fe.desired_size();
    CHECK(ds.width  == doctest::Approx(0.0f));
    CHECK(ds.height == doctest::Approx(0.0f));
}

TEST_CASE("layout_FE_Measure_明确Width和Height") {
    FixedLeaf leaf;
    leaf.set_width(120.0f);
    leaf.set_height(80.0f);
    leaf.measure({800.0f, 600.0f});

    // desired_size 应等于明确设定的 Width/Height
    const Size ds = leaf.desired_size();
    CHECK(ds.width  == doctest::Approx(120.0f));
    CHECK(ds.height == doctest::Approx(80.0f));
}

TEST_CASE("layout_FE_Measure_Margin增大desiredSize") {
    FixedLeaf leaf{100.0f, 50.0f};
    leaf.set_margin({10.0f, 5.0f, 10.0f, 5.0f});  // left, top, right, bottom

    leaf.measure({800.0f, 600.0f});
    const Size ds = leaf.desired_size();

    // desired_size 包含 Margin：width = 100+10+10 = 120，height = 50+5+5 = 60
    CHECK(ds.width  == doctest::Approx(120.0f));
    CHECK(ds.height == doctest::Approx(60.0f));
}

TEST_CASE("layout_FE_Measure_MinWidth约束生效") {
    FixedLeaf leaf{10.0f, 10.0f};  // 期望 10x10
    leaf.set_min_width(50.0f);
    leaf.measure({800.0f, 600.0f});

    // min_width 覆盖 measure_override 返回的 10
    CHECK(leaf.desired_size().width == doctest::Approx(50.0f));
}

TEST_CASE("layout_FE_Measure_MaxHeight约束生效") {
    FixedLeaf leaf{0.0f, 200.0f};  // 期望 200 高
    leaf.set_max_height(100.0f);
    leaf.measure({800.0f, 600.0f});

    CHECK(leaf.desired_size().height == doctest::Approx(100.0f));
}

// ============================================================================
// FrameworkElement — Arrange 协议
// ============================================================================

TEST_CASE("layout_FE_Arrange_Stretch充满slot") {
    FixedLeaf leaf{50.0f, 30.0f};
    // 默认 Stretch 对齐
    leaf.measure({200.0f, 150.0f});
    leaf.arrange({0.0f, 0.0f, 200.0f, 150.0f});

    const Rect br = leaf.bounds_rect();
    // Stretch：充满整个 slot（无 Margin）
    CHECK(br.x      == doctest::Approx(0.0f));
    CHECK(br.y      == doctest::Approx(0.0f));
    CHECK(br.width  == doctest::Approx(200.0f));
    CHECK(br.height == doctest::Approx(150.0f));
}

TEST_CASE("layout_FE_Arrange_水平Left对齐") {
    FixedLeaf leaf{50.0f, 30.0f};
    leaf.set_horizontal_alignment(HorizontalAlignment::Left);
    leaf.set_vertical_alignment(VerticalAlignment::Stretch);
    leaf.measure({200.0f, 150.0f});
    leaf.arrange({0.0f, 0.0f, 200.0f, 150.0f});

    const Rect br = leaf.bounds_rect();
    CHECK(br.x     == doctest::Approx(0.0f));   // Left 紧靠左侧
    CHECK(br.width == doctest::Approx(50.0f));   // 使用 desiredWidth
}

TEST_CASE("layout_FE_Arrange_水平Center对齐") {
    FixedLeaf leaf{50.0f, 30.0f};
    leaf.set_horizontal_alignment(HorizontalAlignment::Center);
    leaf.measure({200.0f, 150.0f});
    leaf.arrange({0.0f, 0.0f, 200.0f, 150.0f});

    const Rect br = leaf.bounds_rect();
    // Center：x = (200 - 50) / 2 = 75
    CHECK(br.x     == doctest::Approx(75.0f));
    CHECK(br.width == doctest::Approx(50.0f));
}

TEST_CASE("layout_FE_Arrange_水平Right对齐") {
    FixedLeaf leaf{50.0f, 30.0f};
    leaf.set_horizontal_alignment(HorizontalAlignment::Right);
    leaf.measure({200.0f, 150.0f});
    leaf.arrange({0.0f, 0.0f, 200.0f, 150.0f});

    const Rect br = leaf.bounds_rect();
    // Right：x = 200 - 50 = 150
    CHECK(br.x     == doctest::Approx(150.0f));
    CHECK(br.width == doctest::Approx(50.0f));
}

TEST_CASE("layout_FE_Arrange_垂直Bottom对齐") {
    FixedLeaf leaf{50.0f, 30.0f};
    leaf.set_vertical_alignment(VerticalAlignment::Bottom);
    leaf.measure({200.0f, 150.0f});
    leaf.arrange({0.0f, 0.0f, 200.0f, 150.0f});

    const Rect br = leaf.bounds_rect();
    // Bottom：y = 150 - 30 = 120
    CHECK(br.y      == doctest::Approx(120.0f));
    CHECK(br.height == doctest::Approx(30.0f));
}

TEST_CASE("layout_FE_Arrange_垂直Center对齐") {
    FixedLeaf leaf{50.0f, 30.0f};
    leaf.set_vertical_alignment(VerticalAlignment::Center);
    leaf.measure({200.0f, 150.0f});
    leaf.arrange({0.0f, 0.0f, 200.0f, 150.0f});

    const Rect br = leaf.bounds_rect();
    // Center：y = (150 - 30) / 2 = 60
    CHECK(br.y      == doctest::Approx(60.0f));
    CHECK(br.height == doctest::Approx(30.0f));
}

TEST_CASE("layout_FE_Arrange_Margin偏移bounds_rect") {
    FixedLeaf leaf{100.0f, 60.0f};
    // left=10, top=8, right=10, bottom=8 → 内容区从(10,8)开始，尺寸 180x134
    leaf.set_margin({10.0f, 8.0f, 10.0f, 8.0f});
    leaf.measure({200.0f, 150.0f});
    leaf.arrange({0.0f, 0.0f, 200.0f, 150.0f});

    const Rect br = leaf.bounds_rect();
    // Stretch：内容区宽 = 200-10-10=180，高 = 150-8-8=134
    CHECK(br.x      == doctest::Approx(10.0f));
    CHECK(br.y      == doctest::Approx(8.0f));
    CHECK(br.width  == doctest::Approx(180.0f));
    CHECK(br.height == doctest::Approx(134.0f));
}

// ============================================================================
// Panel — 子节点管理
// ============================================================================

TEST_CASE("layout_Panel_子节点管理") {
    StackPanel panel;
    FixedLeaf  a{50.0f, 20.0f};
    FixedLeaf  b{80.0f, 30.0f};

    panel.add_child(&a);
    panel.add_child(&b);

    CHECK(panel.children_count() == 2u);
    CHECK(panel.child_at(0) == &a);
    CHECK(panel.child_at(1) == &b);
}

TEST_CASE("layout_Panel_重复add_child不重复") {
    StackPanel panel;
    FixedLeaf  leaf{10.0f, 10.0f};

    panel.add_child(&leaf);
    panel.add_child(&leaf);  // 再次添加同一个

    CHECK(panel.children_count() == 1u);
}

TEST_CASE("layout_Panel_remove_child正确移除") {
    StackPanel panel;
    FixedLeaf  a{10.0f, 10.0f};
    FixedLeaf  b{20.0f, 20.0f};
    FixedLeaf  c{30.0f, 30.0f};

    panel.add_child(&a);
    panel.add_child(&b);
    panel.add_child(&c);

    panel.remove_child(&b);

    CHECK(panel.children_count() == 2u);
    CHECK(panel.child_at(0) == &a);
    CHECK(panel.child_at(1) == &c);
}

// ============================================================================
// StackPanel — 垂直排列
// ============================================================================

TEST_CASE("layout_StackPanel_垂直Measure_desiredSize累加高度") {
    StackPanel panel;
    panel.set_orientation(Orientation::Vertical);

    FixedLeaf a{100.0f, 30.0f};
    FixedLeaf b{60.0f,  50.0f};
    panel.add_child(&a);
    panel.add_child(&b);

    panel.measure({300.0f, 600.0f});
    const Size ds = panel.desired_size();

    // 高度 = 30 + 50 = 80；宽度 = max(100, 60) = 100
    CHECK(ds.height == doctest::Approx(80.0f));
    CHECK(ds.width  == doctest::Approx(100.0f));
}

TEST_CASE("layout_StackPanel_垂直Arrange_子元素y轴顺序") {
    StackPanel panel;
    panel.set_orientation(Orientation::Vertical);

    FixedLeaf a{100.0f, 30.0f};
    FixedLeaf b{100.0f, 50.0f};
    panel.add_child(&a);
    panel.add_child(&b);

    panel.measure({300.0f, 600.0f});
    panel.arrange({0.0f, 0.0f, 300.0f, 600.0f});

    // 子元素 a：slot.y = 0，高 = 30
    // 子元素 b：slot.y = 30，高 = 50
    // Stretch 水平：各宽 = 300（面板宽度）
    const Rect ra = a.bounds_rect();
    CHECK(ra.y      == doctest::Approx(0.0f));
    CHECK(ra.height == doctest::Approx(30.0f));
    CHECK(ra.width  == doctest::Approx(300.0f));

    const Rect rb = b.bounds_rect();
    CHECK(rb.y      == doctest::Approx(30.0f));
    CHECK(rb.height == doctest::Approx(50.0f));
}

// ============================================================================
// StackPanel — 水平排列
// ============================================================================

TEST_CASE("layout_StackPanel_水平Measure_desiredSize累加宽度") {
    StackPanel panel;
    panel.set_orientation(Orientation::Horizontal);

    FixedLeaf a{40.0f, 50.0f};
    FixedLeaf b{70.0f, 30.0f};
    panel.add_child(&a);
    panel.add_child(&b);

    panel.measure({600.0f, 400.0f});
    const Size ds = panel.desired_size();

    // 宽度 = 40 + 70 = 110；高度 = max(50, 30) = 50
    CHECK(ds.width  == doctest::Approx(110.0f));
    CHECK(ds.height == doctest::Approx(50.0f));
}

TEST_CASE("layout_StackPanel_水平Arrange_子元素x轴顺序") {
    StackPanel panel;
    panel.set_orientation(Orientation::Horizontal);

    FixedLeaf a{40.0f, 50.0f};
    FixedLeaf b{70.0f, 30.0f};
    panel.add_child(&a);
    panel.add_child(&b);

    panel.measure({600.0f, 400.0f});
    panel.arrange({0.0f, 0.0f, 600.0f, 400.0f});

    // 子元素 a：slot.x = 0，宽 = 40
    const Rect ra = a.bounds_rect();
    CHECK(ra.x     == doctest::Approx(0.0f));
    CHECK(ra.width == doctest::Approx(40.0f));
    CHECK(ra.height == doctest::Approx(400.0f));  // Stretch 垂直

    // 子元素 b：slot.x = 40，宽 = 70
    const Rect rb = b.bounds_rect();
    CHECK(rb.x     == doctest::Approx(40.0f));
    CHECK(rb.width == doctest::Approx(70.0f));
}

// ============================================================================
// Grid — 基本像素行列
// ============================================================================

TEST_CASE("layout_Grid_像素行列_单子元素slot正确") {
    Grid grid;
    grid.add_row(RowDefinition{GridLength::pixel(100.0f)});
    grid.add_column(ColumnDefinition{GridLength::pixel(200.0f)});

    FixedLeaf leaf{50.0f, 40.0f};
    grid.add_child(&leaf);  // 默认 row=0, col=0

    grid.measure({400.0f, 300.0f});
    grid.arrange({0.0f, 0.0f, 400.0f, 300.0f});

    // Pixel 行列：leaf 的 slot = {0,0,200,100}
    // FixedLeaf 默认 Stretch → bounds_rect == slot（减去 Margin=0）
    const Rect br = leaf.bounds_rect();
    CHECK(br.x      == doctest::Approx(0.0f));
    CHECK(br.y      == doctest::Approx(0.0f));
    CHECK(br.width  == doctest::Approx(200.0f));
    CHECK(br.height == doctest::Approx(100.0f));
}

TEST_CASE("layout_Grid_像素行列_两列子元素位置") {
    Grid grid;
    grid.add_row(RowDefinition{GridLength::pixel(100.0f)});
    grid.add_column(ColumnDefinition{GridLength::pixel(150.0f)});
    grid.add_column(ColumnDefinition{GridLength::pixel(250.0f)});

    FixedLeaf a{10.0f, 10.0f};
    FixedLeaf b{10.0f, 10.0f};
    Grid::set_column(b, 1);
    grid.add_child(&a);
    grid.add_child(&b);

    grid.measure({400.0f, 300.0f});
    grid.arrange({0.0f, 0.0f, 400.0f, 300.0f});

    // a 在列0：slot_x=0, slot_w=150
    const Rect ra = a.bounds_rect();
    CHECK(ra.x     == doctest::Approx(0.0f));
    CHECK(ra.width == doctest::Approx(150.0f));

    // b 在列1：slot_x=150, slot_w=250
    const Rect rb = b.bounds_rect();
    CHECK(rb.x     == doctest::Approx(150.0f));
    CHECK(rb.width == doctest::Approx(250.0f));
}

// ============================================================================
// Grid — Auto 行列
// ============================================================================

TEST_CASE("layout_Grid_Auto行列_尺寸等于子元素期望尺寸") {
    Grid grid;
    grid.add_row(RowDefinition{GridLength::auto_()});
    grid.add_column(ColumnDefinition{GridLength::auto_()});

    FixedLeaf leaf{80.0f, 60.0f};
    grid.add_child(&leaf);

    grid.measure({400.0f, 300.0f});

    // Grid desired = 叶子 desired（无 Margin，无其他行列）
    const Size ds = grid.desired_size();
    CHECK(ds.width  == doctest::Approx(80.0f));
    CHECK(ds.height == doctest::Approx(60.0f));
}

// ============================================================================
// Grid — Star 比例行列
// ============================================================================

TEST_CASE("layout_Grid_Star行列_等权重各占一半") {
    Grid grid;
    grid.add_row(RowDefinition{GridLength::star()});     // 1*
    grid.add_column(ColumnDefinition{GridLength::star()});  // 1*
    grid.add_column(ColumnDefinition{GridLength::star()});  // 1*

    FixedLeaf a{10.0f, 10.0f};
    FixedLeaf b{10.0f, 10.0f};
    Grid::set_column(b, 1);
    grid.add_child(&a);
    grid.add_child(&b);

    grid.measure({400.0f, 200.0f});
    grid.arrange({0.0f, 0.0f, 400.0f, 200.0f});

    // 两列各 1*，总宽 400 → 每列 200
    const Rect ra = a.bounds_rect();
    CHECK(ra.x     == doctest::Approx(0.0f));
    CHECK(ra.width == doctest::Approx(200.0f));

    const Rect rb = b.bounds_rect();
    CHECK(rb.x     == doctest::Approx(200.0f));
    CHECK(rb.width == doctest::Approx(200.0f));
}

TEST_CASE("layout_Grid_Star行列_不等权重按比例分配") {
    Grid grid;
    grid.add_row(RowDefinition{GridLength::star()});
    grid.add_column(ColumnDefinition{GridLength::star(1.0f)});  // 1*
    grid.add_column(ColumnDefinition{GridLength::star(3.0f)});  // 3*

    FixedLeaf a{10.0f, 10.0f};
    FixedLeaf b{10.0f, 10.0f};
    Grid::set_column(b, 1);
    grid.add_child(&a);
    grid.add_child(&b);

    grid.measure({400.0f, 200.0f});
    grid.arrange({0.0f, 0.0f, 400.0f, 200.0f});

    // 1* + 3* = 4 份，总宽 400 → 列0=100，列1=300
    const Rect ra = a.bounds_rect();
    CHECK(ra.width == doctest::Approx(100.0f));

    const Rect rb = b.bounds_rect();
    CHECK(rb.x     == doctest::Approx(100.0f));
    CHECK(rb.width == doctest::Approx(300.0f));
}

// ============================================================================
// Grid — 跨列（ColumnSpan）
// ============================================================================

TEST_CASE("layout_Grid_ColumnSpan_slot宽度为两列之和") {
    Grid grid;
    grid.add_row(RowDefinition{GridLength::pixel(100.0f)});
    grid.add_column(ColumnDefinition{GridLength::pixel(150.0f)});
    grid.add_column(ColumnDefinition{GridLength::pixel(250.0f)});

    FixedLeaf leaf{10.0f, 10.0f};
    Grid::set_column_span(leaf, 2);  // 跨两列
    grid.add_child(&leaf);

    grid.measure({400.0f, 300.0f});
    grid.arrange({0.0f, 0.0f, 400.0f, 300.0f});

    // slot_w = 150 + 250 = 400，Stretch → bounds_rect.width = 400
    const Rect br = leaf.bounds_rect();
    CHECK(br.width == doctest::Approx(400.0f));
}

// ============================================================================
// Grid — 默认单行单列（无显式定义）
// ============================================================================

TEST_CASE("layout_Grid_无行列定义视为单行单列1Star") {
    Grid grid;  // 不调用 add_row / add_column

    FixedLeaf leaf{80.0f, 60.0f};
    grid.add_child(&leaf);

    grid.measure({400.0f, 300.0f});
    grid.arrange({0.0f, 0.0f, 400.0f, 300.0f});

    // 默认 1* 行/列：slot = {0,0,400,300}，Stretch → bounds_rect 充满
    const Rect br = leaf.bounds_rect();
    CHECK(br.x      == doctest::Approx(0.0f));
    CHECK(br.y      == doctest::Approx(0.0f));
    CHECK(br.width  == doctest::Approx(400.0f));
    CHECK(br.height == doctest::Approx(300.0f));
}

// ============================================================================
// Grid — 无限可用空间下 Star 行/列的 star_unit 策略
// ============================================================================

TEST_CASE("layout_Grid_Star列_无限宽度_按内容最小等比分配") {
    // 两列：1* 和 2*，宽度方向无限可用
    // 内容：1* 列子元素期望宽 30px，2* 列子元素期望宽 40px
    // 期望：unit = max(30/1, 40/2) = 30px；1* → 30px，2* → 60px
    Grid grid;
    grid.add_row(RowDefinition{GridLength::pixel(50.0f)});
    grid.add_column(ColumnDefinition{GridLength::star(1.0f)});  // 1*
    grid.add_column(ColumnDefinition{GridLength::star(2.0f)});  // 2*

    FixedLeaf a{30.0f, 10.0f};   // 1* 列，期望宽 30px
    FixedLeaf b{40.0f, 10.0f};   // 2* 列，期望宽 40px
    Grid::set_column(b, 1);
    grid.add_child(&a);
    grid.add_child(&b);

    const float kInf = std::numeric_limits<float>::infinity();
    grid.measure({kInf, 200.0f});

    // desired_width = 1* (30px) + 2* (60px) = 90px
    const Size ds = grid.desired_size();
    CHECK(ds.width == doctest::Approx(90.0f));

    // arrange 时父给出实际有限宽度 200px：1* → 200/3 ≈ 66.67，2* → 400/3 ≈ 133.33
    grid.arrange({0.0f, 0.0f, 200.0f, 200.0f});
    CHECK(a.bounds_rect().width == doctest::Approx(200.0f / 3.0f));
    CHECK(b.bounds_rect().width == doctest::Approx(400.0f / 3.0f));
}

TEST_CASE("layout_Grid_Star行_无限高度_按内容最小等比分配") {
    // 三行：12px（Pixel）+ auto + 1*，高度方向无限可用
    // auto 行子元素期望高 20px，1* 行子元素期望高 50px
    // 期望：unit = 50/1 = 50px；1* → 50px；total = 12 + 20 + 50 = 82px
    Grid grid;
    grid.add_row(RowDefinition{GridLength::pixel(12.0f)});
    grid.add_row(RowDefinition{GridLength::auto_()});
    grid.add_row(RowDefinition{GridLength::star(1.0f)});
    grid.add_column(ColumnDefinition{GridLength::pixel(100.0f)});

    FixedLeaf leaf_auto{10.0f, 20.0f};  // 在 auto 行
    FixedLeaf leaf_star{10.0f, 50.0f};  // 在 star 行
    Grid::set_row(leaf_auto, 1);
    Grid::set_row(leaf_star, 2);
    grid.add_child(&leaf_auto);
    grid.add_child(&leaf_star);

    const float kInf = std::numeric_limits<float>::infinity();
    grid.measure({200.0f, kInf});

    const Size ds = grid.desired_size();
    CHECK(ds.height == doctest::Approx(82.0f));  // 12 + 20 + 50
}

TEST_CASE("layout_Grid_Star行列_两个Star行_等比保持") {
    // 两个 Star 行：1* 内容 30px，2* 内容 40px
    // unit = max(30/1, 40/2) = 30px；1* → 30px，2* → 60px；total = 90px
    Grid grid;
    grid.add_row(RowDefinition{GridLength::star(1.0f)});
    grid.add_row(RowDefinition{GridLength::star(2.0f)});
    grid.add_column(ColumnDefinition{GridLength::pixel(100.0f)});

    FixedLeaf a{10.0f, 30.0f};   // 1* 行
    FixedLeaf b{10.0f, 40.0f};   // 2* 行
    Grid::set_row(b, 1);
    grid.add_child(&a);
    grid.add_child(&b);

    const float kInf = std::numeric_limits<float>::infinity();
    grid.measure({200.0f, kInf});

    const Size ds = grid.desired_size();
    CHECK(ds.height == doctest::Approx(90.0f));  // 30 + 60
}
