/**
 * @file PipeStream.h
 * @brief PipeStream — 顺序流式读写抽象。
 *
 * 提供基于缓冲区的流式 I/O 抽象，可用于文件、管道、网络等顺序数据源。
 * 与 File 相比：PipeStream 更强调顺序访问和缓冲语义。
 *
 * 设计原则：
 *   - 移动语义管理底层资源
 *   - 缓冲 I/O：内部维护读写缓冲区
 *   - 不依赖异常和 RTTI
 *
 * 典型用法：
 * @code
 *   auto stream = PipeStream::from_file("data.txt").value();
 *   char buf[256];
 *   while (auto n = stream.read_some(buf).value()) {
 *       process(buf, n);
 *   }
 *   stream.close();
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
// 流访问模式
// ──────────────────────────────────────────────────────────────────────────────

/// 流方向
enum class StreamAccess : uint8_t {
    Read   = 1,  ///< 只读
    Write  = 2,  ///< 只写
    Both   = 3,  ///< 读写
};

// ──────────────────────────────────────────────────────────────────────────────
// PipeStream
// ──────────────────────────────────────────────────────────────────────────────

/**
 * @brief 缓冲顺序流抽象。
 *
 * 内部维护读写缓冲区，减少系统调用次数。
 * 可通过 File 句柄、Path 或自定义数据源构造。
 */
class MINE_IO_API PipeStream {
public:
    /// 默认缓冲区大小（8 KB）
    static constexpr size_t kDefaultBufferSize = 8192;

    // ── 构造 ──────────────────────────────────────────────────────────────────

    /// 空流
    PipeStream() noexcept;

    /// 移动语义
    PipeStream(PipeStream&& other) noexcept;
    PipeStream& operator=(PipeStream&& other) noexcept;

    /// 禁止拷贝
    PipeStream(const PipeStream&)            = delete;
    PipeStream& operator=(const PipeStream&) = delete;

    /// 析构时自动关闭
    ~PipeStream() noexcept;

    // ── 工厂方法 ──────────────────────────────────────────────────────────────

    /**
     * @brief 以只读方式打开文件流。
     * @param path 文件路径
     * @param buffer_size 内部缓冲区大小，默认 8KB
     */
    [[nodiscard]] static mine::core::Result<PipeStream>
    from_file_read(const Path& path, size_t buffer_size = kDefaultBufferSize) noexcept;

    /**
     * @brief 以只写方式打开文件流。
     * @param path 文件路径
     * @param buffer_size 内部缓冲区大小
     */
    [[nodiscard]] static mine::core::Result<PipeStream>
    from_file_write(const Path& path, size_t buffer_size = kDefaultBufferSize) noexcept;

    /**
     * @brief 以追加方式打开文件流。
     * @param path 文件路径
     * @param buffer_size 内部缓冲区大小
     */
    [[nodiscard]] static mine::core::Result<PipeStream>
    from_file_append(const Path& path, size_t buffer_size = kDefaultBufferSize) noexcept;

    // ── 读取 ──────────────────────────────────────────────────────────────────

    /**
     * @brief 读取一些数据到缓冲区（尽量填满，但可短读）。
     * @param buf 目标缓冲区
     * @return Result<size_t> 成功返回读取字节数，0 表示 EOF
     */
    [[nodiscard]] mine::core::Result<size_t>
    read_some(mine::core::Span<char> buf) noexcept;

    /**
     * @brief 读取精确字节数到缓冲区（循环读取直到满足或出错/EOF）。
     * @param buf 目标缓冲区
     * @return Result<size_t> 成功返回 buf.size()，失败返回错误
     */
    [[nodiscard]] mine::core::Result<size_t>
    read_exact(mine::core::Span<char> buf) noexcept;

    /**
     * @brief 读取一行（直到遇到 '\n' 或缓冲区满或 EOF）。
     * @param buf 目标缓冲区
     * @param delimiter 行分隔符，默认为 '\n'
     * @return Result<size_t> 返回读取字节数（不含分隔符），0 表示 EOF
     */
    [[nodiscard]] mine::core::Result<size_t>
    read_line(mine::core::Span<char> buf, char delimiter = '\n') noexcept;

    // ── 写入 ──────────────────────────────────────────────────────────────────

    /**
     * @brief 写入一些数据（尽量写入，但可短写）。
     * @param buf 源缓冲区
     * @return Result<size_t> 成功返回写入字节数
     */
    [[nodiscard]] mine::core::Result<size_t>
    write_some(mine::core::Span<const char> buf) noexcept;

    /**
     * @brief 写入全部数据（循环写入直到全写完或出错）。
     * @param buf 源缓冲区
     */
    mine::core::Status write_all(mine::core::Span<const char> buf) noexcept;

    /**
     * @brief 写入一行并追加换行符 '\n'。
     * @param line 行内容（不含换行符）
     */
    mine::core::Status write_line(mine::core::Span<const char> line) noexcept;

    // ── 刷新 ──────────────────────────────────────────────────────────────────

    /// 强制将写缓冲区刷入底层
    mine::core::Status flush() noexcept;

    // ── 状态查询 ──────────────────────────────────────────────────────────────

    /// 流是否已打开
    [[nodiscard]] bool is_open() const noexcept;

    /// 是否已到达流末尾（仅读流有意义）
    [[nodiscard]] bool eof() const noexcept;

    /// 流中是否发生了错误
    [[nodiscard]] bool has_error() const noexcept;

    /// 获取最近一次操作的错误状态
    [[nodiscard]] mine::core::Status last_error() const noexcept;

    // ── 关闭 ──────────────────────────────────────────────────────────────────

    /// 显式关闭流
    mine::core::Status close() noexcept;

private:
    struct Impl;
    Impl* impl_;

    /// 内部构造函数（由工厂方法调用）
    explicit PipeStream(Impl* impl) noexcept;
};

} // namespace mine::io
