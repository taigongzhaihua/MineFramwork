// vs_sdf.hlsl 鈥?浠?src/mine/paint/src/RhiRenderer.cpp 鑷姩鎻愬彇
// 鍘熷鍙橀噺鍚嶏細k_sdf_vertex_shader_hlsl
// 鐢熸垚鏃堕棿锛?(Get-Date -Format 'yyyy-MM-dd HH:mm:ss')
// 姝ゆ枃浠剁敱 scripts/extract_hlsl.ps1 鐢熸垚锛岃鍕挎墜鍔ㄧ紪杈戙€?
// 淇敼鐫€鑹插櫒璇风紪杈?RhiRenderer.cpp 鍚庨噸鏂拌繍琛屾彁鍙栬剼鏈€?
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
