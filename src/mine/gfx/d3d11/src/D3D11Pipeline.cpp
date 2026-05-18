/**
 * @file D3D11Pipeline.cpp
 * @brief D3D11 图形管线状态对象实现。
 *
 * 使用 D3DCompile 在运行时编译 HLSL（M0 阶段），
 * 后续工具链（tool.shadercc）将改为构建期预编译字节码。
 */

#include "D3D11Pipeline.h"

// D3DCompile 运行时着色器编译（需链接 d3dcompiler.lib）
#include <d3dcompiler.h>

namespace mine::gfx::d3d11 {

// ── 静态辅助：格式转换 ────────────────────────────────────────────────────────

DXGI_FORMAT D3D11Pipeline::to_dxgi_format(VertexElementFormat fmt) noexcept {
    switch (fmt) {
    case VertexElementFormat::Float2:       return DXGI_FORMAT_R32G32_FLOAT;
    case VertexElementFormat::Float3:       return DXGI_FORMAT_R32G32B32_FLOAT;
    case VertexElementFormat::Float4:       return DXGI_FORMAT_R32G32B32A32_FLOAT;
    case VertexElementFormat::UByte4_UNorm: return DXGI_FORMAT_R8G8B8A8_UNORM;
    default:                                return DXGI_FORMAT_UNKNOWN;
    }
}

const char* D3D11Pipeline::to_semantic_name(VertexSemantic sem) noexcept {
    switch (sem) {
    case VertexSemantic::Position: return "POSITION";
    case VertexSemantic::Color:    return "COLOR";
    case VertexSemantic::TexCoord: return "TEXCOORD";
    case VertexSemantic::Normal:   return "NORMAL";
    default:                       return "POSITION";
    }
}

// ── 静态辅助：HLSL 编译 ───────────────────────────────────────────────────────

bool D3D11Pipeline::compile_hlsl(
    const char*       source,
    const char*       entry_point,
    const char*       target,
    ComPtr<ID3DBlob>& out_blob) noexcept {

    ComPtr<ID3DBlob> error_blob;

    // 编译标志：Debug 模式关闭优化，Release 模式开启 O2 优化
    UINT compile_flags = D3DCOMPILE_ENABLE_STRICTNESS;
#if defined(_DEBUG)
    compile_flags |= D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION;
#else
    compile_flags |= D3DCOMPILE_OPTIMIZATION_LEVEL2;
#endif

    const HRESULT hr = ::D3DCompile(
        source,             // 着色器源码
        strlen(source),     // 源码长度
        nullptr,            // 源文件名（nullptr=不显示文件名）
        nullptr,            // 宏定义（暂无）
        nullptr,            // include 处理器（暂无）
        entry_point,        // 入口函数名
        target,             // 目标字符串（"vs_5_0" / "ps_5_0"）
        compile_flags,
        0,                  // 效果编译标志（不使用 fx）
        &out_blob,
        &error_blob);

    if (FAILED(hr)) {
        if (error_blob) {
            // 输出编译错误信息到调试输出
            OutputDebugStringA("[mine.gfx.d3d11] HLSL 编译错误:\n");
            OutputDebugStringA(static_cast<const char*>(error_blob->GetBufferPointer()));
            OutputDebugStringA("\n");
        }
        return false;
    }

    return true;
}

// ── 构造 ──────────────────────────────────────────────────────────────────────

D3D11Pipeline::D3D11Pipeline(ID3D11Device* device, const PipelineDesc& desc) {
    if (!device) return;

    // ── 1. 编译顶点着色器 ──────────────────────────────────────────────────

    const ShaderDesc& vs_desc = desc.vertex_shader;
    if (!vs_desc.data) {
        OutputDebugStringA("[mine.gfx.d3d11] D3D11Pipeline: 顶点着色器数据为空\n");
        return;
    }

    ComPtr<ID3DBlob> vs_bytecode;

    if (vs_desc.is_source) {
        // 源码模式：运行时编译 HLSL
        if (!compile_hlsl(
                static_cast<const char*>(vs_desc.data),
                vs_desc.entry_point ? vs_desc.entry_point : "main",
                "vs_5_0",
                vs_bytecode)) {
            OutputDebugStringA("[mine.gfx.d3d11] D3D11Pipeline: 顶点着色器编译失败\n");
            return;
        }
    } else {
        // 字节码模式：从预编译数据创建 blob
        const HRESULT hr = ::D3DCreateBlob(vs_desc.size, &vs_bytecode);
        if (FAILED(hr)) return;
        memcpy(vs_bytecode->GetBufferPointer(), vs_desc.data, vs_desc.size);
    }

    // 从字节码创建 D3D11 顶点着色器对象
    HRESULT hr = device->CreateVertexShader(
        vs_bytecode->GetBufferPointer(),
        vs_bytecode->GetBufferSize(),
        nullptr,
        &vs_);
    if (FAILED(hr)) {
        OutputDebugStringA("[mine.gfx.d3d11] D3D11Pipeline: CreateVertexShader 失败\n");
        return;
    }

    // ── 2. 编译像素着色器 ──────────────────────────────────────────────────

    const ShaderDesc& ps_desc = desc.pixel_shader;
    if (!ps_desc.data) {
        OutputDebugStringA("[mine.gfx.d3d11] D3D11Pipeline: 像素着色器数据为空\n");
        return;
    }

    ComPtr<ID3DBlob> ps_bytecode;

    if (ps_desc.is_source) {
        if (!compile_hlsl(
                static_cast<const char*>(ps_desc.data),
                ps_desc.entry_point ? ps_desc.entry_point : "main",
                "ps_5_0",
                ps_bytecode)) {
            OutputDebugStringA("[mine.gfx.d3d11] D3D11Pipeline: 像素着色器编译失败\n");
            return;
        }
    } else {
        hr = ::D3DCreateBlob(ps_desc.size, &ps_bytecode);
        if (FAILED(hr)) return;
        memcpy(ps_bytecode->GetBufferPointer(), ps_desc.data, ps_desc.size);
    }

    hr = device->CreatePixelShader(
        ps_bytecode->GetBufferPointer(),
        ps_bytecode->GetBufferSize(),
        nullptr,
        &ps_);
    if (FAILED(hr)) {
        OutputDebugStringA("[mine.gfx.d3d11] D3D11Pipeline: CreatePixelShader 失败\n");
        return;
    }

    // ── 3. 创建顶点输入布局 ────────────────────────────────────────────────

    if (desc.vertex_element_count > 0) {
        // 将 RHI VertexElement 数组转换为 D3D11_INPUT_ELEMENT_DESC 数组
        D3D11_INPUT_ELEMENT_DESC elements[8]{};
        for (uint32_t i = 0; i < desc.vertex_element_count && i < 8; ++i) {
            const VertexElement& e = desc.vertex_elements[i];
            elements[i].SemanticName         = to_semantic_name(e.semantic);
            elements[i].SemanticIndex        = e.semantic_index;
            elements[i].Format               = to_dxgi_format(e.format);
            elements[i].InputSlot            = e.slot;
            elements[i].AlignedByteOffset    = e.offset;
            elements[i].InputSlotClass       = D3D11_INPUT_PER_VERTEX_DATA;
            elements[i].InstanceDataStepRate = 0;
        }

        // 输入布局必须与顶点着色器字节码配合验证
        hr = device->CreateInputLayout(
            elements,
            desc.vertex_element_count,
            vs_bytecode->GetBufferPointer(),
            vs_bytecode->GetBufferSize(),
            &input_layout_);
        if (FAILED(hr)) {
            OutputDebugStringA("[mine.gfx.d3d11] D3D11Pipeline: CreateInputLayout 失败\n");
            return;
        }
    }

    // ── 4. 混合状态 ────────────────────────────────────────────────────────

    D3D11_BLEND_DESC blend_desc{};
    blend_desc.AlphaToCoverageEnable  = FALSE;
    blend_desc.IndependentBlendEnable = FALSE;

    D3D11_RENDER_TARGET_BLEND_DESC& rt_blend = blend_desc.RenderTarget[0];

    if (desc.enable_blend) {
        // 预乘 Alpha 混合：src=ONE, dst=INV_SRC_ALPHA
        // 颜色已预乘时使用此模式，避免双重预乘
        rt_blend.BlendEnable           = TRUE;
        rt_blend.SrcBlend              = D3D11_BLEND_ONE;
        rt_blend.DestBlend             = D3D11_BLEND_INV_SRC_ALPHA;
        rt_blend.BlendOp               = D3D11_BLEND_OP_ADD;
        rt_blend.SrcBlendAlpha         = D3D11_BLEND_ONE;
        rt_blend.DestBlendAlpha        = D3D11_BLEND_INV_SRC_ALPHA;
        rt_blend.BlendOpAlpha          = D3D11_BLEND_OP_ADD;
        rt_blend.RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;
    } else {
        // 不透明模式：关闭混合，直接覆盖
        rt_blend.BlendEnable           = FALSE;
        rt_blend.RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;
    }

    hr = device->CreateBlendState(&blend_desc, &blend_state_);
    if (FAILED(hr)) {
        OutputDebugStringA("[mine.gfx.d3d11] D3D11Pipeline: CreateBlendState 失败\n");
        return;
    }

    // ── 5. 光栅化状态 ──────────────────────────────────────────────────────

    D3D11_RASTERIZER_DESC raster_desc{};
    raster_desc.FillMode              = D3D11_FILL_SOLID; // 实体填充
    raster_desc.CullMode              = D3D11_CULL_NONE;  // 2D 渲染双面可见
    raster_desc.FrontCounterClockwise = FALSE;
    raster_desc.DepthBias             = 0;
    raster_desc.DepthBiasClamp        = 0.0f;
    raster_desc.SlopeScaledDepthBias  = 0.0f;
    raster_desc.DepthClipEnable       = TRUE;
    raster_desc.ScissorEnable         = FALSE; // M0 暂不使用裁剪矩形
    raster_desc.MultisampleEnable     = FALSE;
    raster_desc.AntialiasedLineEnable = FALSE;

    hr = device->CreateRasterizerState(&raster_desc, &rasterizer_);
    if (FAILED(hr)) {
        OutputDebugStringA("[mine.gfx.d3d11] D3D11Pipeline: CreateRasterizerState 失败\n");
        return;
    }

    // ── 6. 深度/模板状态 ───────────────────────────────────────────────────

    D3D11_DEPTH_STENCIL_DESC ds_desc{};
    ds_desc.DepthEnable    = desc.enable_depth ? TRUE : FALSE;
    ds_desc.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ALL;
    ds_desc.DepthFunc      = D3D11_COMPARISON_LESS;
    ds_desc.StencilEnable  = FALSE; // M0 不使用模板

    hr = device->CreateDepthStencilState(&ds_desc, &depth_stencil_);
    if (FAILED(hr)) {
        OutputDebugStringA("[mine.gfx.d3d11] D3D11Pipeline: CreateDepthStencilState 失败\n");
        return;
    }

    // 所有对象均创建成功
    valid_ = true;
}

} // namespace mine::gfx::d3d11
