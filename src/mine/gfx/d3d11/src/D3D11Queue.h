/**
 * @file D3D11Queue.h
 * @brief D3D11 命令队列实现（基于 D3D11 立即上下文）。
 */

#pragma once

#include "D3D11Helpers.h"
#include "D3D11CommandList.h"
#include <mine/gfx/IQueue.h>

namespace mine::gfx::d3d11 {

/**
 * @brief IQueue 的 D3D11 实现，封装 D3D11 立即上下文（Immediate Context）。
 *
 * D3D11 的命令提交模型：
 *   - 延迟上下文录制命令 → FinishCommandList() → ID3D11CommandList
 *   - 立即上下文 ExecuteCommandList() → GPU 执行
 *   - 立即上下文是单线程的，所有提交操作须在同一线程（渲染线程）完成
 *
 * M0 阶段只支持一个 Graphics 队列，使用立即上下文串行提交。
 */
class D3D11Queue final : public IQueue {
public:
    /**
     * @brief 构造 D3D11 命令队列。
     * @param immediate_ctx  D3D11 立即上下文（从设备获取，借用引用，不拥有）
     * @param type           队列类型（当前 D3D11 只支持 Graphics）
     */
    D3D11Queue(ID3D11DeviceContext* immediate_ctx, QueueType type);

    ~D3D11Queue() override = default;

    D3D11Queue(const D3D11Queue&)            = delete;
    D3D11Queue& operator=(const D3D11Queue&) = delete;

    // ── IQueue 接口 ───────────────────────────────────────────────────────

    /**
     * @brief 将已录制完毕的命令列表提交到 GPU 执行。
     *
     * D3D11 通过立即上下文的 ExecuteCommandList() 将延迟上下文中录制的命令
     * 转交给驱动执行。此调用是同步的（命令被驱动接收，不保证 GPU 完成）。
     */
    void submit(ICommandList* cmd) override;

    /**
     * @brief 等待此队列所有已提交命令在 GPU 侧完成（同步阻塞）。
     *
     * D3D11 通过 Flush() + ID3D11Query(Sync) 实现，性能开销较大，
     * 仅在资源销毁 / Swapchain resize 等必要场合使用。
     */
    void wait_idle() override;

    [[nodiscard]] QueueType type() const noexcept override { return type_; }

private:
    /// 借用的立即上下文引用（生命周期由 D3D11Device 管理）
    ID3D11DeviceContext* immediate_ctx_{nullptr};
    QueueType            type_{QueueType::Graphics};
};

} // namespace mine::gfx::d3d11
