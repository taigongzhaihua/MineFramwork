# MineUI 开发计划

> 本文档基于 [19-roadmap.md](docs/19-roadmap.md) 进一步细化为可执行任务。
> 所有任务遵循 [docs/18-coding-conventions.md](docs/18-coding-conventions.md) 的命名与代码规范。
> 提交消息遵循 [.github/instructions/git-commit.instructions.md](.github/instructions/git-commit.instructions.md)。

---

## 总体原则

1. **严格按层推进**：L0 → L1 → L2 → L3 → L4 → L5/6/7 → L8 → L9，禁止反向依赖。
2. **每个模块完成 = 头文件 + 实现 + 单元测试 + 设计文档对齐**，缺一不交付。
3. **每个里程碑结束做一次 ABI/体积/性能基线快照**，写入 `docs/baseline/Mn.md`。
4. **任何不能立即实现的接口** 用 `MINE_TODO_NOT_IMPLEMENTED()` 标记并登记到 issue，禁止用空函数静默通过。
5. **每完成一个子任务即提交一次 git**，遵循 `<type>(<scope>): <中文动词开头>` 规范。
6. **每个里程碑收尾时同步 `CHANGELOG.md`** 的 Unreleased 段落，并打 tag `vX.Y.Z-mN`。

---

## M0 — 框架骨架（最小闭环：手写 C++ 弹一个矩形窗口）

### M0.1 基础设施（L0）

| # | 任务 | 模块 | 验收 |
|---|------|------|------|
| 1 | `mine.core`：基础类型（`Result<T,E>`、`Status`、`StringView`、`Span`、`Pimpl<T>`）、断言宏 `MINE_ASSERT/MINE_CHECK`、`MINE_API` 导出宏、`[[nodiscard]]` 错误码 | `src/mine/core` | 单测覆盖 ≥80%；无 STL 异常依赖 |
| 2 | `mine.containers`：`Vector`、`HashMap`、`SmallVector`、`InlineString`、自定义分配器接口 | `src/mine/containers` | 与 STL 等价 API 子集；零隐式分配可关 |
| 3 | `mine.diag`：日志接口（无后端实现，仅汇聚口）、崩溃断言信息收集 | `src/mine/diag` | 与 `mine.logging`(L5) 解耦 |
| 4 | `mine.async`：`Task<T>`、`Awaitable`、`Executor` 接口、`SingleThreadExecutor` | `src/mine/async` | C++20 协程 demo 通过 |
| 5 | `mine.io`：`File`、`Path`、`Stream` 抽象，同步实现 + 异步包装 | `src/mine/io` | Win32 实现优先 |
| 6 | `mine.math`：向量/矩阵/几何（Rect/Size/Point）、颜色、变换 | `src/mine/math` | 与 DirectXMath 等价精度 |
| 7 | `mine.text`：UTF-8/16 编解码、字符迭代、换行规则前置 | `src/mine/text` | 暂不依赖 harfbuzz |
| 8 | `mine.reflect`：编译期反射（结构体字段枚举、属性元数据） | `src/mine/reflect` | 单测：宏注册 → 字段访问 |

**M0.1 出口**：所有 L0 模块可独立编译为 `.lib`，`xmake build mine.core mine.containers ...` 全部成功，单测 100% 通过。

提交序列建议：
```
feat(core): 实现 Result/Status 基础类型
feat(core): 添加 MINE_ASSERT/MINE_API 宏
feat(containers): 实现 Vector 与 SmallVector
feat(containers): 实现 HashMap
test(core): 补充 Result 与 Status 单测
... 以此类推
```

---

### M0.2 平台与图形（L1 + L2 最小子集）

| # | 任务 | 模块 | 验收 |
|---|------|------|------|
| 9  | `mine.platform.abi`：`IWindow`、`IApplicationHost`、`IClipboard`、`IDisplay`、事件循环抽象 | `src/mine/platform/abi` | 接口仅头文件，无实现 |
| 10 | `mine.platform.win32`：Win32 窗口创建、消息泵、DPI per-monitor v2、原始输入采集（KB/Mouse） | `src/mine/platform/win32` | 能弹一个空窗口、可关闭 |
| 11 | `mine.gfx.rhi`：抽象 `Device`、`SwapChain`、`CommandList`、`PipelineState`、资源描述符 | `src/mine/gfx/rhi` | 接口纯虚 + 工厂 |
| 12 | `mine.gfx.d3d11`：D3D11 后端实现，能创建设备/交换链/清屏 | `src/mine/gfx/d3d11` | 单元 demo：清成蓝色 |

**M0.2 出口**：能跑 `samples/00-blank-window`（空窗 + RHI 清屏）。

---

### M0.3 绘制与端到端（L3 入口）

| # | 任务 | 模块 | 验收 |
|---|------|------|------|
| 13 | `mine.paint`：`Canvas`、`Brush`（实心色/渐变占位）、`StrokeStyle`、矩形/圆角矩形/线段 | `src/mine/paint` | 与 RHI 解耦，可换后端 |
| 14 | 端到端样例 `samples/00-hello-rect`：手写 C++ 创建窗口 + RHI 清屏 + Paint 画一个圆角矩形 | `samples/00-hello-rect` | 60fps 稳定，关闭无泄漏 |

**M0 完成标志**：
- `xmake build && xmake run hello-rect` 可看到一个深色窗口中央带一个圆角矩形
- CI 矩阵（Windows + MSVC + Debug/Release）全绿
- `docs/baseline/M0.md` 记录二进制体积与启动耗时

git tag：`v0.1.0-m0`

---

## M1 — UI 内核与 MML 雏形（最小闭环：MML 写出 Hello World）

### M1.1 UI 核心（L4）

| # | 任务 | 模块 | 验收 |
|---|------|------|------|
| 15 | `mine.ui.property`：依赖属性系统（注册/读写/通知/默认值/继承） | `src/mine/ui/property` | WPF 风格 API |
| 16 | `mine.ui.binding`：`OneWay` 绑定，路径解析，`INotifyPropertyChanged` 等价接口 | `src/mine/ui/binding` | 单测覆盖路径绑定 |
| 17 | `mine.ui.event`：路由事件（Tunnel/Bubble/Direct）、命令接口 | `src/mine/ui/event` | 与 input 模块对接 |
| 18 | `mine.ui.input`：键盘/鼠标/触摸事件路由、命中测试 | `src/mine/ui/input` | 接收 Win32 输入并派发 |
| 19 | `mine.ui.visual`：`Visual` 基类、视觉树、变换、剪裁 | `src/mine/ui/visual` | 渲染管线接入 paint |
| 20 | ✅ `mine.ui.layout`：`Measure/Arrange` 协议、`StackPanel`、`Grid` | `src/mine/ui/layout` | 经典布局算法 |
| 21 | `mine.ui.window`：`Window` 类、生命周期、与 PAL 桥接 | `src/mine/ui/window` | 多实例支持 |
| 22 | `mine.ui.app`：`Application`、主循环、退出码 | `src/mine/ui/app` | `MINE_APPLICATION_MAIN` 宏 |
| 23 | `mine.ui.controls.basic`：`TextBlock`、`Button`、`Border`、`StackPanel`、`Grid` | `src/mine/ui/controls` | 渲染 + 命中测试通过 |

### M1.2 MML 编译器最小子集（L8 + L9）

| # | 任务 | 模块 | 验收 |
|---|------|------|------|
| 24 | `mine.mml.lex`：词法分析（关键字、标识符、字面量、运算符） | `src/mine/mml/lex` | token 流测试 |
| 25 | `mine.mml.parse` + `mine.mml.ast`：语法分析，组件/属性/绑定 AST | `src/mine/mml/parse` | 解析 hello.mml |
| 26 | `mine.mml.sema`：符号表、类型推断、属性合法性检查 | `src/mine/mml/sema` | 类型错误诊断 |
| 27 | `mine.mml.diagnostics`：诊断聚合、行列定位、错误格式化 | `src/mine/mml/diagnostics` | 类 rustc 风格 |
| 28 | `mine.mml.lower` + `mine.mml.codegen.cpp`：AST → 降级 IR → C++ 代码 | `src/mine/mml/codegen/cpp` | 输出 `.g.h/.g.cpp` |
| 29 | `tool.mmlc`：CLI 包装，支持 `--depfile -o --vm-include` | `tools/mmlc` | xmake `mine.mml` rule 跑通 |

### M1.3 端到端

| # | 任务 | 路径 | 验收 |
|---|------|------|------|
| 30 | 样例 `samples/01-hello-world`：用 `.mml` 描述一个带按钮的窗口 + OneWay 绑定 | `samples/01-hello-world` | mmlc 生成代码 + UI 控件渲染正确 |

**M1 完成标志**：能用 MML 写出一个简单页面、xmake 一键编译运行。

git tag：`v0.2.0-m1`

---

## M2 — MVVM、DI、控件完整化

### M2.1 DI 与 MVVM（L5）

| # | 任务 | 模块 |
|---|------|------|
| 31 | `mine.di`：`ServiceCollection`、`ServiceProvider`、`Scoped/Singleton/Transient`、`MINE_INJECT` 宏 | `src/mine/di` |
| 32 | `mine.mvvm`：`ViewModelBase`、`ObservableObject`、`ObservableCollection<T>`、`RelayCommand` | `src/mine/mvvm` |
| 33 | `mine.nav`：`Frame`、`Page`、`NavigationService`、参数传递 | `src/mine/nav` |
| 34 | `mine.config`：分层配置（默认/用户/环境）+ JSON/TOML 加载 | `src/mine/config` |
| 35 | `mine.localization`：资源字典、运行时切换、MML `tr()` 函数 | `src/mine/localization` |
| 36 | `mine.logging`：文件/控制台 sink、级别过滤、对接 `mine.diag` | `src/mine/logging` |

### M2.2 MML 进阶 + 控件 + 样式 + 动画

| # | 任务 | 模块 |
|---|------|------|
| 37 | MML：`<=>` 双向绑定、事件订阅、`if/for` 模板、`states` 状态机 | `mine.mml.*` |
| 38 | `mine.ui.style`：样式、控件模板、主题（详见下方子任务） | `src/mine/ui/style` |
| 39 | `mine.ui.animation`：缓动函数、过渡、`Storyboard` | `src/mine/ui/animation` |
| 40 | 控件扩展：`TextBox/CheckBox/RadioButton/Slider/ComboBox/ListView/TabControl/Dialog` | `src/mine/ui/controls` |
| 41 | 样例 `samples/02-mvvm-todo`：完整 MVVM Todo 应用 | `samples/02-mvvm-todo` |

#### 任务 #38 子任务分解（mine.ui.style）

> 完整架构设计见 [20-style-template.md](20-style-template.md)，本处为可执行子任务拆分。
> 按阶段 A → B → C 顺序推进，每个阶段均可独立交付。

**阶段 A：ResourceDictionary + 主题切换**（优先，为控件颜色主题化奠基）

| # | 子任务 | 路径 | 验收 |
|---|--------|------|------|
| 38-A1 | `ResourceDictionary`：本地存储、祖先查找（树形作用域）、`resource_changed` 信号 | `src/mine/ui/style/ResourceDictionary.h/.cpp` | 单测：跨层查找、变更通知 |
| 38-A2 | `Application::set_theme()`：合并/切换主题资源字典；`Application` 持有全局 `ResourceDictionary` | `src/mine/ui/app` | 运行时切换浅色/深色无崩溃 |
| 38-A3 | MML `resources { theme ... }` 块 → mmlc 生成 C++ 主题资源注册代码（`.g.cpp`） | `mine.mml.codegen` | Theme.mml 编译后能注册颜色资源 |
| 38-A4 | `Control::set_style_dynamic()`：DynamicResource 订阅机制（属性值随主题变化自动更新） | `src/mine/ui/style/Style.h/.cpp` | Button 背景色随主题切换更新 |

**阶段 B：Style 系统**

| # | 子任务 | 路径 | 验收 |
|---|--------|------|------|
| 38-B1 | `StyleSetter` / `VisualStateSetters` 结构体；`Style` 类（target_type、base、setters）；`StyleRegistry` 单例 | `src/mine/ui/style/Style.h/.cpp`、`StyleRegistry.h/.cpp` | 单测：注册 → 查找 → apply |
| 38-B2 | `Control::set_style_value()`（优先级 5）；依赖属性值优先级链扩展（加入样式层） | `src/mine/ui/property` + `src/mine/ui/style` | 样式 setter 不覆盖本地值 |
| 38-B3 | `Control::update_visual_state()` + `compute_visual_state()` 默认实现（Normal/Hovered/Pressed/Focused/Disabled） | `src/mine/ui/style/Control.h` | Button 状态切换触发状态 setter |
| 38-B4 | MML `style X for T { ... :hover { } }` 块 → mmlc 生成 `apply_X_impl()` 函数 + `StyleRegistry::register_style()` 静态注册 | `mine.mml.codegen` | AccentButton.style.g.cpp 可编译 |
| 38-B5 | 隐式默认样式：控件创建时自动查找 `Default<TypeName>` 样式应用 | `src/mine/ui/style/Control.cpp` | 新建 Button 自动应用 DefaultButton 样式 |

**阶段 C：ControlTemplate + VisualStateManager**

| # | 子任务 | 路径 | 验收 |
|---|--------|------|------|
| 38-C1 | `ContentPresenter`：支持 string（内联 TextBlock）和 FrameworkElement 两种内容类型 | `src/mine/ui/controls/basic/ContentPresenter.h/.cpp` | TextBlock 和 Button 内容都能正确渲染 |
| 38-C2 | `ControlTemplate` / `TemplateRegistry`；`Control::set_template_root()` / `bind_template()` / `find_template_child()` | `src/mine/ui/style/ControlTemplate.h/.cpp` | 单测：build → bind → find |
| 38-C3 | `VisualStateManager`：状态定义、过渡配置（`add_transition()`）、`go_to_state()`；与 `Storyboard` 集成 | `src/mine/ui/style/VisualStateManager.h/.cpp` | Normal→Hovered 过渡动画播放 |
| 38-C4 | MML `template X for T { ... states { ... } }` 块 → mmlc 生成 `build_X_impl()` 函数 | `mine.mml.codegen` | DefaultButton.template.g.cpp 可编译 |
| 38-C5 | Button 迁移：新增 `DefaultButton.mml`（含 style + template），`on_render()` 走模板树渲染（保留手绘兜底） | `src/mine/ui/controls/basic` | samples/01-hello-world 视觉不变 |
| 38-C6 | 其余控件（CheckBox/RadioButton/Slider/ComboBox/TabControl）同步迁移 | `src/mine/ui/controls/input`、`nav` | sample-02-mvvm-todo 视觉不变 |

**M2 阶段不实现（延迟到 M3+）**：类选择器（`.class`）、DataTrigger、控件模板热重载。

git tag：`v0.3.0-m2`

---

## M3 — 数据与网络

### M3.1 数据层（L6）

| # | 任务 | 模块 |
|---|------|------|
| 42 | `mine.data.sqlite`：连接池、参数化查询、事务、prepared statement | `src/mine/data/sqlite` |
| 43 | `mine.data.orm`：基于 `mine.reflect` 的实体映射、查询 DSL | `src/mine/data/orm` |
| 44 | `mine.data.migrate`：版本化迁移脚本执行 | `src/mine/data/migrate` |
| 45 | `mine.data.cache`：键值缓存（LRU/TTL）、SQLite 持久化 | `src/mine/data/cache` |
| 46 | 样例 `samples/03-sqlite-app`：SQLite 增删改查 + ORM 示例 | `samples/03-sqlite-app` |

### M3.2 网络层（L7）

| # | 任务 | 模块 |
|---|------|------|
| 47 | `mine.net.socket`：异步 TCP/UDP，IO Completion Port (Win32) | `src/mine/net/socket` |
| 48 | `mine.net.tls`：mbedTLS 包装，握手、证书校验 | `src/mine/net/tls` |
| 49 | `mine.net.dns`：异步 DNS 解析、缓存 | `src/mine/net/dns` |
| 50 | `mine.net.http`：HTTP/1.1 客户端、连接复用〔Transfer-Encoding | `src/mine/net/http` |
| 51 | `mine.net.ws`：WebSocket（RFC 6455）、ping/pong、分片 | `src/mine/net/ws` |
| 52 | `mine.net.rest`：基于 reflect 的请求/响应序列化 DSL | `src/mine/net/rest` |
| 53 | 样例 `samples/04-network-feed`：HTTP + WebSocket 实时推送示例 | `samples/04-network-feed` |

git tag：`v0.4.0-m3`

---

## M4 — 多窗口、跨平台

| # | 任务 | 模块 |
|---|------|------|
| 54 | `mine.gfx.d3d12` 成主路径，D3D11 仅作回退 | `src/mine/gfx/d3d12` |
| 55 | macOS：`mine.platform.macos` + `mine.gfx.metal`（alpha） | `platform/macos`, `gfx/metal` |
| 56 | Linux：`mine.platform.linux`（X11/Wayland 二选一）+ `mine.gfx.vulkan`（alpha） | `platform/linux`, `gfx/vulkan` |
| 57 | 多窗口/多显示器/多 DPI 端到端测试 | `tests/` |
| 58 | 样例 `samples/05-multiwindow` | `samples/05-multiwindow` |

git tag：`v0.5.0-m4`

---

## M5 — 工具链完善

| # | 任务 | 模块 |
|---|------|------|
| 58 | `tool.mmlls`：LSP 全特性（goto/hover/refs/format/rename/inlay/code-action） | `tools/mmlls` |
| 59 | `tool.mmlfmt`：基于 AST 的格式化 | `tools/mmlfmt` |
| 60 | `ext.vscode`：VSCode 插件套壳 mmlls，语法高亮、片段、调试集成 | `ext/vscode` |
| 61 | `tool.mineinst`：Windows 自举安装器，PATH 注册、SDK 解包 | `tools/mineinst` |
| 62 | `tool.minepack`：SDK 打包脚本，产物清单生成 | `tools/minepack` |
| 63 | 首次发布 `mineui-sdk-0.6.0` | release |

git tag：`v0.6.0-m5`

---

## M6 — 性能与裁剪

| # | 任务 |
|---|------|
| 64 | 验证 `/Gy /Gw /OPT:REF,ICF` 在 MSVC 下的死码消除率 |
| 65 | 验证 `--gc-sections` + `-flto=thin` 在 Clang 下的裁剪率 |
| 66 | 实现并验证 `mine_profile = min/std/full` 的体积梯度（目标：min < 2 MB） |
| 67 | 渲染：多线程命令录制、批合并、纹理 Atlas 自动打包 |
| 68 | 文本完整功能：BiDi、Emoji、连字、复杂脚本（HarfBuzz 集成） |
| 69 | 微基准与回归基准（nanobench）入 CI |

git tag：`v0.7.0-m6`

---

## M7 — 高级特性

| # | 任务 |
|---|------|
| 70 | `mine.mml.runtime` + `mine.mml.hotreload`：开发期热重载 |
| 71 | `controls.chart / docking / media` |
| 72 | HTTP/2 支持（探索 QUIC） |
| 73 | 设计器（基于 mmlls + 自身渲染） |

git tag：`v0.8.0-m7`

---

## M8 — 移动端

| # | 任务 |
|---|------|
| 74 | `mine.platform.android` + `mine.gfx.gles` / Vulkan |
| 75 | `mine.platform.ios` + Metal 移动 |
| 76 | 触屏手势 / 软键盘 / 生命周期对齐 |
| 77 | 移动端样例 |

git tag：`v0.9.0-m8`

---

## v1.0：稳定 ABI

- 冻结 `mine.core` / `mine.platform.abi` / `mine.gfx.rhi` 公共头
- C API（`include/mine/capi/`）冻结
- 发布语言绑定：C# / Python / Rust（基于 capi）

git tag：`v1.0.0`

---

## 持续工作流（每个里程碑均执行）

1. **CI**：每个 PR 必跑 fmt / lint / build / test / 二进制体积检查
2. **审查**：所有公共头修改必须有对应设计文档更新
3. **基线**：`docs/baseline/Mn.md` 记录二进制体积、启动耗时、首帧耗时、内存峰值
4. **CHANGELOG**：每个里程碑结束前合并 Unreleased 段落
5. **依赖审查**：每月检查第三方库版本与 CVE
6. **文档同步**：API 改动必须同时改 `docs/` 与 IDE 注释

---

## 风险与对策

| 风险 | 对策 |
|------|------|
| MML AOT 表达力不足以替代运行时 VM | 在 M1 末期固化 MML 子集，新增能力走标记宏路径 |
| 函数级裁剪在 GCC 上不稳定 | 优先 Clang/LLD，GCC 仅作兼容备份 |
| 多平台后端进度失衡 | M4 之前其它平台保持 alpha 标签，不进入 SDK 主线 |
| ABI 稳定与功能迭代冲突 | v1.0 之前不承诺 ABI；之后按 SemVer 严格执行 |
| 第三方依赖（mbedTLS/HarfBuzz/SQLite）许可与版本碎片 | 全部 vendor 进 `third_party/`，xmake.requires 锁版本 |

---

## 当前位置

- ✅ 项目骨架、设计文档、构建系统、git 提交规范、初始提交（`e54af8a`）已完成
- 🚀 **下一步：开始 M0.1 — 实现 `mine.core`**
