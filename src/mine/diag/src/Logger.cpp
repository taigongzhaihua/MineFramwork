/**
 * @file Logger.cpp
 * @brief mine.diag::Logger 全局日志调度器实现。
 *
 * 内部使用固定大小数组（kMaxSinks = 16）存储 sink，无堆分配。
 * 线程安全：Logger::write() 不提供内部锁，请在单线程或外部同步使用。
 */

#include <mine/diag/Logger.h>
#include <mine/core/Pimpl.h>

#include <cstdio>
#include <cstring>

namespace mine::diag {

// ─────────────────────────────────────────────────────────────────────────────
// Logger::Impl：内部实现
// ─────────────────────────────────────────────────────────────────────────────

struct Logger::Impl {
    // sink 最大数量（固定数组，无堆分配）
    static constexpr uint32_t kMaxSinks = 16u;

    // sink 条目
    struct Entry {
        LogSink  sink{};
        uint32_t token  = 0u;
        bool     active = false;
    };

    Entry    entries_[kMaxSinks]{};
    uint32_t next_token_ = 1u;       // 0 为无效 token，从 1 开始分配
    LogLevel min_level_  = LogLevel::Info;  // 默认全局最低级别：Info
};

// ─────────────────────────────────────────────────────────────────────────────
// Logger 构造/析构
// ─────────────────────────────────────────────────────────────────────────────

Logger::Logger() noexcept
    : p_{mine::core::make_pimpl<Impl>()}
{
}

Logger::~Logger() noexcept {
    // 析构时调用所有 sink 的 destroy_fn，释放上下文资源
    for (uint32_t i = 0u; i < Impl::kMaxSinks; ++i) {
        auto& e = p_->entries_[i];
        if (e.active && e.sink.destroy_fn != nullptr) {
            e.sink.destroy_fn(e.sink.ctx);
            e.sink.destroy_fn = nullptr;
        }
        e.active = false;
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// Logger::global()：全局单例
// ─────────────────────────────────────────────────────────────────────────────

Logger& Logger::global() noexcept {
    // C++11 保证局部静态变量线程安全初始化
    static Logger s_instance;
    return s_instance;
}

// ─────────────────────────────────────────────────────────────────────────────
// 全局最低级别
// ─────────────────────────────────────────────────────────────────────────────

void Logger::set_min_level(LogLevel level) noexcept {
    p_->min_level_ = level;
}

LogLevel Logger::min_level() const noexcept {
    return p_->min_level_;
}

// ─────────────────────────────────────────────────────────────────────────────
// sink 管理
// ─────────────────────────────────────────────────────────────────────────────

uint32_t Logger::add_sink(LogSink sink) noexcept {
    // write_fn 为必填，否则 sink 无效
    if (sink.write_fn == nullptr) {
        return 0u;
    }

    // 找第一个空闲槽位
    for (uint32_t i = 0u; i < Impl::kMaxSinks; ++i) {
        auto& e = p_->entries_[i];
        if (!e.active) {
            e.sink   = sink;
            e.token  = p_->next_token_++;
            // 防止 next_token_ 回绕到 0（极端情况：注册超过 2^32 次）
            if (p_->next_token_ == 0u) {
                p_->next_token_ = 1u;
            }
            e.active = true;
            return e.token;
        }
    }

    // sink 列表已满，注册失败
    return 0u;
}

void Logger::remove_sink(uint32_t token) noexcept {
    if (token == 0u) return;

    for (uint32_t i = 0u; i < Impl::kMaxSinks; ++i) {
        auto& e = p_->entries_[i];
        if (e.active && e.token == token) {
            // 调用 destroy_fn 释放 ctx
            if (e.sink.destroy_fn != nullptr) {
                e.sink.destroy_fn(e.sink.ctx);
            }
            e = {};
            return;
        }
    }
}

void Logger::clear_sinks() noexcept {
    for (uint32_t i = 0u; i < Impl::kMaxSinks; ++i) {
        auto& e = p_->entries_[i];
        if (e.active) {
            if (e.sink.destroy_fn != nullptr) {
                e.sink.destroy_fn(e.sink.ctx);
            }
            e = {};
        }
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// 写入与刷新
// ─────────────────────────────────────────────────────────────────────────────

void Logger::write(const LogRecord& rec) noexcept {
    // 全局最低级别快速过滤
    if (rec.level < p_->min_level_) return;

    for (uint32_t i = 0u; i < Impl::kMaxSinks; ++i) {
        const auto& e = p_->entries_[i];
        if (e.active && rec.level >= e.sink.min_level && e.sink.write_fn != nullptr) {
            e.sink.write_fn(e.sink.ctx, rec);
        }
    }
}

void Logger::flush() noexcept {
    for (uint32_t i = 0u; i < Impl::kMaxSinks; ++i) {
        const auto& e = p_->entries_[i];
        if (e.active && e.sink.flush_fn != nullptr) {
            e.sink.flush_fn(e.sink.ctx);
        }
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// 便利函数
// ─────────────────────────────────────────────────────────────────────────────

void log(LogLevel level, const char* category, const char* message,
         const char* file, int line) noexcept {
    LogRecord rec{};
    rec.level     = level;
    rec.timestamp = ::time(nullptr);
    rec.category  = category;
    rec.message   = (message != nullptr) ? message : "";
    rec.file      = file;
    rec.line      = line;
    Logger::global().write(rec);
}

} // namespace mine::diag
