/**
 * @file PropertyInfo.h
 * @brief PropertyInfo — 类型属性的元数据描述符。
 *
 * 每个 PropertyInfo 对应一个通过 MINE_REFLECT_IMPL 注册的类型属性。
 * 存储属性的名称、值类型、getter/setter 函数指针等元信息，
 * 支持运行期通过名称查找属性并进行读写操作。
 */

#pragma once

#include <mine/reflect/Fwd.h>
#include <mine/reflect/Api.h>

#include <mine/core/TypeId.h>
#include <mine/core/StringView.h>
#include <mine/core/Variant.h>

namespace mine::reflect {

// ──────────────────────────────────────────────────────────────────────────────
// PropertyInfo
// ──────────────────────────────────────────────────────────────────────────────

/**
 * @brief 类型属性的运行期元数据描述符。
 *
 * 每个已注册的属性对应一个 PropertyInfo 实例。getter/setter 通过函数指针
 * 以 void* + Variant 的擦除类型形式实现，避免模板膨胀。
 *
 * @note PropertyInfo 不拥有任何内存，其 name 字符串指向编译期常量。
 *       getter/setter 函数指针指向的 Lambda 函数亦为静态编译期生成。
 *
 * 用法：
 * @code
 *   const PropertyInfo* prop = type->find_property("Title");
 *   if (prop) {
 *       Variant val = prop->getter(obj);
 *       prop->setter(obj, Variant{"新标题"});
 *   }
 * @endcode
 */
struct PropertyInfo {
    /**
     * @brief 属性名称（编译期常量字符串，无需释放）。
     */
    mine::core::StringView name;

    /**
     * @brief 属性值类型的 TypeId。
     *
     * 可用于类型安全检查：`prop->type == TypeId::of<int>()`
     */
    mine::core::TypeId type;

    /**
     * @brief 所属类型的 TypeInfo 指针。
     *
     * 指向拥有该属性的类型的 TypeInfo 实例。
     */
    const TypeInfo* owner{nullptr};

    // ── 访问器 ────────────────────────────────────────────────────────────────

    /**
     * @brief getter：从对象指针读取属性值。
     *
     * @param obj 指向类型实例的指针（const，保证不修改对象）。
     *            调用方负责保证指针有效且类型匹配。
     * @return 属性当前值的 Variant 副本。
     *
     * @pre obj != nullptr 且 obj 的实际类型与 owner 一致。
     */
    mine::core::Variant (*getter)(const void* obj) {nullptr};

    /**
     * @brief setter：向对象指针写入属性值。
     *
     * @param obj   指向类型实例的指针（非 const）。
     *              调用方负责保证指针有效且类型匹配。
     * @param value 待写入的新值，其 type_id() 应与 PropertyInfo::type 匹配。
     *
     * @pre obj != nullptr 且 obj 的实际类型与 owner 一致。
     * @pre value.type_id() == type（类型不匹配会触发断言）
     */
    void (*setter)(void* obj, const mine::core::Variant& value) {nullptr};
};

} // namespace mine::reflect
