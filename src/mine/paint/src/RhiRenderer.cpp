/**
 * @file RhiRenderer.cpp
 * @brief mine.paint 基于 RHI 的 2D 渲染器实现。
 *
 * 通过 mine.gfx.rhi 抽象层（IDevice、ICommandList、IPipeline 等）
 * 实现 IRenderer 接口，不依赖任何具体图形 API（D3D11/D3D12/Vulkan）。
 *
 * M0 阶段支持：
 *   - FillRect / StrokeRect：矩形 SDF 渲染，抗锯齿填充与中心对齐描边
 *   - FillRoundedRect / StrokeRoundedRect：均匀圆角矩形 SDF
 *   - FillComplexRoundedRect / StrokeComplexRoundedRect：四角独立圆角矩形 SDF
 *   - FillEllipse / StrokeEllipse：椭圆 SDF（IQ 近似公式）
 *   - StrokeBorderedRect：四边各自独立宽度的矩形内侧描边 SDF（kind=4）
 *   - StrokeBorderedRoundedRect：四边独立宽度 + 四角独立圆角的内侧描边 SDF（kind=5）
 *   - StrokeLine：折线展开描边（Miter join 方案）
 *
 * 渲染架构：
 *   - SDF 形状（矩形/圆角/椭圆）：每命令一次 DrawCall，SDF 参数编入顶点
 *   - 折线描边（StrokeLine）：顶点展开为三角带，solid pipeline
 *
 * SDF 顶点格式（SdfVertex，80 字节）：
 *   pos(2) + color(4) + local(2) + params0(4) + params1(4) + extra(4) float
 * 折线顶点格式（PaintVertex，24 字节）：pos(2) + color(4) float
 * 视口变换：顶点着色器将屏幕像素坐标映射到 NDC（Y 轴翻转）
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
#include <mine/math/Vec2.h>
#include <cmath>

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

// ── SDF 着色器源码（用于矩形/圆角矩形/复杂圆角/椭圆的填充与描边）──────────

/// SDF 顶点着色器：坐标变换，透传 SDF 参数
static constexpr const char k_sdf_vertex_shader_hlsl[] = R"hlsl(
cbuffer ViewportCB : register(b0) {
    float2 viewport_size;
    float2 _padding;
};

struct SdfVSIn {
    float2 pos     : POSITION;   // 屏幕像素坐标（包围盒顶点）
    float4 color   : COLOR;      // 线性 RGBA 颜色
    float2 local   : TEXCOORD0;  // 本地坐标（以形状中心为原点，像素单位）
    float4 params0 : TEXCOORD1;  // (kind, half_w, half_h, p0)
    float4 params1 : TEXCOORD2;  // (p1, p2, p3, stroke_w)
    float4 extra   : TEXCOORD3;  // 扩展参数（StrokeBorderedRoundedRect 的四角圆角半径）
};

struct SdfVSOut {
    float4 pos     : SV_Position;
    float4 color   : COLOR;
    float2 local   : TEXCOORD0;
    float4 params0 : TEXCOORD1;
    float4 params1 : TEXCOORD2;
    float4 extra   : TEXCOORD3;
};

SdfVSOut main(SdfVSIn i) {
    SdfVSOut o;
    // 屏幕像素坐标 → NDC（Y 轴翻转）
    o.pos.x =  (i.pos.x / viewport_size.x) * 2.0f - 1.0f;
    o.pos.y = -(i.pos.y / viewport_size.y) * 2.0f + 1.0f;
    o.pos.z =  0.0f;
    o.pos.w =  1.0f;
    o.color   = i.color;
    o.local   = i.local;
    o.params0 = i.params0;
    o.params1 = i.params1;
    o.extra   = i.extra;
    return o;
}
)hlsl";

/// SDF 像素着色器：根据形状类型计算有向距离场，实现亚像素抗锯齿
static constexpr const char k_sdf_pixel_shader_hlsl[] = R"hlsl(
// ── SDF 函数（外正内负，IQ 标准约定）─────────────────────────────────────────

// 矩形 SDF：p=本地坐标，b=半尺寸
float box_sdf(float2 p, float2 b) {
    float2 q = abs(p) - b;
    return length(max(q, 0.0f)) + min(max(q.x, q.y), 0.0f);
}

// 均匀圆角矩形 SDF：r = 统一圆角半径
float rounded_box_sdf(float2 p, float2 b, float r) {
    float2 q = abs(p) - b + r;
    return length(max(q, 0.0f)) + min(max(q.x, q.y), 0.0f) - r;
}

// 四角独立圆角矩形 SDF
// r = (右下, 右上, 左下, 左上) 各向同性圆角半径
float complex_rounded_box_sdf(float2 p, float2 b, float4 r) {
    r.xy = (p.x > 0.0f) ? r.xy : r.zw;
    r.x  = (p.y > 0.0f) ? r.x  : r.y;
    float2 q = abs(p) - b + r.x;
    return length(max(q, 0.0f)) + min(max(q.x, q.y), 0.0f) - r.x;
}

// 椭圆 SDF（IQ 近似，适用于中等扁率椭圆）
float ellipse_sdf(float2 p, float2 ab) {
    float k1 = length(p / ab);
    float k2 = length(p / (ab * ab));
    return (k1 - 1.0f) * k1 / max(k2, 1e-6f);
}

// ── 像素着色器主函数 ─────────────────────────────────────────────────────────

struct SdfPSIn {
    float4 pos     : SV_Position;
    float4 color   : COLOR;
    float2 local   : TEXCOORD0;
    float4 params0 : TEXCOORD1;  // (kind, half_w, half_h, p0)
    float4 params1 : TEXCOORD2;  // (p1, p2, p3, stroke_w)
    float4 extra   : TEXCOORD3;  // 扩展参数（圆角四边独立描边：x=r_tl, y=r_tr, z=r_br, w=r_bl）
};

float4 main(SdfPSIn i) : SV_Target {
    // 解包 SDF 参数
    int   kind     = (int)(i.params0.x + 0.5f);
    float half_w   = i.params0.y;
    float half_h   = i.params0.z;
    float p0       = i.params0.w;  // 圆角半径参数 0（均匀圆角/右下）
    float p1       = i.params1.x;  // 圆角半径参数 1（右上）
    float p2       = i.params1.y;  // 圆角半径参数 2（左下）
    float p3       = i.params1.z;  // 圆角半径参数 3（左上）
    float stroke_w = i.params1.w;  // 描边总宽度（0 = 填充模式）

    float2 p = i.local;
    float2 b = float2(half_w, half_h);

    // 根据形状类型计算有向距离场（外正内负）
    float d;
    if (kind == 0) {
        // 矩形
        d = box_sdf(p, b);
    } else if (kind == 1) {
        // 均匀圆角矩形
        d = rounded_box_sdf(p, b, p0);
    } else if (kind == 2) {
        // 四角独立圆角矩形（p0=右下, p1=右上, p2=左下, p3=左上）
        d = complex_rounded_box_sdf(p, b, float4(p0, p1, p2, p3));
    } else if (kind == 4) {
        // 四边不等宽矩形内侧描边（p0=top_w, p1=right_w, p2=bottom_w, p3=left_w）
        //
        // 算法：对四条边各自独立计算描边遮罩，取联集（max）。
        //   - 到上边缘的距离：dist_top    = p.y + b.y  （0 在上边缘，正值向下）
        //   - 到下边缘的距离：dist_bottom = b.y - p.y  （0 在下边缘，正值向上）
        //   - 到左边缘的距离：dist_left   = p.x + b.x
        //   - 到右边缘的距离：dist_right  = b.x - p.x
        // 当 dist < edge_width 时该像素在对应边的描边带内；用 smoothstep AA 过渡。
        // 宽度为 0 的边直接返回 0，不产生任何描边，彻底避免宽度相等时的 SDF 叠乘伪影。
        float d_outer = box_sdf(p, b);
        float fw_o = max(fwidth(d_outer), 0.5f);
        float a_outer = 1.0f - smoothstep(-fw_o, fw_o, d_outer);  // 1=矩形内，外侧 AA

        float fy = max(fwidth(p.y), 0.5f);
        float fx = max(fwidth(p.x), 0.5f);
        // 各边遮罩：dist < edge_width → 1.0（在带内），dist > edge_width → 0.0（带外），smoothstep AA
        float a_top    = p0 > 0.0f ? 1.0f - smoothstep(p0 - fy, p0 + fy, p.y + b.y) : 0.0f;
        float a_bottom = p2 > 0.0f ? 1.0f - smoothstep(p2 - fy, p2 + fy, b.y - p.y) : 0.0f;
        float a_left   = p3 > 0.0f ? 1.0f - smoothstep(p3 - fx, p3 + fx, p.x + b.x) : 0.0f;
        float a_right  = p1 > 0.0f ? 1.0f - smoothstep(p1 - fx, p1 + fx, b.x - p.x) : 0.0f;
        // 联集：任意一边有描边即显示；乘以外矩形遮罩确保超出矩形外的部分透明
        float a_stroke = max(max(a_top, a_bottom), max(a_left, a_right));
        float al = a_outer * a_stroke;
        float4 c2 = i.color;
        return float4(c2.rgb * c2.a * al, c2.a * al);
    } else if (kind == 5) {
        // 四边不等宽 + 四角独立圆角内侧描边
        //
        // 算法：外轮廓圆角矩形 SDF 减去内轮廓圆角矩形 SDF，描边区域 = 外内之差。
        // 内矩形由 Thickness 向各边收缩：
        //   inner_b.x = b.x - (left + right) / 2
        //   inner_b.y = b.y - (top  + bottom) / 2
        //   inner_p   = p - float2((left - right) / 2, (top - bottom) / 2)
        // 内圆角（仿 ComplexRoundedRect::inner_rect 公式，各向同性化后取 min）：
        //   ir_tl = max(0, r_tl - max(left, top))   ← x=r_tl-left, y=r_tl-top 的标量化
        //   其余角依此类推
        //
        // p0=top_w, p1=right_w, p2=bottom_w, p3=left_w（math::Thickness 字段顺序：left,top,right,bottom）
        // extra.x=r_tl（左上）, extra.y=r_tr（右上）, extra.z=r_br（右下）, extra.w=r_bl（左下）
        float r_tl = i.extra.x;
        float r_tr = i.extra.y;
        float r_br = i.extra.z;
        float r_bl = i.extra.w;

        // 外轮廓（含圆角）
        float d_outer = complex_rounded_box_sdf(p, b, float4(r_br, r_tr, r_bl, r_tl));
        float fw_o = max(fwidth(d_outer), 0.5f);
        float a_outer = 1.0f - smoothstep(-fw_o, fw_o, d_outer);

        // 内矩形：各边内缩，中心随非对称边宽偏移。
        //
        // 问题：当某边宽为0时，内矩形该侧边界与外矩形边界重合，
        //       两个 smoothstep 过渡同时激活，产生 a*(1-a)≤0.25 的 ghost 细线。
        //
        // 修复：对宽度为0的边，将内矩形对应侧边界向外扩展 (fw_o+1)px，
        //       超出外矩形 AA 区，使该侧 a_inner→1，消除 ghost。
        //       扩展时只移动零宽那侧边界，通过同步调整内矩形中心保证对侧边界不变。
        //
        // 设 la/ra/ta/ba 为各侧扩展量（零宽边取 fw_o+1，否则取 0）：
        //   inner_center_x 偏移 = (p3-p1-la+ra)/2  （向右为正）
        //   inner_b.x = b.x - (p3+p1)/2 + (la+ra)/2
        //   inner_p.x = p.x - (p3-p1-la+ra)/2
        float ghost = fw_o + 1.0f;
        float la = p3 < 0.0001f ? ghost : 0.0f;  // 左边宽=0时，内矩形左侧外扩
        float ra = p1 < 0.0001f ? ghost : 0.0f;  // 右边宽=0时，内矩形右侧外扩
        float ta = p0 < 0.0001f ? ghost : 0.0f;  // 上边宽=0时，内矩形上侧外扩
        float ba = p2 < 0.0001f ? ghost : 0.0f;  // 下边宽=0时，内矩形下侧外扩
        float2 inner_b = max(float2(b.x - (p3 + p1) * 0.5f + (la + ra) * 0.5f,
                                    b.y - (p0 + p2) * 0.5f + (ta + ba) * 0.5f),
                             float2(0.0f, 0.0f));
        float2 inner_p = p - float2((p3 - p1 - la + ra) * 0.5f, (p0 - p2 - ta + ba) * 0.5f);

        // 内圆角（对应 ComplexRoundedRect::inner_rect：x 分量减水平边宽，y 分量减垂直边宽）
        // 各向同性化后：min(r - left, r - top) = r - max(left, top)，再 clamp 到 0
        float ir_tl = max(0.0f, r_tl - max(p3, p0));  // 左上：left=p3, top=p0
        float ir_tr = max(0.0f, r_tr - max(p1, p0));  // 右上：right=p1, top=p0
        float ir_br = max(0.0f, r_br - max(p1, p2));  // 右下：right=p1, bottom=p2
        float ir_bl = max(0.0f, r_bl - max(p3, p2));  // 左下：left=p3, bottom=p2

        // 内轮廓
        float d_inner = complex_rounded_box_sdf(inner_p, inner_b, float4(ir_br, ir_tr, ir_bl, ir_tl));
        float fw_i = max(fwidth(d_inner), 0.5f);
        float a_inner = 1.0f - smoothstep(-fw_i, fw_i, d_inner);

        // 外轮廓内 且 内轮廓外 → 描边带
        float al = a_outer * (1.0f - a_inner);
        float4 c3 = i.color;
        return float4(c3.rgb * c3.a * al, c3.a * al);
    } else {
        // 椭圆（half_w = X 半径, half_h = Y 半径）
        d = ellipse_sdf(p, b);
    }

    // 基于 fwidth 的像素级抗锯齿过渡宽度
    float fw = max(fwidth(d), 0.5f);

    // 计算不透明度
    float alpha;
    if (stroke_w <= 0.0f) {
        // 填充模式：d < 0 为内部，smoothstep 在边界处平滑过渡
        alpha = 1.0f - smoothstep(-fw, fw, d);
    } else {
        // 描边模式（中心对齐）：描边覆盖 [-half_sw, half_sw] 区间
        float half_sw = stroke_w * 0.5f;
        // 外边缘 AA：d < half_sw 时显示
        float a_outer = 1.0f - smoothstep(-fw, fw, d - half_sw);
        // 内边缘 AA：d > -half_sw 时显示（往内消失）
        float a_inner = smoothstep(-fw, fw, d + half_sw);
        alpha = a_outer * a_inner;
    }

    // 预乘 Alpha 输出（匹配预乘混合模式 ONE / INV_SRC_ALPHA）
    float4 c = i.color;
    float  a = c.a * alpha;
    return float4(c.rgb * a, a);
}
)hlsl";

// ── 顶点结构体 ────────────────────────────────────────────────────────────────

/// 折线描边顶点：屏幕像素坐标 + 线性 RGBA 颜色（用于 StrokeLine 等折线命令）
struct PaintVertex {
    float x, y;        ///< 屏幕像素坐标（左上角为原点）
    float r, g, b, a;  ///< 线性 RGBA 颜色（[0,1]）
};

/**
 * @brief SDF 形状顶点（64 字节）。
 *
 * 用于矩形/圆角矩形/复杂圆角/椭圆的填充与描边。
 * 每个形状生成一个覆盖包围盒的矩形（6 顶点），
 * 像素着色器逐像素计算有向距离场（SDF）确定覆盖度。
 *
 * 内存布局（16 float）：
 *   [0..7]   POSITION (float2)  — 屏幕像素坐标
 *   [8..23]  COLOR    (float4)  — 线性 RGBA 颜色
 *   [24..31] TEXCOORD0 (float2) — 本地坐标（以形状中心为原点，像素单位）
 *   [32..47] TEXCOORD1 (float4) — (kind, half_w, half_h, p0)
 *   [48..63] TEXCOORD2 (float4) — (p1, p2, p3, stroke_w)
 *
 * kind 取值：0=矩形，1=均匀圆角矩形，2=四角独立圆角矩形，3=椭圆
 * p0..p3：四角圆角半径（各向同性），顺序 = (右下, 右上, 左下, 左上)
 * stroke_w：描边总宽度（中心对齐），0 = 填充模式
 */
struct SdfVertex {
    float x, y;           ///< 屏幕像素坐标（offset 0）
    float r, g, b, a;     ///< 线性 RGBA 颜色（offset 8）
    float lx, ly;         ///< 本地坐标，形状中心为原点（offset 24）
    float kind;           ///< 形状类型（offset 32）
    float half_w, half_h; ///< 形状半尺寸（像素，offset 36）
    float p0;             ///< 圆角半径参数 0（均匀圆角 / 右下角，offset 44）
    float p1, p2, p3;     ///< 圆角半径参数 1-3（右上/左下/左上，offset 48）
    float stroke_w;       ///< 描边总宽度（0=填充，offset 60）
    float e0, e1, e2, e3; ///< 扩展参数（offset 64）—— TEXCOORD3，供 kind=5 存放四角圆角半径
};

/// 视口常量缓冲布局（必须为 16 字节倍数，D3D11 硬性要求）
struct ViewportCB {
    float width;
    float height;
    float pad0{0.0f};
    float pad1{0.0f};
};

// ── 折线描边展开辅助函数（StrokeLine 使用）────────────────────────────────────

/**
 * @brief 将折线描边展开为顶点并写入列表（Miter join + Butt/Square cap）。
 *
 * 对折线每个顶点计算角平分线 (miter) 方向的偏移量，生成上下两侧对称的点，
 * 相邻两点之间生成一个矩形（两个三角形）。
 *
 * @param vertices  输出顶点列表（追加）
 * @param pts       折线点序列（屏幕像素坐标）
 * @param half_w    线宽一半（pen.width / 2）
 * @param r,g,b,a   描边颜色
 * @param closed    是否为闭合路径（最后一段连回起点）
 * @param miter_lim Miter 上限，超出时截断为 Bevel（对应 Pen::miter_limit）
 */
static void push_polyline_stroke_vertices(
    containers::Vector<PaintVertex>& vertices,
    const containers::Vector<math::Vec2>& pts,
    float half_w,
    float r, float g, float b, float a,
    bool closed,
    float miter_lim = 10.0f)
{
    const int32_t n = static_cast<int32_t>(pts.size());
    if (n < 2) return;

    // ── 计算各点的垂直偏移（单位法线，已乘以 half_w 后的 miter 长度）──

    // 辅助：计算 2D 向量长度
    auto vec_len = [](float dx, float dy) -> float {
        return std::sqrt(dx * dx + dy * dy);
    };
    // 辅助：归一化 2D 向量（零向量返回 {0,0}）
    auto normalize = [&](float dx, float dy, float& ox, float& oy) {
        const float len = vec_len(dx, dy);
        if (len < 1e-6f) { ox = 0.0f; oy = 0.0f; }
        else              { ox = dx / len; oy = dy / len; }
    };

    // 每个折线顶点的上下侧偏移点
    containers::Vector<math::Vec2> upper(static_cast<uint32_t>(n));
    containers::Vector<math::Vec2> lower(static_cast<uint32_t>(n));

    for (int32_t i = 0; i < n; ++i) {
        // 计算当前顶点的 miter 法线偏移量
        float nx = 0.0f, ny = 0.0f;   // 最终法线方向（已乘以 miter 长度）

        const bool is_first = !closed && (i == 0);
        const bool is_last  = !closed && (i == n - 1);

        if (is_first) {
            // 起点：用第一段方向的法线
            float tx, ty;
            normalize(pts[1].x - pts[0].x, pts[1].y - pts[0].y, tx, ty);
            nx = -ty; ny = tx;  // 左旋 90°
            nx *= half_w; ny *= half_w;
        } else if (is_last) {
            // 终点：用最后一段方向的法线
            float tx, ty;
            normalize(pts[n-1].x - pts[n-2].x, pts[n-1].y - pts[n-2].y, tx, ty);
            nx = -ty; ny = tx;
            nx *= half_w; ny *= half_w;
        } else {
            // 中间点（或闭合路径首/末点）：miter 角平分线
            const int32_t prev = (i - 1 + n) % n;
            const int32_t next = (i + 1) % n;

            // 前段方向向量（已归一化）
            float t0x, t0y;
            normalize(pts[i].x - pts[prev].x, pts[i].y - pts[prev].y, t0x, t0y);
            // 后段方向向量（已归一化）
            float t1x, t1y;
            normalize(pts[next].x - pts[i].x, pts[next].y - pts[i].y, t1x, t1y);

            // 前段法线（左旋 90°）
            const float n0x = -t0y, n0y = t0x;
            // 后段法线
            const float n1x = -t1y, n1y = t1x;

            // miter 法线 = 归一化(n0 + n1)
            float mx, my;
            normalize(n0x + n1x, n0y + n1y, mx, my);

            // miter 长度 = half_w / dot(miter, n0)
            // 当线段几乎平行时 dot ≈ 1，当折角很尖时 dot 趋近于 0（长度趋于无穷）
            const float dot = mx * n0x + my * n0y;
            float miter_len = (std::abs(dot) > 1e-4f) ? (half_w / dot) : half_w;

            // 超过 miter_limit 时，回退到 Bevel（截断为法线的半宽）
            if (miter_len > miter_lim * half_w) miter_len = half_w;

            nx = mx * miter_len;
            ny = my * miter_len;
        }

        upper[static_cast<uint32_t>(i)] = {pts[i].x + nx, pts[i].y + ny};
        lower[static_cast<uint32_t>(i)] = {pts[i].x - nx, pts[i].y - ny};
    }

    // ── 生成三角带（相邻两点之间一个矩形 = 2 个三角形）──────────────────

    const int32_t seg_count = closed ? n : n - 1;
    for (int32_t i = 0; i < seg_count; ++i) {
        const int32_t j = (i + 1) % n;
        const math::Vec2& u0 = upper[static_cast<uint32_t>(i)];
        const math::Vec2& u1 = upper[static_cast<uint32_t>(j)];
        const math::Vec2& l0 = lower[static_cast<uint32_t>(i)];
        const math::Vec2& l1 = lower[static_cast<uint32_t>(j)];

        // 三角形 1：u0, u1, l0
        vertices.push_back({u0.x, u0.y, r, g, b, a});
        vertices.push_back({u1.x, u1.y, r, g, b, a});
        vertices.push_back({l0.x, l0.y, r, g, b, a});

        // 三角形 2：u1, l1, l0
        vertices.push_back({u1.x, u1.y, r, g, b, a});
        vertices.push_back({l1.x, l1.y, r, g, b, a});
        vertices.push_back({l0.x, l0.y, r, g, b, a});
    }
}

// ── RhiRenderer 类 ────────────────────────────────────────────────────────────

/**
 * @brief 基于 RHI 的 2D 渲染器。
 *
 * 所有 GPU 调用都通过 mine.gfx.rhi 抽象层进行，不直接使用具体图形 API。
 *
 * 包含两条渲染管线：
 *   - solid_pipeline_：用于折线描边展开（StrokeLine），POSITION + COLOR 顶点
 *   - sdf_pipeline_  ：用于 SDF 形状（矩形/圆角/椭圆），SdfVertex 顶点
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
    /// 创建折线描边渲染管线（solid，POSITION + COLOR 顶点）
    bool create_solid_pipeline();

    /// 创建 SDF 形状渲染管线（SdfVertex，含 SDF 参数）
    bool create_sdf_pipeline();

    /**
     * @brief 生成 SDF 形状包围盒的 6 个顶点（2 个三角形），追加到 vertices。
     *
     * @param vertices        输出顶点列表
     * @param cx, cy          形状中心（屏幕像素坐标）
     * @param half_w, half_h  形状半尺寸（像素）
     * @param padding         包围盒额外 padding（描边外扩 + 1px AA 余量）
     * @param r,g,b,a         颜色（线性 RGBA）
     * @param kind            形状类型（0=矩形，1=均匀圆角，2=复杂圆角，3=椭圆）
     * @param p0,p1,p2,p3     圆角半径参数（右下/右上/左下/左上）
     * @param stroke_w        描边总宽度（0 = 填充模式）
     * @param e0,e1,e2,e3     扩展参数（TEXCOORD3，供 kind=5 传入四角圆角半径，默认 0）
     */
    static void push_sdf_quad_vertices(
        containers::Vector<SdfVertex>& vertices,
        float cx, float cy,
        float half_w, float half_h, float padding,
        float r, float g, float b, float a,
        float kind,
        float p0, float p1, float p2, float p3,
        float stroke_w,
        float e0 = 0.0f, float e1 = 0.0f, float e2 = 0.0f, float e3 = 0.0f);

    bool valid_{false};

    gfx::IDevice*                        device_{nullptr};       ///< 图形设备（不拥有）
    core::OwnedPtr<gfx::IQueue>          queue_;                 ///< 提交队列
    core::OwnedPtr<gfx::ICommandList>    cmd_list_;              ///< 命令录制列表
    core::OwnedPtr<gfx::IPipeline>       solid_pipeline_;        ///< 折线描边管线（POSITION+COLOR）
    core::OwnedPtr<gfx::IPipeline>       sdf_pipeline_;          ///< SDF 形状管线（SdfVertex）
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

    // 创建折线描边管线（StrokeLine）
    if (!create_solid_pipeline()) return;

    // 创建 SDF 形状管线（矩形/圆角/椭圆的填充与描边）
    if (!create_sdf_pipeline()) return;

    valid_ = true;
}

// ── 管线创建 ──────────────────────────────────────────────────────────────────

bool RhiRenderer::create_solid_pipeline() {
    // 顶点布局：POSITION（float2）+ COLOR（float4），共 24 字节
    gfx::PipelineDesc desc{};

    desc.vertex_shader.data        = k_vertex_shader_hlsl;
    desc.vertex_shader.size        = sizeof(k_vertex_shader_hlsl) - 1;
    desc.vertex_shader.language    = gfx::ShaderLanguage::HLSL;
    desc.vertex_shader.entry_point = "main";
    desc.vertex_shader.is_source   = true;

    desc.pixel_shader.data        = k_pixel_shader_hlsl;
    desc.pixel_shader.size        = sizeof(k_pixel_shader_hlsl) - 1;
    desc.pixel_shader.language    = gfx::ShaderLanguage::HLSL;
    desc.pixel_shader.entry_point = "main";
    desc.pixel_shader.is_source   = true;

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

    desc.vertex_shader.data        = k_sdf_vertex_shader_hlsl;
    desc.vertex_shader.size        = sizeof(k_sdf_vertex_shader_hlsl) - 1;
    desc.vertex_shader.language    = gfx::ShaderLanguage::HLSL;
    desc.vertex_shader.entry_point = "main";
    desc.vertex_shader.is_source   = true;

    desc.pixel_shader.data        = k_sdf_pixel_shader_hlsl;
    desc.pixel_shader.size        = sizeof(k_sdf_pixel_shader_hlsl) - 1;
    desc.pixel_shader.language    = gfx::ShaderLanguage::HLSL;
    desc.pixel_shader.entry_point = "main";
    desc.pixel_shader.is_source   = true;

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

// ── 渲染 ──────────────────────────────────────────────────────────────────────

void RhiRenderer::render(const DisplayList& dl, gfx::ITexture* target) {
    if (!valid_ || target == nullptr) return;

    const auto& cmds = dl.cmds();
    if (cmds.empty()) return;

    // ── 1. 创建视口常量缓冲（所有 DrawCall 共享，每帧创建一次）──────────

    const ViewportCB cb_data{
        static_cast<float>(target->width()),
        static_cast<float>(target->height()),
        0.0f, 0.0f
    };

    gfx::BufferDesc cb_desc{};
    cb_desc.size       = sizeof(ViewportCB);
    cb_desc.stride     = 0;
    cb_desc.bind_flags = gfx::BufferBindFlags::Constant;

    auto viewport_cb = device_->create_buffer(cb_desc, &cb_data);
    if (!viewport_cb) return;

    // ── 2. 设置全局渲染状态（渲染目标 + 视口）────────────────────────────

    cmd_list_->set_render_target(target, nullptr);

    gfx::Viewport viewport{};
    viewport.x         = 0.0f;
    viewport.y         = 0.0f;
    viewport.width     = static_cast<float>(target->width());
    viewport.height    = static_cast<float>(target->height());
    viewport.min_depth = 0.0f;
    viewport.max_depth = 1.0f;
    cmd_list_->set_viewport(viewport);

    // ── 3. 逐命令处理（每命令单独 DrawCall，保证绘制顺序）───────────────

    for (const DrawCmd& cmd : cmds) {

        // ── SDF 填充命令 ─────────────────────────────────────────────────

        if (cmd.kind == DrawCmdKind::FillRect) {
            if (cmd.brush.kind() != BrushKind::SolidColor) continue;
            const math::Color c = cmd.brush.color();
            const float cx = cmd.rect.x + cmd.rect.width  * 0.5f;
            const float cy = cmd.rect.y + cmd.rect.height * 0.5f;
            const float hw = cmd.rect.width  * 0.5f;
            const float hh = cmd.rect.height * 0.5f;

            containers::Vector<SdfVertex> verts;
            // 矩形（kind=0），填充（stroke_w=0），1px AA 余量
            push_sdf_quad_vertices(verts, cx, cy, hw, hh, 1.0f,
                c.r, c.g, c.b, c.a, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f);

            gfx::BufferDesc vb{};
            vb.size       = static_cast<uint64_t>(verts.size()) * sizeof(SdfVertex);
            vb.stride     = sizeof(SdfVertex);
            vb.bind_flags = gfx::BufferBindFlags::Vertex;
            auto buf = device_->create_buffer(vb, verts.data());
            if (!buf) continue;

            cmd_list_->set_pipeline(sdf_pipeline_.get());
            cmd_list_->set_constant_buffer(0, viewport_cb.get());
            cmd_list_->set_vertex_buffer(0, buf.get(), 0);
            cmd_list_->draw(static_cast<uint32_t>(verts.size()), 1, 0, 0);
        }
        else if (cmd.kind == DrawCmdKind::FillRoundedRect) {
            if (cmd.brush.kind() != BrushKind::SolidColor) continue;
            const math::Color c = cmd.brush.color();
            const float cx = cmd.rrect.rect.x + cmd.rrect.rect.width  * 0.5f;
            const float cy = cmd.rrect.rect.y + cmd.rrect.rect.height * 0.5f;
            const float hw = cmd.rrect.rect.width  * 0.5f;
            const float hh = cmd.rrect.rect.height * 0.5f;
            // 均匀圆角（各向同性：取 rx 和 ry 的最小值）
            const float rad = std::min(cmd.rrect.radius_x, cmd.rrect.radius_y);
            // 圆角半径不得超过半尺寸（CSS 规范钳制）
            const float r_clamped = std::min(rad, std::min(hw, hh));

            containers::Vector<SdfVertex> verts;
            // 均匀圆角矩形（kind=1），p0=统一圆角半径
            push_sdf_quad_vertices(verts, cx, cy, hw, hh, 1.0f,
                c.r, c.g, c.b, c.a, 1.0f, r_clamped, 0.0f, 0.0f, 0.0f, 0.0f);

            gfx::BufferDesc vb{};
            vb.size       = static_cast<uint64_t>(verts.size()) * sizeof(SdfVertex);
            vb.stride     = sizeof(SdfVertex);
            vb.bind_flags = gfx::BufferBindFlags::Vertex;
            auto buf = device_->create_buffer(vb, verts.data());
            if (!buf) continue;

            cmd_list_->set_pipeline(sdf_pipeline_.get());
            cmd_list_->set_constant_buffer(0, viewport_cb.get());
            cmd_list_->set_vertex_buffer(0, buf.get(), 0);
            cmd_list_->draw(static_cast<uint32_t>(verts.size()), 1, 0, 0);
        }
        else if (cmd.kind == DrawCmdKind::FillComplexRoundedRect) {
            if (cmd.brush.kind() != BrushKind::SolidColor) continue;
            const math::Color c = cmd.brush.color();
            const float cx = cmd.complex_rrect.rect.x + cmd.complex_rrect.rect.width  * 0.5f;
            const float cy = cmd.complex_rrect.rect.y + cmd.complex_rrect.rect.height * 0.5f;
            const float hw = cmd.complex_rrect.rect.width  * 0.5f;
            const float hh = cmd.complex_rrect.rect.height * 0.5f;
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

            gfx::BufferDesc vb{};
            vb.size       = static_cast<uint64_t>(verts.size()) * sizeof(SdfVertex);
            vb.stride     = sizeof(SdfVertex);
            vb.bind_flags = gfx::BufferBindFlags::Vertex;
            auto buf = device_->create_buffer(vb, verts.data());
            if (!buf) continue;

            cmd_list_->set_pipeline(sdf_pipeline_.get());
            cmd_list_->set_constant_buffer(0, viewport_cb.get());
            cmd_list_->set_vertex_buffer(0, buf.get(), 0);
            cmd_list_->draw(static_cast<uint32_t>(verts.size()), 1, 0, 0);
        }
        else if (cmd.kind == DrawCmdKind::FillEllipse) {
            if (cmd.brush.kind() != BrushKind::SolidColor) continue;
            if (cmd.pt_b.x <= 0.0f || cmd.pt_b.y <= 0.0f) continue;
            const math::Color c = cmd.brush.color();
            // pt_a = 中心，pt_b = (rx, ry) 半径
            const float cx = cmd.pt_a.x;
            const float cy = cmd.pt_a.y;
            const float hw = cmd.pt_b.x;
            const float hh = cmd.pt_b.y;

            containers::Vector<SdfVertex> verts;
            // 椭圆（kind=3），half_w=rx，half_h=ry
            push_sdf_quad_vertices(verts, cx, cy, hw, hh, 1.0f,
                c.r, c.g, c.b, c.a, 3.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f);

            gfx::BufferDesc vb{};
            vb.size       = static_cast<uint64_t>(verts.size()) * sizeof(SdfVertex);
            vb.stride     = sizeof(SdfVertex);
            vb.bind_flags = gfx::BufferBindFlags::Vertex;
            auto buf = device_->create_buffer(vb, verts.data());
            if (!buf) continue;

            cmd_list_->set_pipeline(sdf_pipeline_.get());
            cmd_list_->set_constant_buffer(0, viewport_cb.get());
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

            gfx::BufferDesc vb{};
            vb.size       = static_cast<uint64_t>(verts.size()) * sizeof(SdfVertex);
            vb.stride     = sizeof(SdfVertex);
            vb.bind_flags = gfx::BufferBindFlags::Vertex;
            auto buf = device_->create_buffer(vb, verts.data());
            if (!buf) continue;

            cmd_list_->set_pipeline(sdf_pipeline_.get());
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

            gfx::BufferDesc vb{};
            vb.size       = static_cast<uint64_t>(verts.size()) * sizeof(SdfVertex);
            vb.stride     = sizeof(SdfVertex);
            vb.bind_flags = gfx::BufferBindFlags::Vertex;
            auto buf = device_->create_buffer(vb, verts.data());
            if (!buf) continue;

            cmd_list_->set_pipeline(sdf_pipeline_.get());
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

            gfx::BufferDesc vb{};
            vb.size       = static_cast<uint64_t>(verts.size()) * sizeof(SdfVertex);
            vb.stride     = sizeof(SdfVertex);
            vb.bind_flags = gfx::BufferBindFlags::Vertex;
            auto buf2 = device_->create_buffer(vb, verts.data());
            if (!buf2) continue;

            cmd_list_->set_pipeline(sdf_pipeline_.get());
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

            gfx::BufferDesc vb{};
            vb.size       = static_cast<uint64_t>(verts.size()) * sizeof(SdfVertex);
            vb.stride     = sizeof(SdfVertex);
            vb.bind_flags = gfx::BufferBindFlags::Vertex;
            auto buf = device_->create_buffer(vb, verts.data());
            if (!buf) continue;

            cmd_list_->set_pipeline(sdf_pipeline_.get());
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

            gfx::BufferDesc vb{};
            vb.size       = static_cast<uint64_t>(verts.size()) * sizeof(SdfVertex);
            vb.stride     = sizeof(SdfVertex);
            vb.bind_flags = gfx::BufferBindFlags::Vertex;
            auto buf = device_->create_buffer(vb, verts.data());
            if (!buf) continue;

            cmd_list_->set_pipeline(sdf_pipeline_.get());
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

            gfx::BufferDesc vb{};
            vb.size       = static_cast<uint64_t>(verts.size()) * sizeof(SdfVertex);
            vb.stride     = sizeof(SdfVertex);
            vb.bind_flags = gfx::BufferBindFlags::Vertex;
            auto buf = device_->create_buffer(vb, verts.data());
            if (!buf) continue;

            cmd_list_->set_pipeline(sdf_pipeline_.get());
            cmd_list_->set_constant_buffer(0, viewport_cb.get());
            cmd_list_->set_vertex_buffer(0, buf.get(), 0);
            cmd_list_->draw(static_cast<uint32_t>(verts.size()), 1, 0, 0);
        }

        // ── 折线描边命令（保留折线展开方案）─────────────────────────────

        else if (cmd.kind == DrawCmdKind::StrokeLine) {
            if (cmd.brush.kind() != BrushKind::SolidColor) continue;
            if (cmd.pen.width <= 0.0f) continue;
            const math::Color c = cmd.brush.color();
            const float half_w  = cmd.pen.width * 0.5f;

            containers::Vector<math::Vec2> pts(2u);
            pts[0] = cmd.pt_a;
            pts[1] = cmd.pt_b;

            containers::Vector<PaintVertex> verts;
            push_polyline_stroke_vertices(
                verts, pts, half_w,
                c.r, c.g, c.b, c.a, /*closed=*/false, cmd.pen.miter_limit);

            if (verts.empty()) continue;

            gfx::BufferDesc vb{};
            vb.size       = static_cast<uint64_t>(verts.size()) * sizeof(PaintVertex);
            vb.stride     = sizeof(PaintVertex);
            vb.bind_flags = gfx::BufferBindFlags::Vertex;
            auto buf = device_->create_buffer(vb, verts.data());
            if (!buf) continue;

            cmd_list_->set_pipeline(solid_pipeline_.get());
            cmd_list_->set_constant_buffer(0, viewport_cb.get());
            cmd_list_->set_vertex_buffer(0, buf.get(), 0);
            cmd_list_->draw(static_cast<uint32_t>(verts.size()), 1, 0, 0);
        }
        // StrokePath、ClipPushRect 等高级命令留至后续里程碑实现
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
