/**
 * @file D3D11Device.h
 * @brief D3D11 设备实现（IDevice 接口）。
 */

#pragma once

#include "D3D11Helpers.h"
#include <mine/gfx/IDevice.h>
#include <mine/core/Memory.h>

namespace mine::gfx::d3d11 {

/**
 * @brief IDevice 的 D3D11 实现，封装 D3D11 设备和 DXGI 工厂。
 *
 * 初始化流程：
 *   1. 枚举 DXGI 适配器，选择性能最强的硬件适配器
 *   2. 调用 D3D11CreateDevice 创建 D3D11 设备（Feature Level 11.0+）
 *   3. 查询 IDXGIDevice → IDXGIAdapter → IDXGIFactory2 以创建交换链
 *
 * 线程安全：
 *   - D3D11 设备本身是线程安全的（所有 Create* 调用可并发）
 *   - 立即上下文（immediate_ctx_）不是线程安全的，不要在设备之外直接使用
 *   - IQueue::submit() 必须在渲染线程调用
 */
class D3D11DeviceImpl final : public IDevice {
public:
    /**
     * @brief 创建 D3D11 硬件设备。
     *
     * 构造后通过 is_valid() 判断是否成功。
     * 如果系统不支持 D3D11 Feature Level 11.0，构造失败。
     */
    D3D11DeviceImpl();
    ~D3D11DeviceImpl() override = default;

    D3D11DeviceImpl(const D3D11DeviceImpl&)            = delete;
    D3D11DeviceImpl& operator=(const D3D11DeviceImpl&) = delete;

    // ── IDevice 接口 ──────────────────────────────────────────────────────

    [[nodiscard]] mine::core::OwnedPtr<IQueue>       create_queue      (QueueType type)              override;
    [[nodiscard]] mine::core::OwnedPtr<ISwapchain>   create_swapchain  (const SwapchainDesc& desc)   override;
    [[nodiscard]] mine::core::OwnedPtr<IBuffer>      create_buffer     (const BufferDesc& desc, const void* initial_data = nullptr) override;
    [[nodiscard]] mine::core::OwnedPtr<ITexture>     create_texture    (const TextureDesc& desc)      override;
    [[nodiscard]] mine::core::OwnedPtr<IPipeline>    create_pipeline   (const PipelineDesc& desc)     override;
    [[nodiscard]] mine::core::OwnedPtr<ICommandList> create_command_list(QueueType type = QueueType::Graphics) override;
    [[nodiscard]] mine::core::OwnedPtr<IFence>       create_fence      (uint64_t initial_value = 0)   override;

    [[nodiscard]] Backend     backend()      const noexcept override { return Backend::D3D11; }
    [[nodiscard]] const char* adapter_name() const noexcept override { return adapter_name_; }

    // ── 内部访问器 ────────────────────────────────────────────────────────

    /// 设备是否创建成功
    [[nodiscard]] bool is_valid() const noexcept { return device_ != nullptr; }

    /// 获取原始 D3D11 设备（供后端内部使用）
    [[nodiscard]] ID3D11Device* get_raw_device() const noexcept { return device_.Get(); }

    /// 获取立即上下文（供 D3D11Queue / D3D11Fence 使用）
    [[nodiscard]] ID3D11DeviceContext* get_immediate_context() const noexcept {
        return immediate_ctx_.Get();
    }

    /// 获取 DXGI 工厂（供 D3D11Swapchain 创建交换链使用）
    [[nodiscard]] IDXGIFactory2* get_dxgi_factory() const noexcept {
        return factory_.Get();
    }

private:
    ComPtr<ID3D11Device>        device_;
    ComPtr<ID3D11DeviceContext> immediate_ctx_;
    ComPtr<IDXGIFactory2>       factory_;

    /// 适配器描述（UTF-8 编码，最长 128 字节，含终止符）
    char adapter_name_[128]{};
};

} // namespace mine::gfx::d3d11
