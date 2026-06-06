// ps_basic.hlsl - 从 src/mine/paint/src/RhiRenderer.cpp 自动提取
// 原始变量名：k_pixel_shader_hlsl
// 生成时间：(Get-Date -Format 'yyyy-MM-dd HH:mm:ss')
// 此文件由 scripts/extract_hlsl.ps1 生成，请勿手动编辑。
// 修改着色器请编辑 RhiRenderer.cpp 后重新运行提取脚本。
struct PSIn {
    float4 pos   : SV_Position;
    float4 color : COLOR;
};

float4 main(PSIn input) : SV_Target {
    return input.color;
}
