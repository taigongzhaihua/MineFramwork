// vs_basic.hlsl 鈥?浠?src/mine/paint/src/RhiRenderer.cpp 鑷姩鎻愬彇
// 鍘熷鍙橀噺鍚嶏細k_vertex_shader_hlsl
// 鐢熸垚鏃堕棿锛?(Get-Date -Format 'yyyy-MM-dd HH:mm:ss')
// 姝ゆ枃浠剁敱 scripts/extract_hlsl.ps1 鐢熸垚锛岃鍕挎墜鍔ㄧ紪杈戙€?
// 淇敼鐫€鑹插櫒璇风紪杈?RhiRenderer.cpp 鍚庨噸鏂拌繍琛屾彁鍙栬剼鏈€?
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
