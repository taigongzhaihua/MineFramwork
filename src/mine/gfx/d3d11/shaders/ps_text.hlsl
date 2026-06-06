// ps_text.hlsl - 从 src/mine/paint/src/RhiRenderer.cpp 自动提取
// 原始变量名：k_text_pixel_shader_hlsl
// 生成时间：(Get-Date -Format 'yyyy-MM-dd HH:mm:ss')
// 此文件由 scripts/extract_hlsl.ps1 生成，请勿手动编辑。
// 修改着色器请编辑 RhiRenderer.cpp 后重新运行提取脚本。
cbuffer ViewportCB : register(b0) {
    float2 viewport_size;       // 逻辑像素尺寸（NDC/裁剪坐标换算用）
    float2 phys_viewport_size;  // 物理像素尺寸（SV_Position → 逻辑像素用）
};

struct ClipSdfLayer {
    float4 bounds;          // (cx, cy, half_w, half_h)
    float4 kind_p0_p1_p2;   // (kind, p0, p1, p2)
    float4 p3_nv_pad;       // (p3, poly_vert_count_f, _, _)
    float4 poly_verts[64];  // xy=相对裁剪中心的本地坐标
};

cbuffer ClipSdfCB : register(b3) {
    int   clip_count;
    float _cpad0;
    float _cpad1;
    float _cpad2;
    ClipSdfLayer clip_layers[4];
};

Texture2D    glyph_atlas   : register(t0);  // R8 字形灰度图集
SamplerState glyph_sampler : register(s0);  // 线性双线性采样器（CLAMP）

float box_sdf(float2 p, float2 b) {
    float2 q = abs(p) - b;
    return length(max(q, 0.0f)) + min(max(q.x, q.y), 0.0f);
}

float rounded_box_sdf(float2 p, float2 b, float r) {
    float2 q = abs(p) - b + r;
    return length(max(q, 0.0f)) + min(max(q.x, q.y), 0.0f) - r;
}

float complex_rounded_box_sdf(float2 p, float2 b, float4 r) {
    r.xy = (p.x > 0.0f) ? r.xy : r.zw;
    r.x  = (p.y > 0.0f) ? r.x  : r.y;
    float2 q = abs(p) - b + r.x;
    return length(max(q, 0.0f)) + min(max(q.x, q.y), 0.0f) - r.x;
}

float sdClipPolygon(float2 p, int li, int nv) {
    float d2 = dot(p - clip_layers[li].poly_verts[0].xy,
                   p - clip_layers[li].poly_verts[0].xy);
    float s = 1.0f;
    [loop]
    for (int vi = 0, vj = nv - 1; vi < nv; vj = vi, vi++) {
        float2 vvi = clip_layers[li].poly_verts[vi].xy;
        float2 vvj = clip_layers[li].poly_verts[vj].xy;
        float2 e = vvj - vvi;
        float2 w = p - vvi;
        float2 b2 = w - e * clamp(dot(w, e) / dot(e, e), 0.0f, 1.0f);
        d2 = min(d2, dot(b2, b2));
        bool c0 = (p.y >= vvi.y);
        bool c1 = (p.y <  vvj.y);
        bool c2 = (e.x * w.y > e.y * w.x);
        if ((c0 && c1 && c2) || (!c0 && !c1 && !c2)) s = -s;
    }
    return s * sqrt(d2);
}

float evaluate_clip_coverage(float2 sv_pos) {
    if (clip_count <= 0) return 1.0f;

    float2 lp = sv_pos * viewport_size / phys_viewport_size;
    float coverage = 1.0f;
    [loop]
    for (int ci = 0; ci < clip_count; ci++) {
        float2 cp  = lp - clip_layers[ci].bounds.xy;
        float  chw = clip_layers[ci].bounds.z;
        float  chh = clip_layers[ci].bounds.w;
        int    ck  = (int)(clip_layers[ci].kind_p0_p1_p2.x + 0.5f);
        float  dc;
        if (ck == 1) {
            dc = rounded_box_sdf(cp, float2(chw, chh), clip_layers[ci].kind_p0_p1_p2.y);
        } else if (ck == 2) {
            float r0 = clip_layers[ci].kind_p0_p1_p2.y;
            float r1 = clip_layers[ci].kind_p0_p1_p2.z;
            float r2 = clip_layers[ci].kind_p0_p1_p2.w;
            float r3 = clip_layers[ci].p3_nv_pad.x;
            dc = complex_rounded_box_sdf(cp, float2(chw, chh), float4(r0, r1, r2, r3));
        } else if (ck == 10) {
            int nv = clamp((int)(clip_layers[ci].p3_nv_pad.y + 0.5f), 3, 64);
            dc = sdClipPolygon(cp, ci, nv);
        } else {
            dc = box_sdf(cp, float2(chw, chh));
        }
        float fw_c = max(fwidth(dc), 0.5f);
        coverage *= 1.0f - smoothstep(-fw_c, fw_c, dc);
    }
    return coverage;
}

struct TextPSIn {
    float4 pos   : SV_Position;
    float2 uv    : TEXCOORD0;
    float4 color : COLOR;
};

float4 main(TextPSIn i) : SV_Target {
    // 采样字形灰度值（R 通道即 alpha 遮罩）
    float alpha = glyph_atlas.Sample(glyph_sampler, i.uv).r;
    // 文字与几何命令共用同一套 ClipSdf 软裁剪，避免 clip_rect 只裁选区不裁字形
    alpha *= evaluate_clip_coverage(i.pos.xy);
    // 预乘 Alpha 输出（匹配 ONE / INV_SRC_ALPHA 混合模式）
    return float4(i.color.rgb * alpha, i.color.a * alpha);
}
