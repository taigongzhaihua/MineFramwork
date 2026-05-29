/**
 * @file ConsoleSink.cpp
 * @brief 控制台日志 sink 实现。
 *
 * 输出格式（带 ANSI 颜色）：
 *   \033[颜色m[YYYY-MM-DD HH:MM:SS] [LEVEL] [category] message\033[0m\n
 *
 * 不带颜色：
 *   [YYYY-MM-DD HH:MM:SS] [LEVEL] [category] message\n
 *
 * Info 及以下输出到 stdout；Warn 及以上输出到 stderr（可配置）。
 */

#include <mine/logging/ConsoleSink.h>
#include <mine/diag/Logger.h>

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>

#if defined(_MSC_VER)
#   pragma warning(disable: 4996)  // localtime 在 MSVC 下的 deprecated 警告
#endif

namespace mine::logging {

// ─────────────────────────────────────────────────────────────────────────────
// 内部：ConsoleSink 上下文
// ─────────────────────────────────────────────────────────────────────────────

struct ConsoleSinkCtx {
    bool use_color            = true;
    bool use_stderr_for_warn  = true;
};

// ─────────────────────────────────────────────────────────────────────────────
// 内部：ANSI 颜色码（各级别对应颜色）
// ─────────────────────────────────────────────────────────────────────────────

// 返回各级别对应的 ANSI 前缀转义码（非 null 终止之后需手动追加 "m"）
static const char* level_ansi_color(mine::diag::LogLevel level) noexcept {
    switch (level) {
        case mine::diag::LogLevel::Trace: return "\033[90m";   // 深灰
        case mine::diag::LogLevel::Debug: return "\033[36m";   // 青色
        case mine::diag::LogLevel::Info:  return "\033[32m";   // 绿色
        case mine::diag::LogLevel::Warn:  return "\033[33m";   // 黄色
        case mine::diag::LogLevel::Error: return "\033[31m";   // 红色
        case mine::diag::LogLevel::Fatal: return "\033[35m";   // 洋红
        default:                          return "\033[0m";
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// 内部：级别标签字符串
// ─────────────────────────────────────────────────────────────────────────────

static const char* level_tag(mine::diag::LogLevel level) noexcept {
    switch (level) {
        case mine::diag::LogLevel::Trace: return "TRACE";
        case mine::diag::LogLevel::Debug: return "DEBUG";
        case mine::diag::LogLevel::Info:  return "INFO ";
        case mine::diag::LogLevel::Warn:  return "WARN ";
        case mine::diag::LogLevel::Error: return "ERROR";
        case mine::diag::LogLevel::Fatal: return "FATAL";
        default:                          return "?????";
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// 内部：格式化时间戳
// ─────────────────────────────────────────────────────────────────────────────

// 将 time_t 格式化为 "YYYY-MM-DD HH:MM:SS"（写入 buf，长度至少 20 字节）
static void format_timestamp(time_t t, char* buf, size_t buf_size) noexcept {
#if defined(_MSC_VER)
    struct tm tm_val{};
    (void)localtime_s(&tm_val, &t);
    (void)::snprintf(buf, buf_size, "%04d-%02d-%02d %02d:%02d:%02d",
        tm_val.tm_year + 1900, tm_val.tm_mon + 1, tm_val.tm_mday,
        tm_val.tm_hour, tm_val.tm_min, tm_val.tm_sec);
#else
    struct tm tm_val{};
    (void)localtime_r(&t, &tm_val);
    (void)::snprintf(buf, buf_size, "%04d-%02d-%02d %02d:%02d:%02d",
        tm_val.tm_year + 1900, tm_val.tm_mon + 1, tm_val.tm_mday,
        tm_val.tm_hour, tm_val.tm_min, tm_val.tm_sec);
#endif
}

// ─────────────────────────────────────────────────────────────────────────────
// 内部：sink 回调函数
// ─────────────────────────────────────────────────────────────────────────────

static void console_write(void* ctx, const mine::diag::LogRecord& rec) noexcept {
    const auto* state = static_cast<const ConsoleSinkCtx*>(ctx);

    // 选择输出流
    FILE* stream = stdout;
    if (state->use_stderr_for_warn) {
        if (rec.level >= mine::diag::LogLevel::Warn) {
            stream = stderr;
        }
    }

    // 格式化时间戳
    char ts[24]{};
    format_timestamp(rec.timestamp, ts, sizeof(ts));

    // 类别字段
    const char* cat    = (rec.category != nullptr && rec.category[0] != '\0')
                         ? rec.category : nullptr;
    const char* msg    = (rec.message != nullptr) ? rec.message : "";
    const char* tag    = level_tag(rec.level);

    if (state->use_color) {
        const char* color = level_ansi_color(rec.level);
        if (cat != nullptr) {
            (void)::fprintf(stream, "%s[%s] [%s] [%s] %s\033[0m\n",
                color, ts, tag, cat, msg);
        } else {
            (void)::fprintf(stream, "%s[%s] [%s] %s\033[0m\n",
                color, ts, tag, msg);
        }
    } else {
        if (cat != nullptr) {
            (void)::fprintf(stream, "[%s] [%s] [%s] %s\n", ts, tag, cat, msg);
        } else {
            (void)::fprintf(stream, "[%s] [%s] %s\n", ts, tag, msg);
        }
    }
}

static void console_flush(void* ctx) noexcept {
    (void)ctx;
    (void)::fflush(stdout);
    (void)::fflush(stderr);
}

static void console_destroy(void* ctx) noexcept {
    ::free(ctx);
}

// ─────────────────────────────────────────────────────────────────────────────
// 工厂函数实现
// ─────────────────────────────────────────────────────────────────────────────

mine::diag::LogSink make_console_sink(const ConsoleSinkOptions& opts) noexcept {
    // 分配上下文
    auto* state = static_cast<ConsoleSinkCtx*>(::malloc(sizeof(ConsoleSinkCtx)));
    if (state == nullptr) {
        return {};  // 内存不足，返回无效 sink（write_fn == nullptr）
    }
    state->use_color           = opts.use_color;
    state->use_stderr_for_warn = opts.use_stderr_for_warn;

    mine::diag::LogSink sink{};
    sink.write_fn   = console_write;
    sink.flush_fn   = console_flush;
    sink.destroy_fn = console_destroy;
    sink.ctx        = state;
    sink.min_level  = opts.min_level;
    return sink;
}

uint32_t add_console_sink(const ConsoleSinkOptions& opts) noexcept {
    mine::diag::LogSink sink = make_console_sink(opts);
    if (sink.write_fn == nullptr) return 0u;
    return mine::diag::Logger::global().add_sink(sink);
}

} // namespace mine::logging
