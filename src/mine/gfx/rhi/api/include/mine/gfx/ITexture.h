/**
 * @file ITexture.h
 * @brief GPU 纹理资源接口。
 */

#pragma once

#include "GfxTypes.h"

namespace mine::gfx {

/**
 * @brief GPU 纹理资源抽象接口。
 *
 * 具体纹理对象由 IDevice::create_texture() 或交换链内部创建，
 * 调用方通过此接口操作纹理，无需感知底层 API（D3D/Vulkan/Metal）。
 *
 * 生命周期：通过 OwnedPtr<ITexture> 管理（调用方拥有）；
 * 交换链后缓冲纹理的生命周期由 ISwapchain 管理，返回的是裸指针，
 * 不转移所有权。
 */
class ITexture {
public:
    virtual ~ITexture() = default;

    /// 返回创建时的描述符（只读）
    [[nodiscard]] virtual const TextureDesc& desc() const noexcept = 0;

    /// 纹理宽度（像素）
    [[nodiscard]] uint32_t width() const noexcept { return desc().width; }

    /// 纹理高度（像素）
    [[nodiscard]] uint32_t height() const noexcept { return desc().height; }

    /// 像素格式
    [[nodiscard]] PixelFormat format() const noexcept { return desc().format; }
};

} // namespace mine::gfx
