/**
 * @file Application.cpp
 * @brief mine.ui.app::Application 类实现。
 *
 * 实现要点：
 *   - Application::Impl 持有平台宿主、图形设备、渲染器及所有窗口的完整所有权。
 *   - 内部 MainWindowCloseSink 订阅主窗口原生事件源，在 Closed 事件时调用 host_->quit(0)。
 *   - WindowEntry 将原生 IWindow 与 ui::Window 配对持有，确保析构顺序正确。
 *   - create_window() 必须在 run() 启动基础设施之后调用（在 on_startup 中）。
 */

#include <mine/ui/app/Application.h>

// 平台接口
#include <mine/platform/PlatformHost.h>
#include <mine/platform/IApplicationHost.h>
#include <mine/platform/IWindow.h>
#include <mine/platform/IWindowEventSource.h>
#include <mine/platform/IWindowEventSink.h>
#include <mine/platform/WindowEvent.h>
#include <mine/platform/WindowDesc.h>

// 图形接口
#include <mine/gfx/Gfx.h>

// 渲染接口
#include <mine/paint/IRenderer.h>
#include <mine/paint/Paint.h>

// UI 窗口
#include <mine/ui/window/Window.h>
#include <mine/ui/window/WindowContext.h>

// 样式系统（主题资源字典）
#include <mine/ui/style/ResourceDictionary.h>

// 动画时钟（驱动 AnimationClock::tick_all 推进全部已注册动画）
#include <mine/ui/animation/AnimationClock.h>

// 字体（默认系统字体加载）
#include <mine/text/FontFace.h>

// Core
#include <mine/core/Memory.h>
#include <mine/core/Assert.h>
#include <mine/core/Pimpl.h>

// 容器
#include <mine/containers/SmallVector.h>
#include <mine/containers/InlineString.h>

// 平台时间（GetTickCount64不依赖具体平台 API，通过标准库获取）
#if defined(_WIN32)
#  ifndef WIN32_LEAN_AND_MEAN
#    define WIN32_LEAN_AND_MEAN
#  endif
#  include <windows.h>  // GetTickCount()
#endif

namespace mine::ui::app {

// ============================================================================
// 主窗口关闭监听器：Closed 事件 → 自动调用 host_->quit(0)
// ============================================================================

/**
 * @brief 内部事件接收器：监听主窗口的 Closed 事件以驱动自动退出。
 *
 * 实例由 Application::Impl 持有，subscribe/unsubscribe 配对管理。
 */
struct MainWindowCloseSink final : public platform::IWindowEventSink {
    /// 指向平台宿主（运行时设置，外部持有生命周期）
    platform::IApplicationHost* host_{nullptr};

    /**
     * @brief 接收平台窗口事件，在 Closed 时请求退出主循环。
     */
    void on_window_event(platform::WindowEvent& event) override
    {
        if (event.kind == platform::WindowEventKind::Closed && host_) {
            host_->quit(0);
        }
    }
};

// ============================================================================
// ============================================================================
// Application::Impl 实现
// ============================================================================

/**
 * @brief Application 的内部实现（Pimpl）。
 *
 * 持有生命周期：平台宿主 > 图形设备 > 渲染器 > 所有窗口。
 * 析构顺序与构造顺序相反，由 Pimpl 析构自动保证。
 */
struct Application::Impl {
    // ── 核心基础设施（按生命周期顺序排列）───────────────────────────────────

    /// 平台应用宿主（Win32/Cocoa/Wayland 实现）
    core::OwnedPtr<platform::IApplicationHost> host_;

    /// 图形设备（D3D11/D3D12/Vulkan 实现）
    core::OwnedPtr<gfx::IDevice>               device_;

    /// 图形命令队列（Graphics 类型）
    core::OwnedPtr<gfx::IQueue>                queue_;

    /// 2D 渲染器（基于 RHI）
    core::OwnedPtr<paint::IRenderer>           renderer_;

    // ── 窗口管理 ─────────────────────────────────────────────────────────────

    /**
     * @brief 窗口条目：将原生 IWindow 与 UI Window 配对持有。
     *
     * 析构顺序：ui_win 先析构（取消事件订阅、等待 GPU 空闲），
     * 随后 native 析构（销毁原生窗口），保证 ui::Window 使用的引用在其析构前有效。
     */
    struct WindowEntry {
        core::OwnedPtr<platform::IWindow> native;   ///< 平台原生窗口（被 ui_win 引用）
        core::OwnedPtr<ui::Window>        ui_win;   ///< UI 窗口包装（先析构）

        // 不可拷贝，支持移动
        WindowEntry() = default;
        WindowEntry(const WindowEntry&) = delete;
        WindowEntry& operator=(const WindowEntry&) = delete;
        WindowEntry(WindowEntry&&) = default;
        WindowEntry& operator=(WindowEntry&&) = default;
    };

    /// 路径 B 窗口：Application 持有 native + ui_win 完整生命周期
    containers::SmallVector<WindowEntry, 4> windows_;

    /// 路径 A 窗口（pending/show() 路径）：Application 不持有，仅追踪裸指针
    containers::SmallVector<ui::Window*, 4> tracked_windows_;

    // ── 主窗口语义 ───────────────────────────────────────────────────────────

    /// 主窗口（非拥有指针，路径 A/B 均可指向）
    ui::Window* main_window_{nullptr};

    /// 主窗口关闭监听器（订阅主窗口的原生事件源）
    MainWindowCloseSink main_close_sink_;

    // ── 主题 / 资源系统 ───────────────────────────────────────────────────────

    /**
     * @brief 堆分配的主题条目。
     *
     * dict 使用 OwnedPtr 堆分配，保证在 SmallVector 扩容重分配时地址稳定，
     * 从而使 global_resources_.merge(*dict) 产生的弱引用始终有效。
     */
    struct ThemeEntry {
        containers::InlineString                  name;   ///< 主题名（如 "Light"）
        core::OwnedPtr<style::ResourceDictionary> dict;   ///< 主题资源字典（堆分配）

        ThemeEntry() = default;
        ThemeEntry(const ThemeEntry&)            = delete;
        ThemeEntry& operator=(const ThemeEntry&) = delete;
        ThemeEntry(ThemeEntry&&) noexcept        = default;
        ThemeEntry& operator=(ThemeEntry&&) noexcept = default;
    };

    /// 已注册的主题列表（dict 堆分配，地址稳定）
    containers::SmallVector<ThemeEntry, 4> themes_;

    /// 当前激活的主题名（空串表示未激活）
    containers::InlineString               current_theme_name_;

    /// 应用级全局资源字典（根字典）
    style::ResourceDictionary              global_resources_;

    // ── 帧定时器状态（供 tick_and_render 使用）───────────────────────────────

    /// 上次 tick_all 的系统时间戳（ms，0 = 未初始化）
    unsigned long last_tick_ms_{0};
    // ── 默认字体 ──────────────────────────────────────────────────────────────

    /// 应用默认系统字体（run() 内在 on_startup 前追加载载）
    core::OwnedPtr<text::FontFace> default_font_;};

// ============================================================================
// 进程级窗口上下文：供 ui::Window::show() 懒初始化使用
// ============================================================================

/**
 * @brief 进程级窗口上下文实现，由 Application::run() 在栈上创建并注册为全局 context。
 *
 * 全部功能通过回调函数注入，不持有 Application::Impl 引用（后者是 private），
 * 避免访问控制问题的同时保持代码清晰。
 * 具体的 Impl 访问、protected tick_and_render 调用均由 run() 内的 lambda 封装。
 */
class ApplicationWindowContext final : public ui::IWindowContext {
    using CreateFn    = std::function<core::OwnedPtr<platform::IWindow>(const platform::WindowDesc&)>;
    using FirstShowFn = std::function<void(ui::Window&)>;

    CreateFn            create_fn_;      ///< 创建原生窗口回调（委托 host_->create_window）
    gfx::IDevice&       device_ref_;     ///< 图形设备引用
    gfx::IQueue&        queue_ref_;      ///< 图形命令队列引用
    paint::IRenderer&   renderer_ref_;   ///< 2D 渲染器引用
    platform::IMEService& ime_ref_;      ///< IME 服务引用
    FirstShowFn         first_show_fn_;  ///< 首次 show() 时的后处理回调

public:
    ApplicationWindowContext(CreateFn create, gfx::IDevice& dev, gfx::IQueue& q,
                             paint::IRenderer& r, platform::IMEService& ime, FirstShowFn first_show)
        : create_fn_{std::move(create)}, device_ref_{dev}, queue_ref_{q},
          renderer_ref_{r}, ime_ref_{ime}, first_show_fn_{std::move(first_show)}
    {}

    [[nodiscard]] core::OwnedPtr<platform::IWindow>
        create_native_window(const platform::WindowDesc& desc) override
    {
        return create_fn_(desc);
    }

    [[nodiscard]] gfx::IDevice& device() noexcept override { return device_ref_; }
    [[nodiscard]] gfx::IQueue& queue() noexcept override { return queue_ref_; }
    [[nodiscard]] paint::IRenderer& renderer() noexcept override { return renderer_ref_; }
    [[nodiscard]] platform::IMEService& ime() noexcept override { return ime_ref_; }

    void on_window_first_show(ui::Window& win) override { first_show_fn_(win); }
};

// ============================================================================
// Application 成员函数实现
// ============================================================================

Application::Application()
    : p_{core::make_pimpl<Impl>()}
{}
Application::~Application() = default;

int Application::run(int argc, char** argv)
{
    // ── 步骤 1：初始化平台宿主 ───────────────────────────────────────────────
    p_->host_ = on_create_host();
    MINE_ASSERT(p_->host_ != nullptr, "平台宿主创建失败");

    // ── 步骤 2：初始化图形设备 ───────────────────────────────────────────────
    p_->device_ = on_create_device();
    MINE_ASSERT(p_->device_ != nullptr, "图形设备创建失败（当前系统不支持所需图形 API）");

    // ── 步骤 3：初始化图形命令队列 ───────────────────────────────────────────
    p_->queue_ = p_->device_->create_queue(gfx::QueueType::Graphics);
    MINE_ASSERT(p_->queue_ != nullptr, "图形命令队列创建失败");

    // ── 步骤 4：初始化 2D 渲染器 ─────────────────────────────────────────────
    p_->renderer_ = on_create_renderer(p_->device_.get());
    MINE_ASSERT(p_->renderer_ != nullptr, "2D 渲染器创建失败（着色器编译失败？）");

    // ── 步骤 4.5：注册进程级窗口上下文（供 ui::Window::show() 懒初始化使用）──
    //   window_ctx 生命周期绑定到 run() 调用栈，退出时自动失效
    //   run() 是 Application 的成员函数，可合法访问 protected tick_and_render 和 private p_
    ApplicationWindowContext window_ctx{
        [&](const platform::WindowDesc& desc) { return p_->host_->create_window(desc); },
        *p_->device_, *p_->queue_, *p_->renderer_, p_->host_->ime(),
        [this](ui::Window& win) {
            // 安装输入后处理回调：推进动画并触发重绘（tick_and_render 是 protected）
            win.set_on_input_processed([this, &win]() { tick_and_render(&win); });
            // 追踪此窗口（路径 A：Window 是调用方成员，Application 不拥有生命周期）
            p_->tracked_windows_.push_back(&win);
            // 若尚无主窗口，自动将本窗口设为主窗口
            if (!p_->main_window_) { set_main_window(&win); }
        }
    };
    ui::set_application_window_context(&window_ctx);

    // ── 步骤 5：尝试加载默认系统字体（最优努力，失败则 default_font_ 为 nullptr）──
#if defined(_WIN32)
    {
        const char* candidates[] = {
            "C:\\Windows\\Fonts\\msyh.ttc",    // 微软雅黑（Windows 10/11）
            "C:\\Windows\\Fonts\\msyh.ttf",    // 微软雅黑（Windows 7/8）
            "C:\\Windows\\Fonts\\simhei.ttf",  // 黑体（备用中文字体）
            "C:\\Windows\\Fonts\\segoeui.ttf", // Segoe UI（西文回退）
            "C:\\Windows\\Fonts\\arial.ttf",   // Arial（西文回退）
        };
        for (const char* path : candidates) {
            p_->default_font_ = text::FontFace::load_from_file(path);
            if (p_->default_font_) { break; }
        }
    }
#endif

    // ── 步骤 6：调用应用启动回调 ────────────────────────────────────────────────
    on_startup(argc, argv);

    // ── 步骤 7：进入主消息循环 ──────────────────────────────────────────────
    const int exit_code = p_->host_->run();

    // ── 步骤 8：调用应用退出回调 ──────────────────────────────────────────────
    on_exit(exit_code);

    // ── 步骤 9：清理 ─────────────────────────────────────────────────────────

    // 先注销全局 context（防止清理过程中再次触发 Window::show()）
    ui::set_application_window_context(nullptr);

    // 取消主窗口关闭事件订阅（路径 A/B 均适用）
    if (p_->main_window_ && !p_->main_window_->is_closed()) {
        p_->main_window_->native_window().events().unsubscribe(&p_->main_close_sink_);
    }
    p_->main_window_ = nullptr;

    // 路径 A 窗口（Application 不拥有）：清空追踪指针列表即可
    p_->tracked_windows_.clear();

    // 路径 B 窗口（Application 拥有）：每个 WindowEntry 按 ui_win → native 顺序析构
    p_->windows_.clear();

    return exit_code;
}

void Application::quit(int exit_code)
{
    if (p_->host_) {
        p_->host_->quit(exit_code);
    }
}

ui::Window* Application::create_window(const platform::WindowDesc& desc)
{
    MINE_ASSERT(p_->host_   != nullptr, "create_window() 必须在 run() 内调用");
    MINE_ASSERT(p_->device_ != nullptr, "create_window() 必须在 run() 内调用");
    MINE_ASSERT(p_->queue_  != nullptr, "create_window() 必须在 run() 内调用");
    MINE_ASSERT(p_->renderer_ != nullptr, "create_window() 必须在 run() 内调用");

    // 创建原生窗口
    auto native = p_->host_->create_window(desc);
    MINE_ASSERT(native != nullptr, "原生窗口创建失败");

    // 保存原生窗口引用（在创建 ui::Window 之前，确保引用有效）
    platform::IWindow* native_raw = native.get();

    // 创建 UI 窗口包装（内部创建 ISwapchain 并订阅平台事件）
    auto ui_win = core::make_owned<ui::Window>(
        *native_raw,
        *p_->device_,
        *p_->queue_,
        *p_->renderer_);

    ui::Window* result = ui_win.get();

    // 将配对存入列表（native 须在 ui_win 之前构造，之后析构）
    Impl::WindowEntry entry;
    entry.native = std::move(native);
    entry.ui_win = std::move(ui_win);
    p_->windows_.push_back(std::move(entry));

    // 自动安装输入后处理回调：推进动画并触发重绘（业务代码不需手动设置）
    result->set_on_input_processed([this, result]() {
        tick_and_render(result);
    });

    return result;
}

void Application::set_main_window(ui::Window* window)
{
    // 若已有主窗口且未关闭，取消之前的事件订阅
    if (p_->main_window_ && !p_->main_window_->is_closed()) {
        p_->main_window_->native_window().events().unsubscribe(&p_->main_close_sink_);
    }

    p_->main_window_ = window;

    if (window) {
        // 将宿主引用传给 sink，使其可以调用 quit()
        p_->main_close_sink_.host_ = p_->host_.get();
        // 订阅新主窗口的原生事件源
        window->native_window().events().subscribe(&p_->main_close_sink_);
    } else {
        p_->main_close_sink_.host_ = nullptr;
    }
}

platform::IApplicationHost& Application::host()
{
    MINE_ASSERT(p_->host_ != nullptr, "host() 必须在 run() 之后调用");
    return *p_->host_;
}

gfx::IDevice& Application::device()
{
    MINE_ASSERT(p_->device_ != nullptr, "device() 必须在 run() 之后调用");
    return *p_->device_;
}

paint::IRenderer& Application::renderer()
{
    MINE_ASSERT(p_->renderer_ != nullptr, "renderer() 必须在 run() 之后调用");
    return *p_->renderer_;
}

// ── 生命周期扩展点默认实现（空操作）──────────────────────────────────────────

void Application::on_startup(int /*argc*/, char** /*argv*/) {}
void Application::on_exit(int /*exit_code*/) {}

// ── 工厂扩展点默认实现 ────────────────────────────────────────────────────────

core::OwnedPtr<platform::IApplicationHost> Application::on_create_host()
{
    return platform::create_application_host();
}

core::OwnedPtr<gfx::IDevice> Application::on_create_device()
{
    return gfx::create_device(gfx::Backend::Auto);
}

core::OwnedPtr<paint::IRenderer> Application::on_create_renderer(gfx::IDevice* device)
{
    return paint::create_renderer(device);
}

// ── 主题 / 资源系统 ───────────────────────────────────────────────────────────

void Application::register_theme(core::StringView               name,
                                  style::ResourceDictionary&&    theme_dict)
{
    // 构造主题名的 StringView，用于比较
    const core::StringView name_view{name.data(), name.size()};

    // 检查是否已存在同名主题，存在则覆盖
    for (auto& entry : p_->themes_) {
        if (entry.name == name_view) {
            // 将新内容移入已有的堆对象（地址不变，global_resources_ 的弱引用持续有效）
            *entry.dict = std::move(theme_dict);

            // 若该主题当前正在激活，重新广播全量更新通知
            const core::StringView cur_name{p_->current_theme_name_.data(),
                                            p_->current_theme_name_.size()};
            if (entry.name == cur_name) {
                p_->global_resources_.notify_resource_changed("*");
            }
            return;
        }
    }

    // 新主题：堆分配以保证地址稳定
    Impl::ThemeEntry entry;
    entry.name = containers::InlineString{name.data(), name.size()};
    entry.dict = core::make_owned<style::ResourceDictionary>(std::move(theme_dict));
    p_->themes_.push_back(std::move(entry));
}

bool Application::set_theme(core::StringView name) noexcept
{
    const core::StringView name_view{name.data(), name.size()};

    // 查找已注册主题
    for (const auto& entry : p_->themes_) {
        if (entry.name == name_view) {
            // 1. 清除全局字典的旧合并层
            p_->global_resources_.clear_merged();

            // 2. 合并新主题字典（弱引用，entry.dict 堆分配地址稳定）
            p_->global_resources_.merge(*entry.dict);

            // 3. 记录当前主题名
            p_->current_theme_name_ = containers::InlineString{name.data(), name.size()};

            // 4. 广播全量资源更新（DynamicResource 订阅者均会收到 key="*" 通知）
            p_->global_resources_.notify_resource_changed("*");

            return true;
        }
    }

    // 主题不存在，静默忽略
    return false;
}

core::StringView Application::current_theme() const noexcept
{
    return core::StringView{p_->current_theme_name_.data(),
                            p_->current_theme_name_.size()};
}

style::ResourceDictionary& Application::global_resources() noexcept
{
    return p_->global_resources_;
}

const style::ResourceDictionary& Application::global_resources() const noexcept
{
    return p_->global_resources_;
}

// ── 动画驱动辅助 ──────────────────────────────────────────────────────────────

void Application::tick_and_render(ui::Window* win)
{
    using Clock = animation::AnimationClock;

    if (Clock::instance().has_active()) {
        // ── 计算真实步长（dt），消除定时器精度误差 ──
        const auto now = static_cast<unsigned long>(GetTickCount());
        float dt;
        if (p_->last_tick_ms_ == 0 || now < p_->last_tick_ms_) {
            // 首帧或时钟回绕：使用默认步长（约 60fps）
            dt = 16.0f / 1000.0f;
        } else {
            dt = static_cast<float>(now - p_->last_tick_ms_) / 1000.0f;
            // 限制最大步长：防止调试断点或窗口最小化后大幅跳帧
            if (dt > 0.1f) { dt = 0.1f; }
        }
        p_->last_tick_ms_ = now;

        const bool still_active = Clock::instance().tick_all(dt);
        if (win) { win->render(); }

        if (!still_active) {
            // 所有动画完成：停止帧定时器并重置时间戳
            if (p_->host_) { p_->host_->stop_frame_timer(); }
            p_->last_tick_ms_ = 0;
        } else if (p_->host_) {
            // 仍有动画：确保帧定时器已启动（start_frame_timer 内部幂等）
            p_->host_->start_frame_timer(8u, [](void* ud) {
                auto* self = static_cast<Application*>(ud);
                // 定时器回调：推进动画并渲染主窗口（若存在）
                ui::Window* main_win = self->p_->main_window_;
                self->tick_and_render(main_win);
            }, this);
        }
    } else {
        // 无活跃动画：直接渲染，停止可能残留的帧定时器
        if (win) { win->render(); }
        if (p_->host_) { p_->host_->stop_frame_timer(); }
        p_->last_tick_ms_ = 0;
    }
}

// ── 字体 ──────────────────────────────────────────────────────────────────────

text::FontFace* Application::default_font() noexcept
{
    return p_->default_font_.get();
}

} // namespace mine::ui::app
