/**
 * @file RhiRenderer.cpp
 * @brief mine.paint 基于 RHI 的 2D 渲染器实现。
 *
 * 通过 mine.gfx.rhi 抽象层（IDevice、ICommandList、IPipeline 等）
 * 实现 IRenderer 接口，不依赖任何具体图形 API（D3D11/D3D12/Vulkan）。
 *
 * M0 阶段支持：
 *   - FillRect / StrokeRect / FillRoundedRect / StrokeRoundedRect 等 SDF 形状渲染
 *   - 亚克力背景模糊（高斯模糊分离通道）
 *   - 文字渲染（字形图集采样 + 统一软裁剪）
 *   - 裁剪系统（ClipSdfCB 像素着色器软裁剪，无模板缓冲锯齿）
 *
 * 渲染架构：
 *   - 着色器字节码由 build_shaders.ps1 预编译，通过 ShaderBytecode.h 引入
 *   - 顶点格式和常量缓冲结构体定义在 PaintShaderTypes.h
 *   - 字形图集管理见 GlyphAtlas.h/cpp
 *   - Path 扁平化辅助见 PathFlattener.h/cpp
 *
 * SDF kind 常量：
 *   0=矩形, 1=均匀圆角矩形, 2=四角独立圆角矩形, 3=椭圆
 *   4=四边不等宽矩形内侧描边, 5=四边不等宽+四角独立圆角内侧描边
 *   6=线段描边, 7=圆弧描边, 8=二次贝塞尔描边, 9=三次贝塞尔描边
 */

#include "RhiRenderer.h"
#include "PaintShaderTypes.h"
#include "GlyphAtlas.h"
#include "PathFlattener.h"

#include <mine/paint/IRenderer.h>
#include <mine/paint/DisplayList.h>
#include <mine/paint/DrawCmd.h>
#include <mine/gfx/IDevice.h>
#include <mine/gfx/IQueue.h>
#include <mine/gfx/ICommandList.h>
#include <mine/gfx/IPipeline.h>
#include <mine/gfx/IBuffer.h>
#include <mine/gfx/ITexture.h>
#include <mine/gfx/GfxTypes.h>
#include <mine/core/Memory.h>
#include <mine/containers/Vector.h>
#include <mine/math/Vec2.h>
#include <mine/text/FontFace.h>
#include <algorithm>
#include <cmath>

// 预编译着色器字节码（由 scripts/build_shaders.ps1 生成）
#include <mine/gfx/d3d11/ShaderBytecode.h>

namespace mine::paint {

inline constexpr const auto& g_ps_basic_bytecode = mine::gfx::d3d11::shaders::g_ps_basic_bytecode;
inline constexpr const auto& g_ps_blur_bytecode = mine::gfx::d3d11::shaders::g_ps_blur_bytecode;
inline constexpr const auto& g_ps_sdf_bytecode = mine::gfx::d3d11::shaders::g_ps_sdf_bytecode;
inline constexpr const auto& g_ps_sdf_clip_bytecode = mine::gfx::d3d11::shaders::g_ps_sdf_clip_bytecode;
inline constexpr const auto& g_ps_text_bytecode = mine::gfx::d3d11::shaders::g_ps_text_bytecode;
inline constexpr const auto& g_vs_basic_bytecode = mine::gfx::d3d11::shaders::g_vs_basic_bytecode;
inline constexpr const auto& g_vs_blur_bytecode = mine::gfx::d3d11::shaders::g_vs_blur_bytecode;
inline constexpr const auto& g_vs_sdf_bytecode = mine::gfx::d3d11::shaders::g_vs_sdf_bytecode;
inline constexpr const auto& g_vs_text_bytecode = mine::gfx::d3d11::shaders::g_vs_text_bytecode;

// ── 已提取的代码模块 ─────────────────────────────────────────────────────────
// 顶点格式与常量缓冲 → PaintShaderTypes.h
// 字形图集管理       → GlyphAtlas.h / GlyphAtlas.cpp
// Path 扁平化辅助    → PathFlattener.h / PathFlattener.cpp

// ── 构造 ──────────────────────────────────────────────────────────────────────

RhiRenderer::RhiRenderer(gfx::IDevice* device)
    : device_(device) {

    if (!device_) return;

    // 创建图形队列（用于提交录制好的命令列表）
    queue_ = device_->create_queue(gfx::QueueType::Graphics);
    if (!queue_) return;

    // 创建命令录制列表
    cmd_list_ = device_->create_command_list(gfx::QueueType::Graphics);
    if (!cmd_list_) return;

    // 创建折线描边管线（StrokeLine）
    if (!create_solid_pipeline()) return;

    // 创建 SDF 形状管线（矩形/圆角/椭圆的填充与描边）
    if (!create_sdf_pipeline()) return;

    // 创建文字渲染管线（字形图集四边形采样）
    if (!create_text_pipeline()) return;

    // 创建高斯模糊管线（亚克力背景模糊用，全屏四边形，方向由 BlurCB 控制）
    if (!create_blur_pipeline()) return;

    // 创建裁剪系统管线（模板缓冲：ClipWrite / ClipErase / ClipTest 变体）
    if (!create_clip_pipelines()) return;

    // 创建字形图集管理器（1024×1024 R8 灰度图集）
    glyph_atlas_ = core::OwnedPtr<GlyphAtlas>(
        MINE_NEW(GlyphAtlas),
        &core::detail::typed_deleter<GlyphAtlas>);

    valid_ = true;
}

// ── 管线创建 ──────────────────────────────────────────────────────────────────

bool RhiRenderer::create_solid_pipeline() {
    // 顶点布局：POSITION（float2）+ COLOR（float4），共 24 字节
    gfx::PipelineDesc desc{};

    desc.vertex_shader.data = g_vs_basic_bytecode;
    desc.vertex_shader.size        = sizeof(g_vs_basic_bytecode);
    desc.vertex_shader.language    = gfx::ShaderLanguage::HLSL;
    desc.vertex_shader.entry_point = "main";
    desc.vertex_shader.is_source   = false;

    desc.pixel_shader.data = g_ps_basic_bytecode;
    desc.pixel_shader.size        = sizeof(g_ps_basic_bytecode);
    desc.pixel_shader.language    = gfx::ShaderLanguage::HLSL;
    desc.pixel_shader.entry_point = "main";
    desc.pixel_shader.is_source   = false;

    desc.vertex_element_count = 2;
    desc.vertex_elements[0].semantic       = gfx::VertexSemantic::Position;
    desc.vertex_elements[0].semantic_index = 0;
    desc.vertex_elements[0].format         = gfx::VertexElementFormat::Float2;
    desc.vertex_elements[0].slot           = 0;
    desc.vertex_elements[0].offset         = 0;

    desc.vertex_elements[1].semantic       = gfx::VertexSemantic::Color;
    desc.vertex_elements[1].semantic_index = 0;
    desc.vertex_elements[1].format         = gfx::VertexElementFormat::Float4;
    desc.vertex_elements[1].slot           = 0;
    desc.vertex_elements[1].offset         = sizeof(float) * 2;

    desc.vertex_stride = sizeof(PaintVertex); // 24 字节
    desc.enable_blend  = true;
    desc.enable_depth  = false;

    solid_pipeline_ = device_->create_pipeline(desc);
    return solid_pipeline_ != nullptr;
}

bool RhiRenderer::create_sdf_pipeline() {
    // 顶点布局：SdfVertex（64 字节），5 个顶点元素
    gfx::PipelineDesc desc{};

    desc.vertex_shader.data = g_vs_sdf_bytecode;
    desc.vertex_shader.size        = sizeof(g_vs_sdf_bytecode);
    desc.vertex_shader.language    = gfx::ShaderLanguage::HLSL;
    desc.vertex_shader.entry_point = "main";
    desc.vertex_shader.is_source   = false;

    desc.pixel_shader.data = g_ps_sdf_bytecode;
    desc.pixel_shader.size        = sizeof(g_ps_sdf_bytecode);
    desc.pixel_shader.language    = gfx::ShaderLanguage::HLSL;
    desc.pixel_shader.entry_point = "main";
    desc.pixel_shader.is_source   = false;

    // POSITION (float2) — offset 0，屏幕像素坐标
    desc.vertex_elements[0].semantic       = gfx::VertexSemantic::Position;
    desc.vertex_elements[0].semantic_index = 0;
    desc.vertex_elements[0].format         = gfx::VertexElementFormat::Float2;
    desc.vertex_elements[0].slot           = 0;
    desc.vertex_elements[0].offset         = 0;

    // COLOR (float4) — offset 8，线性 RGBA 颜色
    desc.vertex_elements[1].semantic       = gfx::VertexSemantic::Color;
    desc.vertex_elements[1].semantic_index = 0;
    desc.vertex_elements[1].format         = gfx::VertexElementFormat::Float4;
    desc.vertex_elements[1].slot           = 0;
    desc.vertex_elements[1].offset         = 8;

    // TEXCOORD0 (float2) — offset 24，本地坐标（形状中心为原点）
    desc.vertex_elements[2].semantic       = gfx::VertexSemantic::TexCoord;
    desc.vertex_elements[2].semantic_index = 0;
    desc.vertex_elements[2].format         = gfx::VertexElementFormat::Float2;
    desc.vertex_elements[2].slot           = 0;
    desc.vertex_elements[2].offset         = 24;

    // TEXCOORD1 (float4) — offset 32，(kind, half_w, half_h, p0)
    desc.vertex_elements[3].semantic       = gfx::VertexSemantic::TexCoord;
    desc.vertex_elements[3].semantic_index = 1;
    desc.vertex_elements[3].format         = gfx::VertexElementFormat::Float4;
    desc.vertex_elements[3].slot           = 0;
    desc.vertex_elements[3].offset         = 32;

    // TEXCOORD2 (float4) — offset 48，(p1, p2, p3, stroke_w)
    desc.vertex_elements[4].semantic       = gfx::VertexSemantic::TexCoord;
    desc.vertex_elements[4].semantic_index = 2;
    desc.vertex_elements[4].format         = gfx::VertexElementFormat::Float4;
    desc.vertex_elements[4].slot           = 0;
    desc.vertex_elements[4].offset         = 48;

    // TEXCOORD3 (float4) — offset 64，扩展参数（四角圆角半径：e0=r_tl, e1=r_tr, e2=r_br, e3=r_bl）
    desc.vertex_elements[5].semantic       = gfx::VertexSemantic::TexCoord;
    desc.vertex_elements[5].semantic_index = 3;
    desc.vertex_elements[5].format         = gfx::VertexElementFormat::Float4;
    desc.vertex_elements[5].slot           = 0;
    desc.vertex_elements[5].offset         = 64;

    desc.vertex_element_count = 6;
    desc.vertex_stride        = sizeof(SdfVertex); // 80 字节
    desc.enable_blend         = true;              // 预乘 Alpha 混合
    desc.enable_depth         = false;

    sdf_pipeline_ = device_->create_pipeline(desc);
    return sdf_pipeline_ != nullptr;
}

bool RhiRenderer::create_text_pipeline() {
    // 文字顶点布局：POSITION(float2) + TEXCOORD0(float2) + COLOR(float4) = 32 字节
    gfx::PipelineDesc desc{};

    desc.vertex_shader.data = g_vs_text_bytecode;
    desc.vertex_shader.size        = sizeof(g_vs_text_bytecode);
    desc.vertex_shader.language    = gfx::ShaderLanguage::HLSL;
    desc.vertex_shader.entry_point = "main";
    desc.vertex_shader.is_source   = false;

    desc.pixel_shader.data = g_ps_text_bytecode;
    desc.pixel_shader.size        = sizeof(g_ps_text_bytecode);
    desc.pixel_shader.language    = gfx::ShaderLanguage::HLSL;
    desc.pixel_shader.entry_point = "main";
    desc.pixel_shader.is_source   = false;

    // POSITION (float2) — offset 0：屏幕像素坐标
    desc.vertex_elements[0].semantic       = gfx::VertexSemantic::Position;
    desc.vertex_elements[0].semantic_index = 0;
    desc.vertex_elements[0].format         = gfx::VertexElementFormat::Float2;
    desc.vertex_elements[0].slot           = 0;
    desc.vertex_elements[0].offset         = 0;

    // TEXCOORD0 (float2) — offset 8：字形图集 UV
    desc.vertex_elements[1].semantic       = gfx::VertexSemantic::TexCoord;
    desc.vertex_elements[1].semantic_index = 0;
    desc.vertex_elements[1].format         = gfx::VertexElementFormat::Float2;
    desc.vertex_elements[1].slot           = 0;
    desc.vertex_elements[1].offset         = offsetof(TextVertex, u);

    // COLOR (float4) — offset 16：线性 RGBA 颜色
    desc.vertex_elements[2].semantic       = gfx::VertexSemantic::Color;
    desc.vertex_elements[2].semantic_index = 0;
    desc.vertex_elements[2].format         = gfx::VertexElementFormat::Float4;
    desc.vertex_elements[2].slot           = 0;
    desc.vertex_elements[2].offset         = offsetof(TextVertex, r);

    desc.vertex_element_count = 3;
    desc.vertex_stride        = sizeof(TextVertex);  // 32 字节
    desc.enable_blend         = true;               // 预乘 Alpha 混合
    desc.enable_depth         = false;

    text_pipeline_ = device_->create_pipeline(desc);
    return text_pipeline_ != nullptr;
}

bool RhiRenderer::create_blur_pipeline() {
    // 顶点布局：BlurVertex（16 字节）：POSITION(float2) + TEXCOORD0(float2)
    gfx::PipelineDesc desc{};

    desc.vertex_shader.data = g_vs_blur_bytecode;
    desc.vertex_shader.size        = sizeof(g_vs_blur_bytecode);
    desc.vertex_shader.language    = gfx::ShaderLanguage::HLSL;
    desc.vertex_shader.entry_point = "main";
    desc.vertex_shader.is_source   = false;

    desc.pixel_shader.data = g_ps_blur_bytecode;
    desc.pixel_shader.size        = sizeof(g_ps_blur_bytecode);
    desc.pixel_shader.language    = gfx::ShaderLanguage::HLSL;
    desc.pixel_shader.entry_point = "main";
    desc.pixel_shader.is_source   = false;

    // POSITION (float2) — offset 0：NDC 坐标
    desc.vertex_elements[0].semantic       = gfx::VertexSemantic::Position;
    desc.vertex_elements[0].semantic_index = 0;
    desc.vertex_elements[0].format         = gfx::VertexElementFormat::Float2;
    desc.vertex_elements[0].slot           = 0;
    desc.vertex_elements[0].offset         = 0;

    // TEXCOORD0 (float2) — offset 8：源纹理 UV
    desc.vertex_elements[1].semantic       = gfx::VertexSemantic::TexCoord;
    desc.vertex_elements[1].semantic_index = 0;
    desc.vertex_elements[1].format         = gfx::VertexElementFormat::Float2;
    desc.vertex_elements[1].slot           = 0;
    desc.vertex_elements[1].offset         = offsetof(BlurVertex, u);

    desc.vertex_element_count = 2;
    desc.vertex_stride        = sizeof(BlurVertex);  // 16 字节
    desc.enable_blend         = false;               // 模糊通道不需要混合（覆写 scratch 纹理）
    desc.enable_depth         = false;

    blur_pipeline_ = device_->create_pipeline(desc);
    return blur_pipeline_ != nullptr;
}

bool RhiRenderer::create_clip_pipelines() {
    // ── SDF 顶点元素描述（与 create_sdf_pipeline 完全相同，供各管线共用）───

    // 辅助：填充 SDF 管线顶点布局（6 个顶点元素，80 字节 stride）
    auto fill_sdf_layout = [](gfx::PipelineDesc& d) {
        // POSITION (float2) — offset 0
        d.vertex_elements[0].semantic       = gfx::VertexSemantic::Position;
        d.vertex_elements[0].semantic_index = 0;
        d.vertex_elements[0].format         = gfx::VertexElementFormat::Float2;
        d.vertex_elements[0].slot           = 0;
        d.vertex_elements[0].offset         = 0;
        // COLOR (float4) — offset 8
        d.vertex_elements[1].semantic       = gfx::VertexSemantic::Color;
        d.vertex_elements[1].semantic_index = 0;
        d.vertex_elements[1].format         = gfx::VertexElementFormat::Float4;
        d.vertex_elements[1].slot           = 0;
        d.vertex_elements[1].offset         = 8;
        // TEXCOORD0 (float2) — offset 24
        d.vertex_elements[2].semantic       = gfx::VertexSemantic::TexCoord;
        d.vertex_elements[2].semantic_index = 0;
        d.vertex_elements[2].format         = gfx::VertexElementFormat::Float2;
        d.vertex_elements[2].slot           = 0;
        d.vertex_elements[2].offset         = 24;
        // TEXCOORD1 (float4) — offset 32
        d.vertex_elements[3].semantic       = gfx::VertexSemantic::TexCoord;
        d.vertex_elements[3].semantic_index = 1;
        d.vertex_elements[3].format         = gfx::VertexElementFormat::Float4;
        d.vertex_elements[3].slot           = 0;
        d.vertex_elements[3].offset         = 32;
        // TEXCOORD2 (float4) — offset 48
        d.vertex_elements[4].semantic       = gfx::VertexSemantic::TexCoord;
        d.vertex_elements[4].semantic_index = 2;
        d.vertex_elements[4].format         = gfx::VertexElementFormat::Float4;
        d.vertex_elements[4].slot           = 0;
        d.vertex_elements[4].offset         = 48;
        // TEXCOORD3 (float4) — offset 64
        d.vertex_elements[5].semantic       = gfx::VertexSemantic::TexCoord;
        d.vertex_elements[5].semantic_index = 3;
        d.vertex_elements[5].format         = gfx::VertexElementFormat::Float4;
        d.vertex_elements[5].slot           = 0;
        d.vertex_elements[5].offset         = 64;
        d.vertex_element_count = 6;
        d.vertex_stride        = sizeof(SdfVertex);  // 80 字节
    };

    // ── 1. SDF ClipWrite 管线（压入裁剪层：IncrSat 写入模板，禁止颜色输出）──

    {
        gfx::PipelineDesc desc{};
        desc.vertex_shader.data = g_vs_sdf_bytecode;
        desc.vertex_shader.size        = sizeof(g_vs_sdf_bytecode);
        desc.vertex_shader.language    = gfx::ShaderLanguage::HLSL;
        desc.vertex_shader.entry_point = "main";
        desc.vertex_shader.is_source   = false;

        desc.pixel_shader.data = g_ps_sdf_clip_bytecode;
        desc.pixel_shader.size        = sizeof(g_ps_sdf_clip_bytecode);
        desc.pixel_shader.language    = gfx::ShaderLanguage::HLSL;
        desc.pixel_shader.entry_point = "main";
        desc.pixel_shader.is_source   = false;

        fill_sdf_layout(desc);
        desc.enable_blend    = false;
        desc.enable_depth    = false;
        desc.stencil_mode    = gfx::StencilMode::ClipWrite;

        sdf_clip_write_pipeline_ = device_->create_pipeline(desc);
        if (!sdf_clip_write_pipeline_) return false;
    }

    // ── 2. SDF ClipErase 管线（弹出裁剪层：DecrSat 写入模板，禁止颜色输出）─

    {
        gfx::PipelineDesc desc{};
        desc.vertex_shader.data = g_vs_sdf_bytecode;
        desc.vertex_shader.size        = sizeof(g_vs_sdf_bytecode);
        desc.vertex_shader.language    = gfx::ShaderLanguage::HLSL;
        desc.vertex_shader.entry_point = "main";
        desc.vertex_shader.is_source   = false;

        desc.pixel_shader.data = g_ps_sdf_clip_bytecode;
        desc.pixel_shader.size        = sizeof(g_ps_sdf_clip_bytecode);
        desc.pixel_shader.language    = gfx::ShaderLanguage::HLSL;
        desc.pixel_shader.entry_point = "main";
        desc.pixel_shader.is_source   = false;

        fill_sdf_layout(desc);
        desc.enable_blend    = false;
        desc.enable_depth    = false;
        desc.stencil_mode    = gfx::StencilMode::ClipErase;

        sdf_clip_erase_pipeline_ = device_->create_pipeline(desc);
        if (!sdf_clip_erase_pipeline_) return false;
    }

    // ── 3. SDF ClipTest 管线（裁剪状态下普通 SDF 绘制：Equal 测试，Keep）────

    {
        gfx::PipelineDesc desc{};
        desc.vertex_shader.data = g_vs_sdf_bytecode;
        desc.vertex_shader.size        = sizeof(g_vs_sdf_bytecode);
        desc.vertex_shader.language    = gfx::ShaderLanguage::HLSL;
        desc.vertex_shader.entry_point = "main";
        desc.vertex_shader.is_source   = false;

        desc.pixel_shader.data = g_ps_sdf_bytecode;
        desc.pixel_shader.size        = sizeof(g_ps_sdf_bytecode);
        desc.pixel_shader.language    = gfx::ShaderLanguage::HLSL;
        desc.pixel_shader.entry_point = "main";
        desc.pixel_shader.is_source   = false;

        fill_sdf_layout(desc);
        desc.enable_blend    = true;   // 预乘 Alpha 混合（正常颜色输出）
        desc.enable_depth    = false;
        desc.stencil_mode    = gfx::StencilMode::ClipTest;

        sdf_clip_test_pipeline_ = device_->create_pipeline(desc);
        if (!sdf_clip_test_pipeline_) return false;
    }

    // ── 4. 文字 ClipTest 管线（裁剪状态下文字渲染）───────────────────────────

    {
        gfx::PipelineDesc desc{};
        desc.vertex_shader.data = g_vs_text_bytecode;
        desc.vertex_shader.size        = sizeof(g_vs_text_bytecode);
        desc.vertex_shader.language    = gfx::ShaderLanguage::HLSL;
        desc.vertex_shader.entry_point = "main";
        desc.vertex_shader.is_source   = false;

        desc.pixel_shader.data = g_ps_text_bytecode;
        desc.pixel_shader.size        = sizeof(g_ps_text_bytecode);
        desc.pixel_shader.language    = gfx::ShaderLanguage::HLSL;
        desc.pixel_shader.entry_point = "main";
        desc.pixel_shader.is_source   = false;

        // POSITION (float2) — offset 0
        desc.vertex_elements[0].semantic       = gfx::VertexSemantic::Position;
        desc.vertex_elements[0].semantic_index = 0;
        desc.vertex_elements[0].format         = gfx::VertexElementFormat::Float2;
        desc.vertex_elements[0].slot           = 0;
        desc.vertex_elements[0].offset         = 0;
        // TEXCOORD0 (float2) — offset 8
        desc.vertex_elements[1].semantic       = gfx::VertexSemantic::TexCoord;
        desc.vertex_elements[1].semantic_index = 0;
        desc.vertex_elements[1].format         = gfx::VertexElementFormat::Float2;
        desc.vertex_elements[1].slot           = 0;
        desc.vertex_elements[1].offset         = offsetof(TextVertex, u);
        // COLOR (float4) — offset 16
        desc.vertex_elements[2].semantic       = gfx::VertexSemantic::Color;
        desc.vertex_elements[2].semantic_index = 0;
        desc.vertex_elements[2].format         = gfx::VertexElementFormat::Float4;
        desc.vertex_elements[2].slot           = 0;
        desc.vertex_elements[2].offset         = offsetof(TextVertex, r);

        desc.vertex_element_count = 3;
        desc.vertex_stride        = sizeof(TextVertex);
        desc.enable_blend         = true;
        desc.enable_depth         = false;
        desc.stencil_mode         = gfx::StencilMode::ClipTest;

        text_clip_test_pipeline_ = device_->create_pipeline(desc);
        if (!text_clip_test_pipeline_) return false;
    }

    // ── 5. 折线 ClipTest 管线（裁剪状态下折线描边绘制）──────────────────────

    {
        gfx::PipelineDesc desc{};
        desc.vertex_shader.data = g_vs_basic_bytecode;
        desc.vertex_shader.size        = sizeof(g_vs_basic_bytecode);
        desc.vertex_shader.language    = gfx::ShaderLanguage::HLSL;
        desc.vertex_shader.entry_point = "main";
        desc.vertex_shader.is_source   = false;

        desc.pixel_shader.data = g_ps_basic_bytecode;
        desc.pixel_shader.size        = sizeof(g_ps_basic_bytecode);
        desc.pixel_shader.language    = gfx::ShaderLanguage::HLSL;
        desc.pixel_shader.entry_point = "main";
        desc.pixel_shader.is_source   = false;

        desc.vertex_elements[0].semantic       = gfx::VertexSemantic::Position;
        desc.vertex_elements[0].semantic_index = 0;
        desc.vertex_elements[0].format         = gfx::VertexElementFormat::Float2;
        desc.vertex_elements[0].slot           = 0;
        desc.vertex_elements[0].offset         = 0;
        desc.vertex_elements[1].semantic       = gfx::VertexSemantic::Color;
        desc.vertex_elements[1].semantic_index = 0;
        desc.vertex_elements[1].format         = gfx::VertexElementFormat::Float4;
        desc.vertex_elements[1].slot           = 0;
        desc.vertex_elements[1].offset         = sizeof(float) * 2;

        desc.vertex_element_count = 2;
        desc.vertex_stride        = sizeof(PaintVertex);
        desc.enable_blend         = true;
        desc.enable_depth         = false;
        desc.stencil_mode         = gfx::StencilMode::ClipTest;

        solid_clip_test_pipeline_ = device_->create_pipeline(desc);
        if (!solid_clip_test_pipeline_) return false;
    }

    return true;
}

bool RhiRenderer::ensure_stencil_texture(gfx::ITexture* target) {
    const uint32_t w = target->width();
    const uint32_t h = target->height();

    // 若尺寸未变且已存在，直接复用
    if (clip_stencil_ &&
        clip_stencil_->width()  == w &&
        clip_stencil_->height() == h) {
        return true;
    }

    // 创建 D24_UNorm_S8_UInt 深度模板纹理（DepthStencil 绑定）
    gfx::TextureDesc ds_desc{};
    ds_desc.width      = w;
    ds_desc.height     = h;
    ds_desc.format     = gfx::PixelFormat::D24_UNorm_S8_UInt;
    ds_desc.bind_flags = gfx::TextureBindFlags::DepthStencil;
    ds_desc.mip_levels = 1;
    ds_desc.array_size = 1;

    clip_stencil_ = device_->create_texture(ds_desc);
    return clip_stencil_ != nullptr;
}

// ── 帧生命周期 ────────────────────────────────────────────────────────────────

void RhiRenderer::begin_frame() {
    if (!valid_) return;
    // 开始录制新一帧的 GPU 命令
    cmd_list_->begin();
}

void RhiRenderer::end_frame() {
    if (!valid_) return;
    // 结束录制，生成命令列表对象
    cmd_list_->end();
    // 将命令列表提交到 GPU 执行
    queue_->submit(cmd_list_.get());
    // 等待本帧 GPU 执行完毕（M0 简化同步模型，生产环境改用 IFence 异步同步）
    queue_->wait_idle();
}

// ── 顶点生成辅助 ─────────────────────────────────────────────────────────────

void RhiRenderer::push_sdf_quad_vertices(
    containers::Vector<SdfVertex>& vertices,
    float cx, float cy,
    float half_w, float half_h, float padding,
    float r, float g, float b, float a,
    float kind,
    float p0, float p1, float p2, float p3,
    float stroke_w,
    float e0, float e1, float e2, float e3)
{
    // 包围盒左上角和右下角（含 padding 扩展）
    const float x1 = cx - half_w - padding;
    const float y1 = cy - half_h - padding;
    const float x2 = cx + half_w + padding;
    const float y2 = cy + half_h + padding;

    // 生成单个顶点（本地坐标 = 屏幕坐标 - 形状中心）
    auto make = [&](float sx, float sy) -> SdfVertex {
        return { sx, sy, r, g, b, a, sx - cx, sy - cy,
                 kind, half_w, half_h, p0, p1, p2, p3, stroke_w,
                 e0, e1, e2, e3 };
    };

    // 三角形 1：左上 - 右上 - 左下
    vertices.push_back(make(x1, y1));
    vertices.push_back(make(x2, y1));
    vertices.push_back(make(x1, y2));
    // 三角形 2：右上 - 右下 - 左下
    vertices.push_back(make(x2, y1));
    vertices.push_back(make(x2, y2));
    vertices.push_back(make(x1, y2));
}

// ── 亚克力辅助函数 ────────────────────────────────────────────────────────────

bool RhiRenderer::ensure_scratch_textures(gfx::ITexture* target) {
    const uint32_t w = target->width();
    const uint32_t h = target->height();

    // 若尺寸未变且已存在，直接复用
    if (blur_scratch_a_ &&
        blur_scratch_a_->width()  == w &&
        blur_scratch_a_->height() == h) {
        return true;
    }

    // 创建两个 scratch 纹理（同时绑定 ShaderResource + RenderTarget）
    gfx::TextureDesc tex_desc{};
    tex_desc.width      = w;
    tex_desc.height     = h;
    tex_desc.format     = gfx::PixelFormat::RGBA8_UNorm;
    tex_desc.bind_flags = gfx::TextureBindFlags::ShaderResource
                        | gfx::TextureBindFlags::RenderTarget;
    tex_desc.mip_levels = 1;
    tex_desc.array_size = 1;

    blur_scratch_a_ = device_->create_texture(tex_desc);
    if (!blur_scratch_a_) return false;

    blur_scratch_b_ = device_->create_texture(tex_desc);
    if (!blur_scratch_b_) {
        blur_scratch_a_.reset();
        return false;
    }
    return true;
}

BrushDataCB RhiRenderer::make_brush_cb(
    const Brush& brush,
    float /*cx*/, float /*cy*/,
    float half_w, float half_h) noexcept
{
    BrushDataCB cb{};

    switch (brush.kind()) {
    case BrushKind::SolidColor:
        cb.brush_kind = 0;
        break;

    case BrushKind::LinearGradient: {
        const auto& lg = brush.linear_gradient_data();
        cb.brush_kind = 1;
        cb.stop_count = lg.stop_count;
        // 归一化坐标 → local 坐标（像素，相对形状中心）
        //   local_x = (norm_x - 0.5) × 2 × half_w
        //   local_y = (norm_y - 0.5) × 2 × half_h
        const float sx = (lg.start.x - 0.5f) * 2.0f * half_w;
        const float sy = (lg.start.y - 0.5f) * 2.0f * half_h;
        const float ex = (lg.end.x   - 0.5f) * 2.0f * half_w;
        const float ey = (lg.end.y   - 0.5f) * 2.0f * half_h;
        cb.grad_params[0] = sx;
        cb.grad_params[1] = sy;
        cb.grad_params[2] = ex - sx;  // 方向向量 x
        cb.grad_params[3] = ey - sy;  // 方向向量 y
        // 色标数据
        for (uint32_t i = 0; i < lg.stop_count && i < kMaxGradientStops; ++i) {
            cb.stop_colors[i][0] = lg.stops[i].color.r;
            cb.stop_colors[i][1] = lg.stops[i].color.g;
            cb.stop_colors[i][2] = lg.stops[i].color.b;
            cb.stop_colors[i][3] = lg.stops[i].color.a;
        }
        cb.stop_offsets[0] = (lg.stop_count > 0) ? lg.stops[0].offset : 0.0f;
        cb.stop_offsets[1] = (lg.stop_count > 1) ? lg.stops[1].offset : 1.0f;
        cb.stop_offsets[2] = (lg.stop_count > 2) ? lg.stops[2].offset : 1.0f;
        cb.stop_offsets[3] = (lg.stop_count > 3) ? lg.stops[3].offset : 1.0f;
        break;
    }

    case BrushKind::RadialGradient: {
        const auto& rg = brush.radial_gradient_data();
        cb.brush_kind = 2;
        cb.stop_count = rg.stop_count;
        // 圆心归一化坐标 → local 坐标
        const float cx2 = (rg.center.x - 0.5f) * 2.0f * half_w;
        const float cy2 = (rg.center.y - 0.5f) * 2.0f * half_h;
        // 半径：使用较短的半边作为基准（1.0 = 较短边的一半）
        const float half_short = (half_w < half_h) ? half_w : half_h;
        cb.grad_params[0] = cx2;
        cb.grad_params[1] = cy2;
        cb.grad_params[2] = rg.outer_radius * half_short;  // 外径（像素）
        cb.grad_params[3] = rg.inner_radius * half_short;  // 内径（像素）
        // 色标数据
        for (uint32_t i = 0; i < rg.stop_count && i < kMaxGradientStops; ++i) {
            cb.stop_colors[i][0] = rg.stops[i].color.r;
            cb.stop_colors[i][1] = rg.stops[i].color.g;
            cb.stop_colors[i][2] = rg.stops[i].color.b;
            cb.stop_colors[i][3] = rg.stops[i].color.a;
        }
        cb.stop_offsets[0] = (rg.stop_count > 0) ? rg.stops[0].offset : 0.0f;
        cb.stop_offsets[1] = (rg.stop_count > 1) ? rg.stops[1].offset : 1.0f;
        cb.stop_offsets[2] = (rg.stop_count > 2) ? rg.stops[2].offset : 1.0f;
        cb.stop_offsets[3] = (rg.stop_count > 3) ? rg.stops[3].offset : 1.0f;
        break;
    }

    case BrushKind::AcrylicBrush: {
        const auto& ac = brush.acrylic_data();
        cb.brush_kind      = 3;
        // grad_params = 色调颜色 rgba
        cb.grad_params[0]  = ac.tint_color.r;
        cb.grad_params[1]  = ac.tint_color.g;
        cb.grad_params[2]  = ac.tint_color.b;
        cb.grad_params[3]  = ac.tint_color.a;
        // grad_extra.x = 色调混合比例，grad_extra.y = 饱和度增益
        cb.grad_extra[0]   = ac.tint_opacity;
        cb.grad_extra[1]   = ac.saturation;
        break;
    }

    default:
        cb.brush_kind = 0;
        break;
    }

    return cb;
}

// ── 渲染 ──────────────────────────────────────────────────────────────────────

void RhiRenderer::set_dpi_scale(float scale) {
    dpi_scale_ = (scale > 0.0f) ? scale : 1.0f;
}

void RhiRenderer::render(const DisplayList& dl, gfx::ITexture* target) {
    if (!valid_ || target == nullptr) return;

    const auto& cmds = dl.cmds();
    if (cmds.empty()) return;

    // ── 1. 亚克力前处理：捕获背景并应用高斯模糊 ─────────────────────────
    //
    // 在任何绘制操作之前检测是否有亚克力命令。若有，则：
    //   a. GPU-to-GPU 拷贝当前渲染目标（即上一帧已渲染的内容）到 scratch_a
    //   b. 水平高斯模糊：scratch_a → scratch_b
    //   c. 垂直高斯模糊：scratch_b → scratch_a
    // 亚克力元素在后续绘制中从 scratch_a 采样模糊背景。
    //
    // 注：copy_texture 在立即上下文中立即执行；模糊通道通过 cmd_list_ 录制，
    //     在 end_frame() 提交后统一执行，顺序保证正确。
    float acrylic_blur_amount = 30.0f;  // 使用第一个亚克力命令的模糊量
    bool  has_acrylic         = false;
    for (const DrawCmd& cmd : cmds) {
        if (cmd.brush.kind() == BrushKind::AcrylicBrush) {
            acrylic_blur_amount = cmd.brush.acrylic_data().blur_amount;
            has_acrylic = true;
            break;
        }
    }

    if (has_acrylic && ensure_scratch_textures(target)) {
        // a. GPU-to-GPU 拷贝（立即上下文），捕获上一帧内容到 scratch_a
        device_->copy_texture(blur_scratch_a_.get(), target);

        const float phys_w = static_cast<float>(target->width());
        const float phys_h = static_cast<float>(target->height());
        // 采样步进：每 tap 跨越 blur_amount/8 像素（9-tap 覆盖 ±4 个采样点）
        const float blur_step = acrylic_blur_amount / 8.0f;

        // b. 水平模糊通道：scratch_a → scratch_b
        {
            BlurCB blur_cb{};
            blur_cb.texel_step_x = blur_step / phys_w;
            blur_cb.texel_step_y = 0.0f;

            gfx::BufferDesc blur_cb_desc{};
            blur_cb_desc.size       = sizeof(BlurCB);
            blur_cb_desc.stride     = 0;
            blur_cb_desc.bind_flags = gfx::BufferBindFlags::Constant;
            auto blur_cb_buf = device_->create_buffer(blur_cb_desc, &blur_cb);

            // 全屏四边形（NDC 坐标，UV [0,1]）
            BlurVertex quad[6] = {
                {-1.0f,  1.0f, 0.0f, 0.0f},
                { 1.0f,  1.0f, 1.0f, 0.0f},
                {-1.0f, -1.0f, 0.0f, 1.0f},
                { 1.0f,  1.0f, 1.0f, 0.0f},
                { 1.0f, -1.0f, 1.0f, 1.0f},
                {-1.0f, -1.0f, 0.0f, 1.0f},
            };
            gfx::BufferDesc vb_desc{};
            vb_desc.size       = sizeof(quad);
            vb_desc.stride     = sizeof(BlurVertex);
            vb_desc.bind_flags = gfx::BufferBindFlags::Vertex;
            auto blur_vb = device_->create_buffer(vb_desc, quad);

            if (blur_cb_buf && blur_vb) {
                gfx::Viewport blur_vp{};
                blur_vp.width     = phys_w;
                blur_vp.height    = phys_h;
                blur_vp.max_depth = 1.0f;

                cmd_list_->set_render_target(blur_scratch_b_.get(), nullptr);
                cmd_list_->set_viewport(blur_vp);
                cmd_list_->set_pipeline(blur_pipeline_.get());
                cmd_list_->set_constant_buffer(1, blur_cb_buf.get());
                cmd_list_->set_shader_resource(0, blur_scratch_a_.get());
                cmd_list_->set_vertex_buffer(0, blur_vb.get(), 0);
                cmd_list_->draw(6, 1, 0, 0);
            }
        }

        // c. 垂直模糊通道：scratch_b → scratch_a
        {
            BlurCB blur_cb{};
            blur_cb.texel_step_x = 0.0f;
            blur_cb.texel_step_y = blur_step / phys_h;

            gfx::BufferDesc blur_cb_desc{};
            blur_cb_desc.size       = sizeof(BlurCB);
            blur_cb_desc.stride     = 0;
            blur_cb_desc.bind_flags = gfx::BufferBindFlags::Constant;
            auto blur_cb_buf = device_->create_buffer(blur_cb_desc, &blur_cb);

            BlurVertex quad[6] = {
                {-1.0f,  1.0f, 0.0f, 0.0f},
                { 1.0f,  1.0f, 1.0f, 0.0f},
                {-1.0f, -1.0f, 0.0f, 1.0f},
                { 1.0f,  1.0f, 1.0f, 0.0f},
                { 1.0f, -1.0f, 1.0f, 1.0f},
                {-1.0f, -1.0f, 0.0f, 1.0f},
            };
            gfx::BufferDesc vb_desc{};
            vb_desc.size       = sizeof(quad);
            vb_desc.stride     = sizeof(BlurVertex);
            vb_desc.bind_flags = gfx::BufferBindFlags::Vertex;
            auto blur_vb = device_->create_buffer(vb_desc, quad);

            if (blur_cb_buf && blur_vb) {
                gfx::Viewport blur_vp{};
                blur_vp.width     = phys_w;
                blur_vp.height    = phys_h;
                blur_vp.max_depth = 1.0f;

                cmd_list_->set_render_target(blur_scratch_a_.get(), nullptr);
                cmd_list_->set_viewport(blur_vp);
                cmd_list_->set_pipeline(blur_pipeline_.get());
                cmd_list_->set_constant_buffer(1, blur_cb_buf.get());
                cmd_list_->set_shader_resource(0, blur_scratch_b_.get());
                cmd_list_->set_vertex_buffer(0, blur_vb.get(), 0);
                cmd_list_->draw(6, 1, 0, 0);
            }
        }
    }

    // ── 2. 创建视口常量缓冲（所有 DrawCall 共享，每帧创建一次）──────────
    //
    // 使用逻辑像素大小（物理大小 / dpi_scale），使逻辑坐标到 NDC 的映射
    // 始终正确；光栅化视口仍使用物理大小，fwidth() 在物理像素精度下工作。
    const ViewportCB cb_data{
        static_cast<float>(target->width())  / dpi_scale_,
        static_cast<float>(target->height()) / dpi_scale_,
        static_cast<float>(target->width()),   // 物理宽度（亚克力 UV 采样用）
        static_cast<float>(target->height())   // 物理高度
    };

    gfx::BufferDesc cb_desc{};
    cb_desc.size       = sizeof(ViewportCB);
    cb_desc.stride     = 0;
    cb_desc.bind_flags = gfx::BufferBindFlags::Constant;

    auto viewport_cb = device_->create_buffer(cb_desc, &cb_data);
    if (!viewport_cb) return;

    // ── 3. 设置全局渲染状态（渲染目标 + 视口）────────────────────────────────

    // 直接绑定渲染目标（无模板）：SDF 软裁剪通过 ClipSdfCB（b3 槽）在像素着色器中实现
    cmd_list_->set_render_target(target, nullptr);

    gfx::Viewport viewport{};
    viewport.x         = 0.0f;
    viewport.y         = 0.0f;
    viewport.width     = static_cast<float>(target->width());
    viewport.height    = static_cast<float>(target->height());
    viewport.min_depth = 0.0f;
    viewport.max_depth = 1.0f;
    cmd_list_->set_viewport(viewport);

    // ── 4. 裁剪 SDF 常量缓冲（b3 槽）初始化 ─────────────────────────────────
    //
    // clip_sdf_data 维护 CPU 端裁剪层状态；ClipPush*/ClipPop 命令更新其内容并重建 GPU 缓冲。
    // 初始状态 clip_count=0（无裁剪），pixel shader 中 evaluate_clip_coverage 直接返回 1.0。
    ClipSdfCB clip_sdf_data{};
    clip_sdf_data.clip_count = 0;

    auto rebuild_clip_sdf_cb = [&]() -> core::OwnedPtr<gfx::IBuffer> {
        gfx::BufferDesc clip_cb_desc{};
        clip_cb_desc.size       = sizeof(ClipSdfCB);
        clip_cb_desc.stride     = 0;
        clip_cb_desc.bind_flags = gfx::BufferBindFlags::Constant;
        return device_->create_buffer(clip_cb_desc, &clip_sdf_data);
    };

    // 创建初始 ClipSdfCB（clip_count=0）并绑定至 b3；后续 ClipPush/Pop 更新后重绑
    auto clip_sdf_cb = rebuild_clip_sdf_cb();
    if (clip_sdf_cb) cmd_list_->set_constant_buffer(3, clip_sdf_cb.get());

    // ── 变换状态（save/restore/translate/scale/rotate 使用）─────────────

    /// 当前累积变换（默认单位矩阵；命令序列中按 TransformSet 逐步级联）
    math::Transform2D current_transform;
    /// 是否为单位矩阵（优化：跳过无变换时的顶点遍历）
    bool current_transform_is_identity = true;
    /// save()/restore() 保存/恢复变换快照的栈
    containers::Vector<math::Transform2D> transform_save_stack;

    /// 对 SDF 顶点批次应用当前变换（仅更新屏幕坐标 x/y，local 坐标保持画布空间不变）
    auto apply_sdf_transform = [&](containers::Vector<SdfVertex>& verts) {
        if (current_transform_is_identity) return;
        for (auto& v : verts) {
            const math::Point sp = current_transform.apply(math::Point{v.x, v.y});
            v.x = sp.x;
            v.y = sp.y;
            // v.local_x / v.local_y 不变 → SDF 在画布本地坐标系中正确计算
        }
    };

    /// 对文字顶点批次应用当前变换（更新字形角点屏幕坐标 x/y）
    auto apply_text_transform = [&](containers::Vector<TextVertex>& verts) {
        if (current_transform_is_identity) return;
        for (auto& v : verts) {
            const math::Point sp = current_transform.apply(math::Point{v.x, v.y});
            v.x = sp.x;
            v.y = sp.y;
        }
    };

    /// 对纯色顶点批次应用当前变换（用于 StrokePath 的三角带顶点）
    auto apply_paint_transform = [&](containers::Vector<PaintVertex>& verts) {
        if (current_transform_is_identity) return;
        for (auto& v : verts) {
            const math::Point sp = current_transform.apply(math::Point{v.x, v.y});
            v.x = sp.x;
            v.y = sp.y;
        }
    };

    // ── 5. 逐命令处理（每命令单独 DrawCall，保证绘制顺序）───────────────

    for (const DrawCmd& cmd : cmds) {

        // ── 变换命令（最先处理，直接 continue，不进入后续绘制分支）─────────

        if (cmd.kind == DrawCmdKind::TransformPush) {
            // save() — 将当前累积变换快照压入保存栈，current_transform 不变
            transform_save_stack.push_back(current_transform);
            continue;
        }
        if (cmd.kind == DrawCmdKind::TransformSet) {
            // transform()/translate()/rotate()/scale() — 将 cmd.transform 右乘到当前变换
            current_transform = current_transform * cmd.transform;
            current_transform_is_identity = (current_transform == math::Transform2D{});
            continue;
        }
        if (cmd.kind == DrawCmdKind::TransformPop) {
            // restore() — 弹出最近一次 save() 保存的变换快照
            if (!transform_save_stack.empty()) {
                current_transform = transform_save_stack.back();
                transform_save_stack.pop_back();
                current_transform_is_identity = (current_transform == math::Transform2D{});
            }
            continue;
        }

        // ── 裁剪状态辅助：SDF 软裁剪通过 ClipSdfCB（b3 槽）在像素着色器中实现 ──
        // 始终使用主管线；裁剪效果由 evaluate_clip_coverage() 在每个像素处计算。
        gfx::IPipeline* const active_sdf_pl  = sdf_pipeline_.get();
        gfx::IPipeline* const active_text_pl = text_pipeline_.get();

        // ── SDF 填充命令 ─────────────────────────────────────────────────
        // 填充命令支持：纯色（SolidColor）、线性渐变、径向渐变、亚克力画刷
        // 颜色来源由 BrushDataCB.brush_kind 在 GPU 端决定；顶点颜色仅用于纯色路径

        if (cmd.kind == DrawCmdKind::FillRect) {
            const float cx = cmd.rect.x + cmd.rect.width  * 0.5f;
            const float cy = cmd.rect.y + cmd.rect.height * 0.5f;
            const float hw = cmd.rect.width  * 0.5f;
            const float hh = cmd.rect.height * 0.5f;
            // 纯色时从画刷取颜色写入顶点；渐变/亚克力时顶点颜色由 GPU 端 BrushDataCB 决定（此处传透明占位）
            const math::Color c = (cmd.brush.kind() == BrushKind::SolidColor)
                ? cmd.brush.color()
                : math::Color{0.0f, 0.0f, 0.0f, 0.0f};

            containers::Vector<SdfVertex> verts;
            // 矩形（kind=0），填充（stroke_w=0），1px AA 余量
            push_sdf_quad_vertices(verts, cx, cy, hw, hh, 1.0f,
                c.r, c.g, c.b, c.a, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f);
            apply_sdf_transform(verts);

            gfx::BufferDesc vb{};
            vb.size       = static_cast<uint64_t>(verts.size()) * sizeof(SdfVertex);
            vb.stride     = sizeof(SdfVertex);
            vb.bind_flags = gfx::BufferBindFlags::Vertex;
            auto buf = device_->create_buffer(vb, verts.data());
            if (!buf) continue;

            // 构建并绑定画刷数据常量缓冲（b2 槽）
            const BrushDataCB brush_cb_data = make_brush_cb(cmd.brush, cx, cy, hw, hh);
            gfx::BufferDesc bcb{};
            bcb.size       = sizeof(BrushDataCB);
            bcb.bind_flags = gfx::BufferBindFlags::Constant;
            auto brush_cb_buf = device_->create_buffer(bcb, &brush_cb_data);

            cmd_list_->set_pipeline(active_sdf_pl);
            cmd_list_->set_constant_buffer(0, viewport_cb.get());
            if (brush_cb_buf) {
                cmd_list_->set_constant_buffer(2, brush_cb_buf.get());
            }
            // 亚克力：绑定模糊背景纹理（t0 槽），其他画刷解绑
            if (cmd.brush.kind() == BrushKind::AcrylicBrush && blur_scratch_a_) {
                cmd_list_->set_shader_resource(0, blur_scratch_a_.get());
            } else {
                cmd_list_->set_shader_resource(0, nullptr);
            }
            cmd_list_->set_vertex_buffer(0, buf.get(), 0);
            cmd_list_->draw(static_cast<uint32_t>(verts.size()), 1, 0, 0);
        }
        else if (cmd.kind == DrawCmdKind::FillRoundedRect) {
            const float cx = cmd.rrect.rect.x + cmd.rrect.rect.width  * 0.5f;
            const float cy = cmd.rrect.rect.y + cmd.rrect.rect.height * 0.5f;
            const float hw = cmd.rrect.rect.width  * 0.5f;
            const float hh = cmd.rrect.rect.height * 0.5f;
            const math::Color c = (cmd.brush.kind() == BrushKind::SolidColor)
                ? cmd.brush.color()
                : math::Color{0.0f, 0.0f, 0.0f, 0.0f};
            // 均匀圆角（各向同性：取 rx 和 ry 的最小值）
            const float rad = std::min(cmd.rrect.radius_x, cmd.rrect.radius_y);
            // 圆角半径不得超过半尺寸（CSS 规范钳制）
            const float r_clamped = std::min(rad, std::min(hw, hh));

            containers::Vector<SdfVertex> verts;
            // 均匀圆角矩形（kind=1），p0=统一圆角半径
            push_sdf_quad_vertices(verts, cx, cy, hw, hh, 1.0f,
                c.r, c.g, c.b, c.a, 1.0f, r_clamped, 0.0f, 0.0f, 0.0f, 0.0f);
            apply_sdf_transform(verts);

            gfx::BufferDesc vb{};
            vb.size       = static_cast<uint64_t>(verts.size()) * sizeof(SdfVertex);
            vb.stride     = sizeof(SdfVertex);
            vb.bind_flags = gfx::BufferBindFlags::Vertex;
            auto buf = device_->create_buffer(vb, verts.data());
            if (!buf) continue;

            cmd_list_->set_pipeline(active_sdf_pl);
            cmd_list_->set_constant_buffer(0, viewport_cb.get());
            {
                const BrushDataCB brush_cb_data = make_brush_cb(cmd.brush, cx, cy, hw, hh);
                gfx::BufferDesc bcb{};
                bcb.size       = sizeof(BrushDataCB);
                bcb.bind_flags = gfx::BufferBindFlags::Constant;
                auto brush_cb_buf = device_->create_buffer(bcb, &brush_cb_data);
                if (brush_cb_buf) cmd_list_->set_constant_buffer(2, brush_cb_buf.get());
            }
            if (cmd.brush.kind() == BrushKind::AcrylicBrush && blur_scratch_a_) {
                cmd_list_->set_shader_resource(0, blur_scratch_a_.get());
            } else {
                cmd_list_->set_shader_resource(0, nullptr);
            }
            cmd_list_->set_vertex_buffer(0, buf.get(), 0);
            cmd_list_->draw(static_cast<uint32_t>(verts.size()), 1, 0, 0);
        }
        else if (cmd.kind == DrawCmdKind::FillComplexRoundedRect) {
            const float cx = cmd.complex_rrect.rect.x + cmd.complex_rrect.rect.width  * 0.5f;
            const float cy = cmd.complex_rrect.rect.y + cmd.complex_rrect.rect.height * 0.5f;
            const float hw = cmd.complex_rrect.rect.width  * 0.5f;
            const float hh = cmd.complex_rrect.rect.height * 0.5f;
            const math::Color c = (cmd.brush.kind() == BrushKind::SolidColor)
                ? cmd.brush.color()
                : math::Color{0.0f, 0.0f, 0.0f, 0.0f};
            const auto& radii = cmd.complex_rrect.radii;
            // 各角各向同性化（取 rx/ry 最小值），再钳制到不超过半尺寸
            const float r_br = std::min(std::min(radii.bottom_right.x, radii.bottom_right.y), std::min(hw, hh));
            const float r_tr = std::min(std::min(radii.top_right.x,    radii.top_right.y),    std::min(hw, hh));
            const float r_bl = std::min(std::min(radii.bottom_left.x,  radii.bottom_left.y),  std::min(hw, hh));
            const float r_tl = std::min(std::min(radii.top_left.x,     radii.top_left.y),     std::min(hw, hh));

            containers::Vector<SdfVertex> verts;
            // 复杂圆角矩形（kind=2），p0=右下, p1=右上, p2=左下, p3=左上
            push_sdf_quad_vertices(verts, cx, cy, hw, hh, 1.0f,
                c.r, c.g, c.b, c.a, 2.0f, r_br, r_tr, r_bl, r_tl, 0.0f);
            apply_sdf_transform(verts);

            gfx::BufferDesc vb{};
            vb.size       = static_cast<uint64_t>(verts.size()) * sizeof(SdfVertex);
            vb.stride     = sizeof(SdfVertex);
            vb.bind_flags = gfx::BufferBindFlags::Vertex;
            auto buf = device_->create_buffer(vb, verts.data());
            if (!buf) continue;

            cmd_list_->set_pipeline(active_sdf_pl);
            cmd_list_->set_constant_buffer(0, viewport_cb.get());
            {
                const BrushDataCB brush_cb_data = make_brush_cb(cmd.brush, cx, cy, hw, hh);
                gfx::BufferDesc bcb{};
                bcb.size       = sizeof(BrushDataCB);
                bcb.bind_flags = gfx::BufferBindFlags::Constant;
                auto brush_cb_buf = device_->create_buffer(bcb, &brush_cb_data);
                if (brush_cb_buf) cmd_list_->set_constant_buffer(2, brush_cb_buf.get());
            }
            if (cmd.brush.kind() == BrushKind::AcrylicBrush && blur_scratch_a_) {
                cmd_list_->set_shader_resource(0, blur_scratch_a_.get());
            } else {
                cmd_list_->set_shader_resource(0, nullptr);
            }
            cmd_list_->set_vertex_buffer(0, buf.get(), 0);
            cmd_list_->draw(static_cast<uint32_t>(verts.size()), 1, 0, 0);
        }
        else if (cmd.kind == DrawCmdKind::FillEllipse) {
            if (cmd.pt_b.x <= 0.0f || cmd.pt_b.y <= 0.0f) continue;
            const math::Color c = (cmd.brush.kind() == BrushKind::SolidColor)
                ? cmd.brush.color()
                : math::Color{0.0f, 0.0f, 0.0f, 0.0f};
            // pt_a = 中心，pt_b = (rx, ry) 半径
            const float cx = cmd.pt_a.x;
            const float cy = cmd.pt_a.y;
            const float hw = cmd.pt_b.x;
            const float hh = cmd.pt_b.y;

            containers::Vector<SdfVertex> verts;
            // 椭圆（kind=3），half_w=rx，half_h=ry
            push_sdf_quad_vertices(verts, cx, cy, hw, hh, 1.0f,
                c.r, c.g, c.b, c.a, 3.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f);
            apply_sdf_transform(verts);

            gfx::BufferDesc vb{};
            vb.size       = static_cast<uint64_t>(verts.size()) * sizeof(SdfVertex);
            vb.stride     = sizeof(SdfVertex);
            vb.bind_flags = gfx::BufferBindFlags::Vertex;
            auto buf = device_->create_buffer(vb, verts.data());
            if (!buf) continue;

            cmd_list_->set_pipeline(active_sdf_pl);
            cmd_list_->set_constant_buffer(0, viewport_cb.get());
            {
                const BrushDataCB brush_cb_data = make_brush_cb(cmd.brush, cx, cy, hw, hh);
                gfx::BufferDesc bcb{};
                bcb.size       = sizeof(BrushDataCB);
                bcb.bind_flags = gfx::BufferBindFlags::Constant;
                auto brush_cb_buf = device_->create_buffer(bcb, &brush_cb_data);
                if (brush_cb_buf) cmd_list_->set_constant_buffer(2, brush_cb_buf.get());
            }
            if (cmd.brush.kind() == BrushKind::AcrylicBrush && blur_scratch_a_) {
                cmd_list_->set_shader_resource(0, blur_scratch_a_.get());
            } else {
                cmd_list_->set_shader_resource(0, nullptr);
            }
            cmd_list_->set_vertex_buffer(0, buf.get(), 0);
            cmd_list_->draw(static_cast<uint32_t>(verts.size()), 1, 0, 0);
        }

        // ── SDF 描边命令 ─────────────────────────────────────────────────

        else if (cmd.kind == DrawCmdKind::StrokeRect) {
            if (cmd.brush.kind() != BrushKind::SolidColor) continue;
            if (cmd.pen.width <= 0.0f) continue;
            const math::Color c = cmd.brush.color();
            const float sw = cmd.pen.width;
            const float cx = cmd.rect.x + cmd.rect.width  * 0.5f;
            const float cy = cmd.rect.y + cmd.rect.height * 0.5f;
            const float hw = cmd.rect.width  * 0.5f;
            const float hh = cmd.rect.height * 0.5f;
            // padding = 描边外扩（half_sw）+ AA 余量（1px）
            const float pad = sw * 0.5f + 1.0f;

            containers::Vector<SdfVertex> verts;
            // 矩形描边（kind=0，stroke_w=线宽）
            push_sdf_quad_vertices(verts, cx, cy, hw, hh, pad,
                c.r, c.g, c.b, c.a, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, sw);
            apply_sdf_transform(verts);

            gfx::BufferDesc vb{};
            vb.size       = static_cast<uint64_t>(verts.size()) * sizeof(SdfVertex);
            vb.stride     = sizeof(SdfVertex);
            vb.bind_flags = gfx::BufferBindFlags::Vertex;
            auto buf = device_->create_buffer(vb, verts.data());
            if (!buf) continue;

            cmd_list_->set_pipeline(active_sdf_pl);
            cmd_list_->set_constant_buffer(0, viewport_cb.get());
            cmd_list_->set_vertex_buffer(0, buf.get(), 0);
            cmd_list_->draw(static_cast<uint32_t>(verts.size()), 1, 0, 0);
        }
        else if (cmd.kind == DrawCmdKind::StrokeBorderedRect) {
            // 四边各自独立宽度的矩形内侧描边（SDF kind=4）
            // p0=top_w, p1=right_w, p2=bottom_w, p3=left_w
            if (cmd.brush.kind() != BrushKind::SolidColor) continue;
            const auto& bw = cmd.border_widths;
            // 若四边宽度全为零则跳过
            if (bw.top <= 0.0f && bw.right <= 0.0f && bw.bottom <= 0.0f && bw.left <= 0.0f) continue;
            const math::Color c = cmd.brush.color();
            const float cx = cmd.rect.x + cmd.rect.width  * 0.5f;
            const float cy = cmd.rect.y + cmd.rect.height * 0.5f;
            const float hw = cmd.rect.width  * 0.5f;
            const float hh = cmd.rect.height * 0.5f;
            // 内侧描边不超出矩形外边界，只需要 1px AA 余量
            const float pad = 1.0f;

            containers::Vector<SdfVertex> verts;
            // kind=4，p0=top_w, p1=right_w, p2=bottom_w, p3=left_w，stroke_w=0（未使用）
            push_sdf_quad_vertices(verts, cx, cy, hw, hh, pad,
                c.r, c.g, c.b, c.a, 4.0f,
                bw.top, bw.right, bw.bottom, bw.left, 0.0f);
            apply_sdf_transform(verts);

            gfx::BufferDesc vb{};
            vb.size       = static_cast<uint64_t>(verts.size()) * sizeof(SdfVertex);
            vb.stride     = sizeof(SdfVertex);
            vb.bind_flags = gfx::BufferBindFlags::Vertex;
            auto buf = device_->create_buffer(vb, verts.data());
            if (!buf) continue;

            cmd_list_->set_pipeline(active_sdf_pl);
            cmd_list_->set_constant_buffer(0, viewport_cb.get());
            cmd_list_->set_vertex_buffer(0, buf.get(), 0);
            cmd_list_->draw(static_cast<uint32_t>(verts.size()), 1, 0, 0);
        }
        else if (cmd.kind == DrawCmdKind::StrokeBorderedRoundedRect) {
            // 四边各自独立宽度 + 四角独立圆角的内侧描边（SDF kind=5）
            // p0=top_w, p1=right_w, p2=bottom_w, p3=left_w
            // e0=r_tl, e1=r_tr, e2=r_br, e3=r_bl
            if (cmd.brush.kind() != BrushKind::SolidColor) continue;
            const auto& bw = cmd.border_widths;
            if (bw.top <= 0.0f && bw.right <= 0.0f && bw.bottom <= 0.0f && bw.left <= 0.0f) continue;
            const math::Color c = cmd.brush.color();
            const float cx = cmd.rect.x + cmd.rect.width  * 0.5f;
            const float cy = cmd.rect.y + cmd.rect.height * 0.5f;
            const float hw = cmd.rect.width  * 0.5f;
            const float hh = cmd.rect.height * 0.5f;
            const auto& rd = cmd.border_radii;
            // 各角各向同性化（取 rx/ry 最小值），再钳制到不超过半尺寸
            const float r_tl = std::min(std::min(rd.top_left.x,     rd.top_left.y),     std::min(hw, hh));
            const float r_tr = std::min(std::min(rd.top_right.x,    rd.top_right.y),    std::min(hw, hh));
            const float r_br = std::min(std::min(rd.bottom_right.x, rd.bottom_right.y), std::min(hw, hh));
            const float r_bl = std::min(std::min(rd.bottom_left.x,  rd.bottom_left.y),  std::min(hw, hh));
            const float pad = 1.0f;

            containers::Vector<SdfVertex> verts;
            // kind=5，stroke_w=0（未使用），e0-e3 存四角圆角半径
            push_sdf_quad_vertices(verts, cx, cy, hw, hh, pad,
                c.r, c.g, c.b, c.a, 5.0f,
                bw.top, bw.right, bw.bottom, bw.left, 0.0f,
                r_tl, r_tr, r_br, r_bl);
            apply_sdf_transform(verts);

            gfx::BufferDesc vb{};
            vb.size       = static_cast<uint64_t>(verts.size()) * sizeof(SdfVertex);
            vb.stride     = sizeof(SdfVertex);
            vb.bind_flags = gfx::BufferBindFlags::Vertex;
            auto buf2 = device_->create_buffer(vb, verts.data());
            if (!buf2) continue;

            cmd_list_->set_pipeline(active_sdf_pl);
            cmd_list_->set_constant_buffer(0, viewport_cb.get());
            cmd_list_->set_vertex_buffer(0, buf2.get(), 0);
            cmd_list_->draw(static_cast<uint32_t>(verts.size()), 1, 0, 0);
        }
        else if (cmd.kind == DrawCmdKind::StrokeRoundedRect) {
            if (cmd.brush.kind() != BrushKind::SolidColor) continue;
            if (cmd.pen.width <= 0.0f) continue;
            const math::Color c = cmd.brush.color();
            const float sw = cmd.pen.width;
            const float cx = cmd.rrect.rect.x + cmd.rrect.rect.width  * 0.5f;
            const float cy = cmd.rrect.rect.y + cmd.rrect.rect.height * 0.5f;
            const float hw = cmd.rrect.rect.width  * 0.5f;
            const float hh = cmd.rrect.rect.height * 0.5f;
            const float rad = std::min(cmd.rrect.radius_x, cmd.rrect.radius_y);
            const float r_clamped = std::min(rad, std::min(hw, hh));
            const float pad = sw * 0.5f + 1.0f;

            containers::Vector<SdfVertex> verts;
            // 均匀圆角矩形描边（kind=1，stroke_w=线宽）
            push_sdf_quad_vertices(verts, cx, cy, hw, hh, pad,
                c.r, c.g, c.b, c.a, 1.0f, r_clamped, 0.0f, 0.0f, 0.0f, sw);
            apply_sdf_transform(verts);

            gfx::BufferDesc vb{};
            vb.size       = static_cast<uint64_t>(verts.size()) * sizeof(SdfVertex);
            vb.stride     = sizeof(SdfVertex);
            vb.bind_flags = gfx::BufferBindFlags::Vertex;
            auto buf = device_->create_buffer(vb, verts.data());
            if (!buf) continue;

            cmd_list_->set_pipeline(active_sdf_pl);
            cmd_list_->set_constant_buffer(0, viewport_cb.get());
            cmd_list_->set_vertex_buffer(0, buf.get(), 0);
            cmd_list_->draw(static_cast<uint32_t>(verts.size()), 1, 0, 0);
        }
        else if (cmd.kind == DrawCmdKind::StrokeComplexRoundedRect) {
            if (cmd.brush.kind() != BrushKind::SolidColor) continue;
            if (cmd.pen.width <= 0.0f) continue;
            const math::Color c = cmd.brush.color();
            const float sw = cmd.pen.width;
            const float cx = cmd.complex_rrect.rect.x + cmd.complex_rrect.rect.width  * 0.5f;
            const float cy = cmd.complex_rrect.rect.y + cmd.complex_rrect.rect.height * 0.5f;
            const float hw = cmd.complex_rrect.rect.width  * 0.5f;
            const float hh = cmd.complex_rrect.rect.height * 0.5f;
            const auto& radii = cmd.complex_rrect.radii;
            const float r_br = std::min(std::min(radii.bottom_right.x, radii.bottom_right.y), std::min(hw, hh));
            const float r_tr = std::min(std::min(radii.top_right.x,    radii.top_right.y),    std::min(hw, hh));
            const float r_bl = std::min(std::min(radii.bottom_left.x,  radii.bottom_left.y),  std::min(hw, hh));
            const float r_tl = std::min(std::min(radii.top_left.x,     radii.top_left.y),     std::min(hw, hh));
            const float pad = sw * 0.5f + 1.0f;

            containers::Vector<SdfVertex> verts;
            // 复杂圆角矩形描边（kind=2，stroke_w=线宽）
            push_sdf_quad_vertices(verts, cx, cy, hw, hh, pad,
                c.r, c.g, c.b, c.a, 2.0f, r_br, r_tr, r_bl, r_tl, sw);
            apply_sdf_transform(verts);

            gfx::BufferDesc vb{};
            vb.size       = static_cast<uint64_t>(verts.size()) * sizeof(SdfVertex);
            vb.stride     = sizeof(SdfVertex);
            vb.bind_flags = gfx::BufferBindFlags::Vertex;
            auto buf = device_->create_buffer(vb, verts.data());
            if (!buf) continue;

            cmd_list_->set_pipeline(active_sdf_pl);
            cmd_list_->set_constant_buffer(0, viewport_cb.get());
            cmd_list_->set_vertex_buffer(0, buf.get(), 0);
            cmd_list_->draw(static_cast<uint32_t>(verts.size()), 1, 0, 0);
        }
        else if (cmd.kind == DrawCmdKind::StrokeEllipse) {
            if (cmd.brush.kind() != BrushKind::SolidColor) continue;
            if (cmd.pen.width <= 0.0f) continue;
            if (cmd.pt_b.x <= 0.0f || cmd.pt_b.y <= 0.0f) continue;
            const math::Color c = cmd.brush.color();
            const float sw = cmd.pen.width;
            const float cx = cmd.pt_a.x;
            const float cy = cmd.pt_a.y;
            const float hw = cmd.pt_b.x;
            const float hh = cmd.pt_b.y;
            const float pad = sw * 0.5f + 1.0f;

            containers::Vector<SdfVertex> verts;
            // 椭圆描边（kind=3，stroke_w=线宽）
            push_sdf_quad_vertices(verts, cx, cy, hw, hh, pad,
                c.r, c.g, c.b, c.a, 3.0f, 0.0f, 0.0f, 0.0f, 0.0f, sw);
            apply_sdf_transform(verts);

            gfx::BufferDesc vb{};
            vb.size       = static_cast<uint64_t>(verts.size()) * sizeof(SdfVertex);
            vb.stride     = sizeof(SdfVertex);
            vb.bind_flags = gfx::BufferBindFlags::Vertex;
            auto buf = device_->create_buffer(vb, verts.data());
            if (!buf) continue;

            cmd_list_->set_pipeline(active_sdf_pl);
            cmd_list_->set_constant_buffer(0, viewport_cb.get());
            cmd_list_->set_vertex_buffer(0, buf.get(), 0);
            cmd_list_->draw(static_cast<uint32_t>(verts.size()), 1, 0, 0);
        }

        // ── 线段 SDF 描边（kind=6）──────────────────────────────────────

        else if (cmd.kind == DrawCmdKind::StrokeLine) {
            // 使用 SDF 方案（kind=6），天然抗锯齿，支持 Flat/Round/Square 端点样式。
            // 包围盒 = 线段 AABB + 描边外扩（half_sw）+ 端点最大延伸（half_sw）+ AA 余量（1px）
            if (cmd.brush.kind() != BrushKind::SolidColor) continue;
            if (cmd.pen.width <= 0.0f) continue;

            const math::Color c = cmd.brush.color();
            const float sw      = cmd.pen.width;
            const float half_sw = sw * 0.5f;

            const float ax = cmd.pt_a.x, ay = cmd.pt_a.y;
            const float bx = cmd.pt_b.x, by = cmd.pt_b.y;

            // 线段中心（本地坐标系原点）
            const float cx = (ax + bx) * 0.5f;
            const float cy = (ay + by) * 0.5f;

            // AABB 半尺寸 + 描边外扩 + 端点 cap 最大延伸（half_sw）+ AA 余量（1px）
            // Round/Square cap 各延伸 half_sw，Flat 无延伸；保守取 half_sw 覆盖所有情况
            const float padding = half_sw + 1.0f;
            const float hw = std::abs(bx - ax) * 0.5f + padding;
            const float hh = std::abs(by - ay) * 0.5f + padding;

            // 端点 A/B 在本地坐标系中的坐标（extra.xy / extra.zw）
            const float lax = ax - cx, lay = ay - cy;
            const float lbx = bx - cx, lby = by - cy;

            // 端点样式编码（0=Flat, 1=Round, 2=Square）→ p0=start_cap, p1=end_cap
            auto encode_cap = [](LineCap cap) -> float {
                if (cap == LineCap::Round)  return 1.0f;
                if (cap == LineCap::Square) return 2.0f;
                return 0.0f;  // Flat
            };
            const float scap = encode_cap(cmd.pen.start_cap);
            const float ecap = encode_cap(cmd.pen.end_cap);

            containers::Vector<SdfVertex> verts;
            // kind=6，p0=start_cap, p1=end_cap，p2/p3 未用，stroke_w=线宽
            // e0,e1 = A端本地坐标，e2,e3 = B端本地坐标
            push_sdf_quad_vertices(verts, cx, cy, hw, hh, 0.0f,
                c.r, c.g, c.b, c.a,
                6.0f,           // kind=6（线段 SDF）
                scap, ecap, 0.0f, 0.0f,  // p0=start_cap, p1=end_cap
                sw,             // stroke_w
                lax, lay, lbx, lby);  // e0..e3 = 端点本地坐标

            if (verts.empty()) continue;
            apply_sdf_transform(verts);

            gfx::BufferDesc vb6{};
            vb6.size       = static_cast<uint64_t>(verts.size()) * sizeof(SdfVertex);
            vb6.stride     = sizeof(SdfVertex);
            vb6.bind_flags = gfx::BufferBindFlags::Vertex;
            auto buf6 = device_->create_buffer(vb6, verts.data());
            if (!buf6) continue;

            cmd_list_->set_pipeline(active_sdf_pl);
            cmd_list_->set_constant_buffer(0, viewport_cb.get());
            cmd_list_->set_vertex_buffer(0, buf6.get(), 0);
            cmd_list_->draw(static_cast<uint32_t>(verts.size()), 1, 0, 0);
        }

        // ── 圆弧 SDF 描边（kind=7）──────────────────────────────────────

        else if (cmd.kind == DrawCmdKind::StrokeArc) {
            // SDF 方案（kind=7），IQ 旋转坐标系圆弧距离场。
            // 约定：0=右，正角度=顺时针（屏幕坐标，Y 向下）。
            if (cmd.brush.kind() != BrushKind::SolidColor) continue;
            if (cmd.pen.width <= 0.0f) continue;
            if (cmd.pt_b.x <= 0.0f) continue;  // 半径为零跳过

            const math::Color c = cmd.brush.color();
            const float sw       = cmd.pen.width;
            const float half_sw  = sw * 0.5f;

            // DrawCmd 字段解包
            const float cx         = cmd.pt_a.x;      // 圆心 x
            const float cy         = cmd.pt_a.y;       // 圆心 y
            const float radius     = cmd.pt_b.x;       // 半径
            const float start_ang  = cmd.pt_b.y;       // 起始角（弧度）
            const float sweep_ang  = cmd.pt_c.x;       // 扫掠角（弧度）

            // 弧中心角和半张角（IQ 算法参数）
            const float mid_angle  = start_ang + sweep_ang * 0.5f;
            const float half_sweep = std::abs(sweep_ang) * 0.5f;

            // 端点 cap 编码（0=Flat, 1=Round；圆弧不支持 Square）
            auto arc_cap = [](LineCap cap) -> float {
                return cap == LineCap::Round ? 1.0f : 0.0f;
            };
            const float scap = arc_cap(cmd.pen.start_cap);

            // 圆弧包围盒：圆心 ± (radius + 描边外扩 + AA 余量)
            // 保守正方形包围盒，覆盖全部 cap 延伸情况
            const float box_r = radius + half_sw + 1.0f;
            const float hw    = box_r;
            const float hh    = box_r;

            containers::Vector<SdfVertex> verts;
            // kind=7，圆心即 Quad 中心，e0/e1 = 圆心本地坐标（0,0）
            // p0=radius, p1=mid_angle, p2=half_sweep, p3=cap（两端同样式）
            push_sdf_quad_vertices(verts, cx, cy, hw, hh, 0.0f,
                c.r, c.g, c.b, c.a,
                7.0f,                         // kind=7（圆弧 SDF）
                radius, mid_angle, half_sweep, // p0=r, p1=mid, p2=hs
                scap,                          // p3=cap（两端统一）
                sw,                            // stroke_w
                0.0f, 0.0f, 0.0f, 0.0f);      // e0,e1=圆心本地坐标(0,0)，e2,e3 未用

            if (verts.empty()) continue;
            apply_sdf_transform(verts);

            gfx::BufferDesc vb7{};
            vb7.size       = static_cast<uint64_t>(verts.size()) * sizeof(SdfVertex);
            vb7.stride     = sizeof(SdfVertex);
            vb7.bind_flags = gfx::BufferBindFlags::Vertex;
            auto buf7 = device_->create_buffer(vb7, verts.data());
            if (!buf7) continue;

            cmd_list_->set_pipeline(active_sdf_pl);
            cmd_list_->set_constant_buffer(0, viewport_cb.get());
            cmd_list_->set_vertex_buffer(0, buf7.get(), 0);
            cmd_list_->draw(static_cast<uint32_t>(verts.size()), 1, 0, 0);
        }

        // ── 二次贝塞尔曲线 SDF 描边（kind=8）────────────────────────────

        else if (cmd.kind == DrawCmdKind::StrokeQuadBezier) {
            // SDF 方案（kind=8），IQ 解析解（Cardano 公式求三次方程根）。
            // P2（终点）本地坐标借用 SdfVertex 的 half_w/half_h 槽传入着色器。
            if (cmd.brush.kind() != BrushKind::SolidColor) continue;
            if (cmd.pen.width <= 0.0f) continue;

            const math::Color c = cmd.brush.color();
            const float sw      = cmd.pen.width;
            const float half_sw = sw * 0.5f;

            // 三个控制点
            const float p0x = cmd.pt_a.x, p0y = cmd.pt_a.y;  // 起点 P0
            const float p1x = cmd.pt_b.x, p1y = cmd.pt_b.y;  // 控制点 P1
            const float p2x = cmd.pt_c.x, p2y = cmd.pt_c.y;  // 终点 P2

            // Quad 包围盒 = P0/P1/P2 三点 AABB + 描边外扩 + AA 余量
            // 二次贝塞尔曲线在三点凸包内，P0/P1/P2 的 AABB 是保守包围盒
            const float padding = half_sw + 1.0f;
            const float xmin = std::min(p0x, std::min(p1x, p2x));
            const float xmax = std::max(p0x, std::max(p1x, p2x));
            const float ymin = std::min(p0y, std::min(p1y, p2y));
            const float ymax = std::max(p0y, std::max(p1y, p2y));
            const float cx   = (xmin + xmax) * 0.5f;
            const float cy   = (ymin + ymax) * 0.5f;
            // 实际 Quad 半尺寸（用于生成顶点坐标）
            const float quad_hw = (xmax - xmin) * 0.5f + padding;
            const float quad_hh = (ymax - ymin) * 0.5f + padding;

            // 三点本地坐标（以 Quad 中心为原点）
            const float lp0x = p0x - cx, lp0y = p0y - cy;  // P0 本地坐标
            const float lp1x = p1x - cx, lp1y = p1y - cy;  // P1 本地坐标
            const float lp2x = p2x - cx, lp2y = p2y - cy;  // P2 本地坐标

            // cap 编码（0=Flat, 1=Round）
            auto bez_cap = [](LineCap cap) -> float {
                return cap == LineCap::Round ? 1.0f : 0.0f;
            };
            const float scap = bez_cap(cmd.pen.start_cap);
            const float ecap = bez_cap(cmd.pen.end_cap);

            // 手动构造 6 个 SdfVertex（不用 push_sdf_quad_vertices，
            // 因为 half_w/half_h 槽用于存放 P2 本地坐标，与 Quad 半尺寸分离）
            containers::Vector<SdfVertex> verts;
            verts.reserve(6);

            // kind=8：half_w=P2.x_local, half_h=P2.y_local（借用）
            // p0=start_cap, p1=end_cap, p2/p3=未用
            // e0,e1=P0本地坐标, e2,e3=P1本地坐标
            auto make_v8 = [&](float sx, float sy) -> SdfVertex {
                return {
                    sx, sy,
                    c.r, c.g, c.b, c.a,
                    sx - cx, sy - cy,               // 本地坐标（相对 Quad 中心）
                    8.0f,                            // kind=8
                    lp2x, lp2y,                     // half_w=P2.x_local, half_h=P2.y_local
                    scap,                            // p0=start_cap
                    ecap, 0.0f, 0.0f,               // p1=end_cap, p2/p3 未用
                    sw,                              // stroke_w
                    lp0x, lp0y, lp1x, lp1y          // e0,e1=P0, e2,e3=P1
                };
            };

            // Quad 屏幕坐标（包围盒 6 顶点）
            const float x1 = cx - quad_hw;
            const float y1 = cy - quad_hh;
            const float x2 = cx + quad_hw;
            const float y2 = cy + quad_hh;

            verts.push_back(make_v8(x1, y1));
            verts.push_back(make_v8(x2, y1));
            verts.push_back(make_v8(x1, y2));
            verts.push_back(make_v8(x2, y1));
            verts.push_back(make_v8(x2, y2));
            verts.push_back(make_v8(x1, y2));
            apply_sdf_transform(verts);

            gfx::BufferDesc vb8{};
            vb8.size       = static_cast<uint64_t>(verts.size()) * sizeof(SdfVertex);
            vb8.stride     = sizeof(SdfVertex);
            vb8.bind_flags = gfx::BufferBindFlags::Vertex;
            auto buf8 = device_->create_buffer(vb8, verts.data());
            if (!buf8) continue;

            cmd_list_->set_pipeline(active_sdf_pl);
            cmd_list_->set_constant_buffer(0, viewport_cb.get());
            cmd_list_->set_vertex_buffer(0, buf8.get(), 0);
            cmd_list_->draw(static_cast<uint32_t>(verts.size()), 1, 0, 0);
        }

        // ── 三次贝塞尔曲线 SDF 描边（kind=9）────────────────────────────

        else if (cmd.kind == DrawCmdKind::StrokeCubicBezier) {
            // SDF 方案（kind=9），数值迭代（17步采样 + 4步 Newton 精化）。
            // P2（第二控制点）本地坐标借用 SdfVertex 的 p2/p3 槽。
            // P3（终点）本地坐标借用 SdfVertex 的 half_w/half_h 槽。
            if (cmd.brush.kind() != BrushKind::SolidColor) continue;
            if (cmd.pen.width <= 0.0f) continue;

            const math::Color c = cmd.brush.color();
            const float sw      = cmd.pen.width;
            const float half_sw = sw * 0.5f;

            // 四个控制点
            const float p0x = cmd.pt_a.x, p0y = cmd.pt_a.y;  // 起点 P0
            const float p1x = cmd.pt_b.x, p1y = cmd.pt_b.y;  // 第一控制点 P1
            const float p2x = cmd.pt_c.x, p2y = cmd.pt_c.y;  // 第二控制点 P2
            const float p3x = cmd.pt_d.x, p3y = cmd.pt_d.y;  // 终点 P3

            // Quad 包围盒 = P0/P1/P2/P3 四点 AABB + 描边外扩 + AA 余量
            // 三次贝塞尔曲线在四点凸包内，四点的 AABB 是保守包围盒
            const float padding = half_sw + 1.0f;
            const float xmin = std::min(p0x, std::min(p1x, std::min(p2x, p3x)));
            const float xmax = std::max(p0x, std::max(p1x, std::max(p2x, p3x)));
            const float ymin = std::min(p0y, std::min(p1y, std::min(p2y, p3y)));
            const float ymax = std::max(p0y, std::max(p1y, std::max(p2y, p3y)));
            const float cx   = (xmin + xmax) * 0.5f;
            const float cy   = (ymin + ymax) * 0.5f;
            // 实际 Quad 半尺寸（用于生成顶点坐标）
            const float quad_hw = (xmax - xmin) * 0.5f + padding;
            const float quad_hh = (ymax - ymin) * 0.5f + padding;

            // 四点本地坐标（以 Quad 中心为原点）
            const float lp0x = p0x - cx, lp0y = p0y - cy;  // P0 本地坐标
            const float lp1x = p1x - cx, lp1y = p1y - cy;  // P1 本地坐标
            const float lp2x = p2x - cx, lp2y = p2y - cy;  // P2 本地坐标（借用 p2/p3 槽）
            const float lp3x = p3x - cx, lp3y = p3y - cy;  // P3 本地坐标（借用 half_w/half_h 槽）

            // cap 编码（0=Flat, 1=Round）
            auto bez_cap = [](LineCap cap) -> float {
                return cap == LineCap::Round ? 1.0f : 0.0f;
            };
            const float scap = bez_cap(cmd.pen.start_cap);
            const float ecap = bez_cap(cmd.pen.end_cap);

            // 手动构造 6 个 SdfVertex（不用 push_sdf_quad_vertices，
            // 因为 half_w/half_h 槽用于存放 P3 本地坐标，p2/p3 槽用于存放 P2 本地坐标）
            containers::Vector<SdfVertex> verts;
            verts.reserve(6);

            // kind=9：half_w=P3.x_local, half_h=P3.y_local（借用）
            // p0=start_cap, p1=end_cap, p2=P2.x_local, p3=P2.y_local（借用）
            // e0,e1=P0本地坐标, e2,e3=P1本地坐标
            auto make_v9 = [&](float sx, float sy) -> SdfVertex {
                return {
                    sx, sy,
                    c.r, c.g, c.b, c.a,
                    sx - cx, sy - cy,               // 本地坐标（相对 Quad 中心）
                    9.0f,                            // kind=9
                    lp3x, lp3y,                     // half_w=P3.x_local, half_h=P3.y_local
                    scap,                            // p0=start_cap
                    ecap, lp2x, lp2y,               // p1=end_cap, p2=P2.x_local, p3=P2.y_local
                    sw,                              // stroke_w
                    lp0x, lp0y, lp1x, lp1y          // e0,e1=P0, e2,e3=P1
                };
            };

            // Quad 屏幕坐标（包围盒 6 顶点）
            const float x1 = cx - quad_hw;
            const float y1 = cy - quad_hh;
            const float x2 = cx + quad_hw;
            const float y2 = cy + quad_hh;

            verts.push_back(make_v9(x1, y1));
            verts.push_back(make_v9(x2, y1));
            verts.push_back(make_v9(x1, y2));
            verts.push_back(make_v9(x2, y1));
            verts.push_back(make_v9(x2, y2));
            verts.push_back(make_v9(x1, y2));
            apply_sdf_transform(verts);

            gfx::BufferDesc vb9{};
            vb9.size       = static_cast<uint64_t>(verts.size()) * sizeof(SdfVertex);
            vb9.stride     = sizeof(SdfVertex);
            vb9.bind_flags = gfx::BufferBindFlags::Vertex;
            auto buf9 = device_->create_buffer(vb9, verts.data());
            if (!buf9) continue;

            cmd_list_->set_pipeline(active_sdf_pl);
            cmd_list_->set_constant_buffer(0, viewport_cb.get());
            cmd_list_->set_vertex_buffer(0, buf9.get(), 0);
            cmd_list_->draw(static_cast<uint32_t>(verts.size()), 1, 0, 0);
        }

        // ── FillPath（路径填充）──────────────────────────────────────────────

        else if (cmd.kind == DrawCmdKind::FillPath) {
            // 当前与 FillPolygon 一致：路径填充先支持纯色画刷。
            if (cmd.brush.kind() != BrushKind::SolidColor) continue;
            if (cmd.path_index >= static_cast<uint32_t>(dl.paths().size())) continue;

            const Path& path = dl.paths()[cmd.path_index];
            containers::Vector<FlattenedSubpath> subpaths;
            flatten_path_to_subpaths(path, subpaths);
            if (subpaths.empty()) continue;

            for (const auto& sp : subpaths) {
                // 填充仅处理闭合子路径；开放子路径不参与面填充。
                if (!sp.closed || sp.points.size() < 3) continue;

                // 多边形 SDF 常量缓冲上限 64 顶点，超出时等距降采样。
                containers::Vector<math::Vec2> poly_pts;
                reduce_vertices_evenly(sp.points, poly_pts, 64);
                if (poly_pts.size() < 3) continue;

                float min_x = poly_pts[0].x, min_y = poly_pts[0].y;
                float max_x = poly_pts[0].x, max_y = poly_pts[0].y;
                for (size_t i = 1; i < poly_pts.size(); ++i) {
                    min_x = std::min(min_x, poly_pts[i].x);
                    min_y = std::min(min_y, poly_pts[i].y);
                    max_x = std::max(max_x, poly_pts[i].x);
                    max_y = std::max(max_y, poly_pts[i].y);
                }

                const float cx = (min_x + max_x) * 0.5f;
                const float cy = (min_y + max_y) * 0.5f;
                const float hw = (max_x - min_x) * 0.5f;
                const float hh = (max_y - min_y) * 0.5f;
                const math::Color c = cmd.brush.color();

                containers::Vector<SdfVertex> sdf_verts;
                push_sdf_quad_vertices(sdf_verts, cx, cy, hw, hh, 1.0f,
                    c.r, c.g, c.b, c.a,
                    10.0f,
                    0.0f, 0.0f, 0.0f, 0.0f,
                    0.0f);
                apply_sdf_transform(sdf_verts);

                gfx::BufferDesc vb_poly{};
                vb_poly.size       = static_cast<uint64_t>(sdf_verts.size()) * sizeof(SdfVertex);
                vb_poly.stride     = sizeof(SdfVertex);
                vb_poly.bind_flags = gfx::BufferBindFlags::Vertex;
                auto poly_vb = device_->create_buffer(vb_poly, sdf_verts.data());
                if (!poly_vb) continue;

                PolygonVertsCB poly_cb{};
                poly_cb.vert_count = static_cast<int>(poly_pts.size());
                poly_cb.pad[0] = poly_cb.pad[1] = poly_cb.pad[2] = 0.0f;
                for (int k = 0; k < poly_cb.vert_count; ++k) {
                    poly_cb.verts[k][0] = poly_pts[static_cast<size_t>(k)].x - cx;
                    poly_cb.verts[k][1] = poly_pts[static_cast<size_t>(k)].y - cy;
                    poly_cb.verts[k][2] = 0.0f;
                    poly_cb.verts[k][3] = 0.0f;
                }

                gfx::BufferDesc cb_poly{};
                cb_poly.size       = sizeof(PolygonVertsCB);
                cb_poly.stride     = 0;
                cb_poly.bind_flags = gfx::BufferBindFlags::Constant;
                auto poly_cb_buf = device_->create_buffer(cb_poly, &poly_cb);
                if (!poly_cb_buf) continue;

                cmd_list_->set_pipeline(active_sdf_pl);
                cmd_list_->set_constant_buffer(0, viewport_cb.get());
                cmd_list_->set_constant_buffer(1, poly_cb_buf.get());
                cmd_list_->set_vertex_buffer(0, poly_vb.get(), 0);
                cmd_list_->draw(static_cast<uint32_t>(sdf_verts.size()), 1, 0, 0);
            }
        }

        // ── StrokePath（路径描边）────────────────────────────────────────────
        // 遍历路径的原始命令（MoveTo/LineTo/QuadTo/CubicTo/Close），
        // 对每段分别发射对应的 SDF 图元（kind=6/8/9），真正实现曲线 SDF 抗锯齿描边：
        //   - LineTo  → kind=6（线段 SDF）
        //   - QuadTo  → kind=8（二次贝塞尔 SDF，IQ 解析解）
        //   - CubicTo → kind=9（三次贝塞尔 SDF，数值迭代）
        // 相邻段接缝使用 Round cap，路径首尾使用 pen 指定样式。

        else if (cmd.kind == DrawCmdKind::StrokePath) {
            if (cmd.brush.kind() != BrushKind::SolidColor) continue;
            if (cmd.pen.width <= 0.0f) continue;
            if (cmd.path_index >= static_cast<uint32_t>(dl.paths().size())) continue;

            const Path& path = dl.paths()[cmd.path_index];
            const math::Color c = cmd.brush.color();
            const float sw      = cmd.pen.width;
            const float half_sw = sw * 0.5f;
            const float padding = half_sw + 1.0f;  // SDF quad 外扩：半线宽 + AA 余量

            // 线段端点 cap 编码（0=Flat, 1=Round, 2=Square）
            auto line_cap = [](LineCap cap) -> float {
                if (cap == LineCap::Round)  return 1.0f;
                if (cap == LineCap::Square) return 2.0f;
                return 0.0f;
            };
            // 贝塞尔端点 cap 编码（kind=8/9 不支持 Square，Square 回退为 Flat）
            auto bez_cap = [](LineCap cap) -> float {
                return (cap == LineCap::Round) ? 1.0f : 0.0f;
            };
            const float scap_line = line_cap(cmd.pen.start_cap);
            const float ecap_line = line_cap(cmd.pen.end_cap);
            const float scap_bez  = bez_cap(cmd.pen.start_cap);
            const float ecap_bez  = bez_cap(cmd.pen.end_cap);

            containers::Vector<SdfVertex> verts;

            // 按子路径收集段信息，flush 时根据首末位置设置正确的端点 cap
            struct SegInfo {
                int        type;        // 0=线段, 1=QuadTo, 2=CubicTo
                math::Vec2 p0, p1, p2, p3;  // 起点 / 控制点 / 终点（按 type 使用）
            };
            containers::Vector<SegInfo> subpath_segs;
            bool subpath_closed = false;

            // 发射当前子路径所有段，清空 subpath_segs
            auto flush_subpath = [&]() {
                const size_t n = subpath_segs.size();
                if (n == 0) return;

                for (size_t i = 0; i < n; i++) {
                    const SegInfo& seg  = subpath_segs[i];
                    const bool is_first = (i == 0);
                    const bool is_last  = (i == n - 1);
                    // 闭合路径无开放端点，所有接缝用 Round
                    const float sc_l = subpath_closed ? 1.0f : (is_first ? scap_line : 1.0f);
                    const float ec_l = subpath_closed ? 1.0f : (is_last  ? ecap_line : 1.0f);
                    const float sc_b = subpath_closed ? 1.0f : (is_first ? scap_bez  : 1.0f);
                    const float ec_b = subpath_closed ? 1.0f : (is_last  ? ecap_bez  : 1.0f);

                    if (seg.type == 0) {
                        // ── 线段（kind=6）────────────────────────────────────
                        const float p0x = seg.p0.x, p0y = seg.p0.y;
                        const float p1x = seg.p1.x, p1y = seg.p1.y;
                        const float segcx = (p0x + p1x) * 0.5f;
                        const float segcy = (p0y + p1y) * 0.5f;
                        const float hw = std::abs(p1x - p0x) * 0.5f + padding;
                        const float hh = std::abs(p1y - p0y) * 0.5f + padding;
                        const float lp0x = p0x - segcx, lp0y = p0y - segcy;
                        const float lp1x = p1x - segcx, lp1y = p1y - segcy;
                        push_sdf_quad_vertices(verts, segcx, segcy, hw, hh, 0.0f,
                            c.r, c.g, c.b, c.a,
                            6.0f, sc_l, ec_l, 0.0f, 0.0f,
                            sw, lp0x, lp0y, lp1x, lp1y);

                    } else if (seg.type == 1) {
                        // ── 二次贝塞尔（kind=8）──────────────────────────────
                        const float p0x = seg.p0.x, p0y = seg.p0.y;  // 起点
                        const float p1x = seg.p1.x, p1y = seg.p1.y;  // 控制点
                        const float p2x = seg.p2.x, p2y = seg.p2.y;  // 终点
                        const float xmin = std::min(p0x, std::min(p1x, p2x));
                        const float xmax = std::max(p0x, std::max(p1x, p2x));
                        const float ymin = std::min(p0y, std::min(p1y, p2y));
                        const float ymax = std::max(p0y, std::max(p1y, p2y));
                        const float cx   = (xmin + xmax) * 0.5f;
                        const float cy   = (ymin + ymax) * 0.5f;
                        const float qhw  = (xmax - xmin) * 0.5f + padding;
                        const float qhh  = (ymax - ymin) * 0.5f + padding;
                        const float lp0x = p0x - cx, lp0y = p0y - cy;  // P0 本地坐标
                        const float lp1x = p1x - cx, lp1y = p1y - cy;  // P1（控制点）本地坐标
                        const float lp2x = p2x - cx, lp2y = p2y - cy;  // P2（终点）本地坐标
                        // half_w/half_h 借用存放 P2 本地坐标，e0/e1=P0，e2/e3=P1
                        auto make_v8 = [&](float sx, float sy) -> SdfVertex {
                            return {sx, sy, c.r, c.g, c.b, c.a,
                                    sx - cx, sy - cy,
                                    8.0f, lp2x, lp2y,
                                    sc_b, ec_b, 0.0f, 0.0f, sw,
                                    lp0x, lp0y, lp1x, lp1y};
                        };
                        verts.push_back(make_v8(cx - qhw, cy - qhh));
                        verts.push_back(make_v8(cx + qhw, cy - qhh));
                        verts.push_back(make_v8(cx - qhw, cy + qhh));
                        verts.push_back(make_v8(cx + qhw, cy - qhh));
                        verts.push_back(make_v8(cx + qhw, cy + qhh));
                        verts.push_back(make_v8(cx - qhw, cy + qhh));

                    } else {
                        // ── 三次贝塞尔（kind=9）──────────────────────────────
                        const float p0x = seg.p0.x, p0y = seg.p0.y;  // 起点
                        const float p1x = seg.p1.x, p1y = seg.p1.y;  // 控制点1
                        const float p2x = seg.p2.x, p2y = seg.p2.y;  // 控制点2
                        const float p3x = seg.p3.x, p3y = seg.p3.y;  // 终点
                        const float xmin = std::min(p0x, std::min(p1x, std::min(p2x, p3x)));
                        const float xmax = std::max(p0x, std::max(p1x, std::max(p2x, p3x)));
                        const float ymin = std::min(p0y, std::min(p1y, std::min(p2y, p3y)));
                        const float ymax = std::max(p0y, std::max(p1y, std::max(p2y, p3y)));
                        const float cx   = (xmin + xmax) * 0.5f;
                        const float cy   = (ymin + ymax) * 0.5f;
                        const float qhw  = (xmax - xmin) * 0.5f + padding;
                        const float qhh  = (ymax - ymin) * 0.5f + padding;
                        const float lp0x = p0x - cx, lp0y = p0y - cy;  // P0 本地坐标
                        const float lp1x = p1x - cx, lp1y = p1y - cy;  // P1 本地坐标
                        const float lp2x = p2x - cx, lp2y = p2y - cy;  // P2 本地坐标（借用 p2/p3 槽）
                        const float lp3x = p3x - cx, lp3y = p3y - cy;  // P3 本地坐标（借用 half_w/half_h 槽）
                        // half_w/half_h 借用存放 P3 本地坐标，p2/p3 槽存放 P2 本地坐标
                        auto make_v9 = [&](float sx, float sy) -> SdfVertex {
                            return {sx, sy, c.r, c.g, c.b, c.a,
                                    sx - cx, sy - cy,
                                    9.0f, lp3x, lp3y,
                                    sc_b, ec_b, lp2x, lp2y, sw,
                                    lp0x, lp0y, lp1x, lp1y};
                        };
                        verts.push_back(make_v9(cx - qhw, cy - qhh));
                        verts.push_back(make_v9(cx + qhw, cy - qhh));
                        verts.push_back(make_v9(cx - qhw, cy + qhh));
                        verts.push_back(make_v9(cx + qhw, cy - qhh));
                        verts.push_back(make_v9(cx + qhw, cy + qhh));
                        verts.push_back(make_v9(cx - qhw, cy + qhh));
                    }
                }
                subpath_segs.clear();
                subpath_closed = false;
            };

            // 遍历原始路径命令，按类型收集段并在子路径结束时 flush
            math::Vec2 cur_pt{0.0f, 0.0f};
            math::Vec2 start_pt{0.0f, 0.0f};
            bool has_cur = false;

            for (const auto& pc : path.cmds()) {
                switch (pc.kind) {
                case PathCmdKind::MoveTo:
                    flush_subpath();
                    cur_pt   = pc.pt[0];
                    start_pt = pc.pt[0];
                    has_cur  = true;
                    break;

                case PathCmdKind::LineTo:
                    if (!has_cur) break;
                    subpath_segs.push_back({0, cur_pt, pc.pt[0], {}, {}});
                    cur_pt = pc.pt[0];
                    break;

                case PathCmdKind::QuadTo:
                    if (!has_cur) break;
                    // pt[0]=控制点，pt[1]=终点
                    subpath_segs.push_back({1, cur_pt, pc.pt[0], pc.pt[1], {}});
                    cur_pt = pc.pt[1];
                    break;

                case PathCmdKind::CubicTo:
                    if (!has_cur) break;
                    // pt[0]=控制点1，pt[1]=控制点2，pt[2]=终点
                    subpath_segs.push_back({2, cur_pt, pc.pt[0], pc.pt[1], pc.pt[2]});
                    cur_pt = pc.pt[2];
                    break;

                case PathCmdKind::Close:
                    if (!has_cur) break;
                    // 首尾不重合时追加关闭线段
                    if (std::abs(cur_pt.x - start_pt.x) > 1e-4f ||
                        std::abs(cur_pt.y - start_pt.y) > 1e-4f) {
                        subpath_segs.push_back({0, cur_pt, start_pt, {}, {}});
                    }
                    subpath_closed = true;
                    flush_subpath();
                    cur_pt  = start_pt;
                    has_cur = false;
                    break;

                default:
                    break;
                }
            }
            flush_subpath();  // 处理最后一个未关闭子路径

            if (verts.empty()) continue;
            apply_sdf_transform(verts);

            gfx::BufferDesc vb{};
            vb.size       = static_cast<uint64_t>(verts.size()) * sizeof(SdfVertex);
            vb.stride     = sizeof(SdfVertex);
            vb.bind_flags = gfx::BufferBindFlags::Vertex;
            auto buf = device_->create_buffer(vb, verts.data());
            if (!buf) continue;

            cmd_list_->set_pipeline(active_sdf_pl);
            cmd_list_->set_constant_buffer(0, viewport_cb.get());
            cmd_list_->set_vertex_buffer(0, buf.get(), 0);
            cmd_list_->draw(static_cast<uint32_t>(verts.size()), 1, 0, 0);
        }

        // ── 裁剪状态命令（ClipPush* / ClipPop）──────────────────────────────
        //
        // 不再使用模板缓冲 + ClipWrite/ClipErase 管线；改为直接填写 ClipSdfCB（b3 槽），
        // 由主 SDF 像素着色器的 evaluate_clip_coverage() 在每像素处进行平滑 SDF 评估，
        // 消除了原 stencil 方案中裁剪边缘的锯齿问题。

        else if (cmd.kind == DrawCmdKind::ClipPushRect         ||
                 cmd.kind == DrawCmdKind::ClipPushRoundedRect   ||
                 cmd.kind == DrawCmdKind::ClipPushComplexRoundedRect ||
                 cmd.kind == DrawCmdKind::ClipPushPolygon        ||
                 cmd.kind == DrawCmdKind::ClipPushPath)
        {
            // 超出最大层数则静默跳过（不影响已有裁剪状态）
            if (clip_sdf_data.clip_count >= k_max_clip_layers) continue;

            // 填写新的裁剪层数据
            ClipSdfLayer& layer = clip_sdf_data.layers[clip_sdf_data.clip_count];
            memset(&layer, 0, sizeof(ClipSdfLayer));

            if (cmd.kind == DrawCmdKind::ClipPushRect) {
                layer.cx     = cmd.rect.x + cmd.rect.width  * 0.5f;
                layer.cy     = cmd.rect.y + cmd.rect.height * 0.5f;
                layer.half_w = cmd.rect.width  * 0.5f;
                layer.half_h = cmd.rect.height * 0.5f;
                layer.kind   = 0.0f;  // 矩形
            }
            else if (cmd.kind == DrawCmdKind::ClipPushRoundedRect) {
                layer.cx     = cmd.rrect.rect.x + cmd.rrect.rect.width  * 0.5f;
                layer.cy     = cmd.rrect.rect.y + cmd.rrect.rect.height * 0.5f;
                layer.half_w = cmd.rrect.rect.width  * 0.5f;
                layer.half_h = cmd.rrect.rect.height * 0.5f;
                layer.kind   = 1.0f;  // 均匀圆角矩形
                layer.p0     = std::min({cmd.rrect.radius_x, cmd.rrect.radius_y,
                                         layer.half_w, layer.half_h});
            }
            else if (cmd.kind == DrawCmdKind::ClipPushComplexRoundedRect) {
                const float hw = cmd.complex_rrect.rect.width  * 0.5f;
                const float hh = cmd.complex_rrect.rect.height * 0.5f;
                layer.cx     = cmd.complex_rrect.rect.x + hw;
                layer.cy     = cmd.complex_rrect.rect.y + hh;
                layer.half_w = hw;
                layer.half_h = hh;
                layer.kind   = 2.0f;  // 四角独立圆角矩形
                const auto& radii = cmd.complex_rrect.radii;
                layer.p0 = std::min(std::min(radii.bottom_right.x, radii.bottom_right.y), std::min(hw, hh));  // r_br
                layer.p1 = std::min(std::min(radii.top_right.x,    radii.top_right.y),    std::min(hw, hh));  // r_tr
                layer.p2 = std::min(std::min(radii.bottom_left.x,  radii.bottom_left.y),  std::min(hw, hh));  // r_bl
                layer.p3 = std::min(std::min(radii.top_left.x,     radii.top_left.y),     std::min(hw, hh));  // r_tl
            }
            else if (cmd.kind == DrawCmdKind::ClipPushPolygon) {
                // 提取多边形顶点，转换为相对 AABB 中心的本地坐标
                if (cmd.path_index >= static_cast<uint32_t>(dl.paths().size())) continue;
                const Path& poly_path = dl.paths()[cmd.path_index];
                containers::Vector<math::Vec2> poly_verts;
                for (const auto& pc : poly_path.cmds()) {
                    if (pc.kind == PathCmdKind::MoveTo || pc.kind == PathCmdKind::LineTo) {
                        poly_verts.push_back(pc.pt[0]);
                    }
                }
                const int poly_n = static_cast<int>(poly_verts.size());
                if (poly_n < 3 || poly_n > k_max_clip_poly_verts) continue;

                layer.cx     = cmd.pt_a.x;
                layer.cy     = cmd.pt_a.y;
                layer.half_w = cmd.pt_b.x;
                layer.half_h = cmd.pt_b.y;
                layer.kind   = 10.0f;  // 多边形
                layer.poly_vert_count_f = static_cast<float>(poly_n);
                for (int k = 0; k < poly_n; ++k) {
                    layer.poly_verts[k][0] = poly_verts[k].x - cmd.pt_a.x;
                    layer.poly_verts[k][1] = poly_verts[k].y - cmd.pt_a.y;
                    layer.poly_verts[k][2] = 0.0f;
                    layer.poly_verts[k][3] = 0.0f;
                }
            }
            else if (cmd.kind == DrawCmdKind::ClipPushPath) {
                // 扁平化路径为折线，取最大子路径，降采样至 k_max_clip_poly_verts 顶点
                if (cmd.path_index >= static_cast<uint32_t>(dl.paths().size())) continue;
                const Path& clip_path_ref = dl.paths()[cmd.path_index];

                containers::Vector<FlattenedSubpath> clip_subpaths;
                flatten_path_to_subpaths(clip_path_ref, clip_subpaths);
                if (clip_subpaths.empty()) continue;

                // 优先使用第一个闭合子路径；若无闭合子路径则取顶点最多的子路径
                const FlattenedSubpath* best = nullptr;
                for (const auto& sp : clip_subpaths) {
                    if (sp.closed && sp.points.size() >= 3) {
                        best = &sp;
                        break;
                    }
                }
                if (!best) {
                    for (const auto& sp : clip_subpaths) {
                        if (sp.points.size() >= 3) {
                            if (!best || sp.points.size() > best->points.size()) {
                                best = &sp;
                            }
                        }
                    }
                }
                if (!best) continue;

                // 降采样至最多 k_max_clip_poly_verts 顶点
                containers::Vector<math::Vec2> reduced;
                reduce_vertices_evenly(best->points, reduced, static_cast<size_t>(k_max_clip_poly_verts));
                const int poly_n = static_cast<int>(reduced.size());
                if (poly_n < 3) continue;

                // 计算多边形 AABB 作为裁剪层包围盒
                float mn_x = reduced[0].x, mn_y = reduced[0].y;
                float mx_x = reduced[0].x, mx_y = reduced[0].y;
                for (const auto& v : reduced) {
                    mn_x = std::min(mn_x, v.x); mn_y = std::min(mn_y, v.y);
                    mx_x = std::max(mx_x, v.x); mx_y = std::max(mx_y, v.y);
                }
                const float cen_x = (mn_x + mx_x) * 0.5f;
                const float cen_y = (mn_y + mx_y) * 0.5f;

                layer.cx     = cen_x;
                layer.cy     = cen_y;
                layer.half_w = (mx_x - mn_x) * 0.5f;
                layer.half_h = (mx_y - mn_y) * 0.5f;
                layer.kind   = 10.0f;  // 多边形 SDF 裁剪
                layer.poly_vert_count_f = static_cast<float>(poly_n);
                for (int k = 0; k < poly_n; ++k) {
                    layer.poly_verts[k][0] = reduced[k].x - cen_x;
                    layer.poly_verts[k][1] = reduced[k].y - cen_y;
                    layer.poly_verts[k][2] = 0.0f;
                    layer.poly_verts[k][3] = 0.0f;
                }
            }

            clip_sdf_data.clip_count++;

            // 重建 GPU 端 ClipSdfCB 并绑定至 b3 槽
            clip_sdf_cb = rebuild_clip_sdf_cb();
            if (clip_sdf_cb) cmd_list_->set_constant_buffer(3, clip_sdf_cb.get());
        }

        else if (cmd.kind == DrawCmdKind::ClipPop) {
            // 裁剪栈为空时忽略
            if (clip_sdf_data.clip_count <= 0) continue;

            clip_sdf_data.clip_count--;

            // 重建 GPU 端 ClipSdfCB 并绑定至 b3 槽
            clip_sdf_cb = rebuild_clip_sdf_cb();
            if (clip_sdf_cb) cmd_list_->set_constant_buffer(3, clip_sdf_cb.get());
        }

        // ── FillPolygon / StrokePolygon（SDF 多边形）─────────────────────────

        else if (cmd.kind == DrawCmdKind::FillPolygon
              || cmd.kind == DrawCmdKind::StrokePolygon)
        {
            if (cmd.brush.kind() != BrushKind::SolidColor) continue;

            // 从 DisplayList 的 paths 中取出多边形顶点路径
            if (cmd.path_index >= static_cast<uint32_t>(dl.paths().size())) continue;
            const Path& poly_path = dl.paths()[cmd.path_index];

            // 收集有效顶点（只读 MoveTo/LineTo 命令的首个点，跳过 Close）
            containers::Vector<math::Vec2> poly_verts_list;
            for (const auto& pc : poly_path.cmds()) {
                if (pc.kind == PathCmdKind::MoveTo || pc.kind == PathCmdKind::LineTo) {
                    poly_verts_list.push_back(pc.pt[0]);
                }
            }
            const int poly_n = static_cast<int>(poly_verts_list.size());
            if (poly_n < 3 || poly_n > 64) continue;  // 顶点数超出支持范围

            const math::Color c    = cmd.brush.color();
            const float       cx   = cmd.pt_a.x;         // AABB 中心 x
            const float       cy   = cmd.pt_a.y;         // AABB 中心 y
            const float       hw   = cmd.pt_b.x;         // AABB 半宽
            const float       hh   = cmd.pt_b.y;         // AABB 半高
            const float       kind_val = (cmd.kind == DrawCmdKind::FillPolygon) ? 10.0f : 11.0f;
            const float       stroke_w = (cmd.kind == DrawCmdKind::StrokePolygon)
                                         ? cmd.pen.width : 0.0f;

            // ── 构建 SDF 包围盒顶点（复用现有辅助函数）──────────────────
            containers::Vector<SdfVertex> sdf_verts;
            push_sdf_quad_vertices(sdf_verts, cx, cy, hw, hh, 1.0f,
                c.r, c.g, c.b, c.a,
                kind_val,
                0.0f, 0.0f, 0.0f, 0.0f,
                stroke_w);
            apply_sdf_transform(sdf_verts);

            gfx::BufferDesc vb_poly{};
            vb_poly.size       = static_cast<uint64_t>(sdf_verts.size()) * sizeof(SdfVertex);
            vb_poly.stride     = sizeof(SdfVertex);
            vb_poly.bind_flags = gfx::BufferBindFlags::Vertex;
            auto poly_vb = device_->create_buffer(vb_poly, sdf_verts.data());
            if (!poly_vb) continue;

            // ── 构建多边形顶点常量缓冲（b1 槽）──────────────────────────
            // 顶点转换为本地坐标（相对 AABB 中心），与像素着色器中的 local 坐标系一致
            PolygonVertsCB poly_cb{};
            poly_cb.vert_count = poly_n;
            poly_cb.pad[0] = poly_cb.pad[1] = poly_cb.pad[2] = 0.0f;
            for (int k = 0; k < poly_n; ++k) {
                poly_cb.verts[k][0] = poly_verts_list[k].x - cx;  // 本地 x
                poly_cb.verts[k][1] = poly_verts_list[k].y - cy;  // 本地 y
                poly_cb.verts[k][2] = 0.0f;
                poly_cb.verts[k][3] = 0.0f;
            }

            gfx::BufferDesc cb_poly{};
            cb_poly.size       = sizeof(PolygonVertsCB);
            cb_poly.stride     = 0;
            cb_poly.bind_flags = gfx::BufferBindFlags::Constant;
            auto poly_cb_buf = device_->create_buffer(cb_poly, &poly_cb);
            if (!poly_cb_buf) continue;

            cmd_list_->set_pipeline(active_sdf_pl);
            cmd_list_->set_constant_buffer(0, viewport_cb.get());
            cmd_list_->set_constant_buffer(1, poly_cb_buf.get());  // 多边形顶点（b1）
            cmd_list_->set_vertex_buffer(0, poly_vb.get(), 0);
            cmd_list_->draw(static_cast<uint32_t>(sdf_verts.size()), 1, 0, 0);
        }

        // ── DrawText（文字渲染，HarfBuzz 塑形）──────────────────────────

        else if (cmd.kind == DrawCmdKind::DrawText) {
            if (cmd.brush.kind() != BrushKind::SolidColor) continue;
            if (!glyph_atlas_) continue;

            // 获取 TextRun（path_index 复用为 text_run 索引）
            const auto& text_runs = dl.text_runs();
            if (cmd.path_index >= static_cast<uint32_t>(text_runs.size())) continue;
            const TextRun& run = text_runs[cmd.path_index];
            const uint32_t run_len = static_cast<uint32_t>(run.utf8.size());

            if (run.font_face == nullptr || run_len == 0 || run.size_px <= 0.0f) continue;

            const uint32_t size_px = static_cast<uint32_t>(run.size_px + 0.5f);
            auto* face = static_cast<text::FontFace*>(run.font_face);
            const math::Color color = cmd.brush.color();

            // ── HarfBuzz 塑形（一次调用，获取字形序列 + kern/offset）────
            const text::ShapeResult shaped = face->shape_text(
                run.utf8.data(), run_len, run.size_px);

            // ── 阶段 1：光栅化全部字形并写入 CPU 图集 ───────────────────
            {
                for (const auto& g : shaped.glyphs) {
                    glyph_atlas_->get_or_insert_by_index(face, g.glyph_index, size_px);
                }
            }

            // ── 阶段 2：上传 CPU 图集到 GPU（若有新字形）────────────────
            glyph_atlas_->flush(device_);
            if (!glyph_atlas_->texture()) continue;

            // ── 阶段 3：按 HarfBuzz 塑形结果生成文字四边形顶点 ──────────
            containers::Vector<TextVertex> text_verts;

            float pen_x = run.origin_x;
            const float pen_y = run.origin_y;

            for (const auto& g : shaped.glyphs) {
                const AtlasEntry* entry = glyph_atlas_->get_or_insert_by_index(face, g.glyph_index, size_px);
                if (entry == nullptr) {
                    // 图集已满或光栅化失败：用字号估算跳过该字形
                    pen_x += g.x_advance + run.character_spacing;
                    continue;
                }

                // 空白字形（如空格）：仅推进笔触
                if (entry->atlas_w == 0) {
                    pen_x += g.x_advance + run.character_spacing;
                    continue;
                }

                // 字形顶点左上角：笔触 + HarfBuzz 偏移 + FreeType bitmap bearing
                const float gx = std::roundf(pen_x
                    + g.x_offset
                    + static_cast<float>(entry->bearing_x));
                const float gy = std::roundf(pen_y
                    + g.y_offset
                    - static_cast<float>(entry->bearing_y));
                const float gw = static_cast<float>(entry->atlas_w);
                const float gh = static_cast<float>(entry->atlas_h);

                // 图集 UV
                const float inv = 1.0f / static_cast<float>(GlyphAtlas::kAtlasSize);
                const float u0  = static_cast<float>(entry->atlas_x) * inv;
                const float v0  = static_cast<float>(entry->atlas_y) * inv;
                const float u1  = (static_cast<float>(entry->atlas_x) + gw) * inv;
                const float v1  = (static_cast<float>(entry->atlas_y) + gh) * inv;

                const float cr = color.r, cg = color.g, cb = color.b, ca = color.a;

                // 2 个三角形（6 顶点）
                text_verts.push_back({gx,      gy,      u0, v0, cr, cg, cb, ca});
                text_verts.push_back({gx + gw, gy,      u1, v0, cr, cg, cb, ca});
                text_verts.push_back({gx,      gy + gh, u0, v1, cr, cg, cb, ca});
                text_verts.push_back({gx + gw, gy,      u1, v0, cr, cg, cb, ca});
                text_verts.push_back({gx + gw, gy + gh, u1, v1, cr, cg, cb, ca});
                text_verts.push_back({gx,      gy + gh, u0, v1, cr, cg, cb, ca});

                // HarfBuzz 塑形前进量 + 字符间距
                pen_x += g.x_advance + run.character_spacing;
            }

            if (text_verts.empty()) continue;
            apply_text_transform(text_verts);

            // ── 阶段 4：提交 DrawCall ────────────────────────────────────
            gfx::BufferDesc vb{};
            vb.size       = static_cast<uint64_t>(text_verts.size()) * sizeof(TextVertex);
            vb.stride     = sizeof(TextVertex);
            vb.bind_flags = gfx::BufferBindFlags::Vertex;
            auto text_vb = device_->create_buffer(vb, text_verts.data());
            if (!text_vb) continue;

            cmd_list_->set_pipeline(active_text_pl);
            cmd_list_->set_constant_buffer(0, viewport_cb.get());
            cmd_list_->set_shader_resource(0, glyph_atlas_->texture());
            cmd_list_->set_vertex_buffer(0, text_vb.get(), 0);
            cmd_list_->draw(static_cast<uint32_t>(text_verts.size()), 1, 0, 0);
        }
    }
    // 注：所有在循环内创建的临时顶点缓冲在 OwnedPtr 析构时释放，
    //     D3D11 延迟上下文在录制时已通过 COM 引用计数持有缓冲，安全释放。
}

// ── 工厂函数实现 ─────────────────────────────────────────────────────────────

core::OwnedPtr<IRenderer> create_renderer(gfx::IDevice* device) {
    auto* raw = MINE_NEW(RhiRenderer, device);

    if (!raw->is_valid()) {
        MINE_DELETE(raw);
        return nullptr;
    }

    return core::OwnedPtr<IRenderer>(
        raw,
        &core::detail::typed_deleter<RhiRenderer>);
}

} // namespace mine::paint


