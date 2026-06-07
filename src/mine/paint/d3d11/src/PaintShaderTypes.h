/**
 * @file PaintShaderTypes.h
 * @brief mine.paint 渲染器使用的顶点格式、常量缓冲结构体定义。
 *
 * 这些结构体与 HLSL 着色器中的内存布局一一对应（16 字节对齐）。
 * 从 RhiRenderer.cpp 中提取，供渲染器实现和管线创建代码共用。
 */

#pragma once

#include <cstdint>
#include <cstddef>

namespace mine::paint {

// ── 顶点结构体 ────────────────────────────────────────────────────────────────

/// 折线描边顶点：屏幕像素坐标 + 线性 RGBA 颜色（用于 StrokeLine 等折线命令）
struct PaintVertex {
    float x, y;        ///< 屏幕像素坐标（左上角为原点）
    float r, g, b, a;  ///< 线性 RGBA 颜色（[0,1]）
};

/**
 * @brief SDF 形状顶点（80 字节）。
 *
 * 用于矩形/圆角矩形/复杂圆角/椭圆的填充与描边。
 * 每个形状生成一个覆盖包围盒的矩形（6 顶点），
 * 像素着色器逐像素计算有向距离场（SDF）确定覆盖度。
 *
 * 内存布局（20 float）：
 *   [0..7]   POSITION (float2)  — 屏幕像素坐标
 *   [8..23]  COLOR    (float4)  — 线性 RGBA 颜色
 *   [24..31] TEXCOORD0 (float2) — 本地坐标（以形状中心为原点，像素单位）
 *   [32..47] TEXCOORD1 (float4) — (kind, half_w, half_h, p0)
 *   [48..63] TEXCOORD2 (float4) — (p1, p2, p3, stroke_w)
 *   [64..79] TEXCOORD3 (float4) — 扩展参数（四角圆角半径）
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

// ── 常量缓冲结构体 ────────────────────────────────────────────────────────────

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
};
static_assert(sizeof(BrushDataCB) == 128, "BrushDataCB 大小必须为 128 字节");

/// 最大嵌套裁剪层数（对应 HLSL ClipSdfCB.clip_layers 数组大小）
static constexpr int k_max_clip_layers     = 4;
/// 每层多边形顶点上限（对应 HLSL ClipSdfLayer.poly_verts 数组大小）
static constexpr int k_max_clip_poly_verts = 64;

/**
 * @brief 裁剪 SDF 单层结构体（1072 字节，16 字节对齐）。
 *
 * 与 HLSL ClipSdfLayer 内存布局完全一致。
 */
struct alignas(16) ClipSdfLayer {
    float cx, cy, half_w, half_h;                       ///< 裁剪形状中心和半尺寸（逻辑像素）
    float kind, p0, p1, p2;                             ///< 形状类型及圆角参数
    float p3, poly_vert_count_f, _pad0, _pad1;          ///< r_tl / 多边形顶点数 / 填充
    float poly_verts[k_max_clip_poly_verts][4];          ///< 多边形顶点（xy=local，zw=0）
};
static_assert(sizeof(ClipSdfLayer) == 1072, "ClipSdfLayer 大小必须为 1072 字节");

/**
 * @brief 裁剪 SDF 常量缓冲（4304 字节，16 字节对齐）。
 *
 * 与 HLSL ClipSdfCB 内存布局完全一致。
 */
struct alignas(16) ClipSdfCB {
    int   clip_count;         ///< 当前活跃裁剪层数（0=无裁剪）
    float _pad0, _pad1, _pad2;
    ClipSdfLayer layers[k_max_clip_layers];
};
static_assert(sizeof(ClipSdfCB) == 4304, "ClipSdfCB 大小必须为 4304 字节");

/// 模糊常量缓冲（b1 槽，每个模糊通道更新一次）
struct alignas(16) BlurCB {
    float texel_step_x;  ///< 采样步进 X（水平通道 = blur_step/tex_w，垂直通道 = 0）
    float texel_step_y;  ///< 采样步进 Y（水平通道 = 0，垂直通道 = blur_step/tex_h）
    float _pad0{0.0f};
    float _pad1{0.0f};
};

/// 多边形顶点常量缓冲（b1 槽，仅多边形 DrawCall 绑定）
struct alignas(16) PolygonVertsCB {
    int   vert_count;       ///< 多边形顶点数（3..64）
    float pad[3];           ///< 填充至 16 字节
    float verts[64][4];     ///< 各顶点本地坐标（[k][0]=local_x, [k][1]=local_y）
};

} // namespace mine::paint
