# ModuleTag.h 详细接口文档

## 概述

`ModuleTag.h` 定义了 `kModuleName` 常量,用于标识 `mine.containers` 模块。该常量在日志、调试、反射系统等场景下用于模块识别。

---

## 定义

```cpp
#pragma once

namespace mine::containers {

/// 模块名称常量
inline constexpr const char* kModuleName = "mine.containers";

}
```

---

## 类型说明

### kModuleName

```cpp
inline constexpr const char* kModuleName;
```

**类型**：`const char*`（C 字符串指针）

**值**：`"mine.containers"`

**修饰符**：
- **`inline`**：允许在头文件中定义,多个编译单元共享同一实例
- **`constexpr`**：编译期常量,可用于模板参数和常量表达式

**生命周期**：静态存储期（程序运行期间始终有效）

---

## 使用场景

### 1. 日志输出

```cpp
#include <mine/containers/ModuleTag.h>
#include <iostream>

void log_error(const char* module, const char* msg) {
    std::cerr << "[" << module << "] " << msg << "\n";
}

void example() {
    Vector<int> vec;
    if (vec.empty()) {
        log_error(mine::containers::kModuleName, "Vector is empty");
        // 输出: [mine.containers] Vector is empty
    }
}
```

---

### 2. 断言消息

```cpp
#include <mine/containers/ModuleTag.h>
#include <mine/core/Assert.h>

void check_capacity(size_t cap) {
    MINE_CHECK(cap > 0,
               "{}: capacity must be positive",
               mine::containers::kModuleName);
}
```

---

### 3. 反射系统

```cpp
#include <mine/containers/ModuleTag.h>

struct ModuleInfo {
    const char* name;
    int version_major;
    int version_minor;
};

constexpr ModuleInfo get_module_info() {
    return {
        .name = mine::containers::kModuleName,
        .version_major = 1,
        .version_minor = 0
    };
}
```

---

### 4. 性能分析标记

```cpp
#include <mine/containers/ModuleTag.h>

class Profiler {
public:
    void begin_scope(const char* module, const char* func) {
        // 记录性能数据
    }
};

void some_function() {
    Profiler profiler;
    profiler.begin_scope(mine::containers::kModuleName, __func__);
    // ...
}
```

---

## 模块命名约定

MineFramework 的所有模块都遵循相同的命名约定:

| 模块 | kModuleName 值 | 文件路径 |
|------|---------------|---------|
| `mine.core` | `"mine.core"` | `mine/core/ModuleTag.h` |
| `mine.containers` | `"mine.containers"` | `mine/containers/ModuleTag.h` |
| `mine.math` | `"mine.math"` | `mine/math/ModuleTag.h` |
| `mine.ui.property` | `"mine.ui.property"` | `mine/ui/property/ModuleTag.h` |
| `mine.ui.visual` | `"mine.ui.visual"` | `mine/ui/visual/ModuleTag.h` |

---

## 为什么使用 inline constexpr？

### 传统方式的问题

**方式 1：在头文件中定义（重复定义错误）**

```cpp
// ❌ 错误：多个编译单元包含此头文件时,kModuleName 重复定义
namespace mine::containers {
    const char* kModuleName = "mine.containers";  // 链接错误!
}
```

**方式 2：extern + 在 .cpp 中定义（需要额外源文件）**

```cpp
// ModuleTag.h
namespace mine::containers {
    extern const char* kModuleName;  // 声明
}

// ModuleTag.cpp
namespace mine::containers {
    const char* kModuleName = "mine.containers";  // 定义
}
```

**问题**：
- 需要额外的 `.cpp` 文件
- 运行时初始化,非编译期常量

---

### inline constexpr 的优势

```cpp
// ✅ 正确：inline constexpr 在头文件中定义
namespace mine::containers {
    inline constexpr const char* kModuleName = "mine.containers";
}
```

**优势**：
1. **编译期常量**：`constexpr` 确保编译期求值
2. **无重复定义**：`inline` 允许多个编译单元共享同一定义
3. **仅头文件**：无需额外 `.cpp` 文件
4. **零开销**：编译器可直接内联字符串字面量

---

## 与其他模块的一致性

所有 MineFramework 模块都提供相同的 `kModuleName` 常量:

```cpp
#include <mine/core/ModuleTag.h>
#include <mine/containers/ModuleTag.h>
#include <mine/ui/property/ModuleTag.h>

void print_modules() {
    std::cout << mine::core::kModuleName << "\n";           // "mine.core"
    std::cout << mine::containers::kModuleName << "\n";     // "mine.containers"
    std::cout << mine::ui::property::kModuleName << "\n";   // "mine.ui.property"
}
```

---

## 编译期使用

由于 `kModuleName` 是 `constexpr`,可用于编译期计算:

```cpp
#include <mine/containers/ModuleTag.h>
#include <string_view>

// 编译期计算模块名长度
consteval size_t module_name_length() {
    return std::string_view{mine::containers::kModuleName}.size();
}

static_assert(module_name_length() == 15);  // "mine.containers" 长度为 15
```

---

## 完整示例

### 示例：模块信息注册表

```cpp
#include <mine/core/ModuleTag.h>
#include <mine/containers/ModuleTag.h>
#include <mine/containers/HashMap.h>
#include <mine/containers/InlineString.h>

using namespace mine::containers;

class ModuleRegistry {
    HashMap<StringView, InlineString> modules_;
    
public:
    void register_module(StringView name, StringView version) {
        modules_.insert(name, InlineString{version});
    }
    
    void print_all() {
        for (auto it = modules_.begin(); it != modules_.end(); ++it) {
            std::cout << it.key() << " v" << it.value() << "\n";
        }
    }
};

void example() {
    ModuleRegistry registry;
    
    // 使用 kModuleName 注册模块
    registry.register_module(mine::core::kModuleName, "1.0.0");
    registry.register_module(mine::containers::kModuleName, "1.0.0");
    
    registry.print_all();
    // 输出:
    // mine.core v1.0.0
    // mine.containers v1.0.0
}
```

---

## 最佳实践

### 1. 日志中使用模块名

```cpp
// ✅ 推荐：使用 kModuleName
void log_container_error(const char* msg) {
    log(mine::containers::kModuleName, msg);
}

// ❌ 不推荐：硬编码字符串
void log_container_error2(const char* msg) {
    log("mine.containers", msg);  // 容易拼写错误
}
```

---

### 2. 避免不必要的字符串拷贝

```cpp
// ✅ 推荐：直接使用指针
const char* get_module_name() {
    return mine::containers::kModuleName;
}

// ❌ 不推荐：不必要的 std::string 拷贝
std::string get_module_name2() {
    return std::string{mine::containers::kModuleName};  // 多余的堆分配
}
```

---

## 相关文件

- **`mine/core/ModuleTag.h`**：`kModuleName = "mine.core"`
- **`mine/math/ModuleTag.h`**：`kModuleName = "mine.math"`
- **`mine/ui/property/ModuleTag.h`**：`kModuleName = "mine.ui.property"`

所有模块都遵循相同的 `ModuleTag.h` 约定。

---

## 总结

`ModuleTag.h` 定义了 `kModuleName` 常量,用于:

1. **模块识别**：在日志、调试中标识模块
2. **编译期常量**：`constexpr` 允许编译期使用
3. **零开销**：`inline` 避免重复定义,编译器可内联

**使用原则**：
- 日志/断言中使用 `kModuleName` 而非硬编码字符串
- 作为编译期常量使用,避免运行时拷贝
- 与其他模块保持一致的命名约定
