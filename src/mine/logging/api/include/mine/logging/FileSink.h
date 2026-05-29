/**
 * @file FileSink.h
 * @brief 文件日志 sink：将日志追加写入指定文件。
 *
 * 使用方式：
 * @code
 *   mine::logging::FileSinkOptions opts;
 *   opts.path       = "app.log";
 *   opts.append     = true;   // 追加模式（默认）
 *   opts.auto_flush = true;   // 每条日志后立即 fflush
 *   opts.max_bytes  = 10 * 1024 * 1024;  // 10 MB 后停止写入
 *
 *   uint32_t token = mine::logging::add_file_sink(opts);
 *   // ...
 *   mine::diag::Logger::global().remove_sink(token);
 * @endcode
 *
 * 输出格式：
 *   [YYYY-MM-DD HH:MM:SS] [LEVEL] [category] message\n
 *   category 为空时省略 [category] 字段（不含颜色转义码）。
 *
 * 大小限制说明：
 *   当累计写入字节数达到 max_bytes 后，新日志静默丢弃（不滚动文件）。
 *   若需日志滚动，请在外部调用 Logger::remove_sink() + add_file_sink()
 *   切换到新文件（调用方负责文件重命名/归档）。
 *   max_bytes == 0 表示无限制（默认）。
 */

#pragma once

#include <cstdint>
#include <mine/logging/Api.h>
#include <mine/diag/Logger.h>

namespace mine::logging {

// ─────────────────────────────────────────────────────────────────────────────
// FileSinkOptions：文件 sink 配置项
// ─────────────────────────────────────────────────────────────────────────────

/**
 * @brief 文件 sink 构造选项。
 *
 * path 不为 nullptr 且非空时才能成功创建 sink。
 */
struct FileSinkOptions {
    /// 日志文件路径（UTF-8 null 终止字符串，不可为 nullptr）
    const char* path      = nullptr;

    /// 最低接受级别（默认 Trace）
    mine::diag::LogLevel min_level   = mine::diag::LogLevel::Trace;

    /// 追加模式：true = 追加（默认），false = 覆盖（截断已有内容）
    bool        append     = true;

    /// 自动刷新：true = 每条日志写入后立即 fflush（默认 true，确保日志不丢失）
    bool        auto_flush = true;

    /// 最大写入字节数（0 表示无限制；超出后新记录静默丢弃）
    uint64_t    max_bytes  = 0u;
};

// ─────────────────────────────────────────────────────────────────────────────
// 工厂函数
// ─────────────────────────────────────────────────────────────────────────────

/**
 * @brief 创建文件 sink 并注册到全局 Logger，返回注销 token。
 *
 * token == 0 表示注册失败（path 无效、无法打开文件、sink 列表已满）。
 */
[[nodiscard]] MINE_LOGGING_API uint32_t add_file_sink(
    const FileSinkOptions& opts) noexcept;

/**
 * @brief 创建文件 LogSink 描述符（不注册到全局 Logger）。
 *
 * 失败时返回 write_fn == nullptr 的 LogSink（ctx 为 nullptr，无需 destroy）。
 * 成功时返回的 LogSink.ctx 由调用方管理（需在不使用后调用 destroy_fn(ctx)）。
 */
[[nodiscard]] MINE_LOGGING_API mine::diag::LogSink make_file_sink(
    const FileSinkOptions& opts) noexcept;

} // namespace mine::logging
