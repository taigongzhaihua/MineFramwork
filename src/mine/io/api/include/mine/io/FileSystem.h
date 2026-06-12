/**
 * @file FileSystem.h
 * @brief FileSystem — 文件系统操作：目录遍历、文件属性、创建/删除。
 *
 * 设计原则：
 *   - 无状态函数集合（命名空间级别操作）
 *   - 所有操作返回 Result<T> 或 Status
 *   - 不依赖异常和 RTTI
 *   - 递归操作有防护（最多深度限制）
 *
 * 典型用法：
 * @code
 *   auto entries = FileSystem::list_dir("/home/user").value();
 *   for (auto& e : entries) {
 *       if (e.is_directory()) { ... }
 *   }
 *
 *   if (FileSystem::exists("config.json").value()) { ... }
 * @endcode
 */

#pragma once

#include <cstddef>
#include <cstdint>
#include <type_traits>

#include <mine/io/Api.h>
#include <mine/io/Path.h>
#include <mine/core/Result.h>
#include <mine/core/Status.h>
#include <mine/core/Span.h>

namespace mine::io {

// ──────────────────────────────────────────────────────────────────────────────
// 文件类型
// ──────────────────────────────────────────────────────────────────────────────

/// 文件系统条目类型
enum class FileType : uint8_t {
    Unknown     = 0,  ///< 未知或无法确定
    Regular     = 1,  ///< 普通文件
    Directory   = 2,  ///< 目录
    Symlink     = 3,  ///< 符号链接
    Junction    = 4,  ///< Windows 交接点（类似目录软链接）
    BlockDevice = 5,  ///< 块设备（Unix）
    CharDevice  = 6,  ///< 字符设备（Unix）
    Pipe        = 7,  ///< 命名管道
    Socket      = 8,  ///< Unix 域套接字
};

// ──────────────────────────────────────────────────────────────────────────────
// 时间戳
// ──────────────────────────────────────────────────────────────────────────────

/**
 * @brief 文件系统时间戳，以自 Unix 纪元以来的秒数表示（UTC）。
 *
 * 精度为秒级，与 POSIX stat 结构中的 time_t 一致。
 */
struct FileTimestamp {
    int64_t seconds = 0;  ///< 自 1970-01-01 00:00:00 UTC 以来的秒数
};

// ──────────────────────────────────────────────────────────────────────────────
// 文件/目录条目信息
// ──────────────────────────────────────────────────────────────────────────────

/**
 * @brief 文件系统条目的元数据。
 *
 * 调用 stat() 后返回，包含文件类型、大小、时间戳等。
 */
class MINE_IO_API FileEntry {
public:
    FileEntry() noexcept;
    ~FileEntry() noexcept;

    FileEntry(const FileEntry&) noexcept;
    FileEntry& operator=(const FileEntry&) noexcept;
    FileEntry(FileEntry&&) noexcept;
    FileEntry& operator=(FileEntry&&) noexcept;

    /// 条目路径
    [[nodiscard]] const Path& path() const noexcept;

    /// 文件类型
    [[nodiscard]] FileType type() const noexcept;

    /// 是否为普通文件
    [[nodiscard]] bool is_regular() const noexcept;

    /// 是否为目录
    [[nodiscard]] bool is_directory() const noexcept;

    /// 是否为符号链接/交接点
    [[nodiscard]] bool is_symlink() const noexcept;

    /// 文件大小（字节），目录通常为 0
    [[nodiscard]] uint64_t file_size() const noexcept;

    /// 创建时间
    [[nodiscard]] FileTimestamp creation_time() const noexcept;

    /// 最后访问时间
    [[nodiscard]] FileTimestamp last_access_time() const noexcept;

    /// 最后修改时间
    [[nodiscard]] FileTimestamp last_write_time() const noexcept;

    /// 是否为隐藏文件（Windows 属性 / Unix '.' 前缀）
    [[nodiscard]] bool is_hidden() const noexcept;

    /// 是否为只读
    [[nodiscard]] bool is_read_only() const noexcept;

    struct Impl;
    Impl* impl_;  // PIMPL 保持 ABI 稳定（实现文件可见）
};

// ──────────────────────────────────────────────────────────────────────────────
// 目录遍历选项
// ──────────────────────────────────────────────────────────────────────────────

/// 目录遍历迭代器（一次性获取全部条目）
class MINE_IO_API DirectoryIterator {
public:
    DirectoryIterator() noexcept;
    ~DirectoryIterator() noexcept;

    DirectoryIterator(DirectoryIterator&&) noexcept;
    DirectoryIterator& operator=(DirectoryIterator&&) noexcept;

    /// 禁止拷贝
    DirectoryIterator(const DirectoryIterator&)            = delete;
    DirectoryIterator& operator=(const DirectoryIterator&) = delete;

    /// 遍历完成？
    [[nodiscard]] bool done() const noexcept;

    /// 获取当前条目
    [[nodiscard]] const FileEntry& entry() const noexcept;

    /// 前进到下一个条目
    mine::core::Status next() noexcept;

    struct Impl;
    Impl* impl_;
};

// ──────────────────────────────────────────────────────────────────────────────
// FileSystem 操作
// ──────────────────────────────────────────────────────────────────────────────

/**
 * @brief 文件系统系统操作命名空间。
 *
 * 所有操作为无状态函数，返回 Result<T> 或 Status。
 */
namespace FileSystem {

// ── 查询 ──────────────────────────────────────────────────────────────────────

/// 检查路径是否存在（文件或目录）
[[nodiscard]] MINE_IO_API mine::core::Result<bool>
exists(const Path& path) noexcept;

/// 获取文件条目信息（stat）
[[nodiscard]] MINE_IO_API mine::core::Result<FileEntry>
stat(const Path& path) noexcept;

/// 检查路径是否为目录
[[nodiscard]] MINE_IO_API mine::core::Result<bool>
is_directory(const Path& path) noexcept;

/// 检查路径是否为普通文件
[[nodiscard]] MINE_IO_API mine::core::Result<bool>
is_regular_file(const Path& path) noexcept;

/// 获取文件大小（字节）
[[nodiscard]] MINE_IO_API mine::core::Result<uint64_t>
file_size(const Path& path) noexcept;

// ── 目录操作 ──────────────────────────────────────────────────────────────────

/// 列出目录下的所有条目
[[nodiscard]] MINE_IO_API mine::core::Result<DirectoryIterator>
list_dir(const Path& path) noexcept;

/// 创建目录（不创建父目录）
[[nodiscard]] MINE_IO_API mine::core::Status
create_dir(const Path& path) noexcept;

/// 创建目录及所有必需的父目录
[[nodiscard]] MINE_IO_API mine::core::Status
create_dir_all(const Path& path) noexcept;

/// 删除空目录
[[nodiscard]] MINE_IO_API mine::core::Status
remove_dir(const Path& path) noexcept;

/// 递归删除目录及其所有内容
[[nodiscard]] MINE_IO_API mine::core::Status
remove_dir_all(const Path& path) noexcept;

// ── 文件操作 ──────────────────────────────────────────────────────────────────

/// 删除文件
[[nodiscard]] MINE_IO_API mine::core::Status
remove_file(const Path& path) noexcept;

/// 重命名/移动文件或目录
[[nodiscard]] MINE_IO_API mine::core::Status
rename(const Path& from, const Path& to) noexcept;

/// 复制文件
[[nodiscard]] MINE_IO_API mine::core::Status
copy_file(const Path& from, const Path& to) noexcept;

/// 复制文件（覆盖模式）
[[nodiscard]] MINE_IO_API mine::core::Status
copy_file_overwrite(const Path& from, const Path& to) noexcept;

// ── 路径工具 ──────────────────────────────────────────────────────────────────

/// 获取当前工作目录
[[nodiscard]] MINE_IO_API mine::core::Result<Path>
current_dir() noexcept;

/// 设置当前工作目录
[[nodiscard]] MINE_IO_API mine::core::Status
set_current_dir(const Path& path) noexcept;

/// 获取临时目录路径
[[nodiscard]] MINE_IO_API mine::core::Result<Path>
temp_dir() noexcept;

/// 获取用户主目录路径
[[nodiscard]] MINE_IO_API mine::core::Result<Path>
home_dir() noexcept;

/// 获取可执行文件所在目录
[[nodiscard]] MINE_IO_API mine::core::Result<Path>
exe_dir() noexcept;

} // namespace FileSystem

} // namespace mine::io
