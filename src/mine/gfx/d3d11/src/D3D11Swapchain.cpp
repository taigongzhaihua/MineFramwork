/**
 * @file D3D11Swapchain.cpp
 * @brief D3D11/DXGI 交换链实现。
 */

#include "D3D11Swapchain.h"

namespace mine::gfx::d3d11 {

// ── 构造与析构 ────────────────────────────────────────────────────────────────

D3D11Swapchain::D3D11Swapchain(
    ComPtr<ID3D11Device>    device,
    ID3D11DeviceContext*    immediate_ctx,
    ComPtr<IDXGIFactory2>   factory,
    const SwapchainDesc&    desc)
    : device_(std::move(device))
    , immediate_ctx_(immediate_ctx)
    , factory_(std::move(factory))
    , width_(desc.width)
    , height_(desc.height)
    , format_(desc.format)
    , image_count_(desc.image_count > 0 ? desc.image_count : 2)
    , vsync_(desc.vsync) {

    HWND hwnd = reinterpret_cast<HWND>(desc.native_window);
    if (!hwnd) {
        OutputDebugStringA("[mine.gfx.d3d11] D3D11Swapchain: native_window 不是有效的 HWND\n");
        return;
    }

    // 检查是否支持可变刷新率（Tearing，即 G-Sync/FreeSync）
    {
        ComPtr<IDXGIFactory5> factory5;
        if (SUCCEEDED(factory_.As(&factory5))) {
            BOOL tearing = FALSE;
            factory5->CheckFeatureSupport(
                DXGI_FEATURE_PRESENT_ALLOW_TEARING, &tearing, sizeof(tearing));
            allow_tearing_ = (tearing == TRUE);
        }
    }

    // 交换链格式：使用无类型（TYPELESS）基础格式以支持 sRGB RTV
    const DXGI_FORMAT chain_format = swapchain_typeless_format(format_);

    DXGI_SWAP_CHAIN_DESC1 sc_desc{};
    sc_desc.Width       = width_;
    sc_desc.Height      = height_;
    sc_desc.Format      = chain_format;
    sc_desc.Stereo      = FALSE;
    sc_desc.SampleDesc  = {1, 0};           // FLIP 模型不支持 MSAA
    sc_desc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    sc_desc.BufferCount = image_count_;
    sc_desc.Scaling     = DXGI_SCALING_NONE;
    sc_desc.SwapEffect  = DXGI_SWAP_EFFECT_FLIP_DISCARD;
    sc_desc.AlphaMode   = DXGI_ALPHA_MODE_IGNORE;
    sc_desc.Flags       = allow_tearing_ ? DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING : 0;

    const HRESULT hr = factory_->CreateSwapChainForHwnd(
        device_.Get(),   // pDevice（D3D11 设备，DXGI 内部转为 DXGIDevice）
        hwnd,
        &sc_desc,
        nullptr,         // pFullscreenDesc（NULL=窗口模式）
        nullptr,         // pRestrictToOutput（NULL=不限制）
        &swapchain_);

    if (FAILED(hr)) {
        OutputDebugStringA("[mine.gfx.d3d11] D3D11Swapchain: CreateSwapChainForHwnd 失败\n");
        return;
    }

    // 禁止 DXGI 自动处理 Alt+Enter 全屏切换（由应用层控制）
    factory_->MakeWindowAssociation(hwnd, DXGI_MWA_NO_ALT_ENTER);

    // 获取后缓冲并创建纹理包装器
    if (!acquire_back_buffer()) {
        OutputDebugStringA("[mine.gfx.d3d11] D3D11Swapchain: 获取后缓冲失败\n");
        swapchain_.Reset();
    }
}

D3D11Swapchain::~D3D11Swapchain() {
    // 在销毁前必须确保 GPU 已完成所有工作
    // （调用方应在销毁前调用 IQueue::wait_idle()）
    release_back_buffer();
}

// ── resize ────────────────────────────────────────────────────────────────────

void D3D11Swapchain::resize(uint32_t width, uint32_t height) {
    if (!swapchain_ || (width == width_ && height == height_)) {
        return;
    }

    // 第一步：解除所有后缓冲引用，否则 ResizeBuffers 会返回 DXGI_ERROR_INVALID_CALL
    release_back_buffer();

    // 解除立即上下文中所有渲染目标绑定（防止 D3D11 设备删除警告）
    if (immediate_ctx_) {
        immediate_ctx_->OMSetRenderTargets(0, nullptr, nullptr);
        immediate_ctx_->Flush();
    }

    const UINT flags = allow_tearing_ ? DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING : 0;

    // 第二步：调整交换链缓冲大小（保留格式和缓冲数量）
    const HRESULT hr = swapchain_->ResizeBuffers(
        0,              // 0=保留当前缓冲数量
        width,
        height,
        DXGI_FORMAT_UNKNOWN, // 保留当前格式
        flags);

    if (FAILED(hr)) {
        OutputDebugStringA("[mine.gfx.d3d11] D3D11Swapchain::resize: ResizeBuffers 失败\n");
        return;
    }

    width_  = width;
    height_ = height;

    // 第三步：重新获取后缓冲
    if (!acquire_back_buffer()) {
        OutputDebugStringA("[mine.gfx.d3d11] D3D11Swapchain::resize: 重新获取后缓冲失败\n");
    }
}

// ── present ───────────────────────────────────────────────────────────────────

void D3D11Swapchain::present() {
    if (!swapchain_) {
        return;
    }

    // vsync=Off 且硬件支持 Tearing 时，使用 PRESENT_ALLOW_TEARING 实现无撕裂的不等待呈现
    const UINT sync_interval = (vsync_ == Vsync::Off) ? 0 : 1;
    const UINT present_flags =
        (vsync_ == Vsync::Off && allow_tearing_)
        ? DXGI_PRESENT_ALLOW_TEARING
        : 0;

    const HRESULT hr = swapchain_->Present(sync_interval, present_flags);

    if (hr == DXGI_ERROR_DEVICE_REMOVED || hr == DXGI_ERROR_DEVICE_RESET) {
        // 设备丢失（驱动崩溃、GPU 热插拔等），应触发设备重建
        OutputDebugStringA("[mine.gfx.d3d11] D3D11Swapchain::present: 设备丢失\n");
    } else if (FAILED(hr)) {
        OutputDebugStringA("[mine.gfx.d3d11] D3D11Swapchain::present: Present 失败\n");
    }
}

// ── current_render_target ─────────────────────────────────────────────────────

ITexture* D3D11Swapchain::current_render_target() noexcept {
    return back_buffer_.get();
}

// ── 私有方法 ──────────────────────────────────────────────────────────────────

bool D3D11Swapchain::acquire_back_buffer() {
    // 从交换链获取后缓冲纹理的 COM 接口
    ComPtr<ID3D11Texture2D> back_buffer_tex;
    HRESULT hr = swapchain_->GetBuffer(0, IID_PPV_ARGS(&back_buffer_tex));
    if (FAILED(hr)) {
        OutputDebugStringA("[mine.gfx.d3d11] D3D11Swapchain: GetBuffer 失败\n");
        return false;
    }

    // 创建 sRGB 格式的渲染目标视图
    // 交换链用 TYPELESS 格式，RTV 才指定 sRGB 格式，以保证正确的 sRGB 写入
    D3D11_RENDER_TARGET_VIEW_DESC rtv_desc{};
    rtv_desc.Format             = swapchain_rtv_format(format_);
    rtv_desc.ViewDimension      = D3D11_RTV_DIMENSION_TEXTURE2D;
    rtv_desc.Texture2D.MipSlice = 0;

    ComPtr<ID3D11RenderTargetView> rtv;
    hr = device_->CreateRenderTargetView(back_buffer_tex.Get(), &rtv_desc, &rtv);
    if (FAILED(hr)) {
        OutputDebugStringA("[mine.gfx.d3d11] D3D11Swapchain: CreateRenderTargetView 失败\n");
        return false;
    }

    // 构造 TextureDesc 以包装后缓冲
    TextureDesc tex_desc{};
    tex_desc.width      = width_;
    tex_desc.height     = height_;
    tex_desc.depth      = 1;
    tex_desc.mip_levels = 1;
    tex_desc.array_size = 1;
    tex_desc.format     = format_;
    tex_desc.bind_flags = TextureBindFlags::RenderTarget;

    // 用交换链专用构造函数包装（不重复创建 D3D11 资源）
    back_buffer_ = std::make_unique<D3D11Texture>(
        std::move(back_buffer_tex),
        std::move(rtv),
        tex_desc);

    return back_buffer_->is_valid();
}

void D3D11Swapchain::release_back_buffer() {
    back_buffer_.reset();
}

} // namespace mine::gfx::d3d11
