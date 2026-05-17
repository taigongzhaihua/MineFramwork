/**
 * @file StringView.h
 * @brief StringView — 非拥有的 UTF-8 字符串视图。
 *
 * 仅持有指针和长度，不分配堆内存，不持有所有权。
 * 公共头中不引入 <string> 或任何 STL 容器。
 *
 * 编码规范：
 *   - 始终假设内容为 UTF-8
 *   - 长度以字节（byte）为单位，非 Unicode 码点数
 *   - 允许内部出现 '\0'（不以 null 终止为前提），但提供 c_str() 辅助
 */

#pragma once

#include <cstddef>
#include <cstring>
#include <type_traits>

#include <mine/core/Assert.h>

namespace mine::core {

/**
 * @brief 非拥有 UTF-8 字符串视图，轻量可拷贝。
 *
 * 构造方式：
 * @code
 *   StringView sv{"hello"};          // 从字符串字面量
 *   StringView sv{ptr, len};         // 从指针+长度
 *   StringView sv{};                 // 空视图
 * @endcode
 */
class StringView {
public:
    using size_type = size_t;
    using value_type = char;

    /// 空视图
    constexpr StringView() noexcept = default;

    /// 从 null 结尾的 C 字符串构造（长度由 strlen 计算）
    /*implicit*/ StringView(const char* str) noexcept  // NOLINT(google-explicit-constructor)
        : data_{str}, size_{str ? std::strlen(str) : 0u}
    {}

    /// 从指针和字节长度构造（内容可含 '\0'）
    constexpr StringView(const char* str, size_type len) noexcept
        : data_{str}, size_{len}
    {
        MINE_ASSERT_MSG(str != nullptr || len == 0,
            "StringView: 非空长度但指针为 nullptr");
    }

    // ── 容量 ──────────────────────────────────────────────────────────────────

    [[nodiscard]] constexpr const char* data() const noexcept { return data_; }
    [[nodiscard]] constexpr size_type   size() const noexcept { return size_; }
    [[nodiscard]] constexpr bool        empty() const noexcept { return size_ == 0; }

    // ── 元素访问 ──────────────────────────────────────────────────────────────

    [[nodiscard]] constexpr char operator[](size_type i) const noexcept {
        MINE_ASSERT(i < size_);
        return data_[i];
    }

    [[nodiscard]] constexpr char front() const noexcept {
        MINE_ASSERT(!empty());
        return data_[0];
    }

    [[nodiscard]] constexpr char back() const noexcept {
        MINE_ASSERT(!empty());
        return data_[size_ - 1];
    }

    // ── 迭代器 ────────────────────────────────────────────────────────────────

    [[nodiscard]] constexpr const char* begin() const noexcept { return data_; }
    [[nodiscard]] constexpr const char* end()   const noexcept { return data_ + size_; }

    // ── 子视图 ────────────────────────────────────────────────────────────────

    /**
     * @brief 返回从 offset 开始、长度为 count 的子视图。
     * @param offset 起始字节偏移（闭区间）
     * @param count  字节数，默认到末尾
     */
    [[nodiscard]] StringView substr(size_type offset, size_type count = npos) const noexcept {
        MINE_ASSERT(offset <= size_);
        size_type available = size_ - offset;
        size_type actual = (count == npos || count > available) ? available : count;
        return StringView{data_ + offset, actual};
    }

    // ── 搜索 ──────────────────────────────────────────────────────────────────

    /**
     * @brief 在视图中查找字符 c 首次出现的位置（字节偏移）。
     * @return 偏移量，若未找到则返回 npos
     */
    [[nodiscard]] size_type find(char c, size_type from = 0) const noexcept {
        for (size_type i = from; i < size_; ++i) {
            if (data_[i] == c) return i;
        }
        return npos;
    }

    /**
     * @brief 在视图中查找子串首次出现的位置。
     * @return 偏移量，若未找到则返回 npos
     */
    [[nodiscard]] size_type find(StringView sub, size_type from = 0) const noexcept {
        if (sub.size_ == 0) return from;
        if (sub.size_ > size_) return npos;
        size_type limit = size_ - sub.size_;
        for (size_type i = from; i <= limit; ++i) {
            if (std::memcmp(data_ + i, sub.data_, sub.size_) == 0) {
                return i;
            }
        }
        return npos;
    }

    // ── 比较 ──────────────────────────────────────────────────────────────────

    [[nodiscard]] bool operator==(StringView other) const noexcept {
        if (size_ != other.size_) return false;
        return size_ == 0 || std::memcmp(data_, other.data_, size_) == 0;
    }
    [[nodiscard]] bool operator!=(StringView other) const noexcept {
        return !(*this == other);
    }
    [[nodiscard]] bool operator<(StringView other) const noexcept {
        size_type min_len = size_ < other.size_ ? size_ : other.size_;
        int cmp = min_len == 0 ? 0 : std::memcmp(data_, other.data_, min_len);
        if (cmp != 0) return cmp < 0;
        return size_ < other.size_;
    }

    // ── 前缀/后缀检测 ─────────────────────────────────────────────────────────

    [[nodiscard]] bool starts_with(StringView prefix) const noexcept {
        if (prefix.size_ > size_) return false;
        return std::memcmp(data_, prefix.data_, prefix.size_) == 0;
    }

    [[nodiscard]] bool ends_with(StringView suffix) const noexcept {
        if (suffix.size_ > size_) return false;
        return std::memcmp(data_ + size_ - suffix.size_, suffix.data_, suffix.size_) == 0;
    }

    // ── 辅助 ──────────────────────────────────────────────────────────────────

    /**
     * @brief 若视图本身以 '\0' 结尾（即 data_[size_] == '\0'），返回 C 风格指针。
     *
     * @warning 调用方须保证内存有效性；不保证 null 终止时请勿调用。
     *          通常仅对字符串字面量或以 null 终止的缓冲区构造的视图安全。
     */
    [[nodiscard]] const char* c_str() const noexcept { return data_; }

    /// "无效位置" 常量（与 std::string_view::npos 语义一致）
    static constexpr size_type npos = static_cast<size_type>(-1);

private:
    const char* data_{nullptr};
    size_type   size_{0};
};

} // namespace mine::core
