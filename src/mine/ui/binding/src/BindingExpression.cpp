/**
 * @file BindingExpression.cpp
 * @brief BindingExpression 运行时绑定实现。
 *
 * 核心数据结构：
 *   Impl::sub_records  — Vector<SubRecord>，存储每个依赖项的订阅令牌和元信息
 *   Impl::getter       — Function<Variant()>，源值求值闭包
 *   Impl::setter       — Function<void(const Variant&)>，回写闭包（TwoWay，M2）
 *   Impl::is_updating  — bool，防循环标志（TwoWay 预留，M1.1 始终 false）
 *
 * OneWay 求值流程（每次依赖源变更时触发）：
 *   1. 检查 is_updating（防止递归，TwoWay 时有效）
 *   2. 调用 getter() 获取源值
 *   3. 若有 converter，调用 convert() 转换
 *   4. 若值为空且有 fallback，使用 fallback
 *   5. 调用 target_obj->set_value(target_prop, value, ValuePriority::TemplateBind)
 *
 * 订阅机制：
 *   DependencyObject 来源：通过 DependencyObject::subscribe_property_changed()
 *     订阅，回调内过滤 dep_prop（因为 DependencyObject 通知所有属性变更）
 *   INotifyPropertyChanged 来源：通过 INotifyPropertyChanged::subscribe_property_changed()
 *     订阅，回调内过滤属性名（filter_name）
 *
 * 指针稳定性保证：
 *   订阅回调的 user_data 指向 SubRecord 元素。SubRecord 存储在 Vector 中，
 *   attach() 时通过 reserve() 确保容量，订阅完成前不再增长，指针不会失效。
 *   Impl 通过 Pimpl 存储于堆上，BindingExpression 移动时不改变 Impl 地址。
 */

#include <memory>  // std::construct_at / std::destroy_at（间接依赖 Variant.h）

#include <mine/ui/binding/BindingExpression.h>

#include <mine/core/Assert.h>
#include <mine/core/Pimpl.h>
#include <mine/containers/Vector.h>
#include <mine/ui/binding/IConverter.h>
#include <mine/ui/binding/INotifyPropertyChanged.h>
#include <mine/ui/property/DependencyObject.h>
#include <mine/ui/property/DependencyProperty.h>
#include <mine/ui/property/ValuePriority.h>

namespace mine::ui {

// ────────────────────────────────────────────────────────────────────────────
// Impl：PIMPL 实现结构体
// ────────────────────────────────────────────────────────────────────────────

struct BindingExpression::Impl {
    // ── 目标信息 ─────────────────────────────────────────────────────────────

    /// 目标 DependencyObject（attach 后有效；不拥有，调用方保证生命周期）
    DependencyObject*         target_obj  = nullptr;
    /// 目标属性描述符（attach 后有效；全局静态生命周期）
    const DependencyProperty* target_prop = nullptr;

    // ── 绑定配置（从 BindingExpression 公开字段移动而来）────────────────────

    BindingExpression::Getter getter;    ///< 源值求值闭包
    BindingExpression::Setter setter;    ///< 回写闭包（TwoWay，M2 激活）
    BindingMode               mode     = BindingMode::OneWay;
    IConverter*               converter = nullptr;
    core::Variant             fallback;

    // ── 防循环标志（TwoWay 预留，M1.1 始终 false）──────────────────────────

    /// TwoWay 模式下：正在将值写入目标时置 true，防止目标变更触发反向回调死循环
    bool is_updating = false;

    // ── 订阅记录 ─────────────────────────────────────────────────────────────

    /**
     * @brief 单个依赖源的订阅记录。
     *
     * 存储订阅令牌和回调过滤信息，用于事件分发和取消订阅。
     * 指向此记录的指针作为 user_data 传给订阅回调，必须保证地址稳定。
     * （通过 Vector::reserve() 后不再增长来保证）
     */
    struct SubRecord {
        PropertyDependency::Kind kind  = PropertyDependency::Kind::DependencyProperty;
        uint32_t                 token = 0; ///< 用于取消订阅的令牌

        // ── DependencyProperty 来源字段 ──────────────────────────────────────
        DependencyObject*         dep_obj     = nullptr; ///< 订阅的源对象
        const DependencyProperty* filter_prop = nullptr; ///< 过滤：只响应此属性变更

        // ── INPC 来源字段 ────────────────────────────────────────────────────
        INotifyPropertyChanged* inpc_src   = nullptr; ///< 订阅的 INPC 对象
        core::StringView        filter_name;           ///< 过滤：只响应此属性名（空串=全部）

        /// 指回 Impl，在订阅回调中触发重新求值（Impl 生命周期与 BindingExpression 相同）
        Impl* owner = nullptr;
    };

    /**
     * @brief 订阅记录列表。
     *
     * 使用 Vector（非 SmallVector）以确保 reserve() 后元素地址稳定。
     * 元素数量等于 deps 数量，attach() 时一次性填充，之后不再增长。
     */
    containers::Vector<SubRecord> sub_records;

    // ── 核心操作 ─────────────────────────────────────────────────────────────

    /**
     * @brief 重新求值并将结果写入目标属性。
     *
     * 调用链：getter() → converter（可选）→ fallback（可选）→ set_value()
     * 以 ValuePriority::TemplateBind 写入，低于 Local 优先级（本地设置不会被覆盖）。
     */
    void re_evaluate() noexcept {
        // 防循环保护（M1.1 中 is_updating 始终 false，此检查为 TwoWay 预留）
        if (is_updating) return;
        if (!getter)     return;

        // 调用 getter 获取源值
        core::Variant value = getter();

        // 若有转换器，进行类型/格式转换
        if (converter && value.has_value()) {
            value = converter->convert(value, target_prop->value_type(), {});
        }

        // 若结果为空，使用回退值
        if (!value.has_value() && fallback.has_value()) {
            value = fallback;
        }

        // 将有效值以 TemplateBind 优先级写入目标
        if (value.has_value()) {
            target_obj->set_value(*target_prop, std::move(value), ValuePriority::TemplateBind);
        }
    }

    /**
     * @brief 取消所有依赖源的订阅。
     *
     * 遍历 sub_records，根据来源类型分别调用对应的取消订阅方法。
     * 调用后 sub_records 被清空，target_obj/target_prop 置 nullptr。
     */
    void detach_impl() noexcept {
        for (auto& rec : sub_records) {
            if (rec.token == 0) continue;

            if (rec.kind == PropertyDependency::Kind::DependencyProperty) {
                if (rec.dep_obj) {
                    rec.dep_obj->unsubscribe_property_changed(rec.token);
                }
            } else {
                if (rec.inpc_src) {
                    rec.inpc_src->unsubscribe_property_changed(rec.token);
                }
            }
        }

        sub_records.clear();
        target_obj  = nullptr;
        target_prop = nullptr;
        getter.reset();
        setter.reset();
    }
};

// ────────────────────────────────────────────────────────────────────────────
// BindingExpression 构造 / 析构
// ────────────────────────────────────────────────────────────────────────────

BindingExpression::BindingExpression() noexcept
    : p_{core::make_pimpl<Impl>()}
{}

BindingExpression::~BindingExpression() {
    // 析构时自动取消所有订阅
    if (p_ && is_attached()) {
        detach();
    }
}

BindingExpression::BindingExpression(BindingExpression&& o) noexcept
    : getter{std::move(o.getter)}
    , setter{std::move(o.setter)}
    , deps{std::move(o.deps)}
    , mode{o.mode}
    , converter{o.converter}
    , fallback{std::move(o.fallback)}
    , p_{std::move(o.p_)}
{
    // 清理源对象的配置字段（Impl 已通过 Pimpl 移动，不需要额外处理订阅记录）
    o.mode      = BindingMode::OneWay;
    o.converter = nullptr;
}

BindingExpression& BindingExpression::operator=(BindingExpression&& o) noexcept {
    if (this != &o) {
        // 先分离当前绑定
        if (is_attached()) detach();

        getter    = std::move(o.getter);
        setter    = std::move(o.setter);
        deps      = std::move(o.deps);
        mode      = o.mode;
        converter = o.converter;
        fallback  = std::move(o.fallback);
        p_        = std::move(o.p_);

        o.mode      = BindingMode::OneWay;
        o.converter = nullptr;
    }
    return *this;
}

// ────────────────────────────────────────────────────────────────────────────
// attach：激活绑定
// ────────────────────────────────────────────────────────────────────────────

void BindingExpression::attach(
    DependencyObject&         target,
    const DependencyProperty& target_prop) noexcept
{
    MINE_ASSERT(static_cast<bool>(p_));
    MINE_ASSERT_MSG(!is_attached(), "BindingExpression: 已激活，请先调用 detach()");
    MINE_ASSERT_MSG(static_cast<bool>(getter), "BindingExpression: getter 不可为空");

    auto& impl = *p_;

    // ── 步骤 1：记录目标并移入配置字段 ──────────────────────────────────────

    impl.target_obj  = &target;
    impl.target_prop = &target_prop;
    impl.getter      = std::move(getter);
    impl.setter      = std::move(setter);
    impl.mode        = mode;
    impl.converter   = converter;
    impl.fallback    = std::move(fallback);

    // ── 步骤 2：预分配订阅记录（保证元素地址稳定，之后不再增长）───────────

    impl.sub_records.reserve(deps.size());

    // 先填充所有记录（不含 token），确保 reserve 后不再重分配
    for (const auto& dep : deps) {
        Impl::SubRecord rec;
        rec.kind  = dep.kind;
        rec.owner = &impl;

        if (dep.kind == PropertyDependency::Kind::DependencyProperty) {
            rec.dep_obj     = dep.dep_obj;
            rec.filter_prop = dep.dep_prop;
        } else {
            rec.inpc_src   = dep.inpc_src;
            rec.filter_name = dep.inpc_name;
        }

        impl.sub_records.push_back(std::move(rec));
    }

    deps.clear(); // 依赖已转移到内部记录

    // ── 步骤 3：遍历记录进行实际订阅（此时 Vector 不再增长，指针稳定）──────

    for (auto& rec : impl.sub_records) {
        if (rec.kind == PropertyDependency::Kind::DependencyProperty) {
            // 订阅 DependencyObject 的所有属性变更，在回调内过滤 filter_prop
            rec.token = rec.dep_obj->subscribe_property_changed(
                [](DependencyObject*,
                   const DependencyProperty& p,
                   const core::Variant&,
                   const core::Variant&,
                   void* ud) {
                    auto* r = static_cast<Impl::SubRecord*>(ud);
                    // 过滤：只响应目标属性变更
                    if (&p == r->filter_prop) {
                        r->owner->re_evaluate();
                    }
                },
                &rec);
        } else {
            // 订阅 INotifyPropertyChanged 的属性变更，按属性名过滤
            rec.token = rec.inpc_src->subscribe_property_changed(
                [](INotifyPropertyChanged*,
                   core::StringView prop_name,
                   void* ud) {
                    auto* r = static_cast<Impl::SubRecord*>(ud);
                    // filter_name 为空表示响应所有属性变更
                    if (r->filter_name.empty() || r->filter_name == prop_name) {
                        r->owner->re_evaluate();
                    }
                },
                &rec);
        }
    }

    // ── 步骤 4：立即求值一次写入目标 ─────────────────────────────────────────

    impl.re_evaluate();

    // ── 步骤 5：OneTime 模式：写入后立即取消订阅（不再响应后续变更）────────

    if (mode == BindingMode::OneTime) {
        // 仅取消订阅，保留 target_obj/target_prop 以供 is_attached() 判断
        for (auto& rec : impl.sub_records) {
            if (rec.token == 0) continue;
            if (rec.kind == PropertyDependency::Kind::DependencyProperty) {
                rec.dep_obj->unsubscribe_property_changed(rec.token);
            } else {
                rec.inpc_src->unsubscribe_property_changed(rec.token);
            }
            rec.token = 0;
        }
        impl.sub_records.clear();
    }
}

// ────────────────────────────────────────────────────────────────────────────
// detach：停用绑定
// ────────────────────────────────────────────────────────────────────────────

void BindingExpression::detach() noexcept {
    if (p_) {
        p_->detach_impl();
    }
}

// ────────────────────────────────────────────────────────────────────────────
// evaluate：手动触发重新求值
// ────────────────────────────────────────────────────────────────────────────

void BindingExpression::evaluate() noexcept {
    if (p_ && is_attached()) {
        p_->re_evaluate();
    }
}

// ────────────────────────────────────────────────────────────────────────────
// is_attached
// ────────────────────────────────────────────────────────────────────────────

bool BindingExpression::is_attached() const noexcept {
    return p_ && p_->target_obj != nullptr;
}

// ────────────────────────────────────────────────────────────────────────────
// bind_inpc：INPC OneWay 绑定便捷工厂
// ────────────────────────────────────────────────────────────────────────────

void BindingExpression::bind_inpc(
    BindingExpression&        out,
    INotifyPropertyChanged&   src,
    core::StringView          prop_name,
    Getter                    getter,
    DependencyObject&         target,
    const DependencyProperty& target_prop,
    BindingMode               mode) noexcept
{
    out.getter = std::move(getter);
    out.deps.push_back(PropertyDependency::from_inpc(src, prop_name));
    out.mode   = mode;
    out.attach(target, target_prop);
}

// ────────────────────────────────────────────────────────────────────────────
// bind：WPF 风格属性名绑定工厂（无需手写 getter lambda）
// ────────────────────────────────────────────────────────────────────────────

void BindingExpression::bind(
    BindingExpression&        out,
    INotifyPropertyChanged&   src,
    core::StringView          prop_name,
    DependencyObject&         target,
    const DependencyProperty& target_prop,
    BindingMode               mode) noexcept
{
    // getter 通过接口层的 get_property() 按属性名反射读取值。
    // ObservableObject（经 MINE_OBSERVABLE 自动注册）实现此接口；
    // 视图层完全无需手写任何 lambda，等价于 WPF 的 {Binding PropName} 语法。
    //
    // 注意：prop_name 以值捕获（StringView 是轻量的 pointer+size 结构）。
    //       源属性名须为字符串字面量或长期存活的字符串，同 bind_inpc 约定。
    out.getter = [&src, prop_name]() noexcept -> core::Variant {
        return src.get_property(prop_name);
    };
    out.deps.push_back(PropertyDependency::from_inpc(src, prop_name));
    out.mode = mode;
    out.attach(target, target_prop);
}

} // namespace mine::ui
