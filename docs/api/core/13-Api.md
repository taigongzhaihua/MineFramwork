# Api.h (DLL 导出宏) 详细文档

## 概述

`Api.h` 定义了 `MINE_API` 宏,用于标记跨 DLL 边界的公共 API。所有需要从动态链接库导出的类/函数必须使用此宏,以确保在 Windows 平台上正确生成导入/导出符号。

### 核心特性

- **平台抽象**：自动适配 Windows / Linux / macOS
- **导入/导出切换**：根据 `MINE_EXPORT` 宏自动选择导出或导入
- **ABI 稳定**：配合 PIMPL 模式确保二进制兼容性
- **零开销**：仅影响符号可见性,无运行时开销

### 设计动机

在 Windows 平台上,动态链接库（DLL）的符号默认不导出,必须显式标记:

- **导出**：在编译 DLL 时,使用 `__declspec(dllexport)` 标记公共符号
- **导入**：在使用 DLL 时,使用 `__declspec(dllimport)` 标记外部符号

`MINE_API` 宏自动根据编译上下文选择正确的标记,简化跨平台开发。

---

## 宏定义

### 完整定义

```cpp
#pragma once

#if defined(_WIN32) || defined(_WIN64)
    // Windows 平台
    #ifdef MINE_EXPORT
        // 编译 DLL 时定义 MINE_EXPORT,导出符号
        #define MINE_API __declspec(dllexport)
    #else
        // 使用 DLL 时不定义 MINE_EXPORT,导入符号
        #define MINE_API __declspec(dllimport)
    #endif
#else
    // Linux / macOS（符号默认导出）
    #define MINE_API __attribute__((visibility("default")))
#endif
```

---

## 宏行为

### Windows 平台

| 构建场景 | MINE_EXPORT | MINE_API 展开为 | 说明 |
|---------|------------|----------------|------|
| 编译 DLL | 已定义 | `__declspec(dllexport)` | 导出符号到 `.lib` / `.dll` |
| 使用 DLL | 未定义 | `__declspec(dllimport)` | 从 `.lib` 导入符号 |

**示例**：

```cpp
// 编译 mine.core.dll 时（定义 MINE_EXPORT）
class MINE_API Widget {  // 展开为 __declspec(dllexport)
    // ...
};

// 使用 mine.core.dll 时（未定义 MINE_EXPORT）
class MINE_API Widget {  // 展开为 __declspec(dllimport)
    // ...
};
```

---

### Linux / macOS 平台

| 构建场景 | MINE_API 展开为 | 说明 |
|---------|----------------|------|
| 任意场景 | `__attribute__((visibility("default")))` | 显式导出符号 |

**说明**：
- Linux / macOS 默认导出所有符号,但可通过 `-fvisibility=hidden` 隐藏
- `__attribute__((visibility("default")))` 显式标记公共 API

---

## 使用场景

### 1. 类声明

```cpp
#include <mine/core/Api.h>

// 导出整个类
class MINE_API Widget {
public:
    Widget();
    ~Widget();
    void do_work();
};
```

**效果**：
- 类的所有公共成员（构造/析构/方法/静态成员）均被导出
- 虚函数表（vtable）被导出

---

### 2. 函数声明

```cpp
#include <mine/core/Api.h>

// 导出单个函数
MINE_API void initialize_framework();
MINE_API int calculate(int a, int b);
```

**效果**：函数符号被导出,可从 DLL 外部调用。

---

### 3. 全局变量（不推荐）

```cpp
#include <mine/core/Api.h>

// 导出全局变量（跨 DLL 边界危险）
extern MINE_API int g_global_counter;
```

**警告**：全局变量跨 DLL 边界存在多实例问题,应使用访问器函数:

```cpp
// ✅ 推荐：通过函数访问
MINE_API int get_global_counter();
MINE_API void set_global_counter(int value);
```

---

### 4. 模板类（仅显式实例化）

```cpp
// 模板定义（头文件内联,无需导出）
template<typename T>
class Vector {
    // ...
};

// 显式实例化（导出常用实例）
extern template class MINE_API Vector<int>;
extern template class MINE_API Vector<float>;
```

**说明**：
- 模板定义通常在头文件中内联,无需 `MINE_API`
- 若需导出特定实例,使用显式实例化 + `MINE_API`

---

## 构建配置

### CMakeLists.txt 配置

```cmake
# 编译 mine.core.dll 时定义 MINE_EXPORT
add_library(mine_core SHARED
    src/Widget.cpp
    # ...
)

target_compile_definitions(mine_core PRIVATE MINE_EXPORT)

# 使用 mine.core.dll 的客户代码不定义 MINE_EXPORT
add_executable(app
    main.cpp
)

target_link_libraries(app PRIVATE mine_core)
# app 自动从 mine_core.lib 导入符号
```

---

### xmake.lua 配置

```lua
-- 编译 mine.core.dll
target("mine.core")
    set_kind("shared")
    add_files("src/**.cpp")
    add_defines("MINE_EXPORT")  -- 导出符号

-- 使用 mine.core.dll 的应用
target("app")
    set_kind("binary")
    add_files("main.cpp")
    add_deps("mine.core")  -- 自动链接,导入符号
```

---

## 符号导出示例

### 示例 1：导出类

```cpp
// Widget.h
#pragma once
#include <mine/core/Api.h>

class MINE_API Widget {
public:
    Widget();
    ~Widget();
    
    void set_value(int value);
    int get_value() const;

private:
    struct Impl;
    Pimpl<Impl> pimpl_;
};
```

```cpp
// Widget.cpp
#include "Widget.h"

struct Widget::Impl {
    int value{0};
};

Widget::Widget() : pimpl_{make_pimpl<Impl>()} {}
Widget::~Widget() = default;

void Widget::set_value(int value) {
    pimpl_->value = value;
}

int Widget::get_value() const {
    return pimpl_->value;
}
```

**导出符号**：

```
Widget::Widget()
Widget::~Widget()
Widget::set_value(int)
Widget::get_value() const
```

---

### 示例 2：导出函数

```cpp
// Core.h
#pragma once
#include <mine/core/Api.h>

MINE_API void initialize_graphics();
MINE_API void shutdown_graphics();
```

```cpp
// Core.cpp
#include "Core.h"

void initialize_graphics() {
    // 初始化逻辑...
}

void shutdown_graphics() {
    // 清理逻辑...
}
```

**导出符号**：

```
initialize_graphics
shutdown_graphics
```

---

## 常见问题

### Q1：为什么需要 MINE_API？

**A**：Windows 平台上,DLL 符号默认不导出,必须显式标记。Linux / macOS 虽默认导出,但显式标记提高代码清晰度。

---

### Q2：内联函数需要 MINE_API 吗？

**A**：不需要。内联函数在头文件中定义,无需导出:

```cpp
class MINE_API Widget {
public:
    // ✅ 内联函数,无需额外标记
    int get_value() const { return value_; }

private:
    int value_{0};
};
```

---

### Q3：纯虚基类需要 MINE_API 吗？

**A**：需要（若跨 DLL 边界派生）:

```cpp
class MINE_API IAllocator {
public:
    virtual ~IAllocator() = default;
    virtual void* alloc(size_t size) = 0;
    virtual void dealloc(void* ptr) = 0;
};

// 客户代码（DLL 外）派生
class MyAllocator : public IAllocator {  // 需导入 IAllocator 的 vtable
    // ...
};
```

---

### Q4：模板类需要 MINE_API 吗？

**A**：通常不需要（内联定义）。若显式实例化,需标记:

```cpp
// ❌ 模板定义不需要
template<typename T>
class Vector { /* ... */ };

// ✅ 显式实例化需要
extern template class MINE_API Vector<int>;
```

---

### Q5：静态成员变量如何导出？

**A**：在类定义中使用 `MINE_API`:

```cpp
class MINE_API Config {
public:
    static MINE_API int max_threads;  // 导出静态变量
};

// Config.cpp
int Config::max_threads = 8;
```

**推荐**：使用访问器函数避免跨 DLL 边界访问变量:

```cpp
class MINE_API Config {
public:
    static int get_max_threads();
    static void set_max_threads(int value);
};
```

---

## 最佳实践

### 1. 所有公共 API 必须标记 MINE_API

**推荐**：跨 DLL 边界的类/函数始终使用 `MINE_API`。

```cpp
// ✅ 正确
class MINE_API Widget { /* ... */ };
MINE_API void initialize();

// ❌ 错误：遗漏 MINE_API
class Widget { /* ... */ };  // Windows 上链接失败
void initialize();            // Windows 上链接失败
```

---

### 2. 内部实现类不使用 MINE_API

**推荐**：仅导出公共接口,内部实现隐藏在 `.cpp` 中。

```cpp
// Widget.h（公共接口）
class MINE_API Widget {
    struct Impl;  // 前向声明,无需 MINE_API
    Pimpl<Impl> pimpl_;
};

// Widget.cpp（内部实现,不导出）
struct Widget::Impl {  // 无 MINE_API
    int value;
};
```

---

### 3. 优先使用 PIMPL 模式

**推荐**：跨 DLL 边界的类使用 PIMPL 模式,隐藏实现细节。

```cpp
// ✅ 推荐：PIMPL 模式
class MINE_API Widget {
    struct Impl;
    Pimpl<Impl> pimpl_;
public:
    Widget();
    ~Widget();
};

// ❌ 不推荐：暴露成员变量
class MINE_API Widget {
public:
    std::vector<int> data;  // 破坏 ABI 稳定性
};
```

---

### 4. 避免导出 STL 类型

**推荐**：不直接导出 STL 容器（不同编译器/版本不兼容）。

```cpp
// ❌ 错误：导出 STL 类型
class MINE_API Widget {
public:
    std::vector<int> get_data();  // ABI 不稳定
};

// ✅ 正确：使用 Span<T> 传递
class MINE_API Widget {
public:
    Span<const int> get_data();  // ABI 稳定
};
```

---

## 完整示例

### 示例：跨 DLL 边界的库

```cpp
// MathLib.h（公共头）
#pragma once
#include <mine/core/Api.h>

class MINE_API MathLib {
public:
    MathLib();
    ~MathLib();

    int add(int a, int b);
    int multiply(int a, int b);

private:
    struct Impl;
    Pimpl<Impl> pimpl_;
};

// 导出的全局函数
MINE_API void initialize_math_lib();
MINE_API void shutdown_math_lib();
```

```cpp
// MathLib.cpp（实现）
#include "MathLib.h"

struct MathLib::Impl {
    // 内部状态...
};

MathLib::MathLib() : pimpl_{make_pimpl<Impl>()} {}
MathLib::~MathLib() = default;

int MathLib::add(int a, int b) { return a + b; }
int MathLib::multiply(int a, int b) { return a * b; }

void initialize_math_lib() { /* ... */ }
void shutdown_math_lib() { /* ... */ }
```

```cpp
// main.cpp（客户代码）
#include <MathLib.h>

int main() {
    initialize_math_lib();
    
    MathLib lib;
    int result = lib.add(2, 3);  // 调用 DLL 导出的函数
    
    shutdown_math_lib();
    return 0;
}
```

---

## 总结

`MINE_API` 宏是 MineFramework 中标记跨 DLL 边界 API 的标准方式,具备以下优势:

1. **平台抽象**：自动适配 Windows / Linux / macOS
2. **导入/导出切换**：根据 `MINE_EXPORT` 宏自动选择
3. **ABI 稳定**：配合 PIMPL 模式确保二进制兼容性
4. **零开销**：仅影响符号可见性

在设计跨 DLL 边界的 API 时,始终使用 `MINE_API` 标记公共类/函数,这将确保在所有平台上正确导出/导入符号,避免链接错误。
