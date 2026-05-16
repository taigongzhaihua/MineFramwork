#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
cd "${SCRIPT_DIR}/.."

if ! command -v xmake >/dev/null 2>&1; then
    echo "未找到 xmake，请先安装构建工具。" >&2
    exit 1
fi

xmake f -c
xmake project -k compile_commands

echo "MineFramework 项目骨架已完成基础配置。"