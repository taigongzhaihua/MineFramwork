/**
 * @file Assert.h
 * @brief 断言与不变式检查宏。
 *
 * 提供以下宏：
 *   - MINE_ASSERT / MINE_ASSERT_MSG  — 仅在 Debug 构建中检查（NDEBUG 关闭后为空操作）
 *   - MINE_CHECK  / MINE_CHECK_MSG  — 始终有效的合约检查（Release 同样触发）
 *   - MINE_UNREACHABLE               — 标记不可达路径
 *   - MINE_TODO_NOT_IMPLEMENTED      — 标记尚未实现的功能入口
 *   - MINE_ASSUME                    — 向优化器提供假设（违反时行为未定义）
 *
 * 所有宏在触发时调用 mine::core::detail::assertion_failed，不使用 C++ 异常。
 */

#pragma once

#include <cstddef>

// ──────────────────────────────────────────────────────────────────────────────
// 平台调试陷阱
// ──────────────────────────────────────────────────────────────────────────────

#if defined(_MSC_VER)
#    define MINE_DEBUG_BREAK() __debugbreak()
#elif defined(__clang__) || defined(__GNUC__)
#    define MINE_DEBUG_BREAK() __builtin_trap()
#else
#    include <cstdlib>
#    define MINE_DEBUG_BREAK() ::abort()
#endif

// ──────────────────────────────────────────────────────────────────────────────
// 内部实现：断言失败处理函数（在 DefaultAllocator.cpp 中实现）
// ──────────────────────────────────────────────────────────────────────────────

namespace mine::core::detail {

/**
 * @brief 断言失败时调用，输出诊断信息并终止程序。
 *
 * @param expr 失败的表达式字符串（由宏自动填入）
 * @param file 源文件路径（__FILE__）
 * @param line 行号（__LINE__）
 * @param msg  可选的附加说明（nullptr 时忽略）
 */
[[noreturn]] void assertion_failed(
    const char* expr,
    const char* file,
    int         line,
    const char* msg) noexcept;

} // namespace mine::core::detail

// ──────────────────────────────────────────────────────────────────────────────
// MINE_ASSERT：仅在 Debug 模式下有效
// ──────────────────────────────────────────────────────────────────────────────

#if !defined(NDEBUG)

#    define MINE_ASSERT(cond)                                               \
        do {                                                                \
            if (!(cond)) {                                                  \
                ::mine::core::detail::assertion_failed(                     \
                    #cond, __FILE__, __LINE__, nullptr);                     \
            }                                                               \
        } while (false)

#    define MINE_ASSERT_MSG(cond, msg)                                      \
        do {                                                                \
            if (!(cond)) {                                                  \
                ::mine::core::detail::assertion_failed(                     \
                    #cond, __FILE__, __LINE__, msg);                        \
            }                                                               \
        } while (false)

#else // NDEBUG

/** Debug 构建关闭时，MINE_ASSERT 展开为对条件的 sizeof 求值（防止未使用警告），不产生运行时代码。 */
#    define MINE_ASSERT(cond)          do { (void)sizeof(cond); } while (false)
#    define MINE_ASSERT_MSG(cond, msg) do { (void)sizeof(cond); (void)sizeof(msg); } while (false)

#endif // NDEBUG

// ──────────────────────────────────────────────────────────────────────────────
// MINE_CHECK：Release/Debug 均有效的合约检查
// ──────────────────────────────────────────────────────────────────────────────

/** 始终有效的前置/后置条件检查，触发时终止程序。 */
#define MINE_CHECK(cond)                                                    \
    do {                                                                    \
        if (!(cond)) {                                                      \
            ::mine::core::detail::assertion_failed(                         \
                #cond, __FILE__, __LINE__, nullptr);                        \
        }                                                                   \
    } while (false)

#define MINE_CHECK_MSG(cond, msg)                                           \
    do {                                                                    \
        if (!(cond)) {                                                      \
            ::mine::core::detail::assertion_failed(                         \
                #cond, __FILE__, __LINE__, msg);                            \
        }                                                                   \
    } while (false)

// ──────────────────────────────────────────────────────────────────────────────
// MINE_UNREACHABLE：标记不可达路径
// ──────────────────────────────────────────────────────────────────────────────

/**
 * 标记当前代码路径不可达。到达时终止程序并打印诊断信息；
 * 在发布构建中同时向编译器提供 unreachable 提示以优化代码。
 */
#define MINE_UNREACHABLE()                                                  \
    do {                                                                    \
        ::mine::core::detail::assertion_failed(                             \
            "UNREACHABLE", __FILE__, __LINE__, nullptr);                    \
    } while (false)

// ──────────────────────────────────────────────────────────────────────────────
// MINE_TODO_NOT_IMPLEMENTED：标记未实现入口
// ──────────────────────────────────────────────────────────────────────────────

/**
 * 在函数体内标记"此功能尚未实现"，调用时立即终止程序。
 * 禁止用空函数体静默通过——必须在实现前显式注册为 issue。
 */
#define MINE_TODO_NOT_IMPLEMENTED()                                         \
    do {                                                                    \
        ::mine::core::detail::assertion_failed(                             \
            "NOT_IMPLEMENTED", __FILE__, __LINE__, nullptr);                \
    } while (false)

// ──────────────────────────────────────────────────────────────────────────────
// MINE_ASSUME：优化器提示（违反时行为未定义）
// ──────────────────────────────────────────────────────────────────────────────

/**
 * 向优化器断言条件成立，允许更激进的代码优化。
 * 注意：条件为假时行为未定义，仅在确有把握时使用。
 */
#if defined(_MSC_VER)
#    define MINE_ASSUME(cond) __assume(cond)
#elif defined(__clang__)
#    define MINE_ASSUME(cond) __builtin_assume(cond)
#elif defined(__GNUC__)
#    define MINE_ASSUME(cond)                                               \
        do {                                                                \
            if (!(cond)) __builtin_unreachable();                           \
        } while (false)
#else
#    define MINE_ASSUME(cond) ((void)0)
#endif
