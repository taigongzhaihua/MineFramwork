# SmallVector 详细接口文档

## 概述

`SmallVector<T, N>` 是带有 **Small Buffer Optimization (SBO)** 的动态数组容器。它在栈上内联存储前 `N` 个元素,仅当元素数量超过 `N` 时才切换到堆分配。这种设计显著减少了小规模数据的堆分配开销,同时保持动态扩展能力。

### 核心特性

- **内联存储**：前 `N` 个元素直接存储在对象内部（栈上或对象内）
- **自动提升**：元素数量超过 `N` 时自动切换到堆分配
- **零拷贝小对象**：`N` 以内的元素无需堆分配,极大提升性能
- **完整 Vector 接口**：提供与 `Vector<T>` 相同的 API
- **类型优化**：平凡类型使用 `memcpy`/`memmove` 优化
- **可替换分配器**：支持自定义堆分配策略

### 设计动机

许多场景下,动态数组的元素数量较小且可预测:
- UI 控件的子元素列表（通常 < 10 个）
- 函数参数列表（通常 < 5 个）
- 临时工作缓冲区（通常 < 16 个元素）

使用 `Vector<T>` 即使只有 1 个元素也需要堆分配,而 `SmallVector<T, N>` 通过内联存储避免这一开销:

```cpp
// Vector<int>: 1 个元素也需要堆分配
Vector<int> v1{42};  // malloc(8 * sizeof(int))

// SmallVector<int, 8>: 8 个以内无堆分配
SmallVector<int, 8> v2{42};  // 栈上存储
```

---

## 类定义

```cpp
template<typename T, size_t N>
    requires (N > 0)
class SmallVector {
public:
    using value_type      = T;
    using pointer         = T*;
    using const_pointer   = const T*;
    using reference       = T&;
    using const_reference = const T&;
    using iterator        = T*;
    using const_iterator  = const T*;
    using size_type       = size_t;
    using difference_type = ptrdiff_t;

    // 构造/析构
    explicit SmallVector(IAllocator* alloc = default_allocator()) noexcept;
    explicit SmallVector(size_type count, IAllocator* alloc = default_allocator());
    SmallVector(size_type count, const T& value, IAllocator* alloc = default_allocator());
    SmallVector(std::initializer_list<T> init, IAllocator* alloc = default_allocator());
    template<typename InputIt>
    SmallVector(InputIt first, InputIt last, IAllocator* alloc = default_allocator());
    SmallVector(const SmallVector& other);
    SmallVector(SmallVector&& other) noexcept;
    ~SmallVector() noexcept;

    // 赋值
    SmallVector& operator=(const SmallVector& other);
    SmallVector& operator=(SmallVector&& other) noexcept;
    SmallVector& operator=(std::initializer_list<T> init);

    // 元素访问（与 Vector 相同）
    reference operator[](size_type idx) noexcept;
    const_reference operator[](size_type idx) const noexcept;
    reference at(size_type idx) noexcept;
    const_reference at(size_type idx) const noexcept;
    reference front() noexcept;
    const_reference front() const noexcept;
    reference back() noexcept;
    const_reference back() const noexcept;
    pointer data() noexcept;
    const_pointer data() const noexcept;

    // 迭代器
    iterator begin() noexcept;
    const_iterator begin() const noexcept;
    const_iterator cbegin() const noexcept;
    iterator end() noexcept;
    const_iterator end() const noexcept;
    const_iterator cend() const noexcept;

    // 容量
    bool empty() const noexcept;
    size_type size() const noexcept;
    size_type capacity() const noexcept;
    bool is_heap() const noexcept;  // 特有：判断是否堆分配
    void reserve(size_type new_cap);
    void shrink_to_fit();

    // 修改器（与 Vector 相同）
    void clear() noexcept;
    void push_back(const T& value);
    void push_back(T&& value);
    template<typename... Args>
    reference emplace_back(Args&&... args);
    void pop_back() noexcept;
    void resize(size_type new_size);
    void resize(size_type new_size, const T& value);
    iterator insert(const_iterator pos, const T& value);
    iterator insert(const_iterator pos, T&& value);
    template<typename... Args>
    iterator emplace(const_iterator pos, Args&&... args);
    iterator erase(const_iterator pos) noexcept;
    iterator erase(const_iterator first, const_iterator last) noexcept;
    void swap(SmallVector& other) noexcept;

    // 转换
    Span<T> as_span() noexcept;
    Span<const T> as_span() const noexcept;
};
```

---

## 特有功能

### is_heap

```cpp
bool is_heap() const noexcept;
```

**描述**：判断当前是否使用堆分配（元素数量超过 `N`）。

**返回值**：
- `true`：使用堆分配
- `false`：使用内联存储

**时间复杂度**：O(1)

**示例**：

```cpp
SmallVector<int, 8> v;
assert(!v.is_heap());  // 空容器,内联存储

for (int i = 0; i < 8; ++i) {
    v.push_back(i);
}
assert(!v.is_heap());  // 8 个元素,仍内联

v.push_back(9);
assert(v.is_heap());  // 超过 N,切换到堆
```

---

## 构造函数

SmallVector 的构造函数与 `Vector<T>` 完全相同,区别在于内联存储优化：

### 默认构造

```cpp
explicit SmallVector(IAllocator* alloc = default_allocator()) noexcept;
```

**描述**：构造空容器,使用内联存储。

**后置条件**：
- `size() == 0`
- `capacity() == N`
- `!is_heap()`

**时间复杂度**：O(1)

**示例**：

```cpp
SmallVector<int, 8> v;  // 内联存储,无堆分配
```

---

### 构造指定数量元素

```cpp
explicit SmallVector(size_type count, IAllocator* alloc = default_allocator());
SmallVector(size_type count, const T& value, IAllocator* alloc = default_allocator());
```

**描述**：构造含 `count` 个元素的容器。

**行为**：
- 若 `count <= N`：使用内联存储
- 若 `count > N`：分配堆内存

**示例**：

```cpp
SmallVector<int, 8> v1(5);     // 内联存储,5 个零
SmallVector<int, 8> v2(10, 42); // 堆分配,10 个 42
```

---

### 从初始化列表构造

```cpp
SmallVector(std::initializer_list<T> init, IAllocator* alloc = default_allocator());
```

**示例**：

```cpp
SmallVector<int, 8> v{1, 2, 3, 4, 5};  // 内联存储
```

---

### 拷贝构造

```cpp
SmallVector(const SmallVector& other);
```

**描述**：深拷贝另一个 SmallVector 的所有元素。

**行为**：
- 若 `other.size() <= N`：拷贝到内联存储
- 否则：分配堆内存并拷贝

**时间复杂度**：O(other.size())

**示例**：

```cpp
SmallVector<int, 8> v1{1, 2, 3};
SmallVector<int, 8> v2 = v1;  // 拷贝到内联存储
assert(!v2.is_heap());
```

---

### 移动构造

```cpp
SmallVector(SmallVector&& other) noexcept;
```

**描述**：转移另一个 SmallVector 的所有权。

**行为**：
- 若 `other.is_heap()`：
  - 直接转移堆指针（O(1)）
  - `other` 变为空容器（内联存储）
- 否则：
  - 逐元素移动到内联存储（O(N)）

**时间复杂度**：
- 堆分配：O(1)
- 内联存储：O(N)

**示例**：

```cpp
SmallVector<int, 8> v1{1, 2, 3};
SmallVector<int, 8> v2 = std::move(v1);  // O(3) 移动

SmallVector<int, 8> v3(100, 42);  // 堆分配
SmallVector<int, 8> v4 = std::move(v3);  // O(1) 转移指针
```

---

## 元素访问/迭代器/容量

这些 API 与 `Vector<T>` 完全相同,参见 [Vector 文档](01-Vector.md)。

---

## 修改器

### push_back

```cpp
void push_back(const T& value);
void push_back(T&& value);
```

**描述**：在尾部追加元素。

**行为**：
- 若 `size() < N`：追加到内联存储
- 若 `size() == N`：触发堆提升（分配 2*N 容量,拷贝所有元素）
- 若 `size() >= capacity()` 且已在堆上：扩容 2 倍

**时间复杂度**：
- 无扩容：O(1)
- 堆提升：O(N)
- 堆扩容：O(size())

**示例**：

```cpp
SmallVector<int, 4> v;
v.push_back(1);  // 内联
v.push_back(2);  // 内联
v.push_back(3);  // 内联
v.push_back(4);  // 内联
v.push_back(5);  // 触发堆提升,O(4)
```

---

### reserve

```cpp
void reserve(size_type new_cap);
```

**描述**：预分配容量。

**行为**：
- 若 `new_cap <= N`：无操作（保持内联存储）
- 若 `new_cap > N` 且当前内联：分配堆内存,移动所有元素
- 若 `new_cap > capacity()` 且当前在堆：扩容

**示例**：

```cpp
SmallVector<int, 8> v;
v.reserve(100);  // 分配堆内存,容量 100
assert(v.is_heap());
```

---

### shrink_to_fit

```cpp
void shrink_to_fit();
```

**描述**：释放多余容量。

**行为**：
- 若 `size() <= N` 且当前在堆：将元素移回内联存储,释放堆内存
- 否则：收缩堆内存到 `size()`

**示例**：

```cpp
SmallVector<int, 8> v(100, 42);  // 堆分配
v.resize(5);
v.shrink_to_fit();  // 移回内联存储
assert(!v.is_heap());
```

---

## 性能分析

### 内存布局

```cpp
template<typename T, size_t N>
class SmallVector {
    IAllocator* alloc_;  // 8 字节
    T* data_;            // 8 字节（指向 inline_buf_ 或堆）
    size_t size_;        // 8 字节
    size_t cap_;         // 8 字节
    alignas(T) char inline_buf_[N * sizeof(T)];  // N * sizeof(T) 字节
};
// sizeof(SmallVector<int, 8>) = 32 + 32 = 64 字节
// sizeof(SmallVector<int, 16>) = 32 + 64 = 96 字节
```

### 内联存储 vs 堆分配

| 元素数量 | SmallVector<T, 8> | Vector<T> |
|---------|------------------|-----------|
| 0-8 | 内联存储,0 次 malloc | 首次 push_back 时 malloc |
| 9-16 | 堆分配（1 次 malloc） | 堆分配（1 次 malloc） |
| 17+ | 堆分配,按需扩容 | 堆分配,按需扩容 |

### 操作时间复杂度

| 操作 | 内联存储 | 堆分配 | 说明 |
|------|---------|-------|------|
| `push_back` | O(1) | O(1) 摊销 | 触发堆提升时 O(N) |
| `pop_back` | O(1) | O(1) | 不会自动移回内联 |
| `reserve(>N)` | O(size) | O(size) | 提升到堆 |
| `shrink_to_fit` | O(size) | O(size) | 可能移回内联 |

### N 的选择建议

| 元素类型 | 推荐 N | 说明 |
|---------|-------|------|
| `int`, `float` | 8-16 | 占用 32-64 字节 |
| `std::string` | 4-8 | string 约 24 字节 |
| `std::unique_ptr` | 8-16 | 指针 8 字节 |
| 大对象 (>64B) | 4-8 | 避免 SmallVector 过大 |

---

## 使用场景

### 1. UI 子元素列表

```cpp
class Widget {
    SmallVector<Widget*, 8> children_;  // 大多数控件 < 8 个子元素
public:
    void add_child(Widget* child) {
        children_.push_back(child);  // 通常无堆分配
    }
};
```

---

### 2. 临时工作缓冲区

```cpp
void process_items(Span<const Item> items) {
    SmallVector<int, 16> indices;
    for (size_t i = 0; i < items.size(); ++i) {
        if (items[i].is_valid()) {
            indices.push_back(i);  // 小规模无堆分配
        }
    }
    // ...
}
```

---

### 3. 函数参数列表

```cpp
class Function {
    SmallVector<Variant, 4> args_;  // 大多数函数 < 4 个参数
public:
    void call() {
        // 绑定的参数通常很少,无需堆分配
    }
};
```

---

## 最佳实践

### 1. 根据典型大小选择 N

```cpp
// ✅ 推荐：N 匹配典型用例
SmallVector<int, 8> small_list;  // 90% 情况 < 8 个元素

// ❌ 不推荐：N 过大导致 SmallVector 本身占用过多栈空间
SmallVector<int, 1024> huge_list;  // sizeof = 4128 字节!
```

---

### 2. 大对象使用指针

```cpp
// ❌ 不推荐：大对象直接存储
SmallVector<HugeStruct, 4> v1;  // sizeof(SmallVector) 很大

// ✅ 推荐：存储指针
SmallVector<OwnedPtr<HugeStruct>, 8> v2;  // 指针只占 8 字节
```

---

### 3. 已知会超过 N 时使用 Vector

```cpp
// ❌ 不推荐：明知道会超过 N
SmallVector<int, 8> v;
v.reserve(1000);  // 立即分配堆,N=8 无意义

// ✅ 推荐：直接使用 Vector
Vector<int> v2;
v2.reserve(1000);
```

---

## 完整示例

### 示例：构建表达式树的子节点列表

```cpp
#include <mine/containers/SmallVector.h>

using namespace mine::containers;

struct ExprNode {
    enum class Kind { Number, Add, Mul };
    Kind kind;
    int value;  // 仅 Number 使用
    SmallVector<ExprNode*, 4> children;  // 大多数表达式 < 4 个子节点

    static ExprNode* number(int val) {
        auto* node = new ExprNode;
        node->kind = Kind::Number;
        node->value = val;
        return node;
    }

    static ExprNode* add(ExprNode* lhs, ExprNode* rhs) {
        auto* node = new ExprNode;
        node->kind = Kind::Add;
        node->children.push_back(lhs);
        node->children.push_back(rhs);
        return node;
    }

    int eval() const {
        switch (kind) {
            case Kind::Number: return value;
            case Kind::Add: return children[0]->eval() + children[1]->eval();
            case Kind::Mul: {
                int result = 1;
                for (auto* child : children) {
                    result *= child->eval();
                }
                return result;
            }
        }
    }
};

void example() {
    // 构建表达式树：(1 + 2) * (3 + 4)
    auto* expr = ExprNode::add(
        ExprNode::add(ExprNode::number(1), ExprNode::number(2)),
        ExprNode::add(ExprNode::number(3), ExprNode::number(4))
    );
    
    int result = expr->eval();  // 3 + 7 = 10
    
    // 所有节点的 children 列表都在内联存储中,无堆分配
}
```

---

## 总结

`SmallVector<T, N>` 是针对小规模数据优化的动态数组容器,具备以下优势:

1. **零开销小对象**：`N` 个以内元素无堆分配
2. **自动提升**：超过 `N` 时无缝切换到堆分配
3. **完整 Vector 接口**：与 `Vector<T>` API 兼容
4. **类型优化**：平凡类型自动优化

在已知数据规模较小且可预测时,优先使用 `SmallVector` 获得更好的性能。
