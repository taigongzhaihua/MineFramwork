/**
 * @file ISwapchain.h
 * @brief 与平台窗口绑定的交换链接口。
 */

#pragma once

#include "GfxTypes.h"
#include "ITexture.h"

namespace mine::gfx {

/**
 * @brief GPU 交换链抽象接口。
 *
 * 交换链管理一组后缓冲纹理，每帧渲染完毕后通过 present() 将帧提交给
 * 显示系统（DWM / 操作系统合成器）。
 *
 * 创建：IDevice::create_swapchain(SwapchainDesc)，由 OwnedPtr<ISwapchain> 管理。
 *
 * 生命周期注意：
 *   - 必须在关联窗口仍然存在时销毁 Swapchain。
 *   - 窗口大小变化时，应调用 resize() 而非重建 Swapchain。
 */
class ISwapchain {
public:
    virtual ~ISwapchain() = default;

    /**
     * @brief 调整交换链大小（通常在窗口 SizeChanged 事件时调用）。
     * @param width  新的窗口客户区宽度（像素）
     * @param height 新的窗口客户区高度（像素）
     *
     * 调用此函数前应确保当前帧的命令已提交，且 GPU 不再写入后缓冲。
     */
    virtual void resize(uint32_t width, uint32_t height) = 0;

    /**
     * @brief 将当前渲染帧呈现到屏幕。
     *
     * 根据创建时的 Vsync 设置决定是否等待垂直同步。
     * 函数返回后，下一个 current_render_target() 将指向新的后缓冲。
     */
    virtual void present() = 0;

    /**
     * @brief 获取当前可写入的后缓冲渲染目标。
     *
     * 返回的指针由 Swapchain 内部管理（不转移所有权），
     * 调用方不得删除此指针，且在 resize() / present() 后应重新获取。
     */
    [[nodiscard]] virtual ITexture* current_render_target() noexcept = 0;

    /// 当前交换链宽度（像素）
    [[nodiscard]] virtual uint32_t width() const noexcept = 0;

    /// 当前交换链高度（像素）
    [[nodiscard]] virtual uint32_t height() const noexcept = 0;

    /// 后缓冲像素格式
    [[nodiscard]] virtual PixelFormat format() const noexcept = 0;

    /// 缓冲帧数（2=双缓冲，3=三缓冲）
    [[nodiscard]] virtual uint32_t image_count() const noexcept = 0;

    /// 垂直同步模式
    [[nodiscard]] virtual Vsync vsync() const noexcept = 0;
};

} // namespace mine::gfx
