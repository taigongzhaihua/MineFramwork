$ErrorActionPreference = "Stop"

Set-Location (Join-Path $PSScriptRoot "..")

if (-not (Get-Command xmake -ErrorAction SilentlyContinue)) {
    throw "未找到 xmake，请先安装构建工具。"
}

xmake f -c
xmake project -k compile_commands

Write-Host "MineFramework 项目骨架已完成基础配置。"