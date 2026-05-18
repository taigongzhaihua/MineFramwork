/**
 * @file D3D11Swapchain.h
 * @brief D3D11 交换链实现（ISwapchain 接口）。
 */

#pragma once

#include "D3D11Helpers.h"
#include "D3D11Texture.h"
#include <mine/gfx/ISwapchain.h>
#include <memory>

namespace mine::gfx::d3d11 {

/**
 * @brief ISwapchain 的 D3D11/DXGI 实现。
 *
 * 使用 DXGI 1.2+ 的 FLIP_DISCARD 交换模型（IDXGISwapChain1），
 * 支持可变刷新率（DXGI_FEATURE_PRESENT_ALLOW_TEARING）。
 *
 * 生命周期注意：
 *   - 构造时绑定到特定 HWND，销毁前窗口必须仍然有效。
 *   - resize() 前应确保 GPU 已完成当前帧（通过 IQueue::wait_idle()）。
 *   - 必须在调用 D3D11Device 之前销毁（DXGI 设备依赖关系）。
 */
class D3D11Swapchain final : public ISwapchain {
public:
    /**
     * @brief 构造 D3D11 交换链。
     * @param device         D3D11 设备（COM 对象，AddRef 保持）
     * @param immediate_ctx  立即上下文（借用引用，用于 resize 后的状态清理）
     * @param factory        DXGI 工厂（COM 对象，AddRef 保持）
     * @param desc           交换链描述符
     */
    D3D11Swapchain(
        ComPtr<ID3D11Device>    device,
        ID3D11DeviceContext*    immediate_ctx,
        ComPtr<IDXGIFactory2>   factory,
        const SwapchainDesc&    desc);

    ~D3D11Swapchain() override;

    D3D11Swapchain(const D3D11Swapchain&)            = delete;
    D3D11Swapchain& operator=(const D3D11Swapchain&) = delete;

    // ── ISwapchain 接口 ───────────────────────────────────────────────────

    void resize(uint32_t width, uint32_t height) override;
    void present()                               override;

    [[nodiscard]] ITexture*   current_render_target() noexcept override;
    [[nodiscard]] uint32_t    width()       const noexcept override { return width_; }
    [[nodiscard]] uint32_t    height()      const noexcept override { return height_; }
    [[nodiscard]] PixelFormat format()      const noexcept override { return format_; }
    [[nodiscard]] uint32_t    image_count() const noexcept override { return image_count_; }
    [[nodiscard]] Vsync       vsync()       const noexcept override { return vsync_; }

    /// 判断交换链是否初始化成功
    [[nodiscard]] bool is_valid() const noexcept { return swapchain_ != nullptr; }

private:
    /**
     * @brief 获取交换链后缓冲并创建 D3D11Texture 包装器。
     *
     * 在初始化和 resize() 后调用，更新 back_buffer_ 内容。
     * @return 成功返回 true
     */
    bool acquire_back_buffer();

    /**
     * @brief 释放后缓冲纹理和 RTV（resize 前必须调用）。
     *
     * D3D11/DXGI 规定：ResizeBuffers 前必须释放所有后缓冲引用。
     */
    void release_back_buffer();

private:
    ComPtr<ID3D11Device>         device_;
    ID3D11DeviceContext*         immediate_ctx_{nullptr}; ///< 借用引用（不拥有）
    ComPtr<IDXGIFactory2>        factory_;
    ComPtr<IDXGISwapChain1>      swapchain_;

    /// 后缓冲纹理包装器（生命周期与 swapchain_ 一致，resize 时重建）
    std::unique_ptr<D3D11Texture> back_buffer_;

    uint32_t    width_{0};
    uint32_t    height_{0};
    PixelFormat format_{PixelFormat::BGRA8_UNorm_sRGB};
    uint32_t    image_count_{3};
    Vsync       vsync_{Vsync::On};

    /// 是否支持可变刷新率（DXGI_FEATURE_PRESENT_ALLOW_TEARING）
    bool allow_tearing_{false};
};

} // namespace mine::gfx::d3d11
