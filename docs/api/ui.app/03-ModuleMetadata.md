# 模块元数据详细接口文档

## 1. 概述

本文档详细介绍 `mine.ui.app` 模块的元数据文件，包括 `Api.h`（DLL 导出/导入宏定义）和 `AppAll.h`（伞形头文件）。这些文件是 MineFramework 模块系统的标准组成部分,负责处理符号可见性、链接配置以及头文件包含管理。

### 1.1 核心功能

- **Api.h - 符号可见性控制**：
  - 定义 `MINE_UI_APP_API` 宏，用于标记需要导出/导入的类、函数、变量
  - 根据编译模式（动态库 / 静态库）自动选择正确的导出/导入声明
  - 跨平台兼容（Windows `__declspec(dllexport/dllimport)` / GCC/Clang `__attribute__((visibility("default")))`）
  - 支持头文件专用库（header-only library）模式

- **AppAll.h - 伞形头文件**：
  - 一次性包含模块的所有公共头文件
  - 简化用户代码的包含语句（不需要逐个包含头文件）
  - 提供模块的完整 API 视图
  - 适合快速原型开发和小型项目

### 1.2 设计目标

1. **符号可见性管理**：明确控制哪些符号对外可见，避免符号冲突和 ABI 问题。
2. **跨平台兼容**：统一处理不同编译器的导出/导入语法差异。
3. **构建模式灵活性**：支持动态库、静态库、头文件专用库三种构建模式。
4. **易用性**：用户只需包含 `AppAll.h` 即可使用模块的所有功能。
5. **可维护性**：模块添加新类型时，只需更新 `AppAll.h` 即可。

### 1.3 在整体架构中的位置

```
mine.ui.app 模块
├─ api/
│   ├─ include/
│   │   └─ mine/ui/app/
│   │       ├─ Api.h              ← 符号可见性控制
│   │       ├─ AppAll.h           ← 伞形头文件
│   │       ├─ Application.h      ← 核心类（使用 MINE_UI_APP_API）
│   │       └─ ApplicationMain.h  ← 宏定义
│   └─ src/
│       └─ Application.cpp        ← 实现文件
└─ xmake.lua                      ← 构建脚本（定义 MINE_UI_APP_BUILD）
```

**符号可见性流程**：

```
编译器标志
    ↓ 定义 MINE_UI_APP_BUILD（编译模块时）
Api.h 宏展开
    ↓ MINE_UI_APP_API → __declspec(dllexport) / __attribute__((visibility("default")))
Application.h 类声明
    ↓ class MINE_UI_APP_API Application { ... }
编译器生成导出符号
    ↓ Application::run() 符号可见
链接器
    ↓ 用户代码链接到模块（自动导入符号）
```

### 1.4 与其他框架的对比

| 框架 | 符号可见性宏 | 伞形头文件 | 构建模式 |
|------|-------------|-----------|---------|
| WPF | 无（C# 自动处理） | `using System.Windows;` | .NET 程序集 |
| Qt | `Q_DECL_EXPORT` / `Q_DECL_IMPORT` | `<QtWidgets>` | 静态库 / 动态库 |
| Boost | `BOOST_<LIB>_DECL` | `<boost/all.hpp>` | 头文件专用 / 静态库 / 动态库 |
| POCO | `Poco_<LIB>_API` | `<Poco/Foundation.h>` | 静态库 / 动态库 |
| MineFramework | `MINE_<MODULE>_API` | `<mine/<module>/<Module>All.h>` | 静态库 / 动态库 / 头文件专用 |

**MineFramework 的特色**：
- **模块级符号宏**：每个模块有独立的 `MINE_<MODULE>_API` 宏，避免全局污染。
- **统一伞形头文件模式**：所有模块使用 `<Module>All.h` 命名规范（如 `AppAll.h` / `CoreAll.h`）。
- **构建标志自动化**：构建系统（xmake）自动定义 `MINE_<MODULE>_BUILD` 标志。

### 1.5 典型用法

```cpp
// 使用伞形头文件（推荐）
#include <mine/ui/app/AppAll.h>

// 或者直接包含 Api.h（如果只需要导出宏）
#include <mine/ui/app/Api.h>
```

---

## 2. 文件位置

### 2.1 Api.h

**路径**：
```
src/mine/ui/app/api/include/mine/ui/app/Api.h
```

**用途**：
- 定义 `MINE_UI_APP_API` 宏
- 处理编译器差异（Windows / GCC / Clang）
- 根据构建模式选择导出/导入/无操作

### 2.2 AppAll.h

**路径**：
```
src/mine/ui/app/api/include/mine/ui/app/AppAll.h
```

**用途**：
- 包含模块的所有公共头文件
- 简化用户代码的包含语句
- 提供模块的完整 API 视图

### 2.3 依赖的模块

- **无直接依赖**：`Api.h` 和 `AppAll.h` 是纯头文件，不依赖其他模块。
- **间接依赖**：`AppAll.h` 包含的头文件可能依赖其他模块（如 `Application.h` 依赖 `mine.core`）。

---

## 3. Api.h 详细定义

### 3.1 完整代码

```cpp
// ════════════════════════════════════════════════════════════════
//  Api.h - 符号可见性控制宏
// ════════════════════════════════════════════════════════════════

#pragma once

/**
 * @file Api.h
 * @brief mine.ui.app 模块的符号可见性控制宏定义
 * 
 * 该文件定义了 MINE_UI_APP_API 宏，用于标记需要导出/导入的符号。
 * 根据编译模式和编译器类型，宏会展开为不同的导出/导入声明。
 * 
 * @note 该文件由构建系统自动生成或手动维护，不应直接修改宏定义
 */

// ────────────────────────────────────────────────────────────────
//  动态库模式（DLL / Shared Library）
// ────────────────────────────────────────────────────────────────

#if defined(MINE_UI_APP_BUILD)
    // 编译模块时：导出符号（使符号对外可见）
    #if defined(_MSC_VER)
        // Windows MSVC: 使用 __declspec(dllexport)
        #define MINE_UI_APP_API __declspec(dllexport)
    #elif defined(__GNUC__) || defined(__clang__)
        // GCC / Clang: 使用 __attribute__((visibility("default")))
        #define MINE_UI_APP_API __attribute__((visibility("default")))
    #else
        // 其他编译器：假设符号默认可见
        #define MINE_UI_APP_API
    #endif
#elif defined(MINE_UI_APP_STATIC)
    // 静态库模式：不需要导出/导入
    #define MINE_UI_APP_API
#else
    // 使用模块时：导入符号（从动态库中导入）
    #if defined(_MSC_VER)
        // Windows MSVC: 使用 __declspec(dllimport)
        #define MINE_UI_APP_API __declspec(dllimport)
    #else
        // GCC / Clang: 不需要显式导入（符号默认可见）
        #define MINE_UI_APP_API
    #endif
#endif

// ────────────────────────────────────────────────────────────────
//  使用示例
// ────────────────────────────────────────────────────────────────

/**
 * @example 导出类
 * \code{.cpp}
 * // Application.h
 * class MINE_UI_APP_API Application {
 * public:
 *     Application();
 *     virtual ~Application();
 *     int run(int argc = 0, char** argv = nullptr);
 * };
 * \endcode
 * 
 * @example 导出函数
 * \code{.cpp}
 * // Utils.h
 * MINE_UI_APP_API void init_application_system();
 * MINE_UI_APP_API const char* get_version_string();
 * \endcode
 * 
 * @example 导出全局变量
 * \code{.cpp}
 * // Globals.h
 * extern MINE_UI_APP_API Application* g_current_application;
 * \endcode
 */
```

### 3.2 宏展开逻辑

**情况1：编译模块时（定义了 `MINE_UI_APP_BUILD`）**

```cpp
#define MINE_UI_APP_BUILD  // 由构建系统定义

// Windows MSVC
#define MINE_UI_APP_API __declspec(dllexport)

// GCC / Clang
#define MINE_UI_APP_API __attribute__((visibility("default")))
```

**情况2：静态库模式（定义了 `MINE_UI_APP_STATIC`）**

```cpp
#define MINE_UI_APP_STATIC  // 由构建系统定义

// 所有编译器
#define MINE_UI_APP_API  // 展开为空（不需要导出/导入）
```

**情况3：使用模块时（未定义 `MINE_UI_APP_BUILD` 和 `MINE_UI_APP_STATIC`）**

```cpp
// Windows MSVC
#define MINE_UI_APP_API __declspec(dllimport)

// GCC / Clang
#define MINE_UI_APP_API  // 展开为空（符号默认可见）
```

### 3.3 编译器特定语法

| 编译器 | 导出语法 | 导入语法 | 可见性控制 |
|--------|---------|---------|-----------|
| **MSVC (Windows)** | `__declspec(dllexport)` | `__declspec(dllimport)` | 默认隐藏，需显式导出/导入 |
| **GCC (Linux)** | `__attribute__((visibility("default")))` | 无需显式导入 | 可配置默认可见性（`-fvisibility=hidden`） |
| **Clang (macOS/Linux)** | `__attribute__((visibility("default")))` | 无需显式导入 | 可配置默认可见性（`-fvisibility=hidden`） |

**关键差异**：
- **Windows MSVC**：必须显式使用 `__declspec(dllimport)` 导入符号（否则会生成低效的导入代码）。
- **GCC / Clang**：不需要显式导入（符号默认可见），但可以使用 `-fvisibility=hidden` 编译选项隐藏所有符号，然后显式导出需要可见的符号。

---

## 4. AppAll.h 详细定义

### 4.1 完整代码

```cpp
// ════════════════════════════════════════════════════════════════
//  AppAll.h - 伞形头文件（Umbrella Header）
// ════════════════════════════════════════════════════════════════

#pragma once

/**
 * @file AppAll.h
 * @brief mine.ui.app 模块的伞形头文件
 * 
 * 该文件包含了 mine.ui.app 模块的所有公共头文件，用户只需包含该文件
 * 即可使用模块的所有功能。
 * 
 * @note 对于大型项目，建议只包含需要的头文件以减少编译时间。
 *       对于小型项目或快速原型开发，可以使用该伞形头文件。
 * 
 * @example 基础用法
 * \code{.cpp}
 * #include <mine/ui/app/AppAll.h>
 * 
 * class MyApp : public mine::ui::app::Application {
 * protected:
 *     void on_startup(int argc, char** argv) override {
 *         // ...
 *     }
 * };
 * 
 * MINE_APPLICATION_MAIN(MyApp)
 * \endcode
 */

// ────────────────────────────────────────────────────────────────
//  核心头文件
// ────────────────────────────────────────────────────────────────

// 符号可见性控制
#include <mine/ui/app/Api.h>

// 应用程序基类
#include <mine/ui/app/Application.h>

// 应用程序入口宏
#include <mine/ui/app/ApplicationMain.h>

// ────────────────────────────────────────────────────────────────
//  模块说明
// ────────────────────────────────────────────────────────────────

/**
 * @namespace mine::ui::app
 * @brief UI 应用程序框架命名空间
 * 
 * 该命名空间包含 MineFramework UI 应用程序的核心类和宏：
 * - Application: 应用程序基类，管理生命周期、窗口、主题、资源等
 * - MINE_APPLICATION_MAIN: 进程入口宏，自动生成 main 函数
 * 
 * @section usage 使用方式
 * 
 * 1. 继承 Application 类：
 * \code{.cpp}
 * class MyApp : public mine::ui::app::Application {
 * protected:
 *     void on_startup(int argc, char** argv) override {
 *         // 创建主窗口
 *         auto* win = create_window(desc);
 *         set_main_window(win);
 *         win->show();
 *     }
 * };
 * \endcode
 * 
 * 2. 使用 MINE_APPLICATION_MAIN 宏生成 main 函数：
 * \code{.cpp}
 * MINE_APPLICATION_MAIN(MyApp)
 * \endcode
 * 
 * @section modules 相关模块
 * - mine.platform.abi: 平台抽象层（IApplicationHost / WindowDesc）
 * - mine.gfx.rhi: 图形设备抽象层（IDevice）
 * - mine.paint: 渲染器抽象层（IRenderer）
 * - mine.ui.window: 窗口管理（ui::Window）
 * - mine.ui.style: 样式和资源（style::ResourceDictionary）
 */
```

### 4.2 包含的头文件

`AppAll.h` 包含了 `mine.ui.app` 模块的所有公共头文件：

| 头文件 | 描述 | 主要内容 |
|--------|------|---------|
| `Api.h` | 符号可见性控制 | `MINE_UI_APP_API` 宏定义 |
| `Application.h` | 应用程序基类 | `Application` 类定义 |
| `ApplicationMain.h` | 进程入口宏 | `MINE_APPLICATION_MAIN` 宏定义 |

**注意**：`AppAll.h` 不包含实现文件（`.cpp`），只包含公共头文件（`.h`）。

### 4.3 使用场景

**推荐使用伞形头文件的场景**：
- 快速原型开发（不关心编译时间）
- 小型项目（文件数量少，编译时间短）
- 示例代码或教程（简化包含语句）
- 单元测试（需要访问模块的所有功能）

**不推荐使用伞形头文件的场景**：
- 大型项目（编译时间敏感）
- 只使用模块的一小部分功能（如只使用 `Application` 类，不需要 `MINE_APPLICATION_MAIN` 宏）
- 需要精确控制依赖关系（避免不必要的头文件包含）

---

## 5. 使用场景

### 场景 1：导出类（动态库模式）

**业务需求**：将 `Application` 类导出到动态库（DLL / Shared Library），供其他模块使用。

```cpp
// ════════════════════════════════════════════════════════════════
//  Application.h - 类声明
// ════════════════════════════════════════════════════════════════

#pragma once
#include <mine/ui/app/Api.h>

namespace mine::ui::app {

// 使用 MINE_UI_APP_API 标记类需要导出
class MINE_UI_APP_API Application {
public:
    Application();
    virtual ~Application();
    
    int run(int argc = 0, char** argv = nullptr);
    void quit(int exit_code = 0);
    
protected:
    virtual void on_startup(int argc, char** argv);
    virtual void on_exit(int exit_code);
};

} // namespace mine::ui::app
```

```cpp
// ════════════════════════════════════════════════════════════════
//  Application.cpp - 类实现
// ════════════════════════════════════════════════════════════════

#include <mine/ui/app/Application.h>

namespace mine::ui::app {

Application::Application() {
    // 构造函数实现
}

Application::~Application() {
    // 析构函数实现
}

int Application::run(int argc, char** argv) {
    on_startup(argc, argv);
    // ... 主循环 ...
    on_exit(0);
    return 0;
}

void Application::quit(int exit_code) {
    // 退出逻辑
}

void Application::on_startup(int argc, char** argv) {
    // 默认实现（空）
}

void Application::on_exit(int exit_code) {
    // 默认实现（空）
}

} // namespace mine::ui::app
```

```cmake
# ════════════════════════════════════════════════════════════════
#  CMakeLists.txt - 构建配置（动态库）
# ════════════════════════════════════════════════════════════════

# 创建动态库
add_library(mine.ui.app SHARED
    api/src/Application.cpp
)

# 定义导出宏（编译模块时）
target_compile_definitions(mine.ui.app PRIVATE
    MINE_UI_APP_BUILD  # 使 MINE_UI_APP_API 展开为 __declspec(dllexport)
)

# 设置包含目录
target_include_directories(mine.ui.app PUBLIC
    api/include
)
```

**预期效果**：
- 编译 `mine.ui.app` 动态库时，`MINE_UI_APP_API` 展开为 `__declspec(dllexport)`（Windows）或 `__attribute__((visibility("default")))`（Linux）
- 生成的动态库（`mine.ui.app.dll` / `libmine.ui.app.so`）包含导出的 `Application` 类符号
- 用户代码链接到动态库时，`MINE_UI_APP_API` 展开为 `__declspec(dllimport)`（Windows）或空（Linux）

---

### 场景 2：静态库模式

**业务需求**：将 `mine.ui.app` 模块编译为静态库（`.lib` / `.a`），直接链接到用户程序。

```cmake
# ════════════════════════════════════════════════════════════════
#  CMakeLists.txt - 构建配置（静态库）
# ════════════════════════════════════════════════════════════════

# 创建静态库
add_library(mine.ui.app STATIC
    api/src/Application.cpp
)

# 定义静态库宏（不需要导出/导入）
target_compile_definitions(mine.ui.app PUBLIC
    MINE_UI_APP_STATIC  # 使 MINE_UI_APP_API 展开为空
)

# 设置包含目录
target_include_directories(mine.ui.app PUBLIC
    api/include
)
```

```cpp
// ════════════════════════════════════════════════════════════════
//  用户代码
// ════════════════════════════════════════════════════════════════

#include <mine/ui/app/AppAll.h>

class MyApp : public mine::ui::app::Application {
protected:
    void on_startup(int argc, char** argv) override {
        // ...
    }
};

MINE_APPLICATION_MAIN(MyApp)
```

**预期效果**：
- 编译 `mine.ui.app` 静态库时，`MINE_UI_APP_API` 展开为空（不需要导出）
- 用户代码链接到静态库时，`MINE_UI_APP_API` 展开为空（不需要导入）
- 所有符号直接链接到最终可执行文件（无需动态加载）

---

### 场景 3：跨平台导出（Windows / Linux）

**业务需求**：在 Windows 和 Linux 上使用相同的代码编译动态库，符号可见性由 `Api.h` 自动处理。

```cpp
// ════════════════════════════════════════════════════════════════
//  Utils.h - 导出全局函数
// ════════════════════════════════════════════════════════════════

#pragma once
#include <mine/ui/app/Api.h>

namespace mine::ui::app {

// 导出函数（跨平台）
MINE_UI_APP_API void init_application_system();
MINE_UI_APP_API const char* get_version_string();
MINE_UI_APP_API int get_build_number();

} // namespace mine::ui::app
```

```cpp
// ════════════════════════════════════════════════════════════════
//  Utils.cpp - 实现
// ════════════════════════════════════════════════════════════════

#include "Utils.h"

namespace mine::ui::app {

void init_application_system() {
    // 初始化逻辑
}

const char* get_version_string() {
    return "1.0.0";
}

int get_build_number() {
    return 12345;
}

} // namespace mine::ui::app
```

**预期效果**：

**Windows MSVC**：
- 编译时：`MINE_UI_APP_API` 展开为 `__declspec(dllexport)`
- 使用时：`MINE_UI_APP_API` 展开为 `__declspec(dllimport)`

**Linux GCC**：
- 编译时：`MINE_UI_APP_API` 展开为 `__attribute__((visibility("default")))`
- 使用时：`MINE_UI_APP_API` 展开为空（符号默认可见）

**相同的代码，跨平台编译无需修改**！

---

### 场景 4：伞形头文件简化包含

**业务需求**：用户代码不想逐个包含 `Api.h` / `Application.h` / `ApplicationMain.h`，使用 `AppAll.h` 一次性包含所有头文件。

```cpp
// ════════════════════════════════════════════════════════════════
//  main.cpp - 用户代码（使用伞形头文件）
// ════════════════════════════════════════════════════════════════

// 只需包含一个头文件
#include <mine/ui/app/AppAll.h>

class MyApp : public mine::ui::app::Application {
protected:
    void on_startup(int argc, char** argv) override {
        mine::platform::WindowDesc desc;
        desc.title = u"My Application";
        desc.size  = {1280.0f, 720.0f};
        
        auto* win = create_window(desc);
        set_main_window(win);
        win->show();
    }
};

MINE_APPLICATION_MAIN(MyApp)
```

**预期效果**：
- 用户代码只包含 `AppAll.h` 一个头文件
- 自动包含 `Api.h` / `Application.h` / `ApplicationMain.h`
- 可以使用 `Application` 类和 `MINE_APPLICATION_MAIN` 宏

---

### 场景 5：精确控制依赖（不使用伞形头文件）

**业务需求**：大型项目需要精确控制依赖关系，只包含需要的头文件，减少编译时间。

```cpp
// ════════════════════════════════════════════════════════════════
//  MyApp.h - 应用程序类声明
// ════════════════════════════════════════════════════════════════

#pragma once
// 只包含需要的头文件（不使用 AppAll.h）
#include <mine/ui/app/Application.h>

class MyApp : public mine::ui::app::Application {
protected:
    void on_startup(int argc, char** argv) override;
};
```

```cpp
// ════════════════════════════════════════════════════════════════
//  MyApp.cpp - 应用程序类实现
// ════════════════════════════════════════════════════════════════

#include "MyApp.h"
#include <mine/ui/window/Window.h>  // 只在需要时包含

void MyApp::on_startup(int argc, char** argv) {
    mine::platform::WindowDesc desc;
    desc.title = u"My Application";
    desc.size  = {1280.0f, 720.0f};
    
    auto* win = create_window(desc);
    set_main_window(win);
    win->show();
}
```

```cpp
// ════════════════════════════════════════════════════════════════
//  main.cpp - 进程入口
// ════════════════════════════════════════════════════════════════

#include "MyApp.h"
#include <mine/ui/app/ApplicationMain.h>  // 只在需要时包含

MINE_APPLICATION_MAIN(MyApp)
```

**预期效果**：
- `MyApp.h` 只包含 `Application.h`，编译时不会包含 `ApplicationMain.h`
- `MyApp.cpp` 只包含需要的头文件（`Window.h`）
- `main.cpp` 只包含 `ApplicationMain.h`（用于宏定义）
- 减少不必要的头文件包含，加快编译速度

---

## 6. 最佳实践

### ✅ 实践 1：动态库模式下始终使用 API 宏

**说明**：如果模块编译为动态库，所有需要导出的符号（类、函数、变量）都必须使用 `MINE_UI_APP_API` 标记。

**✅ 推荐写法**：

```cpp
// Application.h
#include <mine/ui/app/Api.h>

// ✅ 导出类
class MINE_UI_APP_API Application {
public:
    Application();
    virtual ~Application();
    int run(int argc = 0, char** argv = nullptr);
};

// ✅ 导出函数
MINE_UI_APP_API void init_application_system();

// ✅ 导出全局变量
extern MINE_UI_APP_API Application* g_current_application;
```

**❌ 不推荐写法**：

```cpp
// Application.h
// ❌ 忘记使用 API 宏
class Application {  // 符号不会导出！
public:
    Application();
    virtual ~Application();
    int run(int argc = 0, char** argv = nullptr);
};

// ❌ 函数不会导出
void init_application_system();  // 链接器找不到该符号
```

**问题说明**：
- 如果忘记使用 API 宏，符号不会导出到动态库
- 用户代码链接时会报错："undefined reference to Application::run()"
- Windows MSVC 更严格：如果不使用 `__declspec(dllimport)`，会生成低效的导入代码

---

### ✅ 实践 2：静态库模式下定义 STATIC 宏

**说明**：如果模块编译为静态库，必须定义 `MINE_UI_APP_STATIC` 宏，使 API 宏展开为空。

**✅ 推荐写法**：

```cmake
# CMakeLists.txt
add_library(mine.ui.app STATIC
    api/src/Application.cpp
)

# ✅ 定义静态库宏
target_compile_definitions(mine.ui.app PUBLIC
    MINE_UI_APP_STATIC  # 公开定义（用户代码也需要知道）
)
```

**❌ 不推荐写法**：

```cmake
# CMakeLists.txt
add_library(mine.ui.app STATIC
    api/src/Application.cpp
)

# ❌ 忘记定义静态库宏
# target_compile_definitions(mine.ui.app PUBLIC MINE_UI_APP_STATIC)
```

**问题说明**：
- 如果不定义 `MINE_UI_APP_STATIC`，用户代码会尝试导入符号（Windows）
- Windows MSVC 会报错："error LNK2019: unresolved external symbol __imp_Application::run()"
- 解决方案：定义 `MINE_UI_APP_STATIC` 宏，使 API 宏展开为空

---

### ✅ 实践 3：小型项目使用伞形头文件

**说明**：对于小型项目或快速原型开发，使用 `AppAll.h` 伞形头文件简化包含语句。

**✅ 推荐写法**：

```cpp
// main.cpp（小型项目）
#include <mine/ui/app/AppAll.h>  // ✅ 一次性包含所有头文件

class MyApp : public mine::ui::app::Application {
protected:
    void on_startup(int argc, char** argv) override {
        // ...
    }
};

MINE_APPLICATION_MAIN(MyApp)
```

**❌ 不推荐写法**（小型项目）：

```cpp
// main.cpp（小型项目）
// ❌ 逐个包含头文件（冗余）
#include <mine/ui/app/Api.h>
#include <mine/ui/app/Application.h>
#include <mine/ui/app/ApplicationMain.h>

class MyApp : public mine::ui::app::Application {
    // ...
};

MINE_APPLICATION_MAIN(MyApp)
```

**好处**：
- 减少包含语句（只需一行）
- 不容易遗漏头文件
- 适合快速原型开发

---

### ✅ 实践 4：大型项目精确控制依赖

**说明**：对于大型项目，应精确控制依赖关系，只包含需要的头文件，避免不必要的编译开销。

**✅ 推荐写法**：

```cpp
// MyApp.h（大型项目）
#pragma once
// ✅ 只包含需要的头文件
#include <mine/ui/app/Application.h>

class MyApp : public mine::ui::app::Application {
protected:
    void on_startup(int argc, char** argv) override;
};
```

```cpp
// main.cpp（大型项目）
#include "MyApp.h"
// ✅ 只在需要时包含
#include <mine/ui/app/ApplicationMain.h>

MINE_APPLICATION_MAIN(MyApp)
```

**❌ 不推荐写法**（大型项目）：

```cpp
// MyApp.h（大型项目）
#pragma once
// ❌ 使用伞形头文件（包含不需要的头文件）
#include <mine/ui/app/AppAll.h>

class MyApp : public mine::ui::app::Application {
protected:
    void on_startup(int argc, char** argv) override;
};
```

**好处**：
- 减少不必要的头文件包含
- 加快编译速度（增量编译时受益明显）
- 更清晰的依赖关系

---

## 7. 常见陷阱

### ❌ 陷阱 1：动态库模式下忘记使用 API 宏

**问题描述**：在动态库模式下，类或函数未使用 `MINE_UI_APP_API` 标记,导致符号未导出，用户代码链接失败。

**❌ 错误代码**：

```cpp
// Application.h
#pragma once
// ❌ 忘记包含 Api.h
// #include <mine/ui/app/Api.h>

namespace mine::ui::app {

// ❌ 忘记使用 API 宏
class Application {
public:
    Application();
    virtual ~Application();
    int run(int argc = 0, char** argv = nullptr);
};

} // namespace mine::ui::app
```

```cmake
# CMakeLists.txt
add_library(mine.ui.app SHARED  # 动态库模式
    api/src/Application.cpp
)

target_compile_definitions(mine.ui.app PRIVATE
    MINE_UI_APP_BUILD  # 定义了构建标志
)
```

**错误现象**：

```
# 用户代码链接时报错
undefined reference to `mine::ui::app::Application::run(int, char**)'
```

**✅ 正确代码**：

```cpp
// Application.h
#pragma once
#include <mine/ui/app/Api.h>  // ✅ 包含 Api.h

namespace mine::ui::app {

// ✅ 使用 API 宏标记类
class MINE_UI_APP_API Application {
public:
    Application();
    virtual ~Application();
    int run(int argc = 0, char** argv = nullptr);
};

} // namespace mine::ui::app
```

**原理解释**：
- 动态库模式下，符号默认不导出（Windows）或可能被隐藏（Linux `-fvisibility=hidden`）
- 必须使用 `MINE_UI_APP_API` 标记需要导出的符号
- API 宏会展开为 `__declspec(dllexport)`（Windows）或 `__attribute__((visibility("default")))`（Linux）

---

### ❌ 陷阱 2：静态库模式下未定义 STATIC 宏

**问题描述**：将模块编译为静态库，但未定义 `MINE_UI_APP_STATIC` 宏，导致 Windows MSVC 尝试导入符号（而不是直接链接）。

**❌ 错误代码**：

```cmake
# CMakeLists.txt
add_library(mine.ui.app STATIC  # 静态库模式
    api/src/Application.cpp
)

# ❌ 忘记定义 MINE_UI_APP_STATIC
# target_compile_definitions(mine.ui.app PUBLIC MINE_UI_APP_STATIC)
```

**错误现象（Windows MSVC）**：

```
error LNK2019: unresolved external symbol __imp_Application::run()
```

**原理说明**：
- 如果不定义 `MINE_UI_APP_STATIC`，`Api.h` 会认为模块是动态库
- Windows MSVC 的 `MINE_UI_APP_API` 会展开为 `__declspec(dllimport)`
- `__declspec(dllimport)` 会生成 `__imp_` 前缀的导入符号（如 `__imp_Application::run()`）
- 但静态库中的符号没有 `__imp_` 前缀，导致链接失败

**✅ 正确代码**：

```cmake
# CMakeLists.txt
add_library(mine.ui.app STATIC  # 静态库模式
    api/src/Application.cpp
)

# ✅ 定义 MINE_UI_APP_STATIC 宏
target_compile_definitions(mine.ui.app PUBLIC
    MINE_UI_APP_STATIC  # 公开定义，用户代码也需要知道
)
```

**原理解释**：
- 定义 `MINE_UI_APP_STATIC` 后，`MINE_UI_APP_API` 展开为空
- 符号不使用 `__declspec(dllimport)`，直接链接到静态库
- Windows MSVC 和 GCC/Clang 都能正常链接

---

### ❌ 陷阱 3：大型项目滥用伞形头文件

**问题描述**：在大型项目中，所有文件都包含 `AppAll.h` 伞形头文件，导致编译时间过长。

**❌ 错误代码**：

```cpp
// MyApp.h（大型项目，500+ 个类）
#pragma once
// ❌ 包含伞形头文件（引入不需要的头文件）
#include <mine/ui/app/AppAll.h>

class MyApp : public mine::ui::app::Application {
protected:
    void on_startup(int argc, char** argv) override;
};
```

**问题说明**：
- `AppAll.h` 包含了 `Api.h` / `Application.h` / `ApplicationMain.h`
- 即使 `MyApp.h` 只需要 `Application.h`，也会包含 `ApplicationMain.h`（不需要）
- 大型项目中，这种冗余会导致编译时间显著增加

**✅ 正确代码**：

```cpp
// MyApp.h（大型项目）
#pragma once
// ✅ 只包含需要的头文件
#include <mine/ui/app/Application.h>

class MyApp : public mine::ui::app::Application {
protected:
    void on_startup(int argc, char** argv) override;
};
```

```cpp
// main.cpp（大型项目）
#include "MyApp.h"
// ✅ 只在需要时包含
#include <mine/ui/app/ApplicationMain.h>

MINE_APPLICATION_MAIN(MyApp)
```

**原理解释**：
- 头文件只包含必需的依赖，减少编译开销
- 增量编译时，修改 `ApplicationMain.h` 不会触发 `MyApp.h` 的重新编译
- 大型项目中，这种优化可以节省数秒到数分钟的编译时间

---

### ❌ 陷阱 4：混淆 BUILD 和 STATIC 宏

**问题描述**：同时定义 `MINE_UI_APP_BUILD` 和 `MINE_UI_APP_STATIC`，导致宏展开错误。

**❌ 错误代码**：

```cmake
# CMakeLists.txt
add_library(mine.ui.app SHARED  # 动态库模式
    api/src/Application.cpp
)

# ❌ 错误：同时定义 BUILD 和 STATIC
target_compile_definitions(mine.ui.app PRIVATE
    MINE_UI_APP_BUILD
    MINE_UI_APP_STATIC  # 冲突！
)
```

**问题说明**：
- `MINE_UI_APP_BUILD` 表示编译动态库（导出符号）
- `MINE_UI_APP_STATIC` 表示使用静态库（不导出/导入符号）
- 同时定义两个宏会导致 `Api.h` 的条件编译逻辑错误

**✅ 正确代码**：

```cmake
# 动态库模式
add_library(mine.ui.app SHARED
    api/src/Application.cpp
)
target_compile_definitions(mine.ui.app PRIVATE
    MINE_UI_APP_BUILD  # ✅ 只定义 BUILD
)

# 或者静态库模式
add_library(mine.ui.app STATIC
    api/src/Application.cpp
)
target_compile_definitions(mine.ui.app PUBLIC
    MINE_UI_APP_STATIC  # ✅ 只定义 STATIC
)
```

**原理解释**：
- `MINE_UI_APP_BUILD` 和 `MINE_UI_APP_STATIC` 是互斥的
- `Api.h` 的条件编译逻辑：优先检查 `MINE_UI_APP_BUILD`，其次检查 `MINE_UI_APP_STATIC`，最后默认为导入模式
- 只能定义其中一个宏

---

## 8. 完整示例

以下是一个完整的示例，展示如何使用 `Api.h` 和 `AppAll.h` 构建和使用 `mine.ui.app` 模块。

```cpp
// ════════════════════════════════════════════════════════════════
//  Api.h - 符号可见性控制（模块提供）
// ════════════════════════════════════════════════════════════════

#pragma once

#if defined(MINE_UI_APP_BUILD)
    #if defined(_MSC_VER)
        #define MINE_UI_APP_API __declspec(dllexport)
    #elif defined(__GNUC__) || defined(__clang__)
        #define MINE_UI_APP_API __attribute__((visibility("default")))
    #else
        #define MINE_UI_APP_API
    #endif
#elif defined(MINE_UI_APP_STATIC)
    #define MINE_UI_APP_API
#else
    #if defined(_MSC_VER)
        #define MINE_UI_APP_API __declspec(dllimport)
    #else
        #define MINE_UI_APP_API
    #endif
#endif


// ════════════════════════════════════════════════════════════════
//  Application.h - 应用程序类（模块提供）
// ════════════════════════════════════════════════════════════════

#pragma once
#include <mine/ui/app/Api.h>

namespace mine::ui::app {

class MINE_UI_APP_API Application {
public:
    Application();
    virtual ~Application();
    
    int run(int argc = 0, char** argv = nullptr);
    void quit(int exit_code = 0);
    
protected:
    virtual void on_startup(int argc, char** argv);
    virtual void on_exit(int exit_code);
};

} // namespace mine::ui::app


// ════════════════════════════════════════════════════════════════
//  ApplicationMain.h - 进程入口宏（模块提供）
// ════════════════════════════════════════════════════════════════

#pragma once

#define MINE_APPLICATION_MAIN(AppClass)         \
    int main(int argc, char** argv) {           \
        AppClass _mine_app_;                    \
        return _mine_app_.run(argc, argv);      \
    }


// ════════════════════════════════════════════════════════════════
//  AppAll.h - 伞形头文件（模块提供）
// ════════════════════════════════════════════════════════════════

#pragma once

#include <mine/ui/app/Api.h>
#include <mine/ui/app/Application.h>
#include <mine/ui/app/ApplicationMain.h>


// ════════════════════════════════════════════════════════════════
//  Application.cpp - 实现文件（模块提供）
// ════════════════════════════════════════════════════════════════

#include <mine/ui/app/Application.h>
#include <iostream>

namespace mine::ui::app {

Application::Application() {
    std::cout << "Application constructed" << std::endl;
}

Application::~Application() {
    std::cout << "Application destructed" << std::endl;
}

int Application::run(int argc, char** argv) {
    std::cout << "Application::run() called" << std::endl;
    
    on_startup(argc, argv);
    
    // 模拟主循环
    std::cout << "Main loop running..." << std::endl;
    
    on_exit(0);
    return 0;
}

void Application::quit(int exit_code) {
    std::cout << "Application::quit(" << exit_code << ") called" << std::endl;
}

void Application::on_startup(int argc, char** argv) {
    std::cout << "Application::on_startup() called with "
              << argc << " arguments" << std::endl;
}

void Application::on_exit(int exit_code) {
    std::cout << "Application::on_exit(" << exit_code << ") called" << std::endl;
}

} // namespace mine::ui::app


// ════════════════════════════════════════════════════════════════
//  CMakeLists.txt - 构建配置（模块）
// ════════════════════════════════════════════════════════════════

/*
cmake_minimum_required(VERSION 3.20)
project(MineFramework)

# ────────────────────────────────────────────────────────────────
#  选项：动态库 / 静态库
# ────────────────────────────────────────────────────────────────

option(BUILD_SHARED_LIBS "Build shared libraries (DLL/SO)" ON)

# ────────────────────────────────────────────────────────────────
#  创建库
# ────────────────────────────────────────────────────────────────

if(BUILD_SHARED_LIBS)
    # 动态库模式
    add_library(mine.ui.app SHARED
        src/mine/ui/app/api/src/Application.cpp
    )
    
    # 定义导出宏
    target_compile_definitions(mine.ui.app PRIVATE
        MINE_UI_APP_BUILD
    )
else()
    # 静态库模式
    add_library(mine.ui.app STATIC
        src/mine/ui/app/api/src/Application.cpp
    )
    
    # 定义静态库宏（公开）
    target_compile_definitions(mine.ui.app PUBLIC
        MINE_UI_APP_STATIC
    )
endif()

# 设置包含目录
target_include_directories(mine.ui.app PUBLIC
    src/mine/ui/app/api/include
)

# 安装库和头文件
install(TARGETS mine.ui.app
    LIBRARY DESTINATION lib
    ARCHIVE DESTINATION lib
    RUNTIME DESTINATION bin
)

install(DIRECTORY src/mine/ui/app/api/include/
    DESTINATION include
)
*/


// ════════════════════════════════════════════════════════════════
//  MyApp.h - 用户应用程序类
// ════════════════════════════════════════════════════════════════

#pragma once
#include <mine/ui/app/Application.h>

class MyApp : public mine::ui::app::Application {
protected:
    void on_startup(int argc, char** argv) override {
        std::cout << "MyApp::on_startup() called" << std::endl;
        
        // 创建主窗口（简化版本）
        std::cout << "Creating main window..." << std::endl;
    }
    
    void on_exit(int exit_code) override {
        std::cout << "MyApp::on_exit() called" << std::endl;
    }
};


// ════════════════════════════════════════════════════════════════
//  main.cpp - 用户程序入口
// ════════════════════════════════════════════════════════════════

#include "MyApp.h"
#include <mine/ui/app/ApplicationMain.h>

MINE_APPLICATION_MAIN(MyApp)


// ════════════════════════════════════════════════════════════════
//  CMakeLists.txt - 用户程序构建配置
// ════════════════════════════════════════════════════════════════

/*
cmake_minimum_required(VERSION 3.20)
project(MyApplication)

# 查找 MineFramework
find_package(MineFramework REQUIRED)

# 创建可执行文件
add_executable(MyApplication
    MyApp.h
    main.cpp
)

# 链接库
target_link_libraries(MyApplication PRIVATE
    mine.ui.app
)

# 设置 C++ 标准
set_property(TARGET MyApplication PROPERTY CXX_STANDARD 20)
*/


// ════════════════════════════════════════════════════════════════
//  编译和运行
// ════════════════════════════════════════════════════════════════

/*
# 编译模块（动态库）
$ cd MineFramework
$ cmake -B build -DBUILD_SHARED_LIBS=ON
$ cmake --build build
$ sudo cmake --install build

# 或者编译模块（静态库）
$ cd MineFramework
$ cmake -B build -DBUILD_SHARED_LIBS=OFF
$ cmake --build build
$ sudo cmake --install build

# 编译用户程序
$ cd MyApplication
$ cmake -B build
$ cmake --build build

# 运行
$ ./build/MyApplication

# 预期输出：
Application constructed
Application::run() called
Application::on_startup() called with 0 arguments
MyApp::on_startup() called
Creating main window...
Main loop running...
Application::on_exit(0) called
MyApp::on_exit() called
Application destructed
*/
```

---

## 9. 总结

### 9.1 核心要点回顾

- **Api.h 的作用**：定义 `MINE_UI_APP_API` 宏，根据编译模式和编译器类型自动选择导出/导入/无操作。
- **宏展开逻辑**：
  - 编译模块时（定义了 `MINE_UI_APP_BUILD`）：导出符号（`__declspec(dllexport)` / `__attribute__((visibility("default")))`）
  - 静态库模式（定义了 `MINE_UI_APP_STATIC`）：展开为空（不需要导出/导入）
  - 使用模块时（两者都未定义）：导入符号（`__declspec(dllimport)` / 空）
- **AppAll.h 的作用**：伞形头文件，一次性包含模块的所有公共头文件，简化用户代码的包含语句。
- **构建模式**：支持动态库、静态库、头文件专用库三种构建模式。
- **跨平台兼容**：统一处理 Windows MSVC / GCC / Clang 的符号可见性差异。
- **最佳实践**：动态库模式下始终使用 API 宏、静态库模式下定义 STATIC 宏、小型项目使用伞形头文件、大型项目精确控制依赖。
- **常见陷阱**：忘记使用 API 宏、未定义 STATIC 宏、滥用伞形头文件、混淆 BUILD 和 STATIC 宏。

### 9.2 与其他模块的协作关系

```
Api.h
    ↓ 定义
MINE_UI_APP_API 宏
    ↓ 标记
Application 类
    ↓ 导出
动态库 / 静态库
    ↓ 链接
用户程序
```

```
AppAll.h
    ↓ 包含
Api.h + Application.h + ApplicationMain.h
    ↓ 简化
用户代码的包含语句
```

### 9.3 适用场景总结

- **动态库模式**：模块编译为 DLL / Shared Library，运行时动态加载。
- **静态库模式**：模块编译为 `.lib` / `.a`，链接时静态嵌入到可执行文件。
- **头文件专用库**：模块全部使用模板实现（如 STL），不需要编译成库。
- **伞形头文件**：小型项目或快速原型开发，简化包含语句。
- **精确依赖控制**：大型项目，只包含需要的头文件，减少编译时间。

### 9.4 下一步学习建议

- **CMake 构建系统**：学习如何使用 CMake 配置动态库 / 静态库构建模式，自动定义 `MINE_UI_APP_BUILD` / `MINE_UI_APP_STATIC` 宏。
- **符号可见性**：深入了解 GCC/Clang 的 `-fvisibility=hidden` 编译选项，以及如何使用 `__attribute__((visibility("default")))` 精确控制符号可见性。
- **DLL / Shared Library**：学习动态库的加载机制（LoadLibrary / dlopen），了解符号查找、版本管理、依赖解析等。
- **静态库链接**：学习静态库的链接顺序、符号冲突、ODR 违规等常见问题。
- **头文件专用库**：学习模板库的设计（如 Boost / Eigen），了解如何避免头文件膨胀和编译时间过长。

---

**相关文档**：
- [01-Application.md](01-Application.md) - Application 类详细接口
- [02-ApplicationMain.md](02-ApplicationMain.md) - MINE_APPLICATION_MAIN 宏详解
- [cmake/MineModule.cmake](../../cmake/MineModule.cmake) - MineFramework 模块构建系统
- [15-build-xmake.md](../../docs/15-build-xmake.md) - xmake 构建系统详解
