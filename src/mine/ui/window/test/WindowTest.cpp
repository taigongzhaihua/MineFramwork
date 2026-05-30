/**
 * @file WindowTest.cpp
 * @brief mine.ui.window 模块单元测试。
 *
 * 使用 Mock 对象替代真实平台/GPU 资源，验证 Window 类的行为：
 *   - 构造与析构（交换链被创建）
 *   - 内容根管理（set_content / content）
 *   - 平台事件处理（Resized / DpiChanged / Closed）
 *   - 渲染流程正确调用 IRenderer
 *   - is_closed 标志在 Closed 事件后为 true
 *
 * 设计原则：
 *   - 所有 Mock 对象均在栈上或通过 make_owned 创建，无 GPU 依赖
 *   - MockDevice::create_swapchain 返回合法的 OwnedPtr<ISwapchain>
 *   - MockSwapchain 提供可用的 current_render_target()（返回 MockTexture*）
 */

#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>

#include <mine/ui/window/Window.h>

// 平台接口
#include <mine/platform/IWindow.h>
#include <mine/platform/IWindowEventSink.h>
#include <mine/platform/IWindowEventSource.h>
#include <mine/platform/WindowEvent.h>
#include <mine/platform/NativeHandle.h>

// GFX 接口
#include <mine/gfx/IDevice.h>
#include <mine/gfx/IQueue.h>
#include <mine/gfx/ISwapchain.h>
#include <mine/gfx/ITexture.h>
#include <mine/gfx/IBuffer.h>
#include <mine/gfx/ICommandList.h>
#include <mine/gfx/IFence.h>
#include <mine/gfx/IPipeline.h>
#include <mine/gfx/GfxTypes.h>

// Paint 接口
#include <mine/paint/IRenderer.h>
#include <mine/paint/DisplayList.h>

// UI Visual
#include <mine/ui/visual/UIElement.h>

// Core
#include <mine/core/Memory.h>

using namespace mine;
using namespace mine::ui;
using namespace mine::math;
using namespace mine::platform;
using namespace mine::gfx;
using namespace mine::paint;

// ============================================================================
// Mock：窗口事件分发器
// ============================================================================

/**
 * @brief 最简实现的 IWindowEventSource：单 sink，支持手动触发事件。
 */
struct MockWindowEventSource : public IWindowEventSource {
    IWindowEventSink* sink_{nullptr};

    void subscribe(IWindowEventSink* s) override
    {
        sink_ = s;
    }

    void unsubscribe(IWindowEventSink* s) override
    {
        if (sink_ == s) sink_ = nullptr;
    }

    /// 向已订阅的 sink 触发事件
    void fire(WindowEvent& evt)
    {
        if (sink_) sink_->on_window_event(evt);
    }
};

// ============================================================================
// Mock：平台窗口
// ============================================================================

/**
 * @brief 模拟 IWindow，提供可编程的尺寸/DPI，并记录 show/hide/close 调用次数。
 */
struct MockWindow : public IWindow {
    MockWindowEventSource event_src_;

    math::Size   size_{800.0f, 600.0f};
    math::Point  position_{};
    float        dpi_{96.0f};
    bool         visible_{false};

    int show_count_{0};
    int hide_count_{0};
    int close_count_{0};

    void show() override { visible_ = true; ++show_count_; }
    void hide() override { visible_ = false; ++hide_count_; }

    void close() override
    {
        ++close_count_;
        // 模拟 Closing → Closed 事件
        WindowEvent closing;
        closing.kind = WindowEventKind::Closing;
        event_src_.fire(closing);

        WindowEvent closed;
        closed.kind = WindowEventKind::Closed;
        event_src_.fire(closed);
    }

    void set_title(core::StringView) override {}
    void set_size(math::Size s) override { size_ = s; }
    void set_position(math::Point p) override { position_ = p; }

    math::Size  size() const override     { return size_; }
    math::Point position() const override { return position_; }
    float       dpi() const override      { return dpi_; }
    bool        is_visible() const override { return visible_; }

    NativeHandle native_handle() const override
    {
        // 测试中不需要真实句柄，返回 nullptr 即可
        return NativeHandle{};
    }

    IWindowEventSource& events() override { return event_src_; }
};

// ============================================================================
// Mock：GPU 纹理
// ============================================================================

/**
 * @brief 最小 ITexture Mock，持有一个固定的 TextureDesc。
 */
struct MockTexture : public ITexture {
    gfx::TextureDesc desc_{};

    const gfx::TextureDesc& desc() const noexcept override { return desc_; }
};

// ============================================================================
// Mock：交换链
// ============================================================================

/**
 * @brief 模拟交换链，记录 resize / present 调用，返回 MockTexture 作为后缓冲。
 */
struct MockSwapchain : public ISwapchain {
    MockTexture tex_;
    uint32_t    w_{800};
    uint32_t    h_{600};
    int         resize_count_{0};
    int         present_count_{0};

    void resize(uint32_t w, uint32_t h) override
    {
        w_ = w;
        h_ = h;
        ++resize_count_;
    }

    void present() override { ++present_count_; }

    ITexture* current_render_target() noexcept override { return &tex_; }

    uint32_t    width() const noexcept override      { return w_; }
    uint32_t    height() const noexcept override     { return h_; }
    PixelFormat format() const noexcept override     { return PixelFormat::BGRA8_UNorm; }
    uint32_t    image_count() const noexcept override { return 2u; }
    Vsync       vsync() const noexcept override       { return Vsync::On; }
};

// ============================================================================
// Mock：图形设备
// ============================================================================

/**
 * @brief 模拟 IDevice，create_swapchain 返回 MockSwapchain，其余方法返回 nullptr。
 */
struct MockDevice : public IDevice {
    /// 记录最近一次创建的 MockSwapchain 原始指针（由 OwnedPtr 管理生命周期）
    MockSwapchain* last_swapchain_{nullptr};

    core::OwnedPtr<IQueue> create_queue(QueueType) override
    {
        return nullptr;
    }

    core::OwnedPtr<ISwapchain> create_swapchain(const SwapchainDesc& desc) override
    {
        // 使用 make_owned 创建 MockSwapchain，初始化为 desc 中的尺寸
        auto* sc = MINE_NEW(MockSwapchain);
        sc->w_ = desc.width  > 0 ? desc.width  : 800u;
        sc->h_ = desc.height > 0 ? desc.height : 600u;
        last_swapchain_ = sc;
        // 返回 OwnedPtr<ISwapchain>，删除器使用 MockSwapchain 的类型擦除删除器
        return core::OwnedPtr<ISwapchain>{
            sc, &core::detail::typed_deleter<MockSwapchain>};
    }

    core::OwnedPtr<IBuffer> create_buffer(
        const BufferDesc&, const void*) override { return nullptr; }

    core::OwnedPtr<ITexture> create_texture(const TextureDesc&) override
    { return nullptr; }

    core::OwnedPtr<IPipeline> create_pipeline(const PipelineDesc&) override
    { return nullptr; }

    core::OwnedPtr<ICommandList> create_command_list(QueueType) override
    { return nullptr; }

    core::OwnedPtr<IFence> create_fence(uint64_t) override { return nullptr; }

    Backend     backend() const noexcept override     { return Backend::D3D11; }
    const char* adapter_name() const noexcept override { return "MockAdapter"; }

    void update_texture_region(
        ITexture*, uint32_t, uint32_t, uint32_t, uint32_t,
        const void*, uint32_t) override {}

    void copy_texture(ITexture*, ITexture*) override {}
};

// ============================================================================
// Mock：命令队列
// ============================================================================

/**
 * @brief 模拟 IQueue，记录 wait_idle 调用次数。
 */
struct MockQueue : public IQueue {
    int wait_idle_count_{0};

    void submit(ICommandList*) override {}
    void wait_idle() override { ++wait_idle_count_; }
    QueueType type() const noexcept override { return QueueType::Graphics; }
};

// ============================================================================
// Mock：2D 渲染器
// ============================================================================

/**
 * @brief 模拟 IRenderer，统计渲染调用次数。
 */
struct MockRenderer : public IRenderer {
    int  begin_count_{0};
    int  end_count_{0};
    int  render_count_{0};
    float last_dpi_scale_{1.0f};

    void begin_frame() override { ++begin_count_; }
    void end_frame() override   { ++end_count_; }

    void render(const DisplayList&, ITexture*) override { ++render_count_; }

    void set_dpi_scale(float scale) override { last_dpi_scale_ = scale; }
};

// ============================================================================
// 测试辅助：简单叶子 UIElement（记录 measure / arrange 被调用次数）
// ============================================================================

/**
 * @brief 可记录布局调用次数的测试专用 UIElement。
 */
class CountingElement : public UIElement {
public:
    int measure_count_{0};
    int arrange_count_{0};

    math::Size preset_{100.0f, 80.0f};

protected:
    void on_measure(math::Size /*available*/) override
    {
        ++measure_count_;
        set_desired_size(preset_);
    }

    void on_arrange(math::Rect /*final_rect*/) override
    {
        ++arrange_count_;
    }
};

/**
 * @brief 带单个子节点的测试元素，用于验证继承属性沿视觉树向下传播。
 */
class TreeElement : public UIElement {
public:
    CountingElement child_;

    TreeElement()
    {
        add_child(&child_);
    }

protected:
    void on_measure(math::Size available) override
    {
        child_.measure(available);
        set_desired_size(child_.desired_size());
    }

    void on_arrange(math::Rect final_rect) override
    {
        child_.arrange(final_rect);
    }
};

// ============================================================================
// 测试工具：构建 Window 所需的 Mock 对象集合
// ============================================================================

struct WindowFixture {
    MockWindow   window;
    MockDevice   device;
    MockQueue    queue;
    MockRenderer renderer;

    /// 便捷创建 Window（使用 make_owned 分配在堆上）
    core::OwnedPtr<Window> make_window()
    {
        return core::make_owned<Window>(window, device, queue, renderer);
    }
};

// ============================================================================
// 测试用例
// ============================================================================

TEST_SUITE("mine.ui.window — Window") {

    // ── 构造与析构 ───────────────────────────────────────────────────────────

    TEST_CASE("构造时创建交换链并订阅平台事件") {
        WindowFixture f;
        {
            auto win = f.make_window();

            // 交换链应被创建
            CHECK(f.device.last_swapchain_ != nullptr);

            // 窗口事件源应已有订阅者（Window::Impl 作为 IWindowEventSink）
            CHECK(f.window.event_src_.sink_ != nullptr);

            // 未调用 show/hide/close
            CHECK(f.window.show_count_ == 0);
        }
        // 析构时取消订阅
        CHECK(f.window.event_src_.sink_ == nullptr);
    }

    // ── 初始状态 ─────────────────────────────────────────────────────────────

    TEST_CASE("初始状态：is_closed = false，content = nullptr") {
        WindowFixture f;
        auto win = f.make_window();

        CHECK_FALSE(win->is_closed());
        CHECK(win->content() == nullptr);
    }

    TEST_CASE("DPI 缩放因子初始化到渲染器") {
        WindowFixture f;
        f.window.dpi_ = 192.0f;  // 200% DPI
        auto win = f.make_window();

        // 渲染器应收到 dpi_scale = 192/96 = 2.0
        CHECK(f.renderer.last_dpi_scale_ == doctest::Approx(2.0f));
    }

    // ── 内容根管理 ───────────────────────────────────────────────────────────

    TEST_CASE("set_content / content 基本读写") {
        WindowFixture f;
        auto win = f.make_window();

        CountingElement elem;
        win->set_content(&elem);

        CHECK(win->content() == &elem);
    }

    TEST_CASE("set_content(nullptr) 清除内容根") {
        WindowFixture f;
        auto win = f.make_window();

        CountingElement elem;
        win->set_content(&elem);
        win->set_content(nullptr);

        CHECK(win->content() == nullptr);
    }

    TEST_CASE("set_content 后立即触发布局") {
        WindowFixture f;
        auto win = f.make_window();

        CountingElement elem;
        win->set_content(&elem);

        // measure 和 arrange 各被调用一次
        CHECK(elem.measure_count_ >= 1);
        CHECK(elem.arrange_count_ >= 1);
    }

    TEST_CASE("set_content 后立即触发渲染") {
        WindowFixture f;
        auto win = f.make_window();

        int render_before = f.renderer.render_count_;
        CountingElement elem;
        win->set_content(&elem);

        // 渲染器应新增至少一次渲染调用
        CHECK(f.renderer.render_count_ > render_before);
    }

    TEST_CASE("set_data_context 在已有内容根时传播到整棵视觉子树") {
        WindowFixture f;
        auto win = f.make_window();

        TreeElement root;
        int value = 42;
        win->set_content(&root);
        win->set_data_context(core::Variant{ static_cast<void*>(&value) });

        REQUIRE(root.get_value(Window::DataContextProperty).has<void*>());
        CHECK(root.get_value(Window::DataContextProperty).get<void*>() == static_cast<void*>(&value));

        REQUIRE(root.child_.get_value(Window::DataContextProperty).has<void*>());
        CHECK(root.child_.get_value(Window::DataContextProperty).get<void*>() == static_cast<void*>(&value));
    }

    // ── 平台委托方法 ─────────────────────────────────────────────────────────

    TEST_CASE("show / hide / close 委托到 IWindow") {
        WindowFixture f;
        auto win = f.make_window();

        win->show();
        CHECK(f.window.show_count_ == 1);
        CHECK(f.window.visible_ == true);

        win->hide();
        CHECK(f.window.hide_count_ == 1);
        CHECK(f.window.visible_ == false);

        win->close();
        CHECK(f.window.close_count_ == 1);
    }

    TEST_CASE("size() 委托到 IWindow") {
        WindowFixture f;
        f.window.size_ = {1280.0f, 720.0f};
        auto win = f.make_window();

        const auto s = win->size();
        CHECK(s.width  == doctest::Approx(1280.0f));
        CHECK(s.height == doctest::Approx(720.0f));
    }

    TEST_CASE("dpi() 委托到 IWindow") {
        WindowFixture f;
        f.window.dpi_ = 144.0f;
        auto win = f.make_window();

        CHECK(win->dpi() == doctest::Approx(144.0f));
    }

    // ── 事件处理：Resized ─────────────────────────────────────────────────────

    TEST_CASE("Resized 事件：交换链 resize 并渲染") {
        WindowFixture f;
        auto win = f.make_window();

        // 记录初始 resize / render 调用次数
        int resize_before = f.device.last_swapchain_->resize_count_;
        int render_before = f.renderer.render_count_;
        int wait_before   = f.queue.wait_idle_count_;

        // 模拟平台发出 Resized 事件
        WindowEvent evt;
        evt.kind     = WindowEventKind::Resized;
        evt.new_size = {1024.0f, 768.0f};
        f.window.event_src_.fire(evt);

        // 应先 wait_idle，再 resize
        CHECK(f.queue.wait_idle_count_ > wait_before);
        CHECK(f.device.last_swapchain_->resize_count_ > resize_before);

        // 应触发渲染
        CHECK(f.renderer.render_count_ > render_before);
    }

    TEST_CASE("Resized 事件：布局以新尺寸重新执行") {
        WindowFixture f;
        auto win = f.make_window();

        CountingElement elem;
        win->set_content(&elem);

        int measure_before = elem.measure_count_;

        // 模拟 Resized 事件
        WindowEvent evt;
        evt.kind     = WindowEventKind::Resized;
        evt.new_size = {640.0f, 480.0f};
        f.window.event_src_.fire(evt);

        CHECK(elem.measure_count_ > measure_before);
    }

    // ── 事件处理：DpiChanged ──────────────────────────────────────────────────

    TEST_CASE("DpiChanged 事件：渲染器 dpi_scale 更新") {
        WindowFixture f;
        auto win = f.make_window();

        WindowEvent evt;
        evt.kind    = WindowEventKind::DpiChanged;
        evt.new_dpi = 192.0f;  // 200% DPI
        f.window.event_src_.fire(evt);

        CHECK(f.renderer.last_dpi_scale_ == doctest::Approx(2.0f));
    }

    TEST_CASE("DpiChanged 事件：交换链 resize（物理像素更新）") {
        WindowFixture f;
        f.window.size_ = {800.0f, 600.0f};  // 逻辑尺寸
        auto win = f.make_window();

        int resize_before = f.device.last_swapchain_->resize_count_;

        WindowEvent evt;
        evt.kind    = WindowEventKind::DpiChanged;
        evt.new_dpi = 192.0f;
        f.window.event_src_.fire(evt);

        CHECK(f.device.last_swapchain_->resize_count_ > resize_before);
    }

    // ── 事件处理：Closing / Closed ────────────────────────────────────────────

    TEST_CASE("Closed 事件：is_closed 变为 true") {
        WindowFixture f;
        auto win = f.make_window();

        CHECK_FALSE(win->is_closed());

        // 直接触发 Closed 事件（不经过 Closing）
        WindowEvent evt;
        evt.kind = WindowEventKind::Closed;
        f.window.event_src_.fire(evt);

        CHECK(win->is_closed());
    }

    TEST_CASE("close() 调用后 is_closed 为 true") {
        WindowFixture f;
        auto win = f.make_window();

        win->close();
        // MockWindow::close() 会依次触发 Closing → Closed 事件
        CHECK(win->is_closed());
    }

    // ── 渲染 ─────────────────────────────────────────────────────────────────

    TEST_CASE("render() 正确调用 begin_frame / render / end_frame") {
        WindowFixture f;
        auto win = f.make_window();

        int begin_before  = f.renderer.begin_count_;
        int render_before = f.renderer.render_count_;
        int end_before    = f.renderer.end_count_;

        win->render();

        CHECK(f.renderer.begin_count_  == begin_before  + 1);
        CHECK(f.renderer.render_count_ == render_before + 1);
        CHECK(f.renderer.end_count_    == end_before    + 1);
    }

    TEST_CASE("render() 调用后 present 被调用") {
        WindowFixture f;
        auto win = f.make_window();

        int present_before = f.device.last_swapchain_->present_count_;
        win->render();

        CHECK(f.device.last_swapchain_->present_count_ > present_before);
    }

    TEST_CASE("is_closed 时 render() 为空操作") {
        WindowFixture f;
        auto win = f.make_window();

        win->close();
        REQUIRE(win->is_closed());

        int render_before = f.renderer.render_count_;
        win->render();  // 应为空操作

        CHECK(f.renderer.render_count_ == render_before);
    }

    // ── native_window 访问 ───────────────────────────────────────────────────

    TEST_CASE("native_window() 返回构造时传入的 IWindow") {
        WindowFixture f;
        auto win = f.make_window();

        CHECK(&win->native_window() == &f.window);
    }

} // TEST_SUITE
