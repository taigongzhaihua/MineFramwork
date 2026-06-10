# 元文件（Api + Property）详细接口文档

## 概述

`Api.h` 和 `Property.h` 是 `mine.ui.property` 模块的**元文件**。

**核心特性：**
- **Api.h**：定义符号导出宏 MINE_UI_PROPERTY_API
- **Property.h**：伞形头文件，一次性包含所有公共类型

---

## 文件位置

```
src/mine/ui/property/api/include/mine/ui/property/Api.h
src/mine/ui/property/api/include/mine/ui/property/Property.h
```

---

## Api.h

### 描述

定义 mine.ui.property 模块的符号导出宏。

### 宏定义

#### MINE_UI_PROPERTY_API

```cpp
// Windows
#if defined(_WIN32)
#    if defined(MINE_SHARED_BUILD)
#        if defined(MINE_BUILDING_MINE_UI_PROPERTY)
#            define MINE_UI_PROPERTY_API __declspec(dllexport)
#        else
#            define MINE_UI_PROPERTY_API __declspec(dllimport)
#        endif
#    else
#        define MINE_UI_PROPERTY_API
#    endif
// Linux/macOS
#else
#    if defined(MINE_SHARED_BUILD)
#        define MINE_UI_PROPERTY_API __attribute__((visibility("default")))
#    else
#        define MINE_UI_PROPERTY_API
#    endif
#endif
```

**描述**：mine.ui.property 模块的符号导出宏。

**用途**：
- Windows 动态库：__declspec(dllexport) / __declspec(dllimport)
- Linux/macOS 动态库：__attribute__((visibility("default")))
- 静态库：空宏

**使用场景**：
- 类声明：`class MINE_UI_PROPERTY_API DependencyObject { ... };`
- 函数声明：`MINE_UI_PROPERTY_API const DependencyProperty& register_property(...);`

---

## Property.h

### 描述

mine.ui.property 模块伞形头文件，一次性包含所有公共类型。

### 包含的头文件

```cpp
#pragma once

#include <mine/ui/property/Api.h>
#include <mine/ui/property/ModuleTag.h>
#include <mine/ui/property/ValuePriority.h>
#include <mine/ui/property/PropertyMetadata.h>
#include <mine/ui/property/DependencyProperty.h>
#include <mine/ui/property/DependencyObject.h>
```

**包含的类型**：
- `Api.h`：MINE_UI_PROPERTY_API 符号导出宏
- `ModuleTag.h`：kPropertyModuleName 模块标识常量
- `ValuePriority.h`：ValuePriority 枚举（依赖属性值优先级）
- `PropertyMetadata.h`：PropertyMetadata 结构体、PropertyChangedFn 回调类型
- `DependencyProperty.h`：DependencyProperty 类、register_property 注册函数
- `DependencyObject.h`：DependencyObject 类、PropertyChangedCallbackFn 回调类型

**使用场景**：
- 一次性包含所有 mine.ui.property 模块的公共类型
- 简化头文件包含

---

## 使用场景

### 1. 使用伞形头文件简化包含

```cpp
// ✅ 推荐：使用伞形头文件
#include <mine/ui/property/Property.h>

// 所有类型均可用
using namespace mine::ui;

void example() {
    DependencyObject obj;
    const DependencyProperty& prop = register_property<MyClass>("Width", 0.0, PropertyMetadata{});
    obj.set_value(prop, Variant{100.0}, ValuePriority::Local);
}
```

---

### 2. 按需包含单个头文件

```cpp
// ✅ 推荐：按需包含单个头文件（减少编译时间）
#include <mine/ui/property/DependencyObject.h>
#include <mine/ui/property/DependencyProperty.h>

// 仅使用 DependencyObject 和 DependencyProperty
```

---

### 3. 使用符号导出宏

```cpp
// 声明导出类
class MINE_UI_PROPERTY_API MyClass {
public:
    void foo();
};

// 声明导出函数
MINE_UI_PROPERTY_API void my_function();
```

---

## 最佳实践

### 1. 按需包含头文件（减少编译时间）

```cpp
// ✅ 推荐：仅包含需要的头文件
#include <mine/ui/property/DependencyObject.h>

// ❌ 不推荐：包含伞形头文件（增加编译时间）
#include <mine/ui/property/Property.h>  // 包含了所有头文件
```

---

### 2. 使用伞形头文件简化包含（开发阶段）

```cpp
// ✅ 推荐：开发阶段使用伞形头文件
#include <mine/ui/property/Property.h>

// 后续优化：替换为按需包含
#include <mine/ui/property/DependencyObject.h>
#include <mine/ui/property/DependencyProperty.h>
```

---

## 总结

`Api.h` 和 `Property.h` 是 `mine.ui.property` 模块的元文件，具备：

1. **Api.h**：定义符号导出宏 MINE_UI_PROPERTY_API（Windows/Linux/macOS 动态库导出）
2. **Property.h**：伞形头文件，一次性包含所有公共类型

**使用建议**：
- 按需包含单个头文件（减少编译时间）
- 使用伞形头文件简化包含（开发阶段）
- 使用符号导出宏声明导出类和函数
