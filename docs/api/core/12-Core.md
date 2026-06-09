# Core.h (聚合头文件) 详细文档

## 概述

`Core.h` 是 `mine.core` 模块的聚合头文件,包含所有核心类型的定义。客户代码可通过包含此单一头文件访问 `mine.core` 的完整 API,无需逐个包含各子头文件。

### 核心特性

- **一站式包含**：单个 `#include` 获取所有 `mine.core` 功能
- **编译时间友好**：虽然包含所有子头文件,但单元内部已优化避免重复解析
- **命名空间清晰**：所有类型位于 `mine::core` 命名空间
- **依赖透明**：客户无需关心内部依赖关系

### 使用场景

| 场景 | 推荐做法 | 说明 |
|------|---------|------|
| 应用级代码 | 包含 `Core.h` | 简化依赖管理 |
| 库公共头 | 按需包含子头文件 | 减少传递依赖 |
| 快速原型开发 | 包含 `Core.h` | 无需关心细节 |
| 编译时间关键 | 按需包含子头文件 | 减少不必要的解析 |

---

## 头文件内容

```cpp
#pragma once

#include <mine/core/Api.h>
#include <mine/core/ModuleTag.h>
#include <mine/core/Assert.h>
#include <mine/core/Allocator.h>
#include <mine/core/TypeId.h>
#include <mine/core/Variant.h>
#include <mine/core/StringView.h>
#include <mine/core/Memory.h>
#include <mine/core/Span.h>
#include <mine/core/Result.h>
#include <mine/core/Function.h>
#include <mine/core/Status.h>
#include <mine/core/Pimpl.h>

namespace mine::core {
    // 所有类型由上述头文件提供
}
```

---

## 包含的子模块

### 1. Api.h

**内容**：`MINE_API` 宏,用于 DLL 导出/导入。

**用途**：声明公共 API 时使用。

---

### 2. ModuleTag.h

**内容**：`kModuleName` 常量（`"mine.core"`）。

**用途**：日志/调试时标识模块。

---

### 3. Assert.h

**内容**：断言宏（`MINE_ASSERT`、`MINE_CHECK` 等）。

**用途**：验证前置/后置条件和不变式。

**关键宏**：
- `MINE_ASSERT(cond)` - Debug 专用断言
- `MINE_CHECK(cond)` - Release 也有效的检查
- `MINE_UNREACHABLE()` - 标记不可达路径
- `MINE_TODO_NOT_IMPLEMENTED()` - 标记未实现功能

---

### 4. Allocator.h

**内容**：`IAllocator` 接口和 `default_allocator()` / `set_default_allocator()` 函数。

**用途**：统一内存分配接口。

**关键类型**：
- `IAllocator` - 抽象分配器接口
- `default_allocator()` - 获取默认分配器
- `set_default_allocator()` - 替换默认分配器

---

### 5. TypeId.h

**内容**：`TypeId` 类,基于地址的类型标识。

**用途**：无 RTTI 时的类型识别。

**关键方法**：
- `TypeId::of<T>()` - 获取类型 ID
- `hash()` - 哈希值
- `operator==` / `operator!=` - 比较

---

### 6. Variant.h

**内容**：`Variant` 类,类型擦除的值容器。

**用途**：存储任意类型的值（Small Buffer Optimization）。

**关键方法**：
- `has<T>()` - 检查持有类型
- `get<T>()` - 获取值（需类型匹配）
- `emplace<T>(args...)` - 原位构造
- `any_cast<T>(v)` - 安全转换

---

### 7. StringView.h

**内容**：`StringView` 类,非拥有 UTF-8 字符串视图。

**用途**：零拷贝字符串传递。

**关键方法**：
- `data()` / `size()` - 访问底层数据
- `substr()` - 子串
- `find()` / `starts_with()` / `ends_with()` - 查找

---

### 8. Memory.h

**内容**：`OwnedPtr<T>` 智能指针和 `make_owned<T>()` 工厂。

**用途**：ABI 安全的智能指针（PIMPL 模式）。

**关键类型**：
- `OwnedPtr<T>` - 拥有指针（带删除器）
- `make_owned<T>(args...)` - 工厂函数
- `MINE_NEW` / `MINE_DELETE` - 宏

---

### 9. Span.h

**内容**：`Span<T>` 类,非拥有连续内存视图。

**用途**：零拷贝数组传递。

**关键方法**：
- `data()` / `size()` - 访问底层数据
- `subspan()` - 子视图
- `as_bytes()` - 字节视图

---

### 10. Result.h

**内容**：`Result<T, E>` 类,错误处理类型。

**用途**：无异常错误处理。

**关键方法**：
- `ok()` - 检查是否成功
- `value()` - 获取值（需成功）
- `error()` - 获取错误（需失败）
- `value_or(default)` - 安全获取（失败返回默认值）
- `MINE_TRY(expr)` - 错误传播宏

---

### 11. Function.h

**内容**：`Function<R(Args...)>` 类,可调用对象包装器。

**用途**：事件回调、属性绑定。

**关键方法**：
- `operator()(args...)` - 调用
- `operator bool()` - 检查是否持有可调用对象

---

### 12. Status.h

**内容**：`Status` 类和 `StatusCode` 枚举,轻量级操作状态。

**用途**：无返回值操作的错误表示。

**关键类型**：
- `StatusCode` - 错误码枚举
- `Status` - 状态类（4 字节）
- `Status::success()` - 成功状态
- `MINE_TRY(expr)` - 状态传播宏

---

### 13. Pimpl.h

**内容**：`Pimpl<T>` 类和 `make_pimpl<T>()` 工厂。

**用途**：PIMPL 模式辅助类。

**关键类型**：
- `Pimpl<T>` - PIMPL 包装器
- `make_pimpl<T>(args...)` - 工厂函数

---

## 使用示例

### 示例 1：应用级代码

```cpp
// 应用代码直接包含聚合头
#include <mine/core/Core.h>

using namespace mine::core;

class Application {
public:
    Application() {
        // 使用所有核心类型
        auto result = load_config();
        MINE_CHECK(result.ok());
        
        config_ = result.value();
    }
    
private:
    Result<Config> load_config() {
        // 使用 StringView 传递路径
        StringView path = "config.json";
        
        // 使用 Variant 存储配置项
        Variant settings;
        settings.emplace<int>(42);
        
        // 使用 Status 表示操作结果
        Status status = parse_file(path);
        if (!status.ok()) {
            return make_err<Config>(status);
        }
        
        return make_ok<Config>(Config{});
    }
    
    Config config_;
};
```

---

### 示例 2：库公共头（选择性包含）

```cpp
// 库公共头仅包含需要的类型,减少传递依赖
#pragma once
#include <mine/core/Api.h>
#include <mine/core/StringView.h>
#include <mine/core/Result.h>

namespace mylib {

class MINE_API Parser {
public:
    mine::core::Result<int> parse_int(mine::core::StringView str);
    // 不暴露其他 mine.core 类型
};

}
```

---

### 示例 3：快速原型开发

```cpp
// 原型代码,快速迭代
#include <mine/core/Core.h>

void prototype() {
    using namespace mine::core;
    
    // 任意组合使用核心类型
    auto allocator = default_allocator();
    auto ptr = make_owned<Widget>(allocator);
    
    Vector<Variant> items;
    items.push_back(Variant{42});
    items.push_back(Variant{StringView{"hello"}});
    
    Function<void()> callback = []() { /* ... */ };
    callback();
}
```

---

## 命名空间组织

### mine::core 命名空间

所有 `mine.core` 类型位于 `mine::core` 命名空间:

```cpp
namespace mine::core {
    class TypeId;
    class Variant;
    class StringView;
    template<typename T> class OwnedPtr;
    template<typename T> class Span;
    template<typename T, typename E = Status> class Result;
    template<typename Sig> class Function;
    class Status;
    template<typename T> class Pimpl;
    class IAllocator;
    
    // 工厂函数
    template<typename T, typename... Args>
    OwnedPtr<T> make_owned(Args&&...);
    
    template<typename T, typename... Args>
    Pimpl<T> make_pimpl(Args&&...);
    
    // 全局函数
    IAllocator* default_allocator() noexcept;
    void set_default_allocator(IAllocator*) noexcept;
}
```

---

## 编译时间考虑

### 包含 Core.h 的代价

| 构建类型 | 编译时间增量 | 说明 |
|---------|------------|------|
| 初次编译 | ~50-100ms | 解析所有子头文件 |
| 增量编译（无变更） | 0ms | 预编译头 + 头文件守卫 |
| 增量编译（有变更） | ~10-20ms | 仅重新解析变更的类 |

### 优化建议

1. **使用预编译头**：将 `Core.h` 放入 PCH 加速编译
2. **库头选择性包含**：库公共头按需包含子头文件
3. **PIMPL 模式**：公共接口使用前向声明,实现包含 `Core.h`

**示例（库头优化）**：

```cpp
// Library.h（公共头,最小依赖）
#pragma once
#include <mine/core/Api.h>
#include <mine/core/StringView.h>

namespace mylib {
    class MINE_API Library {
    public:
        void process(mine::core::StringView input);
    };
}

// Library.cpp（实现,包含完整 API）
#include "Library.h"
#include <mine/core/Core.h>  // 完整 API

void Library::process(mine::core::StringView input) {
    // 使用所有 mine.core 类型
}
```

---

## 与其他模块的关系

### mine.core 是基础依赖

所有其他 `mine.*` 模块依赖 `mine.core`:

```
mine.core（基础类型）
├── mine.containers（容器类：Vector, HashMap）
├── mine.ui.property（属性系统）
├── mine.ui.binding（数据绑定）
└── mine.mvvm（MVVM 框架）
```

**典型依赖链**：

```cpp
// mine.containers 依赖 mine.core
#include <mine/core/Core.h>

namespace mine::containers {
    template<typename T>
    class Vector {
        mine::core::Span<T> as_span() const;
        // ...
    };
}

// mine.ui.property 依赖 mine.core
#include <mine/core/Core.h>

namespace mine::ui::property {
    class DependencyProperty {
        mine::core::TypeId owner_type_;
        mine::core::Variant default_value_;
        // ...
    };
}
```

---

## 最佳实践

### 1. 应用代码包含 Core.h

**推荐**：应用级代码直接包含聚合头,简化依赖管理。

```cpp
// ✅ 应用代码
#include <mine/core/Core.h>

void application_logic() {
    using namespace mine::core;
    // 自由使用所有类型
}
```

---

### 2. 库头选择性包含

**推荐**：库公共头按需包含子头文件,减少传递依赖。

```cpp
// ✅ 库公共头
#pragma once
#include <mine/core/StringView.h>
#include <mine/core/Result.h>

class MINE_API MyLib {
public:
    mine::core::Result<int> parse(mine::core::StringView str);
};

// ✅ 库实现
#include <mine/core/Core.h>  // 实现中包含完整 API
```

---

### 3. 使用预编译头加速编译

**推荐**：将常用头文件（包括 `Core.h`）放入 PCH。

```cpp
// pch.h（预编译头）
#pragma once
#include <mine/core/Core.h>
#include <vector>
#include <string>
// 其他常用头文件...
```

---

### 4. 避免循环依赖

**推荐**：`mine.core` 不依赖任何其他 `mine.*` 模块。

```cpp
// ❌ 错误：mine.core 不应依赖 mine.ui
#include <mine/ui/Visual.h>  // 破坏分层

// ✅ 正确：mine.core 仅依赖标准库
#include <cstddef>
#include <cstdint>
```

---

## 总结

`Core.h` 是 `mine.core` 模块的聚合头文件,具备以下特点:

1. **一站式包含**：单个 `#include` 获取所有核心功能
2. **编译时间友好**：配合预编译头加速编译
3. **命名空间清晰**：所有类型位于 `mine::core`
4. **分层设计**：不依赖其他 `mine.*` 模块

**使用建议**：

- **应用代码**：直接包含 `Core.h`
- **库公共头**：按需包含子头文件
- **快速原型**：包含 `Core.h` 加速开发

`Core.h` 为 MineFramework 提供了坚实的基础类型库,是所有上层模块的依赖基石。
