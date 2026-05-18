/**
 * @file D3D11Fence.cpp
 * @brief D3D11 栅栏实现（事件查询轮询）。
 */

#include "D3D11Fence.h"
#include <windows.h>  // GetTickCount64

namespace mine::gfx::d3d11 {

D3D11Fence::D3D11Fence(
    ID3D11Device*        device,
    ID3D11DeviceContext* immediate_ctx,
    uint64_t             initial_value)
    : immediate_ctx_(immediate_ctx) {

    completed_value_.store(initial_value, std::memory_order_release);
    signaled_value_ = initial_value;

    // 创建 D3D11 事件查询对象（D3D11_QUERY_EVENT：GPU 命令流到达此点时完成）
    D3D11_QUERY_DESC query_desc{};
    query_desc.Query     = D3D11_QUERY_EVENT;
    query_desc.MiscFlags = 0;

    const HRESULT hr = device->CreateQuery(&query_desc, &query_);
    if (FAILED(hr)) {
        OutputDebugStringA("[mine.gfx.d3d11] D3D11Fence: CreateQuery 失败\n");
    }
}

void D3D11Fence::signal(uint64_t value) {
    if (!immediate_ctx_ || !query_) {
        return;
    }

    signaled_value_ = value;

    // 向命令流中插入一个事件查询标记
    // 当 GPU 执行到此点时，后续 GetData 将返回成功
    immediate_ctx_->End(query_.Get());
}

void D3D11Fence::wait(uint64_t value, uint64_t timeout_ns) {
    if (!immediate_ctx_ || !query_) {
        return;
    }

    // 如果目标值已经完成，直接返回
    if (completed_value_.load(std::memory_order_acquire) >= value) {
        return;
    }

    // 计算超时截止时间（毫秒精度，Windows API 限制）
    const uint64_t deadline_ms =
        (timeout_ns == 0)
        ? UINT64_MAX
        : GetTickCount64() + (timeout_ns / 1'000'000ULL) + 1;

    // 轮询等待 GPU 事件查询完成
    // D3D11_ASYNC_GETDATA_DONOTFLUSH：不强制 Flush，避免空转时频繁触发驱动刷新
    while (true) {
        const HRESULT hr = immediate_ctx_->GetData(
            query_.Get(), nullptr, 0, D3D11_ASYNC_GETDATA_DONOTFLUSH);

        if (hr == S_OK) {
            // GPU 已完成，更新完成值
            const uint64_t prev = completed_value_.load(std::memory_order_relaxed);
            if (signaled_value_ > prev) {
                completed_value_.store(signaled_value_, std::memory_order_release);
            }
            break;
        }

        // 超时检测
        if (timeout_ns != 0 && GetTickCount64() >= deadline_ms) {
            OutputDebugStringA("[mine.gfx.d3d11] D3D11Fence::wait: 等待超时\n");
            break;
        }

        // 让出 CPU 时间片，避免忙等待占满核心
        ::Sleep(0);
    }
}

} // namespace mine::gfx::d3d11
