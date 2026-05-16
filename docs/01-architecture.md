# 01 — 分层架构

## 1.1 总体分层

```
┌──────────────────────────────────────────────────────────────┐
│  L9  Tooling          mmlc / mmlls / mmlfmt / shadercc /     │
│                       minepack / mineinst                    │
├──────────────────────────────────────────────────────────────┤
│  L8  Markup           mml.lex/parse/ast/sema/codegen         │
├──────────────────────────────────────────────────────────────┤
│  L7  Networking       net.tcp/tls/http/ws/rpc                │
├──────────────────────────────────────────────────────────────┤
│  L6  Data             data.sqlite / data.orm / data.cache    │
├──────────────────────────────────────────────────────────────┤
│  L5  App Services     di / mvvm / nav / i18n / config        │
├──────────────────────────────────────────────────────────────┤
│  L4  UI Core          property/binding/event/input/layout/   │
│                       visual/style/anim/controls/window/app  │
├──────────────────────────────────────────────────────────────┤
│  L3  Composition/2D   compose / paint / text.shape /         │
│                       text.layout / image / svg / effects    │
├──────────────────────────────────────────────────────────────┤
│  L2  Graphics         gfx.rhi / d3d11 / d3d12 / metal /      │
│                       vulkan / gles                          │
├──────────────────────────────────────────────────────────────┤
│  L1  Platform         platform.abi / win32 / macos / linux / │
│                       android / ios / io.fs（VFS）           │
├──────────────────────────────────────────────────────────────┤
│  L0  Foundation       core / containers / diag / async / io /│
│                       text / math / reflect / meta           │
└──────────────────────────────────────────────────────────────┘
```

依赖箭头**仅可向下**（同层之间通过显式接口或事件），跨层只允许 **N+1 → N**，禁止跨多层向下引用实现头（只能引用接口头）。

## 1.2 模块分类

每个库都是**独立的 xmake target**，归类为 4 种：

| 类别 | 命名 | 链接性 | 说明 |
|------|------|--------|------|
| **接口库** | `mine.<x>.api` | INTERFACE | 仅头文件 + 抽象类 / `concept` |
| **核心库** | `mine.<x>` | static / shared | 主要实现，遵循 ABI 规则 |
| **后端库** | `mine.<x>.<backend>` | static / shared | 平台/驱动相关实现 |
| **工具** | `tool.<name>` | executable | 工具链 |

例如 `mine.gfx.rhi.api` 定义抽象接口，`mine.gfx.d3d12` 实现并通过运行时工厂注册。

## 1.3 ABI 边界

* **公共头**：仅出现 `mine::` 名空间公开符号。
* **PIMPL**：所有跨 DLL 边界类必须 PIMPL（持有 `Pimpl<T>`）。
* **C ABI 桥**：`include/mine/capi/*.h` 提供稳定 C 接口，供 .NET / Python / Rust 绑定使用，同时也是热重载/插件边界。
* **不在公共头出现**：STL 容器（`std::string` 等）默认不暴露；提供 `mine::String`、`mine::Span<T>`、`mine::ArrayView<T>` 等 ABI 安全替代。

## 1.4 线程模型

| 线程 | 拥有者 | 说明 |
|------|--------|------|
| **UI 线程** | 每窗口一个或共享 | 处理消息、布局、合成提交 |
| **Render 线程** | 每窗口一个 | 真正的 RHI 调用与 swapchain present |
| **IO/任务池** | 进程级 | `mine.async` 的工作线程池 |
| **网络/数据库线程** | 各自独立 / 共享池 | 通过 `Future`/`Task` 与 UI 线程交互 |

跨线程访问通过 `mine::async::Dispatcher` 排队到 UI 线程，依赖属性写访问必须发生在拥有它的 UI 线程上。

## 1.5 错误处理策略

* `mine::Result<T,E>`：业务错误。
* `mine::Status`：无返回值版本。
* `MINE_ASSERT(cond, msg)`：debug 断言；release 默认裁剪（可单独开启硬断言）。
* `mine::core::FatalAbort(...)`：不可恢复故障。
* PAL（平台层）允许内部使用异常，但**不能跨越** `mine::platform::abi` 边界。

## 1.6 配置宏

| 宏 | 作用 |
|----|------|
| `MINE_BUILD_SHARED` / `MINE_BUILD_STATIC` | 链接形态 |
| `MINE_API` | 平台导出修饰 |
| `MINE_NO_EXCEPTIONS` | 禁用异常 |
| `MINE_NO_RTTI` | 禁用 RTTI |
| `MINE_ENABLE_<MODULE>=0/1` | 模块开关，被 xmake option 注入 |
| `MINE_PROFILE_<MIN/STD/FULL>` | 预设配置 |
| `MINE_FUNCTION_LEVEL_LINKING` | 函数级链接（release 默认 1） |
