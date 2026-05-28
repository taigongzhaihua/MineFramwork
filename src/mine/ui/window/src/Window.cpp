/**
 * @file Window.cpp
 * @brief Window 实现 —— 平台窗口事件订阅、布局驱动与渲染提交。
 *
 * 实现职责：
 *   1. 构造时向 IWindow::events() 注册事件接收器（IWindowEventSink）
 *   2. Resized 事件 → queue.wait_idle() → swapchain.resize() → 布局 → 渲染
 *   3. DpiChanged 事件 → 更新 dpi_scale → 重设交换链物理尺寸 → 布局 → 渲染
 *   4. Closing / Closed 事件 → 置 is_closed_ 标志（禁止后续渲染）
 *   5. render()：Canvas 绘制背景 + 递归渲染视觉树 → IRenderer 提交 → Present
 */

#include <mine/ui/window/Window.h>
#include <mine/ui/window/WindowContext.h>

#include <mine/platform/IWindow.h>
#include <mine/platform/IWindowEventSink.h>
#include <mine/platform/WindowEvent.h>
#include <mine/platform/WindowDesc.h>
#include <mine/platform/WindowKind.h>
#include <mine/gfx/IDevice.h>
#include <mine/gfx/IQueue.h>
#include <mine/gfx/ISwapchain.h>
#include <mine/gfx/GfxTypes.h>
#include <mine/paint/IRenderer.h>
#include <mine/paint/Canvas.h>
#include <mine/paint/Brush.h>
#include <mine/ui/visual/UIElement.h>
#include <mine/ui/input/InputRouter.h>
#include <mine/core/Memory.h>
#include <mine/core/Assert.h>
#include <mine/containers/InlineString.h>

#include <algorithm>  // std::max
#include <cmath>      // std::round

namespace mine::ui {

// ============================================================================
// Impl 内部实现（同时实现 IWindowEventSink）
// ============================================================================

struct Window::Impl : public platform::IWindowEventSink {
    // ── pending 配置（无参构造后、show() 前有效）───────────────────────────────
    //    首次 show() 时用于构建 WindowDesc，之后不再使用。

    /// 窗口标题（pending 状态下缓存）
    containers::InlineString pending_title_;

    /// 客户区逻辑尺寸（pending 状态下缓存）
    math::Size pending_size_{800.0f, 600.0f};

    /// 是否允许用户调整大小（pending 状态下缓存）
    bool pending_resizable_{true};

    /// 是否由系统决定初始位置（pending 状态下缓存）
    bool pending_auto_position_{true};

    /// 窗口类型（pending 状态下缓存）
    platform::WindowKind pending_kind_{platform::WindowKind::Normal};

    /// 是否已完成懒初始化（绑定了原生窗口和图形资源）
    bool is_initialized_{false};

    // ── 非拥有指针（初始化后由外部管理生命周期）────────────────────────────────

    /// 平台原生窗口（生命周期：路径 A 由 owned_native_ 管理，路径 B 由 Application 管理）
    platform::IWindow* native_window_{nullptr};

    /// 图形设备（生命周期由 Application 管理）
    gfx::IDevice* device_{nullptr};

    /// 图形命令队列（生命周期由 Application 管理）
    gfx::IQueue* queue_{nullptr};

    /// 2D 渲染器（生命周期由 Application 管理）
    paint::IRenderer* renderer_{nullptr};

    // ── 自有资源 ─────────────────────────────────────────────────────────────

    /// 原生窗口所有权（仅路径 A：无参构造 + show() 创建时持有）
    core::OwnedPtr<platform::IWindow> owned_native_;

    /// 交换链（Window 独占所有权，析构时在 owned_native_ 之前销毁）
    core::OwnedPtr<gfx::ISwapchain> swapchain_;

    // ── 输入系统 ─────────────────────────────────────────────────────────────

    /// 内置输入路由器（自动将鼠标/键盘事件路由到视觉树）
    input::InputRouter router_;

    /// 输入事件处理完毕后的回调（由 Application 安装 tick_and_render）
    std::function<void()> post_input_fn_;

    // ── 状态 ─────────────────────────────────────────────────────────────────

    /// 内容根元素（非拥有，nullptr 表示无内容）
    ui::UIElement* content_{nullptr};

    /// 窗口是否已关闭
    bool is_closed_{false};

    /// 当前 DPI 缩放因子（物理像素 / 逻辑像素）
    float dpi_scale_{1.0f};

    // ── 构造 ─────────────────────────────────────────────────────────────────

    /// 默认构造（pending 状态，路径 A）
    Impl() = default;

    /// 带参构造（立即初始化，路径 B：Application::create_window 使用）
    Impl(platform::IWindow& native_window,
         gfx::IDevice&      device,
         gfx::IQueue&       queue,
         paint::IRenderer&  renderer)
    {
        initialize(native_window, device, queue, renderer);
    }

    /**
     * @brief 执行实际的图形资源绑定（由路径 A 的 show() 或路径 B 的构造函数调用）。
     *
     * 内部操作：
     *   1. 设置非拥有指针（native_window_, device_, queue_, renderer_）
     *   2. 计算初始 DPI 缩放因子
     *   3. 创建 ISwapchain（物理像素尺寸）
     *   4. 同步 DPI 到渲染器
     *   5. 订阅平台窗口事件
     */
    void initialize(platform::IWindow& native_window,
                    gfx::IDevice&      device,
                    gfx::IQueue&       queue,
                    paint::IRenderer&  renderer)
    {
        native_window_ = &native_window;
        device_        = &device;
        queue_         = &queue;
        renderer_      = &renderer;

        // 计算初始 DPI 缩放因子
        dpi_scale_ = std::max(0.01f, native_window_->dpi() / 96.0f);

        // 根据当前逻辑尺寸计算物理像素尺寸，创建交换链
        const math::Size logical_size = native_window_->size();
        const auto phys_w = static_cast<uint32_t>(
            std::max(1.0f, std::round(logical_size.width  * dpi_scale_)));
        const auto phys_h = static_cast<uint32_t>(
            std::max(1.0f, std::round(logical_size.height * dpi_scale_)));

        gfx::SwapchainDesc sc_desc{};
        sc_desc.native_window = native_window_->native_handle().ptr;
        sc_desc.width         = phys_w;
        sc_desc.height        = phys_h;
        sc_desc.image_count   = 2;
        sc_desc.format        = gfx::PixelFormat::BGRA8_UNorm;
        sc_desc.vsync         = gfx::Vsync::On;

        swapchain_ = device_->create_swapchain(sc_desc);
        MINE_ASSERT_MSG(swapchain_ != nullptr, "mine.ui.window: 交换链创建失败");

        // 同步 DPI 缩放到渲染器
        renderer_->set_dpi_scale(dpi_scale_);

        // 订阅平台窗口事件
        native_window_->events().subscribe(this);

        is_initialized_ = true;
    }

    ~Impl()
    {
        // 取消事件订阅（必须先于 swapchain_ 和 owned_native_ 的析构）
        if (native_window_) {
            native_window_->events().unsubscribe(this);
        }
        // swapchain_ 在 owned_native_ 之前析构（声明顺序靠前 → 析构顺序靠后 → 先析构），
        // 确保原生窗口销毁前交换链已释放。
    }

    // ── IWindowEventSink 实现 ──────────────────────────────────────────────

    void on_window_event(platform::WindowEvent& event) override
    {
        using Kind = platform::WindowEventKind;

        switch (event.kind) {
        case Kind::Resized:
            handle_resized(event.new_size);
            break;

        case Kind::DpiChanged:
            handle_dpi_changed(event.new_dpi, event.suggested_rect);
            break;

        case Kind::Closing:
            // M1.1 阶段：不拦截 Closing，始终允许关闭
            break;

        case Kind::Closed:
            is_closed_ = true;
            break;

        // 输入事件：转发给 InputRouter，完成后触发 post_input_fn_ 回调
        case Kind::MouseMove:
        case Kind::MouseDown:
        case Kind::MouseUp:
        case Kind::MouseWheel:
        case Kind::KeyDown:
        case Kind::KeyUp:
            router_.on_window_event(event);
            if (post_input_fn_) { post_input_fn_(); }
            break;

        default:
            break;
        }
    }

    // ── 事件处理器 ────────────────────────────────────────────────────────

    /**
     * @brief 处理窗口尺寸变化：等待 GPU 空闲 → 调整交换链 → 布局 → 渲染。
     * @param new_logical_size 新的逻辑像素客户区尺寸
     */
    void handle_resized(math::Size new_logical_size)
    {
        if (!swapchain_) return;

        // 换算为物理像素
        const auto phys_w = static_cast<uint32_t>(
            std::max(1.0f, std::round(new_logical_size.width  * dpi_scale_)));
        const auto phys_h = static_cast<uint32_t>(
            std::max(1.0f, std::round(new_logical_size.height * dpi_scale_)));

        // 等待 GPU 完成当前帧，然后调整交换链大小
        queue_->wait_idle();
        swapchain_->resize(phys_w, phys_h);

        // 布局 + 渲染
        run_layout(new_logical_size);
        do_render(new_logical_size);
    }

    /**
     * @brief 处理 DPI 变化：更新缩放因子 → 告知渲染器 → 调整交换链 → 布局 → 渲染。
     * @param new_dpi         新 DPI 值（系统报告）
     * @param suggested_rect  系统建议的新窗口矩形（屏幕坐标，逻辑像素）
     */
    void handle_dpi_changed(float new_dpi, math::Rect /*suggested_rect*/)
    {
        if (!swapchain_) return;

        // 更新缩放因子
        dpi_scale_ = std::max(0.01f, new_dpi / 96.0f);
        renderer_->set_dpi_scale(dpi_scale_);

        // 以当前逻辑尺寸重新计算物理像素并 resize
        const math::Size logical_size = native_window_->size();
        const auto phys_w = static_cast<uint32_t>(
            std::max(1.0f, std::round(logical_size.width  * dpi_scale_)));
        const auto phys_h = static_cast<uint32_t>(
            std::max(1.0f, std::round(logical_size.height * dpi_scale_)));

        queue_->wait_idle();
        swapchain_->resize(phys_w, phys_h);

        // 布局 + 渲染
        run_layout(logical_size);
        do_render(logical_size);
    }

    // ── 布局 ─────────────────────────────────────────────────────────────

    /**
     * @brief 驱动内容根的两遍布局（Measure + Arrange）。
     * @param available_logical 可用的逻辑像素尺寸
     */
    void run_layout(math::Size available_logical)
    {
        if (!content_) return;

        content_->measure(available_logical);
        content_->arrange(math::Rect{
            0.0f, 0.0f,
            available_logical.width,
            available_logical.height
        });
    }

    // ── 渲染 ─────────────────────────────────────────────────────────────

    /**
     * @brief 执行一帧渲染：填充背景 → 渲染视觉树 → 提交 → Present。
     * @param logical_size 当前逻辑像素窗口尺寸（用于背景填充坐标）
     */
    void do_render(math::Size logical_size)
    {
        if (is_closed_ || !swapchain_) return;

        gfx::ITexture* back_buf = swapchain_->current_render_target();
        if (!back_buf) return;

        // 构建 Canvas 绘制命令
        paint::Canvas canvas;

        // 深色背景（占满整个逻辑窗口区域）
        canvas.fill_rect(
            math::Rect{0.0f, 0.0f, logical_size.width, logical_size.height},
            paint::Brush::solid(math::Color{0.08f, 0.08f, 0.08f, 1.0f}));

        // 递归渲染视觉树
        if (content_) {
            content_->render_to_canvas(canvas);
        }

        // 获取 DisplayList 并提交到 GPU
        paint::DisplayList dl = canvas.end();

        renderer_->begin_frame();
        renderer_->render(dl, back_buf);
        renderer_->end_frame();

        // 将后缓冲呈现到屏幕
        swapchain_->present();
    }
};

// ============================================================================
// Window 公共实现
// ============================================================================

/// 无参构造（pending 状态，路径 A）
Window::Window()
    : p_{ core::make_pimpl<Impl>() }
{}

/// 带参构造（立即初始化，路径 B）
Window::Window(platform::IWindow& native_window,
               gfx::IDevice&      device,
               gfx::IQueue&       queue,
               paint::IRenderer&  renderer)
    : p_{ core::make_pimpl<Impl>(native_window, device, queue, renderer) }
{}

Window::~Window() = default;

// ── 内容根 ───────────────────────────────────────────────────────────────────

void Window::set_content(ui::UIElement* element)
{
    p_->content_ = element;

    // 同步到 InputRouter：命中测试起点和默认键盘焦点
    p_->router_.set_root(element);
    if (element) {
        p_->router_.set_keyboard_focus(element);
    }

    // 仅已初始化且未关闭时才触发立即布局与渲染
    if (p_->is_initialized_ && !p_->is_closed_) {
        const math::Size logical_size = p_->native_window_->size();
        p_->run_layout(logical_size);
        p_->do_render(logical_size);
    }
}

ui::UIElement* Window::content() const noexcept
{
    return p_->content_;
}

// ── 平台窗口委托 ─────────────────────────────────────────────────────────────

void Window::show()
{
    if (!p_->is_initialized_) {
        // 路径 A：首次 show() 触发懒初始化
        auto* ctx = get_application_window_context();
        MINE_ASSERT_MSG(ctx != nullptr,
            "Window::show() 在 Application 初始化前调用，"
            "请确保在 Application::on_startup() 回调中调用 show()");

        // 从 pending 属性构建 WindowDesc
        platform::WindowDesc desc{};
        desc.title          = core::StringView{p_->pending_title_.data(),
                                               p_->pending_title_.size()};
        desc.size           = p_->pending_size_;
        desc.resizable      = p_->pending_resizable_;
        desc.auto_position  = p_->pending_auto_position_;
        desc.kind           = p_->pending_kind_;
        desc.startup_hidden = true;  // 避免原生窗口创建时自动显示，由本函数最后显式 show

        // 创建原生窗口（路径 A：Window 自身持有所有权）
        p_->owned_native_ = ctx->create_native_window(desc);
        MINE_ASSERT_MSG(p_->owned_native_ != nullptr, "mine.ui.window: 原生窗口创建失败");

        // 绑定图形资源（创建交换链、订阅事件）
        p_->initialize(*p_->owned_native_, ctx->device(), ctx->queue(), ctx->renderer());

        // 通知 Application：安装 tick_and_render 回调、自动设为主窗口（如有必要）
        ctx->on_window_first_show(*this);
    }

    p_->native_window_->show();
}

void Window::hide()
{
    if (p_->is_initialized_) {
        p_->native_window_->hide();
    }
}

void Window::close()
{
    if (p_->is_initialized_) {
        p_->native_window_->close();
    }
}

void Window::set_title(core::StringView title)
{
    if (p_->is_initialized_) {
        p_->native_window_->set_title(title);
    } else {
        // pending 状态：缓存标题，首次 show() 时应用
        p_->pending_title_ = containers::InlineString{title};
    }
}

void Window::set_size(math::Size size)
{
    if (p_->is_initialized_) {
        p_->native_window_->set_size(size);
    } else {
        // pending 状态：缓存尺寸，首次 show() 时应用
        p_->pending_size_ = size;
    }
}

void Window::set_resizable(bool resizable)
{
    // 仅 pending 状态下有效（初始化后原生窗口不支持动态修改）
    if (!p_->is_initialized_) {
        p_->pending_resizable_ = resizable;
    }
}

void Window::set_auto_position(bool auto_position)
{
    // 仅 pending 状态下有效
    if (!p_->is_initialized_) {
        p_->pending_auto_position_ = auto_position;
    }
}

void Window::set_kind(platform::WindowKind kind)
{
    // 仅 pending 状态下有效
    if (!p_->is_initialized_) {
        p_->pending_kind_ = kind;
    }
}

math::Size Window::size() const
{
    if (p_->is_initialized_) {
        return p_->native_window_->size();
    }
    return p_->pending_size_;
}

float Window::dpi() const
{
    if (p_->is_initialized_) {
        return p_->native_window_->dpi();
    }
    return 96.0f;
}

bool Window::is_closed() const noexcept
{
    return p_->is_closed_;
}

// ── 平台窗口访问 ─────────────────────────────────────────────────────────────

platform::IWindow& Window::native_window() noexcept
{
    MINE_ASSERT_MSG(p_->is_initialized_,
        "Window::native_window() 在 pending 状态下不可用（尚未调用 show()）");
    return *p_->native_window_;
}

// ── 渲染 ─────────────────────────────────────────────────────────────────────

void Window::render()
{
    if (!p_->is_initialized_ || p_->is_closed_) return;

    const math::Size logical_size = p_->native_window_->size();
    p_->run_layout(logical_size);
    p_->do_render(logical_size);
}

// ── 输入路由 ───────────────────────────────────────────────────────────────────

input::InputRouter& Window::input_router() noexcept
{
    return p_->router_;
}

void Window::set_on_input_processed(std::function<void()> fn)
{
    p_->post_input_fn_ = std::move(fn);
}

} // namespace mine::ui
