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

// 样式系统（主题资源字典）
#include <mine/ui/style/ResourceDictionary.h>

// Core
#include <mine/core/Memory.h>
#include <mine/core/Assert.h>
#include <mine/core/Pimpl.h>

// 容器
#include <mine/containers/SmallVector.h>
#include <mine/containers/InlineString.h>

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

    /// 所有已创建的窗口列表（Application 拥有生命周期）
    containers::SmallVector<WindowEntry, 4> windows_;

    // ── 主窗口语义 ───────────────────────────────────────────────────────────

    /// 主窗口（非拥有指针，指向 windows_ 中某个 ui_win.get()）
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

    // ── 步骤 5：调用应用启动回调 ─────────────────────────────────────────────
    on_startup(argc, argv);

    // ── 步骤 6：进入主消息循环 ───────────────────────────────────────────────
    const int exit_code = p_->host_->run();

    // ── 步骤 7：调用应用退出回调 ─────────────────────────────────────────────
    on_exit(exit_code);

    // ── 步骤 8：清理（先取消主窗口事件订阅，再销毁窗口列表）─────────────────
    if (p_->main_window_ && !p_->main_window_->is_closed()) {
        p_->main_window_->native_window().events().unsubscribe(&p_->main_close_sink_);
    }
    p_->main_window_ = nullptr;

    // 清空窗口列表时，每个 WindowEntry 按 ui_win → native 顺序析构
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

} // namespace mine::ui::app
