/**
 * @file D3D11CommandList.h
 * @brief D3D11 命令录制实现（基于 D3D11 延迟上下文）。
 */

#pragma once

#include "D3D11Helpers.h"
#include "D3D11Texture.h"
#include "D3D11Buffer.h"
#include <mine/gfx/ICommandList.h>

namespace mine::gfx::d3d11 {

/**
 * @brief ICommandList 的 D3D11 实现，基于延迟上下文（Deferred Context）。
 *
 * 录制流程：
 *   1. begin()  — 清空上一帧生成的 ID3D11CommandList，重置延迟上下文状态
 *   2. 录制命令（set_render_target / clear / draw 等）→ 写入延迟上下文
 *   3. end()    — 调用 FinishCommandList() 生成 ID3D11CommandList 对象
 *   4. 通过 get_d3d11_command_list() 供 D3D11Queue::submit() 提交执行
 *
 * 线程安全：同一 D3D11CommandList 实例不应被多线程同时操作。
 */
class D3D11CommandList final : public ICommandList {
public:
    /**
     * @brief 构造延迟命令录制列表。
     * @param device  D3D11 设备（用于创建延迟上下文）
     */
    explicit D3D11CommandList(ID3D11Device* device);

    ~D3D11CommandList() override = default;

    D3D11CommandList(const D3D11CommandList&)            = delete;
    D3D11CommandList& operator=(const D3D11CommandList&) = delete;

    // ── ICommandList 接口 ─────────────────────────────────────────────────

    void begin() override;
    void end()   override;

    void set_render_target(ITexture* rt, ITexture* depth = nullptr) override;
    void clear_render_target(ITexture* rt, const Color4f& color)    override;
    void clear_depth_stencil(
        ITexture* depth_stencil,
        float     depth_value   = 1.0f,
        uint8_t   stencil_value = 0)                                override;

    void set_viewport(const Viewport&    viewport) override;
    void set_scissor (const ScissorRect& rect)     override;

    void set_pipeline    (IPipeline* pipeline)               override;
    void set_vertex_buffer(uint32_t slot, IBuffer* buffer, uint64_t offset = 0) override;
    void set_index_buffer (IBuffer* buffer, uint64_t offset = 0, bool use32bit = false) override;

    void draw(uint32_t vertex_count, uint32_t instance_count = 1,
              uint32_t first_vertex = 0, uint32_t first_instance = 0) override;
    void draw_indexed(uint32_t index_count, uint32_t instance_count = 1,
                      uint32_t first_index = 0, int32_t base_vertex = 0,
                      uint32_t first_instance = 0) override;

    // ── D3D11 内部访问器 ──────────────────────────────────────────────────

    /**
     * @brief 获取 end() 生成的 D3D11 命令列表对象（供 D3D11Queue 提交）。
     * @return 有效时返回 ID3D11CommandList*，begin() 或未调用 end() 时为 nullptr
     */
    [[nodiscard]] ID3D11CommandList* get_d3d11_command_list() const noexcept {
        return d3d11_cmd_list_.Get();
    }

    /// 判断是否处于录制中（begin() 后 end() 前）
    [[nodiscard]] bool is_recording() const noexcept { return recording_; }

private:
    /// D3D11 延迟上下文（每个命令列表独占一个）
    ComPtr<ID3D11DeviceContext>  deferred_ctx_;
    /// end() 调用后生成的 D3D11 原生命令列表
    ComPtr<ID3D11CommandList>    d3d11_cmd_list_;
    /// 是否正在录制（防止重复 begin/end）
    bool                         recording_{false};
};

} // namespace mine::gfx::d3d11
