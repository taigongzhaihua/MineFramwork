# MineFramework

MineFramework 是一套基于 C++20 与 xmake 的声明式 UI 框架工程仓库。

当前仓库已初始化为与设计文档一致的基础骨架，重点覆盖以下内容：

- 顶层 xmake 构建入口与规则目录
- M0 基础模块的标准目录结构
- 样例、工具、测试与脚本目录的占位结构
- 面向后续开发的编码、格式化与忽略规则

## 仓库布局

目录结构遵循 [docs/17-repo-layout.md](docs/17-repo-layout.md) 与 [docs/development-plan.md](docs/development-plan.md)。

首批初始化范围聚焦在 M0：

- mine.core
- mine.containers
- mine.diag
- mine.async
- mine.io
- mine.math
- mine.text
- mine.reflect

## 构建

MineFramework 支持 **xmake** 和 **CMake** 双构建系统。

### 使用 xmake（推荐，开发环境）

```powershell
xmake f -m release
xmake build
xmake run sample.01-mvvm-binding
```

### 使用 CMake（生产环境、CI/CD）

```powershell
# Windows (MSVC)
mkdir build && cd build
cmake .. -G "Visual Studio 17 2022" -A x64
cmake --build . --config Release

# 或使用 Ninja
cmake .. -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build .
```

CMake 构建系统已配置国内镜像源（Gitee、清华大学镜像），详见 [docs/CMAKE_BUILD.md](docs/CMAKE_BUILD.md)。

随着模块实现逐步补充，顶层工程会自动包含已存在的模块 `xmake.lua` 和 `CMakeLists.txt` 文件。

## 文档

完整设计文档位于 `docs/` 目录，初始化工作严格对齐以下文档：

- `docs/00-overview.md`
- `docs/02-modules.md`
- `docs/15-build-xmake.md`
- `docs/17-repo-layout.md`
- `docs/development-plan.md`