/**
 * @file TypeRegistry.h
 * @brief TypeRegistry — 全局类型注册表。
 *
 * TypeRegistry 是一个进程级单例，维护所有通过 MINE_REFLECT_IMPL
 * 注册的类型元信息索引。支持按类型名（StringView）和 TypeId 双向查找。
 *
 * 使用场景：
 *   - 序列化/反序列化：通过类型名字符串查找 TypeInfo
 *   - UI 设计器：枚举可用组件类型
 *   - DI 容器：按接口名解析实现类型
 *
 * 线程安全：
 *   - 已注册的 TypeInfo 为只读（不可变），多线程并发读取安全
 *   - register_type() 应在静态初始化阶段（main 之前）完成，避免
 *     运行时多线程竞争
 */

#pragma once

#include <mine/reflect/Api.h>
#include <mine/reflect/TypeInfo.h>

#include <mine/core/TypeId.h>
#include <mine/core/StringView.h>
#include <mine/core/Span.h>

namespace mine::reflect {

// ──────────────────────────────────────────────────────────────────────────────
// TypeRegistry
// ──────────────────────────────────────────────────────────────────────────────

/**
 * @brief 进程级单例类型注册表。
 *
 * 在静态初始化阶段（MINE_REFLECT_IMPL 宏展开时）自动注册所有类型。
 * 应用层通常无需直接操作 TypeRegistry，除非需要序列化/反序列化或
 * 动态组件枚举。
 *
 * 用法：
 * @code
 *   // 按名称查找类型
 *   const TypeInfo* type = TypeRegistry::instance().find_by_name("Button");
 *
 *   // 按 TypeId 查找类型
 *   const TypeInfo* type2 = TypeRegistry::instance().find_by_id(TypeId::of<Button>());
 *
 *   // 枚举所有已注册的类型
 *   for (const TypeInfo* info : TypeRegistry::instance().all_types()) {
 *       printf("%s\n", info->name.data());
 *   }
 * @endcode
 */
class MINE_REFLECT_API TypeRegistry {
public:
    /**
     * @brief 获取全局唯一实例。
     *
     * 使用 Meyer's Singleton（C++11 线程安全）。
     */
    static TypeRegistry& instance() noexcept;

    // ── 注册 ──────────────────────────────────────────────────────────────────

    /**
     * @brief 注册一个类型的反射信息。
     *
     * @param info 指向静态 TypeInfo 实例的指针（指针本身及其引用的数据
     *             必须在进程生命周期内有效，通常为 static/global 变量）。
     *
     * @note 通常在 MINE_REFLECT_IMPL 宏展开中自动调用，无需手动注册。
     * @note 重复注册同名类型属于逻辑错误，当前实现会触发断言。
     */
    void register_type(const TypeInfo* info) noexcept;

    // ── 查询 ──────────────────────────────────────────────────────────────────

    /**
     * @brief 按类型名查找 TypeInfo。
     *
     * @param name 类型名（如 "Button"、"mine::ui::TextBlock"）。
     * @return 匹配的 TypeInfo 指针；未找到则返回 nullptr。
     *
     * 时间复杂度：O(n)，n 为已注册类型总数
     */
    [[nodiscard]] const TypeInfo* find_by_name(mine::core::StringView name) const noexcept;

    /**
     * @brief 按 TypeId 查找 TypeInfo。
     *
     * @param id 类型的 TypeId（需有效，即 valid() == true）。
     * @return 匹配的 TypeInfo 指针；未找到则返回 nullptr。
     *
     * 时间复杂度：O(n)，n 为已注册类型总数
     */
    [[nodiscard]] const TypeInfo* find_by_id(mine::core::TypeId id) const noexcept;

    /**
     * @brief 获取所有已注册类型的视图。
     *
     * @return 已注册 TypeInfo 指针的 Span 视图。
     *
     * 时间复杂度：O(1)
     */
    [[nodiscard]] mine::core::Span<const TypeInfo* const> all_types() const noexcept;

    /**
     * @brief 获取已注册类型总数。
     *
     * 时间复杂度：O(1)
     */
    [[nodiscard]] size_t type_count() const noexcept;

private:
    TypeRegistry() = default;
    ~TypeRegistry() = default;
    TypeRegistry(const TypeRegistry&) = delete;
    TypeRegistry& operator=(const TypeRegistry&) = delete;

    // 实现细节（Pimpl 或直接内部存储，取决于类型数量预期）
    class Impl;
    Impl* impl_{nullptr};
};

/**
 * @brief 便捷函数：按名称查找类型并返回其 TypeId。
 *
 * @param name 类型名称。
 * @return 匹配类型的 TypeId；未找到则返回无效 TypeId。
 */
[[nodiscard]] inline mine::core::TypeId type_id_by_name(mine::core::StringView name) noexcept {
    const TypeInfo* info = TypeRegistry::instance().find_by_name(name);
    return info ? info->type_id : mine::core::TypeId{};
}

/**
 * @brief 便捷函数：按 TypeId 查找类型并返回其名称。
 *
 * @param id 类型的 TypeId。
 * @return 匹配类型的名称；未找到则返回空 StringView。
 */
[[nodiscard]] inline mine::core::StringView type_name_by_id(mine::core::TypeId id) noexcept {
    const TypeInfo* info = TypeRegistry::instance().find_by_id(id);
    return info ? info->name : mine::core::StringView{};
}

} // namespace mine::reflect
