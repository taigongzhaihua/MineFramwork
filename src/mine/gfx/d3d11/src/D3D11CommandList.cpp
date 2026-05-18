/**
 * @file D3D11CommandList.cpp
 * @brief D3D11 命令录制实现（延迟上下文）。
 */

#include "D3D11CommandList.h"
#include "D3D11Buffer.h"
#include "D3D11Texture.h"

namespace mine::gfx::d3d11 {

// ── 构造 ──────────────────────────────────────────────────────────────────────

D3D11CommandList::D3D11CommandList(ID3D11Device* device) {
    // 为此命令列表创建专用的 D3D11 延迟上下文
    // 延迟上下文线程安全：可在任意线程录制，提交到立即上下文是串行的
    const HRESULT hr = device->CreateDeferredContext(0, &deferred_ctx_);
    if (FAILED(hr)) {
        OutputDebugStringA("[mine.gfx.d3d11] D3D11CommandList: CreateDeferredContext 失败\n");
    }
}

// ── 录制控制 ──────────────────────────────────────────────────────────────────

void D3D11CommandList::begin() {
    if (!deferred_ctx_) {
        return;
    }
    // 释放上一帧的命令列表对象
    d3d11_cmd_list_.Reset();
    // 清空延迟上下文中所有绑定的资源状态，确保录制从干净的状态开始
    deferred_ctx_->ClearState();
    recording_ = true;
}

void D3D11CommandList::end() {
    if (!deferred_ctx_ || !recording_) {
        return;
    }
    recording_ = false;
    // 将延迟上下文中的录制内容固化为 ID3D11CommandList
    // RestoreDeferredContextState=FALSE：执行后不恢复延迟上下文状态（性能更优）
    const HRESULT hr = deferred_ctx_->FinishCommandList(FALSE, &d3d11_cmd_list_);
    if (FAILED(hr)) {
        OutputDebugStringA("[mine.gfx.d3d11] D3D11CommandList::end: FinishCommandList 失败\n");
    }
}

// ── 渲染目标 ──────────────────────────────────────────────────────────────────

void D3D11CommandList::set_render_target(ITexture* rt, ITexture* depth) {
    if (!deferred_ctx_ || !recording_) {
        return;
    }

    ID3D11RenderTargetView* rtv  = nullptr;
    ID3D11DepthStencilView* dsv  = nullptr;

    if (rt != nullptr) {
        rtv = static_cast<D3D11Texture*>(rt)->rtv();
    }
    if (depth != nullptr) {
        dsv = static_cast<D3D11Texture*>(depth)->dsv();
    }

    deferred_ctx_->OMSetRenderTargets(1, &rtv, dsv);
}

void D3D11CommandList::clear_render_target(ITexture* rt, const Color4f& color) {
    if (!deferred_ctx_ || !recording_ || rt == nullptr) {
        return;
    }

    ID3D11RenderTargetView* rtv = static_cast<D3D11Texture*>(rt)->rtv();
    if (rtv == nullptr) {
        return;
    }

    // D3D11 清屏接受 float[4] 颜色数组（RGBA，线性空间）
    const float rgba[4] = {color.r, color.g, color.b, color.a};
    deferred_ctx_->ClearRenderTargetView(rtv, rgba);
}

void D3D11CommandList::clear_depth_stencil(
    ITexture* depth_stencil,
    float     depth_value,
    uint8_t   stencil_value) {

    if (!deferred_ctx_ || !recording_ || depth_stencil == nullptr) {
        return;
    }

    ID3D11DepthStencilView* dsv = static_cast<D3D11Texture*>(depth_stencil)->dsv();
    if (dsv == nullptr) {
        return;
    }

    deferred_ctx_->ClearDepthStencilView(
        dsv,
        D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL,
        depth_value,
        stencil_value);
}

// ── 管线状态与视口 ────────────────────────────────────────────────────────────

void D3D11CommandList::set_viewport(const Viewport& viewport) {
    if (!deferred_ctx_ || !recording_) {
        return;
    }

    D3D11_VIEWPORT vp{};
    vp.TopLeftX = viewport.x;
    vp.TopLeftY = viewport.y;
    vp.Width    = viewport.width;
    vp.Height   = viewport.height;
    vp.MinDepth = viewport.min_depth;
    vp.MaxDepth = viewport.max_depth;

    deferred_ctx_->RSSetViewports(1, &vp);
}

void D3D11CommandList::set_scissor(const ScissorRect& rect) {
    if (!deferred_ctx_ || !recording_) {
        return;
    }

    D3D11_RECT scissor{};
    scissor.left   = rect.left;
    scissor.top    = rect.top;
    scissor.right  = rect.right;
    scissor.bottom = rect.bottom;

    deferred_ctx_->RSSetScissorRects(1, &scissor);
}

// ── 绘制相关（M0 阶段仅支持基础绑定，完整绘制在 M0.3 实现）─────────────────

void D3D11CommandList::set_pipeline(IPipeline* /*pipeline*/) {
    // M0 阶段不实现，等待 mine.paint 管线对象设计完成后补充
}

void D3D11CommandList::set_vertex_buffer(
    uint32_t slot, IBuffer* buffer, uint64_t offset) {

    if (!deferred_ctx_ || !recording_ || buffer == nullptr) {
        return;
    }

    auto* d3d_buf  = static_cast<D3D11Buffer*>(buffer);
    ID3D11Buffer* raw = d3d_buf->buffer();

    // stride 从 BufferDesc 中获取（必须由调用方设置正确步长）
    const UINT stride = d3d_buf->desc().stride;
    const UINT offs   = static_cast<UINT>(offset);

    deferred_ctx_->IASetVertexBuffers(slot, 1, &raw, &stride, &offs);
}

void D3D11CommandList::set_index_buffer(
    IBuffer* buffer, uint64_t offset, bool use32bit) {

    if (!deferred_ctx_ || !recording_ || buffer == nullptr) {
        return;
    }

    auto* d3d_buf = static_cast<D3D11Buffer*>(buffer);
    const DXGI_FORMAT fmt = use32bit ? DXGI_FORMAT_R32_UINT : DXGI_FORMAT_R16_UINT;

    deferred_ctx_->IASetIndexBuffer(
        d3d_buf->buffer(),
        fmt,
        static_cast<UINT>(offset));
}

void D3D11CommandList::draw(
    uint32_t vertex_count,
    uint32_t instance_count,
    uint32_t first_vertex,
    uint32_t first_instance) {

    if (!deferred_ctx_ || !recording_) {
        return;
    }

    if (instance_count <= 1 && first_instance == 0) {
        deferred_ctx_->Draw(vertex_count, first_vertex);
    } else {
        deferred_ctx_->DrawInstanced(
            vertex_count, instance_count, first_vertex, first_instance);
    }
}

void D3D11CommandList::draw_indexed(
    uint32_t index_count,
    uint32_t instance_count,
    uint32_t first_index,
    int32_t  base_vertex,
    uint32_t first_instance) {

    if (!deferred_ctx_ || !recording_) {
        return;
    }

    if (instance_count <= 1 && first_instance == 0) {
        deferred_ctx_->DrawIndexed(index_count, first_index, base_vertex);
    } else {
        deferred_ctx_->DrawIndexedInstanced(
            index_count, instance_count, first_index, base_vertex, first_instance);
    }
}

} // namespace mine::gfx::d3d11
