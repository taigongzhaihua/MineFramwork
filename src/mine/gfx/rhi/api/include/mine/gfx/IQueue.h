/**
 * @file IQueue.h
 * @brief 命令队列接口（对应 D3D12 CommandQueue / Vulkan Queue / Metal CommandQueue）。
 */

#pragma once

#include "GfxTypes.h"

namespace mine::gfx {

class ICommandList;
class IFence;

/**
 * @brief GPU 命令提交队列接口。
 *
 * 创建：IDevice::create_queue(QueueType)
 * 用法：将录制完毕的 ICommandList 提交到此队列，GPU 异步执行。
 * 同步：通过 IFence 或 wait_idle() 等待 GPU 完成。
 */
class IQueue {
public:
    virtual ~IQueue() = default;

    /**
     * @brief 将命令列表提交到 GPU 执行（异步）。
     * @param cmd  已调用 end() 的命令列表
     */
    virtual void submit(ICommandList* cmd) = 0;

    /**
     * @brief 等待此队列上所有已提交命令执行完毕（同步阻塞）。
     *
     * 性能敏感路径不应频繁调用，仅在资源销毁/重建时使用。
     */
    virtual void wait_idle() = 0;

    /// 队列类型
    [[nodiscard]] virtual QueueType type() const noexcept = 0;
};

} // namespace mine::gfx
