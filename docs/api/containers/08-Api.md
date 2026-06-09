# Api.h 详细接口文档

## 概述

`Api.h` 定义了 `MINE_CONTAINERS_API` 宏,用于控制 `mine.containers` 模块中类型和函数的 DLL 导出/导入。这是实现跨 DLL 边界 ABI 稳定性的关键机制。

---

## 宏定义

```cpp
#pragma once

#ifdef MINE_CONTAINERS_EXPORTS
    // 构建 mine.containers DLL 时导出符号
    #define MINE_CONTAINERS_API __declspec(dllexport)
#else
    // 使用 mine.containers DLL 时导入符号
    #define MINE_CONTAINERS_API __declspec(dllimport)
#endif
```

---

## 工作原理

### Windows 平台（MSVC）

- **`__declspec(dllexport)`**：标记符号应导出到 DLL 的导出表
- **`__declspec(dllimport)`**：标记符号从 DLL 导入,编译器生成导入表引用

### 其他平台

在 Linux/macOS 等平台,通常使用 GCC/Clang 的可见性属性:

```cpp
#ifdef __GNUC__
    #define MINE_CONTAINERS_API __attribute__((visibility("default")))
#endif
```

MineFramework 目前主要针对 Windows/MSVC,其他平台的定义可在未来扩展。

---

## 使用方式

### 在类定义中使用

```cpp
#include <mine/containers/Api.h>

namespace mine::containers {

// 导出整个类
class MINE_CONTAINERS_API Vector<T> {
public:
    Vector();
    void push_back(const T& value);
};

}
```

**效果**：
- 构建 DLL 时：`Vector` 的所有公共成员函数导出
- 使用 DLL 时：编译器生成导入代码

---

### 在函数声明中使用

```cpp
namespace mine::containers {

// 导出单个函数
MINE_CONTAINERS_API void some_utility_function();

}
```

---

### 在模板类中使用

对于模板类,通常 **不使用** `MINE_CONTAINERS_API`:

```cpp
// ✅ 正确：模板类不导出（在头文件中完整定义）
template<typename T>
class Vector {
public:
    void push_back(const T& value);
};

// ❌ 错误：模板类不能导出
template<typename T>
class MINE_CONTAINERS_API BadVector {  // 编译错误
    // ...
};
```

**原因**：模板类在每个使用它的编译单元中实例化,无需跨 DLL 导出。

---

## 编译配置

### 构建 mine.containers DLL

在 DLL 项目的编译选项中定义 `MINE_CONTAINERS_EXPORTS`:

```cmake
# CMakeLists.txt
add_library(mine.containers SHARED
    src/Vector.cpp
    # ...
)
target_compile_definitions(mine.containers PRIVATE MINE_CONTAINERS_EXPORTS)
```

**效果**：
- `MINE_CONTAINERS_API` 展开为 `__declspec(dllexport)`
- 符号导出到 DLL

---

### 使用 mine.containers DLL

用户项目无需定义 `MINE_CONTAINERS_EXPORTS`:

```cmake
# 用户的 CMakeLists.txt
add_executable(MyApp main.cpp)
target_link_libraries(MyApp mine.containers)
```

**效果**：
- `MINE_CONTAINERS_API` 展开为 `__declspec(dllimport)`
- 编译器生成导入代码

---

## ABI 稳定性

### 为什么需要 MINE_CONTAINERS_API？

标准库容器（如 `std::vector`）在不同编译器/标准库实现间 ABI 不兼容:
- MSVC：内存布局、内联函数实现不同
- GCC/Clang：不同版本的 libstdc++/libc++ 不兼容

使用 `MINE_CONTAINERS_API` 确保:
1. 符号从 DLL 导出,不内联到用户代码
2. 跨 DLL 边界调用统一的实现
3. 内存管理由 DLL 内部的分配器统一处理

---

### 示例：跨 DLL 边界传递容器

```cpp
// mine.containers DLL 导出的 API
class MINE_CONTAINERS_API DataProvider {
public:
    // 返回 Vector：安全（ABI 稳定）
    Vector<int> get_data();
};

// 用户代码
void user_code() {
    DataProvider provider;
    Vector<int> data = provider.get_data();  // 跨 DLL 边界，安全
    
    // ❌ 不安全：std::vector 跨 DLL 边界
    // std::vector<int> bad_data = provider.get_std_data();  // ABI 不稳定!
}
```

---

## 模块导出约定

`mine.containers` 模块的导出约定:

| 类型 | 是否导出 | 说明 |
|------|---------|------|
| **模板类** | ❌ 否 | 在头文件中完整定义,无需导出 |
| **具体类** | ✅ 是 | 使用 `MINE_CONTAINERS_API` 导出 |
| **自由函数** | ✅ 是 | 使用 `MINE_CONTAINERS_API` 导出 |
| **内联函数** | ❌ 否 | 标记 `inline`,编译器自动内联 |

---

## 完整示例

### 示例：定义导出的工具类

```cpp
// mine/containers/api/include/mine/containers/SomeClass.h
#pragma once

#include <mine/containers/Api.h>

namespace mine::containers {

// 导出的具体类
class MINE_CONTAINERS_API SomeClass {
public:
    SomeClass();
    ~SomeClass();
    
    void do_something();
    
private:
    struct Impl;
    Impl* pimpl_;
};

}

// src/mine/containers/SomeClass.cpp
#include <mine/containers/SomeClass.h>

namespace mine::containers {

struct SomeClass::Impl {
    // 实现细节
};

SomeClass::SomeClass() : pimpl_(new Impl) {}
SomeClass::~SomeClass() { delete pimpl_; }

void SomeClass::do_something() {
    // 实现
}

}
```

---

## 最佳实践

### 1. 模板类不使用 MINE_CONTAINERS_API

```cpp
// ✅ 正确
template<typename T>
class Vector {
    // 完整定义在头文件
};

// ❌ 错误
template<typename T>
class MINE_CONTAINERS_API Vector {  // 编译错误
    // ...
};
```

---

### 2. PIMPL 类使用导出宏

```cpp
// ✅ 推荐：PIMPL 类导出
class MINE_CONTAINERS_API SomeClass {
    struct Impl;  // 前向声明,隐藏实现
    Impl* pimpl_;
};
```

---

### 3. 内联函数不使用导出宏

```cpp
// ✅ 正确：内联函数无需导出
class SomeClass {
public:
    inline int get_value() const { return value_; }
private:
    int value_;
};
```

---

## 相关文件

- **`mine/core/Api.h`**：定义 `MINE_API`,用于 `mine.core` 模块
- **`mine/ui/Api.h`**：定义 `MINE_UI_API`,用于 `mine.ui` 模块

每个模块都有独立的导出宏,避免符号冲突。

---

## 总结

`Api.h` 定义了 `MINE_CONTAINERS_API` 宏,用于:

1. **控制符号导出/导入**：构建 DLL 时导出,使用时导入
2. **实现 ABI 稳定性**：确保跨 DLL 边界安全
3. **跨平台支持**：Windows (`__declspec`) 和 Linux/macOS (`__attribute__`) 统一接口

**使用原则**：
- 模板类：不使用导出宏
- 具体类：使用 `MINE_CONTAINERS_API` 导出
- 内联函数：不使用导出宏
