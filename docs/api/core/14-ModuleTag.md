# ModuleTag.h (模块标识) 详细文档

## 概述

`ModuleTag.h` 定义了 `kModuleName` 常量,用于标识当前模块的名称（`"mine.core"`）。此常量主要用于日志系统、诊断工具和调试器,帮助定位错误来源。

### 核心特性

- **编译期常量**：`inline constexpr` 确保无运行时开销
- **全局唯一**：每个模块有唯一的名称标识
- **跨 DLL 边界安全**：常量在链接时合并,多个 DLL 共享同一地址
- **日志友好**：配合日志系统输出模块信息

### 设计动机

在大型项目中,错误可能来自多个模块。通过在日志/断言中记录模块名称,可以快速定位问题:

- **诊断错误来源**：断言失败时显示所属模块
- **过滤日志输出**：按模块名过滤日志
- **性能分析**：按模块统计函数调用次数

---

## 常量定义

### 完整定义

```cpp
#pragma once

namespace mine::core {

inline constexpr const char* kModuleName = "mine.core";

} // namespace mine::core
```

---

## 常量详解

### kModuleName

```cpp
inline constexpr const char* kModuleName = "mine.core";
```

**类型**：`const char*`（指向字符串字面量的常量指针）

**值**：`"mine.core"`（当前模块的完全限定名）

**存储期**：静态存储期（程序生命周期内始终有效）

**链接性**：`inline` 确保跨翻译单元共享同一地址（ODR-safe）

**用途**：
1. 日志系统中标识日志来源
2. 断言失败时显示模块名
3. 性能分析工具中统计模块调用
4. 调试器中过滤特定模块

---

## C++17 inline 变量

### inline 关键字作用

在 C++17 之前,头文件中定义全局变量会导致多重定义链接错误（ODR 违反）。C++17 的 `inline` 变量解决了此问题:

```cpp
// C++14 及之前（错误）
// ModuleTag.h
namespace mine::core {
    constexpr const char* kModuleName = "mine.core";  // 每个翻译单元一个副本
}
// 链接时报错：multiple definition of 'kModuleName'

// C++17（正确）
// ModuleTag.h
namespace mine::core {
    inline constexpr const char* kModuleName = "mine.core";  // 链接时合并
}
```

**效果**：
- 所有翻译单元共享同一 `kModuleName` 地址
- 可在头文件中定义,无需在 `.cpp` 中声明
- 满足 ODR（One Definition Rule）

---

## 使用场景

### 1. 日志系统

```cpp
#include <mine/core/ModuleTag.h>
#include <mine/core/Assert.h>

void log_error(const char* module, const char* message) {
    fprintf(stderr, "[%s] ERROR: %s\n", module, message);
}

void example() {
    log_error(mine::core::kModuleName, "Allocation failed");
    // 输出: [mine.core] ERROR: Allocation failed
}
```

---

### 2. 断言消息

```cpp
#include <mine/core/ModuleTag.h>
#include <mine/core/Assert.h>

void initialize() {
    void* ptr = allocate_memory();
    if (!ptr) {
        fprintf(stderr, "[%s] Assertion failed: allocation returned nullptr\n",
                mine::core::kModuleName);
        abort();
    }
}
```

---

### 3. 性能分析

```cpp
#include <mine/core/ModuleTag.h>

struct ProfileScope {
    const char* module;
    const char* function;
    
    ProfileScope(const char* mod, const char* func)
        : module{mod}, function{func} {
        start_timer(module, function);
    }
    
    ~ProfileScope() {
        stop_timer(module, function);
    }
};

void expensive_operation() {
    ProfileScope profile{mine::core::kModuleName, __FUNCTION__};
    // 执行昂贵操作...
}
```

---

### 4. 调试器过滤

```cpp
#include <mine/core/ModuleTag.h>

void debug_log(const char* module, const char* message) {
    // 调试器可按模块名过滤日志
    printf("[%s] %s\n", module, message);
}

void example() {
    debug_log(mine::core::kModuleName, "Widget constructed");
}
```

---

## 模块命名规范

### 命名约定

MineFramework 中所有模块遵循统一的命名规范:

| 模块 | kModuleName | 说明 |
|------|------------|------|
| mine.core | `"mine.core"` | 核心类型（TypeId, Variant, StringView 等） |
| mine.containers | `"mine.containers"` | 容器类（Vector, HashMap） |
| mine.ui.property | `"mine.ui.property"` | 属性系统（DependencyProperty） |
| mine.ui.binding | `"mine.ui.binding"` | 数据绑定 |
| mine.ui.visual | `"mine.ui.visual"` | 可视化元素基类 |
| mine.ui.controls | `"mine.ui.controls"` | 控件库（Button, TextBox） |
| mine.mvvm | `"mine.mvvm"` | MVVM 框架 |

**规则**：
1. 全小写
2. 使用 `.` 分隔层级
3. 顶级命名空间为 `mine`
4. 与 C++ 命名空间对应（`mine::core` ↔ `"mine.core"`）

---

## 其他模块的 kModuleName

### mine.containers

```cpp
// mine/containers/ModuleTag.h
namespace mine::containers {
    inline constexpr const char* kModuleName = "mine.containers";
}
```

---

### mine.ui.visual

```cpp
// mine/ui/visual/ModuleTag.h
namespace mine::ui::visual {
    inline constexpr const char* kModuleName = "mine.ui.visual";
}
```

---

## 性能分析

### 内存开销

```cpp
sizeof(kModuleName) == 8  // 64 位系统,指针大小
```

**说明**：`kModuleName` 是指向字符串字面量的指针,字面量本身存储在只读数据段（`.rodata`）。

### 运行时开销

| 操作 | 开销 | 说明 |
|------|------|------|
| 访问 `kModuleName` | O(1) | 读取常量地址 |
| 比较 `kModuleName` | O(1) | 指针比较（同模块相同地址） |
| 字符串比较 | O(n) | 若需内容比较（`strcmp`） |

**优化**：
- 指针比较比字符串比较快（若保证同一字面量）
- 编译器可内联 `kModuleName` 引用

---

## 最佳实践

### 1. 日志中始终包含模块名

**推荐**：所有日志输出包含 `kModuleName`,方便过滤。

```cpp
// ✅ 推荐
log_error(mine::core::kModuleName, "Allocation failed");

// ❌ 不推荐
log_error("Allocation failed");  // 无法定位来源
```

---

### 2. 断言消息中标识模块

**推荐**：自定义 `assertion_failed` 时输出模块名。

```cpp
[[noreturn]] void assertion_failed(
    const char* expr,
    const char* file,
    int line,
    const char* msg) noexcept
{
    fprintf(stderr, "[%s] Assertion failed: %s\n",
            mine::core::kModuleName, expr);
    fprintf(stderr, "  File: %s:%d\n", file, line);
    abort();
}
```

---

### 3. 性能分析时标记模块

**推荐**：性能分析工具中记录模块信息。

```cpp
void expensive_function() {
    PROFILE_SCOPE(mine::core::kModuleName, __FUNCTION__);
    // 执行逻辑...
}
```

---

### 4. 不要修改 kModuleName

**推荐**：`kModuleName` 为编译期常量,不应运行时修改。

```cpp
// ❌ 错误：尝试修改常量
mine::core::kModuleName = "custom.name";  // 编译错误

// ✅ 正确：定义自己的常量
namespace mylib {
    inline constexpr const char* kModuleName = "mylib";
}
```

---

## 完整示例

### 示例：日志系统集成

```cpp
// Logger.h
#pragma once

class Logger {
public:
    static void error(const char* module, const char* message);
    static void warning(const char* module, const char* message);
    static void info(const char* module, const char* message);
};

// Logger.cpp
#include "Logger.h"
#include <cstdio>

void Logger::error(const char* module, const char* message) {
    fprintf(stderr, "[ERROR][%s] %s\n", module, message);
}

void Logger::warning(const char* module, const char* message) {
    fprintf(stderr, "[WARN ][%s] %s\n", module, message);
}

void Logger::info(const char* module, const char* message) {
    printf("[INFO ][%s] %s\n", module, message);
}
```

```cpp
// Widget.cpp
#include "Widget.h"
#include <mine/core/ModuleTag.h>
#include "Logger.h"

Widget::Widget() {
    Logger::info(mine::core::kModuleName, "Widget constructed");
}

void Widget::do_work() {
    if (!pimpl_) {
        Logger::error(mine::core::kModuleName, "Widget not initialized");
        return;
    }
    
    Logger::info(mine::core::kModuleName, "Widget doing work...");
    pimpl_->work();
}
```

**输出**：

```
[INFO ][mine.core] Widget constructed
[INFO ][mine.core] Widget doing work...
```

---

## 常见问题

### Q1：为什么使用字符串字面量而非枚举？

**A**：
- 字符串直接可用于日志输出,无需转换
- 支持层级命名（`"mine.ui.visual"`）
- 跨 DLL 边界安全（只读数据段）

---

### Q2：可以在运行时修改 kModuleName 吗？

**A**：不可以。`constexpr` 确保编译期常量,尝试修改会导致编译错误。

---

### Q3：多个 DLL 中 kModuleName 地址相同吗？

**A**：是的（若链接器支持 COMDAT 折叠）。`inline` 确保所有翻译单元共享同一地址。

---

### Q4：如何为自定义模块定义 kModuleName？

**A**：遵循相同模式:

```cpp
// mylib/ModuleTag.h
namespace mylib {
    inline constexpr const char* kModuleName = "mylib";
}
```

---

## 总结

`kModuleName` 是 MineFramework 中标识模块名称的标准方式,具备以下优势:

1. **编译期常量**：`inline constexpr` 确保零运行时开销
2. **全局唯一**：每个模块有唯一的名称标识
3. **跨 DLL 边界安全**：常量在链接时合并
4. **日志友好**：配合日志系统输出模块信息

在实现日志系统、断言机制和性能分析工具时,合理使用 `kModuleName` 可以显著提升代码的可调试性和可维护性。所有 MineFramework 模块均遵循此模式,确保诊断信息的一致性。
