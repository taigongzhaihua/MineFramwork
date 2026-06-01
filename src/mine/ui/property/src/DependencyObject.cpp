/**
 * @file DependencyObject.cpp
 * @brief DependencyObject 依赖属性值存储与变更通知实现。
 *
 * 核心数据结构：
 *   Impl::slots     — SmallVector<ValueSlot, 16>，存储所有优先级的有效值
 *   Impl::subs      — Vector<Subscription>，存储属性变更事件订阅者
 *
 * 有效值查找规则：
 *   遍历 slots，找到匹配属性且优先级最高的槽，其值即为生效值。
 *   若无匹配槽，则返回 DependencyProperty::default_value()。
 *
 * 变更通知触发条件：
 *   set_value() 调用后，若生效值来源槽发生变化（新槽优先级更高，
 *   或更新了原来最高优先级的槽），则依次执行：
 *     1. on_property_changed() 虚方法（子类可覆盖）
 *     2. invalidate_measure/arrange/render()（根据 PropertyMetadata）
 *     3. PropertyMetadata.changed 回调
 *     4. 所有外部订阅者回调
 *   使用 is_notifying_ 标志防止递归触发。
 */

#include <mine/ui/property/DependencyObject.h>

#include <memory>  // std::construct_at / std::destroy_at（Variant.h 依赖）
#include <mine/core/Assert.h>
#include <mine/core/Pimpl.h>
#include <mine/containers/SmallVector.h>
#include <mine/containers/Vector.h>

namespace mine::ui {

// ────────────────────────────────────────────────────────────────────────────
// Impl：PIMPL 实现结构体
// ────────────────────────────────────────────────────────────────────────────

struct DependencyObject::Impl {
    // ── 属性值槽 ──────────────────────────────────────────────────────────

    /**
     * @brief 单个优先级的属性值存储槽。
     *
     * 同一属性可以在多个优先级上各有一个槽（如 Local + StyleSetter），
     * get_value() 返回优先级最高的槽的值。
     */
    struct ValueSlot {
        const DependencyProperty* prop;      ///< 指向属性描述符（地址即身份，不拥有）
        ValuePriority             priority;  ///< 值优先级
        core::Variant             value;     ///< 存储的值
    };

    /// 有效值槽集合（大多数控件属性数量 < 16，内联存储不触发堆分配）
    containers::SmallVector<ValueSlot, 16> slots;

    // ── 属性变更事件订阅 ──────────────────────────────────────────────────

    /**
     * @brief 属性变更事件订阅条目。
     */
    struct Subscription {
        uint32_t                  id;        ///< 唯一订阅 token
        PropertyChangedCallbackFn callback;  ///< 回调函数指针
        void*                     user_data; ///< 用户自定义数据（原样回传）
    };

    /// 订阅者列表（通常很少，不做小对象优化）
    containers::Vector<Subscription> subs;

    /// 下一个订阅 token（从 1 递增，0 保留为"无效"）
    uint32_t next_sub_id = 1u;

    /// 防递归标志：通知回调期间为 true，期间再次调用 set_value() 将被忽略
    bool is_notifying = false;

    // ── 辅助：查找生效槽（优先级最高的匹配槽）────────────────────────────

    /**
     * @brief 在 slots 中查找属性 prop 优先级最高的槽（生效槽）。
     * @return 指向生效槽的指针，若无匹配则返回 nullptr
     */
    [[nodiscard]] ValueSlot* find_effective(const DependencyProperty& prop) noexcept {
        ValueSlot* best = nullptr;
        for (ValueSlot& s : slots) {
            if (s.prop != &prop) {
                continue;
            }
            if (best == nullptr
                || static_cast<uint8_t>(s.priority) > static_cast<uint8_t>(best->priority)) {
                best = &s;
            }
        }
        return best;
    }

    [[nodiscard]] const ValueSlot* find_effective(const DependencyProperty& prop) const noexcept {
        const ValueSlot* best = nullptr;
        for (const ValueSlot& s : slots) {
            if (s.prop != &prop) {
                continue;
            }
            if (best == nullptr
                || static_cast<uint8_t>(s.priority) > static_cast<uint8_t>(best->priority)) {
                best = &s;
            }
        }
        return best;
    }

    /// 查找 priority <= max_priority 的槽中优先级最高的槽（用于动画 to 值解析）
    [[nodiscard]] const ValueSlot* find_effective_at_or_below(
        const DependencyProperty& prop,
        ValuePriority             max_priority) const noexcept {
        const ValueSlot* best = nullptr;
        for (const ValueSlot& s : slots) {
            if (s.prop != &prop) {
                continue;
            }
            if (static_cast<uint8_t>(s.priority) > static_cast<uint8_t>(max_priority)) {
                continue;  // 超出优先级上限，跳过
            }
            if (best == nullptr
                || static_cast<uint8_t>(s.priority) > static_cast<uint8_t>(best->priority)) {
                best = &s;
            }
        }
        return best;
    }

    /**
     * @brief 查找属性 prop 在指定优先级的槽。
     * @return 指向槽的指针，若不存在则返回 nullptr
     */
    [[nodiscard]] ValueSlot* find_slot(const DependencyProperty& prop,
                                       ValuePriority             priority) noexcept {
        for (ValueSlot& s : slots) {
            if (s.prop == &prop && s.priority == priority) {
                return &s;
            }
        }
        return nullptr;
    }

    [[nodiscard]] const ValueSlot* find_slot(const DependencyProperty& prop,
                                             ValuePriority             priority) const noexcept {
        for (const ValueSlot& s : slots) {
            if (s.prop == &prop && s.priority == priority) {
                return &s;
            }
        }
        return nullptr;
    }
};

// ────────────────────────────────────────────────────────────────────────────
// DependencyObject 实现
// ────────────────────────────────────────────────────────────────────────────

DependencyObject::DependencyObject()
    : p_{core::make_pimpl<Impl>()}
{}

DependencyObject::~DependencyObject() = default;

// ── get_value ────────────────────────────────────────────────────────────────

const core::Variant& DependencyObject::get_value(
    const DependencyProperty& prop) const noexcept {
    // 找到优先级最高的有效槽，返回其值
    const Impl::ValueSlot* slot = p_->find_effective(prop);
    if (slot != nullptr) {
        return slot->value;
    }
    // 无有效槽：返回属性默认值（引用指向 DependencyProperty 内部，生命周期稳定）
    return prop.default_value();
}

const core::Variant& DependencyObject::get_value(
    const DependencyProperty& prop,
    ValuePriority             max_priority) const noexcept {
    // 找到优先级 <= max_priority 中最高的有效槽，返回其值
    const Impl::ValueSlot* slot = p_->find_effective_at_or_below(prop, max_priority);
    if (slot != nullptr) {
        return slot->value;
    }
    // 无满足条件的有效槽：返回属性默认值
    return prop.default_value();
}

// ── set_value ────────────────────────────────────────────────────────────────

void DependencyObject::set_value(const DependencyProperty& prop,
                                 core::Variant             value,
                                 ValuePriority             priority) {
    // 防递归：通知回调期间再次设置同属性会被忽略
    if (p_->is_notifying) {
        return;
    }

    // 记录变更前的生效槽（用于判断生效值是否真正改变）
    const Impl::ValueSlot* prev_effective = p_->find_effective(prop);

    // 找到对应优先级的槽（存在则更新，不存在则插入）
    Impl::ValueSlot* target = p_->find_slot(prop, priority);
    if (target != nullptr) {
        // 更新已有槽的值
        target->value = std::move(value);
    } else {
        // 插入新槽
        p_->slots.push_back({&prop, priority, std::move(value)});
        // push_back 可能导致 SmallVector 重新分配，重新获取指针
        target = p_->find_slot(prop, priority);
    }

    // 重新查找生效槽（插入/更新后可能变化）
    Impl::ValueSlot* new_effective = p_->find_effective(prop);

    // 判断生效值是否发生了变更：
    //   情形1：生效槽指针变化（新槽优先级更高取代了旧生效槽）
    //   情形2：生效槽未变但我们更新了它的值（target == new_effective）
    const bool effective_changed = (new_effective != prev_effective)
                                || (new_effective == target);

    if (!effective_changed) {
        return;
    }

    // ── 触发变更通知 ──────────────────────────────────────────────────────

    // 获取变更前的生效值（用于传递给回调）
    // prev_effective 为 nullptr 时使用属性默认值
    const core::Variant& old_val = (prev_effective != nullptr)
                                       ? prev_effective->value
                                       : prop.default_value();
    const core::Variant& new_val = new_effective->value;

    // 设置防递归标志
    p_->is_notifying = true;

    // 1. 虚方法通知（子类可覆盖）
    on_property_changed(prop, old_val, new_val);

    // 2. 根据属性元数据触发布局/绘制失效
    const PropertyMetadata& meta = prop.metadata();
    if (meta.affects_measure) {
        invalidate_measure();
    }
    if (meta.affects_arrange) {
        invalidate_arrange();
    }
    if (meta.affects_render) {
        invalidate_render();
    }

    // 3. 属性元数据中注册的变更回调
    if (meta.changed != nullptr) {
        meta.changed(this, prop, old_val, new_val);
    }

    // 4. 外部订阅者回调（遍历快照，避免回调内取消订阅导致迭代器失效）
    // 这里直接遍历（不做快照），因为 is_notifying 标志会阻止递归写操作
    for (const Impl::Subscription& sub : p_->subs) {
        sub.callback(this, prop, old_val, new_val, sub.user_data);
    }

    p_->is_notifying = false;
}

// ── clear_value ──────────────────────────────────────────────────────────────

void DependencyObject::clear_value(const DependencyProperty& prop,
                                   ValuePriority             priority) {
    if (p_->is_notifying) {
        return;
    }

    // 查找待清除的槽
    Impl::ValueSlot* target = p_->find_slot(prop, priority);
    if (target == nullptr) {
        return;  // 槽不存在，空操作
    }

    // 记录清除前的生效槽和有效值
    const Impl::ValueSlot* prev_effective = p_->find_effective(prop);
    const core::Variant& old_val = (prev_effective != nullptr)
                                       ? prev_effective->value
                                       : prop.default_value();

    // 从 slots 中移除（通过与末尾元素交换再弹出，保持 O(1)，与顺序无关）
    auto& slots = p_->slots;
    if (target != &slots.back()) {
        std::swap(*target, slots.back());
    }
    slots.pop_back();

    // 检查清除后生效值是否变化
    const Impl::ValueSlot* new_effective = p_->find_effective(prop);

    // 若之前生效槽就是被删除的槽，则生效值必然变化
    const bool effective_changed = (prev_effective == target)
                                || (new_effective != prev_effective);

    if (!effective_changed) {
        return;
    }

    // 触发变更通知
    const core::Variant& new_val = (new_effective != nullptr)
                                       ? new_effective->value
                                       : prop.default_value();

    p_->is_notifying = true;

    on_property_changed(prop, old_val, new_val);

    const PropertyMetadata& meta = prop.metadata();
    if (meta.affects_measure) {
        invalidate_measure();
    }
    if (meta.affects_arrange) {
        invalidate_arrange();
    }
    if (meta.affects_render) {
        invalidate_render();
    }
    if (meta.changed != nullptr) {
        meta.changed(this, prop, old_val, new_val);
    }
    for (const Impl::Subscription& sub : p_->subs) {
        sub.callback(this, prop, old_val, new_val, sub.user_data);
    }

    p_->is_notifying = false;
}

// ── has_value ─────────────────────────────────────────────────────────────────

bool DependencyObject::has_value(const DependencyProperty& prop,
                                 ValuePriority             priority) const noexcept {
    return p_->find_slot(prop, priority) != nullptr;
}

// ── subscribe_property_changed ───────────────────────────────────────────────

uint32_t DependencyObject::subscribe_property_changed(PropertyChangedCallbackFn callback,
                                                       void*                     user_data) {
    MINE_ASSERT_MSG(callback != nullptr, "subscribe_property_changed: callback 不可为 nullptr");
    if (callback == nullptr) {
        return 0u;
    }
    const uint32_t id = p_->next_sub_id++;
    p_->subs.push_back({id, callback, user_data});
    return id;
}

// ── unsubscribe_property_changed ─────────────────────────────────────────────

void DependencyObject::unsubscribe_property_changed(uint32_t token) {
    auto& subs = p_->subs;
    for (size_t i = 0; i < subs.size(); ++i) {
        if (subs[i].id == token) {
            // 与末尾元素交换后弹出（O(1)，订阅列表无顺序要求）
            if (i + 1 < subs.size()) {
                std::swap(subs[i], subs.back());
            }
            subs.pop_back();
            return;
        }
    }
}

// ── 虚方法默认实现 ────────────────────────────────────────────────────────────

void DependencyObject::on_property_changed(const DependencyProperty& /*prop*/,
                                           const core::Variant&      /*old_value*/,
                                           const core::Variant&      /*new_value*/) {
    // 默认空实现；子类可覆盖以响应特定属性变更
}

void DependencyObject::invalidate_measure() {
    // 默认空实现；UIElement 子类覆盖以通知布局系统
}

void DependencyObject::invalidate_arrange() {
    // 默认空实现；UIElement 子类覆盖以通知布局系统
}

void DependencyObject::invalidate_render() {
    // 默认空实现；UIElement 子类覆盖以请求重绘
}

} // namespace mine::ui
