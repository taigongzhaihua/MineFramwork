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
#include <mine/gfx/IDevice.h>
#include <mine/gfx/IQueue.h>
#include <mine/gfx/ICommandList.h>
#include <mine/gfx/IPipeline.h>
#include <mine/gfx/IBuffer.h>
#include <mine/gfx/ITexture.h>
#include <mine/gfx/GfxTypes.h>
#include <mine/core/Memory.h>
#include <mine/containers/Vector.h>

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
        // M0：其他命令类型暂不处理（FillRoundedRect、StrokeLine 等）
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
