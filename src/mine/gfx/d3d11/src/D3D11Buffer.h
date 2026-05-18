/**
 * @file D3D11Buffer.h
 * @brief D3D11 缓冲资源实现（IBuffer 接口）。
 */

#pragma once

#include "D3D11Helpers.h"
#include <mine/gfx/IBuffer.h>

namespace mine::gfx::d3d11 {

/**
 * @brief D3D11 GPU 缓冲资源，封装 ID3D11Buffer。
 *
 * 通过 IDevice::create_buffer(BufferDesc) 创建。
 * 支持顶点缓冲、索引缓冲、常量缓冲等用途（由 bind_flags 决定）。
 */
class D3D11Buffer final : public IBuffer {
public:
    /**
     * @brief 从描述符和 D3D11 设备创建缓冲。
     * @param device      D3D11 设备
     * @param desc        缓冲描述符
     * @param init_data   可选的初始数据指针（可为 nullptr）
     */
    D3D11Buffer(
        ID3D11Device*   device,
        const BufferDesc& desc,
        const void*     init_data = nullptr);

    ~D3D11Buffer() override = default;

    D3D11Buffer(const D3D11Buffer&)            = delete;
    D3D11Buffer& operator=(const D3D11Buffer&) = delete;

    // ── IBuffer 接口 ──────────────────────────────────────────────────────

    [[nodiscard]] const BufferDesc& desc() const noexcept override { return desc_; }

    // ── D3D11 内部访问器 ──────────────────────────────────────────────────

    [[nodiscard]] ID3D11Buffer* buffer() const noexcept { return buffer_.Get(); }

    /// 判断缓冲是否有效（创建成功）
    [[nodiscard]] bool is_valid() const noexcept { return buffer_ != nullptr; }

private:
    BufferDesc          desc_{};
    ComPtr<ID3D11Buffer> buffer_;
};

} // namespace mine::gfx::d3d11
