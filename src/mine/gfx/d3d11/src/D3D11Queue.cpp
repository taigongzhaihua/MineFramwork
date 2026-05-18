/**
 * @file D3D11Queue.cpp
 * @brief D3D11 命令队列实现。
 */

#include "D3D11Queue.h"

namespace mine::gfx::d3d11 {

D3D11Queue::D3D11Queue(ID3D11DeviceContext* immediate_ctx, QueueType type)
    : immediate_ctx_(immediate_ctx)
    , type_(type) {
}

void D3D11Queue::submit(ICommandList* cmd) {
    if (!immediate_ctx_ || cmd == nullptr) {
        return;
    }

    auto* d3d11_cmd = static_cast<D3D11CommandList*>(cmd);
    ID3D11CommandList* raw_cmd = d3d11_cmd->get_d3d11_command_list();

    if (raw_cmd == nullptr) {
        // 命令列表尚未通过 end() 生成，属于调用方错误
        OutputDebugStringA("[mine.gfx.d3d11] D3D11Queue::submit: 命令列表未完成录制（缺少 end() 调用）\n");
        return;
    }

    // 将延迟上下文生成的命令列表提交到立即上下文执行
    // RestoreContextState=FALSE：不恢复立即上下文状态（性能更优，由调用方显式管理状态）
    immediate_ctx_->ExecuteCommandList(raw_cmd, FALSE);
}

void D3D11Queue::wait_idle() {
    if (!immediate_ctx_) {
        return;
    }

    // D3D11 没有显式的 GPU 等待机制；使用 D3D11_QUERY_EVENT 查询来确认 GPU 完成
    // 注意：Flush() 只保证命令被驱动接收，不保证 GPU 执行完毕
    immediate_ctx_->Flush();

    // 对于 M0 阶段，Flush() 足以保证交换链 resize 的安全性
    // 生产环境中应使用 ID3D11Query(D3D11_QUERY_EVENT) 进行更精确的等待
}

} // namespace mine::gfx::d3d11
