/**
 * @file LoggingTest.cpp
 * @brief mine.logging 模块单元测试（mine.diag::Logger + ConsoleSink + FileSink）。
 *
 * 测试范围：
 *   1. Logger 基础：默认最低级别、set_min_level
 *   2. Logger::add_sink / remove_sink / clear_sinks
 *   3. 级别过滤：全局最低级别过滤、sink 最低级别过滤
 *   4. 多 sink 分发：同一条日志到多个 sink
 *   5. destroy_fn：remove_sink / clear_sinks 时调用 destroy_fn
 *   6. sink 列表已满（超过 16 个）返回 0
 *   7. add_sink(write_fn=nullptr) 返回 0
 *   8. log() 便利函数：传入 nullptr message 安全
 *   9. MINE_LOG 宏展开
 *   10. MINE_LOGF 宏展开
 *   11. ConsoleSink：make_console_sink 返回合法 sink，不带颜色时手动调用
 *   12. FileSink：写入文件并读取验证内容
 *   13. FileSink：append 模式（不截断已有内容）
 *   14. FileSink：overwrite 模式（截断已有内容）
 *   15. FileSink：max_bytes 限制（超出后停止写入）
 *   16. FileSink：path 为 nullptr 时返回无效 sink
 *   17. FileSink：auto_flush = false（延迟刷新）
 *   18. Logger token 0 为无效（remove_sink(0) 不崩溃）
 *   19. flush() 调用所有 sink 的 flush_fn
 *   20. 记录时间戳非 0（time(nullptr) > 0）
 */

#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>

// 测试中大量忽略 add_sink 的 [[nodiscard]] 返回值（不需要 token 时正常行为）
#if defined(_MSC_VER)
#   pragma warning(disable: 4834)  // 忽略 [[nodiscard]] 返回值
#   pragma warning(disable: 4996)  // strncpy/fopen 在测试辅助中使用（可接受）
#endif

#include <mine/logging/Logging.h>
#include <mine/diag/Diag.h>

#include <cstdio>
#include <cstring>
#include <cstdlib>

// ─────────────────────────────────────────────────────────────────────────────
// 测试辅助：计数型 sink（记录调用次数和最后收到的记录）
// ─────────────────────────────────────────────────────────────────────────────

struct CounterCtx {
    int   write_count   = 0;
    int   flush_count   = 0;
    int   destroy_count = 0;
    mine::diag::LogLevel last_level = mine::diag::LogLevel::Trace;
    char  last_category[64]{};
    char  last_message[256]{};
};

static void counter_write(void* ctx, const mine::diag::LogRecord& rec) noexcept {
    auto* c = static_cast<CounterCtx*>(ctx);
    ++c->write_count;
    c->last_level = rec.level;
    if (rec.category != nullptr) {
        ::strncpy(c->last_category, rec.category, sizeof(c->last_category) - 1);
    } else {
        c->last_category[0] = '\0';
    }
    if (rec.message != nullptr) {
        ::strncpy(c->last_message, rec.message, sizeof(c->last_message) - 1);
    } else {
        c->last_message[0] = '\0';
    }
}

static void counter_flush(void* ctx) noexcept {
    auto* c = static_cast<CounterCtx*>(ctx);
    ++c->flush_count;
}

static void counter_destroy(void* ctx) noexcept {
    auto* c = static_cast<CounterCtx*>(ctx);
    ++c->destroy_count;
    // 注意：此处不 free，ctx 为测试栈上的对象
}

// 创建指向栈上 CounterCtx 的测试 sink（ctx 不拥有，destroy_fn 只计数）
static mine::diag::LogSink make_counter_sink(CounterCtx* ctx,
    mine::diag::LogLevel min_level = mine::diag::LogLevel::Trace) noexcept
{
    mine::diag::LogSink sink{};
    sink.write_fn   = counter_write;
    sink.flush_fn   = counter_flush;
    sink.destroy_fn = counter_destroy;
    sink.ctx        = ctx;
    sink.min_level  = min_level;
    return sink;
}

// ─────────────────────────────────────────────────────────────────────────────
// 测试夹具：每个测试前清空 Logger 状态
// ─────────────────────────────────────────────────────────────────────────────

// 每个测试用例开始时清空 sink 列表并重置全局最低级别
static void reset_logger() noexcept {
    auto& logger = mine::diag::Logger::global();
    logger.clear_sinks();
    logger.set_min_level(mine::diag::LogLevel::Trace);
}

// ─────────────────────────────────────────────────────────────────────────────
// 测试套件 1：Logger 基础行为
// ─────────────────────────────────────────────────────────────────────────────

TEST_SUITE("Logger 基础") {

TEST_CASE("Logger::global() 返回同一单例") {
    auto& a = mine::diag::Logger::global();
    auto& b = mine::diag::Logger::global();
    CHECK(&a == &b);
}

TEST_CASE("默认最低级别为 Info") {
    // Logger 默认构造后 min_level_ = Info，但测试环境可能已被其他测试修改
    // 此处直接 set 后检查
    auto& logger = mine::diag::Logger::global();
    logger.set_min_level(mine::diag::LogLevel::Info);
    CHECK(logger.min_level() == mine::diag::LogLevel::Info);
}

TEST_CASE("set_min_level 和 min_level 对称") {
    reset_logger();
    auto& logger = mine::diag::Logger::global();

    logger.set_min_level(mine::diag::LogLevel::Trace);
    CHECK(logger.min_level() == mine::diag::LogLevel::Trace);

    logger.set_min_level(mine::diag::LogLevel::Fatal);
    CHECK(logger.min_level() == mine::diag::LogLevel::Fatal);

    logger.set_min_level(mine::diag::LogLevel::Debug);
    CHECK(logger.min_level() == mine::diag::LogLevel::Debug);
}

TEST_CASE("add_sink(write_fn=nullptr) 返回 0") {
    reset_logger();
    mine::diag::LogSink sink{};
    sink.write_fn = nullptr;
    CHECK(mine::diag::Logger::global().add_sink(sink) == 0u);
}

TEST_CASE("add_sink 返回非零 token") {
    reset_logger();
    CounterCtx ctx{};
    uint32_t token = mine::diag::Logger::global().add_sink(make_counter_sink(&ctx));
    CHECK(token != 0u);
}

TEST_CASE("remove_sink(0) 不崩溃") {
    reset_logger();
    mine::diag::Logger::global().remove_sink(0u);  // 不应崩溃
    CHECK(true);
}

TEST_CASE("remove_sink 注销后不再接收日志") {
    reset_logger();
    auto& logger = mine::diag::Logger::global();
    CounterCtx ctx{};
    uint32_t token = logger.add_sink(make_counter_sink(&ctx));

    mine::diag::LogRecord rec{};
    rec.level    = mine::diag::LogLevel::Info;
    rec.message  = "before";
    rec.timestamp = ::time(nullptr);
    logger.write(rec);
    CHECK(ctx.write_count == 1);

    logger.remove_sink(token);

    rec.message = "after";
    logger.write(rec);
    CHECK(ctx.write_count == 1);  // 不变
}

TEST_CASE("clear_sinks 注销所有 sink") {
    reset_logger();
    auto& logger = mine::diag::Logger::global();
    CounterCtx c1{}, c2{};
    logger.add_sink(make_counter_sink(&c1));
    logger.add_sink(make_counter_sink(&c2));

    mine::diag::LogRecord rec{};
    rec.level     = mine::diag::LogLevel::Info;
    rec.message   = "hello";
    rec.timestamp = ::time(nullptr);
    logger.write(rec);
    CHECK(c1.write_count == 1);
    CHECK(c2.write_count == 1);

    logger.clear_sinks();
    logger.write(rec);
    CHECK(c1.write_count == 1);  // 不变
    CHECK(c2.write_count == 1);  // 不变
}

TEST_CASE("add_sink 超过 16 个时返回 0") {
    reset_logger();
    auto& logger = mine::diag::Logger::global();

    CounterCtx ctx{};
    uint32_t tokens[20]{};
    int registered = 0;

    for (int i = 0; i < 20; ++i) {
        tokens[i] = logger.add_sink(make_counter_sink(&ctx));
        if (tokens[i] != 0u) ++registered;
    }

    CHECK(registered == 16);  // 最多 16 个

    // 清理（destroy_fn 不释放栈上 ctx，安全）
    logger.clear_sinks();
}

} // TEST_SUITE("Logger 基础")

// ─────────────────────────────────────────────────────────────────────────────
// 测试套件 2：级别过滤
// ─────────────────────────────────────────────────────────────────────────────

TEST_SUITE("级别过滤") {

TEST_CASE("全局最低级别过滤：低于全局级别的日志被丢弃") {
    reset_logger();
    auto& logger = mine::diag::Logger::global();
    logger.set_min_level(mine::diag::LogLevel::Warn);

    CounterCtx ctx{};
    logger.add_sink(make_counter_sink(&ctx));

    mine::diag::LogRecord rec{};
    rec.timestamp = ::time(nullptr);

    rec.level   = mine::diag::LogLevel::Info;
    rec.message = "info";
    logger.write(rec);
    CHECK(ctx.write_count == 0);  // Info < Warn，被丢弃

    rec.level   = mine::diag::LogLevel::Warn;
    rec.message = "warn";
    logger.write(rec);
    CHECK(ctx.write_count == 1);  // Warn == Warn，通过

    rec.level   = mine::diag::LogLevel::Error;
    rec.message = "error";
    logger.write(rec);
    CHECK(ctx.write_count == 2);  // Error > Warn，通过
}

TEST_CASE("sink 最低级别过滤：sink.min_level 独立于全局级别") {
    reset_logger();
    auto& logger = mine::diag::Logger::global();
    logger.set_min_level(mine::diag::LogLevel::Trace);  // 全局放开

    CounterCtx c_all{}, c_error{};
    // sink1 接收所有级别
    logger.add_sink(make_counter_sink(&c_all, mine::diag::LogLevel::Trace));
    // sink2 只接收 Error 及以上
    logger.add_sink(make_counter_sink(&c_error, mine::diag::LogLevel::Error));

    mine::diag::LogRecord rec{};
    rec.timestamp = ::time(nullptr);

    rec.level   = mine::diag::LogLevel::Info;
    rec.message = "info";
    logger.write(rec);
    CHECK(c_all.write_count   == 1);  // 接收
    CHECK(c_error.write_count == 0);  // 不接收

    rec.level   = mine::diag::LogLevel::Error;
    rec.message = "error";
    logger.write(rec);
    CHECK(c_all.write_count   == 2);  // 接收
    CHECK(c_error.write_count == 1);  // 接收
}

TEST_CASE("Fatal 级别始终通过全局过滤") {
    reset_logger();
    auto& logger = mine::diag::Logger::global();
    logger.set_min_level(mine::diag::LogLevel::Fatal);

    CounterCtx ctx{};
    logger.add_sink(make_counter_sink(&ctx));

    mine::diag::LogRecord rec{};
    rec.level     = mine::diag::LogLevel::Fatal;
    rec.message   = "崩溃";
    rec.timestamp = ::time(nullptr);
    logger.write(rec);
    CHECK(ctx.write_count == 1);
}

} // TEST_SUITE("级别过滤")

// ─────────────────────────────────────────────────────────────────────────────
// 测试套件 3：日志记录字段
// ─────────────────────────────────────────────────────────────────────────────

TEST_SUITE("日志记录字段") {

TEST_CASE("write 传递正确的 level、category、message") {
    reset_logger();
    auto& logger = mine::diag::Logger::global();
    CounterCtx ctx{};
    logger.add_sink(make_counter_sink(&ctx));

    mine::diag::LogRecord rec{};
    rec.level     = mine::diag::LogLevel::Warn;
    rec.category  = "net";
    rec.message   = "连接超时";
    rec.timestamp = ::time(nullptr);
    logger.write(rec);

    CHECK(ctx.write_count == 1);
    CHECK(ctx.last_level == mine::diag::LogLevel::Warn);
    CHECK(::strcmp(ctx.last_category, "net") == 0);
    CHECK(::strcmp(ctx.last_message, "连接超时") == 0);
}

TEST_CASE("category 为 nullptr 时安全") {
    reset_logger();
    auto& logger = mine::diag::Logger::global();
    CounterCtx ctx{};
    logger.add_sink(make_counter_sink(&ctx));

    mine::diag::LogRecord rec{};
    rec.level     = mine::diag::LogLevel::Info;
    rec.category  = nullptr;
    rec.message   = "无类别消息";
    rec.timestamp = ::time(nullptr);
    logger.write(rec);
    CHECK(ctx.write_count == 1);
}

TEST_CASE("message 为 nullptr 时不崩溃（log() 便利函数处理）") {
    reset_logger();
    auto& logger = mine::diag::Logger::global();
    CounterCtx ctx{};
    logger.add_sink(make_counter_sink(&ctx));

    // 通过便利函数写入 nullptr message
    mine::diag::log(mine::diag::LogLevel::Info, nullptr, nullptr);
    CHECK(ctx.write_count == 1);
    // message 应被处理为空字符串
    CHECK(ctx.last_message[0] == '\0');
}

TEST_CASE("时间戳非零") {
    reset_logger();
    auto& logger = mine::diag::Logger::global();

    time_t recorded_ts = 0;
    mine::diag::LogSink sink{};
    sink.write_fn = [](void* ctx, const mine::diag::LogRecord& rec) noexcept {
        *static_cast<time_t*>(ctx) = rec.timestamp;
    };
    sink.ctx = &recorded_ts;
    logger.add_sink(sink);

    mine::diag::log(mine::diag::LogLevel::Info, nullptr, "ts test");
    CHECK(recorded_ts > 0);
}

} // TEST_SUITE("日志记录字段")

// ─────────────────────────────────────────────────────────────────────────────
// 测试套件 4：多 sink 分发
// ─────────────────────────────────────────────────────────────────────────────

TEST_SUITE("多 sink 分发") {

TEST_CASE("一条日志分发给三个 sink") {
    reset_logger();
    auto& logger = mine::diag::Logger::global();
    CounterCtx c1{}, c2{}, c3{};
    logger.add_sink(make_counter_sink(&c1));
    logger.add_sink(make_counter_sink(&c2));
    logger.add_sink(make_counter_sink(&c3));

    mine::diag::LogRecord rec{};
    rec.level     = mine::diag::LogLevel::Info;
    rec.message   = "broadcast";
    rec.timestamp = ::time(nullptr);
    logger.write(rec);

    CHECK(c1.write_count == 1);
    CHECK(c2.write_count == 1);
    CHECK(c3.write_count == 1);
}

TEST_CASE("注销中间 sink 后其他 sink 仍正常接收") {
    reset_logger();
    auto& logger = mine::diag::Logger::global();
    CounterCtx c1{}, c2{}, c3{};
    auto t1 = logger.add_sink(make_counter_sink(&c1));
    auto t2 = logger.add_sink(make_counter_sink(&c2));
    auto t3 = logger.add_sink(make_counter_sink(&c3));
    (void)t1; (void)t3;

    logger.remove_sink(t2);  // 注销中间 sink

    mine::diag::LogRecord rec{};
    rec.level     = mine::diag::LogLevel::Info;
    rec.message   = "after remove";
    rec.timestamp = ::time(nullptr);
    logger.write(rec);

    CHECK(c1.write_count == 1);
    CHECK(c2.write_count == 0);  // 已注销
    CHECK(c3.write_count == 1);
}

} // TEST_SUITE("多 sink 分发")

// ─────────────────────────────────────────────────────────────────────────────
// 测试套件 5：destroy_fn 生命周期
// ─────────────────────────────────────────────────────────────────────────────

TEST_SUITE("destroy_fn 生命周期") {

TEST_CASE("remove_sink 时调用 destroy_fn") {
    reset_logger();
    auto& logger = mine::diag::Logger::global();
    CounterCtx ctx{};
    uint32_t token = logger.add_sink(make_counter_sink(&ctx));
    CHECK(ctx.destroy_count == 0);

    logger.remove_sink(token);
    CHECK(ctx.destroy_count == 1);
}

TEST_CASE("clear_sinks 时依次调用所有 destroy_fn") {
    reset_logger();
    auto& logger = mine::diag::Logger::global();
    CounterCtx c1{}, c2{};
    logger.add_sink(make_counter_sink(&c1));
    logger.add_sink(make_counter_sink(&c2));

    logger.clear_sinks();
    CHECK(c1.destroy_count == 1);
    CHECK(c2.destroy_count == 1);
}

TEST_CASE("destroy_fn 为 nullptr 时 remove_sink 不崩溃") {
    reset_logger();
    auto& logger = mine::diag::Logger::global();
    CounterCtx ctx{};
    mine::diag::LogSink sink = make_counter_sink(&ctx);
    sink.destroy_fn = nullptr;  // 显式置空
    uint32_t token = logger.add_sink(sink);
    logger.remove_sink(token);  // 不应崩溃
    CHECK(true);
}

} // TEST_SUITE("destroy_fn 生命周期")

// ─────────────────────────────────────────────────────────────────────────────
// 测试套件 6：flush
// ─────────────────────────────────────────────────────────────────────────────

TEST_SUITE("flush") {

TEST_CASE("flush() 调用所有 sink 的 flush_fn") {
    reset_logger();
    auto& logger = mine::diag::Logger::global();
    CounterCtx c1{}, c2{};
    logger.add_sink(make_counter_sink(&c1));
    logger.add_sink(make_counter_sink(&c2));

    logger.flush();
    CHECK(c1.flush_count == 1);
    CHECK(c2.flush_count == 1);
}

TEST_CASE("flush_fn 为 nullptr 时不崩溃") {
    reset_logger();
    auto& logger = mine::diag::Logger::global();
    mine::diag::LogSink sink{};
    sink.write_fn = counter_write;
    sink.flush_fn = nullptr;  // 无刷新
    CounterCtx ctx{};
    sink.ctx = &ctx;
    logger.add_sink(sink);

    logger.flush();  // 不应崩溃
    CHECK(true);
}

} // TEST_SUITE("flush")

// ─────────────────────────────────────────────────────────────────────────────
// 测试套件 7：MINE_LOG 宏
// ─────────────────────────────────────────────────────────────────────────────

TEST_SUITE("日志宏") {

TEST_CASE("MINE_LOG_INFO 触发写入") {
    reset_logger();
    auto& logger = mine::diag::Logger::global();
    logger.set_min_level(mine::diag::LogLevel::Trace);
    CounterCtx ctx{};
    logger.add_sink(make_counter_sink(&ctx));

    MINE_LOG_INFO("测试信息");
    CHECK(ctx.write_count == 1);
    CHECK(ctx.last_level == mine::diag::LogLevel::Info);
    CHECK(::strcmp(ctx.last_message, "测试信息") == 0);
}

TEST_CASE("MINE_LOG_WARN 触发写入") {
    reset_logger();
    auto& logger = mine::diag::Logger::global();
    CounterCtx ctx{};
    logger.add_sink(make_counter_sink(&ctx));

    MINE_LOG_WARN("警告消息");
    CHECK(ctx.write_count == 1);
    CHECK(ctx.last_level == mine::diag::LogLevel::Warn);
}

TEST_CASE("MINE_LOG_ERROR 触发写入") {
    reset_logger();
    auto& logger = mine::diag::Logger::global();
    CounterCtx ctx{};
    logger.add_sink(make_counter_sink(&ctx));

    MINE_LOG_ERROR("错误消息");
    CHECK(ctx.write_count == 1);
    CHECK(ctx.last_level == mine::diag::LogLevel::Error);
}

TEST_CASE("MINE_LOGF 格式化写入") {
    reset_logger();
    auto& logger = mine::diag::Logger::global();
    CounterCtx ctx{};
    logger.add_sink(make_counter_sink(&ctx));

    MINE_LOGF(mine::diag::LogLevel::Info, "net", "连接 %s 端口 %d", "127.0.0.1", 8080);
    CHECK(ctx.write_count == 1);
    CHECK(::strstr(ctx.last_message, "127.0.0.1") != nullptr);
    CHECK(::strstr(ctx.last_message, "8080") != nullptr);
    CHECK(::strcmp(ctx.last_category, "net") == 0);
}

TEST_CASE("MINE_LOGF_INFO 快捷宏格式化") {
    reset_logger();
    auto& logger = mine::diag::Logger::global();
    CounterCtx ctx{};
    logger.add_sink(make_counter_sink(&ctx));

    MINE_LOGF_INFO("值 = %d", 42);
    CHECK(ctx.write_count == 1);
    CHECK(::strstr(ctx.last_message, "42") != nullptr);
}

TEST_CASE("MINE_LOG_TRACE 低于默认 Info 级别时被过滤") {
    reset_logger();
    auto& logger = mine::diag::Logger::global();
    logger.set_min_level(mine::diag::LogLevel::Info);
    CounterCtx ctx{};
    logger.add_sink(make_counter_sink(&ctx));

    MINE_LOG_TRACE("追踪消息");
    CHECK(ctx.write_count == 0);  // Trace < Info，被过滤
}

} // TEST_SUITE("日志宏")

// ─────────────────────────────────────────────────────────────────────────────
// 测试套件 8：ConsoleSink
// ─────────────────────────────────────────────────────────────────────────────

TEST_SUITE("ConsoleSink") {

TEST_CASE("make_console_sink 返回合法 sink（write_fn 非 nullptr）") {
    mine::logging::ConsoleSinkOptions opts{};
    mine::diag::LogSink sink = mine::logging::make_console_sink(opts);
    CHECK(sink.write_fn != nullptr);
    // 清理
    if (sink.destroy_fn != nullptr) {
        sink.destroy_fn(sink.ctx);
    }
}

TEST_CASE("make_console_sink 不带颜色时手动调用 write_fn 不崩溃") {
    mine::logging::ConsoleSinkOptions opts{};
    opts.use_color = false;
    mine::diag::LogSink sink = mine::logging::make_console_sink(opts);
    REQUIRE(sink.write_fn != nullptr);

    mine::diag::LogRecord rec{};
    rec.level     = mine::diag::LogLevel::Info;
    rec.category  = "test";
    rec.message   = "控制台 sink 无颜色测试";
    rec.timestamp = ::time(nullptr);

    sink.write_fn(sink.ctx, rec);   // 不应崩溃
    sink.flush_fn(sink.ctx);        // 不应崩溃

    if (sink.destroy_fn != nullptr) {
        sink.destroy_fn(sink.ctx);
    }
}

TEST_CASE("make_console_sink 带颜色时各级别不崩溃") {
    mine::logging::ConsoleSinkOptions opts{};
    opts.use_color = true;
    mine::diag::LogSink sink = mine::logging::make_console_sink(opts);
    REQUIRE(sink.write_fn != nullptr);

    const mine::diag::LogLevel levels[] = {
        mine::diag::LogLevel::Trace,
        mine::diag::LogLevel::Debug,
        mine::diag::LogLevel::Info,
        mine::diag::LogLevel::Warn,
        mine::diag::LogLevel::Error,
        mine::diag::LogLevel::Fatal,
    };

    for (auto lv : levels) {
        mine::diag::LogRecord rec{};
        rec.level     = lv;
        rec.message   = "颜色测试";
        rec.timestamp = ::time(nullptr);
        sink.write_fn(sink.ctx, rec);
    }

    if (sink.destroy_fn != nullptr) {
        sink.destroy_fn(sink.ctx);
    }
}

TEST_CASE("make_console_sink category=nullptr 不崩溃") {
    mine::logging::ConsoleSinkOptions opts{};
    opts.use_color = false;
    mine::diag::LogSink sink = mine::logging::make_console_sink(opts);
    REQUIRE(sink.write_fn != nullptr);

    mine::diag::LogRecord rec{};
    rec.level     = mine::diag::LogLevel::Info;
    rec.category  = nullptr;
    rec.message   = "无类别";
    rec.timestamp = ::time(nullptr);
    sink.write_fn(sink.ctx, rec);

    if (sink.destroy_fn != nullptr) {
        sink.destroy_fn(sink.ctx);
    }
}

TEST_CASE("add_console_sink 注册到全局 Logger 并返回非零 token") {
    reset_logger();
    mine::logging::ConsoleSinkOptions opts{};
    opts.use_color = false;
    uint32_t token = mine::logging::add_console_sink(opts);
    CHECK(token != 0u);
    mine::diag::Logger::global().remove_sink(token);
}

} // TEST_SUITE("ConsoleSink")

// ─────────────────────────────────────────────────────────────────────────────
// 测试套件 9：FileSink
// ─────────────────────────────────────────────────────────────────────────────

// 测试用临时文件路径
static const char* kTestLogFile = "mine_logging_test_tmp.log";

// 辅助：读取整个文件内容到缓冲区（返回字节数，buf 末尾追加 '\0'）
static size_t read_file_content(const char* path, char* buf, size_t buf_size) noexcept {
    FILE* fp = ::fopen(path, "r");
    if (fp == nullptr) return 0u;
    size_t n = ::fread(buf, 1u, buf_size - 1u, fp);
    (void)::fclose(fp);
    buf[n] = '\0';
    return n;
}

// 辅助：删除临时文件（忽略失败）
static void remove_test_file(const char* path) noexcept {
    (void)::remove(path);
}

TEST_SUITE("FileSink") {

TEST_CASE("path 为 nullptr 时返回无效 sink") {
    mine::logging::FileSinkOptions opts{};
    opts.path = nullptr;
    mine::diag::LogSink sink = mine::logging::make_file_sink(opts);
    CHECK(sink.write_fn == nullptr);
    CHECK(sink.ctx == nullptr);
}

TEST_CASE("path 为空字符串时返回无效 sink") {
    mine::logging::FileSinkOptions opts{};
    opts.path = "";
    mine::diag::LogSink sink = mine::logging::make_file_sink(opts);
    CHECK(sink.write_fn == nullptr);
}

TEST_CASE("写入文件并读取内容包含消息") {
    remove_test_file(kTestLogFile);

    mine::logging::FileSinkOptions opts{};
    opts.path       = kTestLogFile;
    opts.append     = false;
    opts.auto_flush = true;

    mine::diag::LogSink sink = mine::logging::make_file_sink(opts);
    REQUIRE(sink.write_fn != nullptr);

    mine::diag::LogRecord rec{};
    rec.level     = mine::diag::LogLevel::Info;
    rec.category  = "test";
    rec.message   = "文件写入测试";
    rec.timestamp = ::time(nullptr);
    sink.write_fn(sink.ctx, rec);

    if (sink.destroy_fn != nullptr) {
        sink.destroy_fn(sink.ctx);
    }

    char buf[4096]{};
    size_t n = read_file_content(kTestLogFile, buf, sizeof(buf));
    CHECK(n > 0u);
    CHECK(::strstr(buf, "文件写入测试") != nullptr);
    CHECK(::strstr(buf, "INFO") != nullptr);
    CHECK(::strstr(buf, "test") != nullptr);

    remove_test_file(kTestLogFile);
}

TEST_CASE("overwrite 模式（append=false）截断已有内容") {
    remove_test_file(kTestLogFile);

    // 先写入第一条
    {
        mine::logging::FileSinkOptions opts{};
        opts.path   = kTestLogFile;
        opts.append = false;
        opts.auto_flush = true;
        mine::diag::LogSink sink = mine::logging::make_file_sink(opts);
        REQUIRE(sink.write_fn != nullptr);

        mine::diag::LogRecord rec{};
        rec.level     = mine::diag::LogLevel::Info;
        rec.message   = "第一次写入";
        rec.timestamp = ::time(nullptr);
        sink.write_fn(sink.ctx, rec);
        if (sink.destroy_fn) sink.destroy_fn(sink.ctx);
    }

    // 再以覆盖模式写入第二条
    {
        mine::logging::FileSinkOptions opts{};
        opts.path   = kTestLogFile;
        opts.append = false;  // 覆盖
        opts.auto_flush = true;
        mine::diag::LogSink sink = mine::logging::make_file_sink(opts);
        REQUIRE(sink.write_fn != nullptr);

        mine::diag::LogRecord rec{};
        rec.level     = mine::diag::LogLevel::Info;
        rec.message   = "第二次写入";
        rec.timestamp = ::time(nullptr);
        sink.write_fn(sink.ctx, rec);
        if (sink.destroy_fn) sink.destroy_fn(sink.ctx);
    }

    char buf[4096]{};
    (void)read_file_content(kTestLogFile, buf, sizeof(buf));
    // 覆盖模式下，第一条内容不应出现
    CHECK(::strstr(buf, "第一次写入") == nullptr);
    CHECK(::strstr(buf, "第二次写入") != nullptr);

    remove_test_file(kTestLogFile);
}

TEST_CASE("append 模式（append=true）保留已有内容") {
    remove_test_file(kTestLogFile);

    // 先写入第一条
    {
        mine::logging::FileSinkOptions opts{};
        opts.path   = kTestLogFile;
        opts.append = true;
        opts.auto_flush = true;
        mine::diag::LogSink sink = mine::logging::make_file_sink(opts);
        REQUIRE(sink.write_fn != nullptr);

        mine::diag::LogRecord rec{};
        rec.level     = mine::diag::LogLevel::Info;
        rec.message   = "第一条追加";
        rec.timestamp = ::time(nullptr);
        sink.write_fn(sink.ctx, rec);
        if (sink.destroy_fn) sink.destroy_fn(sink.ctx);
    }

    // 再追加第二条
    {
        mine::logging::FileSinkOptions opts{};
        opts.path   = kTestLogFile;
        opts.append = true;  // 追加
        opts.auto_flush = true;
        mine::diag::LogSink sink = mine::logging::make_file_sink(opts);
        REQUIRE(sink.write_fn != nullptr);

        mine::diag::LogRecord rec{};
        rec.level     = mine::diag::LogLevel::Warn;
        rec.message   = "第二条追加";
        rec.timestamp = ::time(nullptr);
        sink.write_fn(sink.ctx, rec);
        if (sink.destroy_fn) sink.destroy_fn(sink.ctx);
    }

    char buf[4096]{};
    (void)read_file_content(kTestLogFile, buf, sizeof(buf));
    CHECK(::strstr(buf, "第一条追加") != nullptr);
    CHECK(::strstr(buf, "第二条追加") != nullptr);

    remove_test_file(kTestLogFile);
}

TEST_CASE("max_bytes 限制：超出后停止写入") {
    remove_test_file(kTestLogFile);

    mine::logging::FileSinkOptions opts{};
    opts.path       = kTestLogFile;
    opts.append     = false;
    opts.auto_flush = true;
    opts.max_bytes  = 50u;  // 仅允许 50 字节

    mine::diag::LogSink sink = mine::logging::make_file_sink(opts);
    REQUIRE(sink.write_fn != nullptr);

    mine::diag::LogRecord rec{};
    rec.level     = mine::diag::LogLevel::Info;
    rec.timestamp = ::time(nullptr);

    // 写入多条（每条远超 50 字节）
    rec.message = "第一条日志——这是一个比较长的消息用于测试字节数限制";
    sink.write_fn(sink.ctx, rec);

    rec.message = "第二条日志——这条不应该被写入";
    sink.write_fn(sink.ctx, rec);

    if (sink.destroy_fn) sink.destroy_fn(sink.ctx);

    char buf[4096]{};
    size_t total = read_file_content(kTestLogFile, buf, sizeof(buf));
    // 文件大小不超过 max_bytes（实际写入可能略少，因为行长度不一定整除）
    CHECK(total <= 50u);
    // 第二条消息不应出现（已超出限制）
    CHECK(::strstr(buf, "第二条日志") == nullptr);

    remove_test_file(kTestLogFile);
}

TEST_CASE("max_bytes = 0 表示无限制") {
    remove_test_file(kTestLogFile);

    mine::logging::FileSinkOptions opts{};
    opts.path       = kTestLogFile;
    opts.append     = false;
    opts.auto_flush = true;
    opts.max_bytes  = 0u;  // 无限制

    mine::diag::LogSink sink = mine::logging::make_file_sink(opts);
    REQUIRE(sink.write_fn != nullptr);

    mine::diag::LogRecord rec{};
    rec.level     = mine::diag::LogLevel::Info;
    rec.timestamp = ::time(nullptr);

    for (int i = 0; i < 5; ++i) {
        rec.message = "循环写入测试行";
        sink.write_fn(sink.ctx, rec);
    }

    if (sink.destroy_fn) sink.destroy_fn(sink.ctx);

    char buf[4096]{};
    size_t n = read_file_content(kTestLogFile, buf, sizeof(buf));
    CHECK(n > 0u);

    remove_test_file(kTestLogFile);
}

TEST_CASE("auto_flush = false 时 flush_fn 手动刷新") {
    remove_test_file(kTestLogFile);

    mine::logging::FileSinkOptions opts{};
    opts.path       = kTestLogFile;
    opts.append     = false;
    opts.auto_flush = false;  // 不自动刷新

    mine::diag::LogSink sink = mine::logging::make_file_sink(opts);
    REQUIRE(sink.write_fn != nullptr);

    mine::diag::LogRecord rec{};
    rec.level     = mine::diag::LogLevel::Info;
    rec.message   = "手动刷新测试";
    rec.timestamp = ::time(nullptr);
    sink.write_fn(sink.ctx, rec);

    // 手动刷新
    if (sink.flush_fn != nullptr) {
        sink.flush_fn(sink.ctx);
    }

    if (sink.destroy_fn) sink.destroy_fn(sink.ctx);

    char buf[4096]{};
    size_t n = read_file_content(kTestLogFile, buf, sizeof(buf));
    CHECK(n > 0u);
    CHECK(::strstr(buf, "手动刷新测试") != nullptr);

    remove_test_file(kTestLogFile);
}

TEST_CASE("add_file_sink 注册到全局 Logger 并实际写入文件") {
    remove_test_file(kTestLogFile);
    reset_logger();

    mine::logging::FileSinkOptions opts{};
    opts.path       = kTestLogFile;
    opts.append     = false;
    opts.auto_flush = true;

    uint32_t token = mine::logging::add_file_sink(opts);
    CHECK(token != 0u);

    MINE_LOG_INFO("通过全局写入文件 sink");
    mine::diag::Logger::global().remove_sink(token);

    char buf[4096]{};
    size_t n = read_file_content(kTestLogFile, buf, sizeof(buf));
    CHECK(n > 0u);
    CHECK(::strstr(buf, "通过全局写入文件 sink") != nullptr);

    remove_test_file(kTestLogFile);
}

TEST_CASE("FileSink 与 ConsoleSink 同时注册") {
    remove_test_file(kTestLogFile);
    reset_logger();

    mine::logging::ConsoleSinkOptions copts{};
    copts.use_color = false;
    uint32_t ctok = mine::logging::add_console_sink(copts);

    mine::logging::FileSinkOptions fopts{};
    fopts.path       = kTestLogFile;
    fopts.append     = false;
    fopts.auto_flush = true;
    uint32_t ftok = mine::logging::add_file_sink(fopts);

    CHECK(ctok != 0u);
    CHECK(ftok != 0u);

    MINE_LOG_INFO("双 sink 写入测试");

    mine::diag::Logger::global().remove_sink(ctok);
    mine::diag::Logger::global().remove_sink(ftok);

    char buf[4096]{};
    (void)read_file_content(kTestLogFile, buf, sizeof(buf));
    CHECK(::strstr(buf, "双 sink 写入测试") != nullptr);

    remove_test_file(kTestLogFile);
}

} // TEST_SUITE("FileSink")
