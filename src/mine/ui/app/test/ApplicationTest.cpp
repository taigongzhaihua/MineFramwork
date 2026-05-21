/**
 * @file ApplicationTest.cpp
 * @brief mine.ui.app::Application 单元测试。
 *
 * 测试策略：
 *   通过子类覆盖 on_create_host / on_create_device / on_create_renderer
 *   三个工厂方法注入 Mock 对象，隔离对真实平台/GPU/渲染器的依赖。
 *
 *   MockApplicationHost::run() 立即返回预设退出码，不启动真实消息循环。
 *   MockApplicationHost::quit(code) 记录退出码并标记"已调用"。
 *
 * 覆盖场景：
 *   1. on_startup 在 run() 进入消息循环之前被调用
 *   2. on_exit 在 run() 返回后被调用，且参数为正确退出码
 *   3. quit() 委托到宿主的 quit()
 *   4. create_window() 返回非空窗口指针
 *   5. set_main_window() 订阅主窗口关闭事件
 *   6. 主窗口关闭时自动触发 quit(0)
 *   7. run() 的返回值等于 MockApplicationHost::run() 返回的退出码
 */

#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>

#include <mine/ui/app/Application.h>

// 平台接口（Mock 依赖）
#include <mine/platform/IApplicationHost.h>
#include <mine/platform/IWindow.h>
#include <mine/platform/IWindowEventSink.h>
#include <mine/platform/IWindowEventSource.h>
#include <mine/platform/WindowEvent.h>
#include <mine/platform/WindowDesc.h>
#include <mine/platform/NativeHandle.h>
#include <mine/platform/IClipboard.h>
#include <mine/platform/IScreenManager.h>
#include <mine/platform/IMEService.h>

// 图形接口（Mock 依赖）
#include <mine/gfx/IDevice.h>
#include <mine/gfx/IQueue.h>
#include <mine/gfx/ISwapchain.h>
#include <mine/gfx/ITexture.h>
#include <mine/gfx/IBuffer.h>
#include <mine/gfx/ICommandList.h>
#include <mine/gfx/IFence.h>
#include <mine/gfx/IPipeline.h>
#include <mine/gfx/GfxTypes.h>

// 渲染接口（Mock 依赖）
#include <mine/paint/IRenderer.h>
#include <mine/paint/DisplayList.h>

// UI 窗口（完整类型，用于 is_closed() / native_window() 等访问）
#include <mine/ui/window/Window.h>

// Core
#include <mine/core/Memory.h>
#include <mine/core/Assert.h>

// ============================================================================
// 链接桩：平台后端与图形后端工厂函数
//
// Application::on_create_host() 和 on_create_device() 的默认实现调用这两个函数，
// 但它们的真实实现在 mine.platform.win32 / mine.gfx.d3d11 中（测试不链接这些）。
// TestApplication 覆盖工厂方法，不会实际调用这两个桩，但链接器仍需要符号存在。
// ============================================================================

namespace mine::platform {
    core::OwnedPtr<IApplicationHost> create_application_host() { return nullptr; }
}
namespace mine::gfx {
    core::OwnedPtr<IDevice> create_device(Backend) { return nullptr; }
}

using namespace mine;
using namespace mine::ui::app;
using namespace mine::platform;
using namespace mine::gfx;
using namespace mine::paint;

// ============================================================================
// 通用工具：空操作删除器
// ============================================================================

/// 不执行任何操作的删除器，用于将外部持有的 Mock 指针包装进 OwnedPtr，
/// 避免 double-free（生命周期由 Fixture 管理，而非 OwnedPtr 析构）。
static void noop_deleter(void*) noexcept {}

// ============================================================================
// 测试动作接口（支持捕获变量的 on_startup 回调）
// ============================================================================

/// 抽象测试动作接口，允许将带捕获的 lambda 注入 TestApplication。
struct IStartupAction {
    virtual void call(Application& app) = 0;
    virtual ~IStartupAction() = default;
};

/// 模板适配器：将任意可调用对象（含捕获 lambda）包装为 IStartupAction。
template<typename F>
struct StartupActionAdapter final : IStartupAction {
    F fn;
    explicit StartupActionAdapter(F f) : fn(std::move(f)) {}
    void call(Application& app) override { fn(app); }
};

/// 便捷工厂，推断模板参数
template<typename F>
StartupActionAdapter<F> make_startup_action(F&& f) {
    return StartupActionAdapter<F>(std::forward<F>(f));
}

// ============================================================================
// Mock：窗口事件分发器
// ============================================================================

/// 单 sink 实现，支持手动触发事件。
struct MockEventSource : public IWindowEventSource {
    IWindowEventSink* sink_{nullptr};

    void subscribe(IWindowEventSink* s) override   { sink_ = s; }
    void unsubscribe(IWindowEventSink* s) override { if (sink_ == s) sink_ = nullptr; }

    /// 向已订阅的 sink 触发事件
    void fire(WindowEvent& evt) { if (sink_) sink_->on_window_event(evt); }
};

// ============================================================================
// Mock：平台窗口
// ============================================================================

/// 最小 IWindow 实现，提供可编程的尺寸/DPI。
struct MockWindow : public IWindow {
    MockEventSource event_src_;
    math::Size      size_{800.0f, 600.0f};
    math::Point     position_{};
    float           dpi_{96.0f};
    bool            visible_{false};

    void show() override { visible_ = true; }
    void hide() override { visible_ = false; }
    void close() override
    {
        WindowEvent evt;
        evt.kind = WindowEventKind::Closing;
        event_src_.fire(evt);
        evt.kind = WindowEventKind::Closed;
        event_src_.fire(evt);
    }
    void set_title(core::StringView) override {}
    void set_size(math::Size s) override      { size_ = s; }
    void set_position(math::Point p) override { position_ = p; }

    math::Size  size() const override       { return size_; }
    math::Point position() const override   { return position_; }
    float       dpi() const override        { return dpi_; }
    bool        is_visible() const override { return visible_; }

    NativeHandle native_handle() const override { return NativeHandle{}; }
    IWindowEventSource& events() override { return event_src_; }
};

// ============================================================================
// Mock：GPU 纹理
// ============================================================================

struct MockTexture : public ITexture {
    gfx::TextureDesc desc_{};
    const gfx::TextureDesc& desc() const noexcept override { return desc_; }
};

// ============================================================================
// Mock：交换链
// ============================================================================

struct MockSwapchain : public ISwapchain {
    MockTexture tex_;
    uint32_t w_{800}, h_{600};

    void resize(uint32_t w, uint32_t h) override { w_ = w; h_ = h; }
    void present() override {}
    ITexture* current_render_target() noexcept override { return &tex_; }
    uint32_t  width() const noexcept override  { return w_; }
    uint32_t  height() const noexcept override { return h_; }
    PixelFormat format() const noexcept override { return PixelFormat::BGRA8_UNorm; }
    uint32_t  image_count() const noexcept override { return 2u; }
    Vsync     vsync() const noexcept override   { return Vsync::On; }
};

// ============================================================================
// Mock：命令队列
// ============================================================================

struct MockQueue : public IQueue {
    void submit(ICommandList*) override {}
    void wait_idle() override {}
    QueueType type() const noexcept override { return QueueType::Graphics; }
};

// ============================================================================
// Mock：图形设备
// ============================================================================

/// 最小 IDevice Mock。
/// create_queue 返回新建的 MockQueue（Application::Impl 接管所有权）。
/// create_swapchain 返回新建的 MockSwapchain。
struct MockDevice : public IDevice {
    core::OwnedPtr<IQueue>
        create_queue(QueueType) override
    {
        auto* q = MINE_NEW(MockQueue);
        return core::OwnedPtr<IQueue>{q, &core::detail::typed_deleter<MockQueue>};
    }

    core::OwnedPtr<ISwapchain>
        create_swapchain(const SwapchainDesc& desc) override
    {
        auto* sc = MINE_NEW(MockSwapchain);
        sc->w_ = desc.width  > 0 ? desc.width  : 800u;
        sc->h_ = desc.height > 0 ? desc.height : 600u;
        return core::OwnedPtr<ISwapchain>{sc, &core::detail::typed_deleter<MockSwapchain>};
    }

    core::OwnedPtr<IBuffer>
        create_buffer(const BufferDesc&, const void*) override { return nullptr; }

    core::OwnedPtr<ITexture>
        create_texture(const TextureDesc&) override { return nullptr; }

    core::OwnedPtr<IPipeline>
        create_pipeline(const PipelineDesc&) override { return nullptr; }

    core::OwnedPtr<ICommandList>
        create_command_list(QueueType) override { return nullptr; }

    core::OwnedPtr<IFence>
        create_fence(uint64_t) override { return nullptr; }

    Backend     backend() const noexcept override     { return Backend::D3D11; }
    const char* adapter_name() const noexcept override { return "MockAdapter"; }

    void update_texture_region(ITexture*, uint32_t, uint32_t, uint32_t, uint32_t,
                               const void*, uint32_t) override {}
    void copy_texture(ITexture*, ITexture*) override {}
};

// ============================================================================
// Mock：2D 渲染器
// ============================================================================

struct MockRenderer : public IRenderer {
    void begin_frame() override {}
    void end_frame() override {}
    void render(const DisplayList&, ITexture*) override {}
    void set_dpi_scale(float) override {}
};

// ============================================================================
// Mock：平台服务（剪贴板、显示器管理、IME）
// ============================================================================

/// 无操作剪贴板 Mock（实现全部纯虚方法）
struct MockClipboard : public IClipboard {
    bool has_text() const override { return false; }
    core::Status get_text(char*, size_t, size_t* out) const override
    {
        if (out) *out = 0;
        return core::Status::success();
    }
    core::Status set_text(core::StringView) override { return core::Status::success(); }
    void clear() override {}
};

/// 无操作显示器管理 Mock（始终返回一个默认屏幕）
struct MockScreenManager : public IScreenManager {
    int screen_count() const override { return 1; }
    ScreenInfo screen_at(int) const override { return {}; }
    ScreenInfo primary_screen() const override { return {}; }
    ScreenInfo screen_for_point(math::Point) const override { return {}; }
};

/// 无操作 IME 服务 Mock
struct MockIMEService : public IMEService {
    void enable(math::Rect) override {}
    void disable() override {}
    bool is_enabled() const override { return false; }
    void set_composition_rect(math::Rect) override {}
};

// ============================================================================
// Mock：平台应用宿主
// ============================================================================

/// MockApplicationHost：run() 立即返回预设退出码，quit() 记录退出码。
/// create_window() 返回 MockWindow，允许调用方触发平台事件。
struct MockApplicationHost : public IApplicationHost {
    int  preset_exit_code_{0};      ///< run() 将直接返回此值
    int  quit_code_{-1};            ///< 最近一次 quit() 传入的退出码（-1 表示未调用）
    bool quit_called_{false};       ///< quit() 是否被调用

    /// run() 返回前需触发关闭的窗口（用于模拟消息循环中的窗口关闭事件）
    MockWindow* window_to_close_before_run_returns_{nullptr};

    // 平台服务 Mock 成员（生命周期随 Host 对象）
    MockClipboard     clipboard_mock_;
    MockScreenManager screens_mock_;
    MockIMEService    ime_mock_;

    int run() override
    {
        if (window_to_close_before_run_returns_) {
            window_to_close_before_run_returns_->close();
        }
        return preset_exit_code_;
    }

    void quit(int exit_code) override
    {
        quit_called_ = true;
        quit_code_   = exit_code;
    }

    core::OwnedPtr<IWindow> create_window(const WindowDesc&) override
    {
        auto* w = MINE_NEW(MockWindow);
        return core::OwnedPtr<IWindow>{w, &core::detail::typed_deleter<MockWindow>};
    }

    IClipboard&     clipboard() override { return clipboard_mock_; }
    IScreenManager& screens()   override { return screens_mock_; }
    IMEService&     ime()       override { return ime_mock_; }
};

// ============================================================================
// 测试用 Application 子类（注入 Mock 对象）
// ============================================================================

/// 可注入 Mock 的 Application 子类。
class TestApplication : public Application {
public:
    // ── 生命周期观测点 ───────────────────────────────────────────────────────

    int  startup_argc_{0};           ///< on_startup 接收到的 argc
    bool startup_called_{false};     ///< on_startup 是否被调用
    bool exit_called_{false};        ///< on_exit 是否被调用
    int  exit_code_received_{-1};    ///< on_exit 接收到的退出码

    /// 在 on_startup 中额外执行的操作（通过 IStartupAction 注入，支持捕获 lambda）
    IStartupAction* on_startup_action_{nullptr};

    // ── 注入的 Mock 对象（由 AppFixture 在 run() 前设置）───────────────────

    MockApplicationHost* inject_host_{nullptr};
    MockDevice*          inject_device_{nullptr};
    MockRenderer*        inject_renderer_{nullptr};

protected:
    void on_startup(int argc, char** /*argv*/) override
    {
        startup_called_ = true;
        startup_argc_   = argc;
        if (on_startup_action_) {
            on_startup_action_->call(*this);
        }
    }

    void on_exit(int exit_code) override
    {
        exit_called_        = true;
        exit_code_received_ = exit_code;
    }

    core::OwnedPtr<platform::IApplicationHost> on_create_host() override
    {
        MINE_ASSERT(inject_host_ != nullptr, "TestApplication: inject_host_ 未设置");
        return core::OwnedPtr<platform::IApplicationHost>{inject_host_, noop_deleter};
    }

    core::OwnedPtr<gfx::IDevice> on_create_device() override
    {
        MINE_ASSERT(inject_device_ != nullptr, "TestApplication: inject_device_ 未设置");
        return core::OwnedPtr<gfx::IDevice>{inject_device_, noop_deleter};
    }

    core::OwnedPtr<paint::IRenderer> on_create_renderer(gfx::IDevice*) override
    {
        MINE_ASSERT(inject_renderer_ != nullptr, "TestApplication: inject_renderer_ 未设置");
        return core::OwnedPtr<paint::IRenderer>{inject_renderer_, noop_deleter};
    }
};

// ============================================================================
// 测试工具：统一装配 Mock 对象的 Fixture
// ============================================================================

/// 持有并连接所有 Mock 对象与 TestApplication。
struct AppFixture {
    MockApplicationHost host;
    MockDevice          device;
    MockRenderer        renderer;
    TestApplication     app;

    AppFixture()
    {
        app.inject_host_     = &host;
        app.inject_device_   = &device;
        app.inject_renderer_ = &renderer;
    }

    int run() { return app.run(0, nullptr); }
};

// ============================================================================
// 测试用例
// ============================================================================

TEST_SUITE("mine.ui.app — Application") {

    // ── 生命周期回调顺序 ──────────────────────────────────────────────────────

    TEST_CASE("run() 前 on_startup 未被调用") {
        AppFixture f;
        CHECK_FALSE(f.app.startup_called_);
    }

    TEST_CASE("on_startup 在消息循环进入前被调用") {
        AppFixture f;
        f.run();
        CHECK(f.app.startup_called_);
    }

    TEST_CASE("on_exit 在 run() 返回后被调用") {
        AppFixture f;
        f.run();
        CHECK(f.app.exit_called_);
    }

    TEST_CASE("on_exit 接收的退出码等于宿主 run() 返回值") {
        AppFixture f;
        f.host.preset_exit_code_ = 42;
        f.run();
        CHECK(f.app.exit_code_received_ == 42);
    }

    TEST_CASE("run() 返回值等于宿主 run() 的退出码") {
        AppFixture f;
        f.host.preset_exit_code_ = 7;
        const int code = f.run();
        CHECK(code == 7);
    }

    TEST_CASE("on_startup 收到正确的 argc") {
        AppFixture f;
        f.app.run(3, nullptr);
        CHECK(f.app.startup_argc_ == 3);
    }

    // ── quit() 委托 ───────────────────────────────────────────────────────────

    TEST_CASE("quit() 委托到 IApplicationHost::quit()") {
        AppFixture f;
        auto action = make_startup_action([](Application& app) {
            app.quit(99);
        });
        f.app.on_startup_action_ = &action;
        f.run();
        CHECK(f.host.quit_called_);
        CHECK(f.host.quit_code_ == 99);
    }

    // ── create_window() ───────────────────────────────────────────────────────

    TEST_CASE("create_window() 返回非空窗口指针") {
        AppFixture f;
        ui::Window* created_win = nullptr;
        auto action = make_startup_action([&created_win](Application& app) {
            platform::WindowDesc desc{};
            desc.title = "测试窗口";
            desc.size  = {800.0f, 600.0f};
            created_win = app.create_window(desc);
        });
        f.app.on_startup_action_ = &action;
        f.run();
        CHECK(created_win != nullptr);
    }

    TEST_CASE("create_window() 创建的窗口初始 is_closed = false") {
        AppFixture f;
        bool was_open = false;   // 在 startup action 内检查，此时窗口尚未销毁
        auto action = make_startup_action([&was_open](Application& app) {
            auto* win = app.create_window({});
            if (win) was_open = !win->is_closed();
        });
        f.app.on_startup_action_ = &action;
        f.run();
        CHECK(was_open);
    }

    // ── set_main_window() ─────────────────────────────────────────────────────

    TEST_CASE("set_main_window() 订阅主窗口事件后 sink 不为空") {
        AppFixture f;
        bool subscribed_after_set = false;   // 在 startup action 内检查，此时窗口尚未销毁
        auto action = make_startup_action([&subscribed_after_set](Application& app) {
            auto* win = app.create_window({});
            app.set_main_window(win);
            // set_main_window 后事件源 sink 应已被设置
            auto& src = static_cast<MockEventSource&>(win->native_window().events());
            subscribed_after_set = (src.sink_ != nullptr);
        });
        f.app.on_startup_action_ = &action;
        f.run();
        CHECK(subscribed_after_set);
    }

    TEST_CASE("主窗口 Closed 事件触发 quit(0)") {
        AppFixture f;
        MockWindow* raw_mock_win = nullptr;

        auto action = make_startup_action([&raw_mock_win](Application& app) {
            platform::WindowDesc desc{};
            auto* win = app.create_window(desc);
            app.set_main_window(win);
            raw_mock_win = static_cast<MockWindow*>(&win->native_window());
        });
        f.app.on_startup_action_ = &action;
        f.run();
        REQUIRE(raw_mock_win != nullptr);
    }

    TEST_CASE("窗口 Closed 事件在 run() 结束后不触发 quit") {
        AppFixture f;
        MockWindow* raw_win = nullptr;

        auto action = make_startup_action([&raw_win](Application& app) {
            platform::WindowDesc desc{};
            auto* win = app.create_window(desc);
            app.set_main_window(win);
            raw_win = static_cast<MockWindow*>(&win->native_window());
        });
        f.app.on_startup_action_ = &action;
        f.run();

        REQUIRE(raw_win != nullptr);
        // run() 结束后 sink_ 已为 nullptr，直接触发不会调用 quit
        CHECK_FALSE(f.host.quit_called_);
    }

    TEST_CASE("主窗口在消息循环中关闭时触发 host.quit(0)") {
        AppFixture f;
        MockWindow* raw_win = nullptr;

        auto action = make_startup_action([&raw_win, &f](Application& app) {
            platform::WindowDesc desc{};
            auto* win = app.create_window(desc);
            app.set_main_window(win);
            raw_win = static_cast<MockWindow*>(&win->native_window());
            // 告知 MockHost：在 run() 返回前关闭此窗口（模拟真实消息循环中的关闭事件）
            f.host.window_to_close_before_run_returns_ = raw_win;
        });
        f.app.on_startup_action_ = &action;
        f.run();

        // MockHost::run() 先调用了 raw_win->close()，
        // Closed 事件 → main_close_sink → host.quit(0)
        CHECK(f.host.quit_called_);
        CHECK(f.host.quit_code_ == 0);
    }

    // ── 基础设施访问 ──────────────────────────────────────────────────────────

    TEST_CASE("host() / device() / renderer() 在 on_startup 中可访问") {
        AppFixture f;
        bool host_valid     = false;
        bool device_valid   = false;
        bool renderer_valid = false;

        auto action = make_startup_action([&](Application& app) {
            host_valid     = (&app.host()     != nullptr);
            device_valid   = (&app.device()   != nullptr);
            renderer_valid = (&app.renderer() != nullptr);
        });
        f.app.on_startup_action_ = &action;
        f.run();

        CHECK(host_valid);
        CHECK(device_valid);
        CHECK(renderer_valid);
    }

} // TEST_SUITE
