/**
 * @file D3D11Buffer.cpp
 * @brief D3D11 GPU 缓冲资源实现。
 */

#include "D3D11Buffer.h"

namespace mine::gfx::d3d11 {

D3D11Buffer::D3D11Buffer(
    ID3D11Device*     device,
    const BufferDesc& desc,
    const void*       init_data)
    : desc_(desc) {

    // 将 RHI 绑定标志转换为 D3D11 绑定标志
    UINT bind_flags = 0;
    if (has_flag(desc.bind_flags, BufferBindFlags::Vertex)) {
        bind_flags |= D3D11_BIND_VERTEX_BUFFER;
    }
    if (has_flag(desc.bind_flags, BufferBindFlags::Index)) {
        bind_flags |= D3D11_BIND_INDEX_BUFFER;
    }
    if (has_flag(desc.bind_flags, BufferBindFlags::Constant)) {
        bind_flags |= D3D11_BIND_CONSTANT_BUFFER;
    }
    if (has_flag(desc.bind_flags, BufferBindFlags::ShaderResource)) {
        bind_flags |= D3D11_BIND_SHADER_RESOURCE;
    }
    if (has_flag(desc.bind_flags, BufferBindFlags::UnorderedAccess)) {
        bind_flags |= D3D11_BIND_UNORDERED_ACCESS;
    }

    D3D11_BUFFER_DESC buf_desc{};
    buf_desc.ByteWidth           = static_cast<UINT>(desc.size);
    buf_desc.Usage               = D3D11_USAGE_DEFAULT;
    buf_desc.BindFlags           = bind_flags;
    buf_desc.CPUAccessFlags      = 0;
    buf_desc.MiscFlags           = 0;
    buf_desc.StructureByteStride = desc.stride;

    // 常量缓冲大小必须是 16 字节的倍数（D3D11 硬性要求）
    if (has_flag(desc.bind_flags, BufferBindFlags::Constant)) {
        buf_desc.ByteWidth = (buf_desc.ByteWidth + 15u) & ~15u;
    }

    if (init_data != nullptr) {
        // 提供初始数据时使用不可变（IMMUTABLE）用途以获得最佳性能
        buf_desc.Usage = D3D11_USAGE_IMMUTABLE;

        D3D11_SUBRESOURCE_DATA sub_data{};
        sub_data.pSysMem = init_data;

        const HRESULT hr = device->CreateBuffer(&buf_desc, &sub_data, &buffer_);
        if (FAILED(hr)) {
            OutputDebugStringA("[mine.gfx.d3d11] D3D11Buffer: CreateBuffer（含初始数据）失败\n");
        }
    } else {
        const HRESULT hr = device->CreateBuffer(&buf_desc, nullptr, &buffer_);
        if (FAILED(hr)) {
            OutputDebugStringA("[mine.gfx.d3d11] D3D11Buffer: CreateBuffer 失败\n");
        }
    }
}

} // namespace mine::gfx::d3d11
