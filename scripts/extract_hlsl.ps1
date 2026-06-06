# extract_hlsl.ps1 — 从 RhiRenderer.cpp 提取所有 HLSL 着色器到独立 .hlsl 文件
param(
    [string]$Source = "src/mine/paint/src/RhiRenderer.cpp",
    [string]$OutDir = "src/mine/gfx/d3d11/shaders"
)

$ErrorActionPreference = "Stop"

# 确定项目根目录（脚本位于 <root>/scripts/，源文件相对于项目根目录）
$ScriptDir = Split-Path $PSCommandPath -Parent
$ProjRoot  = Split-Path $ScriptDir -Parent

$srcPath = Join-Path $ProjRoot $Source
$outPath = Join-Path $ProjRoot $OutDir

Write-Host "源文件：$srcPath"
Write-Host "输出目录：$outPath"

if (-not (Test-Path $srcPath)) {
    Write-Error "源文件不存在：$srcPath"
    exit 1
}

$content = Get-Content $srcPath -Raw -Encoding UTF8

# 映射：shader 变量名 → 输出文件名
$shaders = @{
    "k_vertex_shader_hlsl"          = "vs_basic.hlsl"
    "k_pixel_shader_hlsl"           = "ps_basic.hlsl"
    "k_sdf_vertex_shader_hlsl"      = "vs_sdf.hlsl"
    "k_sdf_pixel_shader_hlsl"       = "ps_sdf.hlsl"
    "k_text_vertex_shader_hlsl"     = "vs_text.hlsl"
    "k_text_pixel_shader_hlsl"      = "ps_text.hlsl"
    "k_blur_vertex_shader_hlsl"     = "vs_blur.hlsl"
    "k_blur_pixel_shader_hlsl"      = "ps_blur.hlsl"
    "k_sdf_clip_pixel_shader_hlsl"  = "ps_sdf_clip.hlsl"
}

New-Item -ItemType Directory -Force -Path $outPath | Out-Null

foreach ($varName in $shaders.Keys) {
    $outFile = Join-Path $outPath $shaders[$varName]
    $hlsl = ""

    # 找到变量声明行并确定该变量所占的完整代码段
    $pattern = "static constexpr const char $varName\[]\s*="
    if ($content -match $pattern) {
        $startIdx = $content.IndexOf($matches[0])
        $remaining = $content.Substring($startIdx)

        # 找到该变量段落的结束位置：
        # 下一条 `static constexpr` 声明、或文件末尾
        $nextDecl = [regex]::Match($remaining.Substring(1), 'static constexpr const char \w+_hlsl\[]')
        $segEnd = if ($nextDecl.Success) { $nextDecl.Index + 1 } else { $remaining.Length }

        $shaderBlock = $remaining.Substring(0, $segEnd)

        # 提取该段内所有 R"hlsl( ... )hlsl" 段
        $segments = [regex]::Matches($shaderBlock, 'R"hlsl\((.*?)\)hlsl"', [System.Text.RegularExpressions.RegexOptions]::Singleline)

        foreach ($seg in $segments) {
            $hlsl += $seg.Groups[1].Value.TrimStart("`r`n") + "`r`n"
        }

        if ($hlsl.Length -gt 0) {
            # 移除开头的连续空行
            $hlsl = $hlsl -replace "^\r?\n+", ""
            $hlsl = $hlsl.TrimEnd("`r`n") + "`r`n"

            $header = @"
// $($shaders[$varName]) — 从 $Source 自动提取
// 原始变量名：$varName
// 生成时间：$(Get-Date -Format 'yyyy-MM-dd HH:mm:ss')
// 此文件由 scripts/extract_hlsl.ps1 生成，请勿手动编辑。
// 修改着色器请编辑 RhiRenderer.cpp 后重新运行提取脚本。

"@
            [System.IO.File]::WriteAllText($outFile, $header + $hlsl, [System.Text.UTF8Encoding]::new($false))
            Write-Host "✓ $outFile ($($hlsl.Length) chars)"
        } else {
            Write-Warning "✗ $varName → 未提取到 HLSL 内容"
        }
    } else {
        Write-Warning "✗ $varName → 在源文件中未找到"
    }
}

Write-Host "`n提取完成，输出目录：$outPath"
