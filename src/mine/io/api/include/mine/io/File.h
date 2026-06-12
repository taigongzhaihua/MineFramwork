/**
 * @file File.h
 * @brief File — 跨平台文件读写操作。
 *
 * 设计原则：
 *   - 基于操作系统原生句柄，无 C stdio 间接层
 *   - 所有可能失败的操作返回 Result<T> 或 Status
 *   - 移动语义管理句柄生命周期（RAII）
 *   - 不依赖异常和 RTTI
 *   - 公共 API 全部 noexcept
 *
 * 典型用法：
 * @code
 *   auto file = File::open("data.txt", FileMode::Read).value();
 *   auto data = file.read_all().value();
 *   file.close();
 * @endcode
 */

#pragma once

#include <cstddef>
#include <cstdint>
#include <type_traits>
#include <utility>

#include <mine/io/Api.h>
#include <mine/io/Path.h>
#include <mine/core/Result.h>
#include <mine/core/Status.h>
#include <mine/core/Span.h>

namespace mine::io {

// ──────────────────────────────────────────────────────────────────────────────
// 文件打开模式
// ──────────────────────────────────────────────────────────────────────────────

/**
 * @brief 文件打开模式标志，可组合使用。
 */
enum class FileMode : uint32_t {
    Read        = 1 << 0,  ///< 只读
    Write       = 1 << 1,  ///< 只写
    ReadWrite   = Read | Write,  ///< 读写

    Append      = 1 << 2,  ///< 追加模式（写入总在末尾）

    Create      = 1 << 3,  ///< 不存在则创建
    Truncate    = 1 << 4,  ///< 打开时清空已有内容
    Exclusive   = 1 << 5,  ///< 若文件已存在则失败

    /// 常见组合
    DefaultRead  = Read,               ///< 默认读取
    DefaultWrite = Write | Create | Truncate,  ///< 默认写入（创建并覆盖）
};

/// 位运算支持
inline FileMode operator|(FileMode a, FileMode b) noexcept {
    return static_cast<FileMode>(static_cast<uint32_t>(a) | static_cast<uint32_t>(b));
}
inline FileMode operator&(FileMode a, FileMode b) noexcept {
    return static_cast<FileMode>(static_cast<uint32_t>(a) & static_cast<uint32_t>(b));
}
inline bool has_flag(FileMode mode, FileMode flag) noexcept {
    return (static_cast<uint32_t>(mode) & static_cast<uint32_t>(flag)) == static_cast<uint32_t>(flag);
}

// ──────────────────────────────────────────────────────────────────────────────
// 文件访问权限（POSIX 概念，Windows 下仅作提示）
// ──────────────────────────────────────────────────────────────────────────────

enum class FileAccess : uint32_t {
    OwnerRead   = 0400,
    OwnerWrite  = 0200,
    OwnerExec   = 0100,
    GroupRead   = 0040,
    GroupWrite  = 0020,
    GroupExec   = 0010,
    OtherRead   = 0004,
    OtherWrite  = 0002,
    OtherExec   = 0001,

    Default     = OwnerRead | OwnerWrite             ,  ///< 默认权限（用户读写）
    AllOwner    = OwnerRead | OwnerWrite | OwnerExec  ,  ///< 所有者全权限
    AllGroup    = GroupRead | GroupWrite | GroupExec  ,  ///< 组全权限
    AllOther    = OtherRead | OtherWrite | OtherExec  ,  ///< 其他全权限
    All         = AllOwner | AllGroup | AllOther       ,  ///< 所有人全权限
};

inline FileAccess operator|(FileAccess a, FileAccess b) noexcept {
    return static_cast<FileAccess>(static_cast<uint32_t>(a) | static_cast<uint32_t>(b));
}

// ──────────────────────────────────────────────────────────────────────────────
// 文件位置
// ──────────────────────────────────────────────────────────────────────────────

/// 文件内部位置标识
enum class FileSeekOrigin : int32_t {
    Begin   = 0,  ///< 从文件起始处
    Current = 1,  ///< 从当前位置
    End     = 2,  ///< 从文件末尾
};

// ──────────────────────────────────────────────────────────────────────────────
// 文件句柄
// ──────────────────────────────────────────────────────────────────────────────

/**
 * @brief 跨平台文件句柄封装（RAII 管理操作系统文件句柄）。
 *
 * 仅移动，不可拷贝。析构时自动关闭文件（如果未显式调用 close()）。
 */
class MINE_IO_API File {
public:
    // ── 构造与析构 ────────────────────────────────────────────────────────────

    /// 构造空文件句柄（未打开）
    File() noexcept;

    /// 移动语义
    File(File&& other) noexcept;
    File& operator=(File&& other) noexcept;

    /// 禁止拷贝
    File(const File&)            = delete;
    File& operator=(const File&) = delete;

    /// 析构时自动关闭文件
    ~File() noexcept;

    // ── 打开与关闭 ────────────────────────────────────────────────────────────

    /**
     * @brief 以指定模式打开文件。
     * @param path 文件路径
     * @param mode 打开模式（可组合标志）
     * @return Result<File> 成功返回 File 句柄，失败返回错误
     */
    [[nodiscard]] static mine::core::Result<File>
    open(const Path& path, FileMode mode = FileMode::DefaultRead) noexcept;

    /**
     * @brief 以指定模式与权限打开文件。
     * @param path 文件路径
     * @param mode 打开模式
     * @param access 访问权限（Windows 下效果有限）
     */
    [[nodiscard]] static mine::core::Result<File>
    open_with_access(const Path& path, FileMode mode, FileAccess access) noexcept;

    /// 检查文件是否已打开
    [[nodiscard]] bool is_open() const noexcept;

    /// 显式关闭文件，释放系统句柄
    mine::core::Status close() noexcept;

    // ── 读取 ──────────────────────────────────────────────────────────────────

    /**
     * @brief 从当前位置读取最多 max_bytes 到缓冲区。
     * @param buf 目标缓冲区
     * @param max_bytes 最大读取字节数
     * @return Result<size_t> 成功返回实际读取字节数，失败返回错误
     */
    [[nodiscard]] mine::core::Result<size_t>
    read(mine::core::Span<char> buf) noexcept;

    /**
     * @brief 读取文件全部内容为字节数组。
     * @return Result<Vector<uint8_t>> 成功返回文件内容，失败返回错误
     */
    [[nodiscard]] mine::core::Result<mine::core::Status>
    read_all(mine::core::Span<char> out_buffer) noexcept;

    /**
     * @brief 从文件当前位置读取指定字节数到堆分配缓冲区。
     * @param size 要读取的字节数
     * @return Result 成功返回包含数据的堆内存，失败返回错误
     */
    [[nodiscard]] mine::core::Result<size_t>
    read_exact(mine::core::Span<char> buf) noexcept;

    // ── 写入 ──────────────────────────────────────────────────────────────────

    /**
     * @brief 向当前位置写入缓冲区内容。
     * @param buf 源缓冲区
     * @return Result<size_t> 成功返回实际写入字节数，失败返回错误
     */
    [[nodiscard]] mine::core::Result<size_t>
    write(mine::core::Span<const char> buf) noexcept;

    /**
     * @brief 写入缓冲区全部内容（循环写入直到全部写入或出错）。
     * @param buf 源缓冲区
     * @return Status
     */
    mine::core::Status write_all(mine::core::Span<const char> buf) noexcept;

    /// 强制将缓冲数据刷入物理存储
    mine::core::Status flush() noexcept;

    // ── 定位 ──────────────────────────────────────────────────────────────────

    /**
     * @brief 移动文件内部位置指针。
     * @param offset 偏移量（字节）
     * @param origin 参考位置
     * @return Result<size_t> 成功返回新位置（从文件头计数），失败返回错误
     */
    [[nodiscard]] mine::core::Result<size_t>
    seek(int64_t offset, FileSeekOrigin origin = FileSeekOrigin::Begin) noexcept;

    /// 获取当前文件位置（从文件头计数）
    [[nodiscard]] mine::core::Result<size_t> tell() const noexcept;

    // ── 大小 ──────────────────────────────────────────────────────────────────

    /// 获取文件大小（字节），需要文件已打开
    [[nodiscard]] mine::core::Result<size_t> size() const noexcept;

    /// 设置文件大小（截断或扩展），需要文件以写模式打开
    mine::core::Status set_size(size_t new_size) noexcept;

    // ── 原始句柄 ──────────────────────────────────────────────────────────────

    /// 获取原生操作系统文件句柄（Windows: HANDLE; POSIX: int fd），仅在平台层使用
    [[nodiscard]] void* native_handle() const noexcept;

private:
    struct Impl;
    Impl* impl_;  // PIMPL 封装，隐藏平台相关句柄

    explicit File(Impl* impl) noexcept;
};

} // namespace mine::io
