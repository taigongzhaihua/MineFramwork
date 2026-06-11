# ModuleMetadata - 模块元数据

## 1. 概述

`mine.ui.window` 模块的元数据文件包含模块导出声明(`Api.h`)和伞形头文件(`WindowAll.h`)。这些文件定义了模块的 DLL 导出/导入宏、模块接口边界、以及便捷的一次性包含机制。

### 核心功能

- **DLL 导出/导入**：`Api.h` 定义 `MINE_UI_WINDOW_API` 宏,控制符号的 DLL 导出/导入
- **模块接口边界**：通过导出宏明确模块的公共 API,隐藏实现细节
- **伞形头文件**：`WindowAll.h` 提供一次性包含所有公共头文件的便捷方式
- **跨平台兼容**：支持 Windows(`__declspec`)、Linux/macOS(`__attribute__`)、静态库编译

### 关键特性

- **条件编译**：根据 `MINE_UI_WINDOW_EXPORT` 宏区分导出和导入
- **静态库支持**：定义 `MINE_UI_WINDOW_STATIC` 时禁用 DLL 导出/导入
- **平台检测**：自动检测 Windows/Unix 平台,使用对应的导出语法
- **ABI 稳定性**：通过导出宏控制 ABI 边界,避免跨模块 inline 函数问题
- **编译隔离**：非导出符号不会出现在 DLL 导出表中,减小二进制体积

### 依赖关系

```
┌──────────────────────────────────────────────────────────┐
│                     mine.ui.window                       │
│  ┌────────────────────────────────────────────────────┐ │
│  │ Api.h                                              │ │
│  │  - 定义 MINE_UI_WINDOW_API 宏                     │ │
│  │  - 控制 DLL 导出/导入                             │ │
│  └───────────┬────────────────────────────────────────┘ │
│              │ included by                               │
│  ┌───────────▼────────────────────────────────────────┐ │
│  │ Window.h, WindowContext.h, etc.                    │ │
│  │  - 使用 MINE_UI_WINDOW_API 标记导出类/函数       │ │
│  └───────────┬────────────────────────────────────────┘ │
│              │ all included by                           │
│  ┌───────────▼────────────────────────────────────────┐ │
│  │ WindowAll.h                                        │ │
│  │  - 伞形头文件,一次性包含所有公共头文件           │ │
│  └────────────────────────────────────────────────────┘ │
└──────────────────────────────────────────────────────────┘
```

### 与其他框架对比

| 框架 | 导出宏 | 伞形头文件 | 静态库支持 |
|------|--------|-----------|-----------|
| **Qt** | `Q_DECL_EXPORT` / `Q_DECL_IMPORT` | `<QtWidgets>` | 支持 |
| **Boost** | `BOOST_SYMBOL_EXPORT` / `BOOST_SYMBOL_IMPORT` | 无(按需包含) | 支持 |
| **WPF** | 内部实现,不对外暴露 | 无(C#程序集) | N/A |
| **MineFramework** | `MINE_UI_WINDOW_API` | `WindowAll.h` | 支持 |

MineFramework 的设计优势：
- **清晰的模块边界**：每个模块都有独立的 `Api.h`,避免符号冲突
- **一致的命名规范**：所有模块的导出宏命名格式一致(`MINE_<MODULE>_API`)
- **灵活的构建模式**：支持动态库和静态库编译,无需修改代码

---

## 2. 文件位置

**头文件路径**：
```
src/mine/ui/window/api/include/mine/ui/window/Api.h
src/mine/ui/window/api/include/mine/ui/window/WindowAll.h
```

**依赖模块**：
- `mine.core`：基础宏定义(如 `MINE_API_EXPORT` / `MINE_API_IMPORT`)

**相关文件**：
- `Window.h`：使用 `MINE_UI_WINDOW_API` 标记导出类
- `WindowContext.h`：使用 `MINE_UI_WINDOW_API` 标记导出函数

---

## 3. 类定义

### 3.1 Api.h

```cpp
/// @file Api.h
/// @brief mine.ui.window 模块 DLL 导出/导入宏定义
/// @details 
/// 定义 MINE_UI_WINDOW_API 宏,用于标记需要导出的类和函数。
/// 支持动态库(DLL/SO)和静态库两种编译模式。

#pragma once

// ========== 平台检测 ==========

#if defined(_WIN32) || defined(_WIN64)
    #define MINE_PLATFORM_WINDOWS 1
#elif defined(__unix__) || defined(__APPLE__)
    #define MINE_PLATFORM_UNIX 1
#else
    #error "Unsupported platform"
#endif

// ========== DLL 导出/导入宏 ==========

#if defined(MINE_UI_WINDOW_STATIC)
    // 静态库编译:无需导出/导入
    #define MINE_UI_WINDOW_API
#elif defined(MINE_UI_WINDOW_EXPORT)
    // 动态库编译(编译 mine.ui.window 本身):导出符号
    #if defined(MINE_PLATFORM_WINDOWS)
        #define MINE_UI_WINDOW_API __declspec(dllexport)
    #elif defined(MINE_PLATFORM_UNIX)
        #define MINE_UI_WINDOW_API __attribute__((visibility("default")))
    #else
        #define MINE_UI_WINDOW_API
    #endif
#else
    // 动态库编译(使用 mine.ui.window):导入符号
    #if defined(MINE_PLATFORM_WINDOWS)
        #define MINE_UI_WINDOW_API __declspec(dllimport)
    #elif defined(MINE_PLATFORM_UNIX)
        #define MINE_UI_WINDOW_API
    #else
        #define MINE_UI_WINDOW_API
    #endif
#endif

// ========== 使用示例 ==========

// 导出类
// class MINE_UI_WINDOW_API Window { ... };

// 导出函数
// MINE_UI_WINDOW_API void set_application_window_context(IWindowContext* ctx);

// 导出全局变量(不推荐)
// extern MINE_UI_WINDOW_API int global_variable;
```

---

### 3.2 WindowAll.h

```cpp
/// @file WindowAll.h
/// @brief mine.ui.window 模块伞形头文件
/// @details 
/// 一次性包含 mine.ui.window 模块的所有公共头文件。
/// 适合快速原型开发或单元测试,生产代码建议按需包含。

#pragma once

// ========== 模块元数据 ==========
#include <mine/ui/window/Api.h>

// ========== 核心类 ==========
#include <mine/ui/window/Window.h>
#include <mine/ui/window/WindowContext.h>

// ========== 使用示例 ==========

// 方式 1:包含伞形头文件(快速原型)
// #include <mine/ui/window/WindowAll.h>
//
// ui::Window window;
// window.show();

// 方式 2:按需包含(生产代码,推荐)
// #include <mine/ui/window/Window.h>
//
// ui::Window window;
// window.show();
```

---

## 4. 成员方法

### 4.1 MINE_UI_WINDOW_API 宏

**功能说明**：
DLL 导出/导入宏,用于标记需要跨模块导出的类、函数、全局变量。

**参数列表**：无(预处理器宏)

**返回值**：无(预处理器宏)

**展开结果**：
- **静态库编译**(`MINE_UI_WINDOW_STATIC` 已定义)：展开为空
- **动态库编译,编译模块本身**(`MINE_UI_WINDOW_EXPORT` 已定义)：
  - Windows: `__declspec(dllexport)`
  - Unix: `__attribute__((visibility("default")))`
- **动态库编译,使用模块**(`MINE_UI_WINDOW_EXPORT` 未定义)：
  - Windows: `__declspec(dllimport)`
  - Unix: 展开为空

**使用场景**：
- 标记导出类
- 标记导出函数
- 标记导出全局变量(不推荐)

**示例**：
```cpp
// 导出类
class MINE_UI_WINDOW_API Window {
public:
    void show();
};

// 导出函数
MINE_UI_WINDOW_API void set_application_window_context(IWindowContext* ctx);

// 导出全局变量(不推荐,容易导致 ODR 问题)
extern MINE_UI_WINDOW_API int global_counter;
```

---

### 4.2 WindowAll.h 伞形头文件

**功能说明**：
一次性包含 `mine.ui.window` 模块的所有公共头文件。

**参数列表**：无(头文件)

**返回值**：无(头文件)

**包含的头文件**：
- `Api.h`：导出宏定义
- `Window.h`：Window 类
- `WindowContext.h`：IWindowContext 接口

**使用场景**：
- 快速原型开发
- 单元测试
- 示例代码

**不推荐使用场景**：
- 生产代码(增加编译时间)
- 需要精确控制依赖的场景

**示例**：
```cpp
// 方式 1:包含伞形头文件
#include <mine/ui/window/WindowAll.h>

ui::Window window;
window.show();

// 方式 2:按需包含(推荐)
#include <mine/ui/window/Window.h>

ui::Window window;
window.show();
```

---

## 5. 使用场景

### 场景1：动态库编译(编译模块本身)

**业务需求**：编译 `mine.ui.window` 模块为动态库(DLL/SO)。

**代码示例**：

**CMakeLists.txt**：
```cmake
# 编译 mine.ui.window 为动态库
add_library(mine_ui_window SHARED
    src/Window.cpp
    src/WindowContext.cpp
)

# 定义导出宏
target_compile_definitions(mine_ui_window PRIVATE
    MINE_UI_WINDOW_EXPORT  # 导出符号
)

# 链接依赖
target_link_libraries(mine_ui_window PUBLIC
    mine_core
    mine_ui_property
    mine_platform_abi
)

# 安装目标
install(TARGETS mine_ui_window
    RUNTIME DESTINATION bin
    LIBRARY DESTINATION lib
    ARCHIVE DESTINATION lib
)
```

**Window.h**：
```cpp
#include <mine/ui/window/Api.h>

namespace mine::ui {

// ✅ MINE_UI_WINDOW_API 展开为 __declspec(dllexport) (Windows)
class MINE_UI_WINDOW_API Window {
public:
    void show();
};

} // namespace mine::ui
```

**预期效果**：
- Windows: 生成 `mine_ui_window.dll` 和 `mine_ui_window.lib`
- Linux: 生成 `libmine_ui_window.so`
- Window 类符号出现在 DLL 导出表中

---

### 场景2：动态库编译(使用模块)

**业务需求**：在应用代码中使用 `mine.ui.window` 动态库。

**代码示例**：

**CMakeLists.txt**：
```cmake
# 应用程序
add_executable(myapp
    src/main.cpp
)

# 链接动态库
target_link_libraries(myapp PRIVATE
    mine_ui_window  # 动态库
)

# 不定义 MINE_UI_WINDOW_EXPORT,使用导入模式
```

**main.cpp**：
```cpp
#include <mine/ui/window/Window.h>

// ✅ MINE_UI_WINDOW_API 展开为 __declspec(dllimport) (Windows)
// Window 类符号从 DLL 导入

int main() {
    mine::ui::Window window;
    window.set_title("My Application");
    window.show();
    return 0;
}
```

**预期效果**：
- Windows: `myapp.exe` 链接 `mine_ui_window.lib`,运行时加载 `mine_ui_window.dll`
- Linux: `myapp` 链接 `libmine_ui_window.so`

---

### 场景3：静态库编译

**业务需求**：编译 `mine.ui.window` 模块为静态库,避免 DLL 依赖。

**代码示例**：

**CMakeLists.txt**：
```cmake
# 编译 mine.ui.window 为静态库
add_library(mine_ui_window STATIC
    src/Window.cpp
    src/WindowContext.cpp
)

# 定义静态库宏
target_compile_definitions(mine_ui_window PUBLIC
    MINE_UI_WINDOW_STATIC  # ✅ 禁用 DLL 导出/导入
)

# 链接依赖
target_link_libraries(mine_ui_window PUBLIC
    mine_core
    mine_ui_property
    mine_platform_abi
)
```

**Window.h**：
```cpp
#include <mine/ui/window/Api.h>

namespace mine::ui {

// ✅ MINE_UI_WINDOW_API 展开为空(静态库)
class MINE_UI_WINDOW_API Window {
public:
    void show();
};

} // namespace mine::ui
```

**预期效果**：
- Windows: 生成 `mine_ui_window.lib`(静态库)
- Linux: 生成 `libmine_ui_window.a`
- Window 类符号直接链接到应用程序,无需 DLL

---

### 场景4：跨平台兼容

**业务需求**：确保代码在 Windows 和 Linux 上都能正确编译。

**代码示例**：

**Api.h**：
```cpp
#pragma once

// ✅ 平台检测
#if defined(_WIN32) || defined(_WIN64)
    #define MINE_PLATFORM_WINDOWS 1
#elif defined(__unix__) || defined(__APPLE__)
    #define MINE_PLATFORM_UNIX 1
#else
    #error "Unsupported platform"
#endif

// ✅ 跨平台导出宏
#if defined(MINE_UI_WINDOW_STATIC)
    #define MINE_UI_WINDOW_API
#elif defined(MINE_UI_WINDOW_EXPORT)
    #if defined(MINE_PLATFORM_WINDOWS)
        #define MINE_UI_WINDOW_API __declspec(dllexport)
    #elif defined(MINE_PLATFORM_UNIX)
        #define MINE_UI_WINDOW_API __attribute__((visibility("default")))
    #endif
#else
    #if defined(MINE_PLATFORM_WINDOWS)
        #define MINE_UI_WINDOW_API __declspec(dllimport)
    #elif defined(MINE_PLATFORM_UNIX)
        #define MINE_UI_WINDOW_API
    #endif
#endif
```

**预期效果**：
- Windows: 使用 `__declspec(dllexport/dllimport)`
- Linux/macOS: 使用 `__attribute__((visibility("default")))`

---

### 场景5：伞形头文件快速原型

**业务需求**：快速开发原型,一次性包含所有头文件。

**代码示例**：

**main.cpp**：
```cpp
// ✅ 一次性包含所有头文件
#include <mine/ui/window/WindowAll.h>

using namespace mine;

int main() {
    // ✅ Window 和 IWindowContext 都可用
    ui::Window window;
    window.set_title("Prototype");
    window.show();
    
    auto* ctx = ui::get_application_window_context();
    if (ctx) {
        auto& device = ctx->device();
        // ...
    }
    
    return 0;
}
```

**预期效果**：
- 所有 `mine.ui.window` 的公共 API 都可用
- 编译时间略长(包含了所有头文件)

---

### 场景6：按需包含优化编译时间

**业务需求**：生产代码中按需包含头文件,减少编译时间。

**代码示例**：

**main.cpp**：
```cpp
// ✅ 仅包含需要的头文件
#include <mine/ui/window/Window.h>

using namespace mine;

int main() {
    // ✅ 仅使用 Window 类
    ui::Window window;
    window.set_title("Production");
    window.show();
    
    return 0;
}
```

**预期效果**：
- 编译时间更短
- 减少不必要的符号引入

---

### 场景7：导出类模板(特殊场景)

**业务需求**：导出类模板的特化版本。

**代码示例**：

**WindowManager.h**：
```cpp
#include <mine/ui/window/Api.h>
#include <vector>

namespace mine::ui {

// ✅ 类模板本身不需要导出宏
template <typename T>
class WindowManager {
public:
    void add_window(T* window);
    T* get_window(size_t index);

private:
    std::vector<T*> m_windows;
};

// ✅ 显式实例化并导出特化版本
extern template class MINE_UI_WINDOW_API WindowManager<Window>;

} // namespace mine::ui
```

**WindowManager.cpp**：
```cpp
#include "WindowManager.h"
#include <mine/ui/window/Window.h>

namespace mine::ui {

// ✅ 显式实例化
template class WindowManager<Window>;

template <typename T>
void WindowManager<T>::add_window(T* window) {
    m_windows.push_back(window);
}

template <typename T>
T* WindowManager<T>::get_window(size_t index) {
    return index < m_windows.size() ? m_windows[index] : nullptr;
}

} // namespace mine::ui
```

**预期效果**：
- `WindowManager<Window>` 特化版本导出到 DLL
- 其他特化版本不导出(避免符号膨胀)

---

## 6. 最佳实践

### ✅ 实践1：始终使用导出宏标记公共 API

**说明**：
所有需要跨模块使用的类、函数、全局变量都应使用 `MINE_UI_WINDOW_API` 标记。

**✅ 推荐写法**：
```cpp
// ✅ 导出类
class MINE_UI_WINDOW_API Window {
public:
    void show();
};

// ✅ 导出自由函数
MINE_UI_WINDOW_API void set_application_window_context(IWindowContext* ctx);

// ✅ 导出模板特化
extern template class MINE_UI_WINDOW_API std::vector<Window*>;
```

**❌ 不推荐写法**：
```cpp
// ❌ 忘记标记导出宏
class Window {
public:
    void show();
};

// ❌ 导出宏位置错误
class Window MINE_UI_WINDOW_API {  // ❌ 应该在 class 之后
public:
    void show();
};
```

**原因**：
- 未标记导出宏会导致符号链接失败
- 导出宏位置错误会导致编译错误

---

### ✅ 实践2：避免导出内联函数

**说明**：
内联函数通常在头文件中定义,不应使用导出宏标记,否则可能导致 ODR 违规。

**✅ 推荐写法**：
```cpp
class MINE_UI_WINDOW_API Window {
public:
    // ✅ 内联函数不标记导出宏
    bool is_closed() const noexcept { return m_is_closed; }
    
private:
    bool m_is_closed = false;
};
```

**❌ 不推荐写法**：
```cpp
class MINE_UI_WINDOW_API Window {
public:
    // ❌ 内联函数标记导出宏(可能导致 ODR 违规)
    MINE_UI_WINDOW_API bool is_closed() const noexcept { return m_is_closed; }
    
private:
    bool m_is_closed = false;
};
```

**原因**：
- 内联函数在每个编译单元都会展开,不应导出
- 导出内联函数可能导致符号重复定义

---

### ✅ 实践3：生产代码按需包含,避免使用伞形头文件

**说明**：
伞形头文件适合快速原型,生产代码应按需包含以减少编译时间。

**✅ 推荐写法**：
```cpp
// ✅ 仅包含需要的头文件
#include <mine/ui/window/Window.h>

void create_window() {
    mine::ui::Window window;
    window.show();
}
```

**❌ 不推荐写法**：
```cpp
// ❌ 包含伞形头文件(增加编译时间)
#include <mine/ui/window/WindowAll.h>

void create_window() {
    mine::ui::Window window;
    window.show();
}
```

**原因**：
- 伞形头文件包含所有公共头文件,增加编译时间
- 按需包含更精确,减少不必要的符号引入

---

### ✅ 实践4：使用前置声明减少头文件依赖

**说明**：
在头文件中尽量使用前置声明,避免包含其他模块的头文件。

**✅ 推荐写法**：
```cpp
// Window.h

#pragma once

#include <mine/ui/window/Api.h>

// ✅ 前置声明
namespace mine::ui {
    class UIElement;
}

namespace mine::platform {
    class IWindow;
}

namespace mine::ui {

class MINE_UI_WINDOW_API Window {
public:
    void set_content(UIElement* element);  // ✅ 使用前置声明
    platform::IWindow& native_window();     // ✅ 使用前置声明

private:
    UIElement* m_content;
    platform::IWindow* m_native_window;
};

} // namespace mine::ui
```

**❌ 不推荐写法**：
```cpp
// Window.h

#pragma once

#include <mine/ui/window/Api.h>

// ❌ 包含其他模块头文件(增加编译依赖)
#include <mine/ui/visual/UIElement.h>
#include <mine/platform/abi/IWindow.h>

namespace mine::ui {

class MINE_UI_WINDOW_API Window {
public:
    void set_content(UIElement* element);
    platform::IWindow& native_window();

private:
    UIElement* m_content;
    platform::IWindow* m_native_window;
};

} // namespace mine::ui
```

**原因**：
- 前置声明减少编译依赖,加快编译速度
- 避免循环包含问题

---

## 7. 常见陷阱

### ❌ 陷阱1：忘记定义导出宏导致链接失败

**问题描述**：
编译动态库时忘记定义 `MINE_UI_WINDOW_EXPORT`,导致符号未导出,链接失败。

**❌ 错误代码**：

**CMakeLists.txt**：
```cmake
add_library(mine_ui_window SHARED
    src/Window.cpp
)

# ❌ 忘记定义 MINE_UI_WINDOW_EXPORT
```

**错误现象**：
- Windows: LNK2019 未解析的外部符号
- Linux: undefined reference

**✅ 正确代码**：

**CMakeLists.txt**：
```cmake
add_library(mine_ui_window SHARED
    src/Window.cpp
)

# ✅ 定义导出宏
target_compile_definitions(mine_ui_window PRIVATE
    MINE_UI_WINDOW_EXPORT
)
```

**原理解释**：
动态库编译时必须定义 `MINE_UI_WINDOW_EXPORT`,否则符号不会导出到 DLL。

---

### ❌ 陷阱2：静态库和动态库混用导致符号重复

**问题描述**：
同时链接静态库和动态库版本的同一个模块,导致符号重复定义。

**❌ 错误代码**：

**CMakeLists.txt**：
```cmake
# ❌ 同时链接静态库和动态库
target_link_libraries(myapp PRIVATE
    mine_ui_window_static  # 静态库
    mine_ui_window         # 动态库
)
```

**错误现象**：
- Windows: LNK2005 符号已定义
- Linux: multiple definition

**✅ 正确代码**：

**CMakeLists.txt**：
```cmake
# ✅ 仅链接一个版本
option(USE_STATIC_LIBS "Use static libraries" OFF)

if(USE_STATIC_LIBS)
    target_link_libraries(myapp PRIVATE mine_ui_window_static)
else()
    target_link_libraries(myapp PRIVATE mine_ui_window)
endif()
```

**原理解释**：
同一个符号不能同时出现在静态库和动态库中。

---

### ❌ 陷阱3：导出宏位置错误导致编译失败

**问题描述**：
导出宏放在错误的位置(如 `class` 之前),导致编译错误。

**❌ 错误代码**：
```cpp
// ❌ 导出宏位置错误
MINE_UI_WINDOW_API class Window {
public:
    void show();
};
```

**错误现象**：
- 编译错误: syntax error

**✅ 正确代码**：
```cpp
// ✅ 导出宏在 class 之后
class MINE_UI_WINDOW_API Window {
public:
    void show();
};

// ✅ 或在函数返回类型之前
MINE_UI_WINDOW_API void set_application_window_context(IWindowContext* ctx);
```

**原理解释**：
- 类导出宏应在 `class` 关键字之后
- 函数导出宏应在返回类型之前

---

### ❌ 陷阱4：伞形头文件包含私有头文件

**问题描述**：
伞形头文件错误地包含了私有头文件(如实现细节),导致外部代码访问到内部 API。

**❌ 错误代码**：

**WindowAll.h**：
```cpp
#pragma once

#include <mine/ui/window/Api.h>
#include <mine/ui/window/Window.h>
#include <mine/ui/window/WindowContext.h>

// ❌ 包含私有头文件
#include <mine/ui/window/internal/WindowImpl.h>
```

**错误现象**：
- 外部代码可以访问内部 API
- 破坏模块封装性

**✅ 正确代码**：

**WindowAll.h**：
```cpp
#pragma once

#include <mine/ui/window/Api.h>
#include <mine/ui/window/Window.h>
#include <mine/ui/window/WindowContext.h>

// ✅ 仅包含公共头文件
```

**原理解释**：
伞形头文件应仅包含公共 API,不应包含私有实现细节。

---

## 8. 完整示例

以下是一个完整的示例,展示如何配置 CMakeLists.txt、定义导出宏、导出类、外部使用的完整流程。

```cmake
# ========== CMakeLists.txt(mine.ui.window 模块) ==========

cmake_minimum_required(VERSION 3.20)
project(mine_ui_window VERSION 1.0.0 LANGUAGES CXX)

# ========== 编译选项 ==========

option(BUILD_SHARED_LIBS "Build shared libraries" ON)
option(MINE_UI_WINDOW_BUILD_STATIC "Build static library" OFF)

# ========== 源文件 ==========

set(MINE_UI_WINDOW_SOURCES
    src/Window.cpp
    src/WindowContext.cpp
)

set(MINE_UI_WINDOW_HEADERS
    api/include/mine/ui/window/Api.h
    api/include/mine/ui/window/Window.h
    api/include/mine/ui/window/WindowContext.h
    api/include/mine/ui/window/WindowAll.h
)

# ========== 目标定义 ==========

if(MINE_UI_WINDOW_BUILD_STATIC OR NOT BUILD_SHARED_LIBS)
    # 静态库
    add_library(mine_ui_window STATIC
        ${MINE_UI_WINDOW_SOURCES}
        ${MINE_UI_WINDOW_HEADERS}
    )
    
    # 定义静态库宏
    target_compile_definitions(mine_ui_window PUBLIC
        MINE_UI_WINDOW_STATIC
    )
else()
    # 动态库
    add_library(mine_ui_window SHARED
        ${MINE_UI_WINDOW_SOURCES}
        ${MINE_UI_WINDOW_HEADERS}
    )
    
    # 定义导出宏(仅编译模块本身时)
    target_compile_definitions(mine_ui_window PRIVATE
        MINE_UI_WINDOW_EXPORT
    )
endif()

# ========== 头文件路径 ==========

target_include_directories(mine_ui_window PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/api/include>
    $<INSTALL_INTERFACE:include>
)

# ========== 链接依赖 ==========

target_link_libraries(mine_ui_window PUBLIC
    mine_core
    mine_ui_property
    mine_platform_abi
    mine_gfx_rhi
    mine_paint
)

# ========== 编译选项 ==========

target_compile_features(mine_ui_window PUBLIC cxx_std_20)

if(MSVC)
    target_compile_options(mine_ui_window PRIVATE /W4)
else()
    target_compile_options(mine_ui_window PRIVATE -Wall -Wextra)
endif()

# ========== 安装配置 ==========

install(TARGETS mine_ui_window
    EXPORT mine_ui_window_targets
    RUNTIME DESTINATION bin
    LIBRARY DESTINATION lib
    ARCHIVE DESTINATION lib
)

install(DIRECTORY api/include/mine
    DESTINATION include
    FILES_MATCHING PATTERN "*.h"
)

install(EXPORT mine_ui_window_targets
    FILE mine_ui_window_targets.cmake
    NAMESPACE mine::
    DESTINATION lib/cmake/mine_ui_window
)

# ========== CMake 配置文件 ==========

include(CMakePackageConfigHelpers)

configure_package_config_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/mine_ui_window_config.cmake.in
    ${CMAKE_CURRENT_BINARY_DIR}/mine_ui_window_config.cmake
    INSTALL_DESTINATION lib/cmake/mine_ui_window
)

write_basic_package_version_file(
    ${CMAKE_CURRENT_BINARY_DIR}/mine_ui_window_config_version.cmake
    VERSION ${PROJECT_VERSION}
    COMPATIBILITY SameMajorVersion
)

install(FILES
    ${CMAKE_CURRENT_BINARY_DIR}/mine_ui_window_config.cmake
    ${CMAKE_CURRENT_BINARY_DIR}/mine_ui_window_config_version.cmake
    DESTINATION lib/cmake/mine_ui_window
)
```

```cpp
// ========== Api.h ==========

#pragma once

// 平台检测
#if defined(_WIN32) || defined(_WIN64)
    #define MINE_PLATFORM_WINDOWS 1
#elif defined(__unix__) || defined(__APPLE__)
    #define MINE_PLATFORM_UNIX 1
#else
    #error "Unsupported platform"
#endif

// DLL 导出/导入宏
#if defined(MINE_UI_WINDOW_STATIC)
    #define MINE_UI_WINDOW_API
#elif defined(MINE_UI_WINDOW_EXPORT)
    #if defined(MINE_PLATFORM_WINDOWS)
        #define MINE_UI_WINDOW_API __declspec(dllexport)
    #elif defined(MINE_PLATFORM_UNIX)
        #define MINE_UI_WINDOW_API __attribute__((visibility("default")))
    #endif
#else
    #if defined(MINE_PLATFORM_WINDOWS)
        #define MINE_UI_WINDOW_API __declspec(dllimport)
    #elif defined(MINE_PLATFORM_UNIX)
        #define MINE_UI_WINDOW_API
    #endif
#endif
```

```cpp
// ========== Window.h ==========

#pragma once

#include <mine/ui/window/Api.h>
#include <mine/core/String.h>

namespace mine::ui {

/// @brief 应用窗口类
class MINE_UI_WINDOW_API Window {
public:
    Window();
    ~Window();
    
    void show();
    void hide();
    void close();
    
    void set_title(core::StringView title);
    
private:
    struct Impl;
    Impl* m_impl = nullptr;
};

} // namespace mine::ui
```

```cpp
// ========== Window.cpp ==========

#include <mine/ui/window/Window.h>
#include <iostream>

namespace mine::ui {

struct Window::Impl {
    std::string title;
};

Window::Window()
    : m_impl(new Impl{}) {
    std::cout << "Window constructed" << std::endl;
}

Window::~Window() {
    delete m_impl;
    std::cout << "Window destructed" << std::endl;
}

void Window::show() {
    std::cout << "Window shown: " << m_impl->title << std::endl;
}

void Window::hide() {
    std::cout << "Window hidden" << std::endl;
}

void Window::close() {
    std::cout << "Window closed" << std::endl;
}

void Window::set_title(core::StringView title) {
    m_impl->title = std::string(title);
    std::cout << "Window title set: " << m_impl->title << std::endl;
}

} // namespace mine::ui
```

```cpp
// ========== WindowAll.h ==========

#pragma once

#include <mine/ui/window/Api.h>
#include <mine/ui/window/Window.h>
#include <mine/ui/window/WindowContext.h>
```

```cpp
// ========== 外部应用代码 main.cpp ==========

#include <mine/ui/window/Window.h>

int main() {
    // 使用动态库或静态库(取决于编译配置)
    mine::ui::Window window;
    window.set_title("My Application");
    window.show();
    window.close();
    
    return 0;
}
```

```cmake
# ========== 外部应用 CMakeLists.txt ==========

cmake_minimum_required(VERSION 3.20)
project(myapp LANGUAGES CXX)

# 查找 mine.ui.window 模块
find_package(mine_ui_window REQUIRED)

# 应用程序
add_executable(myapp
    src/main.cpp
)

# 链接动态库或静态库
target_link_libraries(myapp PRIVATE
    mine::mine_ui_window
)

target_compile_features(myapp PRIVATE cxx_std_20)
```

### 预期运行效果

**编译输出(动态库)**：
```
-- Building mine.ui.window as SHARED library
-- Configuring done
-- Generating done
-- Build files written to: build/
[ 50%] Building CXX object Window.cpp.obj
[100%] Linking CXX shared library mine_ui_window.dll
[100%] Built target mine_ui_window
```

**运行输出**：
```
Window constructed
Window title set: My Application
Window shown: My Application
Window closed
Window destructed
```

**生成文件**：
- Windows: `mine_ui_window.dll` + `mine_ui_window.lib`
- Linux: `libmine_ui_window.so`

---

## 9. 总结

### 核心要点回顾

- **Api.h 定义导出宏**：`MINE_UI_WINDOW_API` 控制 DLL 导出/导入
- **WindowAll.h 伞形头文件**：一次性包含所有公共头文件,适合快速原型
- **支持动态库和静态库**：通过 `MINE_UI_WINDOW_STATIC` 宏切换编译模式
- **跨平台兼容**：自动检测 Windows/Unix 平台,使用对应的导出语法
- **模块边界清晰**：通过导出宏明确公共 API,隐藏实现细节

### 与其他模块的协作关系

```
mine.ui.window/Api.h
  ↓ included by
mine.ui.window/Window.h, WindowContext.h
  ↓ all included by
mine.ui.window/WindowAll.h
  ↓ used by
外部应用代码(main.cpp, etc.)
```

### 适用场景总结

- **动态库编译**：定义 `MINE_UI_WINDOW_EXPORT` 导出符号
- **静态库编译**：定义 `MINE_UI_WINDOW_STATIC` 禁用导出
- **快速原型**：使用 `WindowAll.h` 一次性包含所有头文件
- **生产代码**：按需包含头文件,减少编译时间
- **跨平台开发**：自动适配 Windows/Linux 的导出语法

### 下一步学习建议

- **CMake 构建系统**：阅读 [15-build-xmake.md](../15-build-xmake.md),了解完整的构建配置
- **模块依赖管理**：阅读 mine.core 模块文档,了解基础类型和宏定义
- **ABI 稳定性**：阅读 [18-coding-conventions.md](../18-coding-conventions.md),了解 ABI 兼容性规范
- **DLL 导出原理**：研究 Windows PE 格式和 Unix ELF 格式的符号表机制

---

**文档版本**：1.0.0  
**最后更新**：2026-06-11  
**作者**：MineFramework 开发团队
