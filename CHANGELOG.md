# Changelog

本项目遵循 Keep a Changelog，版本号遵循语义化版本。

## [Unreleased]

### Added
- 初始化顶层 xmake 构建入口、构建规则与基础工程配置
- 初始化 M0 基础模块、样例、工具、测试与脚本目录骨架
- **core**：实现 `mine.core` 基础模块，包含以下全套基础类型（无 RTTI、无 STL 容器头、无 C++ 异常）：
  - `Assert.h`：`MINE_ASSERT`/`MINE_CHECK`/`MINE_UNREACHABLE`/`MINE_ASSUME` 断言宏
  - `Status.h`：轻量操作状态 `Status` 与 `StatusCode` 枚举，含 `MINE_TRY` 宏
  - `Result.h`：无异常 `Result<T,E>` 判别联合，支持 `OkTag`/`ErrTag` 工厂
  - `StringView.h`：非拥有 UTF-8 字符串视图，含 `substr`/`find`/`starts_with`/`ends_with`
  - `Span.h`：非拥有连续内存视图 `Span<T>`，支持 C 数组/指针/容器隐式推导
  - `TypeId.h`：无 RTTI 编译期类型标识符，O(1) 比较，cv/引用透明
  - `Allocator.h`：`IAllocator` 接口与 `default_allocator()`/`set_default_allocator()` 全局入口
  - `Memory.h`：`OwnedPtr<T>`（跨 DLL 安全）、`make_owned`、`MINE_NEW`/`MINE_DELETE` 宏
  - `Pimpl.h`：PIMPL 惯用法辅助 `Pimpl<Impl>`/`make_pimpl`
  - `Variant.h`：无 RTTI 类型擦除值容器，16 字节 SBO，支持 `has<T>`/`get<T>`/`emplace<T>`/`any_cast`
  - `Core.h`：伞形头文件，一次 include 引入全部 core 子头
  - `DefaultAllocator.cpp`：`MallocAllocator`（Windows/Linux 对齐分配）、`assertion_failed` 实现
  - `CoreTest.cpp`：61 个测试用例，142 个断言，全部通过