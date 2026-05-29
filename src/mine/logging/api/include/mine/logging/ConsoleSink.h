/**
 * @file ConsoleSink.h
 * @brief 控制台日志 sink：将日志彩色输出到 stdout（Info 及以下）/ stderr（Warn 及以上）。
 *
 * 使用方式：
 * @code
 *   // 创建控制台 sink 并注册到全局 Logger
 *   uint32_t token = mine::logging::add_console_sink();
 *
 *   // 写日志（通过 mine.diag 宏）
 *   MINE_LOG_INFO("应用已启动");
 *   MINE_LOG_ERROR("发生错误");
 *
 *   // 注销 sink（可选，程序退出时自动销毁）
 *   mine::diag::Logger::global().remove_sink(token);
 * @endcode
 *
 * 颜色支持：
 *   在支持 ANSI 转义码的终端下，各级别以不同颜色输出：
 *   Trace=灰色  Debug=青色  Info=绿色  Warn=黄色  Error=红色  Fatal=洋红
 *   可通过 use_color = false 禁用颜色（适合重定向到文件时使用）。
 *
 * 输出格式：
 *   [YYYY-MM-DD HH:MM:SS] [LEVEL] [category] message
 *   category 为空时省略 [category] 字段。
 */

#pragma once

#include <cstdint>
#include <mine/logging/Api.h>
#include <mine/diag/Logger.h>

namespace mine::logging {

// ─────────────────────────────────────────────────────────────────────────────
// ConsoleSinkOptions：控制台 sink 配置项
// ─────────────────────────────────────────────────────────────────────────────

/**
 * @brief 控制台 sink 构造选项。
 */
struct ConsoleSinkOptions {
    /// 最低接受级别（默认 Trace，由 Logger 全局级别控制实际过滤）
    mine::diag::LogLevel min_level  = mine::diag::LogLevel::Trace;

    /// 是否启用 ANSI 颜色输出（默认 true）
    bool                 use_color  = true;

    /// Warn 及以上级别是否输出到 stderr（默认 true；false 则全部输出到 stdout）
    bool                 use_stderr_for_warn = true;
};

// ─────────────────────────────────────────────────────────────────────────────
// 工厂函数
// ─────────────────────────────────────────────────────────────────────────────

/**
 * @brief 创建一个控制台 sink 并注册到全局 Logger，返回注销 token。
 *
 * token == 0 表示注册失败（sink 列表已满或内存不足）。
 * 注销方式：`mine::diag::Logger::global().remove_sink(token)`
 */
[[nodiscard]] MINE_LOGGING_API uint32_t add_console_sink(
    const ConsoleSinkOptions& opts = ConsoleSinkOptions{}) noexcept;

/**
 * @brief 创建控制台 LogSink 描述符（不注册）。
 *
 * 适合手动注册到非全局 Logger，或在测试中检查 sink 行为。
 * 返回的 LogSink.ctx 由调用方管理生命周期（需在不使用后调用 destroy_fn(ctx)）。
 */
[[nodiscard]] MINE_LOGGING_API mine::diag::LogSink make_console_sink(
    const ConsoleSinkOptions& opts = ConsoleSinkOptions{}) noexcept;

} // namespace mine::logging
