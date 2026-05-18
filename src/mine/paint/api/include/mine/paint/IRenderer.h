/**
 * @file IRenderer.h
 * @brief 2D 渲染后端接口。
 *
 * IRenderer 是 mine.paint 与具体图形 API（D3D11/D3D12/Vulkan/Metal）
 * 之间的抽象边界。实现方（如 mine.paint.d3d11）通过工厂函数注入。
 *
 * 设计原则：
 *   - mine.paint 本身不依赖任何具体 RHI 实现。
 *   - IRenderer 依赖 mine.gfx.rhi（IDevice、ITexture）以声明接口。
 *   - 渲染器持有 GPU 资源（着色器、顶点缓冲、图集等），由实现方管理。
 *
 * 依赖：mine.paint（DisplayList）, mine.gfx.rhi（IDevice、ITexture）,
 *       mine.core（OwnedPtr）
 */

#pragma once

#include <mine/paint/DisplayList.h>
#include <mine/gfx/IDevice.h>
#include <mine/gfx/ITexture.h>
#include <mine/core/Memory.h>

namespace mine::paint {

/**
 * @brief 2D 渲染后端接口。
 *
 * 实现类将 DisplayList 中的绘制命令转换为实际的 GPU 绘制调用。
 * 每帧调用顺序：begin_frame() → render() [可多次] → end_frame()。
 */
class IRenderer {
public:
    virtual ~IRenderer() = default;

    // ── 帧生命周期 ──────────────────────────────────────────────────────

    /**
     * @brief 帧开始。
     *
     * 渲染器进行帧级别的准备工作（如重置顶点缓冲写指针、更新帧常量等）。
     * 每帧必须调用一次，对应 end_frame()。
     */
    virtual void begin_frame() = 0;

    /**
     * @brief 帧结束。
     *
     * 提交本帧所有 GPU 命令。
     */
    virtual void end_frame() = 0;

    // ── 渲染 ────────────────────────────────────────────────────────────

    /**
     * @brief 将 DisplayList 渲染到目标纹理（RTV）。
     * @param dl      要渲染的绘制命令列表
     * @param target  目标渲染纹理（必须绑定了 RenderTarget 标志）
     *
     * 可在同一帧内多次调用（渲染到不同目标）。
     */
    virtual void render(const DisplayList& dl, gfx::ITexture* target) = 0;
};

// ── 工厂函数声明 ──────────────────────────────────────────────────────────────

/**
 * @brief 创建 2D 渲染器（实现由链接的后端库提供，如 mine.paint.d3d11）。
 * @param device  图形设备（渲染器内部创建 GPU 资源时使用）
 * @return 渲染器实例，失败时返回空指针
 *
 * @note 此函数声明在 mine.paint 中，实现由后端库（mine.paint.d3d11 等）
 *       通过链接时替换提供，与 mine::gfx::create_device() 机制相同。
 */
[[nodiscard]] mine::core::OwnedPtr<IRenderer> create_renderer(gfx::IDevice* device);

} // namespace mine::paint
