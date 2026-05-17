/**
 * @file Hash.h
 * @brief 键哈希函数特化与哈希工具，供 HashMap 等关联容器使用。
 *
 * 默认哈希算法：FNV-1a（32 位），特化了以下类型：
 *   - 整数类型（uint8/16/32/64、int8/16/32/64）
 *   - 指针类型
 *   - mine::core::StringView
 *   - float / double（按位解释为整数再哈希）
 *
 * 用户可为自定义类型特化 mine::containers::Hash<T>，重载 operator() 即可。
 */

#pragma once

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <type_traits>
#include <mine/core/StringView.h>

namespace mine::containers {

// ──────────────────────────────────────────────────────────────────────────────
// FNV-1a 哈希常量
// ──────────────────────────────────────────────────────────────────────────────

namespace detail {

/// FNV-1a 32 位偏置初始值
inline constexpr uint32_t kFnvOffset32 = 2166136261u;
/// FNV-1a 32 位质数
inline constexpr uint32_t kFnvPrime32  = 16777619u;

/// 对 len 字节的内存块进行 FNV-1a 32 位哈希
[[nodiscard]] inline constexpr uint32_t fnv1a_32(
    const void* data, size_t len) noexcept
{
    const auto* bytes = static_cast<const uint8_t*>(data);
    uint32_t hash = kFnvOffset32;
    for (size_t i = 0; i < len; ++i) {
        hash ^= static_cast<uint32_t>(bytes[i]);
        hash *= kFnvPrime32;
    }
    return hash;
}

} // namespace detail

// ──────────────────────────────────────────────────────────────────────────────
// Hash<T> 主模板（未特化时为删除状态，强制用户显式特化）
// ──────────────────────────────────────────────────────────────────────────────

/**
 * @brief 键哈希函数模板。
 *
 * 为自定义类型实现哈希时特化此模板，提供：
 *   size_t operator()(const T& key) const noexcept;
 */
template<typename T, typename = void>
struct Hash;

// ──────────────────────────────────────────────────────────────────────────────
// 整数类型特化
// ──────────────────────────────────────────────────────────────────────────────

/// 整数类型的通用哈希特化（覆盖 int8/16/32/64 以及对应的无符号版本）
template<typename T>
struct Hash<T, std::enable_if_t<std::is_integral_v<T>>> {
    [[nodiscard]] size_t operator()(T key) const noexcept {
        // 将整数值扩展为 uint64 后用 FNV-1a 哈希以减少碰撞
        const uint64_t v = static_cast<uint64_t>(
            static_cast<std::make_unsigned_t<T>>(key));
        return static_cast<size_t>(detail::fnv1a_32(&v, sizeof(v)));
    }
};

// ──────────────────────────────────────────────────────────────────────────────
// 指针类型特化
// ──────────────────────────────────────────────────────────────────────────────

template<typename T>
struct Hash<T*> {
    [[nodiscard]] size_t operator()(T* ptr) const noexcept {
        // 将指针值按整数处理，右移低几位（通常对齐导致低位为 0）
        auto v = reinterpret_cast<uintptr_t>(ptr);
        v >>= alignof(T) > 1 ? 1 : 0;
        return static_cast<size_t>(detail::fnv1a_32(&v, sizeof(v)));
    }
};

// ──────────────────────────────────────────────────────────────────────────────
// float / double 特化（按位解释为整数）
// ──────────────────────────────────────────────────────────────────────────────

template<>
struct Hash<float> {
    [[nodiscard]] size_t operator()(float key) const noexcept {
        // 将 -0.0 规范化为 0.0，保证哈希一致性
        if (key == 0.0f) key = 0.0f;
        uint32_t v{};
        static_assert(sizeof(v) == sizeof(key));
        std::memcpy(&v, &key, sizeof(v));
        return static_cast<size_t>(detail::fnv1a_32(&v, sizeof(v)));
    }
};

template<>
struct Hash<double> {
    [[nodiscard]] size_t operator()(double key) const noexcept {
        if (key == 0.0) key = 0.0;
        uint64_t v{};
        static_assert(sizeof(v) == sizeof(key));
        std::memcpy(&v, &key, sizeof(v));
        return static_cast<size_t>(detail::fnv1a_32(&v, sizeof(v)));
    }
};

// ──────────────────────────────────────────────────────────────────────────────
// StringView 特化
// ──────────────────────────────────────────────────────────────────────────────

template<>
struct Hash<mine::core::StringView> {
    [[nodiscard]] size_t operator()(mine::core::StringView sv) const noexcept {
        return static_cast<size_t>(
            detail::fnv1a_32(sv.data(), sv.size()));
    }
};

// ──────────────────────────────────────────────────────────────────────────────
// Equal<T> 主模板（使用 operator== 即可）
// ──────────────────────────────────────────────────────────────────────────────

/**
 * @brief 键相等比较函数模板。
 *
 * 默认实现使用 operator==，用户可特化以提供自定义比较逻辑。
 */
template<typename T>
struct Equal {
    [[nodiscard]] bool operator()(const T& a, const T& b) const noexcept {
        return a == b;
    }
};

} // namespace mine::containers
