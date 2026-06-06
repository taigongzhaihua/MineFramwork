// ps_blur.hlsl - 从 src/mine/paint/src/RhiRenderer.cpp 自动提取
// 原始变量名：k_blur_pixel_shader_hlsl
// 生成时间：(Get-Date -Format 'yyyy-MM-dd HH:mm:ss')
// 此文件由 scripts/extract_hlsl.ps1 生成，请勿手动编辑。
// 修改着色器请编辑 RhiRenderer.cpp 后重新运行提取脚本。
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
