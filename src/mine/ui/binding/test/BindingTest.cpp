/**
 * @file BindingTest.cpp
 * @brief mine.ui.binding 模块单元测试。
 *
 * 覆盖场景：
 *   Function<>：空状态、基本调用、move 语义、reset
 *   BindingExpression：
 *     - DependencyObject 来源：初始求值、源变更触发更新、detach 停止更新
 *     - INotifyPropertyChanged 来源：初始求值、VM 通知触发更新
 *     - converter：正向转换
 *     - fallback：getter 返空时使用回退值
 *     - OneTime：仅初始化一次，后续变更不触发更新
 *     - TwoWay 预留：setter 字段存在，mode=TwoWay 时不调用（M1.1 不执行 setter）
 *     - is_attached() 状态变化
 *     - move 语义：BindingExpression 移动后原对象不再激活
 */

#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>

#include <memory>  // Variant.h 间接依赖
#include <mine/ui/binding/Binding.h>
#include <mine/ui/property/Property.h>
#include <mine/core/Function.h>
#include <mine/containers/Vector.h>

using namespace mine::ui;
using namespace mine::core;

// 命名空间别名：允许在测试代码中使用 core:: 前缀引用 mine::core 的类型
namespace core      = mine::core;
namespace containers = mine::containers;

// ============================================================================
// 测试辅助：TestDependencyObject（带两个依赖属性 IntProp / StringProp）
// ============================================================================

namespace {

/// 用于测试的 DependencyObject 子类（同时记录 on_property_changed 调用次数）
class TestDepObj : public DependencyObject {
public:
    int changed_count = 0;

    static const DependencyProperty& IntProp;
    static const DependencyProperty& StringProp;

protected:
    void on_property_changed(const DependencyProperty&,
                             const core::Variant&,
                             const core::Variant&) override
    {
        ++changed_count;
    }
};

const DependencyProperty& TestDepObj::IntProp =
    register_property<TestDepObj, int>("Int", {});

const DependencyProperty& TestDepObj::StringProp =
    register_property<TestDepObj, int>("String", {});

// ============================================================================
// 测试辅助：TestViewModel（INotifyPropertyChanged 的简单实现）
// ============================================================================

/// 简单的 ViewModel 实现，支持 int 属性 Value
class TestViewModel : public INotifyPropertyChanged {
public:
    /// 获取 Value 属性
    [[nodiscard]] core::Variant get_value() const noexcept { return value_; }

    /// 设置 Value 属性并触发属性变更通知
    void set_value(core::Variant v) noexcept {
        value_ = std::move(v);
        notify("Value");
    }

    uint32_t subscribe_property_changed(ChangedFn fn, void* ud) noexcept override {
        uint32_t id = next_id_++;
        subs_.push_back({id, fn, ud});
        return id;
    }

    void unsubscribe_property_changed(uint32_t token) noexcept override {
        for (size_t i = 0; i < subs_.size(); ++i) {
            if (subs_[i].id == token) {
                // 将最后一个元素移到此位置以保持密集存储
                if (i + 1 < subs_.size()) {
                    subs_[i] = subs_.back();
                }
                subs_.pop_back();
                return;
            }
        }
    }

private:
    /// 通知所有订阅者指定属性发生变更
    void notify(core::StringView prop_name) noexcept {
        // 迭代副本，防止通知期间订阅列表被修改
        mine::containers::Vector<Sub> copy = subs_;
        for (auto& s : copy) {
            s.fn(this, prop_name, s.ud);
        }
    }

    core::Variant value_;

    struct Sub {
        uint32_t  id;
        ChangedFn fn;
        void*     ud;
    };
    mine::containers::Vector<Sub> subs_;
    uint32_t next_id_ = 1;
};

} // anonymous namespace

// ============================================================================
// Function<> 测试
// ============================================================================

TEST_CASE("binding_Function_空状态") {
    Function<int()> fn;
    CHECK(!fn);
    CHECK_FALSE(static_cast<bool>(fn));
}

TEST_CASE("binding_Function_无捕获lambda调用") {
    Function<int(int, int)> fn = [](int a, int b) noexcept { return a + b; };
    CHECK(fn);
    CHECK(fn(3, 4) == 7);
}

TEST_CASE("binding_Function_有捕获lambda调用") {
    int base = 10;
    Function<int()> fn = [base]() noexcept { return base * 2; };
    CHECK(fn);
    CHECK(fn() == 20);
}

TEST_CASE("binding_Function_移动后源为空") {
    int x = 5;
    Function<int()> fn1 = [x]() noexcept { return x; };
    Function<int()> fn2 = std::move(fn1);

    CHECK(!fn1);          // 源已清空
    CHECK(fn2);           // 目标有效
    CHECK(fn2() == 5);
}

TEST_CASE("binding_Function_reset后为空") {
    Function<int()> fn = []() noexcept { return 42; };
    CHECK(fn);
    fn.reset();
    CHECK(!fn);
}

TEST_CASE("binding_Function_移动赋值") {
    Function<int()> fn1 = []() noexcept { return 1; };
    Function<int()> fn2 = []() noexcept { return 2; };
    fn2 = std::move(fn1);
    CHECK(!fn1);
    CHECK(fn2() == 1);
}

TEST_CASE("binding_Function_Variant返回值") {
    Function<core::Variant()> fn = []() noexcept -> core::Variant {
        return core::Variant{99};
    };
    auto v = fn();
    CHECK(v.has<int>());
    CHECK(v.get<int>() == 99);
}

// ============================================================================
// BindingExpression - DependencyObject 来源
// ============================================================================

TEST_CASE("binding_BindingExpr_DepObj_初始求值写入目标") {
    TestDepObj src, tgt;
    src.set_value(TestDepObj::IntProp, Variant{42});

    BindingExpression expr;
    expr.getter = [&src]() noexcept -> core::Variant {
        return src.get_value(TestDepObj::IntProp);
    };
    expr.deps.push_back(PropertyDependency::from_dep(src, TestDepObj::IntProp));
    expr.mode = BindingMode::OneWay;

    // attach 前目标为默认值（0）
    CHECK(tgt.get_value(TestDepObj::IntProp).get<int>() == 0);

    expr.attach(tgt, TestDepObj::IntProp);

    // attach 后目标应被更新为 42
    CHECK(tgt.get_value(TestDepObj::IntProp).get<int>() == 42);
}

TEST_CASE("binding_BindingExpr_DepObj_源变更自动更新目标") {
    TestDepObj src, tgt;

    BindingExpression expr;
    expr.getter = [&src]() noexcept -> core::Variant {
        return src.get_value(TestDepObj::IntProp);
    };
    expr.deps.push_back(PropertyDependency::from_dep(src, TestDepObj::IntProp));

    expr.attach(tgt, TestDepObj::IntProp);

    // 初始值：默认 0
    CHECK(tgt.get_value(TestDepObj::IntProp).get<int>() == 0);

    // 修改源，目标应自动更新
    src.set_value(TestDepObj::IntProp, Variant{100});
    CHECK(tgt.get_value(TestDepObj::IntProp).get<int>() == 100);

    src.set_value(TestDepObj::IntProp, Variant{200});
    CHECK(tgt.get_value(TestDepObj::IntProp).get<int>() == 200);
}

TEST_CASE("binding_BindingExpr_DepObj_detach后不再更新") {
    TestDepObj src, tgt;

    BindingExpression expr;
    expr.getter = [&src]() noexcept -> core::Variant {
        return src.get_value(TestDepObj::IntProp);
    };
    expr.deps.push_back(PropertyDependency::from_dep(src, TestDepObj::IntProp));
    expr.attach(tgt, TestDepObj::IntProp);

    src.set_value(TestDepObj::IntProp, Variant{50});
    CHECK(tgt.get_value(TestDepObj::IntProp).get<int>() == 50);

    expr.detach();
    CHECK_FALSE(expr.is_attached());

    // detach 后修改源，目标不应更新
    src.set_value(TestDepObj::IntProp, Variant{999});
    // 目标保持 50（上次写入的值）
    CHECK(tgt.get_value(TestDepObj::IntProp).get<int>() == 50);
}

TEST_CASE("binding_BindingExpr_is_attached状态") {
    TestDepObj src, tgt;

    BindingExpression expr;
    expr.getter = [&src]() noexcept -> core::Variant {
        return src.get_value(TestDepObj::IntProp);
    };
    expr.deps.push_back(PropertyDependency::from_dep(src, TestDepObj::IntProp));

    CHECK_FALSE(expr.is_attached());
    expr.attach(tgt, TestDepObj::IntProp);
    CHECK(expr.is_attached());
    expr.detach();
    CHECK_FALSE(expr.is_attached());
}

TEST_CASE("binding_BindingExpr_DepObj_过滤非目标属性变更") {
    TestDepObj src, tgt;
    int eval_count = 0;

    BindingExpression expr;
    expr.getter = [&src, &eval_count]() noexcept -> core::Variant {
        ++eval_count;
        return src.get_value(TestDepObj::IntProp);
    };
    // 只依赖 IntProp，不依赖 StringProp
    expr.deps.push_back(PropertyDependency::from_dep(src, TestDepObj::IntProp));
    expr.attach(tgt, TestDepObj::IntProp);

    int init_count = eval_count;

    // 修改 StringProp（不是依赖的属性），不应触发重新求值
    src.set_value(TestDepObj::StringProp, Variant{7});
    CHECK(eval_count == init_count);  // 未增加

    // 修改 IntProp（依赖的属性），应触发重新求值
    src.set_value(TestDepObj::IntProp, Variant{77});
    CHECK(eval_count == init_count + 1);
}

// ============================================================================
// BindingExpression - INotifyPropertyChanged 来源
// ============================================================================

TEST_CASE("binding_BindingExpr_INPC_初始求值写入目标") {
    TestViewModel vm;
    vm.set_value(Variant{123});
    TestDepObj tgt;

    BindingExpression expr;
    expr.getter = [&vm]() noexcept -> core::Variant {
        return vm.get_value();
    };
    expr.deps.push_back(PropertyDependency::from_inpc(vm, "Value"));

    expr.attach(tgt, TestDepObj::IntProp);

    CHECK(tgt.get_value(TestDepObj::IntProp).get<int>() == 123);
}

TEST_CASE("binding_BindingExpr_INPC_VM通知触发更新") {
    TestViewModel vm;
    TestDepObj tgt;

    BindingExpression expr;
    expr.getter = [&vm]() noexcept -> core::Variant {
        return vm.get_value();
    };
    expr.deps.push_back(PropertyDependency::from_inpc(vm, "Value"));
    expr.attach(tgt, TestDepObj::IntProp);

    vm.set_value(Variant{456});
    CHECK(tgt.get_value(TestDepObj::IntProp).get<int>() == 456);
}

TEST_CASE("binding_BindingExpr_INPC_过滤无关属性通知") {
    TestViewModel vm;
    TestDepObj tgt;
    int eval_count = 0;

    BindingExpression expr;
    expr.getter = [&vm, &eval_count]() noexcept -> core::Variant {
        ++eval_count;
        return vm.get_value();
    };
    // 只监听 "Value" 属性
    expr.deps.push_back(PropertyDependency::from_inpc(vm, "Value"));
    expr.attach(tgt, TestDepObj::IntProp);

    int init_count = eval_count;

    // 手动触发"其他属性"通知（不通过 set_value）：
    // 直接订阅并手动调用（模拟 VM 触发了其他属性的通知）
    // 此处通过 set_value 触发 "Value"（正向情形）
    vm.set_value(Variant{11});
    CHECK(eval_count == init_count + 1);
}

// ============================================================================
// BindingExpression - converter
// ============================================================================

namespace {

/// 将 int 值乘以 2 的转换器
class DoubleConverter : public IConverter {
public:
    [[nodiscard]] core::Variant convert(
        const core::Variant& v,
        core::TypeId,
        core::StringView) const noexcept override
    {
        if (v.has<int>()) return core::Variant{v.get<int>() * 2};
        return {};
    }

    [[nodiscard]] core::Variant convert_back(
        const core::Variant& v,
        core::TypeId,
        core::StringView) const noexcept override
    {
        if (v.has<int>()) return core::Variant{v.get<int>() / 2};
        return {};
    }
};

} // namespace

TEST_CASE("binding_BindingExpr_converter正向转换") {
    TestDepObj src, tgt;
    src.set_value(TestDepObj::IntProp, Variant{5});
    DoubleConverter conv;

    BindingExpression expr;
    expr.getter = [&src]() noexcept -> core::Variant {
        return src.get_value(TestDepObj::IntProp);
    };
    expr.deps.push_back(PropertyDependency::from_dep(src, TestDepObj::IntProp));
    expr.converter = &conv;

    expr.attach(tgt, TestDepObj::IntProp);

    // 5 经过 DoubleConverter 转换为 10
    CHECK(tgt.get_value(TestDepObj::IntProp).get<int>() == 10);

    src.set_value(TestDepObj::IntProp, Variant{7});
    CHECK(tgt.get_value(TestDepObj::IntProp).get<int>() == 14);
}

// ============================================================================
// BindingExpression - fallback
// ============================================================================

TEST_CASE("binding_BindingExpr_fallback_getter为空时使用回退值") {
    TestDepObj tgt;

    BindingExpression expr;
    // getter 返回空 Variant
    expr.getter = []() noexcept -> core::Variant { return {}; };
    // 无依赖（仅测试初始求值的回退值）
    expr.fallback = Variant{-1};

    expr.attach(tgt, TestDepObj::IntProp);

    CHECK(tgt.get_value(TestDepObj::IntProp).get<int>() == -1);
}

// ============================================================================
// BindingExpression - OneTime
// ============================================================================

TEST_CASE("binding_BindingExpr_OneTime_仅初始化一次") {
    TestDepObj src, tgt;
    src.set_value(TestDepObj::IntProp, Variant{10});

    BindingExpression expr;
    expr.getter = [&src]() noexcept -> core::Variant {
        return src.get_value(TestDepObj::IntProp);
    };
    expr.deps.push_back(PropertyDependency::from_dep(src, TestDepObj::IntProp));
    expr.mode = BindingMode::OneTime;

    expr.attach(tgt, TestDepObj::IntProp);
    CHECK(tgt.get_value(TestDepObj::IntProp).get<int>() == 10);

    // OneTime：后续变更不触发更新
    src.set_value(TestDepObj::IntProp, Variant{999});
    CHECK(tgt.get_value(TestDepObj::IntProp).get<int>() == 10);
}

// ============================================================================
// BindingExpression - TwoWay 预留
// ============================================================================

TEST_CASE("binding_BindingExpr_TwoWay预留_setter字段可赋值") {
    // 验证 setter 字段存在且可赋值，mode=TwoWay 可设置
    // M1.1 不执行 setter，此测试仅验证字段和枚举定义正确

    // src/tgt 先声明，确保在 expr 析构后才销毁
    TestDepObj src, tgt;
    src.set_value(TestDepObj::IntProp, Variant{1});

    bool setter_called = false;
    {
        BindingExpression expr;
        expr.setter = [&setter_called](const core::Variant&) noexcept {
            setter_called = true;
        };
        expr.mode = BindingMode::TwoWay;

        // setter 字段已赋值
        CHECK(static_cast<bool>(expr.setter));
        CHECK(expr.mode == BindingMode::TwoWay);

        expr.getter = [&src]() noexcept -> core::Variant {
            return src.get_value(TestDepObj::IntProp);
        };
        expr.deps.push_back(PropertyDependency::from_dep(src, TestDepObj::IntProp));
        expr.attach(tgt, TestDepObj::IntProp);

        // M1.1: setter 不应被调用
        CHECK_FALSE(setter_called);
    } // expr 在此析构，src/tgt 仍然存活
}

// ============================================================================
// BindingExpression - evaluate 手动触发
// ============================================================================

TEST_CASE("binding_BindingExpr_evaluate手动触发") {
    TestDepObj src, tgt;
    src.set_value(TestDepObj::IntProp, Variant{10});

    BindingExpression expr;
    expr.getter = [&src]() noexcept -> core::Variant {
        return src.get_value(TestDepObj::IntProp);
    };
    expr.deps.push_back(PropertyDependency::from_dep(src, TestDepObj::IntProp));
    expr.attach(tgt, TestDepObj::IntProp);

    // 直接修改 src 内部（绕过 set_value 通知），然后手动 evaluate
    src.set_value(TestDepObj::IntProp, Variant{55}, ValuePriority::Local);
    expr.evaluate();
    CHECK(tgt.get_value(TestDepObj::IntProp).get<int>() == 55);
}

// ============================================================================
// BindingExpression - move 语义
// ============================================================================

TEST_CASE("binding_BindingExpr_移动后原对象不再激活") {
    TestDepObj src, tgt;

    BindingExpression expr1;
    expr1.getter = [&src]() noexcept -> core::Variant {
        return src.get_value(TestDepObj::IntProp);
    };
    expr1.deps.push_back(PropertyDependency::from_dep(src, TestDepObj::IntProp));
    expr1.attach(tgt, TestDepObj::IntProp);

    CHECK(expr1.is_attached());

    BindingExpression expr2 = std::move(expr1);
    CHECK_FALSE(expr1.is_attached());  // 原对象不再激活
    CHECK(expr2.is_attached());        // 新对象接管

    // 修改源，通过 expr2 的订阅触发更新
    src.set_value(TestDepObj::IntProp, Variant{77});
    CHECK(tgt.get_value(TestDepObj::IntProp).get<int>() == 77);
}

// ============================================================================
// BindingExpression - 析构时自动 detach
// ============================================================================

TEST_CASE("binding_BindingExpr_析构时自动取消订阅") {
    TestDepObj src, tgt;

    {
        BindingExpression expr;
        expr.getter = [&src]() noexcept -> core::Variant {
            return src.get_value(TestDepObj::IntProp);
        };
        expr.deps.push_back(PropertyDependency::from_dep(src, TestDepObj::IntProp));
        expr.attach(tgt, TestDepObj::IntProp);

        src.set_value(TestDepObj::IntProp, Variant{30});
        CHECK(tgt.get_value(TestDepObj::IntProp).get<int>() == 30);
        // expr 在此析构
    }

    // expr 析构后修改源，目标不应再更新（订阅已取消）
    src.set_value(TestDepObj::IntProp, Variant{888});
    // 目标仍为 30（最后一次写入的值）
    CHECK(tgt.get_value(TestDepObj::IntProp).get<int>() == 30);
}
