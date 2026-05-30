/**
 * @file D3D11Queue.cpp
 * @brief D3D11 命令队列实现。
 */

#include "D3D11Queue.h"

namespace mine::gfx::d3d11 {

D3D11Queue::D3D11Queue(ID3D11DeviceContext* immediate_ctx, QueueType type)
    : immediate_ctx_(immediate_ctx)
    , type_(type) {

    if (!immediate_ctx_) {
        return;
    }

    // 创建一个可复用的事件查询对象，供 wait_idle() 在需要时插入 GPU 命令流。
    // 当 GetData 返回 S_OK 时，说明 GPU 已经执行到 End(query) 之前的所有命令。
    ComPtr<ID3D11Device> device;
    immediate_ctx_->GetDevice(&device);
    if (!device) {
        OutputDebugStringA("[mine.gfx.d3d11] D3D11Queue: GetDevice 失败，wait_idle 将退化为 Flush\n");
        return;
    }

    D3D11_QUERY_DESC query_desc{};
    query_desc.Query     = D3D11_QUERY_EVENT;
    query_desc.MiscFlags = 0;

    const HRESULT hr = device->CreateQuery(&query_desc, &idle_query_);
    if (FAILED(hr)) {
        OutputDebugStringA("[mine.gfx.d3d11] D3D11Queue: CreateQuery 失败，wait_idle 将退化为 Flush\n");
    }
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

    // ExecuteCommandList 返回后即可释放命令列表 COM 对象本身；
    // 若继续保留到下一帧 begin()，它仍可能持有旧后缓冲引用，
    // 使 swapchain 在窗口 resize 时的 ResizeBuffers() 返回 INVALID_CALL。
    d3d11_cmd->release_submitted_command_list();
}

void D3D11Queue::wait_idle() {
    if (!immediate_ctx_) {
        return;
    }

    // 没有可用查询对象时退化为 Flush；虽然不能保证 GPU 完成，
    // 但至少保持与旧实现一致，避免因为创建查询失败而完全失效。
    if (!idle_query_) {
        immediate_ctx_->Flush();
        return;
    }

    // 将事件查询插入命令流末尾，并主动 Flush，确保驱动尽快开始执行。
    immediate_ctx_->End(idle_query_.Get());
    immediate_ctx_->Flush();

    // 轮询直到 GPU 执行到该事件点。
    while (true) {
        const HRESULT hr = immediate_ctx_->GetData(
            idle_query_.Get(), nullptr, 0, D3D11_ASYNC_GETDATA_DONOTFLUSH);

        if (hr == S_OK) {
            return;
        }

        if (hr != S_FALSE) {
            OutputDebugStringA("[mine.gfx.d3d11] D3D11Queue::wait_idle: GetData 失败，提前返回\n");
            return;
        }

        // 让出时间片，避免忙等占满 CPU。
        ::Sleep(0);
    }
}

} // namespace mine::gfx::d3d11
