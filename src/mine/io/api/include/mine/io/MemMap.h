/**
 * @file MemMap.h
 * @brief MemMap — 内存映射文件。
 *
 * 将文件内容映射到虚拟地址空间，提供类似数组的随机访问。
 * 适用于大文件的只读访问或进程间共享内存。
 *
 * 设计原则：
 *   - RAII 管理映射生命周期
 *   - 仅移动，不可拷贝
 *   - 不依赖异常和 RTTI
 *
 * 典型用法：
 * @code
 *   auto mmap = MemMap::open_read("large_data.bin").value();
 *   auto data = mmap.span();  // 获取内存视图
 *   process(data);
 *   mmap.close();  // 或析构自动释放
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
// 映射模式
// ──────────────────────────────────────────────────────────────────────────────

/// 内存映射访问模式
enum class MemMapMode : uint32_t {
    Read        = 1 << 0,  ///< 只读映射
    Write       = 1 << 1,  ///< 读写映射
    ReadWrite   = Read | Write,  ///< 读写

    CopyOnWrite = 1 << 2,  ///< 写时复制（修改不写回文件）

    Exec        = 1 << 3,  ///< 允许执行（通常仅用于 JIT）

    Private     = CopyOnWrite,  ///< 私有映射（修改不影响原文件）

    /// 私有：将文件完全载入内存（可脱离原文件）
    PrivateCopy = Read | CopyOnWrite,
};

inline MemMapMode operator|(MemMapMode a, MemMapMode b) noexcept {
    return static_cast<MemMapMode>(static_cast<uint32_t>(a) | static_cast<uint32_t>(b));
}

// ──────────────────────────────────────────────────────────────────────────────
// MemMap
// ──────────────────────────────────────────────────────────────────────────────

/**
 * @brief 内存映射文件句柄。
 *
 * 将文件（或文件的一部分）映射到进程虚拟地址空间。
 * 析构时自动解除映射。
 */
class MINE_IO_API MemMap {
public:
    // ── 构造 ──────────────────────────────────────────────────────────────────

    /// 空映射
    MemMap() noexcept;

    /// 移动语义
    MemMap(MemMap&& other) noexcept;
    MemMap& operator=(MemMap&& other) noexcept;

    /// 禁止拷贝
    MemMap(const MemMap&)            = delete;
    MemMap& operator=(const MemMap&) = delete;

    /// 析构时自动解除映射
    ~MemMap() noexcept;

    // ── 打开 ──────────────────────────────────────────────────────────────────

    /**
     * @brief 以只读模式映射整个文件。
     * @param path 文件路径
     * @return Result<MemMap>
     */
    [[nodiscard]] static mine::core::Result<MemMap>
    open_read(const Path& path) noexcept;

    /**
     * @brief 以读写模式映射整个文件。
     * @param path 文件路径
     * @return Result<MemMap>
     */
    [[nodiscard]] static mine::core::Result<MemMap>
    open_read_write(const Path& path) noexcept;

    /**
     * @brief 以指定模式映射文件的指定范围。
     * @param path   文件路径
     * @param mode   映射模式
     * @param offset 文件偏移（字节）
     * @param length 映射长度（字节），0 表示直到文件末尾
     * @return Result<MemMap>
     */
    [[nodiscard]] static mine::core::Result<MemMap>
    open_with_options(const Path& path, MemMapMode mode,
                      uint64_t offset = 0, uint64_t length = 0) noexcept;

    // ── 查询 ──────────────────────────────────────────────────────────────────

    /// 映射是否有效
    [[nodiscard]] bool is_mapped() const noexcept;

    /// 映射数据指针
    [[nodiscard]] void* data() noexcept;
    [[nodiscard]] const void* data() const noexcept;

    /// 映射大小（字节）
    [[nodiscard]] uint64_t size() const noexcept;

    /// 以 Span<char> 视图访问映射区域
    [[nodiscard]] mine::core::Span<char> span() noexcept;
    [[nodiscard]] mine::core::Span<const char> span() const noexcept;

    /// 以 Span<uint8_t> 视图访问映射区域（字节操作）
    [[nodiscard]] mine::core::Span<uint8_t> byte_span() noexcept;
    [[nodiscard]] mine::core::Span<const uint8_t> byte_span() const noexcept;

    // ── 同步 ──────────────────────────────────────────────────────────────────

    /**
     * @brief 将映射中的修改同步（flush）到原文件。
     *
     * 仅对可写映射有效。同步可以是异步的（取决于操作系统）。
     *
     * @param offset 映射内偏移（0 = 从头开始）
     * @param length 刷新的字节数，0 = 直到末尾
     */
    mine::core::Status flush(uint64_t offset = 0, uint64_t length = 0) noexcept;

    /// 将映射中的修改同步到原文件并阻塞等待完成
    mine::core::Status flush_sync(uint64_t offset = 0, uint64_t length = 0) noexcept;

    // ── 关闭 ──────────────────────────────────────────────────────────────────

    /// 显式解除映射
    mine::core::Status close() noexcept;

private:
    struct Impl;
    Impl* impl_;

    /// 内部构造函数（由工厂方法调用）
    explicit MemMap(Impl* impl) noexcept;
};

} // namespace mine::io
