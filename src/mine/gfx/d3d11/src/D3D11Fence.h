/**
 * @file D3D11Fence.h
 * @brief D3D11 栅栏/同步原语实现（IFence 接口）。
 */

#pragma once

#include "D3D11Helpers.h"
#include <mine/gfx/IFence.h>
#include <atomic>

namespace mine::gfx::d3d11 {

/**
 * @brief IFence 的 D3D11 实现，通过 D3D11 事件查询（D3D11_QUERY_EVENT）模拟栅栏。
 *
 * D3D11 没有原生的 Timeline Semaphore 语义，此实现使用事件查询
 * 模拟 CPU 端的等待（轮询 GetData 直到查询完成）。
 *
 * M0 阶段的使用场景主要是交换链 resize 前的 GPU 同步，
 * 不要求高精度的多帧资源管理，此实现已足够。
 */
class D3D11Fence final : public IFence {
public:
    /**
     * @brief 构造 D3D11 栅栏。
     * @param device         D3D11 设备（用于创建查询对象）
     * @param immediate_ctx  立即上下文（用于 GetData）
     * @param initial_value  初始完成值
     */
    D3D11Fence(
        ID3D11Device*        device,
        ID3D11DeviceContext* immediate_ctx,
        uint64_t             initial_value = 0);

    ~D3D11Fence() override = default;

    D3D11Fence(const D3D11Fence&)            = delete;
    D3D11Fence& operator=(const D3D11Fence&) = delete;

    // ── IFence 接口 ───────────────────────────────────────────────────────

    /**
     * @brief 向立即上下文提交事件查询，当 GPU 完成当前所有命令后值变为 value。
     */
    void signal(uint64_t value) override;

    /**
     * @brief 轮询等待完成值达到 value，或超时返回。
     * @param value      目标完成值
     * @param timeout_ns 超时时间（纳秒），0 表示无限等待
     */
    void wait(uint64_t value, uint64_t timeout_ns = 0) override;

    [[nodiscard]] uint64_t completed_value() const noexcept override {
        return completed_value_.load(std::memory_order_acquire);
    }

private:
    ID3D11DeviceContext*        immediate_ctx_{nullptr}; ///< 借用引用（不拥有）
    ComPtr<ID3D11Query>         query_;                  ///< D3D11 事件查询对象
    std::atomic<uint64_t>       completed_value_{0};     ///< 当前完成的最大值
    uint64_t                    signaled_value_{0};       ///< 最近一次 signal 的目标值
};

} // namespace mine::gfx::d3d11
