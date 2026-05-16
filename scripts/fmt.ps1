$ErrorActionPreference = "Stop"

Set-Location (Join-Path $PSScriptRoot "..")

$formatter = Get-Command clang-format -ErrorAction SilentlyContinue
if (-not $formatter) {
    throw "未找到 clang-format，请先安装格式化工具。"
}

$paths = @("src", "samples", "tests", "tools") | Where-Object { Test-Path $_ }
$files = Get-ChildItem -Path $paths -Recurse -File -Include *.h, *.hpp, *.cpp

if (-not $files) {
    Write-Host "未找到需要格式化的 C++ 文件。"
    exit 0
}

foreach ($file in $files) {
    & $formatter.Source -i $file.FullName
}

Write-Host "格式化完成，共处理 $($files.Count) 个文件。"