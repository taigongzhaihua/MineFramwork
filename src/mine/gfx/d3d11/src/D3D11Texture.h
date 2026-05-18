/**
 * @file D3D11Texture.h
 * @brief D3D11 纹理资源实现（ITexture 接口）。
 */

#pragma once

#include "D3D11Helpers.h"
#include <mine/gfx/ITexture.h>

namespace mine::gfx::d3d11 {

/**
 * @brief D3D11 纹理资源，封装 ID3D11Texture2D 及其关联视图。
 *
 * 根据 TextureDesc::bind_flags 自动创建以下视图：
 *   - ShaderResource → ID3D11ShaderResourceView (SRV)
 *   - RenderTarget   → ID3D11RenderTargetView   (RTV)
 *   - DepthStencil   → ID3D11DepthStencilView   (DSV)
 *
 * 交换链后缓冲纹理由 D3D11Swapchain 内部创建，不通过 IDevice::create_texture()。
 */
class D3D11Texture final : public ITexture {
public:
    /**
     * @brief 从已有 ID3D11Texture2D 构造（交换链后缓冲专用）。
     * @param texture     已有的 D3D11 纹理对象
     * @param rtv         对应的渲染目标视图（交换链 RTV）
     * @param desc        描述符（宽/高/格式等）
     */
    D3D11Texture(
        ComPtr<ID3D11Texture2D>        texture,
        ComPtr<ID3D11RenderTargetView> rtv,
        const TextureDesc&             desc);

    /**
     * @brief 通用构造：从描述符和 D3D11 设备创建纹理。
     * @param device  D3D11 设备
     * @param desc    纹理描述符
     */
    D3D11Texture(ID3D11Device* device, const TextureDesc& desc);

    ~D3D11Texture() override = default;

    // 禁止拷贝
    D3D11Texture(const D3D11Texture&)            = delete;
    D3D11Texture& operator=(const D3D11Texture&) = delete;

    // ── ITexture 接口 ─────────────────────────────────────────────────────

    [[nodiscard]] const TextureDesc& desc() const noexcept override { return desc_; }

    // ── D3D11 内部访问器 ──────────────────────────────────────────────────

    [[nodiscard]] ID3D11Texture2D*         texture()  const noexcept { return texture_.Get(); }
    [[nodiscard]] ID3D11RenderTargetView*  rtv()      const noexcept { return rtv_.Get(); }
    [[nodiscard]] ID3D11DepthStencilView*  dsv()      const noexcept { return dsv_.Get(); }
    [[nodiscard]] ID3D11ShaderResourceView* srv()     const noexcept { return srv_.Get(); }

    /// 判断此纹理是否有效（创建成功）
    [[nodiscard]] bool is_valid() const noexcept { return texture_ != nullptr; }

private:
    TextureDesc                     desc_{};
    ComPtr<ID3D11Texture2D>         texture_;
    ComPtr<ID3D11RenderTargetView>  rtv_;
    ComPtr<ID3D11DepthStencilView>  dsv_;
    ComPtr<ID3D11ShaderResourceView> srv_;
};

} // namespace mine::gfx::d3d11
