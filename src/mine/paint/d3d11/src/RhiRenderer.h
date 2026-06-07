/**
 * @file RhiRenderer.h
 * @brief 基于 RHI 的 2D 渲染器类声明。
 *
 * 从 RhiRenderer.cpp 中提取，将接口声明与实现分离。
 * RhiRenderer 实现 IRenderer 接口，将 DisplayList 转换为 GPU 绘制调用。
 */

#pragma once

#include <mine/paint/IRenderer.h>
#include <mine/paint/DisplayList.h>
#include <mine/paint/DrawCmd.h>
#include <mine/gfx/IDevice.h>
#include <mine/gfx/IQueue.h>
#include <mine/gfx/ICommandList.h>
#include <mine/gfx/IPipeline.h>
#include <mine/gfx/IBuffer.h>
#include <mine/gfx/ITexture.h>
#include <mine/core/Memory.h>
#include <mine/containers/Vector.h>
#include <mine/math/Vec2.h>
#include "PaintShaderTypes.h"
#include "GlyphAtlas.h"

namespace mine::paint {

/**
 * @brief 基于 RHI 的 2D 渲染器。
 *
 * 所有 GPU 调用都通过 mine.gfx.rhi 抽象层进行，不直接使用具体图形 API。
 *
 * 包含的渲染管线：
 *   - solid_pipeline_：折线描边展开（StrokePath），POSITION + COLOR 顶点
 *   - sdf_pipeline_  ：SDF 形状（矩形/圆角/椭圆），SdfVertex 顶点
 *   - text_pipeline_ ：文字渲染（字形图集四边形），TextVertex 顶点
 *   - blur_pipeline_ ：亚克力背景高斯模糊（全屏四边形），BlurVertex 顶点
 */
class RhiRenderer final : public IRenderer {
public:
    explicit RhiRenderer(gfx::IDevice* device);
    ~RhiRenderer() override = default;

    RhiRenderer(const RhiRenderer&)            = delete;
    RhiRenderer& operator=(const RhiRenderer&) = delete;

    // ── IRenderer 接口实现 ─────────────────────────────────────────────────

    void begin_frame() override;
    void end_frame()   override;
    void render(const DisplayList& dl, gfx::ITexture* target) override;
    void set_dpi_scale(float scale) override;

    /// 判断渲染器是否初始化成功
    [[nodiscard]] bool is_valid() const noexcept { return valid_; }

private:
    // ── 管线创建 ──────────────────────────────────────────────────────────

    bool create_solid_pipeline();     ///< 折线描边管线（POSITION+COLOR）
    bool create_sdf_pipeline();       ///< SDF 形状管线（SdfVertex）
    bool create_text_pipeline();      ///< 文字渲染管线（TextVertex）
    bool create_blur_pipeline();      ///< 高斯模糊管线（BlurVertex）
    bool create_clip_pipelines();     ///< 裁剪相关管线（ClipWrite/Erase/Test）

    // ── 资源管理 ──────────────────────────────────────────────────────────

    bool ensure_stencil_texture(gfx::ITexture* target);
    bool ensure_scratch_textures(gfx::ITexture* target);

    // ── 顶点生成 ──────────────────────────────────────────────────────────

    /// 生成 SDF 形状包围盒的 6 个顶点
    static void push_sdf_quad_vertices(
        containers::Vector<SdfVertex>& vertices,
        float cx, float cy,
        float half_w, float half_h, float padding,
        float r, float g, float b, float a,
        float kind,
        float p0, float p1, float p2, float p3,
        float stroke_w,
        float e0 = 0.0f, float e1 = 0.0f, float e2 = 0.0f, float e3 = 0.0f);

    // ── 画刷数据 ──────────────────────────────────────────────────────────

    /// 构建画刷数据常量缓冲
    static BrushDataCB make_brush_cb(
        const Brush& brush,
        float cx, float cy,
        float half_w, float half_h) noexcept;

    // ── 成员变量 ──────────────────────────────────────────────────────────

    bool  valid_{false};
    float dpi_scale_{1.0f};

    gfx::IDevice*                        device_{nullptr};
    core::OwnedPtr<gfx::IQueue>          queue_;
    core::OwnedPtr<gfx::ICommandList>    cmd_list_;
    core::OwnedPtr<gfx::IPipeline>       solid_pipeline_;
    core::OwnedPtr<gfx::IPipeline>       sdf_pipeline_;
    core::OwnedPtr<gfx::IPipeline>       text_pipeline_;
    core::OwnedPtr<gfx::IPipeline>       blur_pipeline_;
    core::OwnedPtr<gfx::ITexture>        blur_scratch_a_;
    core::OwnedPtr<gfx::ITexture>        blur_scratch_b_;
    core::OwnedPtr<GlyphAtlas>           glyph_atlas_;

    // ── 裁剪系统（模板缓冲方案）──────────────────────────────────────────
    core::OwnedPtr<gfx::ITexture>        clip_stencil_;
    core::OwnedPtr<gfx::IPipeline>       sdf_clip_write_pipeline_;
    core::OwnedPtr<gfx::IPipeline>       sdf_clip_erase_pipeline_;
    core::OwnedPtr<gfx::IPipeline>       sdf_clip_test_pipeline_;
    core::OwnedPtr<gfx::IPipeline>       text_clip_test_pipeline_;
    core::OwnedPtr<gfx::IPipeline>       solid_clip_test_pipeline_;
};

} // namespace mine::paint
