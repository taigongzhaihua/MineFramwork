# MINE_APPLICATION_MAIN 宏详细接口文档

## 1. 概述

`MINE_APPLICATION_MAIN` 是 MineFramework UI 框架提供的宏，用于生成跨平台的进程入口函数（`main` 函数）。它简化了应用程序的启动流程，自动实例化 Application 子类并调用其 `run()` 方法，消除了手动编写 `main` 函数的样板代码。

### 1.1 核心功能

- **自动生成 main 函数**：根据传入的 Application 子类名称，自动生成标准的 `int main(int argc, char** argv)` 函数。
- **实例化应用程序对象**：在栈上创建 Application 子类的实例，避免手动管理生命周期。
- **调用 run() 方法**：自动调用 `run(argc, argv)` 启动应用程序主循环，并返回退出码。
- **跨平台兼容**：在所有支持的平台（Windows / macOS / Linux / Wasm）上统一使用 `main` 作为入口，平台差异由 Application 内部处理。
- **异常安全**：如果 Application 的构造函数或 `run()` 方法抛出异常，栈展开会自动调用析构函数，释放资源。

### 1.2 设计目标

1. **简化样板代码**：避免每个应用程序都要手写相同的 `main` 函数，减少重复劳动。
2. **类型安全**：宏参数是类型名称（不是字符串），编译期检查确保传入的类名有效。
3. **统一入口**：所有平台使用相同的宏，平台差异由底层 IApplicationHost 抽象。
4. **生命周期管理**：应用程序对象在栈上创建，自动管理生命周期，避免内存泄漏。

### 1.3 在整体架构中的位置

```
用户代码（MyApp.cpp）
    ↓ 使用 MINE_APPLICATION_MAIN(MyApp)
预处理器展开宏
    ↓ 生成 main 函数
编译器编译 main 函数
    ↓ 链接到可执行文件
操作系统加载可执行文件
    ↓ 调用 main 函数
main 函数实例化 MyApp
    ↓ 调用 MyApp::run(argc, argv)
MyApp::run() 启动主循环
    ↓ 主循环退出
main 函数返回退出码
    ↓ 操作系统回收进程
```

### 1.4 与其他框架的对比

| 框架 | 入口宏 | 平台差异 | 生命周期管理 |
|------|--------|---------|-------------|
| WPF | `[STAThread] static void Main()` | Windows 专用，需要 `[STAThread]` 特性 | 手动 `new Application()`，自动 GC |
| Avalonia | `AppBuilder.Configure<App>().Start()` | 需要手动配置 `AppBuilder`，支持多平台 | 手动创建 `AppBuilder`，自动管理 |
| Qt | `QApplication app(argc, argv);` | 手动创建 `QApplication`，支持多平台 | 栈上创建，自动析构 |
| Electron | `app.whenReady().then(createWindow)` | JavaScript，使用事件驱动 | JavaScript 自动 GC |
| MineFramework | `MINE_APPLICATION_MAIN(MyApp)` | 统一宏，支持多平台 | 栈上创建，自动析构 |

**MineFramework 的特色**：
- **宏驱动**：与 Qt 类似使用宏，但更简洁（只需一行代码）。
- **平台统一**：所有平台使用相同的宏，不需要 `#ifdef` 或平台特定代码。
- **类型安全**：宏参数是类型名称，编译期检查，避免拼写错误。

### 1.5 典型用法

```cpp
// main.cpp
#include <mine/ui/app/AppAll.h>
#include "MyApp.h"

// 使用宏生成 main 函数
MINE_APPLICATION_MAIN(MyApp)
```

**宏展开后等价于**：

```cpp
int main(int argc, char** argv) {
    MyApp _mine_app_;
    return _mine_app_.run(argc, argv);
}
```

---

## 2. 文件位置

### 2.1 头文件

```
src/mine/ui/app/api/include/mine/ui/app/ApplicationMain.h
```

### 2.2 依赖的模块

- **mine.ui.app.Application**：宏生成的 `main` 函数会实例化 Application 子类并调用其 `run()` 方法。
- **C++ 标准库**：使用标准 `main` 函数签名（`int main(int argc, char** argv)`）。

### 2.3 包含方式

```cpp
// 方式1：直接包含
#include <mine/ui/app/ApplicationMain.h>

// 方式2：使用伞形头文件（推荐）
#include <mine/ui/app/AppAll.h>
```

---

## 3. 宏定义

```cpp
/**
 * @def MINE_APPLICATION_MAIN(AppClass)
 * @brief 生成进程入口函数，实例化并运行指定的 Application 子类
 * 
 * @param AppClass  继承自 mine::ui::app::Application 的具体类型名称（不加引号）
 * 
 * @details 该宏会展开为标准的 main 函数：
 * \code{.cpp}
 * int main(int argc, char** argv) {
 *     AppClass _mine_app_;
 *     return _mine_app_.run(argc, argv);
 * }
 * \endcode
 * 
 * @note 该宏只能在一个翻译单元（.cpp 文件）中使用一次
 * @note AppClass 必须可默认构造（无参构造函数或默认生成的构造函数）
 * @note AppClass 的析构函数将在 run() 返回后由编译器自动调用（栈展开）
 * 
 * @example 基础用法
 * \code{.cpp}
 * class MyApp : public mine::ui::app::Application {
 * protected:
 *     void on_startup(int argc, char** argv) override {
 *         // 创建主窗口
 *     }
 * };
 * 
 * MINE_APPLICATION_MAIN(MyApp)
 * \endcode
 * 
 * @example 带命名空间的用法
 * \code{.cpp}
 * namespace mycompany::myproduct {
 * class MyApp : public mine::ui::app::Application { ... };
 * }
 * 
 * MINE_APPLICATION_MAIN(mycompany::myproduct::MyApp)
 * \endcode
 */
#define MINE_APPLICATION_MAIN(AppClass)         \
    int main(int argc, char** argv) {           \
        AppClass _mine_app_;                    \
        return _mine_app_.run(argc, argv);      \
    }
```

### 3.1 宏参数

| 参数名 | 类型 | 描述 |
|--------|------|------|
| `AppClass` | 类型名称 | 继承自 `mine::ui::app::Application` 的具体类型名称（可以包含命名空间，如 `myns::MyApp`） |

### 3.2 宏展开机制

**输入**：

```cpp
MINE_APPLICATION_MAIN(MyApp)
```

**展开为**：

```cpp
int main(int argc, char** argv) {
    MyApp _mine_app_;
    return _mine_app_.run(argc, argv);
}
```

**关键点**：
- **局部变量**：应用程序对象 `_mine_app_` 是栈上的局部变量，不是全局变量或堆对象。
- **生命周期**：`_mine_app_` 在 `main` 函数开始时构造，在 `run()` 返回后自动析构（栈展开）。
- **异常安全**：如果 `run()` 抛出异常，栈展开会自动调用 `_mine_app_` 的析构函数，释放资源。
- **返回值**：`main` 函数的返回值是 `run()` 的返回值（退出码）。

---

## 4. 使用约束

### 4.1 唯一性约束

**该宏只能在一个翻译单元中使用一次**。

**✅ 正确用法**：

```cpp
// main.cpp
#include <mine/ui/app/AppAll.h>
#include "MyApp.h"

MINE_APPLICATION_MAIN(MyApp)  // ✅ 只在 main.cpp 中使用一次
```

**❌ 错误用法**：

```cpp
// main.cpp
#include <mine/ui/app/AppAll.h>

MINE_APPLICATION_MAIN(MyApp1)  // ❌ 错误：多次使用宏
MINE_APPLICATION_MAIN(MyApp2)  // ❌ 编译错误：重复定义 main 函数
```

**编译错误**：

```
error: redefinition of 'main'
note: previous definition is here
```

### 4.2 可默认构造约束

**AppClass 必须可默认构造**（即有无参构造函数或默认生成的构造函数）。

**✅ 正确用法**：

```cpp
// 情况1：使用默认生成的构造函数
class MyApp : public mine::ui::app::Application {
    // 编译器自动生成默认构造函数
};

// 情况2：显式定义无参构造函数
class MyApp : public mine::ui::app::Application {
public:
    MyApp() = default;
};

// 情况3：自定义无参构造函数
class MyApp : public mine::ui::app::Application {
public:
    MyApp() : config_path_("config.json") {}
private:
    std::string config_path_;
};

MINE_APPLICATION_MAIN(MyApp)  // ✅ 可以成功编译
```

**❌ 错误用法**：

```cpp
// 情况1：只有有参构造函数，没有无参构造函数
class MyApp : public mine::ui::app::Application {
public:
    MyApp(const std::string& config) : config_(config) {}  // 有参构造函数
    // 编译器不会自动生成无参构造函数
private:
    std::string config_;
};

MINE_APPLICATION_MAIN(MyApp)  // ❌ 编译错误：没有匹配的构造函数
```

**编译错误**：

```
error: no matching constructor for initialization of 'MyApp'
note: candidate constructor not viable: requires 1 argument, but 0 were provided
```

**解决方案**：添加无参构造函数（可以提供默认值）

```cpp
class MyApp : public mine::ui::app::Application {
public:
    MyApp() : MyApp("config.json") {}  // 委托构造函数
    MyApp(const std::string& config) : config_(config) {}
private:
    std::string config_;
};

MINE_APPLICATION_MAIN(MyApp)  // ✅ 现在可以编译
```

### 4.3 类型有效性约束

**AppClass 必须是有效的类型名称**，并且该类型必须在宏使用位置可见（已包含头文件）。

**✅ 正确用法**：

```cpp
// main.cpp
#include <mine/ui/app/AppAll.h>
#include "MyApp.h"  // ✅ 包含了 MyApp 的定义

MINE_APPLICATION_MAIN(MyApp)  // ✅ MyApp 类型可见
```

**❌ 错误用法**：

```cpp
// main.cpp
#include <mine/ui/app/AppAll.h>
// ❌ 忘记包含 MyApp 的头文件

MINE_APPLICATION_MAIN(MyApp)  // ❌ 编译错误：未知类型 MyApp
```

**编译错误**：

```
error: unknown type name 'MyApp'
```

---

## 5. 使用场景

### 场景 1：标准单窗口应用

**业务需求**：创建一个最简单的 MineUI 应用程序，使用宏生成进程入口。

```cpp
// MyApp.h
#pragma once
#include <mine/ui/app/AppAll.h>

class MyApp : public mine::ui::app::Application {
protected:
    void on_startup(int argc, char** argv) override {
        mine::platform::WindowDesc desc;
        desc.title = u"Hello World";
        desc.size  = {800.0f, 600.0f};
        
        auto* win = create_window(desc);
        set_main_window(win);
        win->show();
    }
};
```

```cpp
// main.cpp
#include "MyApp.h"
#include <mine/ui/app/ApplicationMain.h>

MINE_APPLICATION_MAIN(MyApp)
```

**预期效果**：
- 程序启动后显示一个 800x600 的窗口，标题为 "Hello World"
- 用户点击关闭按钮后，应用程序退出，返回退出码 0

---

### 场景 2：命令行参数处理

**业务需求**：在 `on_startup()` 中解析命令行参数，根据参数决定应用行为。

```cpp
// MyApp.h
#pragma once
#include <mine/ui/app/AppAll.h>
#include <cstring>
#include <iostream>

class MyApp : public mine::ui::app::Application {
protected:
    void on_startup(int argc, char** argv) override {
        // 解析命令行参数
        bool fullscreen = false;
        for (int i = 1; i < argc; ++i) {
            if (std::strcmp(argv[i], "--fullscreen") == 0) {
                fullscreen = true;
            } else if (std::strcmp(argv[i], "--help") == 0) {
                print_help();
                quit(0);  // 显示帮助后退出
                return;
            }
        }
        
        // 创建主窗口
        mine::platform::WindowDesc desc;
        desc.title = u"My Application";
        desc.size  = {1280.0f, 720.0f};
        desc.fullscreen = fullscreen;
        
        auto* win = create_window(desc);
        set_main_window(win);
        win->show();
        
        std::cout << "Application started in "
                  << (fullscreen ? "fullscreen" : "windowed")
                  << " mode." << std::endl;
    }
    
private:
    void print_help() {
        std::cout << "Usage: MyApplication [OPTIONS]\n"
                  << "Options:\n"
                  << "  --fullscreen    Start in fullscreen mode\n"
                  << "  --help          Show this help message\n";
    }
};
```

```cpp
// main.cpp
#include "MyApp.h"
#include <mine/ui/app/ApplicationMain.h>

MINE_APPLICATION_MAIN(MyApp)
```

**使用方式**：

```bash
# 普通窗口模式
$ ./MyApplication

# 全屏模式
$ ./MyApplication --fullscreen

# 显示帮助
$ ./MyApplication --help
```

**预期效果**：
- 不带参数启动时，显示普通窗口
- 带 `--fullscreen` 参数时，显示全屏窗口
- 带 `--help` 参数时，显示帮助信息并退出

---

### 场景 3：多实例检测（单实例应用）

**业务需求**：确保应用程序只能运行一个实例，如果检测到已有实例，则退出当前实例。

```cpp
// MyApp.h
#pragma once
#include <mine/ui/app/AppAll.h>
#include <mine/platform/SingleInstance.h>
#include <iostream>

class MyApp : public mine::ui::app::Application {
private:
    mine::platform::SingleInstance single_instance_;
    
protected:
    void on_startup(int argc, char** argv) override {
        // 尝试获取单实例锁
        if (!single_instance_.try_acquire("MyApplication")) {
            std::cerr << "Another instance is already running!" << std::endl;
            quit(1);  // 退出当前实例
            return;
        }
        
        std::cout << "Single instance lock acquired." << std::endl;
        
        // 创建主窗口
        mine::platform::WindowDesc desc;
        desc.title = u"My Application (Single Instance)";
        desc.size  = {1280.0f, 720.0f};
        
        auto* win = create_window(desc);
        set_main_window(win);
        win->show();
    }
    
    void on_exit(int exit_code) override {
        // 释放单实例锁
        single_instance_.release();
        std::cout << "Single instance lock released." << std::endl;
    }
};
```

```cpp
// main.cpp
#include "MyApp.h"
#include <mine/ui/app/ApplicationMain.h>

MINE_APPLICATION_MAIN(MyApp)
```

**预期效果**：
- 第一次启动应用程序时，成功获取单实例锁，显示主窗口
- 第二次启动应用程序时，检测到已有实例，输出错误信息并退出（返回退出码 1）
- 第一个实例关闭后，可以再次启动新实例

---

### 场景 4：异常处理（捕获 run() 异常）

**业务需求**：在 `on_startup()` 中可能抛出异常（如配置文件加载失败），需要捕获并记录日志。

```cpp
// MyApp.h
#pragma once
#include <mine/ui/app/AppAll.h>
#include <stdexcept>
#include <iostream>

class MyApp : public mine::ui::app::Application {
protected:
    void on_startup(int argc, char** argv) override {
        // 尝试加载配置文件
        load_config("config.json");  // 可能抛出异常
        
        // 创建主窗口
        mine::platform::WindowDesc desc;
        desc.title = u"My Application";
        desc.size  = {1280.0f, 720.0f};
        
        auto* win = create_window(desc);
        set_main_window(win);
        win->show();
    }
    
private:
    void load_config(const std::string& path) {
        // 模拟配置加载失败
        throw std::runtime_error("Failed to load config file: " + path);
    }
};
```

**方式1：让宏生成的 main 函数捕获异常**

```cpp
// main.cpp
#include "MyApp.h"
#include <mine/ui/app/ApplicationMain.h>
#include <iostream>
#include <exception>

// ❌ 不能使用宏（因为需要自定义异常处理）
// MINE_APPLICATION_MAIN(MyApp)

// ✅ 手动编写 main 函数并添加异常处理
int main(int argc, char** argv) {
    try {
        MyApp app;
        return app.run(argc, argv);
    } catch (const std::exception& e) {
        std::cerr << "Fatal error: " << e.what() << std::endl;
        return 1;
    } catch (...) {
        std::cerr << "Unknown fatal error!" << std::endl;
        return 1;
    }
}
```

**方式2：在 Application 子类中捕获异常**

```cpp
// MyApp.h
class MyApp : public mine::ui::app::Application {
protected:
    void on_startup(int argc, char** argv) override {
        try {
            // 尝试加载配置文件
            load_config("config.json");
        } catch (const std::exception& e) {
            std::cerr << "Error: " << e.what() << std::endl;
            // 使用默认配置继续运行
            use_default_config();
        }
        
        // 创建主窗口
        mine::platform::WindowDesc desc;
        desc.title = u"My Application";
        desc.size  = {1280.0f, 720.0f};
        
        auto* win = create_window(desc);
        set_main_window(win);
        win->show();
    }
    
private:
    void load_config(const std::string& path) {
        throw std::runtime_error("Failed to load config file: " + path);
    }
    
    void use_default_config() {
        std::cout << "Using default configuration." << std::endl;
    }
};
```

```cpp
// main.cpp
#include "MyApp.h"
#include <mine/ui/app/ApplicationMain.h>

MINE_APPLICATION_MAIN(MyApp)  // ✅ 可以使用宏
```

**预期效果**：
- 方式1：如果 `on_startup()` 抛出异常，`main` 函数捕获并输出错误信息，应用程序退出（返回退出码 1）
- 方式2：如果 `load_config()` 抛出异常，`on_startup()` 捕获并使用默认配置继续运行，应用程序正常启动

---

### 场景 5：跨平台入口（Windows / macOS / Linux）

**业务需求**：在不同平台上使用相同的代码启动应用程序。

```cpp
// MyApp.h
#pragma once
#include <mine/ui/app/AppAll.h>
#include <iostream>

class MyApp : public mine::ui::app::Application {
protected:
    void on_startup(int argc, char** argv) override {
        // 检测当前平台
        auto& host = this->host();
        std::cout << "Running on platform: " << host.platform_name() << std::endl;
        
        // 创建主窗口
        mine::platform::WindowDesc desc;
        desc.title = u"Cross-Platform Application";
        desc.size  = {1280.0f, 720.0f};
        
        auto* win = create_window(desc);
        set_main_window(win);
        win->show();
    }
};
```

```cpp
// main.cpp
#include "MyApp.h"
#include <mine/ui/app/ApplicationMain.h>

MINE_APPLICATION_MAIN(MyApp)
```

**预期效果**：
- Windows：输出 "Running on platform: Windows"
- macOS：输出 "Running on platform: macOS"
- Linux：输出 "Running on platform: Linux"
- 所有平台使用相同的代码，无需 `#ifdef` 或平台特定代码

---

### 场景 6：带命名空间的应用类

**业务需求**：Application 子类位于自定义命名空间中。

```cpp
// mycompany/MyApp.h
#pragma once
#include <mine/ui/app/AppAll.h>

namespace mycompany::myproduct {

class MyApp : public mine::ui::app::Application {
protected:
    void on_startup(int argc, char** argv) override {
        mine::platform::WindowDesc desc;
        desc.title = u"MyCompany Product";
        desc.size  = {1280.0f, 720.0f};
        
        auto* win = create_window(desc);
        set_main_window(win);
        win->show();
    }
};

} // namespace mycompany::myproduct
```

```cpp
// main.cpp
#include "mycompany/MyApp.h"
#include <mine/ui/app/ApplicationMain.h>

// ✅ 宏参数可以包含命名空间
MINE_APPLICATION_MAIN(mycompany::myproduct::MyApp)
```

**预期效果**：
- 宏正确展开为 `mycompany::myproduct::MyApp _mine_app_;`
- 程序正常启动，显示主窗口

---

## 6. 最佳实践

### ✅ 实践 1：使用宏简化入口

**说明**：优先使用 `MINE_APPLICATION_MAIN` 宏生成 `main` 函数，避免手动编写样板代码。

**✅ 推荐写法**：

```cpp
// main.cpp
#include "MyApp.h"
#include <mine/ui/app/ApplicationMain.h>

MINE_APPLICATION_MAIN(MyApp)  // ✅ 简洁、统一、类型安全
```

**❌ 不推荐写法**：

```cpp
// main.cpp
#include "MyApp.h"

// ❌ 手动编写 main 函数（冗余、容易拼写错误）
int main(int argc, char** argv) {
    MyApp app;
    return app.run(argc, argv);
}
```

**好处**：
- 减少样板代码，提高开发效率
- 统一代码风格（所有应用程序使用相同的宏）
- 编译期类型检查（如果类名拼写错误，编译器会报错）

---

### ✅ 实践 2：在单独的文件中使用宏

**说明**：将宏放在单独的翻译单元（如 `main.cpp`），不要与 Application 类定义混在一起。

**✅ 推荐写法**：

```cpp
// MyApp.h
#pragma once
#include <mine/ui/app/AppAll.h>

class MyApp : public mine::ui::app::Application {
protected:
    void on_startup(int argc, char** argv) override;
};
```

```cpp
// MyApp.cpp
#include "MyApp.h"

void MyApp::on_startup(int argc, char** argv) {
    // 实现逻辑
}
```

```cpp
// main.cpp
#include "MyApp.h"
#include <mine/ui/app/ApplicationMain.h>

MINE_APPLICATION_MAIN(MyApp)  // ✅ 单独文件，职责清晰
```

**❌ 不推荐写法**：

```cpp
// MyApp.cpp
#include "MyApp.h"
#include <mine/ui/app/ApplicationMain.h>

void MyApp::on_startup(int argc, char** argv) {
    // 实现逻辑
}

// ❌ 与类实现混在一起，职责不清晰
MINE_APPLICATION_MAIN(MyApp)
```

**好处**：
- 职责分离：`main.cpp` 负责进程入口，`MyApp.cpp` 负责应用逻辑
- 易于维护：修改应用逻辑不影响 `main` 函数
- 符合单一职责原则

---

### ✅ 实践 3：处理命令行参数

**说明**：如果应用程序需要命令行参数，在 `on_startup()` 中解析参数，而不是在 `main` 函数中。

**✅ 推荐写法**：

```cpp
// MyApp.h
class MyApp : public mine::ui::app::Application {
protected:
    void on_startup(int argc, char** argv) override {
        // ✅ 在 on_startup 中解析参数
        parse_args(argc, argv);
        
        // 创建主窗口
        create_main_window();
    }
    
private:
    void parse_args(int argc, char** argv);
    void create_main_window();
};
```

```cpp
// main.cpp
#include "MyApp.h"
#include <mine/ui/app/ApplicationMain.h>

MINE_APPLICATION_MAIN(MyApp)  // ✅ 宏会自动传递 argc/argv 到 on_startup
```

**❌ 不推荐写法**：

```cpp
// main.cpp
#include "MyApp.h"
#include <mine/ui/app/ApplicationMain.h>

// ❌ 不能使用宏（需要在 main 中处理参数）
// MINE_APPLICATION_MAIN(MyApp)

// ❌ 手动编写 main 函数处理参数
int main(int argc, char** argv) {
    // ❌ 在 main 中解析参数，逻辑分散
    bool fullscreen = false;
    for (int i = 1; i < argc; ++i) {
        if (std::strcmp(argv[i], "--fullscreen") == 0) {
            fullscreen = true;
        }
    }
    
    MyApp app;
    app.set_fullscreen(fullscreen);  // 假设有这个方法
    return app.run(argc, argv);
}
```

**好处**：
- 逻辑集中：所有应用初始化逻辑都在 `on_startup()` 中
- 可测试性：可以在单元测试中模拟 `argc/argv` 调用 `on_startup()`
- 使用宏：不需要手动编写 `main` 函数

---

### ✅ 实践 4：异常捕获策略

**说明**：根据应用程序的需求，决定是在 `main` 函数中捕获异常，还是在 `on_startup()` 中捕获。

**策略1：在 on_startup 中捕获异常（推荐）**

```cpp
// MyApp.h
class MyApp : public mine::ui::app::Application {
protected:
    void on_startup(int argc, char** argv) override {
        try {
            // ✅ 在 on_startup 中捕获异常，使用默认配置继续运行
            load_config("config.json");
        } catch (const std::exception& e) {
            std::cerr << "Error: " << e.what() << std::endl;
            use_default_config();
        }
        
        create_main_window();
    }
    
private:
    void load_config(const std::string& path);
    void use_default_config();
    void create_main_window();
};
```

```cpp
// main.cpp
#include "MyApp.h"
#include <mine/ui/app/ApplicationMain.h>

MINE_APPLICATION_MAIN(MyApp)  // ✅ 可以使用宏
```

**策略2：在 main 函数中捕获异常（致命错误）**

```cpp
// MyApp.h
class MyApp : public mine::ui::app::Application {
protected:
    void on_startup(int argc, char** argv) override {
        // 不捕获异常，让异常传播到 main 函数
        load_config("config.json");  // 可能抛出异常
        create_main_window();
    }
    
private:
    void load_config(const std::string& path);
    void create_main_window();
};
```

```cpp
// main.cpp
#include "MyApp.h"
#include <mine/ui/app/ApplicationMain.h>
#include <iostream>
#include <exception>

// ❌ 不能使用宏（需要自定义异常处理）
// MINE_APPLICATION_MAIN(MyApp)

// ✅ 手动编写 main 函数并添加异常处理
int main(int argc, char** argv) {
    try {
        MyApp app;
        return app.run(argc, argv);
    } catch (const std::exception& e) {
        std::cerr << "Fatal error: " << e.what() << std::endl;
        return 1;
    }
}
```

**选择建议**：
- 如果异常可恢复（如配置加载失败，可以使用默认配置），在 `on_startup()` 中捕获
- 如果异常不可恢复（如无法创建图形设备），让异常传播到 `main` 函数，记录日志并退出

---

## 7. 常见陷阱

### ❌ 陷阱 1：多次使用宏

**问题描述**：在同一个翻译单元中多次使用 `MINE_APPLICATION_MAIN` 宏，导致重复定义 `main` 函数。

**❌ 错误代码**：

```cpp
// main.cpp
#include <mine/ui/app/AppAll.h>

MINE_APPLICATION_MAIN(MyApp1)  // ❌ 第一次使用
MINE_APPLICATION_MAIN(MyApp2)  // ❌ 第二次使用，编译错误
```

**错误现象**：

```
error: redefinition of 'main'
note: previous definition is here
```

**✅ 正确代码**：

```cpp
// main.cpp
#include <mine/ui/app/AppAll.h>

MINE_APPLICATION_MAIN(MyApp1)  // ✅ 只使用一次
```

**原理解释**：
- 每次使用宏都会生成一个 `main` 函数
- C++ 标准规定一个程序只能有一个 `main` 函数
- 多次使用宏会导致链接器报错（重复定义）

---

### ❌ 陷阱 2：AppClass 不可默认构造

**问题描述**：Application 子类只有有参构造函数，没有无参构造函数，导致宏生成的 `main` 函数无法实例化对象。

**❌ 错误代码**：

```cpp
// MyApp.h
class MyApp : public mine::ui::app::Application {
public:
    MyApp(const std::string& config_path) : config_path_(config_path) {}
    // ❌ 没有无参构造函数
    
protected:
    void on_startup(int argc, char** argv) override {
        // ...
    }
    
private:
    std::string config_path_;
};
```

```cpp
// main.cpp
#include "MyApp.h"
#include <mine/ui/app/ApplicationMain.h>

MINE_APPLICATION_MAIN(MyApp)  // ❌ 编译错误：没有匹配的构造函数
```

**错误现象**：

```
error: no matching constructor for initialization of 'MyApp'
note: candidate constructor not viable: requires 1 argument, but 0 were provided
```

**✅ 正确代码**：

```cpp
// MyApp.h
class MyApp : public mine::ui::app::Application {
public:
    MyApp() : MyApp("config.json") {}  // ✅ 添加无参构造函数（委托构造）
    MyApp(const std::string& config_path) : config_path_(config_path) {}
    
protected:
    void on_startup(int argc, char** argv) override {
        // ...
    }
    
private:
    std::string config_path_;
};
```

```cpp
// main.cpp
#include "MyApp.h"
#include <mine/ui/app/ApplicationMain.h>

MINE_APPLICATION_MAIN(MyApp)  // ✅ 现在可以编译
```

**原理解释**：
- 宏生成的代码是 `AppClass _mine_app_;`，相当于调用无参构造函数
- 如果类只有有参构造函数，编译器不会自动生成无参构造函数
- 解决方案：添加无参构造函数（可以使用委托构造或提供默认值）

---

### ❌ 陷阱 3：忘记包含 ApplicationMain.h

**问题描述**：使用宏时忘记包含 `ApplicationMain.h` 头文件，导致宏未定义。

**❌ 错误代码**：

```cpp
// main.cpp
#include "MyApp.h"
// ❌ 忘记包含 ApplicationMain.h

MINE_APPLICATION_MAIN(MyApp)  // ❌ 编译错误：宏未定义
```

**错误现象**：

```
error: use of undeclared identifier 'MINE_APPLICATION_MAIN'
```

**✅ 正确代码**：

```cpp
// main.cpp
#include "MyApp.h"
#include <mine/ui/app/ApplicationMain.h>  // ✅ 包含头文件

MINE_APPLICATION_MAIN(MyApp)  // ✅ 宏可用
```

**原理解释**：
- `MINE_APPLICATION_MAIN` 宏定义在 `ApplicationMain.h` 头文件中
- 如果不包含该头文件，预处理器无法识别宏名称
- 解决方案：包含 `ApplicationMain.h` 或使用伞形头文件 `AppAll.h`

---

### ❌ 陷阱 4：类名拼写错误

**问题描述**：宏参数中的类名拼写错误，导致编译器无法找到该类型。

**❌ 错误代码**：

```cpp
// MyApp.h
class MyApp : public mine::ui::app::Application {
    // ...
};
```

```cpp
// main.cpp
#include "MyApp.h"
#include <mine/ui/app/ApplicationMain.h>

MINE_APPLICATION_MAIN(MyAp)  // ❌ 拼写错误：MyAp 而不是 MyApp
```

**错误现象**：

```
error: unknown type name 'MyAp'
did you mean 'MyApp'?
```

**✅ 正确代码**：

```cpp
// main.cpp
#include "MyApp.h"
#include <mine/ui/app/ApplicationMain.h>

MINE_APPLICATION_MAIN(MyApp)  // ✅ 正确的类名
```

**原理解释**：
- 宏参数是类型名称（不是字符串），编译器会在宏展开时检查类型是否存在
- 如果类名拼写错误，编译器会报错并提示可能的正确类名（如果有相似的类型）
- 现代 IDE 通常会在输入宏参数时提供自动补全，避免拼写错误

---

## 8. 完整示例

以下是一个完整的应用程序示例，展示了 `MINE_APPLICATION_MAIN` 宏的典型用法，包括命令行参数解析、异常处理、日志初始化等。

```cpp
// ════════════════════════════════════════════════════════════════
//  Logger.h - 简单的日志工具
// ════════════════════════════════════════════════════════════════

#pragma once
#include <iostream>
#include <fstream>
#include <string>
#include <chrono>
#include <iomanip>

class Logger {
public:
    enum class Level {
        Info,
        Warning,
        Error
    };
    
    static Logger& instance() {
        static Logger logger;
        return logger;
    }
    
    void init(const std::string& log_file) {
        log_file_.open(log_file, std::ios::out | std::ios::app);
        if (!log_file_.is_open()) {
            std::cerr << "Failed to open log file: " << log_file << std::endl;
        }
    }
    
    void log(Level level, const std::string& message) {
        auto now = std::chrono::system_clock::now();
        auto time = std::chrono::system_clock::to_time_t(now);
        
        const char* level_str = "";
        switch (level) {
            case Level::Info:    level_str = "INFO"; break;
            case Level::Warning: level_str = "WARN"; break;
            case Level::Error:   level_str = "ERROR"; break;
        }
        
        std::string log_line = "[" + std::string(std::ctime(&time)).substr(0, 24) + "] "
                               + level_str + ": " + message;
        
        std::cout << log_line << std::endl;
        if (log_file_.is_open()) {
            log_file_ << log_line << std::endl;
            log_file_.flush();
        }
    }
    
    ~Logger() {
        if (log_file_.is_open()) {
            log_file_.close();
        }
    }
    
private:
    Logger() = default;
    std::ofstream log_file_;
};

#define LOG_INFO(msg)    Logger::instance().log(Logger::Level::Info, msg)
#define LOG_WARNING(msg) Logger::instance().log(Logger::Level::Warning, msg)
#define LOG_ERROR(msg)   Logger::instance().log(Logger::Level::Error, msg)


// ════════════════════════════════════════════════════════════════
//  Config.h - 配置管理
// ════════════════════════════════════════════════════════════════

#pragma once
#include <string>
#include <fstream>
#include <stdexcept>

struct AppConfig {
    std::string theme = "Light";
    bool fullscreen = false;
    int window_width = 1280;
    int window_height = 720;
    
    static AppConfig load_from_file(const std::string& path) {
        std::ifstream file(path);
        if (!file.is_open()) {
            throw std::runtime_error("Failed to open config file: " + path);
        }
        
        // 简化版本：假设配置文件格式为 key=value
        AppConfig config;
        std::string line;
        while (std::getline(file, line)) {
            if (line.find("theme=") == 0) {
                config.theme = line.substr(6);
            } else if (line.find("fullscreen=") == 0) {
                config.fullscreen = (line.substr(11) == "true");
            } else if (line.find("window_width=") == 0) {
                config.window_width = std::stoi(line.substr(13));
            } else if (line.find("window_height=") == 0) {
                config.window_height = std::stoi(line.substr(14));
            }
        }
        
        return config;
    }
    
    void save_to_file(const std::string& path) const {
        std::ofstream file(path);
        if (!file.is_open()) {
            throw std::runtime_error("Failed to save config file: " + path);
        }
        
        file << "theme=" << theme << "\n";
        file << "fullscreen=" << (fullscreen ? "true" : "false") << "\n";
        file << "window_width=" << window_width << "\n";
        file << "window_height=" << window_height << "\n";
    }
};


// ════════════════════════════════════════════════════════════════
//  MyApp.h - 应用程序主类
// ════════════════════════════════════════════════════════════════

#pragma once
#include <mine/ui/app/AppAll.h>
#include <mine/ui/controls/ControlsAll.h>
#include "Logger.h"
#include "Config.h"

class MyApp : public mine::ui::app::Application {
public:
    MyApp() = default;
    ~MyApp() override = default;

protected:
    // ── 生命周期回调 ──
    
    void on_startup(int argc, char** argv) override {
        LOG_INFO("Application starting...");
        
        // 1. 解析命令行参数
        parse_command_line(argc, argv);
        
        // 2. 加载配置文件
        try {
            config_ = AppConfig::load_from_file("config.txt");
            LOG_INFO("Configuration loaded from config.txt");
        } catch (const std::exception& e) {
            LOG_WARNING(std::string("Failed to load config: ") + e.what());
            LOG_INFO("Using default configuration");
        }
        
        // 3. 创建主窗口
        create_main_window();
        
        LOG_INFO("Application started successfully");
    }
    
    void on_exit(int exit_code) override {
        LOG_INFO("Application exiting with code: " + std::to_string(exit_code));
        
        // 保存配置
        try {
            config_.save_to_file("config.txt");
            LOG_INFO("Configuration saved to config.txt");
        } catch (const std::exception& e) {
            LOG_ERROR(std::string("Failed to save config: ") + e.what());
        }
    }

private:
    // ── 初始化方法 ──
    
    void parse_command_line(int argc, char** argv) {
        for (int i = 1; i < argc; ++i) {
            std::string arg = argv[i];
            
            if (arg == "--fullscreen") {
                config_.fullscreen = true;
                LOG_INFO("Command line: fullscreen mode enabled");
            } else if (arg == "--theme" && i + 1 < argc) {
                config_.theme = argv[++i];
                LOG_INFO("Command line: theme set to " + config_.theme);
            } else if (arg == "--help") {
                print_help();
                quit(0);
                return;
            } else {
                LOG_WARNING("Unknown command line argument: " + arg);
            }
        }
    }
    
    void print_help() {
        std::cout << "Usage: MyApplication [OPTIONS]\n"
                  << "Options:\n"
                  << "  --fullscreen        Start in fullscreen mode\n"
                  << "  --theme <name>      Set theme (Light or Dark)\n"
                  << "  --help              Show this help message\n";
    }
    
    void create_main_window() {
        // 创建窗口描述符
        mine::platform::WindowDesc desc;
        desc.title = u"MineUI Application Demo";
        desc.size  = {
            static_cast<float>(config_.window_width),
            static_cast<float>(config_.window_height)
        };
        desc.fullscreen = config_.fullscreen;
        desc.resizable = true;
        desc.show_titlebar = true;
        
        main_window_ = create_window(desc);
        set_main_window(main_window_);
        
        // 创建 UI 内容
        create_ui_content();
        
        // 显示窗口
        main_window_->show();
        
        LOG_INFO("Main window created (" 
                 + std::to_string(config_.window_width) + "x" 
                 + std::to_string(config_.window_height) + ")");
    }
    
    void create_ui_content() {
        using namespace mine::ui::controls;
        
        // 根容器
        auto* root = new StackPanel();
        root->set_orientation(Orientation::Vertical);
        root->set_spacing(20.0f);
        root->set_padding(mine::ui::Thickness{20.0f});
        
        // 标题
        auto* title = new TextBlock();
        title->set_text(u"Welcome to MineUI!");
        title->set_font_size(32.0f);
        title->set_font_weight(mine::text::FontWeight::Bold);
        
        // 信息文本
        auto* info = new TextBlock();
        info->set_text(u"This is a complete application demo.\n"
                       u"Features: command line parsing, config management, logging.");
        info->set_font_size(16.0f);
        
        // 退出按钮
        auto* quit_button = new Button();
        quit_button->set_content(u"Quit Application");
        quit_button->set_width(200.0f);
        quit_button->set_height(40.0f);
        quit_button->clicked().connect([this]() {
            LOG_INFO("User clicked quit button");
            quit(0);
        });
        
        // 组装 UI
        root->add_child(title);
        root->add_child(info);
        root->add_child(quit_button);
        
        main_window_->set_content(root);
    }

private:
    AppConfig config_;                          ///< 应用配置
    mine::ui::Window* main_window_ = nullptr;   ///< 主窗口
};


// ════════════════════════════════════════════════════════════════
//  main.cpp - 进程入口（带异常处理）
// ════════════════════════════════════════════════════════════════

#include "MyApp.h"
#include <mine/ui/app/ApplicationMain.h>
#include <exception>

// 方式1：使用宏（简洁）
// MINE_APPLICATION_MAIN(MyApp)

// 方式2：手动编写 main 函数（支持自定义异常处理和日志初始化）
int main(int argc, char** argv) {
    // 1. 初始化日志系统
    Logger::instance().init("application.log");
    LOG_INFO("===== Application Process Started =====");
    
    int exit_code = 0;
    try {
        // 2. 创建并运行应用程序
        MyApp app;
        exit_code = app.run(argc, argv);
    } catch (const std::exception& e) {
        // 3. 捕获异常并记录日志
        LOG_ERROR(std::string("Fatal error: ") + e.what());
        exit_code = 1;
    } catch (...) {
        LOG_ERROR("Unknown fatal error!");
        exit_code = 1;
    }
    
    // 4. 记录退出日志
    LOG_INFO("===== Application Process Exited with code " 
             + std::to_string(exit_code) + " =====");
    
    return exit_code;
}


// ════════════════════════════════════════════════════════════════
//  CMakeLists.txt - 构建配置
// ════════════════════════════════════════════════════════════════

/*
cmake_minimum_required(VERSION 3.20)
project(MyApplication)

# 查找 MineFramework
find_package(MineFramework REQUIRED)

# 创建可执行文件
add_executable(MyApplication
    Logger.h
    Config.h
    MyApp.h
    main.cpp
)

# 链接 MineFramework 库
target_link_libraries(MyApplication PRIVATE
    MineFramework::mine.ui.app
    MineFramework::mine.ui.controls
)

# 设置 C++ 标准
set_property(TARGET MyApplication PROPERTY CXX_STANDARD 20)
*/


// ════════════════════════════════════════════════════════════════
//  config.txt - 示例配置文件
// ════════════════════════════════════════════════════════════════

/*
theme=Light
fullscreen=false
window_width=1280
window_height=720
*/


// ════════════════════════════════════════════════════════════════
//  运行效果
// ════════════════════════════════════════════════════════════════

/*
1. 编译并运行：

   $ cmake -B build
   $ cmake --build build
   $ ./build/MyApplication

2. 使用命令行参数：

   $ ./build/MyApplication --fullscreen
   $ ./build/MyApplication --theme Dark
   $ ./build/MyApplication --help

3. 预期控制台输出：

   [Wed Jun 11 10:00:00 2024] INFO: ===== Application Process Started =====
   [Wed Jun 11 10:00:00 2024] INFO: Application starting...
   [Wed Jun 11 10:00:00 2024] INFO: Configuration loaded from config.txt
   [Wed Jun 11 10:00:00 2024] INFO: Main window created (1280x720)
   [Wed Jun 11 10:00:00 2024] INFO: Application started successfully
   [Wed Jun 11 10:00:05 2024] INFO: User clicked quit button
   [Wed Jun 11 10:00:05 2024] INFO: Application exiting with code: 0
   [Wed Jun 11 10:00:05 2024] INFO: Configuration saved to config.txt
   [Wed Jun 11 10:00:05 2024] INFO: ===== Application Process Exited with code 0 =====

4. 生成的日志文件（application.log）：

   [Wed Jun 11 10:00:00 2024] INFO: ===== Application Process Started =====
   [Wed Jun 11 10:00:00 2024] INFO: Application starting...
   [Wed Jun 11 10:00:00 2024] INFO: Configuration loaded from config.txt
   [Wed Jun 11 10:00:00 2024] INFO: Main window created (1280x720)
   [Wed Jun 11 10:00:00 2024] INFO: Application started successfully
   [Wed Jun 11 10:00:05 2024] INFO: User clicked quit button
   [Wed Jun 11 10:00:05 2024] INFO: Application exiting with code: 0
   [Wed Jun 11 10:00:05 2024] INFO: Configuration saved to config.txt
   [Wed Jun 11 10:00:05 2024] INFO: ===== Application Process Exited with code 0 =====

5. 异常处理示例（如果配置文件损坏）：

   [Wed Jun 11 10:00:00 2024] INFO: ===== Application Process Started =====
   [Wed Jun 11 10:00:00 2024] INFO: Application starting...
   [Wed Jun 11 10:00:00 2024] WARN: Failed to load config: Failed to open config file: config.txt
   [Wed Jun 11 10:00:00 2024] INFO: Using default configuration
   [Wed Jun 11 10:00:00 2024] INFO: Main window created (1280x720)
   [Wed Jun 11 10:00:00 2024] INFO: Application started successfully
   ...
*/
```

---

## 9. 总结

### 9.1 核心要点回顾

- **宏的作用**：`MINE_APPLICATION_MAIN(AppClass)` 自动生成标准的 `main` 函数，实例化 Application 子类并调用 `run()` 方法。
- **宏展开机制**：宏展开为 `int main(int argc, char** argv) { AppClass _mine_app_; return _mine_app_.run(argc, argv); }`。
- **唯一性约束**：宏只能在一个翻译单元中使用一次，重复使用会导致 `main` 函数重复定义。
- **可默认构造约束**：AppClass 必须有无参构造函数（或默认生成的构造函数）。
- **跨平台兼容**：所有平台使用相同的宏，平台差异由 Application 内部处理。
- **生命周期管理**：应用程序对象在栈上创建，自动管理生命周期，异常安全。
- **最佳实践**：优先使用宏简化入口、在单独的文件中使用宏、在 `on_startup()` 中处理命令行参数、根据需求选择异常捕获策略。
- **常见陷阱**：多次使用宏、AppClass 不可默认构造、忘记包含 ApplicationMain.h、类名拼写错误。

### 9.2 与其他模块的协作关系

```
MINE_APPLICATION_MAIN 宏
    ↓ 生成
int main(int argc, char** argv)
    ↓ 实例化
AppClass _mine_app_;
    ↓ 调用
_mine_app_.run(argc, argv)
    ↓ 初始化
IApplicationHost / IDevice / IRenderer
    ↓ 回调
on_startup(argc, argv)
    ↓ 创建
ui::Window
    ↓ 驱动
主消息循环
```

### 9.3 适用场景总结

- **标准单窗口应用**：最常见的场景，使用宏简化进程入口。
- **命令行参数处理**：在 `on_startup()` 中解析参数，宏会自动传递 `argc/argv`。
- **多实例检测**：在 `on_startup()` 中检测单实例锁，如果已有实例则退出。
- **异常处理**：根据需求选择在 `on_startup()` 中捕获（可恢复）或在 `main` 函数中捕获（致命错误）。
- **跨平台入口**：所有平台使用相同的宏，无需 `#ifdef` 或平台特定代码。
- **带命名空间的应用类**：宏参数可以包含命名空间（如 `myns::MyApp`）。

### 9.4 下一步学习建议

- **Application 类**：深入了解 Application 的生命周期、窗口管理、主题系统等（参考 [01-Application.md](01-Application.md)）。
- **平台入口差异**：了解 Windows `WinMain` / macOS `NSApplicationMain` / Linux `main` 的差异，以及 MineFramework 如何统一它们。
- **异常处理策略**：学习如何设计应用程序的异常处理策略（可恢复 vs 致命错误）。
- **日志系统集成**：学习如何在应用程序中集成日志系统，记录启动、退出、异常等事件。
- **配置管理**：学习如何设计应用程序的配置管理系统（文件格式、加载、保存、默认值等）。
- **命令行解析库**：了解第三方命令行解析库（如 CLI11、argparse 等），简化参数处理。

---

**相关文档**：
- [01-Application.md](01-Application.md) - Application 类详细接口
- [03-ModuleMetadata.md](03-ModuleMetadata.md) - Api.h 和 AppAll.h 详解
- [platform/01-IApplicationHost.md](../platform/01-IApplicationHost.md) - 平台宿主接口
