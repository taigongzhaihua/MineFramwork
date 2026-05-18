/**
 * @file RhiRenderer.cpp
 * @brief mine.paint 基于 RHI 的 2D 渲染器实现。
 *
 * 通过 mine.gfx.rhi 抽象层（IDevice、ICommandList、IPipeline 等）
 * 实现 IRenderer 接口，不依赖任何具体图形 API（D3D11/D3D12/Vulkan）。
 *
 * M0 阶段支持：
 *   - FillRect：将矩形三角化为 2 个三角形（6 个顶点），以纯色填充
 *
 * 顶点格式：float2 pos（屏幕像素坐标）+ float4 color（线性 RGBA）= 24 字节
 * 视口变换：在顶点着色器中将像素坐标映射到 NDC（Y 轴翻转）
 */

#include <mine/paint/IRenderer.h>
#include <mine/paint/DisplayList.h>
#include <mine/paint/DrawCmd.h>
#include <mine/paint/PathBuilder.h>
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

namespace mine::paint {

// ── HLSL 着色器源码（M0 阶段运行时编译，后续改为预编译字节码）──────────────

/// 顶点着色器：将屏幕像素坐标转换为 NDC，透传颜色
static constexpr const char k_vertex_shader_hlsl[] = R"hlsl(
// 常量缓冲：视口尺寸（字节大小必须为 16 的倍数）
cbuffer ViewportCB : register(b0) {
    float2 viewport_size;
    float2 _padding;
};

struct VSIn {
    float2 pos   : POSITION;
    float4 color : COLOR;
};

struct VSOut {
    float4 pos   : SV_Position;
    float4 color : COLOR;
};

VSOut main(VSIn input) {
    VSOut output;
    // 屏幕像素坐标 → NDC
    //   X：[0, width]  → [-1,  1]
    //   Y：[0, height] → [ 1, -1]（屏幕 Y 向下，NDC Y 向上，故翻转）
    output.pos.x =  (input.pos.x / viewport_size.x) * 2.0f - 1.0f;
    output.pos.y = -(input.pos.y / viewport_size.y) * 2.0f + 1.0f;
    output.pos.z =  0.0f;
    output.pos.w =  1.0f;
    output.color =  input.color;
    return output;
}
)hlsl";

/// 像素着色器：直接输出顶点插值颜色
static constexpr const char k_pixel_shader_hlsl[] = R"hlsl(
struct PSIn {
    float4 pos   : SV_Position;
    float4 color : COLOR;
};

float4 main(PSIn input) : SV_Target {
    return input.color;
}
)hlsl";

// ── 顶点结构体 ────────────────────────────────────────────────────────────────

/// 2D 渲染顶点：屏幕像素坐标 + 线性 RGBA 颜色
struct PaintVertex {
    float x, y;        ///< 屏幕像素坐标（左上角为原点）
    float r, g, b, a;  ///< 线性 RGBA 颜色（[0,1]）
};

/// 视口常量缓冲布局（必须为 16 字节倍数，D3D11 硬性要求）
struct ViewportCB {
    float width;
    float height;
    float pad0{0.0f};
    float pad1{0.0f};
};

// ── 路径展平与三角化辅助函数 ──────────────────────────────────────────────────

/**
 * @brief 将单段三次贝塞尔曲线均匀细分为折线点，追加到 polygon 中。
 *
 * 使用 de Casteljau 公式逐步插值，每段 `segments` 个子步骤。
 * M0 固定 8 段，对应约 11.25° / 段，视觉误差 < 0.04%。
 *
 * @param polygon  输出多边形点列表（追加）
 * @param p0       曲线起点（已包含在 polygon 末尾，不重复写入）
 * @param p1,p2    两个控制点
 * @param p3       曲线终点
 * @param segments 细分段数（默认 8）
 */
static void flatten_cubic_bezier(
    containers::Vector<math::Vec2>& polygon,
    math::Vec2 p0, math::Vec2 p1, math::Vec2 p2, math::Vec2 p3,
    int32_t segments = 8)
{
    for (int32_t i = 1; i <= segments; ++i) {
        const float t  = static_cast<float>(i) / static_cast<float>(segments);
        const float u  = 1.0f - t;
        const float uu = u * u;
        const float tt = t * t;
        // 三次贝塞尔插值公式：B(t) = (1-t)^3*P0 + 3(1-t)^2*t*P1 + 3(1-t)*t^2*P2 + t^3*P3
        const float x = u * uu * p0.x + 3.0f * uu * t * p1.x + 3.0f * u * tt * p2.x + t * tt * p3.x;
        const float y = u * uu * p0.y + 3.0f * uu * t * p1.y + 3.0f * u * tt * p2.y + t * tt * p3.y;
        polygon.push_back({x, y});
    }
}

/**
 * @brief 将 Path（由 PathBuilder 生成）展平为屏幕像素坐标多边形。
 *
 * 支持 MoveTo / LineTo / CubicTo / Close 命令（add_rounded_rect 的输出）。
 * QuadTo 支持：将二次贝塞尔提升为三次后细分。
 * Close 命令：不重复写入起点，外部使用 % n 环绕处理。
 *
 * @param polygon 输出多边形顶点（屏幕像素坐标）
 * @param path    要展平的路径
 */
static void flatten_path_to_polygon(
    containers::Vector<math::Vec2>& polygon,
    const Path& path)
{
    math::Vec2 current{};

    for (const PathCmd& cmd : path.cmds()) {
        switch (cmd.kind) {
        case PathCmdKind::MoveTo:
            current = cmd.pt[0];
            polygon.push_back(current);
            break;

        case PathCmdKind::LineTo:
            current = cmd.pt[0];
            polygon.push_back(current);
            break;

        case PathCmdKind::CubicTo:
            // pt[0]=控制点1，pt[1]=控制点2，pt[2]=终点
            flatten_cubic_bezier(polygon, current, cmd.pt[0], cmd.pt[1], cmd.pt[2]);
            current = cmd.pt[2];
            break;

        case PathCmdKind::QuadTo: {
            // 二次贝塞尔 → 提升为三次：
            //   C1 = P0 + (2/3)*(ctrl - P0)
            //   C2 = end + (2/3)*(ctrl - end)
            const math::Vec2 ctrl = cmd.pt[0];
            const math::Vec2 end  = cmd.pt[1];
            const math::Vec2 c1{current.x + (2.0f / 3.0f) * (ctrl.x - current.x),
                                current.y + (2.0f / 3.0f) * (ctrl.y - current.y)};
            const math::Vec2 c2{end.x + (2.0f / 3.0f) * (ctrl.x - end.x),
                                end.y + (2.0f / 3.0f) * (ctrl.y - end.y)};
            flatten_cubic_bezier(polygon, current, c1, c2, end, 4);
            current = end;
            break;
        }

        case PathCmdKind::Close:
            // 不重复加入起点；外部以 % n 索引处理环绕
            break;
        }
    }
}

/**
 * @brief 将凸多边形三角化（以 polygon[0] 为扇点）并写入顶点列表。
 *
 * 要求多边形为凸多边形（圆角矩形满足此条件）。
 * 生成 (n-2) 个三角形：(p[0], p[i], p[i+1])，i ∈ [1, n-2]。
 *
 * @param vertices 输出顶点列表（追加）
 * @param polygon  凸多边形顶点（屏幕像素坐标）
 * @param r,g,b,a  填充颜色（线性 RGBA）
 */
static void push_convex_polygon_vertices(
    containers::Vector<PaintVertex>& vertices,
    const containers::Vector<math::Vec2>& polygon,
    float r, float g, float b, float a)
{
    const uint32_t n = static_cast<uint32_t>(polygon.size());
    if (n < 3) return;

    // 扇形展开：以 polygon[0] 为扇点，覆盖整个凸多边形
    for (uint32_t i = 1; i + 1 < n; ++i) {
        vertices.push_back({polygon[0].x, polygon[0].y, r, g, b, a});
        vertices.push_back({polygon[i].x, polygon[i].y, r, g, b, a});
        vertices.push_back({polygon[i + 1].x, polygon[i + 1].y, r, g, b, a});
    }
}

// ── RhiRenderer 类 ────────────────────────────────────────────────────────────

/**
 * @brief 基于 RHI 的 2D 渲染器。
 *
 * 所有 GPU 调用都通过 mine.gfx.rhi 抽象层进行，不直接使用具体图形 API。
 */
class RhiRenderer final : public IRenderer {
public:
    explicit RhiRenderer(gfx::IDevice* device);
    ~RhiRenderer() override = default;

    RhiRenderer(const RhiRenderer&)            = delete;
    RhiRenderer& operator=(const RhiRenderer&) = delete;

    void begin_frame() override;
    void end_frame()   override;
    void render(const DisplayList& dl, gfx::ITexture* target) override;

    /// 判断渲染器是否初始化成功
    [[nodiscard]] bool is_valid() const noexcept { return valid_; }

private:
    /**
     * @brief 创建 2D 纯色渲染管线。
     * @return true 表示成功
     */
    bool create_pipeline();

    /**
     * @brief 将矩形 (x, y, w, h) 三角化为 6 个顶点写入 vertices。
     * @param x, y   矩形左上角屏幕像素坐标
     * @param w, h   矩形宽高（像素）
     * @param r,g,b,a 填充颜色（线性 RGBA）
     */
    static void push_rect_vertices(
        containers::Vector<PaintVertex>& vertices,
        float x, float y, float w, float h,
        float r, float g, float b, float a);

    bool valid_{false};

    gfx::IDevice*                        device_{nullptr};    ///< 图形设备（不拥有）
    core::OwnedPtr<gfx::IQueue>          queue_;              ///< 提交队列
    core::OwnedPtr<gfx::ICommandList>    cmd_list_;           ///< 命令录制列表
    core::OwnedPtr<gfx::IPipeline>       pipeline_;           ///< 2D 渲染管线
};

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

    // 创建 2D 渲染管线（含着色器编译）
    if (!create_pipeline()) return;

    valid_ = true;
}

// ── 管线创建 ──────────────────────────────────────────────────────────────────

bool RhiRenderer::create_pipeline() {
    // 顶点输入布局：float2 POSITION（offset=0）+ float4 COLOR（offset=8）
    gfx::PipelineDesc desc{};

    // 顶点着色器（HLSL 源码，运行时编译）
    desc.vertex_shader.data        = k_vertex_shader_hlsl;
    desc.vertex_shader.size        = sizeof(k_vertex_shader_hlsl) - 1; // 不含末尾 '\0'
    desc.vertex_shader.language    = gfx::ShaderLanguage::HLSL;
    desc.vertex_shader.entry_point = "main";
    desc.vertex_shader.is_source   = true;

    // 像素着色器（HLSL 源码，运行时编译）
    desc.pixel_shader.data        = k_pixel_shader_hlsl;
    desc.pixel_shader.size        = sizeof(k_pixel_shader_hlsl) - 1;
    desc.pixel_shader.language    = gfx::ShaderLanguage::HLSL;
    desc.pixel_shader.entry_point = "main";
    desc.pixel_shader.is_source   = true;

    // 顶点布局：POSITION（float2）+ COLOR（float4）
    desc.vertex_element_count = 2;
    desc.vertex_elements[0].semantic       = gfx::VertexSemantic::Position;
    desc.vertex_elements[0].semantic_index = 0;
    desc.vertex_elements[0].format         = gfx::VertexElementFormat::Float2;
    desc.vertex_elements[0].slot           = 0;
    desc.vertex_elements[0].offset         = 0;                   // float2 起始偏移

    desc.vertex_elements[1].semantic       = gfx::VertexSemantic::Color;
    desc.vertex_elements[1].semantic_index = 0;
    desc.vertex_elements[1].format         = gfx::VertexElementFormat::Float4;
    desc.vertex_elements[1].slot           = 0;
    desc.vertex_elements[1].offset         = sizeof(float) * 2;  // 紧跟 float2 后面

    desc.vertex_stride = sizeof(PaintVertex); // 24 字节
    desc.enable_blend  = true;  // 开启预乘 Alpha 混合（支持半透明 UI 元素）
    desc.enable_depth  = false; // 2D 渲染关闭深度测试

    pipeline_ = device_->create_pipeline(desc);
    return pipeline_ != nullptr;
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

void RhiRenderer::push_rect_vertices(
    containers::Vector<PaintVertex>& vertices,
    float x, float y, float w, float h,
    float r, float g, float b, float a) {

    // 矩形四个角点（屏幕像素坐标）
    const float x1 = x;
    const float y1 = y;
    const float x2 = x + w;
    const float y2 = y + h;

    // 三角形 1：左上 - 右上 - 左下
    vertices.push_back({x1, y1, r, g, b, a});
    vertices.push_back({x2, y1, r, g, b, a});
    vertices.push_back({x1, y2, r, g, b, a});

    // 三角形 2：右上 - 右下 - 左下
    vertices.push_back({x2, y1, r, g, b, a});
    vertices.push_back({x2, y2, r, g, b, a});
    vertices.push_back({x1, y2, r, g, b, a});
}

// ── 渲染 ──────────────────────────────────────────────────────────────────────

void RhiRenderer::render(const DisplayList& dl, gfx::ITexture* target) {
    if (!valid_ || target == nullptr) return;

    const auto& cmds = dl.cmds();
    if (cmds.empty()) return;

    // ── 1. 收集所有顶点 ────────────────────────────────────────────────

    containers::Vector<PaintVertex> vertices;

    for (const DrawCmd& cmd : cmds) {
        if (cmd.kind == DrawCmdKind::FillRect) {
            // 仅处理 SolidColor 画刷（M0）
            if (cmd.brush.kind() != BrushKind::SolidColor) continue;

            const math::Color c = cmd.brush.color();
            push_rect_vertices(
                vertices,
                cmd.rect.x, cmd.rect.y,
                cmd.rect.width, cmd.rect.height,
                c.r, c.g, c.b, c.a);
        }
        else if (cmd.kind == DrawCmdKind::FillRoundedRect) {
            // 仅处理 SolidColor 画刷（M0）
            if (cmd.brush.kind() != BrushKind::SolidColor) continue;

            const math::Color c = cmd.brush.color();

            // 若圆角半径为零，退化为普通矩形（避免不必要的路径展平开销）
            if (cmd.rrect.radius_x <= 0.0f || cmd.rrect.radius_y <= 0.0f) {
                push_rect_vertices(
                    vertices,
                    cmd.rrect.rect.x, cmd.rrect.rect.y,
                    cmd.rrect.rect.width, cmd.rrect.rect.height,
                    c.r, c.g, c.b, c.a);
                continue;
            }

            // 步骤1：用 PathBuilder 生成圆角矩形的三次贝塞尔路径
            PathBuilder builder;
            builder.add_rounded_rect(cmd.rrect);
            Path path = builder.build();

            // 步骤2：展平贝塞尔路径为屏幕像素多边形
            containers::Vector<math::Vec2> polygon;
            flatten_path_to_polygon(polygon, path);

            // 步骤3：凸多边形扇形三角化（圆角矩形始终是凸多边形）
            push_convex_polygon_vertices(vertices, polygon, c.r, c.g, c.b, c.a);
        }
        else if (cmd.kind == DrawCmdKind::FillComplexRoundedRect) {
            // 仅处理 SolidColor 画刷（M0）
            if (cmd.brush.kind() != BrushKind::SolidColor) continue;

            const math::Color c = cmd.brush.color();
            const auto& r = cmd.complex_rrect.radii;

            // 若所有圆角半径均为零，退化为普通矩形（避免路径展平开销）
            if (r.top_left.x <= 0.0f && r.top_left.y <= 0.0f &&
                r.top_right.x <= 0.0f && r.top_right.y <= 0.0f &&
                r.bottom_right.x <= 0.0f && r.bottom_right.y <= 0.0f &&
                r.bottom_left.x <= 0.0f && r.bottom_left.y <= 0.0f) {
                push_rect_vertices(
                    vertices,
                    cmd.complex_rrect.rect.x, cmd.complex_rrect.rect.y,
                    cmd.complex_rrect.rect.width, cmd.complex_rrect.rect.height,
                    c.r, c.g, c.b, c.a);
                continue;
            }

            // 步骤1：用 PathBuilder 生成四角独立椭圆半径的圆角矩形路径
            PathBuilder builder;
            builder.add_complex_rounded_rect(cmd.complex_rrect);
            Path path = builder.build();

            // 步骤2：展平贝塞尔路径为屏幕像素多边形
            containers::Vector<math::Vec2> polygon;
            flatten_path_to_polygon(polygon, path);

            // 步骤3：凸多边形扇形三角化（CSS 钳制保证始终为凸多边形）
            push_convex_polygon_vertices(vertices, polygon, c.r, c.g, c.b, c.a);
        }
        // M0：其余命令类型（FillEllipse、StrokeRect 等）暂不处理
    }

    if (vertices.empty()) return;

    // ── 2. 创建顶点缓冲（含初始数据，IMMUTABLE）────────────────────────

    gfx::BufferDesc vb_desc{};
    vb_desc.size       = static_cast<uint64_t>(vertices.size()) * sizeof(PaintVertex);
    vb_desc.stride     = sizeof(PaintVertex);
    vb_desc.bind_flags = gfx::BufferBindFlags::Vertex;

    auto vertex_buffer = device_->create_buffer(vb_desc, vertices.data());
    if (!vertex_buffer) return;

    // ── 3. 创建常量缓冲（视口尺寸）────────────────────────────────────

    const ViewportCB cb_data{
        static_cast<float>(target->width()),
        static_cast<float>(target->height()),
        0.0f, 0.0f
    };

    gfx::BufferDesc cb_desc{};
    cb_desc.size       = sizeof(ViewportCB); // 16 字节（已满足 D3D11 16 字节倍数要求）
    cb_desc.stride     = 0;                 // 常量缓冲不需要 stride
    cb_desc.bind_flags = gfx::BufferBindFlags::Constant;

    auto const_buffer = device_->create_buffer(cb_desc, &cb_data);
    if (!const_buffer) return;

    // ── 4. 录制 GPU 命令 ───────────────────────────────────────────────

    // 设置渲染目标（无深度缓冲）
    cmd_list_->set_render_target(target, nullptr);

    // 设置视口：覆盖整个渲染目标
    gfx::Viewport viewport{};
    viewport.x         = 0.0f;
    viewport.y         = 0.0f;
    viewport.width     = static_cast<float>(target->width());
    viewport.height    = static_cast<float>(target->height());
    viewport.min_depth = 0.0f;
    viewport.max_depth = 1.0f;
    cmd_list_->set_viewport(viewport);

    // 绑定管线（着色器、混合状态、光栅化状态等）
    cmd_list_->set_pipeline(pipeline_.get());

    // 绑定常量缓冲到槽位 0（顶点着色器用于视口变换）
    cmd_list_->set_constant_buffer(0, const_buffer.get());

    // 绑定顶点缓冲到槽位 0
    cmd_list_->set_vertex_buffer(0, vertex_buffer.get(), 0);

    // 提交绘制调用：绘制所有三角形（非索引，三角形列表）
    cmd_list_->draw(
        static_cast<uint32_t>(vertices.size()), // 顶点数（6 * 矩形数）
        1,                                       // 实例数
        0,                                       // 起始顶点
        0);                                      // 起始实例
    // 注：vertex_buffer / const_buffer 在此函数返回后销毁 OwnedPtr，
    //     但 D3D11 延迟上下文在录制时已通过 COM 引用计数持有缓冲，安全。
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
