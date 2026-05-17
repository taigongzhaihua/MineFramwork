# 21 — mine.core 模块 API 设计

> 对齐 [02-modules.md](02-modules.md)、[18-coding-conventions.md](18-coding-conventions.md) 与
> [development-plan.md](development-plan.md) 中的 M0.1 基础设施要求。

---

## 21.1 模块职责

`mine.core` 是 MineFramework 的 L0 基础模块，负责提供所有上层模块共享的最小公共能力：

- ABI 相关的导出宏与可见性约束。
- 显式错误返回与错误传播工具。
- 视图类型与基础值语义封装。
- 所有权、PIMPL、可选值、变体等公共抽象。
- 字符串与分配器等跨模块基础设施。

该模块必须满足以下约束：

- 公共 API 不依赖异常与 RTTI。
- 公共头不暴露平台后端类型。
- 头文件命名、注释与导出规则遵循统一编码规范。
- 所有更高层模块都可以依赖它，但它不能反向依赖上层。

---

## 21.2 当前骨架

当前仓库已落地以下公开头入口：

| 头文件 | 作用 |
|---|---|
| `mine/core/Api.h` | 定义 `MINE_API` 导出宏与共享库边界可见性规则 |
| `mine/core/ModuleTag.h` | 提供模块稳定名称常量 `kModuleName` |

这两个头文件是后续扩展 `mine.core` 公开 API 的基础锚点：

- `Api.h` 约束所有跨模块导出符号的修饰方式。
- `ModuleTag.h` 提供日志、诊断、工具链与元信息复用的稳定模块标识。

---

## 21.3 规划中的公开头

根据 M0.1 的开发计划，`mine.core` 后续将按以下分组扩展公开头：

### 21.3.1 错误与断言

- `mine/core/Assert.h`：`MINE_ASSERT`、`MINE_CHECK`、调试断言辅助。
- `mine/core/Status.h`：轻量错误码与状态对象。
- `mine/core/Result.h`：`Result<T, E>` 返回模型。
- `mine/core/Try.h`：`MINE_TRY` 等错误传播辅助宏。

### 21.3.2 视图与值语义

- `mine/core/StringView.h`：只读 UTF-8 字符串视图。
- `mine/core/Span.h`：连续内存片段视图。
- `mine/core/Optional.h`：可空值封装。
- `mine/core/Variant.h`：受控多类型值容器。

### 21.3.3 所有权与封装

- `mine/core/Pimpl.h`：稳定 ABI 的 PIMPL 封装。
- `mine/core/OwnedPtr.h`：替代 `std::unique_ptr` 的公共所有权类型。

### 21.3.4 内存与字符串

- `mine/core/Allocator.h`：自定义分配器抽象与默认分配策略。
- `mine/core/String.h`：基于分配器的 UTF-8 字符串实现。

这些头文件在真正实现前，不应通过空壳接口占位绕过设计文档要求。

---

## 21.4 API 设计约束

`mine.core` 的公开 API 需要同时满足可读性、ABI 稳定性与性能约束：

1. 导出类型必须显式使用 `MINE_API`。
2. 返回错误的公共函数优先返回 `Status` 或 `Result<T, E>`。
3. 任何跨 DLL 的复杂类都应通过 PIMPL 或平凡布局类型隔离实现细节。
4. 不允许在公共头中直接暴露 `std::unique_ptr` 之类的 ABI 敏感所有权类型。
5. 文本接口默认按 UTF-8 设计，Windows 编译也必须保证源码按 UTF-8 解析。

---

## 21.5 实现与测试边界

目录约定如下：

- `src/mine/core/api/include/`：公开头，仅放稳定 API。
- `src/mine/core/src/`：非模板实现与内部辅助代码。
- `src/mine/core/test/`：doctest 单元测试。
- `src/mine/core/bench/`：性能基准。

优先测试以下行为：

- `Result`、`Status` 与 `MINE_TRY` 的错误传播。
- `StringView`、`Span` 的边界与生命周期语义。
- `Pimpl` 的拷贝、移动与析构行为。
- `Allocator` 与 `String` 在无异常路径下的失败处理。

---

## 21.6 近期落地顺序

建议按以下顺序推进 `mine.core`：

1. `Api.h` 与 `Assert.h`，先固定导出与断言边界。
2. `Status.h`、`Result.h`、`Try.h`，建立统一错误模型。
3. `StringView.h`、`Span.h`、`Optional.h`，补齐轻量值语义基础。
4. `Allocator.h`、`String.h`、`Variant.h`，补齐后续模块依赖的重心类型。
5. `Pimpl.h`、`OwnedPtr.h`，为跨模块 ABI 稳定做收尾。

这一路径与 M0 的目标一致：先让所有 L0 模块拥有稳定、可复用、可测试的基础 API。