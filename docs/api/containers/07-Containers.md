# Containers.h 详细接口文档

## 概述

`Containers.h` 是 `mine.containers` 模块的 **伞头文件**,一次性包含模块中所有公共容器类型。用户只需包含此头文件即可使用模块的全部功能。

---

## 文件内容

```cpp
#pragma once

#include <mine/containers/Vector.h>
#include <mine/containers/SmallVector.h>
#include <mine/containers/HashMap.h>
#include <mine/containers/IntrusiveList.h>
#include <mine/containers/InlineString.h>
#include <mine/containers/Hash.h>
```

---

## 包含的类型

| 头文件 | 类型 | 描述 |
|-------|------|------|
| `Vector.h` | `Vector<T>` | 动态数组容器 |
| `SmallVector.h` | `SmallVector<T, N>` | 带 SBO 的动态数组 |
| `HashMap.h` | `HashMap<K, V>` | Robin Hood 哈希表 |
| `IntrusiveList.h` | `IntrusiveList<T>` | 侵入式双向链表 |
| `IntrusiveList.h` | `IntrusiveListNode<T>` | 侵入式链表节点 |
| `InlineString.h` | `InlineString` | 带 SSO 的拥有式字符串 |
| `Hash.h` | `Hash<T>` | 哈希函数模板 |
| `Hash.h` | `hash_combine()` | 哈希组合工具 |

---

## 使用方式

### 方法 1：包含单个头文件

```cpp
// 仅使用 Vector
#include <mine/containers/Vector.h>

void example() {
    mine::containers::Vector<int> vec;
    vec.push_back(42);
}
```

**优点**：
- 编译速度快（仅包含所需类型）
- 依赖关系清晰

**适用场景**：
- 头文件中仅需单个容器类型
- 追求最快编译速度

---

### 方法 2：包含伞头文件

```cpp
// 使用多个容器类型
#include <mine/containers/Containers.h>

void example() {
    mine::containers::Vector<int> vec;
    mine::containers::HashMap<int, std::string> map;
    mine::containers::InlineString str{"hello"};
}
```

**优点**：
- 便捷（无需逐个包含）
- 适合实现文件（`.cpp`）

**适用场景**：
- 实现文件中使用多个容器类型
- 快速原型开发

---

## 命名空间

所有类型位于 `mine::containers` 命名空间:

```cpp
#include <mine/containers/Containers.h>

// 方式 1：完全限定名
mine::containers::Vector<int> vec1;

// 方式 2：using 声明
using mine::containers::Vector;
Vector<int> vec2;

// 方式 3：using namespace（不推荐在头文件中使用）
using namespace mine::containers;
Vector<int> vec3;
```

---

## 编译影响

包含 `Containers.h` 的编译时间对比（相对单个头文件）:

| 包含文件 | 相对编译时间 | 说明 |
|---------|-------------|------|
| `Vector.h` | 1.0x | 基线 |
| `Vector.h + HashMap.h` | 1.8x | 手动包含两个 |
| `Containers.h` | 2.5x | 包含所有容器 |

**建议**：
- **头文件**：仅包含所需的单个头文件
- **实现文件**：可包含 `Containers.h` 提高便捷性

---

## 依赖关系

```
Containers.h
├── Vector.h
│   └── mine/core/Allocator.h
│   └── mine/core/Span.h
├── SmallVector.h
│   └── mine/core/Allocator.h
├── HashMap.h
│   └── Hash.h
│   └── mine/core/Allocator.h
├── IntrusiveList.h
│   └── mine/core/Assert.h
├── InlineString.h
│   └── mine/core/StringView.h
│   └── mine/core/Allocator.h
└── Hash.h
    └── mine/core/StringView.h
```

---

## 完整示例

### 示例：综合使用多个容器

```cpp
#include <mine/containers/Containers.h>

using namespace mine::containers;

class DataStore {
    // 动态数组
    Vector<int> ids_;
    
    // 哈希表
    HashMap<int, InlineString> id_to_name_;
    
    // 侵入式链表
    struct Task : IntrusiveListNode<Task> {
        int priority;
        InlineString description;
    };
    IntrusiveList<Task> pending_tasks_;
    
    // 小向量
    SmallVector<Task*, 8> active_tasks_;
    
public:
    void add_item(int id, StringView name) {
        ids_.push_back(id);
        id_to_name_.insert(id, InlineString{name});
    }
    
    void enqueue_task(Task& task) {
        pending_tasks_.push_back(task);
    }
    
    StringView get_name(int id) const {
        if (auto* name = id_to_name_.try_get(id)) {
            return *name;
        }
        return "Unknown";
    }
};
```

---

## 最佳实践

### 1. 头文件中避免包含 Containers.h

```cpp
// ❌ 不推荐：头文件中包含伞头文件
// Widget.h
#include <mine/containers/Containers.h>

class Widget {
    mine::containers::Vector<int> data_;  // 仅需 Vector.h
};

// ✅ 推荐：仅包含所需头文件
// Widget.h
#include <mine/containers/Vector.h>

class Widget {
    mine::containers::Vector<int> data_;
};
```

---

### 2. 实现文件中使用 Containers.h

```cpp
// ✅ 推荐：实现文件中使用伞头文件
// Widget.cpp
#include "Widget.h"
#include <mine/containers/Containers.h>

using namespace mine::containers;

void Widget::process() {
    Vector<int> temp;
    HashMap<int, std::string> cache;
    // ...
}
```

---

### 3. 前向声明

某些场景下可通过前向声明避免包含头文件:

```cpp
// ✅ 推荐：前向声明减少依赖
// Forward.h
namespace mine::containers {
    template<typename T> class Vector;
    template<typename K, typename V> class HashMap;
}

// Widget.h
#include "Forward.h"

class Widget {
    // 仅指针/引用,无需完整定义
    mine::containers::Vector<int>* data_;
};

// Widget.cpp
#include "Widget.h"
#include <mine/containers/Vector.h>  // 需要完整定义
```

---

## 总结

`Containers.h` 是 `mine.containers` 模块的伞头文件,提供以下便利:

1. **一次性包含**：无需逐个包含容器头文件
2. **完整功能**：包含模块所有公共类型
3. **便捷开发**：适合实现文件和快速原型

**使用建议**：
- 头文件中仅包含所需的单个头文件
- 实现文件中可包含 `Containers.h` 提高便捷性
- 注意编译时间影响,按需选择
