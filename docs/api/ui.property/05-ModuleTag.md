# ModuleTag 详细接口文档

## 概述

`ModuleTag` 是 `mine.ui.property` 模块的**模块标识常量**。

**核心特性：**
- **模块名常量**：kPropertyModuleName（字符串字面量 "mine.ui.property"）

---

## 文件位置

```
src/mine/ui/property/api/include/mine/ui/property/ModuleTag.h
```

---

## 常量定义

### kPropertyModuleName

```cpp
inline constexpr const char* kPropertyModuleName = "mine.ui.property";
```

**描述**：mine.ui.property 模块标识常量。

**类型**：`const char*`（字符串字面量）

**用途**：
- 模块识别（日志、调试、元数据）
- 模块化系统中的模块名标识

---

## 使用场景

### 1. 日志输出

```cpp
#include <mine/ui/property/ModuleTag.h>
#include <iostream>

void log_module_info() {
    std::cout << "Module: " << mine::ui::kPropertyModuleName << std::endl;
    // 输出: Module: mine.ui.property
}
```

---

### 2. 模块化系统中的模块识别

```cpp
#include <mine/ui/property/ModuleTag.h>

bool is_property_module(const char* module_name) {
    return std::strcmp(module_name, mine::ui::kPropertyModuleName) == 0;
}
```

---

## 总结

`ModuleTag` 是 `mine.ui.property` 模块的模块标识常量，具备：

1. **模块名常量**：kPropertyModuleName（字符串字面量 "mine.ui.property"）

**使用建议**：
- 用于日志输出、调试、元数据等场景
- 模块化系统中作为模块名标识
