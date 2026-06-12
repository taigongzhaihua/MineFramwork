/**
 * @file MethodInfo.h
 * @brief MethodInfo — 类型方法的元数据描述符。
 *
 * 每个 MethodInfo 对应一个通过 MINE_REFLECT_IMPL 注册的类型方法。
 * 存储方法的名称、返回类型、参数类型列表和调用函数指针等元信息，
 * 支持运行期通过名称查找方法并进行调用。
 */

#pragma once

#include <mine/reflect/Fwd.h>
#include <mine/reflect/Api.h>

#include <mine/core/TypeId.h>
#include <mine/core/StringView.h>
#include <mine/core/Variant.h>
#include <mine/core/Span.h>

namespace mine::reflect {

// ──────────────────────────────────────────────────────────────────────────────
// MethodInfo
// ──────────────────────────────────────────────────────────────────────────────

/**
 * @brief 类型方法的运行期元数据描述符。
 *
 * 每个已注册的方法对应一个 MethodInfo 实例。invoke 通过函数指针以
 * void* + Span<Variant> 的擦除类型形式实现，支持可变参数调用。
 *
 * @note MethodInfo 不拥有任何内存，其 name 字符串指向编译期常量。
 *       invoke 函数指针指向的 Lambda 函数亦为静态编译期生成。
 *
 * 用法：
 * @code
 *   const MethodInfo* method = type->find_method("set_title");
 *   if (method) {
 *       Variant args[] = { Variant{"新标题"} };
 *       method->invoke(obj, Span<const Variant>(args, 1));
 *   }
 * @endcode
 */
struct MethodInfo {
    /**
     * @brief 方法名称（编译期常量字符串，无需释放）。
     */
    mine::core::StringView name;

    /**
     * @brief 返回类型的 TypeId。
     */
    mine::core::TypeId return_type;

    /**
     * @brief 参数类型列表的 TypeId 数组。
     *
     * 数组长度为 param_count，元素依次对应第 1..N 个参数的类型。
     * 若方法无参数，则 param_types 为 nullptr，param_count 为 0。
     */
    const mine::core::TypeId* param_types{nullptr};

    /**
     * @brief 参数个数。
     */
    uint32_t param_count{0};

    /**
     * @brief 所属类型的 TypeInfo 指针。
     */
    const TypeInfo* owner{nullptr};

    // ── 调用 ──────────────────────────────────────────────────────────────────

    /**
     * @brief 调用此方法。
     *
     * @param obj  指向类型实例的指针。对于静态方法可为 nullptr。
     *             调用方负责保证指针有效且类型匹配。
     * @param args 参数列表 Variant 视图，长度应与 param_count 一致。
     *             各参数的 type_id() 应与 param_types 对应项匹配。
     * @return 方法返回值的 Variant。若返回类型为 void，返回空 Variant。
     *
     * @pre args.size() == param_count（参数个数不匹配会触发断言）
     * @pre 对于非静态方法，obj 的实际类型与 owner 一致
     * @pre args[i].type_id() == param_types[i]（类型不匹配会触发断言）
     */
    mine::core::Variant (*invoke)(void* obj,
                                  mine::core::Span<const mine::core::Variant> args) {nullptr};
};

} // namespace mine::reflect
