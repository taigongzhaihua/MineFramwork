/**
 * @file VisualTest.cpp
 * @brief mine.ui.visual 模块单元测试。
 *
 * 覆盖场景：
 *   Visual - 视觉树：
 *     - 添加子节点、child_count、child_at、parent 指针
 *     - 移除单个子节点后 parent 置 nullptr
 *     - remove_all_children 后 child_count 为 0
 *     - 析构时自动从父节点移除
 *   Visual - 变换：
 *     - set_render_transform 存取
 *     - 默认变换为单位矩阵
 *   Visual - 裁剪：
 *     - set_clip_rect / has_clip_rect / clip_rect / clear_clip_rect
 *   Visual - 依赖属性（Opacity / Visibility）：
 *     - 默认值（opacity=1.0, visibility=Visible）
 *     - set_opacity 钳制到 [0,1]
 *     - set_visibility
 *   Visual - 脏区：
 *     - 新建时 is_render_dirty == true
 *     - render_to_canvas 后 is_render_dirty == false
 *     - invalidate_render 后 is_render_dirty == true
 *     - 脏标志向父节点传播
 *   Visual - 路由事件处理器：
 *     - add_handler / invoke_handlers（按注册顺序）
 *     - handled=true 中止后续处理器
 *     - remove_handler 移除匹配项
 *   UIElement - 命中测试：
 *     - 点在 bounds_rect 内命中
 *     - 点在 bounds_rect 外不命中
 *     - Collapsed 不命中
 *     - 子节点优先于父节点命中
 *   UIElement - 布局边界：
 *     - set_bounds_rect / bounds_rect 存取
 *   Control：
 *     - 可实例化，继承自 UIElement
 *   Visual - 渲染管线：
 *     - render_to_canvas 调用 on_render
 *     - Collapsed 跳过 on_render
 *     - Hidden 跳过 on_render 但渲染子节点
 */

#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>

#include <memory>  // Variant.h 间接依赖

#include <mine/ui/visual/VisualAll.h>
#include <mine/paint/Canvas.h>
#include <mine/ui/event/RoutedEvent.h>
#include <mine/ui/event/RoutedEventArgs.h>

using namespace mine::ui;
using namespace mine::math;
using namespace mine::core;

// ============================================================================
// 测试辅助：可记录渲染次数的 Visual 子类
// ============================================================================

namespace {

/**
 * @brief 测试用 Visual 子类，记录 on_render 调用次数。
 */
class TestVisual : public Visual {
public:
    int render_count = 0;

protected:
    void on_render(mine::paint::Canvas& /*canvas*/) override {
        ++render_count;
    }
};

/**
 * @brief 测试用 UIElement 子类，记录 on_render 调用次数。
 */
class TestUIElement : public UIElement {
public:
    int render_count = 0;

protected:
    void on_render(mine::paint::Canvas& /*canvas*/) override {
        ++render_count;
    }
};

} // anonymous namespace

// ============================================================================
// Visual — 视觉树测试
// ============================================================================

TEST_SUITE("Visual - 视觉树") {

    TEST_CASE("默认无父节点和子节点") {
        TestVisual v;
        CHECK(v.parent() == nullptr);
        CHECK(v.child_count() == 0u);
    }

    TEST_CASE("add_child 建立父子关系") {
        TestVisual root;
        TestVisual child1;
        TestVisual child2;

        root.add_child(&child1);
        root.add_child(&child2);

        CHECK(root.child_count() == 2u);
        CHECK(root.child_at(0) == &child1);
        CHECK(root.child_at(1) == &child2);
        CHECK(child1.parent() == &root);
        CHECK(child2.parent() == &root);
    }

    TEST_CASE("remove_child 断开父子关系") {
        TestVisual root;
        TestVisual child;

        root.add_child(&child);
        CHECK(root.child_count() == 1u);

        root.remove_child(&child);
        CHECK(root.child_count() == 0u);
        CHECK(child.parent() == nullptr);
    }

    TEST_CASE("remove_all_children 清空所有子节点") {
        TestVisual root;
        TestVisual c1, c2, c3;

        root.add_child(&c1);
        root.add_child(&c2);
        root.add_child(&c3);
        CHECK(root.child_count() == 3u);

        root.remove_all_children();
        CHECK(root.child_count() == 0u);
        CHECK(c1.parent() == nullptr);
        CHECK(c2.parent() == nullptr);
        CHECK(c3.parent() == nullptr);
    }

    TEST_CASE("子节点析构时自动从父节点移除") {
        TestVisual root;
        {
            TestVisual temp_child;
            root.add_child(&temp_child);
            CHECK(root.child_count() == 1u);
        }
        // temp_child 析构后，root 子列表应自动更新
        CHECK(root.child_count() == 0u);
    }
}

// ============================================================================
// Visual — 局部变换测试
// ============================================================================

TEST_SUITE("Visual - 局部变换") {

    TEST_CASE("默认为单位矩阵") {
        TestVisual v;
        const auto identity = Transform2D::identity();
        // 测试 apply(Point{0,0}) == {0,0}
        const Point origin{ 0.0f, 0.0f };
        const Point result = v.render_transform().apply(origin);
        CHECK(result.x == doctest::Approx(0.0f));
        CHECK(result.y == doctest::Approx(0.0f));
    }

    TEST_CASE("set_render_transform 存取正确") {
        TestVisual v;
        const auto t = Transform2D::translation(10.0f, 20.0f);
        v.set_render_transform(t);

        const Point p{ 0.0f, 0.0f };
        const Point result = v.render_transform().apply(p);
        CHECK(result.x == doctest::Approx(10.0f));
        CHECK(result.y == doctest::Approx(20.0f));
    }

    TEST_CASE("set_render_transform 触发渲染失效") {
        TestVisual v;
        // 先 render 清除脏标志
        mine::paint::Canvas canvas;
        v.render_to_canvas(canvas);
        CHECK(v.is_render_dirty() == false);

        v.set_render_transform(Transform2D::scale(2.0f));
        CHECK(v.is_render_dirty() == true);
    }
}

// ============================================================================
// Visual — 矩形裁剪测试
// ============================================================================

TEST_SUITE("Visual - 矩形裁剪") {

    TEST_CASE("默认无裁剪区域") {
        TestVisual v;
        CHECK(v.has_clip_rect() == false);
    }

    TEST_CASE("set_clip_rect 设置裁剪区域") {
        TestVisual v;
        const Rect r{ 10.0f, 20.0f, 100.0f, 50.0f };
        v.set_clip_rect(r);

        CHECK(v.has_clip_rect() == true);
        CHECK(v.clip_rect().x      == doctest::Approx(10.0f));
        CHECK(v.clip_rect().y      == doctest::Approx(20.0f));
        CHECK(v.clip_rect().width  == doctest::Approx(100.0f));
        CHECK(v.clip_rect().height == doctest::Approx(50.0f));
    }

    TEST_CASE("clear_clip_rect 移除裁剪区域") {
        TestVisual v;
        v.set_clip_rect({ 0.0f, 0.0f, 100.0f, 100.0f });
        v.clear_clip_rect();
        CHECK(v.has_clip_rect() == false);
    }

    TEST_CASE("set_clip_rect 触发渲染失效") {
        TestVisual v;
        mine::paint::Canvas canvas;
        v.render_to_canvas(canvas);
        CHECK(v.is_render_dirty() == false);

        v.set_clip_rect({ 0.0f, 0.0f, 50.0f, 50.0f });
        CHECK(v.is_render_dirty() == true);
    }
}

// ============================================================================
// Visual — 依赖属性（Opacity / Visibility）测试
// ============================================================================

TEST_SUITE("Visual - 依赖属性") {

    TEST_CASE("Opacity 默认值为 1.0") {
        TestVisual v;
        CHECK(v.opacity() == doctest::Approx(1.0f));
    }

    TEST_CASE("set_opacity 正常值") {
        TestVisual v;
        v.set_opacity(0.5f);
        CHECK(v.opacity() == doctest::Approx(0.5f));
    }

    TEST_CASE("set_opacity 钳制到 [0,1]") {
        TestVisual v;
        v.set_opacity(-0.5f);
        CHECK(v.opacity() == doctest::Approx(0.0f));

        v.set_opacity(1.5f);
        CHECK(v.opacity() == doctest::Approx(1.0f));
    }

    TEST_CASE("Visibility 默认值为 Visible") {
        TestVisual v;
        CHECK(v.visibility() == Visibility::Visible);
    }

    TEST_CASE("set_visibility 存取正确") {
        TestVisual v;
        v.set_visibility(Visibility::Collapsed);
        CHECK(v.visibility() == Visibility::Collapsed);

        v.set_visibility(Visibility::Hidden);
        CHECK(v.visibility() == Visibility::Hidden);
    }
}

// ============================================================================
// Visual — 脏区管理测试
// ============================================================================

TEST_SUITE("Visual - 脏区管理") {

    TEST_CASE("新建时渲染脏标志为 true") {
        TestVisual v;
        CHECK(v.is_render_dirty() == true);
    }

    TEST_CASE("render_to_canvas 后脏标志清除") {
        TestVisual v;
        mine::paint::Canvas canvas;
        v.render_to_canvas(canvas);
        CHECK(v.is_render_dirty() == false);
    }

    TEST_CASE("invalidate_render 重新置脏") {
        TestVisual v;
        mine::paint::Canvas canvas;
        v.render_to_canvas(canvas);
        CHECK(v.is_render_dirty() == false);

        v.invalidate_render();
        CHECK(v.is_render_dirty() == true);
    }

    TEST_CASE("子节点 invalidate_render 向上传播到父节点") {
        TestVisual root;
        TestVisual child;
        root.add_child(&child);

        // 清除所有脏标志
        mine::paint::Canvas canvas;
        root.render_to_canvas(canvas);
        CHECK(root.is_render_dirty() == false);
        CHECK(child.is_render_dirty() == false);

        // 子节点 invalidate，父节点应同步变脏
        child.invalidate_render();
        CHECK(child.is_render_dirty() == true);
        CHECK(root.is_render_dirty() == true);
    }

    // 析构时需要 remove_all_children，防止悬空
    TEST_CASE("add_child 触发渲染失效") {
        TestVisual root;
        mine::paint::Canvas canvas;
        root.render_to_canvas(canvas);
        CHECK(root.is_render_dirty() == false);

        TestVisual child;
        root.add_child(&child);
        CHECK(root.is_render_dirty() == true);
        root.remove_child(&child); // 清理
    }
}

// ============================================================================
// Visual — 渲染管线测试
// ============================================================================

TEST_SUITE("Visual - 渲染管线") {

    TEST_CASE("render_to_canvas 调用 on_render") {
        TestVisual v;
        mine::paint::Canvas canvas;
        v.render_to_canvas(canvas);
        CHECK(v.render_count == 1);
    }

    TEST_CASE("Collapsed 时 render_to_canvas 跳过 on_render") {
        TestVisual v;
        v.set_visibility(Visibility::Collapsed);
        mine::paint::Canvas canvas;
        v.render_to_canvas(canvas);
        CHECK(v.render_count == 0);
    }

    TEST_CASE("Hidden 时跳过自身渲染但子节点正常渲染") {
        TestVisual root;
        root.set_visibility(Visibility::Hidden);

        TestVisual child;
        root.add_child(&child);

        mine::paint::Canvas canvas;
        root.render_to_canvas(canvas);

        // root 自身不渲染（Hidden 跳过 on_render）
        CHECK(root.render_count == 0);
        // 子节点正常渲染
        CHECK(child.render_count == 1);

        root.remove_child(&child);
    }

    TEST_CASE("render_to_canvas 递归渲染子树") {
        TestVisual root;
        TestVisual child1;
        TestVisual child2;
        root.add_child(&child1);
        root.add_child(&child2);

        mine::paint::Canvas canvas;
        root.render_to_canvas(canvas);

        CHECK(root.render_count == 1);
        CHECK(child1.render_count == 1);
        CHECK(child2.render_count == 1);

        root.remove_all_children();
    }
}

// ============================================================================
// Visual — 路由事件处理器测试
// ============================================================================

TEST_SUITE("Visual - 路由事件处理器") {

    // 注册测试用路由事件
    namespace {
        const RoutedEvent& TestClickEvent() {
            static const RoutedEvent& ev = register_event<TestVisual>(
                "TestClick", RoutingStrategy::Bubble);
            return ev;
        }
    }

    TEST_CASE("add_handler + invoke_handlers 按注册顺序调用") {
        TestVisual v;

        int call_order = 0;
        int order1 = -1;
        int order2 = -1;

        struct Ctx { int* order_ptr; int* counter; };
        Ctx ctx1{ &order1, &call_order };
        Ctx ctx2{ &order2, &call_order };

        v.add_handler(TestClickEvent(),
            [](void* /*sender*/, RoutedEventArgs& /*args*/, void* ud) {
                auto* c = static_cast<Ctx*>(ud);
                *c->order_ptr = ++(*c->counter);
            }, &ctx1);

        v.add_handler(TestClickEvent(),
            [](void* /*sender*/, RoutedEventArgs& /*args*/, void* ud) {
                auto* c = static_cast<Ctx*>(ud);
                *c->order_ptr = ++(*c->counter);
            }, &ctx2);

        RoutedEventArgs args{ TestClickEvent() };
        v.invoke_handlers(TestClickEvent(), args);

        CHECK(order1 == 1);
        CHECK(order2 == 2);
    }

    TEST_CASE("handled=true 中止后续处理器") {
        TestVisual v;

        int count = 0;

        v.add_handler(TestClickEvent(),
            [](void* /*sender*/, RoutedEventArgs& args, void* ud) {
                ++(*static_cast<int*>(ud));
                args.set_handled(true); // 中止
            }, &count);

        v.add_handler(TestClickEvent(),
            [](void* /*sender*/, RoutedEventArgs& /*args*/, void* ud) {
                ++(*static_cast<int*>(ud)); // 不应被调用
            }, &count);

        RoutedEventArgs args{ TestClickEvent() };
        v.invoke_handlers(TestClickEvent(), args);

        CHECK(count == 1); // 仅第一个处理器被调用
    }

    TEST_CASE("remove_handler 移除处理器") {
        TestVisual v;
        int count = 0;

        auto handler = [](void* /*sender*/, RoutedEventArgs& /*args*/, void* ud) {
            ++(*static_cast<int*>(ud));
        };

        v.add_handler(TestClickEvent(), handler, &count);
        v.remove_handler(TestClickEvent(), handler, &count);

        RoutedEventArgs args{ TestClickEvent() };
        v.invoke_handlers(TestClickEvent(), args);
        CHECK(count == 0);
    }
}

// ============================================================================
// UIElement — 命中测试
// ============================================================================

TEST_SUITE("UIElement - 命中测试") {

    TEST_CASE("点在 bounds_rect 内命中") {
        TestUIElement e;
        e.set_bounds_rect({ 10.0f, 10.0f, 100.0f, 50.0f });

        UIElement* hit = e.hit_test(Point{ 50.0f, 30.0f });
        CHECK(hit == &e);
    }

    TEST_CASE("点在 bounds_rect 外不命中") {
        TestUIElement e;
        e.set_bounds_rect({ 10.0f, 10.0f, 100.0f, 50.0f });

        UIElement* hit = e.hit_test(Point{ 5.0f, 5.0f });
        CHECK(hit == nullptr);
    }

    TEST_CASE("Collapsed 时不参与命中测试") {
        TestUIElement e;
        e.set_bounds_rect({ 0.0f, 0.0f, 100.0f, 100.0f });
        e.set_visibility(Visibility::Collapsed);

        UIElement* hit = e.hit_test(Point{ 50.0f, 50.0f });
        CHECK(hit == nullptr);
    }

    TEST_CASE("子节点优先于父节点命中") {
        TestUIElement parent;
        parent.set_bounds_rect({ 0.0f, 0.0f, 200.0f, 200.0f });

        TestUIElement child;
        child.set_bounds_rect({ 50.0f, 50.0f, 80.0f, 80.0f });
        parent.add_child(&child);

        // 点在子节点范围内，应命中子节点
        UIElement* hit = parent.hit_test(Point{ 80.0f, 80.0f });
        CHECK(hit == &child);

        // 点在父节点范围内但子节点外，命中父节点
        UIElement* hit2 = parent.hit_test(Point{ 10.0f, 10.0f });
        CHECK(hit2 == &parent);

        parent.remove_child(&child);
    }

    TEST_CASE("set_bounds_rect 存取正确") {
        TestUIElement e;
        e.set_bounds_rect({ 5.0f, 15.0f, 200.0f, 100.0f });
        const Rect r = e.bounds_rect();
        CHECK(r.x      == doctest::Approx(5.0f));
        CHECK(r.y      == doctest::Approx(15.0f));
        CHECK(r.width  == doctest::Approx(200.0f));
        CHECK(r.height == doctest::Approx(100.0f));
    }
}

// ============================================================================
// Control — 基本可实例化
// ============================================================================

TEST_SUITE("Control - 基本验证") {

    TEST_CASE("Control 可正常构造和析构") {
        Control c;
        // 继承自 UIElement，默认值与 UIElement 一致
        CHECK(c.parent() == nullptr);
        CHECK(c.child_count() == 0u);
        CHECK(c.opacity() == doctest::Approx(1.0f));
        CHECK(c.visibility() == Visibility::Visible);
    }
}
