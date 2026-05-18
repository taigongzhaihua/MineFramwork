/**
 * @file D3D11Texture.cpp
 * @brief D3D11 纹理资源实现。
 */

#include "D3D11Texture.h"
#include <utility> // std::move

namespace mine::gfx::d3d11 {

// ── 交换链后缓冲构造（直接传入已有纹理和 RTV）────────────────────────────────

D3D11Texture::D3D11Texture(
    ComPtr<ID3D11Texture2D>        texture,
    ComPtr<ID3D11RenderTargetView> rtv,
    const TextureDesc&             desc)
    : desc_(desc)
    , texture_(std::move(texture))
    , rtv_(std::move(rtv)) {
    // 交换链后缓冲纹理只需 RTV，无需 SRV/DSV
}

// ── 通用构造（从描述符创建纹理及关联视图）──────────────────────────────────

D3D11Texture::D3D11Texture(ID3D11Device* device, const TextureDesc& desc)
    : desc_(desc) {

    // 将 RHI 绑定标志转换为 D3D11 绑定标志
    UINT bind_flags = 0;
    if (has_flag(desc.bind_flags, TextureBindFlags::ShaderResource)) {
        bind_flags |= D3D11_BIND_SHADER_RESOURCE;
    }
    if (has_flag(desc.bind_flags, TextureBindFlags::RenderTarget)) {
        bind_flags |= D3D11_BIND_RENDER_TARGET;
    }
    if (has_flag(desc.bind_flags, TextureBindFlags::DepthStencil)) {
        bind_flags |= D3D11_BIND_DEPTH_STENCIL;
    }
    if (has_flag(desc.bind_flags, TextureBindFlags::UnorderedAccess)) {
        bind_flags |= D3D11_BIND_UNORDERED_ACCESS;
    }

    // 深度格式在用作 SRV 时需要特殊处理（使用无类型格式）
    const DXGI_FORMAT tex_format = to_dxgi_format(desc.format);
    if (tex_format == DXGI_FORMAT_UNKNOWN) {
        // 格式不支持，纹理保持无效状态
        return;
    }

    // 创建 D3D11 2D 纹理
    D3D11_TEXTURE2D_DESC tex_desc{};
    tex_desc.Width              = desc.width;
    tex_desc.Height             = desc.height;
    tex_desc.MipLevels          = desc.mip_levels;
    tex_desc.ArraySize          = desc.array_size;
    tex_desc.Format             = tex_format;
    tex_desc.SampleDesc.Count   = 1;
    tex_desc.SampleDesc.Quality = 0;
    tex_desc.Usage              = D3D11_USAGE_DEFAULT;
    tex_desc.BindFlags          = bind_flags;
    tex_desc.CPUAccessFlags     = 0;
    tex_desc.MiscFlags          = 0;

    // 允许 Mipmap 自动生成（需要同时具有 SRV 和 RTV 绑定标志）
    if (has_flag(desc.bind_flags, TextureBindFlags::ShaderResource) &&
        has_flag(desc.bind_flags, TextureBindFlags::RenderTarget) &&
        desc.mip_levels == 0) {
        tex_desc.MiscFlags |= D3D11_RESOURCE_MISC_GENERATE_MIPS;
        tex_desc.MipLevels  = 0; // 由驱动计算完整 Mip 链
    }

    HRESULT hr = device->CreateTexture2D(&tex_desc, nullptr, &texture_);
    if (FAILED(hr)) {
        OutputDebugStringA("[mine.gfx.d3d11] D3D11Texture: CreateTexture2D 失败\n");
        return;
    }

    // 创建着色器资源视图（SRV）
    if (has_flag(desc.bind_flags, TextureBindFlags::ShaderResource)) {
        D3D11_SHADER_RESOURCE_VIEW_DESC srv_desc{};
        srv_desc.Format                    = tex_format;
        srv_desc.ViewDimension             = D3D11_SRV_DIMENSION_TEXTURE2D;
        srv_desc.Texture2D.MipLevels       = static_cast<UINT>(-1); // 所有 Mip
        srv_desc.Texture2D.MostDetailedMip = 0;

        device->CreateShaderResourceView(texture_.Get(), &srv_desc, &srv_);
    }

    // 创建渲染目标视图（RTV）
    if (has_flag(desc.bind_flags, TextureBindFlags::RenderTarget)) {
        D3D11_RENDER_TARGET_VIEW_DESC rtv_desc{};
        rtv_desc.Format             = tex_format;
        rtv_desc.ViewDimension      = D3D11_RTV_DIMENSION_TEXTURE2D;
        rtv_desc.Texture2D.MipSlice = 0;

        device->CreateRenderTargetView(texture_.Get(), &rtv_desc, &rtv_);
    }

    // 创建深度/模板视图（DSV）
    if (has_flag(desc.bind_flags, TextureBindFlags::DepthStencil)) {
        D3D11_DEPTH_STENCIL_VIEW_DESC dsv_desc{};
        dsv_desc.Format             = tex_format;
        dsv_desc.ViewDimension      = D3D11_DSV_DIMENSION_TEXTURE2D;
        dsv_desc.Texture2D.MipSlice = 0;

        device->CreateDepthStencilView(texture_.Get(), &dsv_desc, &dsv_);
    }
}

} // namespace mine::gfx::d3d11
