/**
 * @file TypeRegistry.cpp
 * @brief TypeRegistry 全局类型注册表的实现。
 *
 * 内部使用 Vector 存储已注册的 TypeInfo 指针，支持按名称和 TypeId 查找。
 */

#include <mine/reflect/TypeRegistry.h>
#include <mine/reflect/TypeInfo.h>

#include <mine/core/Assert.h>

// 内部需要 Vector 存储注册表，但不暴露在公共头中
#include <mine/containers/Vector.h>

namespace mine::reflect {

// ══════════════════════════════════════════════════════════════════════════════
// TypeRegistry::Impl
// ══════════════════════════════════════════════════════════════════════════════

/**
 * @brief TypeRegistry 的内部实现。
 *
 * 使用 Vector 存储已注册 TypeInfo 指针，提供按名称和 TypeId 的线性搜索。
 * 对于 UI 框架中预期的类型数量（数百个），线性搜索完全可接受。
 */
class TypeRegistry::Impl {
public:
    /**
     * @brief 已注册类型的 TypeInfo 指针列表。
     */
    mine::containers::Vector<const TypeInfo*> types_;

    /**
     * @brief 注册一个类型。
     *
     * @param info 类型元数据指针（需在进程生命周期内有效）。
     */
    void register_type(const TypeInfo* info) noexcept {
        MINE_ASSERT_MSG(info != nullptr, "TypeRegistry: 不能注册 nullptr TypeInfo");
        MINE_ASSERT_MSG(info->type_id.valid(), "TypeRegistry: TypeInfo 的 type_id 无效");

        // 检查是否重复注册（按 TypeId）
        for (auto* existing : types_) {
            if (existing->type_id == info->type_id) {
                MINE_ASSERT_MSG(false, "TypeRegistry: 类型已注册，不可重复注册");
                return;
            }
        }

        types_.push_back(info);
    }

    /**
     * @brief 按名称查找类型。
     */
    [[nodiscard]] const TypeInfo* find_by_name(mine::core::StringView name) const noexcept {
        for (auto* info : types_) {
            if (info->name == name) {
                return info;
            }
        }
        return nullptr;
    }

    /**
     * @brief 按 TypeId 查找类型。
     */
    [[nodiscard]] const TypeInfo* find_by_id(mine::core::TypeId id) const noexcept {
        if (!id.valid()) {
            return nullptr;
        }
        for (auto* info : types_) {
            if (info->type_id == id) {
                return info;
            }
        }
        return nullptr;
    }

    /**
     * @brief 获取所有类型视图。
     */
    [[nodiscard]] mine::core::Span<const TypeInfo* const> all_types() const noexcept {
        return mine::core::Span<const TypeInfo* const>(types_.data(), types_.size());
    }

    /**
     * @brief 获取已注册类型总数。
     */
    [[nodiscard]] size_t type_count() const noexcept {
        return types_.size();
    }
};

// ══════════════════════════════════════════════════════════════════════════════
// TypeRegistry 公开 API
// ══════════════════════════════════════════════════════════════════════════════

TypeRegistry& TypeRegistry::instance() noexcept {
    static TypeRegistry registry;
    return registry;
}

void TypeRegistry::register_type(const TypeInfo* info) noexcept {
    // 延迟初始化 Impl（首次注册时）
    if (!impl_) {
        impl_ = new Impl();
    }
    impl_->register_type(info);
}

const TypeInfo* TypeRegistry::find_by_name(mine::core::StringView name) const noexcept {
    if (!impl_) return nullptr;
    return impl_->find_by_name(name);
}

const TypeInfo* TypeRegistry::find_by_id(mine::core::TypeId id) const noexcept {
    if (!impl_) return nullptr;
    return impl_->find_by_id(id);
}

mine::core::Span<const TypeInfo* const> TypeRegistry::all_types() const noexcept {
    if (!impl_) {
        return mine::core::Span<const TypeInfo* const>{};
    }
    return impl_->all_types();
}

size_t TypeRegistry::type_count() const noexcept {
    if (!impl_) return 0;
    return impl_->type_count();
}

} // namespace mine::reflect
