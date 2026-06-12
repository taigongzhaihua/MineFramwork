# MineUI 框架设计文档

> 代号：**Mine** / **MineUI**
> 标记语言：**MML**（Mine Markup Language，文件后缀 `.mml`）
> 构建系统：**xmake**
> 主语言：**C++20**（部分 C++23 特性视编译器支持开关）
> 目标：跨平台、强 MVVM、声明式 UI、SDK 交付、可裁剪、可定制到任意层级

本目录是 MineUI 框架的**总体设计文档集合**，目的是让任何贡献者都能依据这些文档独立实现某一模块而不破坏整体一致性。

## 阅读顺序

| # | 文档 | 主题 |
|---|------|------|
| 00 | [00-overview.md](00-overview.md) | 总体目标、设计原则、技术选型 |
| 01 | [01-architecture.md](01-architecture.md) | 分层架构、依赖方向、ABI 边界 |
| 02 | [02-modules.md](02-modules.md) | 模块清单（库一览） |
| 03 | [03-mml-language.md](03-mml-language.md) | MML 语言规格（语法/类型/语义） |
| 04 | [04-precompiler.md](04-precompiler.md) | 预编译器 `mmlc` 的设计 |
| 05 | [05-rendering.md](05-rendering.md) | 渲染抽象 RHI / DX11 / DX12 / 其它后端 |
| 06 | [06-composition.md](06-composition.md) | 合成树、脏区、文本、图像 |
| 07 | [07-windowing.md](07-windowing.md) | 多窗口、平台适配层 |
| 08 | [08-input.md](08-input.md) | 输入与事件路由 |
| 09 | [09-property-binding.md](09-property-binding.md) | 依赖属性系统与数据绑定 |
| 10 | [10-mvvm.md](10-mvvm.md) | MVVM、命令、导航、验证 |
| 11 | [11-di-ioc.md](11-di-ioc.md) | DI/IoC 容器 |
| 12 | [12-data-sqlite.md](12-data-sqlite.md) | SQLite 与轻量 ORM |
| 13 | [13-network.md](13-network.md) | 网络模块 |
| 14 | [14-toolchain.md](14-toolchain.md) | 工具链：mmlc/mmlls/mmlfmt/VSCode 扩展/安装程序 |
| 15 | [15-build-xmake.md](15-build-xmake.md) | xmake 构建、函数级裁剪、LTCG/LTO |
| 16 | [16-sdk-packaging.md](16-sdk-packaging.md) | SDK 交付物、版本策略、ABI 兼容 |
| 17 | [17-repo-layout.md](17-repo-layout.md) | 仓库目录布局 |
| 18 | [18-coding-conventions.md](18-coding-conventions.md) | 编码规范、命名、错误处理 |
| 19 | [19-roadmap.md](19-roadmap.md) | 路线图与里程碑 |
| 20 | [20-style-template.md](20-style-template.md) | 样式、模板与主题系统设计 |
| 21 | [21-core-api.md](21-core-api.md) | mine.core 模块 API 文档 |
| 22 | [22-appearance-architecture.md](22-appearance-architecture.md) | 控件外观架构（基元绘制 + 组合装配 + DP↔DP 绑定） |
| 23 | [23-io-module.md](23-io-module.md) | mine.io 模块：文件与 I/O API |
| 24 | [api/reflect/](api/reflect/06-Reflect.md) | mine.reflect 模块：静态反射 API |

## 一句话定位

**MineUI = (Slint 的 AOT 思路) + (WPF/Avalonia 的 MVVM 与依赖属性) + (现代 RHI 的多后端) + (高度模块化、函数级裁剪的 C++ SDK)**。

## 设计契约（红线）

1. **依赖单向**：上层可依赖下层，反之禁止；同层模块不得相互循环依赖。
2. **平台无关接口**：所有平台细节都隐藏在 `mine.platform.*` 与 `mine.gfx.*` 后端之后，公共 API 不出现 `HWND`、`ID3D11Device` 等类型。
3. **零隐式分配**：核心路径（输入/合成/渲染）默认不走全局 `new`，使用 `mine::core::Allocator`。
4. **可裁剪**：每个模块都是独立 xmake target，正式版默认开启 `/Gy /Gw + /OPT:REF,ICF`（MSVC）或 `-ffunction-sections -fdata-sections -Wl,--gc-sections`（GCC/Clang）。
5. **AOT 优先**：MML 在构建期被 `mmlc` 翻译为 C++，运行时**无解释器**；可选的 `mine.mml.runtime` 仅用于热重载/设计器场景。
6. **API 分级**：`mine::*`（稳定公开）/`mine::ext::*`（扩展点，半稳定）/`mine::detail::*`（内部，不保证兼容）。

---
# 注意

设计文档只是最初的设计构想，有很多地方实现已与设计时不一样，设计文档未及时更新，请以实际代码为准。