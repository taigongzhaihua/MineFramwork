/**
 * @file IFence.h
 * @brief GPU/CPU 同步围栏接口（单调递增计数器模型）。
 */

#pragma once

#include <cstdint>

namespace mine::gfx {

/**
 * @brief GPU-CPU 同步围栏接口。
 *
 * 围栏使用单调递增的 uint64 计数器：
 *   - GPU 提交时带上目标值（via IQueue 内部使用）
 *   - CPU 通过 wait() 阻塞直到 GPU 完成对应值的信号
 *   - completed_value() 返回 GPU 已完成的最新值
 *
 * D3D12：ID3D12Fence；Vulkan：VkSemaphore/VkFence；D3D11：ID3D11Query（Timestamp 或 Event）
 *
 * @note M0 阶段（D3D11 后端）Fence 以轮询方式实现。
 */
class IFence {
public:
    virtual ~IFence() = default;

    /**
     * @brief 发出信号，通知 GPU/CPU 当前值已达到 value。
     *
     * D3D12 后端中此调用通过 ID3D12CommandQueue::Signal 完成（GPU 端信号）；
     * D3D11 后端中此调用插入 GPU 事件查询并等待查询完成。
     */
    virtual void signal(uint64_t value) = 0;

    /**
     * @brief 阻塞调用线程，直到围栏计数器 ≥ value 或超时。
     * @param value      等待的目标计数器值
     * @param timeout_ns 超时时间（纳秒），UINT64_MAX 表示无限等待
     */
    virtual void wait(uint64_t value, uint64_t timeout_ns = UINT64_MAX) = 0;

    /**
     * @brief 返回 GPU 已完成的最新计数器值（非阻塞查询）。
     */
    [[nodiscard]] virtual uint64_t completed_value() const noexcept = 0;
};

} // namespace mine::gfx
