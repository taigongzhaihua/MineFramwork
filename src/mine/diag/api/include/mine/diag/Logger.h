/**
 * @file Logger.h
 * @brief mine.diag 日志接口：LogLevel、LogRecord、LogSink、Logger 全局调度器。
 *
 * 架构分层：
 *   - mine.diag（本文件）：定义日志接口，不依赖任何 sink 实现
 *   - mine.logging：提供 ConsoleSink / FileSink 等具体 sink 实现
 *
 * 设计约束（C++20，MSVC，/GR-，/EHs-c-）：
 *   - 无虚函数：sink 通过函数指针（LogWriteFn / LogFlushFn / LogDestroyFn）接入
 *   - 无异常：所有方法标记 noexcept
 *   - 无 RTTI：不使用 dynamic_cast / typeid
 *   - Pimpl 封装：Logger 内部实现（sink 列表）完全隐藏
 *   - 最多支持 16 个并发 sink（固定数组，无堆分配）
 *
 * 典型用法：
 * @code
 *   // 注册一个自定义 sink
 *   mine::diag::LogSink sink;
 *   sink.write_fn = [](void*, const mine::diag::LogRecord& rec) noexcept {
 *       ::puts(rec.message);
 *   };
 *   uint32_t token = mine::diag::Logger::global().add_sink(sink);
 *
 *   // 写日志（通过宏）
 *   MINE_LOG_INFO("应用启动");
 *   MINE_LOGF(mine::diag::LogLevel::Error, "net", "连接失败：%s", host);
 *
 *   // 注销 sink
 *   mine::diag::Logger::global().remove_sink(token);
 * @endcode
 *
 * 线程安全说明：
 *   Logger::write() 不提供内部锁保护。
 *   若在多线程环境中使用，调用方负责在外部同步，
 *   或确保所有写操作发生在同一线程（推荐使用队列+专用日志线程）。
 */

#pragma once

#include <cstdint>
#include <ctime>
#include <mine/diag/Api.h>
#include <mine/core/Pimpl.h>

namespace mine::diag {

// ─────────────────────────────────────────────────────────────────────────────
// LogLevel：日志级别枚举
// ─────────────────────────────────────────────────────────────────────────────

/**
 * @brief 日志级别，从低到高依次为 Trace < Debug < Info < Warn < Error < Fatal。
 *
 * Logger 和每个 LogSink 均可独立设置 min_level：
 *   - Logger 全局最低级别：低于此级别的日志在调度前直接丢弃
 *   - sink 最低级别：仅接收达到或超过此级别的日志
 */
enum class LogLevel : uint8_t {
    Trace = 0,  ///< 极细粒度追踪（热路径调试，默认关闭）
    Debug = 1,  ///< 调试信息
    Info  = 2,  ///< 普通运行信息
    Warn  = 3,  ///< 警告（不影响功能但值得关注）
    Error = 4,  ///< 错误（功能受损，需要处理）
    Fatal = 5,  ///< 致命错误（程序无法继续，触发终止）
};

// ─────────────────────────────────────────────────────────────────────────────
// LogRecord：一条日志记录（值类型，不拥有字符串内存）
// ─────────────────────────────────────────────────────────────────────────────

/**
 * @brief 单条日志记录，生命周期不超过 LogSink::write_fn 调用。
 *
 * 所有指针字段均不拥有内存（借用调用方的字符串）。
 * write_fn 不得在返回后持有这些指针（如需持久化，必须拷贝字符串内容）。
 */
struct LogRecord {
    LogLevel    level;          ///< 日志级别
    time_t      timestamp;      ///< 消息时间（time(nullptr)，秒精度）
    const char* category;       ///< 日志类别（模块名），可为 nullptr
    const char* message;        ///< 日志消息（null 终止 UTF-8 字符串）
    const char* file;           ///< 源文件路径（__FILE__），可为 nullptr
    int         line;           ///< 源文件行号（__LINE__）
};

// ─────────────────────────────────────────────────────────────────────────────
// LogSink：sink 函数指针描述符
// ─────────────────────────────────────────────────────────────────────────────

/// sink 写入函数：将一条日志记录输出到具体目标（文件/控制台等）
using LogWriteFn   = void (*)(void* ctx, const LogRecord& rec) noexcept;

/// sink 刷新函数：将缓冲区内容强制写入目标（可为 nullptr）
using LogFlushFn   = void (*)(void* ctx) noexcept;

/// sink 销毁函数：释放 ctx 持有的所有资源（可为 nullptr）
using LogDestroyFn = void (*)(void* ctx) noexcept;

/**
 * @brief sink 描述符，值类型，通过 Logger::add_sink() 注册。
 *
 * - write_fn 为必填字段，其余可为 nullptr。
 * - ctx 由调用方负责生命周期管理（通过 destroy_fn 清理）。
 * - min_level：只有 rec.level >= min_level 的记录才会传入 write_fn。
 */
struct LogSink {
    LogWriteFn   write_fn   = nullptr;  ///< 写入回调（必填）
    LogFlushFn   flush_fn   = nullptr;  ///< 刷新回调（可选）
    LogDestroyFn destroy_fn = nullptr;  ///< 销毁回调（可选）
    void*        ctx        = nullptr;  ///< 传给回调的用户上下文
    LogLevel     min_level  = LogLevel::Trace;  ///< sink 最低接受级别
};

// ─────────────────────────────────────────────────────────────────────────────
// Logger：全局日志调度器
// ─────────────────────────────────────────────────────────────────────────────

/**
 * @brief 全局日志调度器，将日志记录分发给所有已注册的 sink。
 *
 * Logger 是单例（通过 Logger::global() 访问），不可拷贝/移动。
 * 内部使用固定大小数组（最多 16 个 sink）存储注册列表，无堆分配。
 */
#if defined(_MSC_VER)
#   pragma warning(push)
#   pragma warning(disable: 4251)
#endif
class MINE_DIAG_API Logger {
public:
    ~Logger() noexcept;
    Logger(const Logger&) = delete;
    Logger& operator=(const Logger&) = delete;

    // ── 全局单例 ─────────────────────────────────────────────────────────

    /**
     * @brief 获取全局 Logger 单例引用。
     *
     * 首次调用时构造（局部静态变量，C++11 保证线程安全初始化）。
     */
    [[nodiscard]] static Logger& global() noexcept;

    // ── 全局最低级别 ─────────────────────────────────────────────────────

    /**
     * @brief 设置全局最低日志级别。
     *
     * 低于此级别的消息在调度前直接丢弃（不传给任何 sink），
     * 这是在 Logger 层面的快速过滤，先于每个 sink 的 min_level 检查。
     */
    void set_min_level(LogLevel level) noexcept;

    /// 获取当前全局最低日志级别（默认 Info）。
    [[nodiscard]] LogLevel min_level() const noexcept;

    // ── sink 管理 ────────────────────────────────────────────────────────

    /**
     * @brief 注册一个 sink，返回用于注销的 token（0 表示注册失败/已满）。
     *
     * sink 列表最多容纳 16 个。write_fn 为 nullptr 时注册无效返回 0。
     * Logger 不拷贝 ctx 的内容，destroy_fn 由 Logger 在 remove_sink
     * 或 clear_sinks 时调用（若 destroy_fn != nullptr）。
     */
    [[nodiscard]] uint32_t add_sink(LogSink sink) noexcept;

    /**
     * @brief 注销 token 对应的 sink，并调用其 destroy_fn（若非 nullptr）。
     *
     * token 无效时静默忽略。
     */
    void remove_sink(uint32_t token) noexcept;

    /**
     * @brief 注销所有已注册 sink，并依次调用它们的 destroy_fn。
     */
    void clear_sinks() noexcept;

    // ── 写入与刷新 ───────────────────────────────────────────────────────

    /**
     * @brief 将一条日志记录分发给所有级别满足条件的 sink。
     *
     * 若 rec.level < min_level()，直接返回不分发。
     * 不提供内部锁，需调用方保证线程安全（或限定在单线程使用）。
     */
    void write(const LogRecord& rec) noexcept;

    /**
     * @brief 通知所有已注册 sink 执行刷新（调用 flush_fn）。
     */
    void flush() noexcept;

private:
    Logger() noexcept;
    struct Impl;
    mine::core::Pimpl<Impl> p_;
};
#if defined(_MSC_VER)
#   pragma warning(pop)
#endif

// ─────────────────────────────────────────────────────────────────────────────
// 便利函数（通过全局 Logger 写入）
// ─────────────────────────────────────────────────────────────────────────────

/**
 * @brief 向全局 Logger 写入一条日志（带文件/行号信息）。
 *
 * 通常不直接调用，而是通过宏 MINE_LOG / MINE_LOGF 使用。
 */
MINE_DIAG_API void log(
    LogLevel    level,
    const char* category,
    const char* message,
    const char* file  = nullptr,
    int         line  = 0) noexcept;

} // namespace mine::diag

// ─────────────────────────────────────────────────────────────────────────────
// 日志宏
// ─────────────────────────────────────────────────────────────────────────────

/**
 * MINE_LOG(level, cat, msg)
 *   将固定字符串 msg 以指定 level 和 category 写入全局 Logger。
 *   cat 可以为 nullptr（表示无类别）。
 *
 * MINE_LOGF(level, cat, fmt, ...)
 *   printf 风格格式化后写入全局 Logger（内部缓冲区 512 字节，超出截断）。
 *
 * MINE_LOG_TRACE / MINE_LOG_DEBUG / MINE_LOG_INFO / MINE_LOG_WARN /
 * MINE_LOG_ERROR / MINE_LOG_FATAL
 *   不带类别的快捷宏（category = nullptr）。
 */

#define MINE_LOG(level, cat, msg) \
    ::mine::diag::log((level), (cat), (msg), __FILE__, __LINE__)

// NOLINTBEGIN(cppcoreguidelines-pro-type-vararg)
#define MINE_LOGF(level, cat, fmt, ...) \
    do { \
        char _mine_logbuf_[512]; \
        ::snprintf(_mine_logbuf_, sizeof(_mine_logbuf_), (fmt), ##__VA_ARGS__); \
        ::mine::diag::log((level), (cat), _mine_logbuf_, __FILE__, __LINE__); \
    } while (false)
// NOLINTEND(cppcoreguidelines-pro-type-vararg)

#define MINE_LOG_TRACE(msg)  MINE_LOG(::mine::diag::LogLevel::Trace, nullptr, (msg))
#define MINE_LOG_DEBUG(msg)  MINE_LOG(::mine::diag::LogLevel::Debug, nullptr, (msg))
#define MINE_LOG_INFO(msg)   MINE_LOG(::mine::diag::LogLevel::Info,  nullptr, (msg))
#define MINE_LOG_WARN(msg)   MINE_LOG(::mine::diag::LogLevel::Warn,  nullptr, (msg))
#define MINE_LOG_ERROR(msg)  MINE_LOG(::mine::diag::LogLevel::Error, nullptr, (msg))
#define MINE_LOG_FATAL(msg)  MINE_LOG(::mine::diag::LogLevel::Fatal, nullptr, (msg))

#define MINE_LOGF_TRACE(fmt, ...)  MINE_LOGF(::mine::diag::LogLevel::Trace, nullptr, fmt, ##__VA_ARGS__)
#define MINE_LOGF_DEBUG(fmt, ...)  MINE_LOGF(::mine::diag::LogLevel::Debug, nullptr, fmt, ##__VA_ARGS__)
#define MINE_LOGF_INFO(fmt, ...)   MINE_LOGF(::mine::diag::LogLevel::Info,  nullptr, fmt, ##__VA_ARGS__)
#define MINE_LOGF_WARN(fmt, ...)   MINE_LOGF(::mine::diag::LogLevel::Warn,  nullptr, fmt, ##__VA_ARGS__)
#define MINE_LOGF_ERROR(fmt, ...)  MINE_LOGF(::mine::diag::LogLevel::Error, nullptr, fmt, ##__VA_ARGS__)
#define MINE_LOGF_FATAL(fmt, ...)  MINE_LOGF(::mine::diag::LogLevel::Fatal, nullptr, fmt, ##__VA_ARGS__)
