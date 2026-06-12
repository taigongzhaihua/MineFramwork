/**
 * @file ReflectMacros.h
 * @brief 类型反射注册宏：MINE_REFLECT_DECL / MINE_REFLECT_IMPL。
 */

#pragma once

#include <mine/reflect/TypeInfo.h>
#include <mine/reflect/PropertyInfo.h>
#include <mine/reflect/MethodInfo.h>
#include <mine/reflect/TypeRegistry.h>

/// @cond INTERNAL

#define MINE_REFL_CAT_I(A, B)  A##B
#define MINE_REFL_CAT(A, B)    MINE_REFL_CAT_I(A, B)
#define MINE_REFL_ID(NAME)     MINE_REFL_CAT(NAME, __LINE__)

/// @endcond

// ══════════════════════════════════════════════════════════════════════════════
// MINE_PROP_ENTRY
// ══════════════════════════════════════════════════════════════════════════════

#define MINE_PROP_ENTRY(OWNER, PROP_NAME, VALUE_TYPE, GETTER_FN, SETTER_FN) \
    ::mine::reflect::PropertyInfo{ \
        ::mine::core::StringView{#PROP_NAME}, \
        ::mine::core::TypeId::of<VALUE_TYPE>(), \
        nullptr, \
        [](const void* obj) -> ::mine::core::Variant { \
            auto* self = static_cast<const OWNER*>(obj); \
            return ::mine::core::Variant{self->GETTER_FN()}; \
        }, \
        [](void* obj, const ::mine::core::Variant& v) { \
            auto* self = static_cast<OWNER*>(obj); \
            self->SETTER_FN(::mine::core::any_cast<VALUE_TYPE>(v)); \
        } \
    }

// ══════════════════════════════════════════════════════════════════════════════
// 内部辅助函数
// ══════════════════════════════════════════════════════════════════════════════

namespace mine::reflect::detail {

template<typename Base>
inline const TypeInfo* resolve_base_type_info() {
    if constexpr (!std::is_same_v<Base, void>) {
        return Base::static_type_info();
    } else {
        return nullptr;
    }
}

} // namespace mine::reflect::detail

// ══════════════════════════════════════════════════════════════════════════════
// MINE_REFLECT_DECL
// ══════════════════════════════════════════════════════════════════════════════

#define MINE_REFLECT_DECL() \
    static const ::mine::reflect::TypeInfo* static_type_info(); \
    virtual const ::mine::reflect::TypeInfo* _get_reflect_type_info() const { return static_type_info(); }

// ══════════════════════════════════════════════════════════════════════════════
// MINE_REFLECT_IMPL
// ══════════════════════════════════════════════════════════════════════════════

#define MINE_REFLECT_IMPL(TYPE, BASE, ...) \
    \
    /* ── 属性元数据数组（哑元条目在前，避免零大小数组） ─────────────────── */ \
    static const ::mine::reflect::PropertyInfo \
        MINE_REFL_ID(_s_rf_props_)[] = { \
            /* 哑元条目：当 __VA_ARGS__ 为空时确保数组大小 ≥ 1 */ \
            ::mine::reflect::PropertyInfo{ \
                ::mine::core::StringView{}, \
                ::mine::core::TypeId{}, \
                nullptr, nullptr, nullptr \
            }, \
            __VA_ARGS__ \
        }; \
    \
    /* 属性计数：排除哑元条目 */ \
    static constexpr size_t MINE_REFL_ID(_s_rf_nprops_) = \
        (sizeof(MINE_REFL_ID(_s_rf_props_)) / sizeof(::mine::reflect::PropertyInfo)) - 1; \
    \
    /* 属性视图：跳过哑元条目 */ \
    static const ::mine::reflect::TypeInfo MINE_REFL_ID(_s_rf_type_) = { \
        ::mine::core::StringView{#TYPE}, \
        ::mine::core::TypeId::of<TYPE>(), \
        ::mine::reflect::detail::resolve_base_type_info<BASE>(), \
        ::mine::core::Span<const ::mine::reflect::PropertyInfo>( \
            (MINE_REFL_ID(_s_rf_nprops_) > 0) \
                ? (static_cast<const ::mine::reflect::PropertyInfo*>(MINE_REFL_ID(_s_rf_props_)) + 1) \
                : static_cast<const ::mine::reflect::PropertyInfo*>(nullptr), \
            MINE_REFL_ID(_s_rf_nprops_) \
        ), \
        ::mine::core::Span<const ::mine::reflect::MethodInfo>{}, \
        nullptr \
    }; \
    \
    namespace { \
        struct MINE_REFL_ID(_s_rf_auto_reg_) { \
            MINE_REFL_ID(_s_rf_auto_reg_)() noexcept { \
                ::mine::reflect::TypeRegistry::instance().register_type( \
                    &MINE_REFL_ID(_s_rf_type_)); \
            } \
        }; \
        static const MINE_REFL_ID(_s_rf_auto_reg_) MINE_REFL_ID(_s_rf_auto_inst_); \
    } \
    \
    const ::mine::reflect::TypeInfo* TYPE::static_type_info() { \
        return &MINE_REFL_ID(_s_rf_type_); \
    }
