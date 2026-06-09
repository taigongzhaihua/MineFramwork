# Api 详细接口文档

## 概述

`Api.h` 定义了 `mine.platform.abi` 模块的 **API 导出宏** `MINE_PLATFORM_ABI_API`，用于标识导出接口。

**核心特性：**
- **导出宏**：`MINE_PLATFORM_ABI_API` 为空占位
- **纯接口层**：`mine.platform.abi` 是头文件库，无实际二进制输出
- **未来扩展**：预留用于文档与未来扩展

---

## 文件位置

```
src/mine/platform/abi/api/include/mine/platform/Api.h
```

---

## 类型定义

```cpp
#pragma once

/// 平台 ABI 接口导出标记（接口层无需 DLL 导出，此处为空占位）
#define MINE_PLATFORM_ABI_API
```

---

## 宏定义

### MINE_PLATFORM_ABI_API

```cpp
#define MINE_PLATFORM_ABI_API
```

**描述**：平台 ABI 接口导出标记（接口层无需 DLL 导出，此处为空占位）。

**特征**：
- 空占位（`mine.platform.abi` 是头文件库）
- 用于文档与未来扩展
- 与其他模块的导出宏格式保持一致

---

## 使用场景

### 1. 接口类标记

```cpp
class MINE_PLATFORM_ABI_API IWindow {
public:
    virtual ~IWindow() = default;
    virtual void show() = 0;
    // ...
};
```

---

### 2. 函数声明标记

```cpp
MINE_PLATFORM_ABI_API core::OwnedPtr<IApplicationHost> create_application_host();
```

---

## 设计理念

### 纯接口层

**特征**：
- `mine.platform.abi` 是纯接口层（头文件库）
- 无实际二进制输出
- 无需 DLL 导出

**实现**：
- `MINE_PLATFORM_ABI_API` 为空占位
- 接口类和函数声明无需 `__declspec(dllexport)`

---

### 与其他模块的导出宏对比

| 模块 | 导出宏 | 定义 |
|------|--------|------|
| `mine.core` | `MINE_CORE_API` | `__declspec(dllexport)` / `__declspec(dllimport)` |
| `mine.containers` | `MINE_CONTAINERS_API` | `__declspec(dllexport)` / `__declspec(dllimport)` |
| `mine.math` | `MINE_MATH_API` | 空占位（头文件库） |
| `mine.platform.abi` | `MINE_PLATFORM_ABI_API` | 空占位（头文件库） |

---

## 最佳实践

### 1. 接口类标记

```cpp
// ✅ 推荐：接口类标记
class MINE_PLATFORM_ABI_API IWindow {
    // ...
};

// ✅ 可选：省略标记（接口层无需导出）
class IWindow {
    // ...
};
```

---

### 2. 函数声明标记

```cpp
// ✅ 推荐：函数声明标记
MINE_PLATFORM_ABI_API core::OwnedPtr<IApplicationHost> create_application_host();

// ✅ 可选：省略标记（接口层无需导出）
core::OwnedPtr<IApplicationHost> create_application_host();
```

---

## 完整示例

```cpp
#include <mine/platform/Api.h>

// 接口类
class MINE_PLATFORM_ABI_API IWindow {
public:
    virtual ~IWindow() = default;
    virtual void show() = 0;
    virtual void hide() = 0;
    virtual void close() = 0;
};

// 工厂函数
MINE_PLATFORM_ABI_API core::OwnedPtr<IWindow> create_window();
```

---

## 总结

`Api.h` 定义了 `mine.platform.abi` 模块的 API 导出宏 `MINE_PLATFORM_ABI_API`，具备：

1. **导出宏**：`MINE_PLATFORM_ABI_API` 为空占位
2. **纯接口层**：`mine.platform.abi` 是头文件库，无实际二进制输出
3. **未来扩展**：预留用于文档与未来扩展

**使用建议**：
- 接口类和函数声明可标记 `MINE_PLATFORM_ABI_API`
- 也可省略标记（接口层无需 DLL 导出）
- 与其他模块的导出宏格式保持一致
