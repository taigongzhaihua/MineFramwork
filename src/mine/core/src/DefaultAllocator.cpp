/**
 * @file DefaultAllocator.cpp
 * @brief mine.core 模块的默认分配器实现与断言失败处理。
 *
 * 提供：
 *   1. MallocAllocator — 基于 malloc/free 的默认 IAllocator 实现
 *   2. default_allocator() / set_default_allocator() — 全局分配器访问入口
 *   3. assertion_failed() — 断言/检查失败时的诊断输出与终止逻辑
 *   4. IAllocator::realloc() — 默认实现（alloc + memcpy + dealloc）
 */

#include <mine/core/Allocator.h>
#include <mine/core/Assert.h>

// 使用 C 标准库（不引入 STL 容器）
#include <cstdlib>    // malloc / free / realloc
#include <cstdio>     // fprintf / stderr
#include <cstring>    // memcpy

// ──────────────────────────────────────────────────────────────────────────────
// 断言失败处理（实现 Assert.h 中声明的函数）
// ──────────────────────────────────────────────────────────────────────────────

namespace mine::core::detail {

[[noreturn]] void assertion_failed(
    const char* expr,
    const char* file,
    int         line,
    const char* msg) noexcept
{
    // 输出到 stderr，不使用 std::string 或 STL 格式化
    ::fprintf(stderr, "\n[MineFramework] 断言失败\n");
    ::fprintf(stderr, "  表达式 : %s\n", expr ? expr : "(null)");
    ::fprintf(stderr, "  位置   : %s:%d\n", file ? file : "(unknown)", line);
    if (msg && msg[0] != '\0') {
        ::fprintf(stderr, "  说明   : %s\n", msg);
    }
    ::fprintf(stderr, "\n");
    ::fflush(stderr);

    // 平台调试陷阱（让调试器捕获断点，而非直接 abort）
    MINE_DEBUG_BREAK();

    // 若调试器未介入，强制终止进程
    ::abort();
}

} // namespace mine::core::detail

// ──────────────────────────────────────────────────────────────────────────────
// IAllocator::realloc 默认实现
// ──────────────────────────────────────────────────────────────────────────────

namespace mine::core {

void* IAllocator::realloc(
    void*  ptr,
    size_t old_size,
    size_t new_size,
    size_t align) noexcept
{
    // 默认实现：分配新块 → 拷贝旧数据 → 释放旧块
    // 子类（如基于内存池的分配器）可覆盖此方法以提供就地 realloc 能力
    if (new_size == 0) {
        dealloc(ptr, old_size, align);
        return nullptr;
    }
    void* new_ptr = alloc(new_size, align);
    if (!new_ptr) return nullptr;

    if (ptr && old_size > 0) {
        size_t copy_size = old_size < new_size ? old_size : new_size;
        ::memcpy(new_ptr, ptr, copy_size);
        dealloc(ptr, old_size, align);
    }
    return new_ptr;
}

// ──────────────────────────────────────────────────────────────────────────────
// MallocAllocator：基于 malloc/free 的默认实现
// ──────────────────────────────────────────────────────────────────────────────

namespace {

/**
 * @brief 基于 C 标准库 malloc/free 的分配器实现。
 *
 * 对齐要求超过 alignof(max_align_t)（通常为 16 字节）时使用
 * 平台对齐分配函数（_aligned_malloc / aligned_alloc）。
 */
class MallocAllocator final : public IAllocator {
public:
    [[nodiscard]] void* alloc(size_t bytes, size_t align) noexcept override {
        if (bytes == 0) return nullptr;

        // 标准 malloc 保证对齐到 alignof(max_align_t)（通常 8 或 16 字节）
        if (align <= alignof(max_align_t)) {
            return ::malloc(bytes);
        }

        // 超额对齐：使用平台特定 API
#if defined(_WIN32)
        return ::_aligned_malloc(bytes, align);
#elif defined(__APPLE__) || defined(__linux__)
        void* ptr = nullptr;
        if (::posix_memalign(&ptr, align, bytes) != 0) return nullptr;
        return ptr;
#else
        // 回退：手动对齐分配（浪费最多 align-1 字节）
        size_t total = bytes + align - 1 + sizeof(void*);
        void* raw = ::malloc(total);
        if (!raw) return nullptr;
        // 对齐到 align，在对齐地址前存储原始指针
        uintptr_t addr = reinterpret_cast<uintptr_t>(raw) + sizeof(void*);
        uintptr_t aligned_addr = (addr + align - 1) & ~(uintptr_t)(align - 1);
        void** slot = reinterpret_cast<void**>(aligned_addr) - 1;
        *slot = raw;
        return reinterpret_cast<void*>(aligned_addr);
#endif
    }

    void dealloc(void* ptr, size_t /*bytes*/, size_t align) noexcept override {
        if (!ptr) return;

        if (align <= alignof(max_align_t)) {
            ::free(ptr);
            return;
        }

#if defined(_WIN32)
        ::_aligned_free(ptr);
#elif defined(__APPLE__) || defined(__linux__)
        ::free(ptr);
#else
        // 回退：通过存储的原始指针释放
        void** slot = reinterpret_cast<void**>(ptr) - 1;
        ::free(*slot);
#endif
    }

    [[nodiscard]] void* realloc(
        void*  ptr,
        size_t old_size,
        size_t new_size,
        size_t align) noexcept override
    {
        if (align <= alignof(max_align_t)) {
            // 标准对齐：可直接使用系统 realloc
            if (new_size == 0) {
                if (ptr) ::free(ptr);
                return nullptr;
            }
            return ::realloc(ptr, new_size);
        }
        // 超额对齐：退回到基类的 alloc+memcpy+dealloc 实现
        return IAllocator::realloc(ptr, old_size, new_size, align);
    }
};

} // anonymous namespace

// ──────────────────────────────────────────────────────────────────────────────
// 全局默认分配器访问
// ──────────────────────────────────────────────────────────────────────────────

/// 进程级默认分配器指针，初始指向内嵌的 MallocAllocator 单例
static MallocAllocator g_malloc_allocator;
static IAllocator*     g_default_allocator = &g_malloc_allocator;

IAllocator* default_allocator() noexcept {
    return g_default_allocator;
}

void set_default_allocator(IAllocator* allocator) noexcept {
    MINE_CHECK_MSG(allocator != nullptr, "set_default_allocator：不允许传入 nullptr");
    g_default_allocator = allocator;
}

} // namespace mine::core
