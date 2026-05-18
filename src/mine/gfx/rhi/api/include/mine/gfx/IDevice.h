/**
 * @file IDevice.h
 * @brief 图形设备接口（对应 D3D12 Device / Vulkan Device / Metal Device）。
 */

#pragma once

#include "GfxTypes.h"
#include "IBuffer.h"
#include "ICommandList.h"
#include "IFence.h"
#include "IPipeline.h"
#include "IQueue.h"
#include "ISwapchain.h"
#include "ITexture.h"
#include <mine/core/Memory.h>

namespace mine::gfx {

/**
 * @brief 图形设备抽象接口。
 *
 * IDevice 是 RHI 的入口点，负责创建所有 GPU 资源。
 * 同一进程可以同时存在多个 IDevice（多 GPU 场景），但通常只需一个。
 *
 * 创建方式：mine::gfx::create_device(Backend)
 * 返回 OwnedPtr<IDevice>（nullptr 表示创建失败，例如硬件不支持该后端）。
 */
class IDevice {
public:
    virtual ~IDevice() = default;

    // ── 队列创建 ──────────────────────────────────────────────────────────

    /**
     * @brief 创建指定类型的命令队列。
     * @param type  Graphics / Compute / Copy
     * @return 队列对象；若该类型不支持则返回 nullptr
     */
    [[nodiscard]] virtual core::OwnedPtr<IQueue>
        create_queue(QueueType type) = 0;

    // ── 交换链创建 ────────────────────────────────────────────────────────

    /**
     * @brief 为平台窗口创建交换链。
     * @param desc  描述符，native_window 必须为有效窗口句柄
     * @return 交换链对象；失败返回 nullptr
     */
    [[nodiscard]] virtual core::OwnedPtr<ISwapchain>
        create_swapchain(const SwapchainDesc& desc) = 0;

    // ── 资源创建 ──────────────────────────────────────────────────────────

    /**
     * @brief 创建 GPU 缓冲。
     * @param desc  缓冲描述符（大小、绑定标志等）
     * @return 缓冲对象；失败返回 nullptr
     */
    [[nodiscard]] virtual core::OwnedPtr<IBuffer>
        create_buffer(const BufferDesc& desc) = 0;

    /**
     * @brief 创建 GPU 纹理。
     * @param desc  纹理描述符（尺寸、格式、绑定标志等）
     * @return 纹理对象；失败返回 nullptr
     */
    [[nodiscard]] virtual core::OwnedPtr<ITexture>
        create_texture(const TextureDesc& desc) = 0;

    // ── 命令列表创建 ──────────────────────────────────────────────────────

    /**
     * @brief 为指定队列类型创建命令录制列表。
     * @param type  命令列表对应的队列类型（需与提交目标队列一致）
     * @return 命令列表；失败返回 nullptr
     */
    [[nodiscard]] virtual core::OwnedPtr<ICommandList>
        create_command_list(QueueType type = QueueType::Graphics) = 0;

    // ── 同步原语 ──────────────────────────────────────────────────────────

    /**
     * @brief 创建 GPU-CPU 同步围栏。
     * @param initial_value  初始计数器值（通常为 0）
     * @return 围栏对象；失败返回 nullptr
     */
    [[nodiscard]] virtual core::OwnedPtr<IFence>
        create_fence(uint64_t initial_value = 0) = 0;

    // ── 设备信息 ──────────────────────────────────────────────────────────

    /// 返回此设备使用的图形后端
    [[nodiscard]] virtual Backend backend() const noexcept = 0;

    /// 返回 GPU 名称（用于日志/调试）
    [[nodiscard]] virtual const char* adapter_name() const noexcept = 0;
};

} // namespace mine::gfx
