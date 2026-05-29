/**
 * @file FileSink.cpp
 * @brief 文件日志 sink 实现（基于 C 标准库 FILE*）。
 *
 * 输出格式（无颜色转义码，适合机器解析）：
 *   [YYYY-MM-DD HH:MM:SS] [LEVEL] [category] message\n
 *   category 为空时省略 [category] 字段。
 *
 * 使用 fopen/fputs/fflush/fclose，不依赖 mine.io（尚未完整实现）。
 */

#include <mine/logging/FileSink.h>
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
// 内部：FileSink 上下文
// ─────────────────────────────────────────────────────────────────────────────

struct FileSinkCtx {
    FILE*    fp         = nullptr;
    bool     auto_flush = true;
    uint64_t max_bytes  = 0u;     // 0 = 无限制
    uint64_t written    = 0u;     // 已写入字节数
};

// ─────────────────────────────────────────────────────────────────────────────
// 内部：级别标签
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

static void file_write(void* ctx, const mine::diag::LogRecord& rec) noexcept {
    auto* state = static_cast<FileSinkCtx*>(ctx);
    if (state->fp == nullptr) return;

    // 检查大小限制
    if (state->max_bytes > 0u && state->written >= state->max_bytes) return;

    // 格式化时间戳
    char ts[24]{};
    format_timestamp(rec.timestamp, ts, sizeof(ts));

    const char* cat = (rec.category != nullptr && rec.category[0] != '\0')
                      ? rec.category : nullptr;
    const char* msg = (rec.message != nullptr) ? rec.message : "";
    const char* tag = level_tag(rec.level);

    // 格式化并写入（固定缓冲区，超长消息截断）
    char buf[2048];
    int  n = 0;
    if (cat != nullptr) {
        n = ::snprintf(buf, sizeof(buf), "[%s] [%s] [%s] %s\n", ts, tag, cat, msg);
    } else {
        n = ::snprintf(buf, sizeof(buf), "[%s] [%s] %s\n", ts, tag, msg);
    }

    if (n <= 0) return;

    // 确保不超过缓冲区（snprintf 保证 null 终止但可能截断）
    size_t write_len = (n < static_cast<int>(sizeof(buf)))
                       ? static_cast<size_t>(n)
                       : sizeof(buf) - 1u;

    // 大小限制：仅写入不超出限制的部分
    if (state->max_bytes > 0u) {
        uint64_t remaining = state->max_bytes - state->written;
        if (write_len > remaining) {
            write_len = static_cast<size_t>(remaining);
        }
    }

    if (write_len == 0u) return;

    size_t actually = ::fwrite(buf, 1u, write_len, state->fp);
    state->written += static_cast<uint64_t>(actually);

    if (state->auto_flush) {
        (void)::fflush(state->fp);
    }
}

static void file_flush(void* ctx) noexcept {
    const auto* state = static_cast<const FileSinkCtx*>(ctx);
    if (state->fp != nullptr) {
        (void)::fflush(state->fp);
    }
}

static void file_destroy(void* ctx) noexcept {
    auto* state = static_cast<FileSinkCtx*>(ctx);
    if (state->fp != nullptr) {
        (void)::fclose(state->fp);
        state->fp = nullptr;
    }
    ::free(state);
}

// ─────────────────────────────────────────────────────────────────────────────
// 工厂函数实现
// ─────────────────────────────────────────────────────────────────────────────

mine::diag::LogSink make_file_sink(const FileSinkOptions& opts) noexcept {
    // path 不能为空
    if (opts.path == nullptr || opts.path[0] == '\0') {
        return {};
    }

    // 打开文件
    const char* mode = opts.append ? "a" : "w";
    FILE* fp = ::fopen(opts.path, mode);
    if (fp == nullptr) {
        return {};  // 文件打开失败，返回无效 sink
    }

    // 分配上下文
    auto* state = static_cast<FileSinkCtx*>(::malloc(sizeof(FileSinkCtx)));
    if (state == nullptr) {
        (void)::fclose(fp);
        return {};
    }

    state->fp         = fp;
    state->auto_flush = opts.auto_flush;
    state->max_bytes  = opts.max_bytes;
    state->written    = 0u;

    mine::diag::LogSink sink{};
    sink.write_fn   = file_write;
    sink.flush_fn   = file_flush;
    sink.destroy_fn = file_destroy;
    sink.ctx        = state;
    sink.min_level  = opts.min_level;
    return sink;
}

uint32_t add_file_sink(const FileSinkOptions& opts) noexcept {
    mine::diag::LogSink sink = make_file_sink(opts);
    if (sink.write_fn == nullptr) return 0u;
    return mine::diag::Logger::global().add_sink(sink);
}

} // namespace mine::logging
