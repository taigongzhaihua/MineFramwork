/**
 * @file TypeInfo.h
 * @brief TypeInfo — 类型的完整反射元数据描述符。
 *
 * TypeInfo 是 mine.reflect 模块的核心数据结构，汇集了一个类型的
 * 所有编译期可获知的元信息：名称、TypeId、基类、属性列表、方法列表。
 *
 * 每个通过 MINE_REFLECT_IMPL 注册的类型都会生成一个静态 TypeInfo 实例，
 * 并由 TypeRegistry 统一索引，支持按名称或 TypeId 查找。
 *
 * 设计目标：
 *   - 纯静态数据，无需堆分配（属性/方法列表通过 Span 引用外部静态数组）
 *   - 可通过名称查找属性/方法（线性搜索，适合属性/方法数较少的场景）
 *   - 支持继承链遍历（通过 base 指针）
 */

#pragma once

#include <mine/reflect/Api.h>
#include <mine/reflect/PropertyInfo.h>
#include <mine/reflect/MethodInfo.h>

#include <mine/core/TypeId.h>
#include <mine/core/StringView.h>
#include <mine/core/Span.h>

namespace mine::reflect {

// ──────────────────────────────────────────────────────────────────────────────
// TypeInfo
// ──────────────────────────────────────────────────────────────────────────────

/**
 * @brief 类型的完整反射元数据。
 *
 * TypeInfo 存储了一个 C++ 类型的编译期元信息，包括：
 *   - 类型名（用于序列化、日志、调试器）
 *   - TypeId（O(1) 类型鉴别）
 *   - 基类指针（支持继承链遍历，如 is_a 检查）
 *   - 属性列表（通过 Span<const PropertyInfo> 引用静态数组）
 *   - 方法列表（通过 Span<const MethodInfo> 引用静态数组）
 *
 * 所有成员均为编译期常量或指向静态数据的指针，TypeInfo 自身不拥有
 * 任何需要释放的资源。
 *
 * 用法：
 * @code
 *   const TypeInfo* type = TypeRegistry::instance().find_by_name("Button");
 *   if (type) {
 *       printf("类型: %s\n", type->name.data());
 *       for (auto& prop : type->properties) {
 *           printf("  .%s\n", prop.name.data());
 *       }
 *   }
 * @endcode
 */
struct TypeInfo {
    /**
     * @brief 类型名称（编译期常量字符串，无需释放）。
     *
     * 示例："Button"、"mine::ui::TextBlock"
     */
    mine::core::StringView name;

    /**
     * @brief 类型的 TypeId。
     *
     * 通过 TypeId::of<T>() 获取，可用于 O(1) 类型比较。
     */
    mine::core::TypeId type_id;

    /**
     * @brief 直接基类的 TypeInfo 指针。
     *
     * 若类型无基类（如非组件类型），则为 nullptr。
     * 可通过此指针沿继承链向上遍历。
     */
    const TypeInfo* base{nullptr};

    /**
     * @brief 属性元数据列表视图。
     *
     * 按 MINE_REFLECT_IMPL 中声明的顺序排列。
     * 查找属性使用 find_property() 方法。
     */
    mine::core::Span<const PropertyInfo> properties{};

    /**
     * @brief 方法元数据列表视图。
     *
     * 按 MINE_REFLECT_IMPL 中声明的顺序排列。
     * 查找方法使用 find_method() 方法。
     */
    mine::core::Span<const MethodInfo> methods{};

    /**
     * @brief 默认构造工厂函数。
     *
     * 若为 nullptr，表示该类型不支持通过反射创建实例。
     * 若有效，调用返回堆分配的对象指针（调用方负责通过 delete 释放，
     * 或通过类型自身的销毁机制管理生命周期）。
     */
    void* (*factory)() {nullptr};

    // ── 查询方法 ──────────────────────────────────────────────────────────────

    /**
     * @brief 按名称查找属性。
     *
     * @param name 属性名称。
     * @return 匹配的 PropertyInfo 指针；未找到则返回 nullptr。
     *
     * 时间复杂度：O(n)，其中 n = properties.size()
     */
    const PropertyInfo* find_property(mine::core::StringView prop_name) const noexcept {
        for (auto& prop : properties) {
            if (prop.name == prop_name) {
                return &prop;
            }
        }
        return nullptr;
    }

    /**
     * @brief 按名称查找方法。
     *
     * @param method_name 方法名称。
     * @return 匹配的 MethodInfo 指针；未找到则返回 nullptr。
     *
     * 时间复杂度：O(n)，其中 n = methods.size()
     */
    const MethodInfo* find_method(mine::core::StringView method_name) const noexcept {
        for (auto& method : methods) {
            if (method.name == method_name) {
                return &method;
            }
        }
        return nullptr;
    }

    /**
     * @brief 检查当前类型是否为指定基类的子类型（含自身）。
     *
     * 沿 base 指针链向上遍历，直到匹配或到达根部。
     *
     * @param base_type 候选基类的 TypeInfo 指针。
     * @return 若当前类型是 base_type 自身或其派生类，返回 true。
     *
     * 时间复杂度：O(h)，其中 h 为继承链深度
     */
    [[nodiscard]] bool is_a(const TypeInfo* base_type) const noexcept {
        const TypeInfo* current = this;
        while (current) {
            if (current == base_type) {
                return true;
            }
            current = current->base;
        }
        return false;
    }
};

} // namespace mine::reflect
