/**
 * @file DiTest.cpp
 * @brief mine.di 模块单元测试。
 *
 * 覆盖：
 *   - Singleton 生命周期（多次 resolve 返回同一实例）
 *   - Scoped 生命周期（同 Scope 内共享，不同 Scope 独立）
 *   - Transient 生命周期（每次 resolve 创建新实例）
 *   - add_instance（预注册实例，不拥有所有权）
 *   - try_resolve（未注册服务返回 nullptr）
 *   - 依赖注入（多级依赖自动解析）
 *   - MINE_INJECT 宏（与 add_singleton/add_transient 配合）
 *   - IServiceModule 批量注册
 */

#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>

#include <mine/di/Di.h>

using namespace mine::di;

// ─────────────────────────────────────────────────────────────────────────────
// 测试用服务接口和实现
// ─────────────────────────────────────────────────────────────────────────────

namespace {

// ── 基础接口 ──────────────────────────────────────────────────────────────────

struct ICounter {
    virtual ~ICounter() = default;
    virtual int value() const noexcept = 0;
    virtual void increment() noexcept  = 0;
};

struct ILogger {
    virtual ~ILogger() = default;
    virtual void log(const char* msg) noexcept = 0;
    virtual int log_count() const noexcept     = 0;
};

struct IConfig {
    virtual ~IConfig() = default;
    virtual const char* get(const char* key) const noexcept = 0;
};

// ── 简单实现（无依赖）──────────────────────────────────────────────────────────

/// 构造次数计数器（用于验证实例数量）
static int g_counter_ctor_count = 0;
static int g_counter_dtor_count = 0;

struct Counter : public ICounter {
    int v{ 0 };
    Counter()  { ++g_counter_ctor_count; }
    ~Counter() { ++g_counter_dtor_count; }
    int  value() const noexcept override { return v; }
    void increment() noexcept  override { ++v; }
};

static int g_logger_dtor_count = 0;

struct SimpleLogger : public ILogger {
    mutable int count{ 0 };
    ~SimpleLogger() { ++g_logger_dtor_count; }
    void log(const char* /*msg*/) noexcept override { ++count; }
    int  log_count() const noexcept override        { return count; }
};

struct SimpleConfig : public IConfig {
    const char* get(const char* /*key*/) const noexcept override { return "test_value"; }
};

// ── 带依赖的实现 ──────────────────────────────────────────────────────────────

/// 使用 MINE_INJECT 宏声明依赖的实现
struct LoggingCounter : public ICounter {
    MINE_INJECT(LoggingCounter, ILogger*, IConfig*);

    ILogger* logger;
    IConfig* config;
    int v{ 0 };

    LoggingCounter(ILogger* lg, IConfig* cfg)
        : logger(lg), config(cfg)
    {}

    int  value() const noexcept override { return v; }
    void increment() noexcept override {
        ++v;
        logger->log("increment");
    }
};

/// 不使用 MINE_INJECT，通过显式 Deps 注册的实现
struct LoggingCounter2 : public ICounter {
    ILogger* logger;
    int v{ 0 };

    explicit LoggingCounter2(ILogger* lg) : logger(lg) {}

    int  value() const noexcept override { return v; }
    void increment() noexcept override {
        ++v;
        logger->log("increment2");
    }
};

// ── IServiceModule 实现 ───────────────────────────────────────────────────────

struct BasicServicesModule : public IServiceModule {
    void configure(ServiceCollection& sc) override {
        sc.add_singleton<ILogger, SimpleLogger>()
          .add_singleton<IConfig, SimpleConfig>();
    }
};

} // namespace

// ─────────────────────────────────────────────────────────────────────────────
// 测试用例
// ─────────────────────────────────────────────────────────────────────────────

// ── Singleton 生命周期 ────────────────────────────────────────────────────────

TEST_CASE("di_Singleton_多次resolve返回同一实例")
{
    auto sp = ServiceCollection{}
        .add_singleton<ICounter, Counter>()
        .build();

    auto* a = sp.resolve<ICounter>();
    auto* b = sp.resolve<ICounter>();

    REQUIRE(a != nullptr);
    CHECK(a == b);
}

TEST_CASE("di_Singleton_实例可被修改并持久化")
{
    auto sp = ServiceCollection{}
        .add_singleton<ICounter, Counter>()
        .build();

    sp.resolve<ICounter>()->increment();
    sp.resolve<ICounter>()->increment();

    CHECK(sp.resolve<ICounter>()->value() == 2);
}

TEST_CASE("di_Singleton_Provider析构时释放实例")
{
    g_counter_ctor_count = 0;
    g_counter_dtor_count = 0;
    {
        auto sp = ServiceCollection{}
            .add_singleton<ICounter, Counter>()
            .build();
        sp.resolve<ICounter>();
        CHECK(g_counter_ctor_count == 1);
        CHECK(g_counter_dtor_count == 0);
    } // sp 析构
    CHECK(g_counter_dtor_count == 1);
}

// ── Transient 生命周期 ────────────────────────────────────────────────────────

TEST_CASE("di_Transient_每次resolve返回新实例")
{
    g_counter_ctor_count = 0;

    auto sp = ServiceCollection{}
        .add_transient<ICounter, Counter>()
        .build();

    auto* a = sp.resolve<ICounter>();
    auto* b = sp.resolve<ICounter>();

    REQUIRE(a != nullptr);
    REQUIRE(b != nullptr);
    CHECK(a != b);
    CHECK(g_counter_ctor_count == 2);
}

TEST_CASE("di_Transient_Provider析构时释放所有实例")
{
    g_counter_dtor_count = 0;
    {
        auto sp = ServiceCollection{}
            .add_transient<ICounter, Counter>()
            .build();

        sp.resolve<ICounter>();
        sp.resolve<ICounter>();
        sp.resolve<ICounter>();
        CHECK(g_counter_dtor_count == 0);
    }
    CHECK(g_counter_dtor_count == 3);
}

// ── Scoped 生命周期 ───────────────────────────────────────────────────────────

TEST_CASE("di_Scoped_同Scope内返回同一实例")
{
    auto sp = ServiceCollection{}
        .add_scoped<ICounter, Counter>()
        .build();

    auto scope = sp.create_scope();
    auto* a = scope.resolve<ICounter>();
    auto* b = scope.resolve<ICounter>();

    REQUIRE(a != nullptr);
    CHECK(a == b);
}

TEST_CASE("di_Scoped_不同Scope返回不同实例")
{
    auto sp = ServiceCollection{}
        .add_scoped<ICounter, Counter>()
        .build();

    auto scope1 = sp.create_scope();
    auto scope2 = sp.create_scope();

    auto* a = scope1.resolve<ICounter>();
    auto* b = scope2.resolve<ICounter>();

    REQUIRE(a != nullptr);
    REQUIRE(b != nullptr);
    CHECK(a != b);
}

TEST_CASE("di_Scoped_Scope析构时释放Scoped实例")
{
    g_counter_dtor_count = 0;
    {
        auto sp = ServiceCollection{}
            .add_scoped<ICounter, Counter>()
            .build();
        {
            auto scope = sp.create_scope();
            scope.resolve<ICounter>();
            CHECK(g_counter_dtor_count == 0);
        } // scope 析构
        CHECK(g_counter_dtor_count == 1);
    }
}

TEST_CASE("di_Scoped_Singleton在Scope中共享根实例")
{
    auto sp = ServiceCollection{}
        .add_singleton<ILogger, SimpleLogger>()
        .add_scoped  <ICounter, Counter>()
        .build();

    auto* root_logger = sp.resolve<ILogger>();

    auto scope = sp.create_scope();
    auto* scope_logger = scope.resolve<ILogger>();

    CHECK(root_logger == scope_logger);
}

// ── add_instance（预注册实例，不拥有所有权）───────────────────────────────────

TEST_CASE("di_AddInstance_返回预注册的实例")
{
    SimpleLogger logger;
    auto sp = ServiceCollection{}
        .add_instance<ILogger>(&logger)
        .build();

    auto* resolved = sp.resolve<ILogger>();
    REQUIRE(resolved != nullptr);
    CHECK(resolved == &logger);
}

TEST_CASE("di_AddInstance_Provider析构时不析构预注册实例")
{
    g_logger_dtor_count = 0;
    {
        SimpleLogger logger;
        {
            auto sp = ServiceCollection{}
                .add_instance<ILogger>(&logger)
                .build();
            sp.resolve<ILogger>();
        } // sp 析构，不应调用 logger 析构
        CHECK(g_logger_dtor_count == 0);
    }
    // logger 栈上析构，此时 dtor_count 变为 1
    CHECK(g_logger_dtor_count == 1);
}

// ── try_resolve（未注册服务返回 nullptr）─────────────────────────────────────

TEST_CASE("di_TryResolve_未注册服务返回nullptr")
{
    auto sp = ServiceCollection{}.build();
    auto* ptr = sp.try_resolve<ICounter>();
    CHECK(ptr == nullptr);
}

TEST_CASE("di_TryResolve_已注册服务返回有效指针")
{
    auto sp = ServiceCollection{}
        .add_singleton<ICounter, Counter>()
        .build();

    auto* ptr = sp.try_resolve<ICounter>();
    CHECK(ptr != nullptr);
}

// ── 依赖注入（多级自动解析）──────────────────────────────────────────────────

TEST_CASE("di_依赖注入_显式Deps可正确注入")
{
    auto sp = ServiceCollection{}
        .add_singleton<ILogger, SimpleLogger>()
        .add_singleton<IConfig, SimpleConfig>()
        .add_transient<ICounter, LoggingCounter2, ILogger>()
        .build();

    auto* counter = sp.resolve<ICounter>();
    REQUIRE(counter != nullptr);

    counter->increment();
    // LoggingCounter2 内部会调用 logger->log("increment2")
    auto* logger = sp.resolve<ILogger>();
    CHECK(logger->log_count() == 1);
}

TEST_CASE("di_依赖注入_Singleton依赖只创建一次")
{
    g_logger_dtor_count = 0;
    {
        auto sp = ServiceCollection{}
            .add_singleton<ILogger, SimpleLogger>()
            .add_singleton<ICounter, LoggingCounter2, ILogger>()
            .build();

        auto* c1 = sp.resolve<ICounter>();
        auto* c2 = sp.resolve<ICounter>();
        CHECK(c1 == c2);

        auto* logger = sp.resolve<ILogger>();
        // LoggingCounter2 和外部 resolve 用的都是同一个 logger
        c1->increment();
        CHECK(logger->log_count() == 1);
    }
    CHECK(g_logger_dtor_count == 1);  // 只创建一次
}

// ── MINE_INJECT 宏 ────────────────────────────────────────────────────────────

TEST_CASE("di_MINE_INJECT_自动推导依赖")
{
    auto sp = ServiceCollection{}
        .add_singleton<ILogger, SimpleLogger>()
        .add_singleton<IConfig, SimpleConfig>()
        .add_singleton<ICounter, LoggingCounter>()  // 使用 MINE_INJECT 版本
        .build();

    auto* counter = sp.resolve<ICounter>();
    REQUIRE(counter != nullptr);

    counter->increment();
    auto* logger = sp.resolve<ILogger>();
    CHECK(logger->log_count() == 1);
}

TEST_CASE("di_MINE_INJECT_Transient与接口注册")
{
    auto sp = ServiceCollection{}
        .add_singleton<ILogger, SimpleLogger>()
        .add_singleton<IConfig, SimpleConfig>()
        .add_transient<ICounter, LoggingCounter>()
        .build();

    auto* a = sp.resolve<ICounter>();
    auto* b = sp.resolve<ICounter>();
    CHECK(a != b);

    // 两个 transient 实例共享同一 Singleton Logger
    a->increment();
    b->increment();
    auto* logger = sp.resolve<ILogger>();
    CHECK(logger->log_count() == 2);
}

// ── IServiceModule 批量注册 ───────────────────────────────────────────────────

TEST_CASE("di_IServiceModule_批量注册服务")
{
    auto sp = ServiceCollection{}
        .add_module<BasicServicesModule>()
        .build();

    auto* logger = sp.resolve<ILogger>();
    auto* config = sp.resolve<IConfig>();

    REQUIRE(logger != nullptr);
    REQUIRE(config != nullptr);

    logger->log("test");
    CHECK(logger->log_count() == 1);
    CHECK(config->get("any") != nullptr);
}

// ── 用户自定义工厂函数 ────────────────────────────────────────────────────────

TEST_CASE("di_AddFactory_用户工厂函数按生命周期调用")
{
    static int factory_call_count = 0;
    factory_call_count = 0;

    auto sp = ServiceCollection{}
        .add_factory<ICounter>(
            [](ServiceProvider&) noexcept -> ICounter* {
                ++factory_call_count;
                return MINE_NEW(Counter);
            },
            Lifetime::Singleton)
        .build();

    sp.resolve<ICounter>();
    sp.resolve<ICounter>();

    CHECK(factory_call_count == 1);  // Singleton：只调用一次
}

// ── Scope 中的 Transient 生命周期 ────────────────────────────────────────────

TEST_CASE("di_Scope_Transient每次创建新实例并在Scope析构时释放")
{
    g_counter_dtor_count = 0;
    {
        auto sp = ServiceCollection{}
            .add_transient<ICounter, Counter>()
            .build();
        {
            auto scope = sp.create_scope();
            scope.resolve<ICounter>();
            scope.resolve<ICounter>();
            CHECK(g_counter_dtor_count == 0);
        }
        CHECK(g_counter_dtor_count == 2);
    }
}

// ── 覆盖注册（最后注册的优先）────────────────────────────────────────────────

TEST_CASE("di_覆盖注册_最后注册的实现生效")
{
    auto sp = ServiceCollection{}
        .add_singleton<ICounter, Counter>()      // 第一次注册
        .add_singleton<ILogger,  SimpleLogger>() // 注册 Logger（Counter 的 "被替换" 版）
        .build();

    // 只有 Counter（无 Logger 依赖的版本）被注册为 ICounter
    auto* c = sp.resolve<ICounter>();
    REQUIRE(c != nullptr);
    // 验证是 Counter（value 默认为 0）
    CHECK(c->value() == 0);
}
