/**
 * @file D3D11Pipeline.h
 * @brief D3D11 图形管线状态对象实现（IPipeline 接口）。
 *
 * 封装 D3D11 着色器、顶点输入布局、混合状态、光栅化状态等，
 * 通过 D3D11Device::create_pipeline(PipelineDesc) 创建。
 */

#pragma once

#include "D3D11Helpers.h"
#include <mine/gfx/IPipeline.h>
#include <mine/gfx/GfxTypes.h>

namespace mine::gfx::d3d11 {

/**
 * @brief IPipeline 的 D3D11 实现。
 *
 * 持有以下 D3D11 状态对象：
 *   - ID3D11VertexShader      — 顶点着色器
 *   - ID3D11PixelShader       — 像素着色器
 *   - ID3D11InputLayout       — 顶点输入布局（与 VS 字节码绑定）
 *   - ID3D11BlendState        — 混合状态（Alpha 混合）
 *   - ID3D11RasterizerState   — 光栅化状态（双面渲染，关闭剪裁）
 *   - ID3D11DepthStencilState — 深度/模板状态（2D 绘制通常关闭深度测试）
 *
 * 通过 D3D11CommandList::set_pipeline() 将上述状态一次性绑定到延迟上下文。
 */
class D3D11Pipeline final : public IPipeline {
public:
    /**
     * @brief 从 PipelineDesc 编译并构建管线状态对象。
     *
     * @param device  D3D11 设备（用于创建状态对象）
     * @param desc    管线描述符（含着色器源码、顶点布局等）
     *
     * 构造后通过 is_valid() 判断是否成功。
     * 失败原因：HLSL 编译错误、输入布局不匹配、系统资源不足等。
     */
    D3D11Pipeline(ID3D11Device* device, const PipelineDesc& desc);

    ~D3D11Pipeline() override = default;

    D3D11Pipeline(const D3D11Pipeline&)            = delete;
    D3D11Pipeline& operator=(const D3D11Pipeline&) = delete;

    // ── IPipeline 接口 ────────────────────────────────────────────────────

    [[nodiscard]] PipelineType type() const noexcept override {
        return PipelineType::Graphics;
    }

    // ── D3D11 内部访问器 ──────────────────────────────────────────────────

    /// 管线是否构建成功（所有状态对象均已创建）
    [[nodiscard]] bool is_valid() const noexcept { return valid_; }

    /// 顶点着色器
    [[nodiscard]] ID3D11VertexShader*      vertex_shader()   const noexcept { return vs_.Get(); }
    /// 像素着色器
    [[nodiscard]] ID3D11PixelShader*       pixel_shader()    const noexcept { return ps_.Get(); }
    /// 顶点输入布局
    [[nodiscard]] ID3D11InputLayout*       input_layout()    const noexcept { return input_layout_.Get(); }
    /// 混合状态
    [[nodiscard]] ID3D11BlendState*        blend_state()     const noexcept { return blend_state_.Get(); }
    /// 光栅化状态
    [[nodiscard]] ID3D11RasterizerState*   rasterizer_state()const noexcept { return rasterizer_.Get(); }
    /// 深度/模板状态
    [[nodiscard]] ID3D11DepthStencilState* depth_stencil_state()const noexcept { return depth_stencil_.Get(); }

private:
    /**
     * @brief 将 RHI VertexElementFormat 转换为 DXGI_FORMAT。
     */
    static DXGI_FORMAT to_dxgi_format(VertexElementFormat fmt) noexcept;

    /**
     * @brief 将 RHI VertexSemantic 转换为 D3D11 语义字符串。
     */
    static const char* to_semantic_name(VertexSemantic sem) noexcept;

    /**
     * @brief 编译 HLSL 源码，返回字节码 blob。
     * @param source      HLSL 源码字符串（null 结尾）
     * @param entry_point 入口函数名
     * @param target      着色器目标（如 "vs_5_0", "ps_5_0"）
     * @param out_blob    输出字节码 blob
     * @return true 表示编译成功
     */
    static bool compile_hlsl(
        const char*         source,
        const char*         entry_point,
        const char*         target,
        ComPtr<ID3DBlob>&   out_blob) noexcept;

    bool valid_{false};

    ComPtr<ID3D11VertexShader>      vs_;
    ComPtr<ID3D11PixelShader>       ps_;
    ComPtr<ID3D11InputLayout>       input_layout_;
    ComPtr<ID3D11BlendState>        blend_state_;
    ComPtr<ID3D11RasterizerState>   rasterizer_;
    ComPtr<ID3D11DepthStencilState> depth_stencil_;
};

} // namespace mine::gfx::d3d11
