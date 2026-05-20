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

#include <mine/platform/IWindow.h>
#include <mine/platform/IWindowEventSink.h>
#include <mine/platform/WindowEvent.h>
#include <mine/gfx/IDevice.h>
#include <mine/gfx/IQueue.h>
#include <mine/gfx/ISwapchain.h>
#include <mine/gfx/GfxTypes.h>
#include <mine/paint/IRenderer.h>
#include <mine/paint/Canvas.h>
#include <mine/paint/Brush.h>
#include <mine/ui/visual/UIElement.h>
#include <mine/core/Memory.h>
#include <mine/core/Assert.h>

#include <algorithm>  // std::max
#include <cmath>      // std::round

namespace mine::ui {

// ============================================================================
// Impl 内部实现（同时实现 IWindowEventSink）
// ============================================================================

struct Window::Impl : public platform::IWindowEventSink {
    // ── 引用（不持有所有权）──────────────────────────────────────────────────

    /// 平台原生窗口（生命周期由外部 Application 管理）
    platform::IWindow& native_window_;

    /// 图形设备（用于创建/重建交换链）
    gfx::IDevice& device_;

    /// 图形命令队列（resize 前 wait_idle）
    gfx::IQueue& queue_;

    /// 2D 渲染器（提交 Canvas DisplayList）
    paint::IRenderer& renderer_;

    // ── 自有资源 ─────────────────────────────────────────────────────────────

    /// 交换链（Window 独占所有权）
    core::OwnedPtr<gfx::ISwapchain> swapchain_;

    // ── 状态 ─────────────────────────────────────────────────────────────────

    /// 内容根元素（非拥有，nullptr 表示无内容）
    ui::UIElement* content_{nullptr};

    /// 窗口是否已关闭
    bool is_closed_{false};

    /// 当前 DPI 缩放因子（物理像素 / 逻辑像素）
    float dpi_scale_{1.0f};

    // ── 构造 ─────────────────────────────────────────────────────────────────

    Impl(platform::IWindow& native_window,
         gfx::IDevice&      device,
         gfx::IQueue&       queue,
         paint::IRenderer&  renderer)
        : native_window_(native_window)
        , device_(device)
        , queue_(queue)
        , renderer_(renderer)
    {
        // 计算初始 DPI 缩放因子
        dpi_scale_ = std::max(0.01f, native_window_.dpi() / 96.0f);

        // 根据当前逻辑尺寸计算物理像素尺寸，创建交换链
        const math::Size logical_size = native_window_.size();
        const auto phys_w = static_cast<uint32_t>(
            std::max(1.0f, std::round(logical_size.width  * dpi_scale_)));
        const auto phys_h = static_cast<uint32_t>(
            std::max(1.0f, std::round(logical_size.height * dpi_scale_)));

        gfx::SwapchainDesc sc_desc{};
        sc_desc.native_window = native_window_.native_handle().ptr;
        sc_desc.width         = phys_w;
        sc_desc.height        = phys_h;
        sc_desc.image_count   = 2;
        sc_desc.format        = gfx::PixelFormat::BGRA8_UNorm;
        sc_desc.vsync         = gfx::Vsync::On;

        swapchain_ = device_.create_swapchain(sc_desc);
        MINE_ASSERT_MSG(swapchain_ != nullptr, "mine.ui.window: 交换链创建失败");

        // 同步 DPI 缩放到渲染器
        renderer_.set_dpi_scale(dpi_scale_);

        // 订阅平台窗口事件
        native_window_.events().subscribe(this);
    }

    ~Impl()
    {
        // 在销毁交换链前先取消事件订阅
        native_window_.events().unsubscribe(this);
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
        queue_.wait_idle();
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
        renderer_.set_dpi_scale(dpi_scale_);

        // 以当前逻辑尺寸重新计算物理像素并 resize
        const math::Size logical_size = native_window_.size();
        const auto phys_w = static_cast<uint32_t>(
            std::max(1.0f, std::round(logical_size.width  * dpi_scale_)));
        const auto phys_h = static_cast<uint32_t>(
            std::max(1.0f, std::round(logical_size.height * dpi_scale_)));

        queue_.wait_idle();
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

        renderer_.begin_frame();
        renderer_.render(dl, back_buf);
        renderer_.end_frame();

        // 将后缓冲呈现到屏幕
        swapchain_->present();
    }
};

// ============================================================================
// Window 公共实现
// ============================================================================

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

    // 内容变化后立即执行布局与渲染
    if (!p_->is_closed_) {
        const math::Size logical_size = p_->native_window_.size();
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
    p_->native_window_.show();
}

void Window::hide()
{
    p_->native_window_.hide();
}

void Window::close()
{
    p_->native_window_.close();
}

void Window::set_title(core::StringView title)
{
    p_->native_window_.set_title(title);
}

void Window::set_size(math::Size size)
{
    p_->native_window_.set_size(size);
}

math::Size Window::size() const
{
    return p_->native_window_.size();
}

float Window::dpi() const
{
    return p_->native_window_.dpi();
}

bool Window::is_closed() const noexcept
{
    return p_->is_closed_;
}

// ── 平台窗口访问 ─────────────────────────────────────────────────────────────

platform::IWindow& Window::native_window() noexcept
{
    return p_->native_window_;
}

// ── 渲染 ─────────────────────────────────────────────────────────────────────

void Window::render()
{
    if (p_->is_closed_) return;

    const math::Size logical_size = p_->native_window_.size();
    p_->run_layout(logical_size);
    p_->do_render(logical_size);
}

} // namespace mine::ui
