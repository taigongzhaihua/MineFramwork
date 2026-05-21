/**
 * @file InlineString.h
 * @brief 带小字符串优化（SSO）的 UTF-8 字符串类型。
 *
 * InlineString 在字符串长度 ≤ kInlineCap（默认 23 字节）时完全使用
 * 栈上内联存储，不进行任何堆分配；超过阈值时自动切换到堆分配。
 *
 * 内存布局（24 字节，与 std::string 的常见实现类似）：
 *   内联模式：[ char data[23] | len(1 byte) ]
 *   堆分配模式：[ char* ptr(8) | size_t size(8) | size_t cap(8) ] + 最高位标志
 *
 * 与 mine::core::StringView 的关系：
 *   - InlineString 拥有字符串数据（Owning）
 *   - StringView 是非拥有视图（Non-owning），InlineString 可隐式转换为 StringView
 *
 * 约束：
 *   - 内容始终以 '\0' 结尾（C 字符串兼容）
 *   - 仅处理 UTF-8 字节序列（不做 Unicode 验证）
 *   - 不支持 C++ 异常，分配失败通过 MINE_CHECK 断言终止
 */

#pragma once

#include <cstddef>
#include <cstring>
#include <new>        // 对齐 placement new（用于 operator=(InlineString&&)）
#include <type_traits>
#include <mine/core/Assert.h>
#include <mine/core/Allocator.h>
#include <mine/core/StringView.h>

namespace mine::containers {

/**
 * @brief 带 SSO 的拥有式 UTF-8 字符串。
 */
class InlineString {
public:
    using size_type       = size_t;
    using iterator        = char*;
    using const_iterator  = const char*;

    /// 内联存储容量（不含终止符），即可在栈上存储的最大字节数
    static constexpr size_type kInlineCap = 23;

    // ── 构造 / 析构 ───────────────────────────────────────────────────────

    InlineString(mine::core::IAllocator* alloc
                 = mine::core::default_allocator()) noexcept
        : alloc_{alloc}
    {
        // 初始化为空内联字符串
        inline_.len = 0;
        inline_.buf[0] = '\0';
    }

    /// 从 C 字符串构造
    InlineString(const char* str,
                 mine::core::IAllocator* alloc
                 = mine::core::default_allocator())
        : alloc_{alloc}
    {
        inline_.len = 0;
        inline_.buf[0] = '\0';
        const size_type len = str ? std::strlen(str) : 0;
        assign(str, len);
    }

    /// 从 StringView 构造
    /*implicit*/ InlineString(mine::core::StringView sv,  // NOLINT(google-explicit-constructor)
                              mine::core::IAllocator* alloc
                              = mine::core::default_allocator())
        : alloc_{alloc}
    {
        inline_.len = 0;
        inline_.buf[0] = '\0';
        assign(sv.data(), sv.size());
    }

    /// 从字节区间构造
    InlineString(const char* data, size_type len,
                 mine::core::IAllocator* alloc
                 = mine::core::default_allocator())
        : alloc_{alloc}
    {
        inline_.len = 0;
        inline_.buf[0] = '\0';
        assign(data, len);
    }

    InlineString(const InlineString& other)
        : alloc_{other.alloc_}
    {
        inline_.len = 0;
        inline_.buf[0] = '\0';
        assign(other.data(), other.size());
    }

    InlineString(InlineString&& other) noexcept
        : alloc_{other.alloc_}
    {
        if (other.is_heap()) {
            // 转移堆所有权
            heap_.ptr  = other.heap_.ptr;
            heap_.size = other.heap_.size;
            heap_.cap  = other.heap_.cap;
            set_heap_flag(true);
            other.heap_.ptr  = nullptr;
            other.heap_.size = 0;
            other.heap_.cap  = 0;
            other.set_heap_flag(false);
            other.inline_.len = 0;
            other.inline_.buf[0] = '\0';
        } else {
            // 拷贝内联数据
            std::memcpy(&inline_, &other.inline_, sizeof(inline_));
            other.inline_.len = 0;
            other.inline_.buf[0] = '\0';
        }
    }

    ~InlineString() noexcept {
        if (is_heap()) {
            alloc_->dealloc(heap_.ptr,
                            (heap_.cap + 1) * sizeof(char),
                            alignof(char));
        }
    }

    // ── 赋值 ──────────────────────────────────────────────────────────────

    InlineString& operator=(const InlineString& other) {
        if (this == &other) return *this;
        assign(other.data(), other.size());
        return *this;
    }

    InlineString& operator=(InlineString&& other) noexcept {
        if (this == &other) return *this;
        this->~InlineString();
        new (this) InlineString(std::move(other));
        return *this;
    }

    InlineString& operator=(const char* str) {
        assign(str, str ? std::strlen(str) : 0);
        return *this;
    }

    InlineString& operator=(mine::core::StringView sv) {
        assign(sv.data(), sv.size());
        return *this;
    }

    // ── 元素访问 ──────────────────────────────────────────────────────────

    [[nodiscard]] const char* c_str() const noexcept { return data(); }
    [[nodiscard]] const char* data()  const noexcept {
        return is_heap() ? heap_.ptr : inline_.buf;
    }
    [[nodiscard]] char* data() noexcept {
        return is_heap() ? heap_.ptr : inline_.buf;
    }

    [[nodiscard]] char operator[](size_type idx) const noexcept {
        MINE_ASSERT_MSG(idx < size(), "InlineString: 下标越界");
        return data()[idx];
    }
    [[nodiscard]] char& operator[](size_type idx) noexcept {
        MINE_ASSERT_MSG(idx < size(), "InlineString: 下标越界");
        return data()[idx];
    }

    // ── 容量 ──────────────────────────────────────────────────────────────

    [[nodiscard]] size_type size()   const noexcept {
        return is_heap() ? heap_.size : static_cast<size_type>(inline_.len);
    }
    [[nodiscard]] size_type length() const noexcept { return size(); }
    [[nodiscard]] bool      empty()  const noexcept { return size() == 0; }

    [[nodiscard]] size_type capacity() const noexcept {
        return is_heap() ? heap_.cap : kInlineCap;
    }

    /// 当前是否使用内联存储（未发生堆分配）
    [[nodiscard]] bool is_small() const noexcept { return !is_heap(); }

    void reserve(size_type new_cap) {
        if (new_cap <= capacity()) return;
        grow_to(new_cap);
    }

    // ── 修改器 ────────────────────────────────────────────────────────────

    void clear() noexcept {
        if (is_heap()) {
            heap_.size = 0;
            heap_.ptr[0] = '\0';
        } else {
            inline_.len = 0;
            inline_.buf[0] = '\0';
        }
    }

    void push_back(char c) {
        const size_type sz = size();
        ensure_capacity(sz + 1);
        char* d = data();
        d[sz] = c;
        d[sz + 1] = '\0';
        set_size(sz + 1);
    }

    void pop_back() noexcept {
        const size_type sz = size();
        MINE_ASSERT_MSG(sz > 0, "InlineString::pop_back: 字符串为空");
        data()[sz - 1] = '\0';
        set_size(sz - 1);
    }

    InlineString& append(const char* str, size_type len) {
        if (!str || len == 0) return *this;
        const size_type old_size = size();
        ensure_capacity(old_size + len);
        std::memcpy(data() + old_size, str, len);
        data()[old_size + len] = '\0';
        set_size(old_size + len);
        return *this;
    }

    InlineString& append(mine::core::StringView sv) {
        return append(sv.data(), sv.size());
    }

    InlineString& append(const InlineString& other) {
        return append(other.data(), other.size());
    }

    InlineString& operator+=(char c) {
        push_back(c);
        return *this;
    }

    InlineString& operator+=(const char* str) {
        return append(str, str ? std::strlen(str) : 0);
    }

    InlineString& operator+=(mine::core::StringView sv) {
        return append(sv);
    }

    InlineString& operator+=(const InlineString& other) {
        return append(other);
    }

    // ── 迭代器 ────────────────────────────────────────────────────────────

    [[nodiscard]] iterator       begin() noexcept       { return data(); }
    [[nodiscard]] const_iterator begin() const noexcept { return data(); }
    [[nodiscard]] iterator       end() noexcept         { return data() + size(); }
    [[nodiscard]] const_iterator end() const noexcept   { return data() + size(); }

    // ── 查找与子串 ────────────────────────────────────────────────────────

    static constexpr size_type npos = size_t(-1);

    [[nodiscard]] mine::core::StringView substr(
        size_type pos, size_type len = npos) const noexcept
    {
        const size_type sz = size();
        MINE_ASSERT_MSG(pos <= sz, "InlineString::substr: 起始位置越界");
        const size_type actual_len = (len == npos || pos + len > sz)
                                     ? sz - pos : len;
        return mine::core::StringView{data() + pos, actual_len};
    }

    [[nodiscard]] size_type find(char c, size_type pos = 0) const noexcept {
        const size_type sz = size();
        for (size_type i = pos; i < sz; ++i) {
            if (data()[i] == c) return i;
        }
        return npos;
    }

    [[nodiscard]] size_type find(mine::core::StringView sv,
                                  size_type pos = 0) const noexcept
    {
        const mine::core::StringView me{data(), size()};
        return me.find(sv, pos);
    }

    [[nodiscard]] bool starts_with(mine::core::StringView sv) const noexcept {
        return mine::core::StringView{data(), size()}.starts_with(sv);
    }

    [[nodiscard]] bool ends_with(mine::core::StringView sv) const noexcept {
        return mine::core::StringView{data(), size()}.ends_with(sv);
    }

    // ── 隐式转换为 StringView ─────────────────────────────────────────────

    /*implicit*/ operator mine::core::StringView() const noexcept {  // NOLINT(google-explicit-constructor)
        return mine::core::StringView{data(), size()};
    }

    // ── 比较 ──────────────────────────────────────────────────────────────

    [[nodiscard]] int compare(mine::core::StringView sv) const noexcept {
        const size_type a_sz = size();
        const size_type b_sz = sv.size();
        const size_type min_sz = a_sz < b_sz ? a_sz : b_sz;
        const int r = std::memcmp(data(), sv.data(), min_sz);
        if (r != 0) return r;
        if (a_sz < b_sz) return -1;
        if (a_sz > b_sz) return  1;
        return 0;
    }

    [[nodiscard]] bool operator==(const InlineString& other) const noexcept {
        return compare(mine::core::StringView{other.data(), other.size()}) == 0;
    }
    [[nodiscard]] bool operator!=(const InlineString& other) const noexcept {
        return !(*this == other);
    }
    [[nodiscard]] bool operator<(const InlineString& other) const noexcept {
        return compare(mine::core::StringView{other.data(), other.size()}) < 0;
    }
    [[nodiscard]] bool operator==(mine::core::StringView sv) const noexcept {
        return compare(sv) == 0;
    }
    [[nodiscard]] bool operator!=(mine::core::StringView sv) const noexcept {
        return !(*this == sv);
    }
    [[nodiscard]] bool operator==(const char* str) const noexcept {
        const mine::core::StringView sv{str};
        return compare(sv) == 0;
    }

    // ── 分配器 ────────────────────────────────────────────────────────────

    [[nodiscard]] mine::core::IAllocator* allocator() const noexcept {
        return alloc_;
    }

    void swap(InlineString& other) noexcept {
        InlineString tmp{std::move(other)};
        other = std::move(*this);
        *this = std::move(tmp);
    }

private:
    mine::core::IAllocator* alloc_ = mine::core::default_allocator();

    // ── 内存布局 ──────────────────────────────────────────────────────────
    //
    // 两种状态共用同一块内存（union）：
    //   内联模式：buf[0..22] 存字符数据，buf[23]='\0' 保留，len 存长度（0..23）
    //   堆模式：  ptr、size、cap 存储堆信息；inline_.len 最高位（bit7）置 1 作标志

    struct HeapRep {
        char*     ptr;   ///< 堆缓冲区指针（含终止符，分配大小 = cap+1）
        size_type size;  ///< 当前字符串长度（不含终止符）
        size_type cap;   ///< 堆缓冲区容量（不含终止符）
    };

    struct InlineRep {
        char          buf[kInlineCap];  ///< 内联字符缓冲区（含终止符最多 24 字节）
        unsigned char len;              ///< 当前长度（0..kInlineCap）；最高位=堆标志
    };

    union {
        HeapRep   heap_;
        InlineRep inline_;
    };

    /// 最高位用作堆分配标志（1 = 堆模式，0 = 内联模式）
    static constexpr unsigned char kHeapFlag = 0x80u;

    [[nodiscard]] bool is_heap() const noexcept {
        return (inline_.len & kHeapFlag) != 0;
    }

    void set_heap_flag(bool heap) noexcept {
        if (heap) inline_.len |= kHeapFlag;
        else      inline_.len &= static_cast<unsigned char>(~kHeapFlag);
    }

    void set_size(size_type sz) noexcept {
        if (is_heap()) {
            heap_.size = sz;
        } else {
            inline_.len = static_cast<unsigned char>(sz);
        }
    }

    /// 确保内部缓冲区容量 ≥ needed（不含终止符）
    void ensure_capacity(size_type needed) {
        if (needed <= capacity()) return;
        grow_to(needed);
    }

    /// 扩容到 new_cap（不含终止符）
    void grow_to(size_type new_cap) {
        const size_type old_size = size();
        // 2 倍增长策略
        size_type actual_cap = capacity() < 8 ? 16 : capacity() * 2;
        while (actual_cap < new_cap) actual_cap *= 2;

        void* raw = alloc_->alloc((actual_cap + 1) * sizeof(char), alignof(char));
        MINE_CHECK_MSG(raw != nullptr, "InlineString: 内存分配失败");
        char* new_buf = static_cast<char*>(raw);

        // 拷贝原数据（含终止符）
        std::memcpy(new_buf, data(), old_size + 1);

        // 释放旧堆内存（若有）
        if (is_heap()) {
            alloc_->dealloc(heap_.ptr,
                            (heap_.cap + 1) * sizeof(char),
                            alignof(char));
        }

        // 切换到堆模式
        heap_.ptr  = new_buf;
        heap_.size = old_size;
        heap_.cap  = actual_cap;
        set_heap_flag(true);
    }

    void assign(const char* src, size_type len) {
        if (len <= kInlineCap) {
            // 优先使用内联存储
            if (is_heap()) {
                // 若之前在堆上，释放堆内存并退回内联
                char* old_ptr = heap_.ptr;
                size_type old_cap = heap_.cap;
                set_heap_flag(false);
                if (src) std::memcpy(inline_.buf, src, len);
                inline_.buf[len] = '\0';
                inline_.len = static_cast<unsigned char>(len);
                alloc_->dealloc(old_ptr, (old_cap + 1) * sizeof(char), alignof(char));
            } else {
                if (src) std::memcpy(inline_.buf, src, len);
                inline_.buf[len] = '\0';
                inline_.len = static_cast<unsigned char>(len);
            }
        } else {
            ensure_capacity(len);
            if (src) std::memcpy(data(), src, len);
            data()[len] = '\0';
            set_size(len);
        }
    }
};

// ── 非成员运算符 ───────────────────────────────────────────────────────────

[[nodiscard]] inline InlineString operator+(
    const InlineString& a, mine::core::StringView b)
{
    InlineString result{a};
    result.append(b);
    return result;
}

[[nodiscard]] inline InlineString operator+(
    mine::core::StringView a, const InlineString& b)
{
    InlineString result{a, b.allocator()};
    result.append(b);
    return result;
}

[[nodiscard]] inline bool operator==(
    mine::core::StringView sv, const InlineString& s) noexcept
{
    return s == sv;
}
[[nodiscard]] inline bool operator!=(
    mine::core::StringView sv, const InlineString& s) noexcept
{
    return s != sv;
}

inline void swap(InlineString& a, InlineString& b) noexcept {
    a.swap(b);
}

// ── Hash 特化 ─────────────────────────────────────────────────────────────

} // namespace mine::containers

// 在定义 Hash<InlineString> 之前必须先 include Hash.h，
// 但 Hash.h 包含此头（潜在循环），因此特化在 Containers.h 中统一声明。
