# 18 — 编码规范

## 18.1 语言版本

* C++20 强制；C++23 选用特性需用 `MINE_HAS_CPP23_*` 探测。
* 不依赖 RTTI / 异常（核心模块）；PAL 内部允许，边界处转换为 `Result/Status`。

## 18.2 命名

| 实体 | 风格 | 示例 |
|------|------|------|
| 名空间 | `lower_snake` | `mine::ui::controls` |
| 类型 | `PascalCase` | `Visual`, `DependencyProperty` |
| 函数/方法 | `snake_case` | `set_content()`, `invalidate_measure()` |
| 变量/字段 | `snake_case` | `child_count_` |
| 私有字段 | `snake_case_` 末尾下划线 | `parent_` |
| 常量/枚举值 | `PascalCase`（enum class）/ `kPascalCase`（compile-time） | `enum class Mode { OneWay, TwoWay }` |
| 宏 | `MINE_UPPER_SNAKE` | `MINE_ASSERT` |
| 模板参数 | `T`、`U`、`TFoo` | — |
| 文件名 | `PascalCase.h/.cpp`，与主类型同名 | `Visual.h` |

## 18.3 头文件结构

```cpp
// Visual.h
#pragma once
#include <mine/core/Pimpl.h>
#include <mine/math/Rect.h>

namespace mine::ui {

class MINE_API Visual {
public:
    Visual();
    ~Visual();
    /* ... */
private:
    struct Impl;
    mine::core::Pimpl<Impl> p_;
};

} // namespace mine::ui
```

* 公共头不 `#include` STL 容器（除非 `<cstdint>`、`<cstddef>` 等基础头）。
* `MINE_API` 标记导出符号，宏定义在 `mine/core/Api.h`。

## 18.4 错误处理

* 公共 API 不抛异常；返回 `Result<T,E>` 或 `Status`。
* `[[nodiscard]]` 默认加在 `Result/Status` 返回上。
* `MINE_TRY(x)` 宏：在协程/函数中提前返回错误。
* 资源管理用 RAII；裸指针仅在性能敏感局部，且生命周期显式。

## 18.5 格式化

* 通过 `clang-format` 强制（工程根目录提供 `.clang-format`）。
* 列宽 100；缩进 4 空格；指针/引用绑定到类型 `T* x`。

## 18.6 lint / 静态分析

* `clang-tidy`：必启检查清单见 `.clang-tidy`。
* MSVC `/analyze`、Clang `-Wthread-safety`、`-Wlifetime`（非主线）。
* 自定义 lint：检测公共头非法包含 STL、缺失 `MINE_API` 等。

## 18.7 单元测试

* 单测使用 doctest，每模块 `test/` 子目录；
* 测试名 `Module_Feature_Behavior`：
  ```cpp
  TEST_CASE("ui_visual_AddChild_AppendsToEnd") { ... }
  ```
* 覆盖率目标：核心库 ≥ 85%，控件库 ≥ 70%。

## 18.8 提交与代码评审

* Conventional Commits：`feat(ui.binding): add fallback support`。
* PR 模板：动机 / 设计 / 影响范围 / 测试。
* 至少 1 个 reviewer + CI 全绿合入。

## 18.9 文档与注释

* 公共类型/方法必须 Doxygen 注释；
* 每个模块根目录 `README.md` 解释模块边界、对外契约、示例；
* 大特性走 `docs/proposals/NNN-xxx.md` RFC 流程。
