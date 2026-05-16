# 19 — 路线图与里程碑

> 时序仅描述阶段先后，不给具体时间。

## M0 — 框架骨架

* 仓库目录、xmake 顶层、CI 矩阵
* `mine.core / containers / diag / async / io / math` 完成
* `mine.platform.abi` + `mine.platform.win32` 最小消息泵 + 单窗口
* `mine.gfx.rhi.api` + `mine.gfx.d3d11` 能清屏一帧
* `mine.paint` 能画矩形 / 圆角矩形 / 画刷
* 最小 `Hello World`：手写 C++ 创建窗口 + 渲染一个矩形

## M1 — UI 内核与 MML 雏形

* `mine.ui.property / binding / event / input / layout / visual / window / app`
* `mine.ui.controls.basic`（Button、TextBlock、Border、StackPanel、Grid）
* `mml.lex / parse / ast` 完成；`mml.sema / codegen.cpp` 支持组件、属性、绑定（OneWay）
* `tool.mmlc` 最小可用，xmake rule 集成
* 示例：`samples/01-hello-world` 用 MML 写完

## M2 — MVVM、DI、控件

* `mine.di / mvvm / nav / config / localization`
* MML：双向绑定、事件、状态/动画基础
* `controls.input / collection / dialog / nav` 完成
* `mine.ui.style / animation`
* 示例：`samples/02-mvvm-todo`

## M3 — 数据与网络

* `mine.data.sqlite / orm / migrate / cache`
* `mine.net.socket / tls / dns / http(1.1) / ws / rest`
* 示例：`samples/03-sqlite-app`、`samples/04-network-feed`

## M4 — 多窗口、跨平台

* `mine.gfx.d3d12` 主路径，D3D11 回退稳定
* macOS Metal、Linux Vulkan 后端 alpha
* 多窗口、多显示器、多 DPI 完整覆盖
* 示例：`samples/05-multiwindow`

## M5 — 工具链完善

* `mmlls` 全特性（跳转/悬停/引用/格式化/重命名/inlay/fix-it）
* `ext.vscode` 套壳发布
* `mineinst` 自举安装程序（Windows 优先）
* `minepack` SDK 打包，第一次发 `mineui-sdk-x.y.z`

## M6 — 性能与裁剪

* 函数级裁剪 / LTO / ICF 全链路打通
* `min/std/full` profile 验证体积指标
* 渲染线程并行录制、批合并、Atlas
* 文本完整功能（BiDi/emoji/连字）

## M7 — 高级

* `mine.mml.runtime / hotreload`：开发期热重载
* `controls.chart / docking / media`
* HTTP/2、QUIC（探索）
* 设计器 / 可视化预览（基于 mmlls + 框架自身）

## M8 — 移动端

* Android / iOS 后端
* 触屏手势 / 软键盘 / 生命周期对齐
* 移动端样例

## 持续

* ABI 稳定承诺（在每个 MAJOR 节点）
* 第三方语言绑定：C# / Python / Rust（基于 capi）
* Brand 与生态文档（`docs.mineui.dev`、模板 `mineui new`）
