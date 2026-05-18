/**
 * @file ICommandList.h
 * @brief 命令录制接口（对应 D3D12 CommandList / Vulkan CommandBuffer / Metal CommandEncoder）。
 */

#pragma once

#include "GfxTypes.h"
#include "ITexture.h"
#include "IBuffer.h"

namespace mine::gfx {

class IPipeline;

/**
 * @brief GPU 命令录制接口。
 *
 * 使用流程：
 *   1. begin()          — 开始录制（重置内部状态）
 *   2. set_render_target / clear / draw ...
 *   3. end()            — 结束录制
 *   4. IQueue::submit() — 将命令列表提交到 GPU
 *
 * 同一时刻只允许单线程录制，不同线程应使用各自的 ICommandList 实例。
 * D3D12/Vulkan 后端使用真正的延迟命令列表；D3D11 后端使用延迟上下文。
 */
class ICommandList {
public:
    virtual ~ICommandList() = default;

    // ── 录制控制 ──────────────────────────────────────────────────────────

    /// 开始录制（重置上一帧录制的状态）
    virtual void begin() = 0;

    /// 结束录制，之后可提交给 IQueue
    virtual void end() = 0;

    // ── 渲染目标 ──────────────────────────────────────────────────────────

    /**
     * @brief 设置渲染目标与深度/模板缓冲。
     * @param rt     渲染目标纹理（必须具有 RenderTarget 绑定标志）
     * @param depth  深度/模板纹理（可为 nullptr，表示无深度测试）
     */
    virtual void set_render_target(ITexture* rt, ITexture* depth = nullptr) = 0;

    /**
     * @brief 用指定颜色清除渲染目标。
     * @param rt    目标纹理（必须已通过 set_render_target 绑定）
     * @param color 清除颜色（线性 RGBA）
     */
    virtual void clear_render_target(ITexture* rt, const Color4f& color) = 0;

    /**
     * @brief 清除深度/模板缓冲。
     * @param depth_stencil  深度/模板纹理
     * @param depth_value    写入的深度值（0.0–1.0），反向 Z 时使用 0.0
     * @param stencil_value  写入的模板值
     */
    virtual void clear_depth_stencil(
        ITexture* depth_stencil,
        float     depth_value   = 1.0f,
        uint8_t   stencil_value = 0) = 0;

    // ── 管线状态与视口 ────────────────────────────────────────────────────

    /// 设置视口
    virtual void set_viewport(const Viewport& viewport) = 0;

    /// 设置裁剪矩形
    virtual void set_scissor(const ScissorRect& rect) = 0;

    // ── 绘制（M0 阶段仅声明接口，后续补充实现）────────────────────────────

    /**
     * @brief 绑定图形/计算管线状态对象。
     * @param pipeline  已编译的管线对象
     */
    virtual void set_pipeline(IPipeline* pipeline) = 0;

    /**
     * @brief 绑定常量缓冲（Uniform Buffer）到顶点/像素着色器。
     * @param slot    绑定槽位（对应 HLSL: register(bN)）
     * @param buffer  常量缓冲（必须以 BufferBindFlags::Constant 创建）
     *
     * 同时绑定到顶点着色器（VSSetConstantBuffers）和像素着色器（PSSetConstantBuffers）。
     */
    virtual void set_constant_buffer(uint32_t slot, IBuffer* buffer) = 0;

    /**
     * @brief 绑定顶点缓冲。
     * @param slot    绑定槽位
     * @param buffer  顶点缓冲
     * @param offset  缓冲内字节偏移
     */
    virtual void set_vertex_buffer(
        uint32_t slot, IBuffer* buffer, uint64_t offset = 0) = 0;

    /**
     * @brief 绑定 16/32-bit 索引缓冲。
     * @param buffer   索引缓冲
     * @param offset   字节偏移
     * @param use32bit true = 32-bit 索引，false = 16-bit 索引
     */
    virtual void set_index_buffer(
        IBuffer* buffer, uint64_t offset = 0, bool use32bit = false) = 0;

    /// 非索引绘制
    virtual void draw(
        uint32_t vertex_count,
        uint32_t instance_count = 1,
        uint32_t first_vertex   = 0,
        uint32_t first_instance = 0) = 0;

    /// 索引绘制
    virtual void draw_indexed(
        uint32_t index_count,
        uint32_t instance_count  = 1,
        uint32_t first_index     = 0,
        int32_t  base_vertex     = 0,
        uint32_t first_instance  = 0) = 0;
};

} // namespace mine::gfx
