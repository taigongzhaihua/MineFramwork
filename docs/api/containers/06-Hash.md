# Hash 详细接口文档

## 概述

`Hash.h` 提供了用于 `HashMap` 的哈希函数模板及哈希组合工具。它为常见类型提供了高质量的哈希实现,并支持用户自定义类型的哈希特化。

### 核心特性

- **内置类型哈希**：整型、浮点型、指针、枚举等内置类型的哈希实现
- **字符串哈希**：`StringView`、C 字符串的 FNV-1a 哈希
- **哈希组合**：`hash_combine` 工具用于组合多个值的哈希
- **自定义类型支持**：通过特化 `Hash<T>` 为自定义类型提供哈希

---

## 类定义

### Hash<T> 模板

```cpp
template<typename T>
struct Hash {
    size_t operator()(const T& value) const noexcept;
};
```

**描述**：哈希函数对象模板,用于 `HashMap<K, V, Hash<K>>`。

**要求**：
- `operator()` 必须对相同值返回相同哈希
- 哈希冲突应尽可能少
- 必须是 `noexcept`

---

## 内置类型特化

### 整型哈希

```cpp
template<> struct Hash<int8_t>;
template<> struct Hash<uint8_t>;
template<> struct Hash<int16_t>;
template<> struct Hash<uint16_t>;
template<> struct Hash<int32_t>;
template<> struct Hash<uint32_t>;
template<> struct Hash<int64_t>;
template<> struct Hash<uint64_t>;
```

**描述**：整型的身份哈希（直接返回整数值或混淆后的值）。

**实现**：
- 小整型（≤32 位）：直接返回值
- 64 位整型：使用 **MurmurHash64** 混淆

**示例**：

```cpp
Hash<int> hasher;
size_t h1 = hasher(42);  // 直接返回 42
size_t h2 = hasher(42);  // h1 == h2
```

---

### 浮点型哈希

```cpp
template<> struct Hash<float>;
template<> struct Hash<double>;
```

**描述**：浮点数的位级哈希。

**实现**：
- 将浮点数的位模式重新解释为整数
- 特殊处理：`+0.0` 和 `-0.0` 哈希相同,`NaN` 哈希为 0

**示例**：

```cpp
Hash<float> hasher;
size_t h1 = hasher(3.14f);
size_t h2 = hasher(3.14f);
assert(h1 == h2);
```

---

### 指针哈希

```cpp
template<typename T>
struct Hash<T*>;
```

**描述**：指针地址的哈希。

**实现**：将指针值转换为 `uintptr_t` 并混淆

**示例**：

```cpp
int x = 42;
Hash<int*> hasher;
size_t h = hasher(&x);
```

---

### 枚举哈希

```cpp
template<typename E>
    requires std::is_enum_v<E>
struct Hash<E>;
```

**描述**：枚举类型的哈希（转换为底层整型）。

**实现**：
```cpp
template<typename E>
struct Hash<E> {
    size_t operator()(E e) const noexcept {
        using U = std::underlying_type_t<E>;
        return Hash<U>{}(static_cast<U>(e));
    }
};
```

**示例**：

```cpp
enum class Color { Red, Green, Blue };

Hash<Color> hasher;
size_t h = hasher(Color::Red);
```

---

## 字符串哈希

### StringView 哈希

```cpp
template<>
struct Hash<mine::core::StringView> {
    size_t operator()(mine::core::StringView sv) const noexcept;
};
```

**描述**：UTF-8 字符串视图的哈希,使用 **FNV-1a** 算法。

**FNV-1a 算法**：
```cpp
size_t fnv1a(const char* data, size_t len) {
    size_t hash = 14695981039346656037ULL;  // FNV offset basis
    for (size_t i = 0; i < len; ++i) {
        hash ^= static_cast<uint8_t>(data[i]);
        hash *= 1099511628211ULL;  // FNV prime
    }
    return hash;
}
```

**特性**：
- 雪崩效应好：单字符差异导致完全不同的哈希
- 速度快：每字节仅需 1 次乘法和 1 次异或

**示例**：

```cpp
Hash<StringView> hasher;
size_t h1 = hasher(StringView{"hello"});
size_t h2 = hasher(StringView{"world"});
assert(h1 != h2);
```

---

### C 字符串哈希

```cpp
template<>
struct Hash<const char*> {
    size_t operator()(const char* str) const noexcept;
};
```

**描述**：C 字符串的哈希,内部转换为 `StringView` 并调用 FNV-1a。

**示例**：

```cpp
Hash<const char*> hasher;
size_t h = hasher("hello");
```

---

## 哈希组合

### hash_combine

```cpp
template<typename T>
inline void hash_combine(size_t& seed, const T& value) noexcept;
```

**描述**：将新值的哈希合并到现有种子中。

**参数**：
- `seed`：现有哈希种子（输入/输出）
- `value`：要组合的值

**算法**：
```cpp
seed ^= Hash<T>{}(value) + 0x9e3779b9 + (seed << 6) + (seed >> 2);
```

**用途**：为复合类型（如 `std::pair`、自定义结构体）实现哈希

**示例**：

```cpp
struct Point {
    int x, y;
};

template<>
struct Hash<Point> {
    size_t operator()(const Point& p) const noexcept {
        size_t h = 0;
        hash_combine(h, p.x);
        hash_combine(h, p.y);
        return h;
    }
};

HashMap<Point, std::string> map;
map.insert(Point{10, 20}, "origin");
```

---

## 自定义类型哈希

### 方法 1：特化 Hash<T>

```cpp
struct Widget {
    int id;
    std::string name;
};

template<>
struct mine::containers::Hash<Widget> {
    size_t operator()(const Widget& w) const noexcept {
        size_t h = 0;
        hash_combine(h, w.id);
        hash_combine(h, StringView{w.name});
        return h;
    }
};

HashMap<Widget, int> map;
```

---

### 方法 2：提供成员函数 hash()

某些场景下可通过成员函数实现:

```cpp
struct Gadget {
    int x, y;
    
    size_t hash() const noexcept {
        size_t h = 0;
        hash_combine(h, x);
        hash_combine(h, y);
        return h;
    }
};

// 特化 Hash<Gadget> 调用成员函数
template<>
struct mine::containers::Hash<Gadget> {
    size_t operator()(const Gadget& g) const noexcept {
        return g.hash();
    }
};
```

---

## 哈希质量

### 雪崩效应

好的哈希函数应具有 **雪崩效应**：输入的微小变化导致输出的巨大变化。

**测试**：

```cpp
Hash<StringView> hasher;
size_t h1 = hasher(StringView{"hello"});
size_t h2 = hasher(StringView{"hallo"});  // 仅1字符不同
// h1 和 h2 应完全不同（约 50% 位翻转）
```

### 冲突率

对于随机输入,哈希冲突率应接近理论下界:

| 元素数量 (n) | 桶数 (m) | 理论冲突率 |
|-------------|---------|-----------|
| 1000 | 1024 | ~48.6% |
| 10000 | 16384 | ~52.3% |

---

## 使用场景

### 1. HashMap 键类型

```cpp
// 内置类型：无需特化
HashMap<int, std::string> map1;

// StringView：已提供特化
HashMap<StringView, int> map2;

// 自定义类型：需要特化
struct MyKey { ... };
template<> struct Hash<MyKey> { ... };
HashMap<MyKey, int> map3;
```

---

### 2. 组合键

```cpp
using CompositeKey = std::pair<int, StringView>;

template<>
struct mine::containers::Hash<CompositeKey> {
    size_t operator()(const CompositeKey& key) const noexcept {
        size_t h = 0;
        hash_combine(h, key.first);
        hash_combine(h, key.second);
        return h;
    }
};

HashMap<CompositeKey, Variant> config;
config.insert({42, "width"}, 1920);
```

---

### 3. 指针作为键

```cpp
class Widget;

HashMap<Widget*, int> widget_ids;  // 使用指针地址作为哈希

Widget* w = new Widget;
widget_ids.insert(w, 1);
```

---

## 最佳实践

### 1. 使用 hash_combine 组合哈希

```cpp
// ✅ 推荐：使用 hash_combine
struct Point { int x, y; };

template<>
struct Hash<Point> {
    size_t operator()(const Point& p) const noexcept {
        size_t h = 0;
        hash_combine(h, p.x);
        hash_combine(h, p.y);
        return h;
    }
};

// ❌ 不推荐：简单异或（冲突率高）
template<>
struct Hash<Point> {
    size_t operator()(const Point& p) const noexcept {
        return Hash<int>{}(p.x) ^ Hash<int>{}(p.y);  // 冲突！(1,2) == (2,1)
    }
};
```

---

### 2. 确保哈希函数 noexcept

```cpp
// ✅ 推荐：noexcept
template<>
struct Hash<MyType> {
    size_t operator()(const MyType& t) const noexcept {
        return ...;
    }
};

// ❌ 不推荐：可能抛出异常
template<>
struct Hash<MyType> {
    size_t operator()(const MyType& t) const {  // 缺少 noexcept
        return ...;
    }
};
```

---

### 3. 浮点数键使用整数代替

```cpp
// ❌ 不推荐：浮点数作为键（精度问题）
HashMap<float, int> map1;
map1[3.14f] = 1;
float x = 3.14f;
auto* val = map1.try_get(x);  // 可能因精度找不到

// ✅ 推荐：量化为整数
HashMap<int, int> map2;
map2[int(3.14f * 1000)] = 1;  // 3140
```

---

## 完整示例

### 示例：自定义结构体哈希

```cpp
#include <mine/containers/HashMap.h>
#include <mine/containers/Hash.h>

using namespace mine::containers;

struct Employee {
    int id;
    StringView name;
    int department;
    
    bool operator==(const Employee& other) const noexcept {
        return id == other.id;  // 按 ID 比较
    }
};

// 特化 Hash
template<>
struct mine::containers::Hash<Employee> {
    size_t operator()(const Employee& e) const noexcept {
        return Hash<int>{}(e.id);  // 仅哈希 ID
    }
};

void example() {
    HashMap<Employee, int> salaries;
    
    Employee e1{1, "Alice", 10};
    Employee e2{2, "Bob", 20};
    
    salaries.insert(e1, 50000);
    salaries.insert(e2, 60000);
    
    if (int* salary = salaries.try_get(e1)) {
        std::cout << "Alice's salary: " << *salary;
    }
}
```

---

## 总结

`Hash.h` 提供了 `HashMap` 所需的哈希基础设施,具备以下优势:

1. **内置类型支持**：常见类型开箱即用
2. **字符串优化**：FNV-1a 算法快速且质量高
3. **组合工具**：`hash_combine` 简化复合类型哈希
4. **可扩展**：通过特化支持自定义类型

在使用 `HashMap` 时,对于自定义键类型,需要特化 `Hash<K>` 和提供 `operator==`。
