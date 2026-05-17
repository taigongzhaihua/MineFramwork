/**
 * @file Status.h
 * @brief 操作状态类型：StatusCode 枚举与轻量 Status 包装。
 *
 * Status 不含堆分配，可以在寄存器中传递。
 * 标记 [[nodiscard]] 确保调用方不遗漏错误检查。
 */

#pragma once

#include <cstdint>
#include <mine/core/Assert.h>

namespace mine::core {

// ──────────────────────────────────────────────────────────────────────────────
// StatusCode：错误码枚举
// ──────────────────────────────────────────────────────────────────────────────

/**
 * @brief 通用错误码，0 表示成功，负值表示各类错误。
 *
 * 各模块可扩展自己的错误域，但 core 错误码在此统一定义。
 */
enum class StatusCode : int32_t {
    Ok            =  0,  ///< 操作成功
    Unknown       = -1,  ///< 未知错误
    InvalidArg    = -2,  ///< 参数非法
    OutOfRange    = -3,  ///< 索引或值超出范围
    OutOfMemory   = -4,  ///< 内存分配失败
    NotFound      = -5,  ///< 资源或键不存在
    AlreadyExists = -6,  ///< 资源已存在（重复创建）
    NotSupported  = -7,  ///< 功能在当前平台/配置下不支持
    Cancelled     = -8,  ///< 操作被主动取消
    Timeout       = -9,  ///< 操作超时
    IoError       = -10, ///< 文件或流 I/O 错误
    ParseError    = -11, ///< 解析/反序列化失败
    InvalidState  = -12, ///< 对象处于非法状态（状态机保护）
    PermissionDenied = -13, ///< 权限不足
};

// ──────────────────────────────────────────────────────────────────────────────
// Status：轻量操作状态
// ──────────────────────────────────────────────────────────────────────────────

/**
 * @brief 表示操作结果的轻量类型，仅携带 StatusCode。
 *
 * - 构造成功状态：`Status{}`  或  `Status::success()`
 * - 构造失败状态：`Status{StatusCode::InvalidArg}`
 * - 检查：`if (s.ok()) ...`  或  `if (!s) ...`
 *
 * 标记 [[nodiscard]]，遗漏检查时触发编译器警告。
 */
class [[nodiscard]] Status {
public:
    /// 默认构造为成功状态
    constexpr Status() noexcept = default;

    /// 以指定错误码构造
    constexpr explicit Status(StatusCode code) noexcept : code_{code} {}

    /// 返回一个成功的 Status（等价于默认构造）
    [[nodiscard]] static constexpr Status success() noexcept { return Status{}; }

    /// 以 StatusCode 快速构造失败状态（方便链式调用）
    [[nodiscard]] static constexpr Status from_code(StatusCode c) noexcept {
        return Status{c};
    }

    // ── 查询 ──────────────────────────────────────────────────────────────────

    /// 是否表示成功
    [[nodiscard]] constexpr bool ok() const noexcept {
        return code_ == StatusCode::Ok;
    }

    /// 原始错误码
    [[nodiscard]] constexpr StatusCode code() const noexcept { return code_; }

    // ── 转换 ──────────────────────────────────────────────────────────────────

    /// 显式布尔转换：成功为 true，失败为 false
    constexpr explicit operator bool() const noexcept { return ok(); }

    // ── 比较 ──────────────────────────────────────────────────────────────────

    constexpr bool operator==(Status other) const noexcept {
        return code_ == other.code_;
    }
    constexpr bool operator!=(Status other) const noexcept {
        return code_ != other.code_;
    }
    constexpr bool operator==(StatusCode c) const noexcept { return code_ == c; }
    constexpr bool operator!=(StatusCode c) const noexcept { return code_ != c; }

private:
    StatusCode code_{StatusCode::Ok};
};

// ──────────────────────────────────────────────────────────────────────────────
// MINE_TRY：在返回 Status 的函数中提前传播错误
// ──────────────────────────────────────────────────────────────────────────────

/**
 * @def MINE_TRY(expr)
 * @brief 对 expr（须返回 Status）求值，若失败则立即 return 该 Status。
 *
 * 仅适用于返回类型为 Status 的函数体内。
 *
 * 示例：
 * @code
 *   Status write_file(Path path, StringView data) {
 *       MINE_TRY(ensure_directory(path.parent()));
 *       MINE_TRY(file.open(path));
 *       ...
 *       return Status::success();
 *   }
 * @endcode
 */
#define MINE_TRY(expr)                              \
    do {                                            \
        auto _mine_status_ = (expr);                \
        if (!_mine_status_.ok()) {                  \
            return _mine_status_;                   \
        }                                           \
    } while (false)

} // namespace mine::core
