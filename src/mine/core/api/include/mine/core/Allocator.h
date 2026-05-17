/**
 * @file Allocator.h
 * @brief 分配器接口与全局默认分配器访问入口。
 *
 * 所有模块的堆内存分配应通过 IAllocator 接口进行，而不是直接调用 malloc/free，
 * 这使得测试时可替换为追踪分配器，生产时可替换为内存池分配器。
 *
 * 分配失败策略：
 *   分配失败时调用 MINE_CHECK 断言终止程序，不抛出 std::bad_alloc。
 *   （核心模块禁用 C++ 异常）
 */

#pragma once

#include <cstddef>
#include <mine/core/Api.h>
#include <mine/core/Assert.h>

namespace mine::core {

// ──────────────────────────────────────────────────────────────────────────────
// IAllocator：分配器接口
// ──────────────────────────────────────────────────────────────────────────────

/**
 * @brief 堆内存分配器抽象接口。
 *
 * 实现约定：
 *   - alloc() 分配失败时返回 nullptr（调用方通过 MINE_CHECK 检查）
 *   - dealloc() 对 nullptr 指针调用是安全的（同 free()）
 *   - 所有分配的内存至少对齐到 alignof(max_align_t)（16 字节）
 *   - 分配器实例本身的生命周期由框架管理，调用方不得持有裸指针
 */
class MINE_API IAllocator {
public:
    virtual ~IAllocator() noexcept = default;

    /**
     * @brief 分配至少 bytes 字节的内存块，对齐到 align。
     * @param bytes 请求分配的字节数（0 时行为由实现定义，通常返回 nullptr）
     * @param align 对齐要求（字节），必须为 2 的幂，默认为 alignof(max_align_t)
     * @return 指向已分配内存的指针，失败时返回 nullptr
     */
    [[nodiscard]] virtual void* alloc(
        size_t bytes,
        size_t align = alignof(max_align_t)) noexcept = 0;

    /**
     * @brief 释放由本分配器分配的内存块。
     * @param ptr   要释放的指针（nullptr 时为空操作）
     * @param bytes 当初分配时的字节数（部分实现需要此信息）
     * @param align 当初分配时的对齐值
     */
    virtual void dealloc(
        void*  ptr,
        size_t bytes,
        size_t align = alignof(max_align_t)) noexcept = 0;

    /**
     * @brief 重新分配内存块（可选实现，默认为 alloc + memcpy + dealloc）。
     * @param ptr      当前内存块指针
     * @param old_size 当前大小（字节）
     * @param new_size 期望新大小（字节）
     * @param align    对齐要求
     * @return 新内存块指针，失败时返回 nullptr（原块保持有效）
     */
    [[nodiscard]] virtual void* realloc(
        void*  ptr,
        size_t old_size,
        size_t new_size,
        size_t align = alignof(max_align_t)) noexcept;
};

// ──────────────────────────────────────────────────────────────────────────────
// 全局默认分配器
// ──────────────────────────────────────────────────────────────────────────────

/**
 * @brief 获取进程级默认分配器（基于 malloc/free）。
 *
 * 返回指针在进程生命周期内始终有效。
 * 线程安全：仅设置一次，之后只读访问。
 */
[[nodiscard]] MINE_API IAllocator* default_allocator() noexcept;

/**
 * @brief 替换默认分配器（需在任何分配发生前调用）。
 *
 * @warning 必须在多线程启动之前、任何 mine:: 对象创建之前调用。
 *          不检查是否有已分配内存泄漏，调用方负责正确生命周期管理。
 */
MINE_API void set_default_allocator(IAllocator* allocator) noexcept;

} // namespace mine::core
