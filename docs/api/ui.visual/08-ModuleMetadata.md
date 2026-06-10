# mine.ui.visual 模块元数据文件

## 概述

mine.ui.visual 模块包含以下元数据文件：

1. **ModuleTag.h**：模块名称常量
2. **Api.h**：DLL 导出/导入宏定义
3. **VisualAll.h**：伞形头文件（一次性包含所有公共类型）

---

## ModuleTag.h

### 文件位置

```
src/mine/ui/visual/api/include/mine/ui/visual/ModuleTag.h
```

### 内容

```cpp
namespace mine::ui {

/// 模块名称常量，用于日志和诊断信息标识
inline constexpr const char* kVisualModuleName = "mine.ui.visual";

} // namespace mine::ui
```

### 用途

- 用于日志和诊断信息标识
- 可供运行时查询模块名称

### 使用示例

```cpp
#include <mine/ui/visual/ModuleTag.h>
#include <iostream>

using namespace mine::ui;

int main() {
    std::cout << "当前模块: " << kVisualModuleName << std::endl;
    // 输出：当前模块: mine.ui.visual
    return 0;
}
```

---

## Api.h

### 文件位置

```
src/mine/ui/visual/api/include/mine/ui/visual/Api.h
```

### 内容

```cpp
#if defined(_MSC_VER) || defined(__MINGW32__)
#   if defined(MINE_BUILDING_MINE_UI_VISUAL)
#       define MINE_UI_VISUAL_API __declspec(dllexport)
#   elif defined(MINE_BUILD_SHARED)
#       define MINE_UI_VISUAL_API __declspec(dllimport)
#   else
#       define MINE_UI_VISUAL_API
#   endif
#elif defined(__GNUC__) || defined(__clang__)
#   if defined(MINE_BUILDING_MINE_UI_VISUAL)
#       define MINE_UI_VISUAL_API __attribute__((visibility("default")))
#   else
#       define MINE_UI_VISUAL_API
#   endif
#else
#   define MINE_UI_VISUAL_API
#endif
```

### 用途

- DLL 导出/导入宏定义
- 用于控制符号可见性（Windows、Linux）

### 宏定义

| 宏名称 | 含义 |
|--------|------|
| `MINE_UI_VISUAL_API` | 类/函数导出/导入修饰符 |

### 使用场景

**Windows (MSVC / MinGW)：**
- `MINE_BUILDING_MINE_UI_VISUAL` 定义时：`__declspec(dllexport)`（构建 DLL）
- `MINE_BUILD_SHARED` 定义时：`__declspec(dllimport)`（使用 DLL）
- 否则：空宏（静态链接）

**Linux / macOS (GCC / Clang)：**
- `MINE_BUILDING_MINE_UI_VISUAL` 定义时：`__attribute__((visibility("default")))`（构建共享库）
- 否则：空宏（静态链接）

### 使用示例

```cpp
#include <mine/ui/visual/Api.h>

// 导出类
class MINE_UI_VISUAL_API MyClass {
public:
    void my_method();
};

// 导出函数
MINE_UI_VISUAL_API void my_function();
```

---

## VisualAll.h

### 文件位置

```
src/mine/ui/visual/api/include/mine/ui/visual/VisualAll.h
```

### 内容

```cpp
#pragma once

#include <mine/ui/visual/Api.h>
#include <mine/ui/visual/ModuleTag.h>
#include <mine/ui/visual/Visibility.h>
#include <mine/ui/visual/HorizontalAlignment.h>
#include <mine/ui/visual/VerticalAlignment.h>
#include <mine/ui/visual/Visual.h>
#include <mine/ui/visual/UIElement.h>
#include <mine/ui/visual/FrameworkElement.h>
#include <mine/ui/visual/Control.h>
```

### 用途

- 伞形头文件（Umbrella Header）
- 一次性包含 mine.ui.visual 模块所有公共类型
- 简化头文件包含

### 包含文件列表

| 头文件 | 描述 |
|--------|------|
| `Api.h` | DLL 导出/导入宏 |
| `ModuleTag.h` | 模块名称常量 |
| `Visibility.h` | 可见性枚举 |
| `HorizontalAlignment.h` | 水平对齐枚举 |
| `VerticalAlignment.h` | 垂直对齐枚举 |
| `Visual.h` | Visual 基类 |
| `UIElement.h` | UIElement 基类 |
| `FrameworkElement.h` | FrameworkElement 基类 |
| `Control.h` | Control 基类 |

### 使用示例

```cpp
// 方式 1：包含单个头文件
#include <mine/ui/visual/Visual.h>
#include <mine/ui/visual/UIElement.h>
#include <mine/ui/visual/Control.h>

// 方式 2：包含伞形头文件（一次性包含所有）
#include <mine/ui/visual/VisualAll.h>

using namespace mine::ui;

int main() {
    Visual visual;
    UIElement element;
    Control control;
    
    // 使用 mine.ui.visual 模块所有类型
    return 0;
}
```

---

## 最佳实践

### 1. 优先包含单个头文件

```cpp
// ✅ 推荐：仅包含需要的头文件
#include <mine/ui/visual/Visual.h>
#include <mine/ui/visual/UIElement.h>

// ❌ 不推荐：包含伞形头文件（编译时间增加）
#include <mine/ui/visual/VisualAll.h>
```

---

### 2. 仅在必要时包含伞形头文件

```cpp
// ✅ 推荐：测试代码或原型代码使用伞形头文件
#include <mine/ui/visual/VisualAll.h>

// 快速原型开发，无需关心单个头文件

// ✅ 推荐：生产代码使用单个头文件
#include <mine/ui/visual/Visual.h>
#include <mine/ui/visual/UIElement.h>

// 减少编译时间和依赖
```

---

### 3. 正确使用导出宏

```cpp
// ✅ 推荐：在类声明中使用导出宏
class MINE_UI_VISUAL_API MyClass {
public:
    void my_method();
};

// ❌ 不推荐：在类定义中使用导出宏
class MyClass {
public:
    MINE_UI_VISUAL_API void my_method();  // ❌ 仅在声明中使用
};
```

---

## 总结

mine.ui.visual 模块元数据文件包括：

1. **ModuleTag.h**：模块名称常量（`kVisualModuleName = "mine.ui.visual"`）
2. **Api.h**：DLL 导出/导入宏（`MINE_UI_VISUAL_API`）
3. **VisualAll.h**：伞形头文件（包含所有公共类型）

**使用建议**：
- 优先包含单个头文件（减少编译时间）
- 仅在测试代码或原型代码中使用伞形头文件
- 正确使用导出宏（在类声明中使用，不在成员函数中重复使用）
