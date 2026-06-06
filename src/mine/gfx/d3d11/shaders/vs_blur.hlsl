// vs_blur.hlsl 鈥?浠?src/mine/paint/src/RhiRenderer.cpp 鑷姩鎻愬彇
// 鍘熷鍙橀噺鍚嶏細k_blur_vertex_shader_hlsl
// 鐢熸垚鏃堕棿锛?(Get-Date -Format 'yyyy-MM-dd HH:mm:ss')
// 姝ゆ枃浠剁敱 scripts/extract_hlsl.ps1 鐢熸垚锛岃鍕挎墜鍔ㄧ紪杈戙€?
// 淇敼鐫€鑹插櫒璇风紪杈?RhiRenderer.cpp 鍚庨噸鏂拌繍琛屾彁鍙栬剼鏈€?
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
