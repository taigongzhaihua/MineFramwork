/**
 * @file Path.h
 * @brief Path — 跨平台文件路径操作类型。
 *
 * 设计原则：
 *   - 仅持有 UTF-8 路径字符串，无堆分配（SBO 优先）
 *   - 统一使用正斜杠 '/' 作为内部路径分隔符
 *   - Windows 下自动处理 '\\' → '/' 转换
 *   - 不依赖异常和 RTTI
 *
 * 典型用法：
 * @code
 *   Path p{"/home/user/docs/file.txt"};
 *   Path parent = p.parent_path();          // "/home/user/docs"
 *   StringView ext = p.extension();          // ".txt"
 *   Path combined = p / "subdir" / "data";   // "/home/user/docs/file.txt/subdir/data"
 * @endcode
 */

#pragma once

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <type_traits>

#include <mine/io/Api.h>
#include <mine/core/StringView.h>

namespace mine::io {

// ──────────────────────────────────────────────────────────────────────────────
// 内部常量
// ──────────────────────────────────────────────────────────────────────────────

namespace detail {
    /// 内部缓冲区用于小字符串优化（SBO），与 core::String 保持一致
    inline constexpr size_t kPathSBO = 31;  // 31 + null = 32 字节
} // namespace detail

// ──────────────────────────────────────────────────────────────────────────────
// Path
// ──────────────────────────────────────────────────────────────────────────────

/**
 * @brief 跨平台文件系统路径抽象。
 *
 * 内部始终维护规范化的路径字符串：
 *   - 分隔符统一为 '/'
 *   - 结尾无 '/'（除非是根路径 "/"）
 *   - 以 '/' 开头表示绝对路径（Windows 下也保留盘符后跟 '/'）
 *
 * 可拷贝、可移动，默认空路径表示当前目录 "."。
 */
class MINE_IO_API Path {
public:
    // ── 构造 ──────────────────────────────────────────────────────────────────

    /// 空路径（等同于 "."）
    Path() noexcept;

    /// 从 UTF-8 C 字符串构造
    /* implicit */ Path(const char* str) noexcept;

    /// 从 UTF-8 字符串视图构造
    /* implicit */ Path(mine::core::StringView sv) noexcept;

    /// 从长度限定的 UTF-8 字符串构造
    Path(const char* str, size_t len) noexcept;

    /// 拷贝/移动
    Path(const Path& other) noexcept;
    Path(Path&& other) noexcept;
    Path& operator=(const Path& other) noexcept;
    Path& operator=(Path&& other) noexcept;

    ~Path() noexcept;

    // ── 路径组件查询 ──────────────────────────────────────────────────────────

    /// 返回完整路径字符串视图
    [[nodiscard]] mine::core::StringView string() const noexcept;

    /// 返回 C 字符串（null 结尾）
    [[nodiscard]] const char* c_str() const noexcept;

    /// 路径是否为空
    [[nodiscard]] bool empty() const noexcept;

    /// 是否为绝对路径
    [[nodiscard]] bool is_absolute() const noexcept;

    /// 是否为相对路径
    [[nodiscard]] bool is_relative() const noexcept;

    /// 获取父目录路径
    [[nodiscard]] Path parent_path() const noexcept;

    /// 获取文件名（含扩展名）
    [[nodiscard]] mine::core::StringView filename() const noexcept;

    /// 获取扩展名（含点号，如 ".txt"）
    [[nodiscard]] mine::core::StringView extension() const noexcept;

    /// 获取文件名（不含扩展名）
    [[nodiscard]] mine::core::StringView stem() const noexcept;

    // ── 路径修改 ──────────────────────────────────────────────────────────────

    /// 替换扩展名（若原无扩展则追加）
    void replace_extension(mine::core::StringView new_ext) noexcept;

    /// 移除扩展名
    void remove_extension() noexcept;

    /// 替换文件名部分
    void replace_filename(mine::core::StringView new_name) noexcept;

    // ── 路径拼接 ──────────────────────────────────────────────────────────────

    /// 追加子路径，返回新 Path
    [[nodiscard]] Path join(mine::core::StringView sub) const noexcept;

    /// 追加子路径（运算符重载）
    [[nodiscard]] friend Path operator/(const Path& lhs, mine::core::StringView rhs) noexcept;

    // ── 比较 ──────────────────────────────────────────────────────────────────

    friend bool operator==(const Path& lhs, const Path& rhs) noexcept;
    friend bool operator!=(const Path& lhs, const Path& rhs) noexcept;
    friend bool operator<(const Path& lhs, const Path& rhs) noexcept;

    // ── 工具 ──────────────────────────────────────────────────────────────────

    /// 路径是否以指定后缀结尾
    [[nodiscard]] bool ends_with(mine::core::StringView suffix) const noexcept;

    /// 路径是否以指定前缀开头
    [[nodiscard]] bool starts_with(mine::core::StringView prefix) const noexcept;

    /// 计算相对路径（从 base 到达当前路径）
    [[nodiscard]] Path relative_to(const Path& base) const noexcept;

    /// 将路径中的分隔符规范化为 '/'
    [[nodiscard]] static Path normalize(mine::core::StringView raw) noexcept;

private:
    struct Impl;
    Impl* impl_;  // 实际存储采用 PIMPL 以保持 ABI 稳定
};

// ──────────────────────────────────────────────────────────────────────────────
// 运算符实现
// ──────────────────────────────────────────────────────────────────────────────

inline Path operator/(const Path& lhs, mine::core::StringView rhs) noexcept {
    return lhs.join(rhs);
}

inline bool operator==(const Path& lhs, const Path& rhs) noexcept {
    return lhs.string() == rhs.string();
}

inline bool operator!=(const Path& lhs, const Path& rhs) noexcept {
    return !(lhs == rhs);
}

inline bool operator<(const Path& lhs, const Path& rhs) noexcept {
    return std::strcmp(lhs.c_str(), rhs.c_str()) < 0;
}

} // namespace mine::io
