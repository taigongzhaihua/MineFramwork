# ModuleTag 详细接口文档

## 概述

`ModuleTag.h` 定义了 `mine.platform.abi` 模块的**模块标识**，包含 `kModuleName` 常量。

**核心特性：**
- **模块名称**：`kModuleName = "mine.platform.abi"`
- **编译时常量**：`inline constexpr const char*`
- **用途**：日志、诊断、模块识别

---

## 文件位置

```
src/mine/platform/abi/api/include/mine/platform/ModuleTag.h
```

---

## 类型定义

```cpp
#pragma once

namespace mine::platform {

/// 模块名称标识符
inline constexpr const char* kModuleName = "mine.platform.abi";

}
```

---

## 常量

### kModuleName

```cpp
inline constexpr const char* kModuleName = "mine.platform.abi";
```

**描述**：模块名称标识符。

**特征**：
- `inline constexpr`：编译时常量，无需链接
- 值：`"mine.platform.abi"`
- 用途：日志、诊断、模块识别

---

## 使用场景

### 1. 日志输出

```cpp
#include <mine/platform/ModuleTag.h>

void log_message(const char* msg) {
    log("[{}] {}", mine::platform::kModuleName, msg);
}
```

---

### 2. 模块识别

```cpp
#include <mine/platform/ModuleTag.h>

void identify_module() {
    log("当前模块: {}", mine::platform::kModuleName);
}
```

---

### 3. 调试信息

```cpp
#include <mine/platform/ModuleTag.h>

void debug_info() {
    log("模块: {}, 版本: {}", mine::platform::kModuleName, "0.1.0");
}
```

---

## 设计理念

### 编译时常量

**特征**：
- `inline constexpr`：编译时求值
- 无需链接：头文件即可使用

**优点**：
- 零开销
- 编译时确定
- 无需定义文件

---

### 模块标识

**目标**：
- 标识模块名称
- 用于日志、诊断
- 模块识别

---

## 最佳实践

### 1. 日志输出

```cpp
// ✅ 推荐：日志输出
log("[{}] 窗口创建成功", mine::platform::kModuleName);

// ❌ 不推荐：硬编码模块名
log("[mine.platform.abi] 窗口创建成功");
```

---

### 2. 模块识别

```cpp
// ✅ 推荐：模块识别
if (module_name == mine::platform::kModuleName) {
    // ...
}

// ❌ 不推荐：硬编码模块名
if (module_name == "mine.platform.abi") {
    // ...
}
```

---

## 完整示例

```cpp
#include <mine/platform/ModuleTag.h>
#include <mine/platform/PlatformAbi.h>

class Application {
public:
    void log_message(const char* msg) {
        log("[{}] {}", mine::platform::kModuleName, msg);
    }
    
    void identify_module() {
        log("当前模块: {}", mine::platform::kModuleName);
    }
    
    void debug_info() {
        log("模块: {}, 版本: {}", mine::platform::kModuleName, "0.1.0");
    }
};

int main() {
    Application app;
    app.log_message("应用启动");
    app.identify_module();
    app.debug_info();
    return 0;
}
```

---

## 总结

`ModuleTag.h` 定义了 `mine.platform.abi` 模块的模块标识 `kModuleName`，具备：

1. **模块名称**：`kModuleName = "mine.platform.abi"`
2. **编译时常量**：`inline constexpr const char*`
3. **用途**：日志、诊断、模块识别

**使用建议**：
- 日志输出时使用 `kModuleName`
- 模块识别时使用 `kModuleName`
- 避免硬编码模块名
