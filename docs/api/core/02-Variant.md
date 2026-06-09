# Variant 类详细接口文档

## 概述

**模块**：`mine.core`  
**头文件**：`<mine/core/Variant.h>`  
**命名空间**：`mine::core`

**用途**：无 RTTI 的类型擦除值容器，类似 `std::any`，但使用 `TypeId` 进行类型鉴别，支持小对象优化（SBO）。

**核心特性**：
- 不依赖 RTTI（使用 `TypeId` 代替 `std::type_info`）
- 小对象优化（≤ 16 字节的 noexcept 移动类型内联存储）
- 仅支持可拷贝类型（确保 Variant 自身可拷贝）
- 不使用 C++ 异常（类型不匹配时断言终止）
- 属性系统的核心值容器

---

## 类定义

```cpp
namespace mine::core {

class Variant {
public:
    // 构造
    constexpr Variant() noexcept = default;
    
    template<typename T>
    /*implicit*/ Variant(T&& value) noexcept;
    
    // 五法则
    ~Variant() noexcept;
    Variant(const Variant& other) noexcept;
    Variant(Variant&& other) noexcept;
    Variant& operator=(const Variant& other) noexcept;
    Variant& operator=(Variant&& other) noexcept;
    
    // 值赋值
    template<typename T>
    Variant& operator=(T&& value) noexcept;
    
    // 状态查询
    [[nodiscard]] bool has_value() const noexcept;
    [[nodiscard]] explicit operator bool() const noexcept;
    [[nodiscard]] TypeId type_id() const noexcept;
    
    template<typename T>
    [[nodiscard]] bool has() const noexcept;
    
    // 值访问
    template<typename T>
    [[nodiscard]] T& get() & noexcept;
    
    template<typename T>
    [[nodiscard]] const T& get() const& noexcept;
    
    // 就地构造
    template<typename T, typename... Args>
    T& emplace(Args&&... args) noexcept;
    
    // 重置与交换
    void reset() noexcept;
    void swap(Variant& other) noexcept;
};

// 自由函数
template<typename T> T& any_cast(Variant& v) noexcept;
template<typename T> const T& any_cast(const Variant& v) noexcept;
template<typename T> T* any_cast(Variant* v) noexcept;
template<typename T> const T* any_cast(const Variant* v) noexcept;

void swap(Variant& a, Variant& b) noexcept;

} // namespace mine::core
```

---

## 小对象优化（SBO）

### 机制

**SBO 条件**（三个同时满足）：
1. `sizeof(T) <= 16` 字节
2. `alignof(T) <= alignof(max_align_t)` (通常为 16)
3. `std::is_nothrow_move_constructible_v<T>` 为 `true`

**满足 SBO 条件的类型**：
- 内联存储在 Variant 的 16 字节缓冲区内
- 无堆分配，构造/析构/拷贝性能更高

**不满足 SBO 条件的类型**：
- 在堆上分配内存
- Variant 内部存储 `T*` 指针（8 字节）

### 常见类型的 SBO 状态

| 类型 | sizeof | SBO | 备注 |
|------|--------|-----|------|
| `int` | 4 | ✅ | 基本类型 |
| `double` | 8 | ✅ | 基本类型 |
| `void*` | 8 | ✅ | 指针 |
| `Vec2f` | 8 | ✅ | 2 个 float |
| `Vec4f` | 16 | ✅ | 刚好 16 字节 |
| `StringView` | 16 | ✅ | 指针+长度 |
| `InlineString<23>` | 32 | ❌ | 超过 16 字节 → 堆分配 |
| `std::string` | 32 | ❌ | 通常 32 字节（SSO） |

---

## 构造函数

### 默认构造

**签名**：
```cpp
constexpr Variant() noexcept = default;
```

**描述**：构造空 Variant（不持有任何值）。

**后置条件**：
- `has_value()` 返回 `false`
- `type_id()` 返回无效 TypeId
- `operator bool()` 返回 `false`

**示例**：
```cpp
Variant v;
assert(!v.has_value());
assert(!v);
```

---

### 值构造

**签名**：
```cpp
template<typename T>
    requires (!std::is_same_v<std::decay_t<T>, Variant> &&
              std::is_copy_constructible_v<std::decay_t<T>> &&
              !std::is_reference_v<std::decay_t<T>>)
/*implicit*/ Variant(T&& value) noexcept;
```

**描述**：从任意可拷贝类型的值构造 Variant（隐式转换）。

**模板参数**：
- `T`：值类型（自动 decay，去除 cv 限定符与引用）

**约束**：
- `T` 必须可拷贝构造（`std::is_copy_constructible_v<T>` 为 `true`）
- `T` 不可为引用类型
- `T` 不可为 `Variant` 本身（避免与拷贝构造冲突）

**行为**：
- 若 `T` 满足 SBO 条件：在内部缓冲区就地构造
- 若 `T` 不满足 SBO 条件：在堆上分配内存，缓冲区存储指针

**特殊处理**：
- `const char*` → 转换为 `StringView`（不拥有字符串）
- `StringView` → 直接存储（调用方须保证底层字符串生命周期）

**示例**：
```cpp
Variant v1 = 42;              // int（SBO）
Variant v2 = 3.14f;           // float（SBO）
Variant v3 = Vec2f{1.0f, 2.0f}; // 小结构体（SBO）
Variant v4 = StringView{"hello"}; // 字符串视图（SBO）

// 大对象 → 堆分配
InlineString<32> big_str{"long string..."};
Variant v5 = big_str;  // sizeof(InlineString<32>) > 16 → 堆分配
```

---

### 拷贝构造

**签名**：
```cpp
Variant(const Variant& other) noexcept;
```

**描述**：拷贝构造另一个 Variant。

**行为**：
- 若 `other` 为空：构造空 Variant
- 若 `other` 持有 SBO 类型：调用类型的拷贝构造（内联缓冲区）
- 若 `other` 持有堆类型：分配新堆内存并拷贝对象

**异常安全**：`noexcept`（分配失败时断言终止）

**时间复杂度**：
- SBO 类型：O(1)
- 堆类型：O(sizeof(T)) + 分配器开销

**示例**：
```cpp
Variant v1 = 42;
Variant v2 = v1;  // 拷贝 int 值
assert(v2.get<int>() == 42);
```

---

### 移动构造

**签名**：
```cpp
Variant(Variant&& other) noexcept;
```

**描述**：移动构造另一个 Variant。

**行为**：
- 若 `other` 为空：构造空 Variant
- 若 `other` 持有 SBO 类型：调用类型的移动构造（内联缓冲区）
- 若 `other` 持有堆类型：转移指针所有权（无拷贝，无分配）

**后置条件**：
- `other` 变为空状态（`has_value()` 返回 `false`）
- 原对象的资源转移到新对象

**时间复杂度**：O(1)

**示例**：
```cpp
Variant v1 = InlineString<32>{"long string"};
Variant v2 = std::move(v1);
assert(!v1.has_value());  // v1 已清空
assert(v2.has<InlineString<32>>());
```

---

## 赋值运算符

### 拷贝赋值

**签名**：
```cpp
Variant& operator=(const Variant& other) noexcept;
```

**描述**：从另一个 Variant 拷贝赋值。

**行为**：
1. 若 `this != &other`，先析构当前持有的值（调用 `reset()`）
2. 拷贝构造 `other` 持有的值

**自赋值安全**：是（显式检查 `this != &other`）

**异常安全**：`noexcept`

**示例**：
```cpp
Variant v1 = 42;
Variant v2 = 3.14f;
v2 = v1;  // v2 现在持有 int 值 42
assert(v2.get<int>() == 42);
```

---

### 移动赋值

**签名**：
```cpp
Variant& operator=(Variant&& other) noexcept;
```

**描述**：从另一个 Variant 移动赋值。

**行为**：
1. 若 `this != &other`，先析构当前持有的值
2. 移动构造 `other` 持有的值
3. `other` 变为空状态

**示例**：
```cpp
Variant v1 = 42;
Variant v2 = 3.14f;
v2 = std::move(v1);  // v2 现在持有 int 值 42
assert(!v1.has_value());
```

---

### 值赋值

**签名**：
```cpp
template<typename T>
    requires (!std::is_same_v<std::decay_t<T>, Variant> &&
              std::is_copy_constructible_v<std::decay_t<T>> &&
              !std::is_reference_v<std::decay_t<T>>)
Variant& operator=(T&& value) noexcept;
```

**描述**：从任意可拷贝类型的值赋值。

**行为**：
1. 析构当前持有的值
2. 就地构造新类型 `T` 的值

**约束**：与值构造相同

**示例**：
```cpp
Variant v = 42;
v = 3.14f;  // 现在持有 float 值
assert(v.has<float>());
assert(v.get<float>() == 3.14f);
```

---

## 析构函数

**签名**：
```cpp
~Variant() noexcept;
```

**描述**：析构 Variant 持有的值。

**行为**：
- 调用 `reset()`（见下文）

---

## 状态查询方法

### `has_value()`

**签名**：
```cpp
[[nodiscard]] bool has_value() const noexcept;
```

**描述**：检查 Variant 是否持有值。

**返回值**：
- `true`：持有值
- `false`：空 Variant

**时间复杂度**：O(1)

**示例**：
```cpp
Variant v;
assert(!v.has_value());

v = 42;
assert(v.has_value());

v.reset();
assert(!v.has_value());
```

---

### `operator bool()`

**签名**：
```cpp
[[nodiscard]] explicit operator bool() const noexcept;
```

**描述**：等价于 `has_value()`，用于条件判断。

**返回值**：同 `has_value()`

**示例**：
```cpp
Variant v = 42;
if (v) {  // 等价于 if (v.has_value())
    use(v.get<int>());
}
```

---

### `type_id()`

**签名**：
```cpp
[[nodiscard]] TypeId type_id() const noexcept;
```

**描述**：返回持有值的类型标识符。

**返回值**：
- 若持有值：返回类型 `T` 的 `TypeId::of<T>()`
- 若为空：返回无效 TypeId（`valid()` 为 `false`）

**时间复杂度**：O(1)

**示例**：
```cpp
Variant v = 42;
assert(v.type_id() == TypeId::of<int>());

Variant empty;
assert(!empty.type_id().valid());
```

---

### `has<T>()`

**签名**：
```cpp
template<typename T>
[[nodiscard]] bool has() const noexcept;
```

**描述**：检查 Variant 是否持有类型 `T` 的值。

**模板参数**：
- `T`：待检查的类型（cv 限定符与引用会被自动剥除）

**返回值**：
- `true`：持有类型 `T` 的值
- `false`：为空或持有其他类型

**时间复杂度**：O(1)（TypeId 指针比较）

**示例**：
```cpp
Variant v = 42;
assert(v.has<int>());
assert(!v.has<float>());

// cv 限定符不影响
assert(v.has<const int>());
```

---

## 值访问方法

### `get<T>()` (左值版本)

**签名**：
```cpp
template<typename T>
[[nodiscard]] T& get() & noexcept;

template<typename T>
[[nodiscard]] const T& get() const& noexcept;
```

**描述**：获取持有值的引用（类型不匹配时断言终止）。

**模板参数**：
- `T`：期望的值类型

**返回值**：
- 返回对 Variant 内部存储的直接引用（无拷贝）

**前置条件**：
- `has<T>()` 必须为 `true`（否则触发 `MINE_ASSERT`）

**时间复杂度**：O(1)

**示例**：
```cpp
Variant v = 42;
int& x = v.get<int>();  // OK：类型匹配
x = 99;
assert(v.get<int>() == 99);

// float& y = v.get<float>();  // 断言失败：类型不匹配
```

---

## 就地构造方法

### `emplace<T>()`

**签名**：
```cpp
template<typename T, typename... Args>
    requires std::is_copy_constructible_v<T> && std::is_constructible_v<T, Args...>
T& emplace(Args&&... args) noexcept;
```

**描述**：就地构造类型 `T` 的值（先析构当前值，再构造新值）。

**模板参数**：
- `T`：要构造的类型
- `Args...`：构造参数类型列表

**参数**：
- `args...`：转发给 `T` 构造函数的参数

**返回值**：
- 返回新构造对象的引用

**约束**：
- `T` 必须可拷贝构造
- `T` 必须可从 `Args...` 构造

**行为**：
1. 调用 `reset()` 析构当前值
2. 就地构造 `T(std::forward<Args>(args)...)`

**时间复杂度**：
- SBO 类型：O(1)
- 堆类型：O(1) + 分配器开销

**示例**：
```cpp
Variant v;
int& x = v.emplace<int>(42);  // 就地构造 int
assert(x == 42);

v.emplace<float>(3.14f);  // 析构 int，构造 float
assert(v.has<float>());
```

---

## 修改方法

### `reset()`

**签名**：
```cpp
void reset() noexcept;
```

**描述**：析构当前持有的值，Variant 变为空状态。

**行为**：
- 若持有 SBO 类型：调用对象析构函数
- 若持有堆类型：调用对象析构函数 + 释放堆内存
- 若为空：无操作

**后置条件**：
- `has_value()` 返回 `false`

**时间复杂度**：O(1)

**示例**：
```cpp
Variant v = 42;
assert(v.has_value());

v.reset();
assert(!v.has_value());
```

---

### `swap()`

**签名**：
```cpp
void swap(Variant& other) noexcept;
```

**描述**：交换两个 Variant 的内容。

**参数**：
- `other`：待交换的另一个 Variant

**时间复杂度**：O(1)（通过三次移动操作实现）

**示例**：
```cpp
Variant v1 = 42;
Variant v2 = 3.14f;

v1.swap(v2);
assert(v1.has<float>());
assert(v2.has<int>());
```

---

## 自由函数

### `any_cast<T>()` (引用版本)

**签名**：
```cpp
template<typename T>
[[nodiscard]] T& any_cast(Variant& v) noexcept;

template<typename T>
[[nodiscard]] const T& any_cast(const Variant& v) noexcept;
```

**描述**：从 Variant 中提取类型 `T` 的值引用（类型不匹配时断言终止）。

**参数**：
- `v`：Variant 引用

**返回值**：
- 返回对 Variant 内部存储的直接引用

**前置条件**：
- `v.has<T>()` 必须为 `true`（否则触发断言）

**等价于**：`v.get<T>()`

**示例**：
```cpp
Variant v = 42;
int& x = any_cast<int>(v);  // OK
x = 99;
assert(any_cast<int>(v) == 99);

// float& y = any_cast<float>(v);  // 断言失败
```

---

### `any_cast<T>()` (指针版本)

**签名**：
```cpp
template<typename T>
[[nodiscard]] T* any_cast(Variant* v) noexcept;

template<typename T>
[[nodiscard]] const T* any_cast(const Variant* v) noexcept;
```

**描述**：从 Variant 中提取类型 `T` 的值指针（类型不匹配时返回 `nullptr`，不断言）。

**参数**：
- `v`：Variant 指针（可为 `nullptr`）

**返回值**：
- 类型匹配：返回指向内部存储的指针
- 类型不匹配或 `v == nullptr`：返回 `nullptr`

**无异常版本**：不触发断言，适合错误处理路径

**示例**：
```cpp
Variant v = 42;

int* p1 = any_cast<int>(&v);
assert(p1 != nullptr);
assert(*p1 == 42);

float* p2 = any_cast<float>(&v);
assert(p2 == nullptr);  // 类型不匹配，返回 nullptr
```

---

### `swap()` (自由函数)

**签名**：
```cpp
void swap(Variant& a, Variant& b) noexcept;
```

**描述**：交换两个 Variant 的内容。

**等价于**：`a.swap(b)`

**示例**：
```cpp
using mine::core::swap;
Variant v1 = 42;
Variant v2 = 3.14f;
swap(v1, v2);
```

---

## 使用场景

### 1. 依赖属性值存储

```cpp
class DependencyObject {
private:
    HashMap<DependencyProperty*, Variant> local_values_;
    
public:
    Variant get_value(DependencyProperty* dp) const {
        auto it = local_values_.find(dp);
        if (it != local_values_.end()) {
            return it->second;  // 拷贝 Variant
        }
        return dp->default_value();
    }
    
    void set_value(DependencyProperty* dp, Variant value) {
        local_values_[dp] = std::move(value);
    }
};
```

### 2. 配置键值对

```cpp
class Config {
private:
    HashMap<StringView, Variant> settings_;
    
public:
    template<typename T>
    T get(StringView key, T default_value) const {
        auto it = settings_.find(key);
        if (it == settings_.end()) return default_value;
        
        if (it->second.has<T>()) {
            return it->second.get<T>();
        }
        return default_value;  // 类型不匹配，返回默认值
    }
    
    template<typename T>
    void set(StringView key, T value) {
        settings_[key] = std::move(value);
    }
};
```

### 3. 属性动画值插值

```cpp
Variant lerp_value(const Variant& from, const Variant& to, float t) {
    if (from.type_id() != to.type_id()) {
        return t < 0.5f ? from : to;  // 类型不同，阶跃
    }
    
    if (from.has<float>()) {
        float a = from.get<float>();
        float b = to.get<float>();
        return Variant{a + (b - a) * t};
    }
    
    if (from.has<Vec2f>()) {
        Vec2f a = from.get<Vec2f>();
        Vec2f b = to.get<Vec2f>();
        return Variant{lerp(a, b, t)};
    }
    
    // 其他类型不支持插值
    return t < 0.5f ? from : to;
}
```

---

## 性能特征

| 操作 | SBO 类型 | 堆类型 | 备注 |
|------|----------|--------|------|
| 默认构造 | O(1) | O(1) | 仅初始化指针 |
| 值构造 | O(1) | O(alloc) | SBO 无分配 |
| 拷贝构造 | O(1) | O(alloc) | 堆类型需分配新内存 |
| 移动构造 | O(1) | O(1) | 堆类型仅转移指针 |
| 析构 | O(1) | O(1) | 堆类型需释放内存 |
| `get<T>()` | O(1) | O(1) | 直接返回引用 |
| `has<T>()` | O(1) | O(1) | TypeId 比较 |
| `emplace<T>()` | O(1) | O(alloc) | 先析构旧值 |

**内存占用**：
- Variant 对象：`32` 字节（16 字节缓冲区 + 8 字节 ops 指针 + 8 字节对齐填充）
- SBO 类型：无额外堆分配
- 堆类型：堆上额外 `sizeof(T)` 字节

---

## 线程安全

**非线程安全**：
- Variant 对象本身无内部锁
- 多线程同时读写同一 Variant 需外部同步

**线程安全操作**（多线程只读）：
- `has_value()` / `type_id()` / `has<T>()`：只读查询，无竞争
- `get<T>()` const 版本：只读访问，无竞争

**需要同步的操作**：
- 任何修改操作（赋值、`emplace()`、`reset()`）与其他操作并发执行

---

## 限制与注意事项

### 不支持引用类型

```cpp
int x = 42;
// Variant v = std::ref(x);  // 编译错误：引用类型不可存储
```

**原因**：
- Variant 需要可拷贝语义
- 引用不可重绑定，不符合值语义

**替代方案**：存储指针
```cpp
Variant v = &x;  // 存储 int* 指针
```

### 不支持不可拷贝类型

```cpp
// std::unique_ptr 不可拷贝
// Variant v = std::make_unique<int>(42);  // 编译错误
```

**原因**：
- Variant 自身需要可拷贝（用于属性值传递）
- 不可拷贝类型会破坏 Variant 的拷贝语义

**替代方案**：包装为可拷贝的智能指针
```cpp
Variant v = std::shared_ptr<int>{std::make_unique<int>(42)};
```

### StringView 生命周期

```cpp
Variant create_variant() {
    InlineString<32> temp{"temporary"};
    return Variant{temp.view()};  // ⚠️ 危险：返回悬空 StringView
}

Variant v = create_variant();
// v.get<StringView>() 指向已销毁的 temp → 未定义行为
```

**解决方案**：存储拥有型字符串
```cpp
Variant create_variant() {
    return Variant{InlineString<32>{"temporary"}};  // ✅ 拷贝完整字符串
}
```

---

## 调试技巧

### 检查类型名

```cpp
#include <mine/reflect/TypeRegistry.h>

Variant v = 42;
const char* type_name = reflect::TypeRegistry::name(v.type_id());
printf("Variant holds: %s\n", type_name);  // "int"
```

### 打印 Variant 内容

```cpp
void debug_print(const Variant& v) {
    if (!v) {
        printf("Variant: empty\n");
        return;
    }
    
    if (v.has<int>()) {
        printf("Variant: int = %d\n", v.get<int>());
    } else if (v.has<float>()) {
        printf("Variant: float = %f\n", v.get<float>());
    } else if (v.has<StringView>()) {
        auto sv = v.get<StringView>();
        printf("Variant: StringView = \"%.*s\"\n", (int)sv.size(), sv.data());
    } else {
        printf("Variant: unknown type (hash = 0x%zx)\n", v.type_id().hash());
    }
}
```

### 断点条件

```cpp
// Visual Studio / GDB 断点条件
v.type_id().hash() == 0x7ff6abc12340  // 特定类型
v.has_value() && v.has<Button*>()     // Button 指针
```

---

## 示例：完整用法

```cpp
#include <mine/core/Variant.h>
#include <mine/core/StringView.h>
#include <mine/containers/InlineString.h>
#include <cassert>

using namespace mine;

void example_basic() {
    // 构造与赋值
    core::Variant v1 = 42;
    core::Variant v2 = 3.14f;
    core::Variant v3 = core::StringView{"hello"};
    
    // 类型查询
    assert(v1.has<int>());
    assert(v2.has<float>());
    assert(v3.has<core::StringView>());
    
    // 值访问
    assert(v1.get<int>() == 42);
    assert(v2.get<float>() == 3.14f);
    
    // 修改
    v1 = 99;
    assert(v1.get<int>() == 99);
    
    // 重置
    v1.reset();
    assert(!v1.has_value());
}

void example_any_cast() {
    core::Variant v = 42;
    
    // 引用版本（类型不匹配断言终止）
    int& x = core::any_cast<int>(v);
    x = 99;
    
    // 指针版本（类型不匹配返回 nullptr）
    int* p1 = core::any_cast<int>(&v);
    assert(p1 != nullptr);
    
    float* p2 = core::any_cast<float>(&v);
    assert(p2 == nullptr);  // 类型不匹配
}

void example_container() {
    // 作为容器元素
    std::vector<core::Variant> values;
    values.push_back(42);
    values.push_back(3.14f);
    values.push_back(core::StringView{"text"});
    
    // 遍历
    for (const auto& v : values) {
        if (v.has<int>()) {
            printf("int: %d\n", v.get<int>());
        } else if (v.has<float>()) {
            printf("float: %f\n", v.get<float>());
        } else if (v.has<core::StringView>()) {
            auto sv = v.get<core::StringView>();
            printf("string: %.*s\n", (int)sv.size(), sv.data());
        }
    }
}

int main() {
    example_basic();
    example_any_cast();
    example_container();
    return 0;
}
```

---

## 相关类型

- `mine::core::TypeId`：类型标识符，Variant 的类型判断基础
- `mine::core::StringView`：Variant 的常见存储类型
- `mine::containers::InlineString`：拥有型字符串，适合存储于 Variant
- `mine::ui::DependencyProperty`：使用 Variant 存储属性默认值

---

## 最佳实践

### ✅ 推荐用法

```cpp
// 1. 存储小对象（利用 SBO）
Variant v1 = 42;
Variant v2 = Vec2f{1.0f, 2.0f};

// 2. 使用指针版本 any_cast 进行安全查询
if (int* p = any_cast<int>(&v)) {
    use(*p);
}

// 3. 存储拥有型字符串
Variant v = InlineString<32>{"owned string"};
```

### ❌ 避免的用法

```cpp
// ❌ 存储悬空 StringView
Variant create_variant() {
    std::string temp = "temp";
    return Variant{StringView{temp}};  // temp 销毁后悬空
}

// ❌ 存储引用类型
int x = 42;
// Variant v = std::ref(x);  // 编译错误

// ❌ 存储不可拷贝类型
// Variant v = std::unique_ptr<int>{new int{42}};  // 编译错误
```
