# Span<T> 详细接口文档

## 概述

`Span<T>` 是 MineFramework 中的非拥有连续内存视图类型，提供对数组、容器或原始内存块的轻量级只读或可变访问接口。它类似于 C++20 的 `std::span`，但不依赖标准库头文件，并在下标越界时提供断言检查。

### 核心特性

- **零开销抽象**：仅包含指针和长度两个成员（16 字节），可在寄存器中传递
- **非拥有语义**：不管理内存生命周期，调用方负责确保底层数据有效
- **类型安全**：通过模板参数区分可变视图 `Span<T>` 和只读视图 `Span<const T>`
- **统一接口**：为数组、容器、原始指针提供一致的迭代与访问 API
- **边界检查**：所有索引访问均通过 `MINE_ASSERT` 检查越界（Debug 模式）
- **子视图操作**：支持 `first()`、`last()`、`subspan()` 等零拷贝分割操作

### 设计动机

在 MineFramework 中，公共 API 不能直接使用 STL 容器（如 `std::vector<T>`）作为参数类型，因为：

1. **ABI 不稳定**：不同编译器/版本的 STL 实现不兼容，跨 DLL 边界传递会导致崩溃
2. **强制所有权**：`std::vector<T>` 蕴含所有权语义，限制了参数传递的灵活性
3. **编译依赖**：包含 `<vector>` 会引入大量模板代码，拖慢编译速度

`Span<T>` 解决了以上问题，作为公共 API 的参数类型，它：

- 接受任何连续内存源（数组、自有容器、临时缓冲区）
- 不涉及内存分配或拷贝
- 头文件轻量，仅依赖 `<cstddef>` 和 `<type_traits>`
- ABI 稳定（POD 布局，两个指针大小）

### 典型使用场景

| 场景 | 用法示例 |
|------|----------|
| 函数参数 | `void draw(Span<const Vertex> vertices)` |
| 容器包装 | `Vector<int> vec{1,2,3}; Span<int> s{vec};` |
| 数组切片 | `int arr[10]; Span<int> view = Span{arr}.subspan(2, 5);` |
| 字节序列化 | `Span<const uint8_t> bytes = span.as_bytes();` |
| 遍历子序列 | `for (auto& elem : span.first(5)) { ... }` |

---

## 类定义

```cpp
namespace mine::core {

template<typename T>
class Span {
public:
    using element_type    = T;
    using value_type      = std::remove_cv_t<T>;
    using size_type       = size_t;
    using difference_type = ptrdiff_t;
    using pointer         = T*;
    using const_pointer   = const T*;
    using reference       = T&;
    using iterator        = T*;
    using const_iterator  = const T*;

    // 构造
    constexpr Span() noexcept;
    constexpr Span(T* data, size_type count) noexcept;
    template<size_t N> constexpr Span(T (&arr)[N]) noexcept;
    template<typename Container> constexpr Span(Container& c) noexcept;
    template<typename Container> constexpr Span(const Container& c) noexcept;

    // 容量
    [[nodiscard]] constexpr T*        data()  const noexcept;
    [[nodiscard]] constexpr size_type size()  const noexcept;
    [[nodiscard]] constexpr bool      empty() const noexcept;
    [[nodiscard]] constexpr size_type size_bytes() const noexcept;

    // 元素访问
    [[nodiscard]] constexpr T& operator[](size_type i) const noexcept;
    [[nodiscard]] constexpr T& front() const noexcept;
    [[nodiscard]] constexpr T& back()  const noexcept;

    // 迭代器
    [[nodiscard]] constexpr iterator begin() const noexcept;
    [[nodiscard]] constexpr iterator end()   const noexcept;

    // 子视图
    [[nodiscard]] constexpr Span first(size_type count) const noexcept;
    [[nodiscard]] constexpr Span last(size_type count) const noexcept;
    [[nodiscard]] constexpr Span subspan(size_type offset,
                                          size_type count = -1) const noexcept;

    // 类型转换
    constexpr operator Span<const T>() const noexcept;
    [[nodiscard]] Span<const unsigned char> as_bytes() const noexcept;

private:
    T*        data_{nullptr};
    size_type size_{0};
};

} // namespace mine::core
```

### 类型别名

| 别名 | 定义 | 用途 |
|------|------|------|
| `element_type` | `T` | 元素类型（保留 const） |
| `value_type` | `std::remove_cv_t<T>` | 去除 cv 限定的值类型 |
| `size_type` | `size_t` | 索引与计数类型 |
| `difference_type` | `ptrdiff_t` | 迭代器距离类型 |
| `pointer` | `T*` | 指针类型 |
| `iterator` | `T*` | 迭代器类型（原始指针） |

---

## 构造函数

### 默认构造

```cpp
constexpr Span() noexcept = default;
```

**描述**：构造空视图，`data() == nullptr`，`size() == 0`。

**时间复杂度**：O(1)

**示例**：

```cpp
mine::core::Span<int> empty;
MINE_ASSERT(empty.data() == nullptr);
MINE_ASSERT(empty.size() == 0);
MINE_ASSERT(empty.empty());
```

---

### 从指针和大小构造

```cpp
constexpr Span(T* data, size_type count) noexcept;
```

**描述**：从指向连续内存块的指针和元素数构造视图，表示区间 `[data, data + count)`。

**参数**：
- `data`：指向首元素的指针（可为 `nullptr` 当且仅当 `count == 0`）
- `count`：元素个数

**前置条件**：
- 若 `count > 0`，则 `data` 必须非空且指向至少 `count` 个连续的 `T` 对象
- 若 `data == nullptr`，则 `count` 必须为 0

**断言**：Debug 模式下检查 `data != nullptr || count == 0`

**时间复杂度**：O(1)

**示例**：

```cpp
int buffer[10];
mine::core::Span<int> view{buffer, 10};
MINE_ASSERT(view.size() == 10);
MINE_ASSERT(view.data() == buffer);

// 空视图
mine::core::Span<int> empty{nullptr, 0};  // 合法
```

---

### 从 C 数组构造

```cpp
template<size_t N>
constexpr Span(T (&arr)[N]) noexcept;
```

**描述**：从 C 数组引用构造，自动推导元素个数 `N`。

**模板参数**：
- `N`：数组元素个数（编译期常量）

**参数**：
- `arr`：C 数组引用

**时间复杂度**：O(1)

**隐式转换**：允许隐式转换（标记 `// NOLINT(google-explicit-constructor)`），因为：
1. 数组到视图的转换是无损的
2. 不涉及内存分配
3. 符合直觉（数组本质上是连续内存）

**示例**：

```cpp
int arr[] = {1, 2, 3, 4, 5};
mine::core::Span<int> view{arr};  // 自动推导为 Span<int, 5>
MINE_ASSERT(view.size() == 5);
MINE_ASSERT(view[0] == 1);

// 只读视图
mine::core::Span<const int> readonly{arr};
```

---

### 从容器构造

```cpp
template<typename Container>
    requires requires(Container& c) {
        { c.data() } -> std::convertible_to<T*>;
        { c.size() } -> std::convertible_to<size_t>;
    }
constexpr Span(Container& c) noexcept;
```

**描述**：从具有 `.data()` 和 `.size()` 成员函数的容器构造视图。

**约束**（C++20 concepts）：
- 容器必须有 `data()` 方法返回 `T*` 类型
- 容器必须有 `size()` 方法返回可转换为 `size_t` 的类型

**支持的容器**：
- `mine::containers::Vector<T>`
- `mine::containers::SmallVector<T, N>`
- 任何符合约束的自定义容器

**隐式转换**：允许隐式转换，使得函数调用时可以直接传递容器：

```cpp
void process(mine::core::Span<int> data);

mine::containers::Vector<int> vec{1, 2, 3};
process(vec);  // 隐式转换为 Span<int>
```

**时间复杂度**：O(1)

**示例**：

```cpp
mine::containers::Vector<float> vertices{1.0f, 2.0f, 3.0f};
mine::core::Span<float> view{vertices};
MINE_ASSERT(view.size() == 3);
MINE_ASSERT(view.data() == vertices.data());

// 只读视图
mine::core::Span<const float> readonly{vertices};
```

---

### 从 const 容器构造

```cpp
template<typename Container>
    requires requires(const Container& c) {
        { c.data() } -> std::convertible_to<T*>;
        { c.size() } -> std::convertible_to<size_t>;
    }
constexpr Span(const Container& c) noexcept;
```

**描述**：从 `const` 容器构造只读视图（仅当 `T` 为 `const U` 时有效）。

**约束**：仅当 `T` 为 `const` 限定时启用此重载。

**示例**：

```cpp
const mine::containers::Vector<int> vec{1, 2, 3};
mine::core::Span<const int> view{vec};  // OK
// mine::core::Span<int> bad{vec};      // 编译错误：const 容器不能构造可变视图
```

---

## 容量查询方法

### data

```cpp
[[nodiscard]] constexpr T* data() const noexcept;
```

**描述**：返回指向首元素的指针。

**返回值**：
- 非空视图：指向首元素的有效指针
- 空视图：`nullptr`

**时间复杂度**：O(1)

**示例**：

```cpp
int arr[] = {1, 2, 3};
mine::core::Span<int> view{arr};
int* ptr = view.data();
MINE_ASSERT(ptr == arr);
```

---

### size

```cpp
[[nodiscard]] constexpr size_type size() const noexcept;
```

**描述**：返回元素个数。

**返回值**：元素数量（可能为 0）

**时间复杂度**：O(1)

**示例**：

```cpp
int arr[5];
mine::core::Span<int> view{arr};
MINE_ASSERT(view.size() == 5);
```

---

### empty

```cpp
[[nodiscard]] constexpr bool empty() const noexcept;
```

**描述**：检查视图是否为空。

**返回值**：
- `true`：`size() == 0`
- `false`：视图包含至少一个元素

**时间复杂度**：O(1)

**示例**：

```cpp
mine::core::Span<int> empty;
MINE_ASSERT(empty.empty());

int arr[1];
mine::core::Span<int> view{arr};
MINE_ASSERT(!view.empty());
```

---

### size_bytes

```cpp
[[nodiscard]] constexpr size_type size_bytes() const noexcept;
```

**描述**：返回视图表示的总字节数（`size() * sizeof(T)`）。

**返回值**：字节数

**时间复杂度**：O(1)

**用途**：用于序列化、I/O 操作、内存拷贝等场景。

**示例**：

```cpp
int arr[10];  // sizeof(int) == 4
mine::core::Span<int> view{arr};
MINE_ASSERT(view.size_bytes() == 40);  // 10 * 4
```

---

## 元素访问方法

### operator[]

```cpp
[[nodiscard]] constexpr T& operator[](size_type i) const noexcept;
```

**描述**：通过索引访问元素（零基）。

**参数**：
- `i`：元素索引，范围 `[0, size())`

**返回值**：第 `i` 个元素的引用

**前置条件**：`i < size()`

**断言**：Debug 模式下检查 `i < size()`，Release 模式下越界访问是未定义行为

**时间复杂度**：O(1)

**注意**：
- `Span` 不提供 `at()` 方法（无异常支持）
- 越界访问在 Debug 模式触发断言，Release 模式为未定义行为
- 调用方应确保索引有效，或使用迭代器接口

**示例**：

```cpp
int arr[] = {10, 20, 30};
mine::core::Span<int> view{arr};
MINE_ASSERT(view[0] == 10);
MINE_ASSERT(view[2] == 30);

view[1] = 99;
MINE_ASSERT(arr[1] == 99);  // 修改传播到底层数组

// view[3];  // Debug: 触发断言; Release: 未定义行为
```

---

### front

```cpp
[[nodiscard]] constexpr T& front() const noexcept;
```

**描述**：访问首元素（等价于 `(*this)[0]`）。

**返回值**：首元素引用

**前置条件**：`!empty()`

**断言**：Debug 模式下检查 `!empty()`

**时间复杂度**：O(1)

**示例**：

```cpp
int arr[] = {1, 2, 3};
mine::core::Span<int> view{arr};
MINE_ASSERT(view.front() == 1);

view.front() = 99;
MINE_ASSERT(arr[0] == 99);
```

---

### back

```cpp
[[nodiscard]] constexpr T& back() const noexcept;
```

**描述**：访问尾元素（等价于 `(*this)[size() - 1]`）。

**返回值**：尾元素引用

**前置条件**：`!empty()`

**断言**：Debug 模式下检查 `!empty()`

**时间复杂度**：O(1)

**示例**：

```cpp
int arr[] = {1, 2, 3};
mine::core::Span<int> view{arr};
MINE_ASSERT(view.back() == 3);

view.back() = 99;
MINE_ASSERT(arr[2] == 99);
```

---

## 迭代器方法

### begin / end

```cpp
[[nodiscard]] constexpr iterator begin() const noexcept;
[[nodiscard]] constexpr iterator end()   const noexcept;
```

**描述**：返回指向首元素和尾后位置的迭代器。

**返回值**：
- `begin()`：指向首元素的指针（`data()`）
- `end()`：指向尾后位置的指针（`data() + size()`）

**时间复杂度**：O(1)

**兼容性**：
- 支持 C++11 range-based for 循环
- 兼容标准算法（`std::find`、`std::copy` 等）

**示例**：

```cpp
int arr[] = {1, 2, 3, 4, 5};
mine::core::Span<int> view{arr};

// Range-based for
for (int& x : view) {
    x *= 2;
}

// 标准算法
auto it = std::find(view.begin(), view.end(), 6);
MINE_ASSERT(it != view.end());  // 找到 3*2 == 6
```

---

## 子视图方法

### first

```cpp
[[nodiscard]] constexpr Span first(size_type count) const noexcept;
```

**描述**：返回前 `count` 个元素的子视图。

**参数**：
- `count`：子视图长度

**返回值**：`Span{data(), count}`

**前置条件**：`count <= size()`

**断言**：Debug 模式下检查 `count <= size()`

**时间复杂度**：O(1)

**示例**：

```cpp
int arr[] = {1, 2, 3, 4, 5};
mine::core::Span<int> view{arr};

auto head = view.first(3);
MINE_ASSERT(head.size() == 3);
MINE_ASSERT(head[0] == 1);
MINE_ASSERT(head[2] == 3);

// 边界情况
auto empty_head = view.first(0);
MINE_ASSERT(empty_head.empty());

auto full = view.first(5);
MINE_ASSERT(full.size() == view.size());
```

---

### last

```cpp
[[nodiscard]] constexpr Span last(size_type count) const noexcept;
```

**描述**：返回后 `count` 个元素的子视图。

**参数**：
- `count`：子视图长度

**返回值**：`Span{data() + size() - count, count}`

**前置条件**：`count <= size()`

**断言**：Debug 模式下检查 `count <= size()`

**时间复杂度**：O(1)

**示例**：

```cpp
int arr[] = {1, 2, 3, 4, 5};
mine::core::Span<int> view{arr};

auto tail = view.last(2);
MINE_ASSERT(tail.size() == 2);
MINE_ASSERT(tail[0] == 4);
MINE_ASSERT(tail[1] == 5);

// 边界情况
auto empty_tail = view.last(0);
MINE_ASSERT(empty_tail.empty());

auto full = view.last(5);
MINE_ASSERT(full.size() == view.size());
```

---

### subspan

```cpp
[[nodiscard]] constexpr Span subspan(size_type offset,
                                      size_type count = static_cast<size_type>(-1)) const noexcept;
```

**描述**：返回从 `offset` 开始、长度为 `count` 的子视图。

**参数**：
- `offset`：起始索引
- `count`：子视图长度，默认 `-1` 表示"到末尾"

**返回值**：
- 若 `count == -1` 或 `count > size() - offset`：返回 `[offset, size())` 的子视图
- 否则：返回 `[offset, offset + count)` 的子视图

**前置条件**：`offset <= size()`

**断言**：Debug 模式下检查 `offset <= size()`

**时间复杂度**：O(1)

**示例**：

```cpp
int arr[] = {1, 2, 3, 4, 5, 6, 7, 8};
mine::core::Span<int> view{arr};

// 从索引 2 开始的 3 个元素
auto sub1 = view.subspan(2, 3);
MINE_ASSERT(sub1.size() == 3);
MINE_ASSERT(sub1[0] == 3);
MINE_ASSERT(sub1[2] == 5);

// 从索引 3 到末尾
auto sub2 = view.subspan(3);
MINE_ASSERT(sub2.size() == 5);  // 8 - 3
MINE_ASSERT(sub2[0] == 4);

// 空子视图
auto empty = view.subspan(8, 0);
MINE_ASSERT(empty.empty());

// 等价用法
auto tail = view.subspan(5);      // 等价于 view.last(3)
auto head = view.subspan(0, 3);   // 等价于 view.first(3)
```

---

## 类型转换方法

### operator Span<const T>

```cpp
constexpr operator Span<const T>() const noexcept;
```

**描述**：隐式转换为只读视图（允许可变视图赋给只读视图）。

**隐式转换**：允许隐式转换，因为：
1. 从可变到只读是安全的（只减少权限）
2. 不涉及内存操作
3. 符合 C++ const 语义（`T*` 可隐式转换为 `const T*`）

**示例**：

```cpp
int arr[] = {1, 2, 3};
mine::core::Span<int> mutable_view{arr};

// 隐式转换为只读视图
mine::core::Span<const int> readonly_view = mutable_view;
MINE_ASSERT(readonly_view.size() == 3);
// readonly_view[0] = 99;  // 编译错误：const 视图不允许修改

// 函数参数隐式转换
void process(mine::core::Span<const int> data);
process(mutable_view);  // OK：自动转换为 Span<const int>
```

---

### as_bytes

```cpp
[[nodiscard]] Span<const unsigned char> as_bytes() const noexcept;
```

**描述**：返回将视图内容解释为字节序列的只读视图。

**返回值**：`Span<const unsigned char>{reinterpret_cast<const unsigned char*>(data()), size_bytes()}`

**用途**：
- 序列化/反序列化
- 网络传输
- 文件 I/O
- 哈希计算
- 内存比较

**时间复杂度**：O(1)

**注意**：
- 返回的字节视图是只读的（即使原视图可变）
- 不涉及内存拷贝，仅改变解释方式

**示例**：

```cpp
int arr[] = {0x12345678, 0xABCDEF00};
mine::core::Span<int> view{arr};

auto bytes = view.as_bytes();
MINE_ASSERT(bytes.size() == 8);  // 2 * sizeof(int)

// 访问字节（小端序）
MINE_ASSERT(bytes[0] == 0x78);  // 最低字节
MINE_ASSERT(bytes[1] == 0x56);

// 用于哈希计算
uint64_t hash = compute_hash(bytes.data(), bytes.size());
```

---

## 使用场景

### 1. 函数参数（避免拷贝）

**问题**：传递 `std::vector<T>` 会强制所有权语义，传递 `const std::vector<T>&` 限制了灵活性。

**解决方案**：使用 `Span<T>` 接受任意连续内存源。

```cpp
// ❌ 不推荐：限制了参数类型
void process_bad(const std::vector<float>& data);

// ✅ 推荐：接受任意连续内存
void process_good(mine::core::Span<const float> data) {
    for (float x : data) {
        // 处理...
    }
}

// 调用示例
mine::containers::Vector<float> vec{1.0f, 2.0f};
float arr[] = {3.0f, 4.0f};

process_good(vec);   // 自动转换
process_good(arr);   // 自动转换
process_good({arr, 2});  // 显式构造
```

---

### 2. 容器适配器

**用途**：为自有容器（`Vector<T>`、`SmallVector<T>`）提供统一的迭代接口。

```cpp
template<typename T>
class Vector {
public:
    mine::core::Span<T> as_span() noexcept {
        return {data(), size()};
    }

    mine::core::Span<const T> as_span() const noexcept {
        return {data(), size()};
    }

    // 隐式转换支持（通过构造函数约束）
    operator mine::core::Span<T>() noexcept { return as_span(); }
    operator mine::core::Span<const T>() const noexcept { return as_span(); }
};
```

---

### 3. 数组切片操作

**用途**：零拷贝地访问数组的子区间。

```cpp
void draw_lines(mine::core::Span<const Vertex> vertices) {
    // 每两个顶点画一条线
    for (size_t i = 0; i + 1 < vertices.size(); i += 2) {
        auto line = vertices.subspan(i, 2);
        draw_line(line[0], line[1]);
    }
}
```

---

### 4. 序列化/反序列化

**用途**：以字节视图形式访问结构体数据。

```cpp
struct Header {
    uint32_t magic;
    uint32_t version;
};

void write_header(mine::core::Span<Header> headers, FILE* file) {
    auto bytes = headers.as_bytes();
    fwrite(bytes.data(), 1, bytes.size(), file);
}
```

---

### 5. 分块处理

**用途**：将大数据分块并行处理。

```cpp
void process_parallel(mine::core::Span<int> data, size_t chunk_size) {
    for (size_t offset = 0; offset < data.size(); offset += chunk_size) {
        auto chunk = data.subspan(offset, chunk_size);
        async_task([chunk]() {
            for (int& x : chunk) {
                x *= 2;
            }
        });
    }
}
```

---

## 性能分析

### 内存布局

```cpp
sizeof(Span<T>) == 16   // 64 位系统：指针(8) + size_t(8)
alignof(Span<T>) == 8
```

**布局**：

```
┌────────────────┬────────────────┐
│   T* data_     │  size_t size_  │
│   (8 bytes)    │   (8 bytes)    │
└────────────────┴────────────────┘
```

### 操作时间复杂度

| 操作 | 时间复杂度 | 说明 |
|------|-----------|------|
| 构造 | O(1) | 仅存储指针和大小 |
| 拷贝 | O(1) | 两个指针大小的拷贝 |
| `operator[]` | O(1) | 直接指针偏移 |
| `front()` / `back()` | O(1) | 直接指针访问 |
| `subspan()` | O(1) | 指针算术 |
| `as_bytes()` | O(1) | `reinterpret_cast` |
| `begin()` / `end()` | O(1) | 返回存储的指针 |

### 编译器优化

**寄存器传递**：大多数架构上，`Span<T>` 可完全在寄存器中传递（两个寄存器：数据指针 + 大小）。

**内联**：所有方法均标记 `constexpr`，编译器可在编译期完全展开。

**无开销**：生成的汇编代码与手写指针+大小参数完全相同。

**示例（x86-64）**：

```cpp
void foo(mine::core::Span<int> span) {
    span[0] = 42;
}

// 生成汇编（近似）
foo(int* rdi, size_t rsi):  // span.data_ in rdi, span.size_ in rsi
    mov DWORD PTR [rdi], 42
    ret
```

---

## 线程安全性

### 并发读取

**安全**：多个线程同时读取同一 `Span` 是安全的，前提是底层数据不被修改。

```cpp
mine::core::Span<const int> shared_data = ...;

// 线程 1
int sum1 = std::accumulate(shared_data.begin(), shared_data.end(), 0);

// 线程 2（同时执行）
int max = *std::max_element(shared_data.begin(), shared_data.end());
```

### 并发写入

**不安全**：`Span<T>` 不提供同步机制，并发修改同一元素需要外部同步。

```cpp
mine::core::Span<int> data = ...;

// ❌ 数据竞争
// 线程 1: data[0] = 1;
// 线程 2: data[0] = 2;

// ✅ 外部同步
std::mutex mtx;
// 线程 1: { std::lock_guard lock{mtx}; data[0] = 1; }
// 线程 2: { std::lock_guard lock{mtx}; data[0] = 2; }
```

### 分块并发

**安全**：通过 `subspan()` 创建非重叠子视图，多线程分别处理不同块。

```cpp
mine::core::Span<int> data{buffer, 1000};

// 线程 1 处理前半部分
async_task([data]() {
    auto chunk = data.first(500);
    for (int& x : chunk) x *= 2;
});

// 线程 2 处理后半部分
async_task([data]() {
    auto chunk = data.last(500);
    for (int& x : chunk) x *= 2;
});
```

---

## 限制与注意事项

### 1. 生命周期管理

**问题**：`Span` 不管理底层内存，调用方必须确保底层数据在视图使用期间保持有效。

**危险示例**：

```cpp
mine::core::Span<int> create_dangling() {
    int local[] = {1, 2, 3};
    return {local};  // ❌ 返回局部变量的视图（悬垂指针）
}

auto span = create_dangling();
int x = span[0];  // 未定义行为：访问已释放的栈内存
```

**安全实践**：

```cpp
// ✅ 返回持久存储的视图
class DataStore {
    mine::containers::Vector<int> data_;
public:
    mine::core::Span<int> get_span() { return data_; }
};

// ✅ 函数内部使用临时视图
void process() {
    int arr[] = {1, 2, 3};
    mine::core::Span<int> view{arr};
    do_work(view);  // OK：arr 在作用域内有效
}
```

---

### 2. 不支持动态扩展

**限制**：`Span` 是只读大小的视图，不支持 `push_back()`、`resize()` 等操作。

**解决方案**：若需动态增长，使用 `Vector<T>` 并在完成后转为 `Span<T>`。

```cpp
mine::containers::Vector<int> vec;
vec.push_back(1);
vec.push_back(2);

mine::core::Span<int> view{vec};  // 转为视图传递给其他函数
```

---

### 3. 非连续内存不适用

**限制**：`Span` 要求内存连续，不支持链表、`std::deque` 等非连续容器。

**不支持**：

```cpp
std::list<int> lst{1, 2, 3};
// mine::core::Span<int> bad{lst};  // 编译错误：没有 data() 方法
```

---

### 4. 隐式转换陷阱

**问题**：容器到 `Span` 的隐式转换可能导致生命周期问题。

**危险示例**：

```cpp
mine::core::Span<int> span = mine::containers::Vector<int>{1, 2, 3};  
// ❌ 临时对象在语句结束后销毁
int x = span[0];  // 未定义行为
```

**安全实践**：

```cpp
auto vec = mine::containers::Vector<int>{1, 2, 3};
mine::core::Span<int> span = vec;  // ✅ vec 生命周期覆盖 span
```

---

### 5. 边界检查仅在 Debug 模式

**限制**：Release 构建下，越界访问不会触发断言，导致未定义行为。

**最佳实践**：

```cpp
// ❌ Release 下不安全
void unsafe_access(mine::core::Span<int> data, size_t index) {
    int x = data[index];  // 若 index >= size()，未定义行为
}

// ✅ 显式检查
void safe_access(mine::core::Span<int> data, size_t index) {
    if (index < data.size()) {
        int x = data[index];
    } else {
        // 处理错误
    }
}

// ✅ 使用迭代器避免索引
void iterator_access(mine::core::Span<int> data) {
    for (int x : data) {
        // 自动边界安全
    }
}
```

---

## 最佳实践

### 1. 优先使用 Span 作为函数参数

**推荐**：对于需要只读访问连续数据的函数，使用 `Span<const T>`。

```cpp
// ✅ 灵活接受任意数据源
void compute_sum(mine::core::Span<const int> data) {
    int sum = 0;
    for (int x : data) sum += x;
    return sum;
}
```

---

### 2. const 正确性

**推荐**：需要修改时使用 `Span<T>`，只读时使用 `Span<const T>`。

```cpp
void fill_zeros(mine::core::Span<int> data) {
    for (int& x : data) x = 0;
}

int count_positive(mine::core::Span<const int> data) {
    return std::count_if(data.begin(), data.end(), [](int x) { return x > 0; });
}
```

---

### 3. 避免存储临时对象的视图

**推荐**：确保底层数据生命周期长于视图。

```cpp
// ❌ 错误
mine::core::Span<int> get_data() {
    mine::containers::Vector<int> temp{1, 2, 3};
    return temp;  // temp 被销毁，返回悬垂视图
}

// ✅ 正确：返回持久存储的视图
class Storage {
    mine::containers::Vector<int> data_;
public:
    mine::core::Span<int> view() { return data_; }
};
```

---

### 4. 使用子视图代替拷贝

**推荐**：通过 `subspan()` 创建子视图，而非拷贝数据。

```cpp
// ❌ 不必要的拷贝
mine::containers::Vector<int> copy_tail(mine::core::Span<int> data) {
    mine::containers::Vector<int> result;
    for (size_t i = data.size() / 2; i < data.size(); ++i) {
        result.push_back(data[i]);
    }
    return result;
}

// ✅ 零拷贝子视图
mine::core::Span<int> view_tail(mine::core::Span<int> data) {
    return data.subspan(data.size() / 2);
}
```

---

### 5. 与标准算法结合

**推荐**：利用 `begin()` / `end()` 与标准算法协同。

```cpp
#include <algorithm>

void sort_data(mine::core::Span<int> data) {
    std::sort(data.begin(), data.end());
}

bool contains(mine::core::Span<const int> data, int value) {
    return std::find(data.begin(), data.end(), value) != data.end();
}
```

---

### 6. 空视图检查

**推荐**：在访问元素前检查 `empty()`。

```cpp
void process(mine::core::Span<int> data) {
    if (data.empty()) {
        return;  // 提前返回
    }
    
    int first = data.front();  // 安全：已检查非空
    // ...
}
```

---

### 7. 分块处理大数据

**推荐**：使用 `subspan()` 分块处理，提高缓存局部性。

```cpp
void process_large_data(mine::core::Span<float> data) {
    constexpr size_t kChunkSize = 1024;
    for (size_t offset = 0; offset < data.size(); offset += kChunkSize) {
        auto chunk = data.subspan(offset, kChunkSize);
        process_chunk(chunk);
    }
}
```

---

## 完整示例

### 示例 1：数据处理管道

```cpp
#include <mine/core/Span.h>
#include <mine/containers/Vector.h>

// 阶段 1：加载数据
mine::containers::Vector<float> load_sensor_data() {
    mine::containers::Vector<float> data;
    for (int i = 0; i < 100; ++i) {
        data.push_back(static_cast<float>(i) * 0.1f);
    }
    return data;
}

// 阶段 2：过滤（接受视图，避免拷贝）
mine::containers::Vector<float> filter_outliers(mine::core::Span<const float> data, float threshold) {
    mine::containers::Vector<float> result;
    for (float x : data) {
        if (x <= threshold) {
            result.push_back(x);
        }
    }
    return result;
}

// 阶段 3：归一化（原地修改）
void normalize(mine::core::Span<float> data, float max_value) {
    for (float& x : data) {
        x /= max_value;
    }
}

// 主流程
void process_pipeline() {
    auto raw_data = load_sensor_data();
    auto filtered = filter_outliers(raw_data, 5.0f);  // 自动转为 Span<const float>
    normalize(filtered, 10.0f);  // 自动转为 Span<float>
    
    // filtered 现在包含归一化后的结果
}
```

---

### 示例 2：二维矩阵视图

```cpp
#include <mine/core/Span.h>

class Matrix {
    mine::containers::Vector<float> data_;
    size_t rows_;
    size_t cols_;

public:
    Matrix(size_t rows, size_t cols)
        : data_(rows * cols), rows_{rows}, cols_{cols} {}

    // 获取第 row 行的视图
    mine::core::Span<float> row(size_t row) {
        MINE_ASSERT(row < rows_);
        return {data_.data() + row * cols_, cols_};
    }

    // 获取第 col 列（不连续，需拷贝或自定义迭代器）
    mine::containers::Vector<float> column(size_t col) {
        MINE_ASSERT(col < cols_);
        mine::containers::Vector<float> result;
        for (size_t r = 0; r < rows_; ++r) {
            result.push_back(data_[r * cols_ + col]);
        }
        return result;
    }
};

void example_matrix() {
    Matrix mat{3, 4};
    
    // 填充第 0 行
    auto row0 = mat.row(0);
    for (size_t i = 0; i < row0.size(); ++i) {
        row0[i] = static_cast<float>(i);
    }
    
    // 遍历第 1 行
    for (float& x : mat.row(1)) {
        x = 99.0f;
    }
}
```

---

### 示例 3：序列化与反序列化

```cpp
#include <mine/core/Span.h>
#include <cstdio>

struct Vertex {
    float x, y, z;
};

// 写入二进制文件
void save_vertices(mine::core::Span<const Vertex> vertices, const char* filename) {
    FILE* file = fopen(filename, "wb");
    if (!file) return;
    
    auto bytes = vertices.as_bytes();
    fwrite(bytes.data(), 1, bytes.size(), file);
    fclose(file);
}

// 从文件读取
mine::containers::Vector<Vertex> load_vertices(const char* filename) {
    FILE* file = fopen(filename, "rb");
    if (!file) return {};
    
    fseek(file, 0, SEEK_END);
    size_t file_size = ftell(file);
    fseek(file, 0, SEEK_SET);
    
    mine::containers::Vector<Vertex> vertices;
    vertices.resize(file_size / sizeof(Vertex));
    
    fread(vertices.data(), 1, file_size, file);
    fclose(file);
    
    return vertices;
}
```

---

## 推导指南

```cpp
template<typename T, size_t N>
Span(T (&)[N]) -> Span<T>;

template<typename Container>
Span(Container&) -> Span<typename Container::value_type>;

template<typename Container>
Span(const Container&) -> Span<const typename Container::value_type>;
```

**用途**：C++17 类模板参数推导（CTAD），允许省略模板参数。

**示例**：

```cpp
int arr[] = {1, 2, 3};
mine::core::Span view{arr};  // 推导为 Span<int>

mine::containers::Vector<float> vec;
mine::core::Span s1{vec};        // 推导为 Span<float>
mine::core::Span s2{std::as_const(vec)};  // 推导为 Span<const float>
```

---

## 与 std::span 的差异

| 特性 | `mine::core::Span<T>` | `std::span<T>` (C++20) |
|------|------------------------|------------------------|
| 标准 | C++17 | C++20 |
| 静态大小 | ❌ 不支持 | ✅ `std::span<T, N>` |
| 边界检查 | Debug 模式断言 | 无（或通过 `at()` in C++26） |
| `as_bytes()` | ✅ 返回 `Span<const unsigned char>` | ✅ 返回 `std::span<const std::byte>` |
| `as_writable_bytes()` | ❌ 不提供 | ✅ 返回 `std::span<std::byte>` |
| 依赖 | 仅 `<cstddef>` | `<span>` 头文件 |
| ABI 稳定性 | ✅ 跨 DLL 边界安全 | ⚠️ 依赖 STL 实现 |

---

## 常见问题

### Q1：为什么不支持 `std::vector<T>` 作为构造参数？

**A**：因为 MineFramework 避免在公共头文件中引入 STL 容器（ABI 不稳定性）。解决方案：

```cpp
std::vector<int> vec{1, 2, 3};
mine::core::Span<int> view{vec.data(), vec.size()};  // 显式构造
```

---

### Q2：`Span<T>` 与 `T*` + `size_t` 有何区别？

**A**：

| 特性 | `Span<T>` | `T* + size_t` |
|------|-----------|---------------|
| 类型安全 | ✅ 单一类型 | ❌ 参数易错位 |
| 边界检查 | ✅ Debug 断言 | ❌ 手动检查 |
| 子视图操作 | ✅ `subspan()` | ❌ 手动计算指针 |
| Range-for | ✅ 自动支持 | ❌ 需手写循环 |
| 标准算法 | ✅ `begin()/end()` | ❌ 需手动传递 |

---

### Q3：如何处理 `Span<T>` 与 `Span<const T>` 的函数重载？

**A**：通常只提供 `Span<const T>` 版本（只读），避免重载冲突。若需修改，显式提供 `Span<T>` 版本。

```cpp
// 只读版本（接受任何视图）
int compute_sum(mine::core::Span<const int> data);

// 修改版本（仅接受可变视图）
void fill_zeros(mine::core::Span<int> data);
```

---

### Q4：如何安全地存储 `Span<T>` 作为类成员？

**A**：必须确保 `Span` 引用的数据生命周期长于类实例。通常不推荐存储 `Span`，而是存储拥有所有权的容器。

```cpp
// ❌ 危险：外部数据可能先释放
class Bad {
    mine::core::Span<int> data_;
public:
    Bad(mine::core::Span<int> data) : data_{data} {}
};

// ✅ 安全：拥有数据所有权
class Good {
    mine::containers::Vector<int> data_;
public:
    mine::core::Span<int> view() { return data_; }
};
```

---

## 总结

`Span<T>` 是 MineFramework 中传递连续内存视图的标准方式，具备以下优势：

1. **ABI 稳定**：跨 DLL 边界安全，不依赖 STL 实现
2. **零开销**：编译为与原始指针相同的汇编代码
3. **类型安全**：编译期区分只读/可变视图
4. **灵活性**：接受数组、容器、原始指针等任意数据源
5. **易用性**：支持 range-for、标准算法、子视图操作

在设计公共 API 时，优先使用 `Span<const T>` 作为只读参数类型，使用 `Span<T>` 作为可变参数类型，这将大幅提升代码的灵活性和性能。
