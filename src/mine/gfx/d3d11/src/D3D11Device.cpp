/**
 * @file D3D11Device.cpp
 * @brief D3D11 设备实现。
 */

#include "D3D11Device.h"
#include "D3D11Queue.h"
#include "D3D11Swapchain.h"
#include "D3D11Buffer.h"
#include "D3D11Texture.h"
#include "D3D11CommandList.h"
#include "D3D11Fence.h"
#include "D3D11Pipeline.h"

#include <mine/core/Memory.h>
#include <iterator> // std::size

// 链接 D3D11 / DXGI 系统库（通过 xmake add_syslinks 声明，此处注释说明）
// #pragma comment(lib, "d3d11.lib")
// #pragma comment(lib, "dxgi.lib")

namespace mine::gfx::d3d11 {

// ── 工具函数：WCHAR 转 UTF-8（最多 max_bytes 字节） ─────────────────────────

static void wchar_to_utf8(const wchar_t* src, char* dst, int max_bytes) noexcept {
    const int result = ::WideCharToMultiByte(
        CP_UTF8, 0, src, -1, dst, max_bytes, nullptr, nullptr);
    if (result <= 0) {
        dst[0] = '\0';
    }
}

// ── 构造 ──────────────────────────────────────────────────────────────────────

D3D11DeviceImpl::D3D11DeviceImpl() {
    // 设备创建标志
    UINT create_flags = 0;
#if defined(_DEBUG)
    // Debug 模式下启用 D3D11 调试层（可在 PIX/RenderDoc 中查看详细错误）
    create_flags |= D3D11_CREATE_DEVICE_FLAG::D3D11_CREATE_DEVICE_DEBUG;
#endif

    // 优先选择 Feature Level 11.1，回退到 11.0（兼容较旧 GPU）
    const D3D_FEATURE_LEVEL feature_levels[] = {
        D3D_FEATURE_LEVEL_11_1,
        D3D_FEATURE_LEVEL_11_0,
    };

    D3D_FEATURE_LEVEL actual_level{};

    // 尝试使用硬件驱动创建设备（D3D_DRIVER_TYPE_HARDWARE）
    HRESULT hr = ::D3D11CreateDevice(
        nullptr,                           // pAdapter=NULL→使用默认适配器
        D3D_DRIVER_TYPE_HARDWARE,
        nullptr,                           // Software=NULL（硬件驱动）
        create_flags,
        feature_levels,
        static_cast<UINT>(std::size(feature_levels)),
        D3D11_SDK_VERSION,
        &device_,
        &actual_level,
        &immediate_ctx_);

    if (FAILED(hr)) {
        // 硬件适配器失败时，回退到 WARP 软件光栅化器（用于测试环境）
        OutputDebugStringA("[mine.gfx.d3d11] 硬件设备创建失败，尝试 WARP 软件渲染器\n");
        hr = ::D3D11CreateDevice(
            nullptr,
            D3D_DRIVER_TYPE_WARP,
            nullptr,
            create_flags,
            feature_levels,
            static_cast<UINT>(std::size(feature_levels)),
            D3D11_SDK_VERSION,
            &device_,
            &actual_level,
            &immediate_ctx_);
    }

    if (FAILED(hr)) {
        OutputDebugStringA("[mine.gfx.d3d11] D3D11CreateDevice 完全失败，此设备不可用\n");
        return;
    }

    // 从 D3D11 设备查询 DXGI 设备 → 适配器 → 父工厂（IDXGIFactory2）
    // 必须通过查询链获取，而非直接创建，否则与设备的 DXGI 对象不匹配
    ComPtr<IDXGIDevice>  dxgi_device;
    ComPtr<IDXGIAdapter> dxgi_adapter;

    hr = device_.As(&dxgi_device);
    if (FAILED(hr)) {
        OutputDebugStringA("[mine.gfx.d3d11] 查询 IDXGIDevice 失败\n");
        device_.Reset();
        immediate_ctx_.Reset();
        return;
    }

    hr = dxgi_device->GetAdapter(&dxgi_adapter);
    if (FAILED(hr)) {
        OutputDebugStringA("[mine.gfx.d3d11] GetAdapter 失败\n");
        device_.Reset();
        immediate_ctx_.Reset();
        return;
    }

    // 获取适配器描述（名称用于日志和调试）
    DXGI_ADAPTER_DESC adapter_desc{};
    if (SUCCEEDED(dxgi_adapter->GetDesc(&adapter_desc))) {
        wchar_to_utf8(adapter_desc.Description, adapter_name_, sizeof(adapter_name_));
    }

    // 获取 IDXGIFactory2（用于创建 IDXGISwapChain1）
    ComPtr<IDXGIFactory1> factory1;
    hr = dxgi_adapter->GetParent(IID_PPV_ARGS(&factory1));
    if (FAILED(hr)) {
        OutputDebugStringA("[mine.gfx.d3d11] GetParent(IDXGIFactory1) 失败\n");
        device_.Reset();
        immediate_ctx_.Reset();
        return;
    }

    hr = factory1.As(&factory_);
    if (FAILED(hr)) {
        OutputDebugStringA("[mine.gfx.d3d11] 查询 IDXGIFactory2 失败（系统可能不支持 DXGI 1.2）\n");
        device_.Reset();
        immediate_ctx_.Reset();
        return;
    }
}

// ── IDevice 接口实现 ──────────────────────────────────────────────────────────

mine::core::OwnedPtr<IQueue> D3D11DeviceImpl::create_queue(QueueType type) {
    if (!device_) return nullptr;

    // D3D11 只有一种命令流（立即上下文），所有类型队列共享同一个即时上下文
    // 不同 QueueType 在 M0 阶段仅作标记用途，实际提交均通过立即上下文
    auto* raw = MINE_NEW(D3D11Queue, immediate_ctx_.Get(), type);
    return mine::core::OwnedPtr<IQueue>(
        raw,
        &mine::core::detail::typed_deleter<D3D11Queue>);
}

mine::core::OwnedPtr<ISwapchain> D3D11DeviceImpl::create_swapchain(const SwapchainDesc& desc) {
    if (!device_ || !factory_) return nullptr;

    auto* raw = MINE_NEW(D3D11Swapchain, device_, immediate_ctx_.Get(), factory_, desc);

    if (!raw->is_valid()) {
        MINE_DELETE(raw);
        return nullptr;
    }

    return mine::core::OwnedPtr<ISwapchain>(
        raw,
        &mine::core::detail::typed_deleter<D3D11Swapchain>);
}

mine::core::OwnedPtr<IBuffer> D3D11DeviceImpl::create_buffer(
    const BufferDesc& desc, const void* initial_data) {
    if (!device_) return nullptr;

    auto* raw = MINE_NEW(D3D11Buffer, device_.Get(), desc, initial_data);

    if (!raw->is_valid()) {
        MINE_DELETE(raw);
        return nullptr;
    }

    return mine::core::OwnedPtr<IBuffer>(
        raw,
        &mine::core::detail::typed_deleter<D3D11Buffer>);
}

mine::core::OwnedPtr<ITexture> D3D11DeviceImpl::create_texture(const TextureDesc& desc) {
    if (!device_) return nullptr;

    auto* raw = MINE_NEW(D3D11Texture, device_.Get(), desc);

    if (!raw->is_valid()) {
        MINE_DELETE(raw);
        return nullptr;
    }

    return mine::core::OwnedPtr<ITexture>(
        raw,
        &mine::core::detail::typed_deleter<D3D11Texture>);
}

mine::core::OwnedPtr<ICommandList> D3D11DeviceImpl::create_command_list(QueueType /*type*/) {
    if (!device_) return nullptr;

    // 每个命令列表独占一个延迟上下文
    auto* raw = MINE_NEW(D3D11CommandList, device_.Get());

    return mine::core::OwnedPtr<ICommandList>(
        raw,
        &mine::core::detail::typed_deleter<D3D11CommandList>);
}

mine::core::OwnedPtr<IFence> D3D11DeviceImpl::create_fence(uint64_t initial_value) {
    if (!device_) return nullptr;

    auto* raw = MINE_NEW(D3D11Fence, device_.Get(), immediate_ctx_.Get(), initial_value);

    return mine::core::OwnedPtr<IFence>(
        raw,
        &mine::core::detail::typed_deleter<D3D11Fence>);
}

mine::core::OwnedPtr<IPipeline> D3D11DeviceImpl::create_pipeline(const PipelineDesc& desc) {
    if (!device_) return nullptr;

    // D3D11Pipeline 内部调用 D3DCompile 编译 HLSL 并创建所有 D3D11 状态对象
    auto* raw = MINE_NEW(D3D11Pipeline, device_.Get(), desc);

    if (!raw->is_valid()) {
        MINE_DELETE(raw);
        return nullptr;
    }

    return mine::core::OwnedPtr<IPipeline>(
        raw,
        &mine::core::detail::typed_deleter<D3D11Pipeline>);
}

} // namespace mine::gfx::d3d11
