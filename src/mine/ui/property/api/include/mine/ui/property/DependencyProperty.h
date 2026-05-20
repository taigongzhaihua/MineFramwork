/**
 * @file DependencyProperty.h
 * @brief 依赖属性描述符——全局唯一、静态注册的属性定义。
 *
 * DependencyProperty 描述一个"可被 DependencyObject 持有、具有优先级覆盖
 * 和变更通知能力"的属性。每个属性在程序启动时通过 register_property() 或
 * register_attached_property() 注册，返回对全局唯一实例的常量引用。
 *
 * 典型用法（头文件声明 + cpp 注册）：
 * @code
 *   // Button.h
 *   class Button : public Control {
 *   public:
 *       static const mine::ui::DependencyProperty& ContentProperty;
 *       void              set_content(mine::core::Variant v);
 *       mine::core::Variant content() const;
 *   };
 *
 *   // Button.cpp
 *   static void s_on_content_changed(mine::ui::DependencyObject* obj,
 *                                    const mine::ui::DependencyProperty&,
 *                                    const mine::core::Variant& old_v,
 *                                    const mine::core::Variant& new_v) {
 *       static_cast<Button*>(obj)->on_content_changed(old_v, new_v);
 *   }
 *
 *   const DependencyProperty& Button::ContentProperty =
 *       mine::ui::register_property(
 *           "Content",
 *           mine::core::TypeId::of<Button>(),
 *           mine::core::TypeId::of<mine::core::Variant>(),
 *           mine::core::Variant{},
 *           mine::ui::PropertyMetadata{
 *               .affects_measure = true,
 *               .changed = s_on_content_changed
 *           });
 * @endcode
 *
 * 或使用模板便捷版本：
 * @code
 *   const DependencyProperty& Button::ContentProperty =
 *       mine::ui::register_property<Button>(
 *           "Content",
 *           mine::core::Variant{},
 *           mine::ui::PropertyMetadata{.affects_measure = true});
 * @endcode
 */

#pragma once

#include <mine/core/StringView.h>
#include <mine/core/TypeId.h>
#include <mine/core/Variant.h>
#include <mine/ui/property/Api.h>
#include <mine/ui/property/PropertyMetadata.h>

namespace mine::ui {

/**
 * @brief 依赖属性描述符（全局唯一，不可拷贝/移动）。
 *
 * 外部代码只通过 `const DependencyProperty&` 引用使用此类型；
 * 实例由 register_property / register_attached_property 创建并由内部注册表管理生命周期。
 * 身份比较使用地址（operator==）。
 */
class MINE_UI_PROPERTY_API DependencyProperty {
public:
    // ── 禁止拷贝与移动（全局唯一实例，身份即地址）────────────────────────────
    DependencyProperty(const DependencyProperty&)            = delete;
    DependencyProperty& operator=(const DependencyProperty&) = delete;
    DependencyProperty(DependencyProperty&&)                 = delete;
    DependencyProperty& operator=(DependencyProperty&&)      = delete;

    // ── 属性查询 ──────────────────────────────────────────────────────────

    /// 属性名称（对应注册时传入的 name 参数，通常为字符串字面量）
    [[nodiscard]] core::StringView        name()          const noexcept;
    /// 注册此属性的所有者类型（对于附加属性，为定义者类型如 Grid）
    [[nodiscard]] core::TypeId            owner_type()    const noexcept;
    /// 属性值的预期类型（用于类型检查和调试）
    [[nodiscard]] core::TypeId            value_type()    const noexcept;
    /// 属性默认值（当 DependencyObject 中无任何有效值槽时返回此值）
    [[nodiscard]] const core::Variant&    default_value() const noexcept;
    /// 属性元数据（影响布局/渲染失效和继承行为）
    [[nodiscard]] const PropertyMetadata& metadata()      const noexcept;
    /// 是否为附加属性（如 Grid::ColumnProperty、Canvas::LeftProperty）
    [[nodiscard]] bool                    is_attached()   const noexcept;

    // ── 身份比较（地址比较）───────────────────────────────────────────────

    [[nodiscard]] bool operator==(const DependencyProperty& rhs) const noexcept {
        return this == &rhs;
    }
    [[nodiscard]] bool operator!=(const DependencyProperty& rhs) const noexcept {
        return this != &rhs;
    }

private:
    // 只有注册表和注册函数可以构造与析构
    friend class DependencyPropertyRegistry;

    DependencyProperty(core::StringView    name,
                       core::TypeId        owner_type,
                       core::TypeId        value_type,
                       core::Variant       default_value,
                       PropertyMetadata    metadata,
                       bool                is_attached) noexcept;

    ~DependencyProperty() noexcept;

    // ── 数据成员 ──────────────────────────────────────────────────────────
    // 属性名（指向字符串字面量，生命周期贯穿程序运行期）
    core::StringView  name_;
    core::TypeId      owner_type_;
    core::TypeId      value_type_;
    core::Variant     default_value_;
    PropertyMetadata  metadata_;
    bool              is_attached_;
};

// ── 注册函数（自由函数）──────────────────────────────────────────────────────

/**
 * @brief 注册一个普通依赖属性（非附加属性）。
 *
 * 此函数线程不安全，应在静态初始化阶段（程序启动时）调用。
 * 对同一 (owner_type, name) 重复注册将触发断言终止。
 *
 * @param name          属性名（应为字符串字面量，生命周期贯穿程序运行期）
 * @param owner_type    所有者类型（通常为 TypeId::of<MyClass>()）
 * @param value_type    属性值预期类型
 * @param default_value 属性默认值
 * @param metadata      属性元数据
 * @return              对新注册属性描述符的常量引用（全局唯一，地址稳定）
 */
MINE_UI_PROPERTY_API
const DependencyProperty& register_property(core::StringView   name,
                                            core::TypeId       owner_type,
                                            core::TypeId       value_type,
                                            core::Variant      default_value,
                                            PropertyMetadata   metadata = {});

/**
 * @brief 注册一个附加属性（如 Grid::ColumnProperty）。
 *
 * 附加属性可被任意 DependencyObject 持有，不限于 owner_type 的实例。
 * 其余语义与 register_property() 相同。
 */
MINE_UI_PROPERTY_API
const DependencyProperty& register_attached_property(core::StringView  name,
                                                     core::TypeId      owner_type,
                                                     core::TypeId      value_type,
                                                     core::Variant     default_value,
                                                     PropertyMetadata  metadata = {});

/**
 * @brief 模板便捷版本：自动推导 owner_type。
 *
 * @tparam TOwner 属性所属类型（register_property<Button>("Content", ...)）
 * @param name          属性名
 * @param default_value 默认值
 * @param metadata      元数据
 */
template<typename TOwner>
const DependencyProperty& register_property(core::StringView  name,
                                            core::Variant     default_value = {},
                                            PropertyMetadata  metadata      = {}) {
    return register_property(name,
                             core::TypeId::of<TOwner>(),
                             core::TypeId{},  // 值类型不强制（Variant 无类型）
                             std::move(default_value),
                             metadata);
}

/**
 * @brief 模板便捷版本：自动推导 owner_type 和 value_type。
 *
 * @tparam TOwner 属性所属类型
 * @tparam TValue 属性值类型（用于生成类型化 setter 时参考）
 */
template<typename TOwner, typename TValue>
const DependencyProperty& register_property(core::StringView  name,
                                            TValue            default_value = {},
                                            PropertyMetadata  metadata      = {}) {
    return register_property(name,
                             core::TypeId::of<TOwner>(),
                             core::TypeId::of<TValue>(),
                             core::Variant{std::move(default_value)},
                             metadata);
}

} // namespace mine::ui
