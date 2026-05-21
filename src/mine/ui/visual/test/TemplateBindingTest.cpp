/**
 * @file TemplateBindingTest.cpp
 * @brief ControlTemplate + TemplateRegistry + bind_template / find_template_child 单元测试。
 *
 * 覆盖场景：
 *   1.  ControlTemplate 构建器接口（set_name / set_target_type）
 *   2.  ControlTemplate::build() 调用 build_fn_（回调被触发验证）
 *   3.  TemplateRegistry::instance()：注册 + find()
 *   4.  TemplateRegistry::find_default() 按 TypeId 查找
 *   5.  UIElement::set_template_name / template_name 存取
 *   6.  Control::set_template_root() 将根元素加入视觉子树（child_count 增加）
 *   7.  Control::find_template_child() 返回正确元素
 *   8.  Control::find_template_child() 不存在时返回 nullptr
 *   9.  Control::bind_template() 立即同步宿主属性值到子元素
 *  10.  Control::bind_template() 属性变更传播（host 更新 → child 自动更新）
 *  11.  ControlTemplate::build() 使用 nullptr build_fn_ 时不崩溃
 *  12.  find_template_child() 嵌套搜索（深度 > 1）
 *  13.  【核心验收】build → bind → find 全流程集成测试
 *
 * 注意：本文件不定义 DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN，
 *       main 函数由 VisualTest.cpp 提供。
 */

#include <doctest/doctest.h>

#include <mine/ui/visual/VisualAll.h>
#include <mine/ui/style/StyleAll.h>
#include <mine/ui/property/DependencyProperty.h>
#include <mine/ui/property/ValuePriority.h>

using namespace mine::ui;
using namespace mine::ui::style;
using namespace mine::core;

// ─────────────────────────────────────────────────────────────────────────────
// 测试专用辅助类型
// ─────────────────────────────────────────────────────────────────────────────

namespace {

/// 宿主控件属性 owner 标签（保证属性名全局唯一）
struct TBHost {};
/// 子元素属性 owner 标签
struct TBChild {};

/// 模板注册测试用类型标签 A
struct TBRegistryTypeA {};
/// 模板注册测试用类型标签 B
struct TBRegistryTypeB {};

/// 宿主属性（float，默认 0.0f）
static const DependencyProperty& host_prop()
{
    static const DependencyProperty& p =
        register_property<TBHost>("TB_HostProp", Variant{0.0f});
    return p;
}

/// 子属性（float，默认 0.0f）
static const DependencyProperty& child_prop()
{
    static const DependencyProperty& p =
        register_property<TBChild>("TB_ChildProp", Variant{0.0f});
    return p;
}

// ─────────────────────────────────────────────────────────────────────────────
// 集成测试所用全局状态（通过 build_fn_ 回调与 Control 交互）
// ─────────────────────────────────────────────────────────────────────────────

/// 集成测试数据容器
struct IntegrationData {
    UIElement child;         ///< 模板内子元素（"border"）
    UIElement grandchild;    ///< 深度 > 1 的孙子元素（"inner"）
    bool      build_called{false};
};

/// 全局测试数据指针（测试开始前设置，测试结束后清空）
static IntegrationData* g_integration_data = nullptr;

/// 集成测试 build_fn_：构建一层模板子树，并注册孙子节点
static void integration_build_fn(DependencyObject& target)
{
    g_integration_data->build_called = true;
    // 配置子元素名称
    g_integration_data->child.set_template_name("border");
    // 将孙子挂在子元素下（深度 > 1 搜索测试）
    g_integration_data->grandchild.set_template_name("inner");
    g_integration_data->child.add_child(&g_integration_data->grandchild);
    // 将模板根挂到控件
    auto& ctrl = static_cast<Control&>(target);
    ctrl.set_template_root(&g_integration_data->child);
}

}  // anonymous namespace

// ─────────────────────────────────────────────────────────────────────────────
// 测试用例
// ─────────────────────────────────────────────────────────────────────────────

TEST_SUITE("ControlTemplate - 构建器接口") {

    TEST_CASE("set_name / set_target_type / 查询") {
        ControlTemplate tmpl;
        // 链式调用构建器
        tmpl.set_name("MyButton")
            .set_target_type(TypeId::of<TBHost>());

        CHECK(tmpl.name()        == StringView{"MyButton"});
        CHECK(tmpl.target_type() == TypeId::of<TBHost>());
    }

    TEST_CASE("build_fn_ 为非空时 build() 触发回调") {
        bool called = false;
        UIElement target;

        ControlTemplate tmpl;
        // 使用 lambda 无法做到（无 RTTI / captures），用结构+静态函数替代
        struct CallCtx {
            bool* flag;
        };
        static CallCtx ctx;
        ctx.flag = &called;
        tmpl.build_fn_ = [](DependencyObject& /*t*/) {
            *ctx.flag = true;
        };
        tmpl.build(target);
        CHECK(called == true);
    }

    TEST_CASE("build_fn_ 为 nullptr 时 build() 不崩溃") {
        UIElement target;
        ControlTemplate tmpl;
        // build_fn_ 默认为 nullptr，直接调用不应崩溃
        CHECK_NOTHROW(tmpl.build(target));
    }
}

TEST_SUITE("TemplateRegistry - 注册与查询") {

    TEST_CASE("register_template + find() 按名称查找") {
        // 注册一个模板（名称唯一：加 TB_ 前缀防止与其他测试冲突）
        TemplateRegistry::instance().register_template(
            "TB_ButtonTemplate",
            TypeId::of<TBRegistryTypeA>(),
            nullptr);

        const ControlTemplate* found =
            TemplateRegistry::instance().find("TB_ButtonTemplate");

        REQUIRE(found != nullptr);
        CHECK(found->name()        == StringView{"TB_ButtonTemplate"});
        CHECK(found->target_type() == TypeId::of<TBRegistryTypeA>());
    }

    TEST_CASE("find() 未注册名称返回 nullptr") {
        const ControlTemplate* found =
            TemplateRegistry::instance().find("TB_DoesNotExist");
        CHECK(found == nullptr);
    }

    TEST_CASE("find_default() 按 TypeId 查找") {
        // 注册一个以 TBRegistryTypeB 为目标类型的模板
        TemplateRegistry::instance().register_template(
            "TB_TypeBTemplate",
            TypeId::of<TBRegistryTypeB>(),
            nullptr);

        const ControlTemplate* found =
            TemplateRegistry::instance().find_default(TypeId::of<TBRegistryTypeB>());

        REQUIRE(found != nullptr);
        CHECK(found->target_type() == TypeId::of<TBRegistryTypeB>());
    }

    TEST_CASE("find_default() 未注册类型返回 nullptr") {
        struct TBUnknown {};
        const ControlTemplate* found =
            TemplateRegistry::instance().find_default(TypeId::of<TBUnknown>());
        CHECK(found == nullptr);
    }
}

TEST_SUITE("UIElement - template_name") {

    TEST_CASE("set_template_name 后 template_name 返回正确值") {
        UIElement elem;
        elem.set_template_name("header");
        CHECK(elem.template_name() == StringView{"header"});
    }

    TEST_CASE("默认 template_name 为空") {
        UIElement elem;
        CHECK(elem.template_name().empty());
    }
}

TEST_SUITE("Control - 模板根与子树") {

    TEST_CASE("set_template_root() 将根加入视觉子树") {
        UIElement root;
        Control   ctrl;

        CHECK(ctrl.child_count() == 0u);
        ctrl.set_template_root(&root);
        CHECK(ctrl.child_count() == 1u);
    }

    TEST_CASE("set_template_root(nullptr) 移除旧根") {
        UIElement root;
        Control   ctrl;

        ctrl.set_template_root(&root);
        CHECK(ctrl.child_count() == 1u);

        ctrl.set_template_root(nullptr);
        CHECK(ctrl.child_count() == 0u);
    }

    TEST_CASE("find_template_child() 未设置根时返回 nullptr") {
        Control ctrl;
        CHECK(ctrl.find_template_child("any") == nullptr);
    }

    TEST_CASE("find_template_child() 返回匹配元素") {
        UIElement root;
        root.set_template_name("root");

        Control ctrl;
        ctrl.set_template_root(&root);

        UIElement* found = ctrl.find_template_child("root");
        CHECK(found == &root);
    }

    TEST_CASE("find_template_child() 不存在名称时返回 nullptr") {
        UIElement root;
        root.set_template_name("root");

        Control ctrl;
        ctrl.set_template_root(&root);

        CHECK(ctrl.find_template_child("nonexistent") == nullptr);
    }

    TEST_CASE("find_template_child() 支持深度 > 1 的嵌套搜索") {
        UIElement root;
        UIElement child;
        UIElement grandchild;

        root.set_template_name("root");
        child.set_template_name("child");
        grandchild.set_template_name("grandchild");

        root.add_child(&child);
        child.add_child(&grandchild);

        Control ctrl;
        ctrl.set_template_root(&root);

        // 根节点
        CHECK(ctrl.find_template_child("root")       == &root);
        // 深度 1
        CHECK(ctrl.find_template_child("child")      == &child);
        // 深度 2
        CHECK(ctrl.find_template_child("grandchild") == &grandchild);
    }
}

TEST_SUITE("Control - bind_template 绑定") {

    TEST_CASE("bind_template() 立即同步宿主当前值到子元素") {
        UIElement child_elem;
        Control   ctrl;

        // 先给宿主设置属性值
        ctrl.set_value(host_prop(), Variant{42.0f}, ValuePriority::Local);

        // 建立绑定（立即同步）
        ctrl.bind_template(child_elem, child_prop(), host_prop());

        CHECK(child_elem.get_value(child_prop()).get<float>()
              == doctest::Approx(42.0f));
    }

    TEST_CASE("bind_template() 宿主属性变更自动同步到子元素") {
        UIElement child_elem;
        Control   ctrl;

        ctrl.set_value(host_prop(), Variant{10.0f}, ValuePriority::Local);
        ctrl.bind_template(child_elem, child_prop(), host_prop());

        // 修改宿主属性，子元素应自动更新
        ctrl.set_value(host_prop(), Variant{99.0f}, ValuePriority::Local);

        CHECK(child_elem.get_value(child_prop()).get<float>()
              == doctest::Approx(99.0f));
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// 核心验收：build → bind → find 全流程集成测试
// ─────────────────────────────────────────────────────────────────────────────

TEST_CASE("【核心验收】build → bind → find 集成测试") {
    IntegrationData data;
    g_integration_data = &data;

    {
        Control ctrl;  // 在 data 之后构造，确保先于 data 成员析构

        // ── 1. build ────────────────────────────────────────────────────────
        ControlTemplate tmpl;
        tmpl.build_fn_ = integration_build_fn;
        tmpl.build(ctrl);

        CHECK(data.build_called == true);
        CHECK(ctrl.child_count() == 1u);  // 模板根已加入视觉子树

        // ── 2. find ─────────────────────────────────────────────────────────
        UIElement* border_elem = ctrl.find_template_child("border");
        REQUIRE(border_elem == &data.child);

        UIElement* inner_elem = ctrl.find_template_child("inner");
        REQUIRE(inner_elem == &data.grandchild);

        // ── 3. bind ─────────────────────────────────────────────────────────
        // 先给宿主设置初始值，再建立绑定（验证立即同步）
        ctrl.set_value(host_prop(), Variant{55.0f}, ValuePriority::Local);
        ctrl.bind_template(*border_elem, child_prop(), host_prop());

        CHECK(border_elem->get_value(child_prop()).get<float>()
              == doctest::Approx(55.0f));

        // ── 4. 属性变更传播 ──────────────────────────────────────────────────
        ctrl.set_value(host_prop(), Variant{77.0f}, ValuePriority::Local);

        CHECK(border_elem->get_value(child_prop()).get<float>()
              == doctest::Approx(77.0f));

        // ── 5. ctrl 析构时应安全解绑（不崩溃）──────────────────────────────
    }  // ctrl 析构（自动取消订阅），data.child / data.grandchild 随后析构

    g_integration_data = nullptr;
}
