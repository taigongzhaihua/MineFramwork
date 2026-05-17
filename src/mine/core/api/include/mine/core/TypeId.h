/**
 * @file TypeId.h
 * @brief TypeId — 无 RTTI 的编译期类型标识符。
 *
 * 实现原理：
 *   利用 C++17 inline 变量在同一翻译单元集合（链接单元）内地址唯一的特性，
 *   以 `&kTypeAnchor<T>` 作为类型的唯一标识符（uintptr_t 可比较的整数）。
 *
 * 约束：
 *   - TypeId 在单个可执行文件或单个 DLL 边界内是唯一且稳定的。
 *   - 跨 DLL 边界比较 TypeId 需要显式注册（见 mine.reflect 模块）；
 *     在 mine.core 内部的属性系统中仅在同一 DLL 内使用。
 *   - 不依赖 RTTI，不使用 std::type_info。
 *
 * 用法：
 * @code
 *   TypeId id = TypeId::of<Button>();
 *   if (element->type_id() == TypeId::of<Button>()) {
 *       auto* btn = static_cast<Button*>(element);
 *   }
 * @endcode
 */

#pragma once

#include <cstdint>
#include <type_traits>

namespace mine::core {

// ──────────────────────────────────────────────────────────────────────────────
// 内部锚点变量：每个类型 T 实例化一个唯一地址
// ──────────────────────────────────────────────────────────────────────────────

namespace detail {

/**
 * @brief 每个类型 T 对应一个 inline constexpr 变量，其地址在链接单元内唯一。
 *
 * 使用 char 而非 void 以满足 `addressof` 的合法性要求。
 * `std::remove_cvref_t<T>` 确保 cv 限定符和引用不影响 TypeId 的唯一性。
 */
template<typename T>
inline constexpr char kTypeAnchor = 0;

} // namespace detail

// ──────────────────────────────────────────────────────────────────────────────
// TypeId
// ──────────────────────────────────────────────────────────────────────────────

/**
 * @brief 无 RTTI 的类型标识符，以指针值比较实现 O(1) 类型判断。
 *
 * 轻量、可拷贝、可作为 HashMap 键（通过 hash() 方法）。
 * TypeId{} 为无效/空标识符。
 */
class TypeId {
public:
    /// 默认构造为无效 TypeId
    constexpr TypeId() noexcept = default;

    // ── 工厂 ──────────────────────────────────────────────────────────────────

    /**
     * @brief 获取类型 T 的 TypeId。
     *
     * cv 限定符与引用会被剥除（TypeId::of<T>() == TypeId::of<const T>()）。
     */
    template<typename T>
    [[nodiscard]] static constexpr TypeId of() noexcept {
        using DecayedT = std::remove_cvref_t<T>;
        return TypeId{&detail::kTypeAnchor<DecayedT>};
    }

    // ── 查询 ──────────────────────────────────────────────────────────────────

    /// 是否有效（非默认构造的空 TypeId）
    [[nodiscard]] constexpr bool valid() const noexcept {
        return anchor_ != nullptr;
    }

    /**
     * @brief 返回可用于哈希表的整数值。
     *
     * 值本身无语义，仅保证同一类型 TypeId 返回相同值，不同类型返回不同值。
     */
    [[nodiscard]] uintptr_t hash() const noexcept {
        return reinterpret_cast<uintptr_t>(anchor_);
    }

    // ── 比较 ──────────────────────────────────────────────────────────────────

    [[nodiscard]] constexpr bool operator==(TypeId other) const noexcept {
        return anchor_ == other.anchor_;
    }
    [[nodiscard]] constexpr bool operator!=(TypeId other) const noexcept {
        return anchor_ != other.anchor_;
    }

    /// 提供排序能力（用于有序容器）
    [[nodiscard]] bool operator<(TypeId other) const noexcept {
        return reinterpret_cast<uintptr_t>(anchor_) <
               reinterpret_cast<uintptr_t>(other.anchor_);
    }

private:
    /// 私有构造：仅由 of<T>() 工厂调用
    constexpr explicit TypeId(const char* anchor) noexcept : anchor_{anchor} {}

    const char* anchor_{nullptr};
};

} // namespace mine::core
