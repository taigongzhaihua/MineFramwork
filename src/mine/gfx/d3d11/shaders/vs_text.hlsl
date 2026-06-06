// vs_text.hlsl 鈥?浠?src/mine/paint/src/RhiRenderer.cpp 鑷姩鎻愬彇
// 鍘熷鍙橀噺鍚嶏細k_text_vertex_shader_hlsl
// 鐢熸垚鏃堕棿锛?(Get-Date -Format 'yyyy-MM-dd HH:mm:ss')
// 姝ゆ枃浠剁敱 scripts/extract_hlsl.ps1 鐢熸垚锛岃鍕挎墜鍔ㄧ紪杈戙€?
// 淇敼鐫€鑹插櫒璇风紪杈?RhiRenderer.cpp 鍚庨噸鏂拌繍琛屾彁鍙栬剼鏈€?
cbuffer ViewportCB : register(b0) {
    float2 viewport_size;       // 逻辑像素尺寸（NDC 变换用）
    float2 phys_viewport_size;  // 物理像素尺寸（与像素着色器共享布局）
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
