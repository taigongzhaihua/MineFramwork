$ErrorActionPreference = "Stop"

Set-Location (Join-Path $PSScriptRoot "..")

$headers = Get-ChildItem -Path "src/mine" -Recurse -File -Filter *.h
if (-not $headers) {
    throw "未找到公开头文件，无法完成基础检查。"
}

xmake f -c | Out-Null

Write-Host "基础检查通过，共发现 $($headers.Count) 个头文件。"