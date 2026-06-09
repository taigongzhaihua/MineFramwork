# ModuleTag 详细接口文档

## 概述

`ModuleTag.h` 定义 `mine.math` 模块的**模块标识常量** `kModuleName`，用于日志、调试、模块识别等场景。

**核心特性：**
- **模块标识**：唯一字符串标识 `"mine.math"`
- **编译期常量**：`constexpr const char*`
- **运行时可用**：可用于日志输出、模块查询

---

## 文件位置

```
src/mine/math/api/include/mine/math/ModuleTag.h
```

---

## 常量定义

### kModuleName

```cpp
namespace mine::math {

inline constexpr const char* kModuleName = "mine.math";

}
```

**类型**：`constexpr const char*`

**值**：`"mine.math"`

**作用域**：`mine::math` 命名空间

---

## 使用场景

### 1. 日志输出

```cpp
#include <mine/math/ModuleTag.h>
#include <iostream>

void log_info(const char* module, const char* message) {
    std::cout << "[" << module << "] " << message << "\n";
}

void example() {
    log_info(mine::math::kModuleName, "Initializing math module");
    // 输出：[mine.math] Initializing math module
}
```

---

### 2. 模块查询

```cpp
struct Module {
    const char* name;
    void (*initialize)();
};

Module g_modules[] = {
    {mine::core::kModuleName, &init_core},
    {mine::math::kModuleName, &init_math},
    {mine::ui::kModuleName, &init_ui},
};

void initialize_module(const char* module_name) {
    for (auto& module : g_modules) {
        if (std::strcmp(module.name, module_name) == 0) {
            module.initialize();
            return;
        }
    }
}
```

---

### 3. 调试断言

```cpp
#include <mine/core/Assert.h>
#include <mine/math/ModuleTag.h>

void validate_rect(Rect rect) {
    MINE_ASSERT(rect.width >= 0.0f, mine::math::kModuleName,
                "Rect width must be non-negative");
    MINE_ASSERT(rect.height >= 0.0f, mine::math::kModuleName,
                "Rect height must be non-negative");
}
```

---

### 4. 性能分析标记

```cpp
#include <mine/profiler/Profiler.h>
#include <mine/math/ModuleTag.h>

void complex_calculation() {
    MINE_PROFILE_SCOPE(mine::math::kModuleName, "Matrix Inversion");
    
    // 执行复杂矩阵运算...
}
```

---

## 命名约定

### MineFramework 模块命名规范

所有模块遵循 `"mine.<category>"` 格式：

| 模块 | kModuleName 值 | 说明 |
|------|---------------|------|
| `mine.core` | `"mine.core"` | 核心基础设施 |
| `mine.containers` | `"mine.containers"` | 容器类型 |
| `mine.math` | `"mine.math"` | 数学库 |
| `mine.async` | `"mine.async"` | 异步框架 |
| `mine.io` | `"mine.io"` | 输入输出 |
| `mine.platform` | `"mine.platform"` | 平台抽象 |
| `mine.gfx` | `"mine.gfx"` | 图形 API |
| `mine.ui` | `"mine.ui"` | UI 框架 |

---

## 实现细节

### 为什么使用 inline constexpr？

```cpp
inline constexpr const char* kModuleName = "mine.math";
```

**`inline`**：
- 允许在头文件定义且多个编译单元包含，避免"重复定义"链接错误
- C++17 开始支持 `inline` 变量

**`constexpr`**：
- 编译期常量，可用于常量表达式
- 零运行时开销

**`const char*`**：
- 指向字符串字面量的指针
- 字符串存储在只读数据段（`.rodata`）

---

## 最佳实践

### 1. 使用 kModuleName 而非硬编码

```cpp
// ✅ 推荐：使用模块常量
log_info(mine::math::kModuleName, "Message");

// ❌ 不推荐：硬编码字符串
log_info("mine.math", "Message");
```

**优点**：
- 修改模块名时只需更改一处
- 类型安全（编译期检查）
- IDE 自动补全和重构支持

---

### 2. 在断言中使用模块名

```cpp
// ✅ 推荐：提供模块上下文
MINE_ASSERT(condition, mine::math::kModuleName, "Error message");

// ❌ 不推荐：无模块信息
MINE_ASSERT(condition, "Error message");
```

---

### 3. 避免运行时字符串比较

```cpp
// ❌ 不推荐：运行时字符串比较（慢）
if (std::strcmp(module_name, mine::math::kModuleName) == 0) { /*...*/ }

// ✅ 推荐：编译期或使用哈希/枚举
constexpr uint32_t module_hash = hash(mine::math::kModuleName);
if (hash(module_name) == module_hash) { /*...*/ }
```

---

## 性能分析

### 内存占用

```cpp
sizeof(kModuleName) == sizeof(const char*)  // 4 字节（32位）或 8 字节（64位）
```

**字符串本身**：
- `"mine.math"` = 10 字节（9 字符 + `'\0'`）
- 存储在只读数据段（`.rodata`）
- 所有引用共享同一份内存

---

### 运行时开销

**常量访问**：O(1)，零运行时开销

**字符串比较**：O(n)，`n` = 字符串长度

**哈希计算**（编译期）：O(1)

---

## 完整示例

```cpp
#include <mine/math/ModuleTag.h>
#include <mine/math/Vec2.h>
#include <iostream>
#include <cstring>

// 示例：简单的模块化日志系统
class Logger {
public:
    static void log(const char* module, const char* level, const char* message) {
        std::cout << "[" << module << "][" << level << "] " << message << "\n";
    }
    
    static void info(const char* module, const char* message) {
        log(module, "INFO", message);
    }
    
    static void error(const char* module, const char* message) {
        log(module, "ERROR", message);
    }
};

// 使用模块标签
void initialize_math() {
    Logger::info(mine::math::kModuleName, "Initializing math module");
    
    // 验证类型大小
    if (sizeof(mine::math::Vec2) != 8) {
        Logger::error(mine::math::kModuleName, "Vec2 size mismatch!");
    }
    
    Logger::info(mine::math::kModuleName, "Math module initialized");
}

// 模块查询示例
bool is_math_module(const char* module_name) {
    return std::strcmp(module_name, mine::math::kModuleName) == 0;
}

int main() {
    initialize_math();
    
    // 输出：
    // [mine.math][INFO] Initializing math module
    // [mine.math][INFO] Math module initialized
    
    if (is_math_module("mine.math")) {
        std::cout << "Module recognized\n";
    }
    
    return 0;
}
```

---

## 与其他模块的关系

### 统一的模块标识系统

所有 MineFramework 模块都提供 `kModuleName` 常量：

```cpp
// mine.core
namespace mine::core {
    inline constexpr const char* kModuleName = "mine.core";
}

// mine.containers
namespace mine::containers {
    inline constexpr const char* kModuleName = "mine.containers";
}

// mine.math
namespace mine::math {
    inline constexpr const char* kModuleName = "mine.math";
}

// mine.ui
namespace mine::ui {
    inline constexpr const char* kModuleName = "mine.ui";
}
```

**优点**：
- 统一的模块识别机制
- 便于日志过滤和模块管理
- 支持依赖注入和插件系统

---

## 总结

`ModuleTag.h` 定义 `mine.math` 模块的**模块标识常量** `kModuleName`，具备：

1. **唯一标识**：字符串 `"mine.math"` 标识模块
2. **编译期常量**：`inline constexpr` 零运行时开销
3. **统一规范**：所有模块遵循相同命名约定

**使用建议**：
- 日志输出、断言、性能分析时使用 `kModuleName`
- 避免硬编码模块名字符串
- 避免运行时频繁字符串比较（改用哈希/枚举）
- 遵循 MineFramework 模块命名规范 `"mine.<category>"`
