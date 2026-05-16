# 02 — 模块清单

> 命名规则：`mine.<layer>.<feature>[.<backend>]`
> 每个模块在 `xmake.lua` 中是一个独立 `target`，并伴随 `mine.<x>.api` 接口子目标。

## L0 Foundation

| 模块 | 职责 | 关键类型/API |
|------|------|--------------|
| `mine.core` | 基本类型、内存、Result | `Result<T,E>`, `Status`, `Allocator`, `Pimpl<T>`, `Span<T>`, `String`, `StringView`, `Optional<T>`, `Variant<...>` |
| `mine.containers` | 高性能容器 | `Vector<T>`, `SmallVector<T,N>`, `HashMap<K,V>`, `IntrusiveList`, `Slot​Map<T>`, `RingBuffer<T>` |
| `mine.diag` | 诊断 | `Logger`, `MINE_ASSERT`, `Trace::Scope`, `PerfCounter` |
| `mine.async` | 任务调度 | `Task<T>`(协程), `Future<T>`, `Dispatcher`, `ThreadPool`, `Timer` |
| `mine.io` | 文件/IO | `File`, `Path`, `FileSystem`, `MemMap`, `PipeStream` |
| `mine.text` | Unicode | `Utf8`, `Utf16`, `GraphemeIterator`, `Normalize` |
| `mine.math` | 数学 | `Vec2/3/4`, `Mat3/4`, `Rect`, `RoundedRect`, `Color`, `Transform2D` |
| `mine.reflect` | 静态反射 | `MINE_REFLECT(...)`, `TypeInfo`, `PropertyInfo`, `MethodInfo` |
| `mine.meta` | 类型 ID/工厂/序列化 | `TypeId`, `Factory<T>`, `Serializer`, `JsonReader/Writer` |

## L1 Platform

| 模块 | 职责 |
|------|------|
| `mine.platform.abi` | 平台抽象接口（窗口、消息泵、剪贴板、屏幕、DPI、IME） |
| `mine.platform.win32` | Win32 实现 |
| `mine.platform.macos` | Cocoa/AppKit 实现 |
| `mine.platform.linux` | X11 + Wayland 双后端 |
| `mine.platform.android` | NativeActivity / Choreographer |
| `mine.platform.ios` | UIKit |
| `mine.io.fs` | VFS（虚拟文件系统）、可挂载资源包；依赖 `mine.platform.abi` 的文件抽象 |

## L2 Graphics

| 模块 | 职责 |
|------|------|
| `mine.gfx.rhi.api` | RHI 抽象（设备、队列、命令、缓冲、纹理、管线、Swapchain） |
| `mine.gfx.d3d11` | D3D11 后端（Win 默认低端回退） |
| `mine.gfx.d3d12` | D3D12 后端（Win 默认） |
| `mine.gfx.metal` | macOS/iOS |
| `mine.gfx.vulkan` | Linux/Android/Win 备选 |
| `mine.gfx.gles` | 老旧 Android |

> **`mine.gfx.shadercc`**（着色器跨编译：HLSL → SPIRV/MSL/DXIL）是**构建期工具**，不参与运行时模块依赖图。其实体形态归入 L9 Tooling（`tool.shadercc`）；在 xmake 构建图中作为 gfx 后端目标的前置依赖执行，运行包中不包含此模块。

## L3 Composition / 2D

| 模块 | 职责 |
|------|------|
| `mine.paint` | 画刷、画笔、几何、Path、SkiaPath 风格 API |
| `mine.compose` | 视觉合成树、Layer、变换、效果、脏区、合成器线程 |
| `mine.text.shape` | 文本整形（HarfBuzz 适配） |
| `mine.text.layout` | 行排版、富文本块、Hit-test |
| `mine.image` | 位图、解码、缓存 |
| `mine.svg` | SVG 解析 + 转 path（可裁剪） |
| `mine.effects` | 模糊、阴影、滤镜、合成模式 |

## L4 UI Core

| 模块 | 职责 |
|------|------|
| `mine.ui.property` | 依赖属性系统、附加属性、值优先级、`PropertyChanged` |
| `mine.ui.binding` | 绑定引擎、表达式、转换器、双向绑定 |
| `mine.ui.event` | 路由事件、命令、隧道/冒泡 |
| `mine.ui.input` | 键鼠/触摸/手写笔/手势识别/快捷键 |
| `mine.ui.layout` | Measure/Arrange、Stack/Grid/Flex/Wrap/Dock/Canvas/Virtual |
| `mine.ui.visual` | `Visual`/`UIElement`/`Control` 基类 |
| `mine.ui.style` | 样式（`Style`/`StyleSetter`/状态伪类/`BasedOn` 继承）、控件模板（`ControlTemplate` AOT `build_fn_`）、`ResourceDictionary`（树形作用域）、`VisualStateManager`（状态驱动过渡动画）、`StyleRegistry`/`TemplateRegistry`（静态注册）、主题切换（`Application::set_theme()`）、`ContentPresenter` |
| `mine.ui.animation` | 时间线、缓动、Storyboard、隐式动画 |
| `mine.ui.controls` | 标准控件库（按需子模块拆分，详见 §2.x） |
| `mine.ui.window` | Window/Dialog/Popup/MenuFlyout |
| `mine.ui.app` | Application、生命周期、单实例、UnhandledException |

### 2.x 标准控件子拆分

控件按使用频率拆分为更细的子库，便于裁剪：

* `controls.basic`：Button、TextBlock、Image、Border、ContentControl
* `controls.input`：TextBox、PasswordBox、CheckBox、RadioButton、Slider
* `controls.collection`：ItemsControl、ListView、TreeView、DataGrid（重）、Carousel
* `controls.nav`：TabView、NavigationView、Pager、BreadCrumb
* `controls.dialog`：MessageDialog、ContentDialog、Flyout、Tooltip
* `controls.media`：MediaElement（依赖平台 codec）
* `controls.chart`：折线/柱状/饼图（独立模块）
* `controls.docking`：MDI/Docking（独立模块）

## L5 App Services

| 模块 | 职责 |
|------|------|
| `mine.di` | IoC 容器，编译期 + 运行期混合 |
| `mine.mvvm` | `ViewModelBase`, `ICommand`, `RelayCommand`, `Validation` |
| `mine.nav` | 路由、回退栈、Frame、参数传递 |
| `mine.localization` | i18n、复数规则、RTL |
| `mine.config` | 用户设置、JSON/TOML/INI 持久化 |
| `mine.logging` | 面向产品的日志 sink |

## L6 Data

| 模块 | 职责 |
|------|------|
| `mine.data.sqlite` | SQLite 高层封装 |
| `mine.data.orm` | 元数据驱动 ORM、查询构建器 |
| `mine.data.cache` | 内存/磁盘缓存（LRU、ARC） |
| `mine.data.migrate` | Schema 迁移 |

## L7 Networking

| 模块 | 职责 |
|------|------|
| `mine.net.socket` | TCP/UDP/Unix |
| `mine.net.tls` | TLS 适配（mbedTLS / OpenSSL 二选一） |
| `mine.net.dns` | 异步 DNS |
| `mine.net.http` | HTTP/1.1, HTTP/2 客户端 + 简易服务端 |
| `mine.net.ws` | WebSocket |
| `mine.net.rest` | REST 高层封装、模型映射 |
| `mine.net.rpc` | 自定义二进制 RPC（基于 Protobuf 风格的 IDL，可选） |

## L8 Markup

| 模块 | 职责 |
|------|------|
| `mine.mml.lex` | 词法分析器 |
| `mine.mml.parse` | 语法分析器（手写下降 + Pratt） |
| `mine.mml.ast` | 抽象语法树定义 |
| `mine.mml.sema` | 类型/绑定/符号语义分析 |
| `mine.mml.diagnostics` | 诊断（错误/警告/位置/修复建议） |
| `mine.mml.codegen.cpp` | C++ 代码生成器 |
| `mine.mml.runtime` | 运行期反射桥（仅热重载/设计器开启） |
| `mine.mml.hotreload` | 热重载协议 |

## L9 Tooling

| 模块 | 类型 | 职责 |
|------|------|------|
| `tool.mmlc` | exe | 预编译器 CLI |
| `tool.mmlfmt` | exe | 格式化 |
| `tool.mmlls` | exe | LSP（C++ 实现） |
| `tool.shadercc` | exe | 构建期着色器跨编译（HLSL → DXIL/SPIRV/MSL），仅构建时调用，不进运行时 |
| `tool.minepack` | exe | SDK 打包 / 资源打包 |
| `tool.mineinst` | exe | 安装程序（用 MineUI 自举） |
| `ext.vscode` | npm 包 | VSCode 扩展 TS 套壳 |

## 2.y 模块依赖矩阵（节选）

```
ui.controls.collection → ui.controls.basic → ui.visual → compose → paint → gfx.rhi.api
ui.binding             → ui.property → reflect → core
mvvm                   → ui.binding, ui.event, di, async
mml.codegen.cpp        → mml.sema → mml.parse → mml.lex → core
data.orm               → data.sqlite → core
net.http               → net.tls → net.socket → async → core
```
