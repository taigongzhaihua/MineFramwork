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
 *   - StrokeLine：线段 SDF 描边，天然抗锯齿，支持 Flat/Round/Square 端点样式（kind=6）
 *   - StrokeArc：圆弧 SDF 描边（IQ 旋转坐标系法），支持 Flat/Round cap（kind=7）
 *   - StrokeQuadBezier：二次贝塞尔曲线 SDF 描边（IQ 解析解），支持 Flat/Round cap（kind=8）
 *   - StrokeCubicBezier：三次贝塞尔曲线 SDF 描边（数値迭代：17步采样+Newton精化），支持 Flat/Round cap（kind=9）
 *
 * 渲染架构：
 *   - 所有形状（含线段）均走 SDF 管线，SDF 参数编入顶点，逐像素精确 AA
 *   - StrokePath 等折线命令留至后续里程碑，届时使用 CPU 展开三角带（solid pipeline）
 *
 * SDF 顶点格式（SdfVertex，80 字节）：
 *   pos(2) + color(4) + local(2) + params0(4) + params1(4) + extra(4) float
 * 视口变换：顶点着色器将屏幕像素坐标映射到 NDC（Y 轴翻转）
 *
 * SDF kind 常量：
 *   0=矩形, 1=均匀圆角矩形, 2=四角独立圆角矩形, 3=椭圆
 *   4=四边不等宽矩形内侧描边, 5=四边不等宽+四角独立圆角内侧描边
 *   6=线段描边（Flat/Round/Square 端点样式）
 *   7=圆弧描边（Flat/Round cap，IQ 旋转坐标系）
 *   8=二次贝塞尔曲线描边（Flat/Round cap，IQ 解析解）
 *   9=三次贝塞尔曲线描边（Flat/Round cap，17步采样+Newton数値迭代）
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
#include <mine/text/FontFace.h>
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
    float2 viewport_size;       // 逻辑像素尺寸（NDC 变换用）
    float2 phys_viewport_size;  // 物理像素尺寸（像素着色器使用）
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
/// 注：此着色器源码因 MSVC 单字符串常量不超过 16380 字节的限制，被拆分为多段相邻字符串常量。
static constexpr const char k_sdf_pixel_shader_hlsl[] =
// ── 段 1：常量缓冲声明、纹理绑定、渐变辅助函数 ────────────────────────────
R"hlsl(
// 视口常量缓冲（b0 槽，与顶点着色器共享）
cbuffer ViewportCB : register(b0) {
    float2 viewport_size;       // 逻辑像素尺寸（NDC 变换用）
    float2 phys_viewport_size;  // 物理像素尺寸（SV_Position → UV 用）
};

// 画刷数据常量缓冲（b2 槽，每个 DrawCall 更新一次）
// 总大小：128 字节（8 × float4，16 字节对齐）
cbuffer BrushDataCB : register(b2) {
    uint   brush_kind;       // 画刷类型：0=纯色，1=线性渐变，2=径向渐变，3=亚克力
    uint   stop_count;       // 渐变色标数量 [2..4]（brush_kind=1/2 时有效）
    float  _brush_pad0;
    float  _brush_pad1;
    //   linear:  xy=起点 local 坐标（像素，相对形状中心），zw=渐变方向向量 local
    //   radial:  xy=圆心 local 坐标（像素），z=外径（像素），w=内径（像素）
    //   acrylic: xyzw=色调颜色（rgba，线性空间）
    float4 grad_params;
    //   acrylic: x=色调混合比例 [0,1]，y=饱和度增益（1.0=原始）
    float4 grad_extra;
    float4 stop_colors[4];   // 渐变色标颜色（rgba，线性空间，最多 4 个）
    float4 stop_offsets;     // 色标偏移量（x=off0, y=off1, z=off2, w=off3）
};

// 亚克力模糊背景纹理（t0 槽，仅亚克力 DrawCall 绑定时有效）
Texture2D    acrylic_backdrop : register(t0);
SamplerState acrylic_sampler  : register(s0);  // 线性双线性采样器

// 根据归一化参数 t [0,1] 在色标数组中插值，返回 rgba 颜色
float4 sample_gradient(float t) {
    t = saturate(t);
    float o0 = stop_offsets.x;
    float o1 = stop_offsets.y;
    float o2 = stop_offsets.z;
    float o3 = stop_offsets.w;
    float t01 = saturate((t - o0) / max(o1 - o0, 1e-6f));
    float t12 = saturate((t - o1) / max(o2 - o1, 1e-6f));
    float t23 = saturate((t - o2) / max(o3 - o2, 1e-6f));
    float4 c01 = lerp(stop_colors[0], stop_colors[1], t01);
    float4 c12 = lerp(stop_colors[1], stop_colors[2], t12);
    float4 c23 = lerp(stop_colors[2], stop_colors[3], t23);
    float4 c = c01;
    if (stop_count >= 3u && t > o1) c = c12;
    if (stop_count >= 4u && t > o2) c = c23;
    return c;
}
)hlsl"
// ── 段 2：SDF 函数（外正内负，IQ 标准约定）─────────────────────────────────
R"hlsl(

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

// 椭圆精确 SDF（IQ 解析公式，通过三次方程求最近点，任意椭圆率均精确）
// 来源：https://iquilezles.org/articles/ellipsedist/
// 算法：将椭圆最近点问题转化为一元三次方程，分两个分支求解（d<0 用 acos，d>=0 用立方根）。
// 约定：外正内负（返回值正=外部，负=内部），与其余 SDF 保持一致。
float ellipse_sdf(float2 p, float2 ab) {
    // 将问题折叠到第一象限；确保 ab.y >= ab.x（长轴在 y 方向）
    // 若 a > b（x 轴为长轴），交换两轴，将问题等价变换为 b' > a' 的情况
    p = abs(p);
    if (ab.x > ab.y) { p = p.yx; ab = ab.yx; }
    // 圆形特殊处理（l = 0 → m/n 分母为零）
    float l = ab.y*ab.y - ab.x*ab.x;
    if (l < 1e-4f) return length(p) - ab.x;
    float m  = ab.x*p.x / l;   float m2 = m*m;
    float n  = ab.y*p.y / l;   float n2 = n*n;
    float c  = (m2 + n2 - 1.0f) / 3.0f;
    float c3 = c*c*c;
    float q  = c3 + m2*n2*2.0f;
    float d  = c3 + m2*n2;
    float g  = m + m*n2;

    float co;
    if (d < 0.0f) {
        // 三个实根分支：用 acos 公式
        // d<0 分支中 c<0（故 c3<0），直接除（无零值风险），再 clamp 至 [-1,1] 保证 acos 稳定
        float h  = acos(clamp(q / c3, -1.0f, 1.0f)) / 3.0f;
        float s  = cos(h);
        float t  = sin(h) * 1.7320508f;  // sqrt(3)
        float rx = sqrt(max(-c*(s + t + 2.0f) + m2, 0.0f));
        float ry = sqrt(max(-c*(s - t + 2.0f) + m2, 0.0f));
        co = (ry + sign(l)*rx + abs(g) / max(rx*ry, 1e-7f) - m) / 2.0f;
    } else {
        // 一个实根分支：用立方根公式
        float h  = 2.0f*m*n*sqrt(d);
        float s  = sign(q + h) * pow(abs(q + h), 1.0f/3.0f);
        float u  = sign(q - h) * pow(abs(q - h), 1.0f/3.0f);
        float rx = -s - u - c*4.0f + 2.0f*m2;
        float ry = (s - u) * 1.7320508f;
        float rm = sqrt(rx*rx + ry*ry);
        // ry/sqrt(rm-rx) = sign(ry)*sqrt(rm+rx)（代数等价，避免 rm≈rx 时的下溢）
        co = (sign(ry)*sqrt(max(rm + rx, 0.0f)) + 2.0f*g / max(rm, 1e-7f) - m) / 2.0f;
    }

    // co 为最近椭圆点的 x 归一化坐标，clamp 防止数值漂移超出 [0,1]
    co = clamp(co, 0.0f, 1.0f);
    float2 r = ab * float2(co, sqrt(max(1.0f - co*co, 0.0f)));
    // 用椭圆方程判断内外：(p/ab)² - 1 < 0 = 内部（负）；> 0 = 外部（正）
    // 比 sign(p.y-r.y) 更稳健，避免 p 在 x 轴时两者均为 0 导致 sign=0 的歧义
    float inside = dot(p / ab, p / ab) - 1.0f;
    return length(r - p) * sign(inside);
}

// ── 像素着色器主函数 ─────────────────────────────────────────────────────────

// ── 多边形顶点常量缓冲（b1 槽，仅多边形 DrawCall 绑定）────────────────────
// 内存布局（16 字节对齐）：
//   偏移   0: poly_vert_count（int）— 实际顶点数（3..64）
//   偏移   4: _poly_pad0/1/2（填充）
//   偏移  16: poly_verts[0..63]（float4，每顶点 xy=本地坐标，zw 填充为0）
cbuffer PolygonVertsCB : register(b1) {
    int   poly_vert_count;  // 多边形实际顶点数（3..64）
    float _poly_pad0;
    float _poly_pad1;
    float _poly_pad2;
    float4 poly_verts[64];  // 顶点本地坐标（x=local_x, y=local_y，zw 不使用）
};

// 多边形 SDF（IQ 绕数法，支持凸多边形和凹多边形，任意简单多边形均正确）
// 算法来源：https://iquilezles.org/articles/distfunctions2d/
// 参数 p：当前像素的本地坐标（以 AABB 中心为原点，单位像素）
// 返回值：外正内负的有向距离（像素单位）
float sdPolygon(float2 p) {
    int n = poly_vert_count;
    // 以第一条边到 p 的最近点距离平方作为初始最小距离
    float d = dot(p - poly_verts[0].xy, p - poly_verts[0].xy);
    float s = 1.0f;  // 符号（1=外部，-1=内部）
    [loop]
    for (int i = 0, j = n - 1; i < n; j = i, i++) {
        float2 vi = poly_verts[i].xy;
        float2 vj = poly_verts[j].xy;
        // 点 p 到边 (vi, vj) 的最近点投影，计算距离平方
        float2 e = vj - vi;
        float2 w = p - vi;
        float2 b = w - e * clamp(dot(w, e) / dot(e, e), 0.0f, 1.0f);
        d = min(d, dot(b, b));
        // 用射线法（绕数法变体）判断 p 是否在多边形内部，确定 SDF 符号
        // 三个条件同为 true 或同为 false 时翻转符号（等价于 all(cond) || all(!cond)）
        bool c0 = (p.y >= vi.y);
        bool c1 = (p.y <  vj.y);
        bool c2 = (e.x * w.y > e.y * w.x);
        if ((c0 && c1 && c2) || (!c0 && !c1 && !c2)) s = -s;
    }
    return s * sqrt(d);
}

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
    } else if (kind == 6) {
        // ── 线段 SDF 描边（天然抗锯齿，含独立起止端点样式）──────────────
        //
        // 参数映射：
        //   extra.xy = 端点 A 本地坐标（起点，以线段中心为原点）
        //   extra.zw = 端点 B 本地坐标（终点，以线段中心为原点）
        //   p0       = start_cap（0=Flat 截断, 1=Round 圆形, 2=Square 方形延伸）
        //   p1       = end_cap（同上）
        //   stroke_w = 线宽（总宽度，非半宽）
        //
        // 算法：将像素投影到线段方向和法线方向，
        //   按投影位置分三段（A 端/主体/B 端）分别计算 SDF，
        //   最终用 smoothstep AA 过渡。
        float2 a = i.extra.xy;             // 端点 A（本地坐标）
        float2 b = i.extra.zw;             // 端点 B（本地坐标）
        float half_sw = stroke_w * 0.5f;  // 线宽一半

        float2 ba = b - a;
        float seg_len = length(ba);
        // 线段单位方向向量（零长线段退化为水平方向）
        float2 dir = seg_len > 1e-6f ? ba / seg_len : float2(1.0f, 0.0f);
        // 法线单位向量（左旋 90°）
        float2 nor = float2(-dir.y, dir.x);

        float2 pa = p - a;                      // 像素相对于端点 A 的向量
        float proj_along = dot(pa, dir);        // 沿线段方向的投影（0=A点, seg_len=B点）
        float v = abs(dot(pa, nor));            // 垂直线段方向的距离（取绝对值）

        int scap = (int)(p0 + 0.5f);           // start_cap 类型
        int ecap = (int)(p1 + 0.5f);           // end_cap 类型

        float d6;
        if (proj_along < 0.0f) {
            // A 端 cap 区域（投影超出 A 点之外）
            if (scap == 1) {
                // Round：圆形端点，到端点 A 的欧氏距离 - 半线宽
                d6 = length(pa) - half_sw;
            } else if (scap == 2) {
                // Square：从 A 点向外延伸 half_sw 的矩形角
                d6 = length(float2(max(-proj_along - half_sw, 0.0f),
                                   max(v - half_sw, 0.0f)));
            } else {
                // Flat（默认）：在 A 端面精确截断，超出端面即外部
                d6 = length(float2(-proj_along, max(v - half_sw, 0.0f)));
            }
        } else if (proj_along > seg_len) {
            // B 端 cap 区域（投影超出 B 点之外）
            float2 pb = p - b;                  // 像素相对于端点 B 的向量
            float pb_over = proj_along - seg_len; // 超出 B 端的量（>0）
            if (ecap == 1) {
                // Round：圆形端点，到端点 B 的欧氏距离 - 半线宽
                d6 = length(pb) - half_sw;
            } else if (ecap == 2) {
                // Square：从 B 点向外延伸 half_sw 的矩形角
                d6 = length(float2(max(pb_over - half_sw, 0.0f),
                                   max(v - half_sw, 0.0f)));
            } else {
                // Flat（默认）：在 B 端面精确截断
                d6 = length(float2(pb_over, max(v - half_sw, 0.0f)));
            }
        } else {
            // 主体区域（投影在线段内 [0, seg_len]）：仅垂直方向距离
            d6 = v - half_sw;
        }

        float fw6 = max(fwidth(d6), 0.5f);
        float al6 = 1.0f - smoothstep(-fw6, fw6, d6);
        float4 cs = i.color;
        return float4(cs.rgb * cs.a * al6, cs.a * al6);
    }
)hlsl"
// ── 字符串拆分（MSVC 原始字符串长度限制），以下为 kind=7/8 续接 ──────────
R"hlsl(
    else if (kind == 7) {
        // ── 圆弧 SDF 描边（IQ 旋转坐标系法，支持 Flat/Round 端点）────────
        //
        // 参数映射：
        //   p0       = 圆弧半径 r
        //   p1       = 弧中心角（弧度，= start_angle + sweep * 0.5）
        //   p2       = 半张角（弧度，= |sweep| * 0.5）
        //   p3       = cap 样式（0=Flat 截断, 1=Round 圆形；两端使用相同 cap）
        //   stroke_w = 线宽（总宽度，非半宽）
        //   extra.xy = 圆心本地坐标（Quad 中心通常即为圆心，故通常为 (0,0)）
        //
        // 算法（IQ arc SDF）：
        //   1. 将 p 旋转到以弧中心角方向为 y 轴正方向的坐标系（q）
        //   2. 利用弧对称性，折叠 q.x → abs(q.x)（化为单侧端点判断）
        //   3. 条件 sc.y*qa.x > sc.x*qa.y 判断 qa 是否在端点扇形区域外
        //      是 → 端点 cap SDF；否 → 圆环 SDF（abs(|qa|-r) - half_sw）
        //   Flat cap：用端点处切线截断矩形截面 SDF（类比线段 Flat cap）
        //   Round cap：到端点的欧氏距离 - half_sw
        float arc_r   = p0;
        float mid     = p1;                    // 弧中心角（弧度）
        float hs      = p2;                    // 半张角（弧度，= |sweep| * 0.5）
        float half_sw7 = stroke_w * 0.5f;

        // 相对圆心的本地向量（圆心本地坐标 = extra.xy，通常为 (0,0)）
        float2 pc7 = p - i.extra.xy;

        // 旋转坐标系：令弧中心角方向 (cos(mid), sin(mid)) 对齐 y 轴正方向
        //   新 x 轴 = (-sin(mid), cos(mid))（左旋 90°）
        //   新 y 轴 = ( cos(mid), sin(mid))（弧中心角方向）
        float cm = cos(mid), sm = sin(mid);
        float2 q7;
        q7.x = -pc7.x * sm + pc7.y * cm;  // 垂直弧中心角方向
        q7.y =  pc7.x * cm + pc7.y * sm;  // 沿弧中心角方向

        // 端点方向向量（旋转坐标系中）：sc = (sin(hs), cos(hs))
        //   弧端点在 ±hs 处，旋转坐标系中为 (±sin(hs), cos(hs))
        float2 sc = float2(sin(hs), cos(hs));

        // 利用弧的左右对称性（关于 y 轴），折叠 q7.x → abs(q7.x)
        float2 qa = float2(abs(q7.x), q7.y);

        float d7;
        if (sc.y * qa.x > sc.x * qa.y) {
            // qa 在弧端点外侧扇形区域（角度超出 hs）→ 端点 cap 处理
            // 端点坐标（折叠坐标系）：Pend = sc * r = (sin(hs)*r, cos(hs)*r)
            float2 Pend = sc * arc_r;

            int cap7 = (int)(p3 + 0.5f);  // abs 折叠后两端对称，使用统一 cap
            if (cap7 == 1) {
                // Round cap：到端点的欧氏距离 - 半线宽
                d7 = length(qa - Pend) - half_sw7;
            } else {
                // Flat cap：在端点处沿切线方向截断（矩形截面 SDF）
                // 端点处切线方向（沿弧增大方向）：tc = (cos(hs), -sin(hs)) = (sc.y, -sc.x)
                // 沿切线超出端点的量（在端点外侧区域内，此值 > 0）
                float along7 = qa.x * sc.y - qa.y * sc.x;
                // 到圆弧的径向距离（在半线宽内为负，超出为正）
                float rdist7 = abs(length(qa) - arc_r) - half_sw7;
                // 类比线段 Flat cap：length(along, max(rdist, 0))
                d7 = length(float2(along7, max(rdist7, 0.0f)));
            }
        } else {
            // qa 在弧段范围内（角度 ≤ hs）→ 圆环 SDF
            d7 = abs(length(qa) - arc_r) - half_sw7;
        }

        float fw7 = max(fwidth(d7), 0.5f);
        float al7 = 1.0f - smoothstep(-fw7, fw7, d7);
        float4 cs7 = i.color;
        return float4(cs7.rgb * cs7.a * al7, cs7.a * al7);
    } else if (kind == 8) {
        // ── 二次贝塞尔曲线描边 SDF（IQ 解析法，支持 Flat/Round cap）────
        //
        // 参数映射：
        //   half_w, half_h = P2 本地坐标（终点）← 借用"半尺寸"槽
        //   p0        = start_cap（0=Flat 截断, 1=Round 圆形）
        //   p1        = end_cap（同上）
        //   stroke_w  = 线宽（总宽度）
        //   extra.xy  = P0 本地坐标（起点，t=0）
        //   extra.zw  = P1 本地坐标（控制点）
        //
        // 算法（IQ sdBezier，闭合解析解）：
        //   将"点到曲线最近点"问题转化为解三次方程，通过 Cardano 公式求解。
        //   clamp(t,0,1) 天然处理端点 → Round cap（端点圆形延伸）。
        //   Flat cap：额外用端点处切线半平面截断（CSG max 操作）。
        float2 Bz_A = i.extra.xy;              // P0（起点）
        float2 Bz_B = i.extra.zw;              // P1（控制点）
        float2 Bz_C = float2(half_w, half_h);  // P2（终点，借用 half_w/half_h 槽）
        float half_sw8 = stroke_w * 0.5f;

        // IQ sdBezier 核心：求 p 到二次贝塞尔曲线的最小距离
        float2 bz_a = Bz_B - Bz_A;                         // 一次项系数 / 2
        float2 bz_b = Bz_A - 2.0f * Bz_B + Bz_C;          // 二次项系数（曲率向量）
        float2 bz_c = bz_a * 2.0f;                         // 切线系数
        float2 bz_d = Bz_A - p;                             // 常数项

        // 三次方程系数（通过换元 u = t - kx 消去二次项）
        float kk = 1.0f / max(dot(bz_b, bz_b), 1e-10f);    // 防除零（曲线退化为直线）
        float kx = kk * dot(bz_a, bz_b);
        float ky = kk * (2.0f * dot(bz_a, bz_a) + dot(bz_d, bz_b)) / 3.0f;
        float kz = kk * dot(bz_d, bz_a);

        // 压缩三次方程为标准形式 u³ + p*u + q = 0
        float bz_p = ky - kx * kx;
        float bz_p3 = bz_p * bz_p * bz_p;
        float bz_q = kx * (2.0f * kx * kx - 3.0f * ky) + kz;
        float bz_h = bz_q * bz_q + 4.0f * bz_p3;          // 判别式

        float d8_sq;
        if (bz_h >= 0.0f) {
            // 判别式 ≥ 0：一个实根（另两个复根）
            float sq_h = sqrt(bz_h);
            float2 x_vec = (float2(sq_h, -sq_h) - bz_q) * 0.5f;
            // 实数立方根（保留符号）
            float2 uv = sign(x_vec) * pow(abs(x_vec), float2(1.0f / 3.0f, 1.0f / 3.0f));
            float t_cand = clamp(uv.x + uv.y - kx, 0.0f, 1.0f);
            float2 pt_cand = Bz_A + (bz_c + bz_b * t_cand) * t_cand;
            d8_sq = dot(p - pt_cand, p - pt_cand);
        } else {
            // 判别式 < 0：三个实根，逐一计算取最小距离
            float bz_z = sqrt(-bz_p);
            float bz_v = acos(clamp(bz_q / (bz_p * bz_z * 2.0f), -1.0f, 1.0f)) / 3.0f;
            float bz_m = cos(bz_v);
            float bz_n = sin(bz_v) * 1.7320508075688772f;  // √3
            float3 t3  = clamp(float3(bz_m + bz_m, -bz_n - bz_m, bz_n - bz_m) * bz_z - kx,
                               0.0f, 1.0f);
            float2 pt0 = Bz_A + (bz_c + bz_b * t3.x) * t3.x;
            float2 pt1 = Bz_A + (bz_c + bz_b * t3.y) * t3.y;
            float2 pt2 = Bz_A + (bz_c + bz_b * t3.z) * t3.z;
            float d0_sq = dot(p - pt0, p - pt0);
            float d1_sq = dot(p - pt1, p - pt1);
            float d2_sq = dot(p - pt2, p - pt2);
            d8_sq = min(min(d0_sq, d1_sq), d2_sq);
        }

        float d8 = sqrt(d8_sq) - half_sw8;

        // Flat cap 截断（CSG max：用端点切线半平面限制描边区域）
        //   P0 处切线方向（指向曲线内侧）：normalize(Bz_B - Bz_A)
        //   当 p 在 P0 外侧（与切线方向相反），dot(A - p, t0) > 0，截断生效
        int scap8 = (int)(p0 + 0.5f);
        int ecap8 = (int)(p1 + 0.5f);
        if (scap8 == 0) {
            // P0 Flat cap：切线 = Bz_B - Bz_A（归一化）
            float2 t0_len = Bz_B - Bz_A;
            float t0_l = length(t0_len);
            float2 t0  = t0_l > 1e-6f ? t0_len / t0_l : float2(1.0f, 0.0f);
            // dot(Bz_A - p, t0) > 0 表示 p 在 P0 外侧（切线背面），截断
            d8 = max(d8, dot(Bz_A - p, t0));
        }
        if (ecap8 == 0) {
            // P2 Flat cap：切线 = Bz_C - Bz_B（归一化）
            float2 t1_len = Bz_C - Bz_B;
            float t1_l = length(t1_len);
            float2 t1  = t1_l > 1e-6f ? t1_len / t1_l : float2(1.0f, 0.0f);
            // dot(p - Bz_C, t1) > 0 表示 p 在 P2 外侧（切线正面），截断
            d8 = max(d8, dot(p - Bz_C, t1));
        }

        float fw8 = max(fwidth(d8), 0.5f);
        float al8 = 1.0f - smoothstep(-fw8, fw8, d8);
        float4 cs8 = i.color;
        return float4(cs8.rgb * cs8.a * al8, cs8.a * al8);
    } else if (kind == 9) {
        // ── 三次贝塞尔曲线描边 SDF（四区间候选 + Newton 精化）──
        //
        // 参数映射：
        //   half_w, half_h = P3 本地坐标；p2, p3 = P2 本地坐标
        //   p0=start_cap, p1=end_cap, stroke_w=线宽
        //   extra.xy=P0, extra.zw=P1
        //
        // 三次贝塞尔点到曲线的距离函数最多有 3 个局部极小值。
        // 三段方案的中段 [1/3,2/3] 对于 S 形曲线可能包含两个极小值（拐点
        // 附近像素），两者都在中段内时候选 tb 只能覆盖其一，导致孤立亮斑。
        // 改为四段方案 [0,1/4]、[1/4,1/2]、[1/2,3/4]、[3/4,1]，将中间区域
        // 分成两段，保证两个极小值各自被不同候选覆盖。
        float2 Cb_A = i.extra.xy;               // P0（起点）
        float2 Cb_B = i.extra.zw;               // P1（第一控制点）
        float2 Cb_C = float2(p2, p3);           // P2（第二控制点）← 借用 p2/p3 槽
        float2 Cb_D = float2(half_w, half_h);   // P3（终点）← 借用 half_w/half_h 槽
        float half_sw9 = stroke_w * 0.5f;

        // 三次贝塞尔多项式系数（Horner 展开）
        float2 cb3 = -Cb_A + 3.0f * Cb_B - 3.0f * Cb_C + Cb_D;
        float2 cb2 =  3.0f * Cb_A - 6.0f * Cb_B + 3.0f * Cb_C;
        float2 cb1 = -3.0f * Cb_A + 3.0f * Cb_B;
        float2 cb0 =  Cb_A;

        // ── 阶段 1：四区间各 12 步采样，找四个独立候选 t ────────────────────
        float ta = 0.0f, tb = 0.375f, tc = 0.625f, td = 1.0f;
        float da_sq = 1e20f, db_sq = 1e20f, dc_sq = 1e20f, dd_sq = 1e20f;
        [unroll]
        for (int si = 0; si <= 48; si++) {
            float t = float(si) / 48.0f;
            float2 q = ((cb3 * t + cb2) * t + cb1) * t + cb0;
            float  dd = dot(p - q, p - q);
            if (t <= 0.25f) {
                if (dd < da_sq) { ta = t; da_sq = dd; }
            } else if (t <= 0.5f) {
                if (dd < db_sq) { tb = t; db_sq = dd; }
            } else if (t <= 0.75f) {
                if (dd < dc_sq) { tc = t; dc_sq = dd; }
            } else {
                if (dd < dd_sq) { td = t; dd_sq = dd; }
            }
        }

        // ── 阶段 2：四个候选各做 6 步 Newton-Raphson 精化 ──────────────────
        // f(t) = dot(Q(t)-p, Q'(t)) = 0，牛顿步：t -= f / f'
        [unroll]
        for (int ni = 0; ni < 6; ni++) {
            // 候选 a
            float2 qa   = ((cb3 * ta + cb2) * ta + cb1) * ta + cb0;
            float2 dqa  = (3.0f * cb3 * ta + 2.0f * cb2) * ta + cb1;
            float2 d2qa = 6.0f * cb3 * ta + 2.0f * cb2;
            float2 dfa  = qa - p;
            float  fa   = dot(dfa, dqa);
            float  fpa  = dot(dqa, dqa) + dot(dfa, d2qa);
            if (abs(fpa) > 1e-10f) ta -= fa / fpa;
            ta = clamp(ta, 0.0f, 1.0f);
            // 候选 b
            float2 qb   = ((cb3 * tb + cb2) * tb + cb1) * tb + cb0;
            float2 dqb  = (3.0f * cb3 * tb + 2.0f * cb2) * tb + cb1;
            float2 d2qb = 6.0f * cb3 * tb + 2.0f * cb2;
            float2 dfb  = qb - p;
            float  fb   = dot(dfb, dqb);
            float  fpb  = dot(dqb, dqb) + dot(dfb, d2qb);
            if (abs(fpb) > 1e-10f) tb -= fb / fpb;
            tb = clamp(tb, 0.0f, 1.0f);
            // 候选 c
            float2 qc   = ((cb3 * tc + cb2) * tc + cb1) * tc + cb0;
            float2 dqc  = (3.0f * cb3 * tc + 2.0f * cb2) * tc + cb1;
            float2 d2qc = 6.0f * cb3 * tc + 2.0f * cb2;
            float2 dfc  = qc - p;
            float  fc   = dot(dfc, dqc);
            float  fpc  = dot(dqc, dqc) + dot(dfc, d2qc);
            if (abs(fpc) > 1e-10f) tc -= fc / fpc;
            tc = clamp(tc, 0.0f, 1.0f);
            // 候选 d
            float2 qd   = ((cb3 * td + cb2) * td + cb1) * td + cb0;
            float2 dqd  = (3.0f * cb3 * td + 2.0f * cb2) * td + cb1;
            float2 d2qd = 6.0f * cb3 * td + 2.0f * cb2;
            float2 dfd  = qd - p;
            float  fd   = dot(dfd, dqd);
            float  fpd  = dot(dqd, dqd) + dot(dfd, d2qd);
            if (abs(fpd) > 1e-10f) td -= fd / fpd;
            td = clamp(td, 0.0f, 1.0f);
        }

        // ── 阶段 3：取四候选精化结果中距离最小者 ─────────────────────────
        float2 cla = ((cb3 * ta + cb2) * ta + cb1) * ta + cb0;
        float2 clb = ((cb3 * tb + cb2) * tb + cb1) * tb + cb0;
        float2 clc = ((cb3 * tc + cb2) * tc + cb1) * tc + cb0;
        float2 cld = ((cb3 * td + cb2) * td + cb1) * td + cb0;
        float d9 = min(min(length(p - cla), length(p - clb)),
                       min(length(p - clc), length(p - cld))) - half_sw9;

        // Flat cap 截断（CSG max：端点切线半平面截断）
        int scap9 = (int)(p0 + 0.5f);
        int ecap9 = (int)(p1 + 0.5f);
        if (scap9 == 0) {
            float2 t0_vec = Cb_B - Cb_A;
            float  t0_l   = length(t0_vec);
            float2 t0     = t0_l > 1e-6f ? t0_vec / t0_l : float2(1.0f, 0.0f);
            d9 = max(d9, dot(Cb_A - p, t0));
        }
        if (ecap9 == 0) {
            float2 t1_vec = Cb_D - Cb_C;
            float  t1_l   = length(t1_vec);
            float2 t1     = t1_l > 1e-6f ? t1_vec / t1_l : float2(1.0f, 0.0f);
            d9 = max(d9, dot(p - Cb_D, t1));
        }

        float fw9 = max(fwidth(d9), 0.5f);
        float al9 = 1.0f - smoothstep(-fw9, fw9, d9);
        float4 cs9 = i.color;
        return float4(cs9.rgb * cs9.a * al9, cs9.a * al9);
    }
)hlsl"
// ── 字符串拆分（MSVC 原始字符串长度限制），以下为 kind=10/11 多边形续接 ──
R"hlsl(
    else if (kind == 10 || kind == 11) {
        // ── kind=10：FillPolygon（填充多边形 SDF）─────────────────────────
        // ── kind=11：StrokePolygon（描边多边形 SDF）──────────────────────
        //
        // 顶点数据来自 PolygonVertsCB（b1 槽），坐标为本地坐标（以 AABB 中心为原点）。
        // sdPolygon() 使用 IQ 绕数法，同时支持凸多边形和凹多边形。
        float dpoly = sdPolygon(p);

        float fpoly = max(fwidth(dpoly), 0.5f);
        float apoly;
        if (kind == 10) {
            // 填充：多边形内部（d<0）不透明，边界处平滑过渡
            apoly = 1.0f - smoothstep(-fpoly, fpoly, dpoly);
        } else {
            // 描边：围绕多边形轮廓向内外各扩展 stroke_w/2
            float half_sw11 = stroke_w * 0.5f;
            float a_outer   = 1.0f - smoothstep(-fpoly, fpoly, dpoly - half_sw11);
            float a_inner   = smoothstep(-fpoly, fpoly, dpoly + half_sw11);
            apoly = a_outer * a_inner;
        }
        float4 cpoly = i.color;
        float  apre  = cpoly.a * apoly;
        return float4(cpoly.rgb * apre, apre);
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

    // ── 画刷颜色计算（根据 brush_kind 决定颜色来源）──────────────────────────
    float4 c;
    if (brush_kind == 1u) {
        // 线性渐变：t = 像素本地坐标在渐变轴上的投影（归一化）
        float2 grad_start = grad_params.xy;
        float2 grad_dir   = grad_params.zw;  // 终点 - 起点（local 坐标，像素）
        float  len_sq     = dot(grad_dir, grad_dir);
        float  t_lin      = (len_sq > 1e-8f)
            ? dot(p - grad_start, grad_dir) / len_sq
            : 0.0f;
        c = sample_gradient(t_lin);
    } else if (brush_kind == 2u) {
        // 径向渐变：t = (像素到圆心距离 - 内径) / (外径 - 内径)
        float2 center  = grad_params.xy;
        float  outer_r = grad_params.z;
        float  inner_r = grad_params.w;
        float  dist    = length(p - center);
        float  range   = outer_r - inner_r;
        float  t_rad   = (range > 1e-6f) ? (dist - inner_r) / range : 0.0f;
        c = sample_gradient(t_rad);
    } else if (brush_kind == 3u) {
        // 亚克力：采样模糊背景纹理，叠加饱和度调整和色调
        float2 backdrop_uv = i.pos.xy / phys_viewport_size;
        float4 backdrop    = acrylic_backdrop.Sample(acrylic_sampler, backdrop_uv);
        // 饱和度调整（基于亮度保留法）
        float  luma = dot(backdrop.rgb, float3(0.2126f, 0.7152f, 0.0722f));
        backdrop.rgb = lerp(float3(luma, luma, luma), backdrop.rgb, grad_extra.y);
        // 色调叠加（线性混合）
        float4 tint   = grad_params;               // rgba 色调颜色
        float  tint_a = grad_extra.x * tint.a;    // 实际混合比例
        c = float4(lerp(backdrop.rgb, tint.rgb, tint_a), 1.0f);
    } else {
        // 纯色（brush_kind == 0）：使用顶点插值颜色
        c = i.color;
    }

    // 预乘 Alpha 输出（匹配预乘混合模式 ONE / INV_SRC_ALPHA）
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
    float width;        ///< 逻辑宽度（physical/dpi_scale，用于 NDC 变换）
    float height;       ///< 逻辑高度
    float phys_width;   ///< 物理宽度（用于 SV_Position → UV 转换，亚克力采样用）
    float phys_height;  ///< 物理高度
};

/**
 * @brief 画刷数据常量缓冲（b2 槽，每 DrawCall 更新一次）。
 *
 * 总大小：128 字节（8 × float4，16 字节对齐）
 * 与 HLSL BrushDataCB 内存布局完全一致。
 */
struct alignas(16) BrushDataCB {
    uint32_t brush_kind;         ///< 画刷类型：0=纯色，1=线性渐变，2=径向渐变，3=亚克力
    uint32_t stop_count;         ///< 渐变色标数量 [2..4]
    float    _pad0{0.0f};
    float    _pad1{0.0f};
    // grad_params（16 字节）
    float    grad_params[4];     ///< 渐变参数（见 HLSL BrushDataCB 注释）
    // grad_extra（16 字节）
    float    grad_extra[4];      ///< 附加参数（亚克力：x=色调混合比例，y=饱和度增益）
    // stop_colors[4]（64 字节）
    float    stop_colors[4][4];  ///< 色标颜色（rgba，线性空间）
    // stop_offsets（16 字节）
    float    stop_offsets[4];    ///< 色标偏移量（x=off0, y=off1, z=off2, w=off3）
    // 总大小：4+4+4+4+16+16+64+16 = 128 字节 ✓
};
static_assert(sizeof(BrushDataCB) == 128, "BrushDataCB 大小必须为 128 字节");

// ── 文字渲染 HLSL 着色器 ──────────────────────────────────────────────────────

/// 文字顶点着色器：屏幕像素坐标 → NDC，透传 UV 和颜色
static constexpr const char k_text_vertex_shader_hlsl[] = R"hlsl(
cbuffer ViewportCB : register(b0) {
    float2 viewport_size;
    float2 _padding;
};

struct TextVSIn {
    float2 pos   : POSITION;   // 屏幕像素坐标
    float2 uv    : TEXCOORD0;  // 字形图集 UV（归一化 [0,1]）
    float4 color : COLOR;      // 线性 RGBA 颜色（预乘 alpha）
};

struct TextVSOut {
    float4 pos   : SV_Position;
    float2 uv    : TEXCOORD0;
    float4 color : COLOR;
};

TextVSOut main(TextVSIn i) {
    TextVSOut o;
    // 屏幕像素坐标 → NDC（Y 轴翻转）
    o.pos.x =  (i.pos.x / viewport_size.x) * 2.0f - 1.0f;
    o.pos.y = -(i.pos.y / viewport_size.y) * 2.0f + 1.0f;
    o.pos.z =  0.0f;
    o.pos.w =  1.0f;
    o.uv    =  i.uv;
    o.color =  i.color;
    return o;
}
)hlsl";

/// 文字像素着色器：采样 R8 字形图集，输出预乘 Alpha 颜色
static constexpr const char k_text_pixel_shader_hlsl[] = R"hlsl(
Texture2D    glyph_atlas   : register(t0);  // R8 字形灰度图集
SamplerState glyph_sampler : register(s0);  // 线性双线性采样器（CLAMP）

struct TextPSIn {
    float4 pos   : SV_Position;
    float2 uv    : TEXCOORD0;
    float4 color : COLOR;
};

float4 main(TextPSIn i) : SV_Target {
    // 采样字形灰度值（R 通道即 alpha 遮罩）
    float alpha = glyph_atlas.Sample(glyph_sampler, i.uv).r;
    // 预乘 Alpha 输出（匹配 ONE / INV_SRC_ALPHA 混合模式）
    return float4(i.color.rgb * alpha, i.color.a * alpha);
}
)hlsl";

// ── 文字顶点格式 ──────────────────────────────────────────────────────────────

/**
 * @brief 文字渲染顶点（32 字节）。
 *
 * 内存布局：
 *   [0..7]   POSITION (float2) — 屏幕像素坐标（左上角原点）
 *   [8..15]  TEXCOORD0 (float2) — 字形图集 UV（归一化 [0,1]）
 *   [16..31] COLOR (float4)    — 线性 RGBA 颜色（已由 Canvas 写入）
 */
struct TextVertex {
    float x, y;        ///< 屏幕像素坐标
    float u, v;        ///< 字形图集 UV（归一化 [0,1]）
    float r, g, b, a;  ///< 线性 RGBA 颜色
};

// ── 高斯模糊着色器（亚克力背景模糊用）────────────────────────────────────────

/// 模糊通道顶点着色器：直接透传 NDC 坐标，无需视口转换
static constexpr const char k_blur_vertex_shader_hlsl[] = R"hlsl(
struct BlurVSIn {
    float2 pos : POSITION;   // NDC 坐标（[-1,1]×[-1,1]）
    float2 uv  : TEXCOORD0;  // 源纹理 UV（[0,1]×[0,1]）
};

struct BlurVSOut {
    float4 pos : SV_Position;
    float2 uv  : TEXCOORD0;
};

BlurVSOut main(BlurVSIn i) {
    BlurVSOut o;
    o.pos = float4(i.pos, 0.0f, 1.0f);  // 直接传入 NDC，无需视口变换
    o.uv  = i.uv;
    return o;
}
)hlsl";

/// 高斯模糊像素着色器（分离式，方向由 texel_step 常量缓冲控制）
/// 使用 9-tap 高斯核（σ ≈ 2.5），权重归一化 = 1.0
/// 采样间距 = texel_step × [-4, -3, ..., 3, 4]（方向由 xy 分量决定）
static constexpr const char k_blur_pixel_shader_hlsl[] = R"hlsl(
// 模糊步进常量缓冲（b1 槽）
cbuffer BlurCB : register(b1) {
    float2 texel_step;   // 采样步进：(step/tex_w, 0) 水平 或 (0, step/tex_h) 垂直
    float  _blur_pad0;
    float  _blur_pad1;
};

Texture2D    blur_src     : register(t0);
SamplerState blur_sampler : register(s0);

// 9-tap 高斯核权重（σ ≈ 2.5，总和 = 1.0）
static const float k_gauss_weights[9] = {
    0.0162162162f, 0.0540540541f, 0.1216216216f,
    0.1945945946f, 0.2270270270f, 0.1945945946f,
    0.1216216216f, 0.0540540541f, 0.0162162162f
};

struct BlurPSIn {
    float4 pos : SV_Position;
    float2 uv  : TEXCOORD0;
};

float4 main(BlurPSIn i) : SV_Target {
    float4 color = float4(0.0f, 0.0f, 0.0f, 0.0f);
    [unroll]
    for (int k = -4; k <= 4; ++k) {
        float2 sample_uv = i.uv + texel_step * float(k);
        color += blur_src.Sample(blur_sampler, sample_uv) * k_gauss_weights[k + 4];
    }
    return color;
}
)hlsl";

// ── 模糊通道顶点格式 ──────────────────────────────────────────────────────────

/**
 * @brief 模糊通道顶点（16 字节）。
 *
 * 用于全屏四边形的 H/V 高斯模糊通道。
 * pos 使用 NDC 坐标（直接传入，顶点着色器不做视口变换）。
 */
struct BlurVertex {
    float x, y;  ///< NDC 坐标（[-1,1]）
    float u, v;  ///< 源纹理 UV（[0,1]）
};
/// 模糊常量缓冲（b1 槽，每个模糊通道更新一次）
struct alignas(16) BlurCB {
    float texel_step_x;  ///< 采样步进 X（水平通道 = blur_step/tex_w，垂直通道 = 0）
    float texel_step_y;  ///< 采样步进 Y（水平通道 = 0，垂直通道 = blur_step/tex_h）
    float _pad0{0.0f};
    float _pad1{0.0f};
};

/// 多边形顶点常量缓冲（b1 槽，仅多边形 DrawCall 绑定）
/// 内存布局与 HLSL PolygonVertsCB 完全一致（16 字节对齐）：
///   偏移   0: vert_count（int）
///   偏移   4: pad[3]（3 × float 填充）
///   偏移  16: verts[64][4]（每顶点 float4，xy=本地坐标，zw=0）
/// 总大小：16 + 64×16 = 1040 字节（65 × 16，符合 D3D11 16 字节倍数要求）
struct alignas(16) PolygonVertsCB {
    int   vert_count;  ///< 多边形顶点数（3..64）
    float pad[3];      ///< 填充至 16 字节
    float verts[64][4]; ///< 各顶点本地坐标（[k][0]=local_x, [k][1]=local_y, [k][2..3]=0）
};

// ── 裁剪写入像素着色器（SDF clip() 内置函数方案）─────────────────────────────
//
// 与普通 SDF PS 使用相同的顶点格式和顶点着色器（k_sdf_vertex_shader_hlsl）。
// 与普通 SDF PS 的差异：
//   1. 计算 SDF 距离 d
//   2. 调用 clip(-d) 丢弃形状外部像素（SDF 外正内负：d > 0 时 -d < 0 → clip 丢弃）
//   3. 不输出颜色（被 ClipWrite/ClipErase 管线的 RenderTargetWriteMask=0 屏蔽）
//
// 支持的 kind 值：
//   0 = 矩形，1 = 均匀圆角矩形，2 = 四角独立圆角矩形，10 = 多边形
//   其他 kind 退化为矩形处理
static constexpr const char k_sdf_clip_pixel_shader_hlsl[] = R"hlsl(
// 多边形顶点常量缓冲（b1 槽，仅多边形裁剪时绑定）
cbuffer PolygonVertsCB : register(b1) {
    int   poly_vert_count;
    float _poly_pad0;
    float _poly_pad1;
    float _poly_pad2;
    float4 poly_verts[64];
};

// 矩形 SDF（外正内负）
float clip_box_sdf(float2 p, float2 b) {
    float2 q = abs(p) - b;
    return length(max(q, 0.0f)) + min(max(q.x, q.y), 0.0f);
}

// 均匀圆角矩形 SDF（r = 统一圆角半径）
float clip_rounded_box_sdf(float2 p, float2 b, float r) {
    float2 q = abs(p) - b + r;
    return length(max(q, 0.0f)) + min(max(q.x, q.y), 0.0f) - r;
}

// 四角独立圆角矩形 SDF（r = (右下, 右上, 左下, 左上)）
float clip_complex_rounded_box_sdf(float2 p, float2 b, float4 r) {
    r.xy = (p.x > 0.0f) ? r.xy : r.zw;
    r.x  = (p.y > 0.0f) ? r.x  : r.y;
    float2 q = abs(p) - b + r.x;
    return length(max(q, 0.0f)) + min(max(q.x, q.y), 0.0f) - r.x;
}

// 多边形 SDF（IQ 绕数法，支持凸多边形和凹多边形）
float clip_sdPolygon(float2 p) {
    int n = poly_vert_count;
    float d = dot(p - poly_verts[0].xy, p - poly_verts[0].xy);
    float s = 1.0f;
    [loop]
    for (int i = 0, j = n - 1; i < n; j = i, i++) {
        float2 vi = poly_verts[i].xy;
        float2 vj = poly_verts[j].xy;
        float2 e  = vj - vi;
        float2 w  = p - vi;
        float2 b2 = w - e * clamp(dot(w, e) / dot(e, e), 0.0f, 1.0f);
        d = min(d, dot(b2, b2));
        bool c0 = (p.y >= vi.y);
        bool c1 = (p.y <  vj.y);
        bool c2 = (e.x * w.y > e.y * w.x);
        if ((c0 && c1 && c2) || (!c0 && !c1 && !c2)) s = -s;
    }
    return s * sqrt(d);
}

struct SdfClipPSIn {
    float4 pos     : SV_Position;
    float4 color   : COLOR;
    float2 local   : TEXCOORD0;
    float4 params0 : TEXCOORD1;  // (kind, half_w, half_h, p0)
    float4 params1 : TEXCOORD2;  // (p1, p2, p3, stroke_w)
    float4 extra   : TEXCOORD3;
};

float4 main(SdfClipPSIn i) : SV_Target {
    int    kind   = (int)(i.params0.x + 0.5f);
    float  half_w = i.params0.y;
    float  half_h = i.params0.z;
    float  p0     = i.params0.w;
    float  p1     = i.params1.x;
    float  p2     = i.params1.y;
    float  p3     = i.params1.z;
    float2 p      = i.local;
    float2 b      = float2(half_w, half_h);

    float d;
    if (kind == 1) {
        d = clip_rounded_box_sdf(p, b, p0);
    } else if (kind == 2) {
        d = clip_complex_rounded_box_sdf(p, b, float4(p0, p1, p2, p3));
    } else if (kind == 10) {
        d = clip_sdPolygon(p);
    } else {
        // 默认：矩形（kind == 0 或其他未支持类型）
        d = clip_box_sdf(p, b);
    }

    // 丢弃形状外部像素（d > 0 = 外部，-d < 0 → clip 丢弃）
    clip(-d);

    // 颜色输出被 RenderTargetWriteMask=0 屏蔽，返回值无实际影响
    return float4(0.0f, 0.0f, 0.0f, 0.0f);
}
)hlsl";

// ── 折线描边展开辅助函数（StrokePath 备用，当前未调用）───────────────────────

/**
 * @brief 将折线描边展开为顶点并写入列表（Miter join + Butt/Square cap）。
 *
 * 对折线每个顶点计算角平分线 (miter) 方向的偏移量，生成上下两侧对称的点，
 * 相邻两点之间生成一个矩形（两个三角形）。
 *
 * 注：StrokeLine 单线段命令已改为 SDF 方案（kind=6）。
 *     本函数为后续 StrokePath（多段折线）预留，暂未被调用。
 *
 * @param vertices  输出顶点列表（追加）
 * @param pts       折线点序列（屏幕像素坐标）
 * @param half_w    线宽一半（pen.width / 2）
 * @param r,g,b,a   描边颜色
 * @param closed    是否为闭合路径（最后一段连回起点）
 * @param miter_lim Miter 上限，超出时截断为 Bevel（对应 Pen::miter_limit）
 */
// NOLINTNEXTLINE(clang-diagnostic-unused-function)
#pragma warning(push)
#pragma warning(disable: 4505)  // 暂未调用，为 StrokePath 预留
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
#pragma warning(pop)

// ── RhiRenderer 类 ────────────────────────────────────────────────────────────

/**
 * @brief 字形 Atlas 打包器（Shelf Packer 算法）。
 *
 * 从图集左上角开始，按行（shelf）分配空间。当前行放不下时换行。
 */
struct ShelfPacker {
    uint32_t atlas_w{0};   ///< 图集宽度（像素）
    uint32_t atlas_h{0};   ///< 图集高度（像素）
    uint32_t cur_x{0};     ///< 当前行已用宽度
    uint32_t cur_y{0};     ///< 当前行顶部 Y 坐标
    uint32_t shelf_h{0};   ///< 当前行已分配的最大高度

    /**
     * @brief 尝试在图集中分配一块 w×h 的区域。
     * @return true=分配成功，out_x/out_y 为左上角坐标；false=图集已满。
     */
    bool pack(uint32_t w, uint32_t h, uint32_t& out_x, uint32_t& out_y) {
        // 当前行容纳不下时，换到新行
        if (cur_x + w > atlas_w) {
            cur_x  = 0;
            cur_y += shelf_h;
            shelf_h = 0;
        }
        if (cur_y + h > atlas_h) {
            return false;  // 图集已满
        }
        out_x    = cur_x;
        out_y    = cur_y;
        cur_x   += w;
        if (h > shelf_h) shelf_h = h;
        return true;
    }
};

/**
 * @brief 字形图集条目（缓存键 + 图集位置 + 度量）。
 */
struct GlyphKey {
    void*    face;       ///< FontFace* 指针（用于区分不同字体）
    uint32_t codepoint;  ///< Unicode 码点
    uint32_t size_px;    ///< 字号（整像素）
};

struct AtlasEntry {
    GlyphKey key;         ///< 缓存键
    uint16_t atlas_x;     ///< 字形在图集中的 X 坐标（像素）
    uint16_t atlas_y;     ///< 字形在图集中的 Y 坐标（像素）
    uint16_t atlas_w;     ///< 字形宽度（像素，0 表示空白字形）
    uint16_t atlas_h;     ///< 字形高度（像素）
    int16_t  bearing_x;   ///< 左边距（像素，相对基线笔触点）
    int16_t  bearing_y;   ///< 顶边距（像素，基线上方为正）
    int16_t  advance_x;   ///< 水平步进（像素）
};

/**
 * @brief 字形 GPU 图集管理器。
 *
 * 职责：
 *   - 维护一块 1024×1024 R8_UNorm CPU 位图（所有字形灰度数据）
 *   - 维护相应的 GPU 纹理（懒创建，首次 flush 时创建）
 *   - 字形缓存（线性查找，最多 kMaxGlyphs 条目）
 *   - Shelf Packer 分配图集区域
 *   - 脏标记机制：有新字形加入时标记 dirty，flush() 时上传到 GPU
 *
 * 注意：
 *   - cpu_data_ 为 1MB 堆内存（GlyphAtlas 通过 MINE_NEW 分配）
 *   - 非线程安全，在单一渲染线程中使用
 */
class GlyphAtlas {
public:
    static constexpr uint32_t kAtlasSize = 1024;   ///< 图集边长（像素）
    static constexpr uint32_t kMaxGlyphs = 2048;   ///< 最大字形缓存条目数
    static constexpr uint32_t kGlyphPad  = 1;      ///< 字形间距（防采样越界）

    GlyphAtlas() {
        packer_.atlas_w = kAtlasSize;
        packer_.atlas_h = kAtlasSize;
        // cpu_data_ 已在构造时零初始化（全黑透明图集）
    }

    ~GlyphAtlas() = default;

    // 禁止拷贝
    GlyphAtlas(const GlyphAtlas&)            = delete;
    GlyphAtlas& operator=(const GlyphAtlas&) = delete;

    /**
     * @brief 查找或插入一个字形到图集。
     *
     * 若字形已在缓存中，直接返回；否则调用 FreeType 光栅化并写入图集。
     *
     * @param face      FontFace 对象（用于光栅化）
     * @param codepoint Unicode 码点
     * @param size_px   字号（整像素）
     * @return 字形条目指针；若图集已满或光栅化失败则返回 nullptr。
     */
    const AtlasEntry* get_or_insert(text::FontFace* face, uint32_t codepoint, uint32_t size_px);

    /**
     * @brief 将图集数据上传到 GPU 纹理（若 dirty）。
     *
     * 首次调用时创建 GPU 纹理。M0 阶段全量上传（1024×1024 = 1MB R8 数据）。
     *
     * @param device 图形设备（用于创建纹理和上传数据）
     */
    void flush(gfx::IDevice* device);

    /// 获取 GPU 纹理（flush() 之后有效；首次 flush 前为 nullptr）
    [[nodiscard]] gfx::ITexture* texture() const noexcept { return gpu_texture_.get(); }

private:
    uint8_t     cpu_data_[kAtlasSize * kAtlasSize]{};  ///< R8 灰度图集（CPU 端，全零初始化）
    ShelfPacker packer_{};                              ///< Shelf 打包器
    bool        dirty_{false};                         ///< 是否有新字形未上传 GPU

    core::OwnedPtr<gfx::ITexture> gpu_texture_;        ///< GPU 纹理（R8_UNorm，ShaderResource）

    AtlasEntry  entries_[kMaxGlyphs]{};                ///< 字形缓存（线性数组）
    uint32_t    entry_count_{0};                       ///< 已缓存字形数量

    /// 在已缓存条目中查找（线性扫描）
    const AtlasEntry* find(void* face, uint32_t codepoint, uint32_t size_px) const noexcept {
        for (uint32_t i = 0; i < entry_count_; ++i) {
            const AtlasEntry& e = entries_[i];
            if (e.key.face == face &&
                e.key.codepoint == codepoint &&
                e.key.size_px == size_px) {
                return &e;
            }
        }
        return nullptr;
    }
};

// ── GlyphAtlas 方法实现 ───────────────────────────────────────────────────────

const AtlasEntry* GlyphAtlas::get_or_insert(
    text::FontFace* face, uint32_t codepoint, uint32_t size_px) {

    if (face == nullptr) return nullptr;

    // 1. 缓存命中直接返回
    const AtlasEntry* cached = find(face, codepoint, size_px);
    if (cached != nullptr) return cached;

    // 2. 缓存已满
    if (entry_count_ >= kMaxGlyphs) return nullptr;

    // 3. 设置字号并光栅化
    if (!face->set_pixel_size(0, size_px)) return nullptr;

    text::GlyphBitmap bitmap{};
    if (!face->rasterize(codepoint, bitmap)) return nullptr;

    // 4. 构造新条目
    AtlasEntry& entry = entries_[entry_count_];
    entry.key.face      = face;
    entry.key.codepoint = codepoint;
    entry.key.size_px   = size_px;
    entry.bearing_x     = static_cast<int16_t>(bitmap.metrics.bearing_x);
    entry.bearing_y     = static_cast<int16_t>(bitmap.metrics.bearing_y);
    entry.advance_x     = static_cast<int16_t>(bitmap.metrics.advance_x);
    entry.atlas_w       = static_cast<uint16_t>(bitmap.metrics.width);
    entry.atlas_h       = static_cast<uint16_t>(bitmap.metrics.height);

    // 5. 空白字形（空格等）：仅记录步进，不占图集空间
    if (bitmap.metrics.width == 0 || bitmap.metrics.height == 0) {
        entry.atlas_x = 0;
        entry.atlas_y = 0;
        entry.atlas_w = 0;
        entry.atlas_h = 0;
        ++entry_count_;
        return &entries_[entry_count_ - 1];
    }

    // 6. 在图集中分配区域（含 1px 边距防采样越界）
    uint32_t ax = 0, ay = 0;
    const uint32_t alloc_w = entry.atlas_w + kGlyphPad;
    const uint32_t alloc_h = entry.atlas_h + kGlyphPad;
    if (!packer_.pack(alloc_w, alloc_h, ax, ay)) {
        // 图集空间不足，返回失败（M0 不做动态扩容）
        return nullptr;
    }
    entry.atlas_x = static_cast<uint16_t>(ax);
    entry.atlas_y = static_cast<uint16_t>(ay);

    // 7. 将字形位图逐行写入 CPU 图集
    if (bitmap.data != nullptr) {
        for (uint32_t row = 0; row < entry.atlas_h; ++row) {
            const uint8_t* src_row = bitmap.data + row * bitmap.pitch;
            uint8_t*       dst_row = cpu_data_ + (ay + row) * kAtlasSize + ax;
            for (uint32_t col = 0; col < entry.atlas_w; ++col) {
                dst_row[col] = src_row[col];
            }
        }
    }

    dirty_ = true;
    ++entry_count_;
    return &entries_[entry_count_ - 1];
}

void GlyphAtlas::flush(gfx::IDevice* device) {
    if (device == nullptr) return;

    // 首次 flush：创建 GPU 纹理（R8_UNorm，ShaderResource 绑定）
    if (!gpu_texture_) {
        gfx::TextureDesc tex_desc{};
        tex_desc.width      = kAtlasSize;
        tex_desc.height     = kAtlasSize;
        tex_desc.format     = gfx::PixelFormat::R8_UNorm;
        tex_desc.bind_flags = gfx::TextureBindFlags::ShaderResource;
        tex_desc.mip_levels = 1;
        tex_desc.array_size = 1;

        gpu_texture_ = device->create_texture(tex_desc);
        if (!gpu_texture_) return;

        // 创建时上传一次全量数据（全零初始化图集）
        dirty_ = true;
    }

    // 有新字形加入：全量上传 CPU 图集到 GPU（M0 简化策略）
    if (dirty_) {
        device->update_texture_region(
            gpu_texture_.get(),
            0, 0,              // 目标区域左上角
            kAtlasSize,        // 宽度
            kAtlasSize,        // 高度
            cpu_data_,         // CPU 数据指针
            kAtlasSize);       // 每行字节数（R8，每像素 1 字节，故 = 宽度）
        dirty_ = false;
    }
}

/**
 * @brief 基于 RHI 的 2D 渲染器。
 *
 * 所有 GPU 调用都通过 mine.gfx.rhi 抽象层进行，不直接使用具体图形 API。
 *
 * 包含的渲染管线：
 *   - solid_pipeline_：用于折线描边展开（StrokePath），POSITION + COLOR 顶点
 *   - sdf_pipeline_  ：用于 SDF 形状（矩形/圆角/椭圆），SdfVertex 顶点（含 BrushDataCB 渐变支持）
 *   - text_pipeline_ ：用于文字渲染（字形图集四边形），TextVertex 顶点
 *   - blur_pipeline_ ：用于亚克力背景高斯模糊（全屏四边形，BlurVertex），方向由 BlurCB 控制
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
    void set_dpi_scale(float scale) override;

    /// 判断渲染器是否初始化成功
    [[nodiscard]] bool is_valid() const noexcept { return valid_; }

private:
    /// 创建折线描边渲染管线（solid，POSITION + COLOR 顶点）
    bool create_solid_pipeline();

    /// 创建 SDF 形状渲染管线（SdfVertex，含 SDF 参数）
    bool create_sdf_pipeline();

    /// 创建文字渲染管线（TextVertex，字形图集采样）
    bool create_text_pipeline();

    /// 创建高斯模糊管线（BlurVertex，全屏四边形，用于亚克力背景模糊）
    bool create_blur_pipeline();

    /**
     * @brief 创建裁剪相关管线（SDF ClipWrite/ClipErase/ClipTest 以及对应变体）。
     *
     * 裁剪系统使用模板缓冲实现：
     *   - sdf_clip_write_pipeline_  : SDF 几何 + ClipWrite 模板（压入裁剪层）
     *   - sdf_clip_erase_pipeline_  : SDF 几何 + ClipErase 模板（弹出裁剪层）
     *   - sdf_clip_test_pipeline_   : SDF 几何 + ClipTest 模板（裁剪状态下普通 SDF 绘制）
     *   - text_clip_test_pipeline_  : 文字 + ClipTest 模板（裁剪状态下文字绘制）
     *   - solid_clip_test_pipeline_ : 折线 + ClipTest 模板（裁剪状态下折线绘制）
     *
     * @return true = 所有管线创建成功
     */
    bool create_clip_pipelines();

    /**
     * @brief 确保裁剪模板纹理已创建且尺寸匹配目标渲染纹理。
     *
     * 模板纹理格式为 D24_UNorm_S8_UInt（24 位深度 + 8 位模板），同时绑定 DepthStencil。
     * 首次调用或目标尺寸变化时（懒创建）才实际创建 GPU 纹理。
     *
     * @param target  主渲染目标纹理（用于获取需要的尺寸）
     * @return true = 模板纹理可用，false = 创建失败（将禁用裁剪功能）
     */
    bool ensure_stencil_texture(gfx::ITexture* target);

    /**
     * @brief 确保亚克力 scratch 纹理已创建且尺寸匹配目标渲染纹理。
     *
     * 两个 scratch 纹理均为 RGBA8_UNorm 格式，同时绑定 ShaderResource 和 RenderTarget。
     * 首次调用或目标尺寸变化时（懒创建）才实际创建 GPU 纹理。
     *
     * @param target  主渲染目标纹理（用于获取需要的尺寸和格式）
     * @return true = scratch 纹理可用，false = 创建失败
     */
    bool ensure_scratch_textures(gfx::ITexture* target);

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

    /**
     * @brief 构建画刷数据常量缓冲（BrushDataCB）。
     *
     * 将 Brush 中存储的画刷数据（归一化坐标）转换为 HLSL 着色器使用的 local 坐标（像素）。
     *
     * @param brush   画刷对象
     * @param cx,cy   形状中心（屏幕像素坐标）
     * @param half_w  形状包围盒半宽（像素）
     * @param half_h  形状包围盒半高（像素）
     * @return 填充好的 BrushDataCB 结构体
     */
    static BrushDataCB make_brush_cb(
        const Brush& brush,
        float cx, float cy,
        float half_w, float half_h) noexcept;

    bool  valid_{false};
    float dpi_scale_{1.0f};  ///< 物理像素 / 逻辑像素缩放因子，默认 1（不缩放）

    gfx::IDevice*                        device_{nullptr};       ///< 图形设备（不拥有）
    core::OwnedPtr<gfx::IQueue>          queue_;                 ///< 提交队列
    core::OwnedPtr<gfx::ICommandList>    cmd_list_;              ///< 命令录制列表
    core::OwnedPtr<gfx::IPipeline>       solid_pipeline_;        ///< 折线描边管线（POSITION+COLOR）
    core::OwnedPtr<gfx::IPipeline>       sdf_pipeline_;          ///< SDF 形状管线（SdfVertex）
    core::OwnedPtr<gfx::IPipeline>       text_pipeline_;         ///< 文字渲染管线（TextVertex）
    core::OwnedPtr<gfx::IPipeline>       blur_pipeline_;         ///< 高斯模糊管线（BlurVertex，亚克力用）
    core::OwnedPtr<gfx::ITexture>        blur_scratch_a_;        ///< 亚克力模糊 scratch 纹理 A（捕获/结果）
    core::OwnedPtr<gfx::ITexture>        blur_scratch_b_;        ///< 亚克力模糊 scratch 纹理 B（中间结果）
    core::OwnedPtr<GlyphAtlas>           glyph_atlas_;           ///< 字形 GPU 图集管理器

    // ── 裁剪系统（模板缓冲方案）────────────────────────────────────────────────
    core::OwnedPtr<gfx::ITexture>        clip_stencil_;              ///< D24_S8 深度模板纹理（懒创建）
    core::OwnedPtr<gfx::IPipeline>       sdf_clip_write_pipeline_;   ///< SDF + ClipWrite（压入裁剪层）
    core::OwnedPtr<gfx::IPipeline>       sdf_clip_erase_pipeline_;   ///< SDF + ClipErase（弹出裁剪层）
    core::OwnedPtr<gfx::IPipeline>       sdf_clip_test_pipeline_;    ///< SDF + ClipTest（裁剪状态 SDF 绘制）
    core::OwnedPtr<gfx::IPipeline>       text_clip_test_pipeline_;   ///< 文字 + ClipTest（裁剪状态文字绘制）
    core::OwnedPtr<gfx::IPipeline>       solid_clip_test_pipeline_;  ///< 折线 + ClipTest（裁剪状态折线绘制）
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

bool RhiRenderer::create_text_pipeline() {
    // 文字顶点布局：POSITION(float2) + TEXCOORD0(float2) + COLOR(float4) = 32 字节
    gfx::PipelineDesc desc{};

    desc.vertex_shader.data        = k_text_vertex_shader_hlsl;
    desc.vertex_shader.size        = sizeof(k_text_vertex_shader_hlsl) - 1;
    desc.vertex_shader.language    = gfx::ShaderLanguage::HLSL;
    desc.vertex_shader.entry_point = "main";
    desc.vertex_shader.is_source   = true;

    desc.pixel_shader.data        = k_text_pixel_shader_hlsl;
    desc.pixel_shader.size        = sizeof(k_text_pixel_shader_hlsl) - 1;
    desc.pixel_shader.language    = gfx::ShaderLanguage::HLSL;
    desc.pixel_shader.entry_point = "main";
    desc.pixel_shader.is_source   = true;

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

    desc.vertex_shader.data        = k_blur_vertex_shader_hlsl;
    desc.vertex_shader.size        = sizeof(k_blur_vertex_shader_hlsl) - 1;
    desc.vertex_shader.language    = gfx::ShaderLanguage::HLSL;
    desc.vertex_shader.entry_point = "main";
    desc.vertex_shader.is_source   = true;

    desc.pixel_shader.data        = k_blur_pixel_shader_hlsl;
    desc.pixel_shader.size        = sizeof(k_blur_pixel_shader_hlsl) - 1;
    desc.pixel_shader.language    = gfx::ShaderLanguage::HLSL;
    desc.pixel_shader.entry_point = "main";
    desc.pixel_shader.is_source   = true;

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
        desc.vertex_shader.data        = k_sdf_vertex_shader_hlsl;
        desc.vertex_shader.size        = sizeof(k_sdf_vertex_shader_hlsl) - 1;
        desc.vertex_shader.language    = gfx::ShaderLanguage::HLSL;
        desc.vertex_shader.entry_point = "main";
        desc.vertex_shader.is_source   = true;

        desc.pixel_shader.data        = k_sdf_clip_pixel_shader_hlsl;
        desc.pixel_shader.size        = sizeof(k_sdf_clip_pixel_shader_hlsl) - 1;
        desc.pixel_shader.language    = gfx::ShaderLanguage::HLSL;
        desc.pixel_shader.entry_point = "main";
        desc.pixel_shader.is_source   = true;

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
        desc.vertex_shader.data        = k_sdf_vertex_shader_hlsl;
        desc.vertex_shader.size        = sizeof(k_sdf_vertex_shader_hlsl) - 1;
        desc.vertex_shader.language    = gfx::ShaderLanguage::HLSL;
        desc.vertex_shader.entry_point = "main";
        desc.vertex_shader.is_source   = true;

        desc.pixel_shader.data        = k_sdf_clip_pixel_shader_hlsl;
        desc.pixel_shader.size        = sizeof(k_sdf_clip_pixel_shader_hlsl) - 1;
        desc.pixel_shader.language    = gfx::ShaderLanguage::HLSL;
        desc.pixel_shader.entry_point = "main";
        desc.pixel_shader.is_source   = true;

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
        desc.vertex_shader.data        = k_text_vertex_shader_hlsl;
        desc.vertex_shader.size        = sizeof(k_text_vertex_shader_hlsl) - 1;
        desc.vertex_shader.language    = gfx::ShaderLanguage::HLSL;
        desc.vertex_shader.entry_point = "main";
        desc.vertex_shader.is_source   = true;

        desc.pixel_shader.data        = k_text_pixel_shader_hlsl;
        desc.pixel_shader.size        = sizeof(k_text_pixel_shader_hlsl) - 1;
        desc.pixel_shader.language    = gfx::ShaderLanguage::HLSL;
        desc.pixel_shader.entry_point = "main";
        desc.pixel_shader.is_source   = true;

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

    // ── 3. 设置全局渲染状态（渲染目标 + 视口 + 模板缓冲）────────────────────

    // 确保裁剪模板纹理就绪（懒创建，尺寸与渲染目标一致）
    const bool stencil_ok = ensure_stencil_texture(target);

    // 设置主渲染目标（若模板纹理可用则同时绑定，否则无模板）
    cmd_list_->set_render_target(
        target,
        stencil_ok ? clip_stencil_.get() : nullptr);

    // 若模板纹理可用，清除模板值为 0（每帧开始时重置所有裁剪状态）
    if (stencil_ok) {
        cmd_list_->clear_depth_stencil(clip_stencil_.get(), 1.0f, 0);
    }

    gfx::Viewport viewport{};
    viewport.x         = 0.0f;
    viewport.y         = 0.0f;
    viewport.width     = static_cast<float>(target->width());
    viewport.height    = static_cast<float>(target->height());
    viewport.min_depth = 0.0f;
    viewport.max_depth = 1.0f;
    cmd_list_->set_viewport(viewport);

    // ── 4. 裁剪状态跟踪变量 ─────────────────────────────────────────────────

    // clip_depth：当前活跃裁剪层深度（0 = 无裁剪，1 = 一层裁剪，依此类推）
    // clip_stack：已压入的 ClipPush* DrawCmd 列表（ClipPop 时弹出并重绘同一形状）
    uint32_t clip_depth = 0;
    containers::Vector<DrawCmd> clip_stack;

    // 辅助函数：根据给定的 ClipPush* DrawCmd 上传 PolygonVertsCB 并设置多边形裁剪参数
    // 仅当命令为 ClipPushPolygon 时需要绑定 b1 槽；其他类型返回 nullptr
    auto make_clip_polygon_cb = [&](const DrawCmd& clip_cmd) -> core::OwnedPtr<gfx::IBuffer> {
        if (clip_cmd.kind != DrawCmdKind::ClipPushPolygon) return nullptr;
        if (clip_cmd.path_index >= static_cast<uint32_t>(dl.paths().size())) return nullptr;

        const Path& poly_path = dl.paths()[clip_cmd.path_index];
        containers::Vector<math::Vec2> poly_verts;
        for (const auto& pc : poly_path.cmds()) {
            if (pc.kind == PathCmdKind::MoveTo || pc.kind == PathCmdKind::LineTo) {
                poly_verts.push_back(pc.pt[0]);
            }
        }
        const int poly_n = static_cast<int>(poly_verts.size());
        if (poly_n < 3 || poly_n > 64) return nullptr;

        const float cx = clip_cmd.pt_a.x;
        const float cy = clip_cmd.pt_a.y;

        PolygonVertsCB poly_cb{};
        poly_cb.vert_count = poly_n;
        poly_cb.pad[0] = poly_cb.pad[1] = poly_cb.pad[2] = 0.0f;
        for (int k = 0; k < poly_n; ++k) {
            poly_cb.verts[k][0] = poly_verts[k].x - cx;
            poly_cb.verts[k][1] = poly_verts[k].y - cy;
            poly_cb.verts[k][2] = 0.0f;
            poly_cb.verts[k][3] = 0.0f;
        }

        gfx::BufferDesc cb_desc{};
        cb_desc.size       = sizeof(PolygonVertsCB);
        cb_desc.stride     = 0;
        cb_desc.bind_flags = gfx::BufferBindFlags::Constant;
        return device_->create_buffer(cb_desc, &poly_cb);
    };

    // 辅助函数：为裁剪命令（ClipPushRect/RoundedRect/ComplexRoundedRect/Polygon）
    // 生成 SDF 包围盒顶点缓冲。
    auto make_clip_vb = [&](const DrawCmd& clip_cmd) -> core::OwnedPtr<gfx::IBuffer> {
        containers::Vector<SdfVertex> verts;

        switch (clip_cmd.kind) {
        case DrawCmdKind::ClipPushRect: {
            const float cx = clip_cmd.rect.x + clip_cmd.rect.width  * 0.5f;
            const float cy = clip_cmd.rect.y + clip_cmd.rect.height * 0.5f;
            const float hw = clip_cmd.rect.width  * 0.5f;
            const float hh = clip_cmd.rect.height * 0.5f;
            push_sdf_quad_vertices(verts, cx, cy, hw, hh, 1.0f,
                0.0f, 0.0f, 0.0f, 1.0f,  // 颜色占位（ColorWriteMask=0 不输出）
                0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f);
            break;
        }
        case DrawCmdKind::ClipPushRoundedRect: {
            const float cx = clip_cmd.rrect.rect.x + clip_cmd.rrect.rect.width  * 0.5f;
            const float cy = clip_cmd.rrect.rect.y + clip_cmd.rrect.rect.height * 0.5f;
            const float hw = clip_cmd.rrect.rect.width  * 0.5f;
            const float hh = clip_cmd.rrect.rect.height * 0.5f;
            const float rad = std::min({clip_cmd.rrect.radius_x, clip_cmd.rrect.radius_y, hw, hh});
            push_sdf_quad_vertices(verts, cx, cy, hw, hh, 1.0f,
                0.0f, 0.0f, 0.0f, 1.0f,
                1.0f, rad, 0.0f, 0.0f, 0.0f, 0.0f);
            break;
        }
        case DrawCmdKind::ClipPushComplexRoundedRect: {
            const float cx = clip_cmd.complex_rrect.rect.x + clip_cmd.complex_rrect.rect.width  * 0.5f;
            const float cy = clip_cmd.complex_rrect.rect.y + clip_cmd.complex_rrect.rect.height * 0.5f;
            const float hw = clip_cmd.complex_rrect.rect.width  * 0.5f;
            const float hh = clip_cmd.complex_rrect.rect.height * 0.5f;
            const auto& radii = clip_cmd.complex_rrect.radii;
            const float r_br = std::min(std::min(radii.bottom_right.x, radii.bottom_right.y), std::min(hw, hh));
            const float r_tr = std::min(std::min(radii.top_right.x,    radii.top_right.y),    std::min(hw, hh));
            const float r_bl = std::min(std::min(radii.bottom_left.x,  radii.bottom_left.y),  std::min(hw, hh));
            const float r_tl = std::min(std::min(radii.top_left.x,     radii.top_left.y),     std::min(hw, hh));
            push_sdf_quad_vertices(verts, cx, cy, hw, hh, 1.0f,
                0.0f, 0.0f, 0.0f, 1.0f,
                2.0f, r_br, r_tr, r_bl, r_tl, 0.0f);
            break;
        }
        case DrawCmdKind::ClipPushPolygon: {
            const float cx = clip_cmd.pt_a.x;
            const float cy = clip_cmd.pt_a.y;
            const float hw = clip_cmd.pt_b.x;
            const float hh = clip_cmd.pt_b.y;
            push_sdf_quad_vertices(verts, cx, cy, hw, hh, 1.0f,
                0.0f, 0.0f, 0.0f, 1.0f,
                10.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f);
            break;
        }
        default:
            return nullptr;
        }

        if (verts.empty()) return nullptr;

        gfx::BufferDesc vb{};
        vb.size       = static_cast<uint64_t>(verts.size()) * sizeof(SdfVertex);
        vb.stride     = sizeof(SdfVertex);
        vb.bind_flags = gfx::BufferBindFlags::Vertex;
        return device_->create_buffer(vb, verts.data());
    };

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

        // ── 裁剪状态辅助：预选管线，ClipPush*/ClipPop 分支自行覆盖 ──────────
        //
        // 当 clip_depth > 0 且模板纹理可用时：
        //   - 预先调用 set_stencil_ref(clip_depth)，使后续 draw() 以正确参考值通过等式测试
        //   - SDF 形状切换到 sdf_clip_test_pipeline_（Equal 测试，Keep，正常颜色输出）
        //   - 文字切换到 text_clip_test_pipeline_（同上）
        // ClipPush* / ClipPop 分支会覆盖 stencil_ref 并使用专用管线，不受此影响。
        const bool in_clip = (clip_depth > 0 && stencil_ok);
        if (in_clip) {
            cmd_list_->set_stencil_ref(static_cast<uint8_t>(clip_depth));
        }
        gfx::IPipeline* const active_sdf_pl  =
            in_clip ? sdf_clip_test_pipeline_.get() : sdf_pipeline_.get();
        gfx::IPipeline* const active_text_pl =
            in_clip ? text_clip_test_pipeline_.get() : text_pipeline_.get();

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

        // StrokePath、ClipPushRect 等高级命令留至后续里程碑实现

        // ── 裁剪状态命令（ClipPush* / ClipPop）──────────────────────────────

        else if (cmd.kind == DrawCmdKind::ClipPushRect         ||
                 cmd.kind == DrawCmdKind::ClipPushRoundedRect   ||
                 cmd.kind == DrawCmdKind::ClipPushComplexRoundedRect ||
                 cmd.kind == DrawCmdKind::ClipPushPolygon)
        {
            // 裁剪功能需要模板纹理支持；若模板纹理不可用则静默跳过
            if (!stencil_ok) continue;

            // 生成形状 SDF 包围盒顶点缓冲
            auto clip_vb = make_clip_vb(cmd);
            if (!clip_vb) continue;

            // 多边形裁剪需要额外的顶点常量缓冲（b1 槽）
            auto polygon_cb_buf = make_clip_polygon_cb(cmd);

            // 设置模板参考值为当前裁剪深度（Equal(clip_depth) → 匹配"在所有祖先裁剪区内"的像素）
            cmd_list_->set_stencil_ref(static_cast<uint8_t>(clip_depth));
            // 使用 ClipWrite 管线（IncrSat 写入，禁止颜色输出）
            cmd_list_->set_pipeline(sdf_clip_write_pipeline_.get());
            cmd_list_->set_constant_buffer(0, viewport_cb.get());
            if (polygon_cb_buf) {
                cmd_list_->set_constant_buffer(1, polygon_cb_buf.get());
            }
            cmd_list_->set_vertex_buffer(0, clip_vb.get(), 0);
            cmd_list_->draw(6, 1, 0, 0);  // SDF quad = 6 顶点

            // 压入裁剪栈，增加裁剪深度
            clip_stack.push_back(cmd);
            clip_depth++;
        }

        else if (cmd.kind == DrawCmdKind::ClipPop) {
            // 裁剪栈为空时忽略（与 save/restore 不匹配的 ClipPop 防御）
            if (!stencil_ok || clip_stack.empty() || clip_depth == 0) continue;

            // 弹出最近一次压入的裁剪命令
            DrawCmd popped = clip_stack.back();
            clip_stack.pop_back();
            clip_depth--;

            // 生成与压入时相同的 SDF 几何体
            auto clip_vb = make_clip_vb(popped);
            if (!clip_vb) continue;

            auto polygon_cb_buf = make_clip_polygon_cb(popped);

            // 设置模板参考值为 clip_depth+1（即被压入时的深度值，Equal(ref) → 精确撤销本层写入）
            cmd_list_->set_stencil_ref(static_cast<uint8_t>(clip_depth + 1));
            // 使用 ClipErase 管线（DecrSat 写入，禁止颜色输出）
            cmd_list_->set_pipeline(sdf_clip_erase_pipeline_.get());
            cmd_list_->set_constant_buffer(0, viewport_cb.get());
            if (polygon_cb_buf) {
                cmd_list_->set_constant_buffer(1, polygon_cb_buf.get());
            }
            cmd_list_->set_vertex_buffer(0, clip_vb.get(), 0);
            cmd_list_->draw(6, 1, 0, 0);
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

        // ── DrawText（文字渲染）─────────────────────────────────────────────

        else if (cmd.kind == DrawCmdKind::DrawText) {
            if (cmd.brush.kind() != BrushKind::SolidColor) continue;
            if (!glyph_atlas_) continue;

            // 获取 TextRun（path_index 复用为 text_run 索引）
            const auto& text_runs = dl.text_runs();
            if (cmd.path_index >= static_cast<uint32_t>(text_runs.size())) continue;
            const TextRun& run = text_runs[cmd.path_index];

            if (run.font_face == nullptr || run.length == 0 || run.size_px <= 0.0f) continue;

            // 字号取整（缓存键使用整像素）
            const uint32_t size_px = static_cast<uint32_t>(run.size_px + 0.5f);
            auto* face = static_cast<text::FontFace*>(run.font_face);
            const math::Color color = cmd.brush.color();

            // ── 阶段 1：光栅化全部字形并写入 CPU 图集 ───────────────────────
            // UTF-8 解码，逐码点查询/插入字形图集
            {
                uint32_t  i   = 0;
                const char* s = run.utf8;
                while (i < run.length) {
                    uint32_t codepoint = 0;
                    const uint8_t c0 = static_cast<uint8_t>(s[i]);
                    uint32_t      adv = 1;

                    if (c0 < 0x80u) {
                        // ASCII（0xxxxxxx）
                        codepoint = c0;
                        adv = 1;
                    } else if ((c0 & 0xE0u) == 0xC0u && i + 1 < run.length) {
                        // 2 字节（110xxxxx 10xxxxxx）
                        const uint8_t c1 = static_cast<uint8_t>(s[i + 1]);
                        codepoint = ((c0 & 0x1Fu) << 6u) | (c1 & 0x3Fu);
                        adv = 2;
                    } else if ((c0 & 0xF0u) == 0xE0u && i + 2 < run.length) {
                        // 3 字节（1110xxxx 10xxxxxx 10xxxxxx）
                        const uint8_t c1 = static_cast<uint8_t>(s[i + 1]);
                        const uint8_t c2 = static_cast<uint8_t>(s[i + 2]);
                        codepoint = ((c0 & 0x0Fu) << 12u) | ((c1 & 0x3Fu) << 6u) | (c2 & 0x3Fu);
                        adv = 3;
                    } else if ((c0 & 0xF8u) == 0xF0u && i + 3 < run.length) {
                        // 4 字节（11110xxx 10xxxxxx 10xxxxxx 10xxxxxx）
                        const uint8_t c1 = static_cast<uint8_t>(s[i + 1]);
                        const uint8_t c2 = static_cast<uint8_t>(s[i + 2]);
                        const uint8_t c3 = static_cast<uint8_t>(s[i + 3]);
                        codepoint = ((c0 & 0x07u) << 18u) | ((c1 & 0x3Fu) << 12u)
                                  | ((c2 & 0x3Fu) << 6u)  |  (c3 & 0x3Fu);
                        adv = 4;
                    } else {
                        // 非法 UTF-8 序列，跳过此字节
                        adv = 1;
                        codepoint = 0xFFFDu;  // 替换字符
                    }

                    i += adv;
                    // 预热缓存（光栅化到 CPU 图集，尚未上传 GPU）
                    glyph_atlas_->get_or_insert(face, codepoint, size_px);
                }
            }

            // ── 阶段 2：上传 CPU 图集到 GPU（若有新字形）────────────────────
            glyph_atlas_->flush(device_);
            if (!glyph_atlas_->texture()) continue;

            // ── 阶段 3：生成文字四边形顶点 ──────────────────────────────────
            containers::Vector<TextVertex> text_verts;

            float pen_x = run.origin_x;  // 当前笔触 X（基线）
            const float pen_y = run.origin_y;   // 当前笔触 Y（基线，Y 向下）

            // 再次遍历文字，生成每个字形的顶点
            {
                uint32_t  i   = 0;
                const char* s = run.utf8;
                while (i < run.length) {
                    uint32_t codepoint = 0;
                    uint32_t adv = 1;
                    const uint8_t c0 = static_cast<uint8_t>(s[i]);

                    if (c0 < 0x80u) {
                        codepoint = c0; adv = 1;
                    } else if ((c0 & 0xE0u) == 0xC0u && i + 1 < run.length) {
                        const uint8_t c1 = static_cast<uint8_t>(s[i + 1]);
                        codepoint = ((c0 & 0x1Fu) << 6u) | (c1 & 0x3Fu); adv = 2;
                    } else if ((c0 & 0xF0u) == 0xE0u && i + 2 < run.length) {
                        const uint8_t c1 = static_cast<uint8_t>(s[i + 1]);
                        const uint8_t c2 = static_cast<uint8_t>(s[i + 2]);
                        codepoint = ((c0 & 0x0Fu) << 12u) | ((c1 & 0x3Fu) << 6u) | (c2 & 0x3Fu); adv = 3;
                    } else if ((c0 & 0xF8u) == 0xF0u && i + 3 < run.length) {
                        const uint8_t c1 = static_cast<uint8_t>(s[i + 1]);
                        const uint8_t c2 = static_cast<uint8_t>(s[i + 2]);
                        const uint8_t c3 = static_cast<uint8_t>(s[i + 3]);
                        codepoint = ((c0 & 0x07u) << 18u) | ((c1 & 0x3Fu) << 12u)
                                  | ((c2 & 0x3Fu) << 6u)  |  (c3 & 0x3Fu); adv = 4;
                    } else {
                        adv = 1; codepoint = 0xFFFDu;
                    }
                    i += adv;

                    const AtlasEntry* entry = glyph_atlas_->get_or_insert(face, codepoint, size_px);
                    if (entry == nullptr) {
                        // 缓存查找失败（理论上不会发生，第一阶段已插入）
                        pen_x += static_cast<float>(size_px) * 0.5f;
                        continue;
                    }

                    // 步进（包括宽度为 0 的空白字形）
                    if (entry->atlas_w == 0) {
                        pen_x += static_cast<float>(entry->advance_x);
                        continue;
                    }

                    // 字形顶点左上角屏幕坐标（Y 向下，bearing_y 为基线上方，故减去）
                    const float gx = pen_x + static_cast<float>(entry->bearing_x);
                    const float gy = pen_y - static_cast<float>(entry->bearing_y);
                    const float gw = static_cast<float>(entry->atlas_w);
                    const float gh = static_cast<float>(entry->atlas_h);

                    // 图集 UV（不偏移半像素，依赖 1px 边距保护采样越界）
                    const float inv = 1.0f / static_cast<float>(GlyphAtlas::kAtlasSize);
                    const float u0  = static_cast<float>(entry->atlas_x) * inv;
                    const float v0  = static_cast<float>(entry->atlas_y) * inv;
                    const float u1  = (static_cast<float>(entry->atlas_x) + gw) * inv;
                    const float v1  = (static_cast<float>(entry->atlas_y) + gh) * inv;

                    const float cr = color.r, cg = color.g, cb = color.b, ca = color.a;

                    // 生成 2 个三角形（6 顶点）覆盖字形矩形
                    // 三角形 1：左上, 右上, 左下
                    text_verts.push_back({gx,      gy,      u0, v0, cr, cg, cb, ca});
                    text_verts.push_back({gx + gw, gy,      u1, v0, cr, cg, cb, ca});
                    text_verts.push_back({gx,      gy + gh, u0, v1, cr, cg, cb, ca});
                    // 三角形 2：右上, 右下, 左下
                    text_verts.push_back({gx + gw, gy,      u1, v0, cr, cg, cb, ca});
                    text_verts.push_back({gx + gw, gy + gh, u1, v1, cr, cg, cb, ca});
                    text_verts.push_back({gx,      gy + gh, u0, v1, cr, cg, cb, ca});

                    pen_x += static_cast<float>(entry->advance_x);
                }
            }

            if (text_verts.empty()) continue;
            apply_text_transform(text_verts);

            // ── 阶段 4：提交 DrawCall ──────────────────────────────────────────
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
