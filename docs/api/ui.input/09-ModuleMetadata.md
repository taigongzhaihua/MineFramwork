# mine.ui.input 模块元数据文件

## 概述

本文档介绍 `mine.ui.input` 模块的元数据文件：

- **ModuleTag.h**：模块标签类型（用于模板特化等场景）
- **Api.h**：DLL 导出宏定义
- **InputAll.h**：伞形头文件（包含所有公开接口）

这些文件是模块的基础设施，用于模块标识、动态链接库导出和便捷引入。

---

## ModuleTag.h

### 概述

`ModuleTag.h` 定义了 `mine.ui.input` 模块的标签类型。

**用途：**
- 模板特化：区分不同模块的类型
- 类型萃取：在编译期识别模块归属
- 模块标识：用于依赖注入、模块管理等场景

---

### 文件位置

```
src/mine/ui/input/api/include/mine/ui/input/ModuleTag.h
```

---

### 类型定义

```cpp
namespace mine::ui::input {

/// @brief mine.ui.input 模块标签类型。
struct ModuleTag {};

} // namespace mine::ui::input
```

---

### 使用场景

#### 1. 模板特化

```cpp
#include <mine/ui/input/ModuleTag.h>

using namespace mine::ui::input;

// 模板泛化
template <typename ModuleTag>
class ModuleManager {
    static const char* module_name() {
        return "Unknown";
    }
};

// 针对 mine.ui.input 模块特化
template <>
class ModuleManager<input::ModuleTag> {
    static const char* module_name() {
        return "mine.ui.input";
    }
};
```

---

#### 2. 类型萃取

```cpp
template <typename T>
struct module_traits;

// 针对 mine.ui.input 模块特化
template <>
struct module_traits<input::ModuleTag> {
    using module_type = input::ModuleTag;
    static constexpr const char* module_name = "mine.ui.input";
};
```

---

## Api.h

### 概述

`Api.h` 定义了 `mine.ui.input` 模块的 DLL 导出宏 `MINE_UI_INPUT_API`。

**用途：**
- **Windows（MSVC / MinGW）**：使用 `__declspec(dllexport)` / `__declspec(dllimport)`
- **Linux / macOS（GCC / Clang）**：使用 `__attribute__((visibility("default")))`
- **静态链接**：宏展开为空

---

### 文件位置

```
src/mine/ui/input/api/include/mine/ui/input/Api.h
```

---

### 宏定义

```cpp
#if defined(_MSC_VER) || defined(__MINGW32__)
#    if defined(MINE_BUILDING_MINE_UI_INPUT)
#        define MINE_UI_INPUT_API __declspec(dllexport)
#    elif defined(MINE_BUILD_SHARED)
#        define MINE_UI_INPUT_API __declspec(dllimport)
#    else
#        define MINE_UI_INPUT_API
#    endif
#elif defined(__GNUC__) || defined(__clang__)
#    if defined(MINE_BUILDING_MINE_UI_INPUT)
#        define MINE_UI_INPUT_API __attribute__((visibility("default")))
#    else
#        define MINE_UI_INPUT_API
#    endif
#else
#    define MINE_UI_INPUT_API
#endif
```

---

### 宏说明

| 宏 | 说明 |
|----|------|
| `MINE_UI_INPUT_API` | DLL 导出宏（公开符号） |
| `MINE_BUILDING_MINE_UI_INPUT` | 构建 `mine.ui.input` 模块时定义（导出符号） |
| `MINE_BUILD_SHARED` | 构建共享库（DLL）时定义（导入符号） |

---

### 使用场景

#### 1. 导出类

```cpp
#include <mine/ui/input/Api.h>

namespace mine::ui::input {

// 导出类
class MINE_UI_INPUT_API InputRouter {
public:
    void set_root(UIElement* root) noexcept;
    UIElement* root() const noexcept;
};

} // namespace mine::ui::input
```

---

#### 2. 导出函数

```cpp
#include <mine/ui/input/Api.h>

namespace mine::ui::input {

// 导出函数
MINE_UI_INPUT_API const RoutedEvent& KeyDownEvent();
MINE_UI_INPUT_API const RoutedEvent& MouseDownEvent();

} // namespace mine::ui::input
```

---

## InputAll.h

### 概述

`InputAll.h` 是 `mine.ui.input` 模块的伞形头文件，包含所有公开接口。

**用途：**
- 一次性引入所有公开接口
- 简化头文件包含
- 便于快速使用模块

---

### 文件位置

```
src/mine/ui/input/api/include/mine/ui/input/InputAll.h
```

---

### 包含内容

```cpp
#pragma once

#include <mine/ui/input/Api.h>
#include <mine/ui/input/Key.h>
#include <mine/ui/input/KeyEventArgs.h>
#include <mine/ui/input/ModifierKeys.h>
#include <mine/ui/input/MouseButton.h>
#include <mine/ui/input/MouseEventArgs.h>
#include <mine/ui/input/TextInputEventArgs.h>
#include <mine/ui/input/InputEvents.h>
#include <mine/ui/input/InputRouter.h>
```

---

### 包含文件列表

| 文件 | 说明 |
|------|------|
| `Api.h` | DLL 导出宏定义 |
| `Key.h` | 虚拟键枚举 |
| `KeyEventArgs.h` | 键盘事件参数类 |
| `ModifierKeys.h` | 修饰键枚举 |
| `MouseButton.h` | 鼠标按钮枚举 |
| `MouseEventArgs.h` | 鼠标事件参数类 |
| `TextInputEventArgs.h` | 字符输入事件参数类 |
| `InputEvents.h` | 输入事件声明 |
| `InputRouter.h` | 输入路由器类 |

---

### 使用场景

#### 1. 快速引入所有接口

```cpp
// 一次性引入所有接口
#include <mine/ui/input/InputAll.h>

using namespace mine::ui::input;

// 直接使用所有类型
Key key = Key::Enter;
ModifierKeys modifiers = ModifierKeys::Ctrl;
MouseButton button = MouseButton::Left;
InputRouter router;
```

---

#### 2. 精细控制引入（推荐）

```cpp
// 仅引入需要的头文件（减少编译依赖）
#include <mine/ui/input/Key.h>
#include <mine/ui/input/ModifierKeys.h>
#include <mine/ui/input/KeyEventArgs.h>

using namespace mine::ui::input;

// 仅使用引入的类型
Key key = Key::Enter;
ModifierKeys modifiers = ModifierKeys::Ctrl;
```

---

## 最佳实践

### 1. 优先使用精细引入

```cpp
// ✅ 推荐：仅引入需要的头文件
#include <mine/ui/input/Key.h>
#include <mine/ui/input/ModifierKeys.h>

// ❌ 不推荐：引入所有接口（增加编译依赖）
#include <mine/ui/input/InputAll.h>
```

---

### 2. 使用 MINE_UI_INPUT_API 导出公开符号

```cpp
// ✅ 推荐：使用 MINE_UI_INPUT_API 导出公开符号
class MINE_UI_INPUT_API MyClass {
public:
    void public_method();
};

// ❌ 不推荐：忘记导出宏（符号不可见）
class MyClass {  // ❌ 缺少 MINE_UI_INPUT_API
public:
    void public_method();
};
```

---

### 3. ModuleTag 用于模板特化

```cpp
// ✅ 推荐：使用 ModuleTag 进行模板特化
template <>
class ModuleManager<input::ModuleTag> {
    // 特化实现
};

// ❌ 不推荐：使用字符串标识模块（运行期开销）
class ModuleManager {
    std::string module_name_;
};
```

---

## 常见陷阱

### 1. 忘记包含 Api.h

```cpp
// ❌ 问题：忘记包含 Api.h
class InputRouter {  // ❌ 缺少 MINE_UI_INPUT_API
public:
    void set_root(UIElement* root);
};

// ✅ 解决：包含 Api.h 并使用宏
#include <mine/ui/input/Api.h>

class MINE_UI_INPUT_API InputRouter {
public:
    void set_root(UIElement* root);
};
```

---

### 2. 滥用 InputAll.h

```cpp
// ❌ 问题：滥用 InputAll.h（增加编译依赖）
#include <mine/ui/input/InputAll.h>

// 仅使用 Key 枚举
Key key = Key::Enter;

// ✅ 解决：仅引入需要的头文件
#include <mine/ui/input/Key.h>

Key key = Key::Enter;
```

---

### 3. 混淆 dllexport 和 dllimport

```cpp
// ⚠️ 注意：构建模块时定义 MINE_BUILDING_MINE_UI_INPUT

// 构建 mine.ui.input 模块时：
// #define MINE_BUILDING_MINE_UI_INPUT
// → MINE_UI_INPUT_API → __declspec(dllexport)

// 使用 mine.ui.input 模块时：
// 未定义 MINE_BUILDING_MINE_UI_INPUT
// → MINE_UI_INPUT_API → __declspec(dllimport)
```

---

## 总结

### ModuleTag.h

- **模块标签类型**（`struct ModuleTag {}`）
- 用于模板特化、类型萃取、模块标识

### Api.h

- **DLL 导出宏**（`MINE_UI_INPUT_API`）
- Windows：`__declspec(dllexport)` / `__declspec(dllimport)`
- Linux / macOS：`__attribute__((visibility("default")))`

### InputAll.h

- **伞形头文件**（包含所有公开接口）
- 用于快速引入所有接口
- 推荐使用精细引入（减少编译依赖）

**使用建议：**
- **精细引入**（仅引入需要的头文件）
- **使用导出宏**（MINE_UI_INPUT_API）
- **ModuleTag 用于模板特化**
- 避免忘记包含 Api.h
- 避免滥用 InputAll.h
- 避免混淆 dllexport 和 dllimport
