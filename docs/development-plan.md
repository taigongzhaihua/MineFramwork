# MineUI 开发计划

> 本文档基于 [19-roadmap.md](19-roadmap.md) 进一步细化为可执行任务。
> 所有任务遵循 [18-coding-conventions.md](18-coding-conventions.md) 的命名与代码规范。
> 提交消息遵循 [.github/instructions/git-commit.instructions.md](../.github/instructions/git-commit.instructions.md)。

---

## 总体原则

1. **严格按层推进**：L0 → L1 → L2 → L3 → L4 → L5/6/7 → L8 → L9，禁止反向依赖。
2. **每个模块完成 = 头文件 + 实现 + 单元测试 + 设计文档对齐**，缺一不交付。
3. **任何不能立即实现的接口** 用 `MINE_TODO_NOT_IMPLEMENTED()` 标记并登记 issue，禁止空函数静默通过。
4. **每完成一个子任务即提交一次 git**，遵循 `<type>(<scope>): <中文动词开头>` 规范。
5. **每个里程碑收尾时同步 `CHANGELOG.md`** 的 Unreleased 段落，并打 tag。
6. **每个里程碑结束做一次 ABI/体积/性能基线快照**，写入 `docs/baseline/Fn.md` 或 `Tn.md`。

---

## 轨道划分

本计划分为两条**完全独立**的开发轨道：

| 轨道 | 标识 | 内容 | 前置条件 |
|------|------|------|----------|
| **主框架轨道** | `F0–F7` | UI 内核、控件、样式、数据、网络、多平台、性能 | 无 |
| **MML 工具链轨道** | `T1–T4` | 词法/语法/语义/代码生成、CLI 工具、LSP、IDE 插件 | F1 稳定后可并行 |

两条轨道**互不干扰**。主框架所有里程碑均不依赖 MML 工具链；MML 工具链依赖主框架的公共 API，但不反向影响主框架的开发节奏。

---

## 主框架开发轨道

---

### F0 ✅ — 框架骨架

**目标**：手写 C++ 能弹出一个矩形窗口（60fps，无泄漏）。

**已完成模块：**

| 阶段 | 模块 | 路径 |
|------|------|------|
| F0.1 基础设施 | `mine.core`、`mine.containers`、`mine.math` | `src/mine/core`, `containers`, `math` |
| F0.2 平台与图形 | `mine.platform.abi`、`mine.platform.win32`、`mine.gfx.rhi`、`mine.gfx.d3d11` | `src/mine/platform`, `gfx` |
| F0.3 绘制与端到端 | `mine.paint`、`samples/00-hello-rect` | `src/mine/paint`, `samples/00-hello-rect` |

git tag：`v0.1.0-f0`

---

### F1 ✅ — UI 核心

**目标**：完整的 UI 控件基础栈（属性、事件、布局、控件、应用程序主循环）可用。

**已完成模块：**

| # | 模块 | 路径 |
|---|------|------|
| 1 ✅ | `mine.ui.property`：依赖属性系统（注册/读写/通知/默认值/继承） | `src/mine/ui/property` |
| 2 ✅ | `mine.ui.binding`：`OneWay` 绑定、路径解析、`INotifyPropertyChanged` 等价接口 | `src/mine/ui/binding` |
| 3 ✅ | `mine.ui.event`：路由事件（Tunnel/Bubble/Direct）、命令接口 | `src/mine/ui/event` |
| 4 ✅ | `mine.ui.input`：键盘/鼠标事件路由、命中测试 | `src/mine/ui/input` |
| 5 ✅ | `mine.ui.visual`：`Visual` 基类、视觉树、变换、剪裁、`Control` 基类 | `src/mine/ui/visual` |
| 6 ✅ | `mine.ui.layout`：`Measure/Arrange` 协议、`StackPanel`、`Grid` | `src/mine/ui/layout` |
| 7 ✅ | `mine.ui.window`：`Window` 类、生命周期、与 PAL 桥接 | `src/mine/ui/window` |
| 8 ✅ | `mine.ui.app`：`Application`、主循环、退出码、`MINE_APPLICATION_MAIN` 宏 | `src/mine/ui/app` |
| 9 ✅ | `mine.ui.controls`（基础）：`TextBlock`、`Button`、`Border`；样式/模板/视觉状态挂点 | `src/mine/ui/controls` |

git tag：`v0.2.0-f1`

---

### F2 — 样式与动画

**目标**：控件能通过样式系统换肤；状态变化有平滑过渡动画。

#### F2.1 样式系统

| # | 模块 | 路径 | 验收 |
|---|------|------|------|
| 10 | `mine.ui.style`：`ResourceDictionary`（树形作用域、`resource_changed` 信号） | `src/mine/ui/style` | 单测：跨层查找、变更通知 |
| 11 | `mine.ui.style`：`Style` + `StyleSetter`；依赖属性值优先级链（样式层） | `src/mine/ui/style` | 样式 setter 不覆盖本地值 |
| 12 | `mine.ui.style`：`ControlTemplate` + `TemplateRegistry`；`bind_template` / `find_template_child` | `src/mine/ui/style` | 单测：build → bind → find |
| 13 | `mine.ui.style`：`VisualStateManager`：状态定义、过渡配置、`go_to_state()` | `src/mine/ui/style` | Normal→Hovered 正确触发 |
| 14 | `mine.ui.app`：`Application::set_theme()`：合并/切换主题资源字典 | `src/mine/ui/app` | 运行时切换浅色/深色无崩溃 |
| 15 | `mine.ui.controls`：`ContentPresenter`；Button/TextBlock 迁移至样式模板路径 | `src/mine/ui/controls` | 视觉效果与迁移前一致 |

#### F2.2 动画系统

| # | 模块 | 路径 | 验收 |
|---|------|------|------|
| 16 | `mine.ui.animation`：缓动函数库（Linear / Ease / Spring） | `src/mine/ui/animation` | 单测：帧序列值正确 |
| 17 | `mine.ui.animation`：`Storyboard`、`Timeline`、属性动画 | `src/mine/ui/animation` | 与 `VisualStateManager` 过渡集成 |

git tag：`v0.3.0-f2`

---

### F3 — MVVM + DI + 控件完整化

**目标**：完整的应用层框架；控件覆盖常用交互组件。

#### F3.1 MVVM 与 DI

| # | 模块 | 路径 |
|---|------|------|
| 18 | `mine.di`：`ServiceCollection`、`ServiceProvider`、`Scoped/Singleton/Transient`、`MINE_INJECT` 宏 | `src/mine/di` |
| 19 | `mine.mvvm`：`ViewModelBase`、`ObservableObject`、`ObservableCollection<T>`、`RelayCommand` | `src/mine/mvvm` |
| 20 | `mine.nav`：`Frame`、`Page`、`NavigationService`、参数传递 | `src/mine/nav` |
| 21 | `mine.config`：分层配置（默认/用户/环境）+ JSON/TOML 加载 | `src/mine/config` |
| 22 | `mine.localization`：资源字典、运行时语言切换、`tr()` 函数 | `src/mine/localization` |
| 23 | `mine.logging`：文件/控制台 sink、级别过滤、对接 `mine.diag` | `src/mine/logging` |

#### F3.2 控件完整化

| # | 模块 | 路径 |
|---|------|------|
| 24 | `mine.ui.controls`：`TextBox`、`CheckBox`、`RadioButton`、`Slider`、`ProgressBar` | `src/mine/ui/controls` |
| 25 | `mine.ui.controls`：`ComboBox`、`ListBox`、`ListView`（虚拟化） | `src/mine/ui/controls` |
| 26 | `mine.ui.controls`：`TabControl`、`Expander`、`ScrollViewer` | `src/mine/ui/controls` |
| 27 | `mine.ui.controls`：`Dialog`（模态/非模态）、`MessageBox`、`Popup` | `src/mine/ui/controls` |

#### F3.3 端到端样例

| # | 样例 | 路径 |
|---|------|------|
| 28 | `samples/01-pure-cpp`：纯 C++ 实现的 MVVM Todo 应用（无 MML 依赖） | `samples/01-pure-cpp` |

git tag：`v0.4.0-f3`

---

### F4 — 数据层

**目标**：本地结构化数据的完整读写能力。

| # | 模块 | 路径 | 验收 |
|---|------|------|------|
| 29 | `mine.data.sqlite`：连接池、参数化查询、事务、prepared statement | `src/mine/data/sqlite` | 单测覆盖 CRUD |
| 30 | `mine.data.orm`：基于 `mine.reflect` 的实体映射、查询 DSL | `src/mine/data/orm` | 宏注册 → 查询端到端 |
| 31 | `mine.data.migrate`：版本化迁移脚本执行 | `src/mine/data/migrate` | 版本跳跃测试通过 |
| 32 | `mine.data.cache`：键值缓存（LRU/TTL）、SQLite 持久化 | `src/mine/data/cache` | TTL 过期自动淘汰 |
| 33 | `samples/02-sqlite-app`：SQLite 增删改查 + ORM 示例 | `samples/02-sqlite-app` | — |

git tag：`v0.5.0-f4`

---

### F5 — 网络层

**目标**：异步 HTTP / WebSocket 能力。

| # | 模块 | 路径 | 验收 |
|---|------|------|------|
| 34 | `mine.net.socket`：异步 TCP/UDP，IOCP（Win32） | `src/mine/net/socket` | echo server 单测 |
| 35 | `mine.net.tls`：mbedTLS 包装，握手、证书校验 | `src/mine/net/tls` | TLS 握手成功 |
| 36 | `mine.net.dns`：异步 DNS 解析、缓存 | `src/mine/net/dns` | 解析 + 缓存命中 |
| 37 | `mine.net.http`：HTTP/1.1 客户端、连接复用 | `src/mine/net/http` | GET/POST 端到端 |
| 38 | `mine.net.ws`：WebSocket（RFC 6455）、ping/pong、分片 | `src/mine/net/ws` | echo ws 单测 |
| 39 | `mine.net.rest`：基于 reflect 的请求/响应序列化 DSL | `src/mine/net/rest` | JSON 反序列化为实体 |
| 40 | `samples/03-network-feed`：HTTP + WebSocket 实时推送示例 | `samples/03-network-feed` | — |

git tag：`v0.6.0-f5`

---

### F6 — 多平台

**目标**：Windows 以外的主流平台可运行。

| # | 任务 | 路径 |
|---|------|------|
| 41 | `mine.gfx.d3d12` 升为主路径，D3D11 仅作回退 | `src/mine/gfx/d3d12` |
| 42 | macOS：`mine.platform.macos` + `mine.gfx.metal`（alpha） | `src/mine/platform/macos` |
| 43 | Linux：`mine.platform.linux`（X11/Wayland）+ `mine.gfx.vulkan`（alpha） | `src/mine/platform/linux` |
| 44 | 多窗口 / 多显示器 / 多 DPI 完整覆盖 | `tests/` |
| 45 | `samples/04-multiwindow`：多窗口示例 | `samples/04-multiwindow` |

git tag：`v0.7.0-f6`

---

### F7 — 性能与高级特性

| # | 任务 |
|---|------|
| 46 | `/Gy /Gw /OPT:REF,ICF` MSVC 死码消除验证；`--gc-sections -flto=thin` Clang 裁剪验证 |
| 47 | `mine_profile = min/std/full` 体积梯度验证（目标：min < 2 MB） |
| 48 | 渲染：多线程命令录制、批合并、纹理 Atlas 自动打包 |
| 49 | 文本：BiDi、Emoji、连字、复杂脚本（HarfBuzz 集成） |
| 50 | 微基准与回归基准（nanobench）入 CI |
| 51 | `mine.mml.runtime` + `mine.mml.hotreload`（依赖 T 轨道完成） |

git tag：`v0.8.0-f7`

---

## MML 工具链开发轨道

> **启动前置条件**：F1 完成（UI 内核稳定）后，T1 可独立并行推进。
> 工具链不修改、不依赖主框架的内部实现；只依赖主框架的公共 API 头文件。

---

### T1 — 编译器核心

**目标**：能将一个简单 `.mml` 文件编译为可直接集成到 C++ 项目的 `.g.h/.g.cpp`。

| # | 模块 | 路径 | 验收 |
|---|------|------|------|
| T1 | `mine.mml.lex`：词法分析（关键字、标识符、字面量、运算符、注释） | `src/mine/mml/lex` | token 流单测 |
| T2 | `mine.mml.parse` + `mine.mml.ast`：语法分析，组件 / 属性 / 单向绑定 AST | `src/mine/mml/parse` | 解析最小 .mml 无错误 |
| T3 | `mine.mml.sema`：符号表、类型推断、属性合法性检查 | `src/mine/mml/sema` | 错误属性能精确报错 |
| T4 | `mine.mml.diagnostics`：诊断聚合、行列定位、错误格式化（类 rustc 风格） | `src/mine/mml/diagnostics` | 格式化输出含文件名 + 行列 |
| T5 | `mine.mml.lower` + `mine.mml.codegen.cpp`：AST → 降级 IR → `.g.h/.g.cpp` | `src/mine/mml/codegen/cpp` | 生成文件可被 MSVC 编译 |
| T6 | `tool.mmlc`：CLI 包装，`--depfile -o --vm-include` 支持；xmake `mine.mml` rule 集成 | `tools/mmlc` | `xmake build samples/05-mml-hello` 一键通过 |

git tag：`v0.2.1-t1`

---

### T2 — 语言特性扩展

**目标**：MML 语言能力覆盖双向绑定、事件、条件/循环、状态机、资源与主题。

| # | 特性 | 模块 | 验收 |
|---|------|------|------|
| T7 | 双向绑定 `<=>` 语法 | `mine.mml.parse` + `codegen` | `<=>` 编译后 C++ 双向绑定正常工作 |
| T8 | 事件订阅语法（`on:Click="..."` 等） | `mine.mml.parse` + `codegen` | 点击触发 ViewModel 命令 |
| T9 | `if` / `for` 模板指令 | `mine.mml.parse` + `codegen` | 条件显隐 + 列表渲染 |
| T10 | `states { ... }` 状态机块；与 `mine.ui.style.VisualStateManager` 联动 | `mine.mml.*` | 状态切换触发 VSM 动画 |
| T11 | `resources { theme ... }` 主题块 → 生成主题资源注册代码 | `mine.mml.codegen` | 切换主题无崩溃 |
| T12 | `style X for T { ... }` 块 → 生成 `apply_X_impl()` + `StyleRegistry::register_style()` | `mine.mml.codegen` | AccentButton.style.g.cpp 可编译 |
| T13 | `template X for T { ... states { } }` 块 → 生成 `build_X_impl()` | `mine.mml.codegen` | DefaultButton.template.g.cpp 可编译 |

git tag：`v0.3.1-t2`

---

### T3 — 开发工具

**目标**：IDE 开发体验达到"打字即提示、保存即格式化"的水准。

| # | 工具 | 路径 | 验收 |
|---|------|------|------|
| T14 | `tool.mmlls`：LSP 全特性（goto / hover / refs / format / rename / inlay / code-action） | `tools/mmlls` | VS Code 测试通过 |
| T15 | `tool.mmlfmt`：基于 AST 的格式化工具 | `tools/mmlfmt` | 幂等格式化；diff 为零 |
| T16 | `ext.vscode`：VS Code 插件套壳 mmlls，语法高亮、代码片段、调试集成 | `ext/vscode` | 扩展市场可安装 |
| T17 | `tool.minepack`：SDK 打包脚本，产物清单生成 | `tools/minepack` | 产物 zip 含所有头文件 |
| T18 | `tool.mineinst`：Windows 自举安装器，PATH 注册、SDK 解包 | `tools/mineinst` | 全新机器一键安装可用 |

git tag：`v0.4.1-t3`

---

### T4 — 集成验收与样例

**目标**：端到端样例验证 MML 工具链与主框架的集成质量。

| # | 样例 | 路径 | 说明 |
|---|------|------|------|
| T19 | `samples/05-mml-hello`：最小 MML hello world（标签 + 文本 + 按钮） | `samples/05-mml-hello` | mmlc 生成代码 + 渲染正确 |
| T20 | `samples/06-mml-mvvm-todo`：MVVM Todo 应用的 MML 版本（对比 samples/01-pure-cpp） | `samples/06-mml-mvvm-todo` | 功能与纯 C++ 版一致 |
| T21 | `samples/07-mml-style-theme`：样式 + 主题切换的 MML 示例 | `samples/07-mml-style-theme` | 运行时切换主题无闪烁 |

git tag：`v0.5.1-t4`

---

## 版本与里程碑对照

| git tag | 主框架里程碑 | MML 工具链里程碑 |
|---------|------------|----------------|
| `v0.1.0-f0` | F0 完成（框架骨架） | — |
| `v0.2.0-f1` | F1 完成（UI 核心） | — |
| `v0.2.1-t1` | — | T1 完成（编译器核心） |
| `v0.3.0-f2` | F2 完成（样式与动画） | — |
| `v0.3.1-t2` | — | T2 完成（语言特性扩展） |
| `v0.4.0-f3` | F3 完成（MVVM + 控件） | — |
| `v0.4.1-t3` | — | T3 完成（开发工具） |
| `v0.5.0-f4` | F4 完成（数据层） | — |
| `v0.5.1-t4` | — | T4 完成（集成验收） |
| `v0.6.0-f5` | F5 完成（网络层） | — |
| `v0.7.0-f6` | F6 完成（多平台） | — |
| `v0.8.0-f7` | F7 完成（性能优化） | — |
| `v1.0.0` | 稳定 ABI，冻结公共头，发布语言绑定（C# / Python / Rust） | — |

---

## 持续工作流（每个里程碑均执行）

1. **CI**：每个 PR 必跑 fmt / lint / build / test / 二进制体积检查
2. **审查**：所有公共头修改必须有对应设计文档更新
3. **基线**：`docs/baseline/Fn.md` / `Tn.md` 记录二进制体积、启动耗时、首帧耗时、内存峰值
4. **CHANGELOG**：每个里程碑结束前合并 Unreleased 段落
5. **依赖审查**：每月检查第三方库版本与 CVE
6. **文档同步**：API 改动必须同时改 `docs/` 与 IDE 注释

---

## 风险与对策

| 风险 | 对策 |
|------|------|
| MML AOT 表达力不足以替代运行时 VM | T1 末期固化 MML 子集，新增能力走标记宏路径 |
| 函数级裁剪在 GCC 上不稳定 | 优先 Clang/LLD，GCC 仅作兼容备份 |
| 多平台后端进度失衡 | F6 之前其它平台保持 alpha 标签，不进入 SDK 主线 |
| ABI 稳定与功能迭代冲突 | v1.0 之前不承诺 ABI；之后按 SemVer 严格执行 |
| 第三方依赖（mbedTLS/HarfBuzz/SQLite）许可与版本碎片 | 全部 vendor 进 `third_party/`，xmake.requires 锁版本 |
| 主框架与 MML 工具链集成时接口不匹配 | T1 启动前先对齐 codegen 所需的公共 API 清单，以主框架 API 为准 |

---

## 当前位置（2026-05-21）

- ✅ **F0**（框架骨架）— 全部完成
- ✅ **F1**（UI 核心，#1–#9）— 全部完成，含样式/模板/视觉状态挂点
- 🚀 **下一步：F2.1 — 实现 `mine.ui.style` 样式系统**
- ⏳ **MML 工具链**（T1–T4）— 待 F1 完成后按独立节奏启动
