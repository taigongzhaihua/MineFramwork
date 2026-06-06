// ps_sdf_clip.hlsl - 从 src/mine/paint/src/RhiRenderer.cpp 自动提取
// 原始变量名：k_sdf_clip_pixel_shader_hlsl
// 生成时间：(Get-Date -Format 'yyyy-MM-dd HH:mm:ss')
// 此文件由 scripts/extract_hlsl.ps1 生成，请勿手动编辑。
// 修改着色器请编辑 RhiRenderer.cpp 后重新运行提取脚本。
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
