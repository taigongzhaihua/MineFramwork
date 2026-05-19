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
     * @param desc        缓冲描述符（大小、绑定标志等）
     * @param initial_data 可选初始数据指针（传 nullptr 则创建空缓冲）
     * @return 缓冲对象；失败返回 nullptr
     */
    [[nodiscard]] virtual core::OwnedPtr<IBuffer>
        create_buffer(const BufferDesc& desc, const void* initial_data = nullptr) = 0;

    /**
     * @brief 创建 GPU 纹理。
     * @param desc  纹理描述符（尺寸、格式、绑定标志等）
     * @return 纹理对象；失败返回 nullptr
     */
    [[nodiscard]] virtual core::OwnedPtr<ITexture>
        create_texture(const TextureDesc& desc) = 0;
    // ── 管线创建 ──────────────────────────────────────────────────────────────

    /**
     * @brief 编译并创建图形管线状态对象（含着色器、顶点布局、混合/光栅化状态）。
     * @param desc  管线描述符（着色器源码/字节码、顶点布局、混合开关等）
     * @return 管线对象；着色器编译失败或参数非法时返回 nullptr
     *
     * @note D3D11 后端在此调用 D3DCompile 完成 HLSL 编译；
     *       后续工具链（tool.shadercc）将改为传入预编译字节码。
     */
    [[nodiscard]] virtual core::OwnedPtr<IPipeline>
        create_pipeline(const PipelineDesc& desc) = 0;
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

    /**
     * @brief 将 CPU 数据上传到 GPU 纹理的指定矩形区域。
     *
     * 纹理必须以 D3D11_USAGE_DEFAULT / D3D11_USAGE_DYNAMIC 创建。
     * 常用于字形图集的增量更新：仅上传新增字形所占的脏区域。
     *
     * @param texture   目标纹理（必须由 create_texture() 创建）
     * @param x         目标区域左上角 X（像素，包含）
     * @param y         目标区域左上角 Y（像素，包含）
     * @param width     区域宽度（像素）
     * @param height    区域高度（像素）
     * @param data      源数据指针（从 (x,y) 开始的矩形像素数据，行主序）
     * @param row_pitch 源数据每行的字节数（≥ width × bytes_per_pixel）
     */
    virtual void update_texture_region(
        ITexture*    texture,
        uint32_t     x,
        uint32_t     y,
        uint32_t     width,
        uint32_t     height,
        const void*  data,
        uint32_t     row_pitch) = 0;
};

} // namespace mine::gfx
