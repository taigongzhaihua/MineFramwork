# Vector 详细接口文档

## 概述

`Vector<T>` 是 MineFramework 中的动态数组容器,语义上等价于 `std::vector` 的无异常子集。它通过 `mine::core::IAllocator` 接口管理堆内存,支持运行时替换分配器,适用于需要可扩展连续存储的场景。

### 核心特性

- **动态扩容**：容量不足时自动 2 倍增长（最小 8 个元素）
- **类型优化**：平凡类型使用 `memcpy`/`memmove` 优化,非平凡类型使用 placement new/显式析构
- **可替换分配器**：通过 `IAllocator*` 支持自定义分配策略
- **迭代器兼容**：迭代器为裸指针,与标准算法兼容
- **无异常**：分配失败通过 `MINE_CHECK` 终止程序,不抛出异常
- **ABI 稳定**：公共接口不暴露 STL 类型,跨 DLL 边界安全

### 设计动机

直接使用 `std::vector` 存在以下问题:
1. **ABI 不稳定**：不同编译器/标准库版本的实现不兼容,跨 DLL 边界传递会导致崩溃
2. **异常依赖**：MineFramework 禁用异常,`std::vector` 的错误处理不适用
3. **分配器不可替换**：无法运行时替换分配器,测试时无法追踪内存泄漏

`Vector<T>` 通过以下方式解决:
- 提供 ABI 稳定的接口（无 STL 容器作为参数/返回值）
- 分配失败通过断言终止（而非异常）
- 运行时可替换分配器（测试时使用追踪分配器）

---

## 类定义

```cpp
template<typename T>
class Vector {
public:
    // 类型别名
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
    explicit Vector(IAllocator* alloc = default_allocator()) noexcept;
    explicit Vector(size_type count, IAllocator* alloc = default_allocator());
    Vector(size_type count, const T& value, IAllocator* alloc = default_allocator());
    Vector(std::initializer_list<T> init, IAllocator* alloc = default_allocator());
    template<typename InputIt>
    Vector(InputIt first, InputIt last, IAllocator* alloc = default_allocator());
    Vector(const Vector& other);
    Vector(Vector&& other) noexcept;
    ~Vector() noexcept;

    // 赋值
    Vector& operator=(const Vector& other);
    Vector& operator=(Vector&& other) noexcept;
    Vector& operator=(std::initializer_list<T> init);

    // 元素访问
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
    void reserve(size_type new_cap);
    void shrink_to_fit();

    // 修改器
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
    iterator insert(const_iterator pos, size_type count, const T& value);
    template<typename... Args>
    iterator emplace(const_iterator pos, Args&&... args);
    iterator erase(const_iterator pos) noexcept;
    iterator erase(const_iterator first, const_iterator last) noexcept;
    void swap(Vector& other) noexcept;

    // 转换
    Span<T> as_span() noexcept;
    Span<const T> as_span() const noexcept;
};
```

---

## 构造函数

### 默认构造

```cpp
explicit Vector(IAllocator* alloc = default_allocator()) noexcept;
```

**描述**：构造空容器,使用指定分配器。

**参数**：
- `alloc`：分配器指针（默认使用全局默认分配器）

**后置条件**：
- `size() == 0`
- `capacity() == 0`
- `data() == nullptr`

**时间复杂度**：O(1)

**示例**：

```cpp
using namespace mine::containers;

Vector<int> v1;  // 使用默认分配器
Vector<int> v2{custom_allocator()};  // 使用自定义分配器
```

---

### 构造指定数量的值初始化元素

```cpp
explicit Vector(size_type count, IAllocator* alloc = default_allocator());
```

**描述**：构造含 `count` 个值初始化元素的容器。

**参数**：
- `count`：初始元素数量
- `alloc`：分配器指针

**后置条件**：
- `size() == count`
- 所有元素值初始化（内置类型为零,类类型调用默认构造函数）

**约束**：`T` 必须可默认构造

**时间复杂度**：O(count)

**示例**：

```cpp
Vector<int> v1(10);  // [0, 0, 0, ..., 0] (10个零)

struct Widget {
    int x = 42;
    Widget() = default;
};
Vector<Widget> v2(5);  // 5个Widget{42}
```

---

### 构造指定数量的拷贝元素

```cpp
Vector(size_type count, const T& value,
       IAllocator* alloc = default_allocator());
```

**描述**：构造含 `count` 个 `value` 拷贝的容器。

**参数**：
- `count`：元素数量
- `value`：要拷贝的值
- `alloc`：分配器指针

**约束**：`T` 必须可拷贝构造

**时间复杂度**：O(count)

**示例**：

```cpp
Vector<int> v1(10, 42);  // [42, 42, ..., 42] (10个42)
Vector<std::string> v2(3, "hello");  // ["hello", "hello", "hello"]
```

---

### 从初始化列表构造

```cpp
Vector(std::initializer_list<T> init,
       IAllocator* alloc = default_allocator());
```

**描述**：从初始化列表构造容器。

**参数**：
- `init`：初始化列表
- `alloc`：分配器指针

**时间复杂度**：O(init.size())

**示例**：

```cpp
Vector<int> v1{1, 2, 3, 4, 5};
Vector<std::string> v2{"apple", "banana", "cherry"};
```

---

### 从迭代器区间构造

```cpp
template<typename InputIt>
    requires (!std::is_integral_v<InputIt>)
Vector(InputIt first, InputIt last,
       IAllocator* alloc = default_allocator());
```

**描述**：从迭代器区间 `[first, last)` 构造容器。

**参数**：
- `first`：起始迭代器
- `last`：终止迭代器
- `alloc`：分配器指针

**约束**：
- `InputIt` 不是整型（避免与 `Vector(count, value)` 混淆）
- `T` 可从 `*first` 构造

**时间复杂度**：O(distance(first, last))

**示例**：

```cpp
int arr[] = {1, 2, 3, 4, 5};
Vector<int> v1(arr, arr + 5);  // [1, 2, 3, 4, 5]

std::vector<int> stl_vec{10, 20, 30};
Vector<int> v2(stl_vec.begin(), stl_vec.end());  // [10, 20, 30]
```

---

### 拷贝构造

```cpp
Vector(const Vector& other);
```

**描述**：深拷贝 `other` 的所有元素。

**参数**：
- `other`：被拷贝的容器

**后置条件**：
- `size() == other.size()`
- 所有元素与 `other` 中对应元素相等
- 使用 `other` 的分配器

**约束**：`T` 必须可拷贝构造

**时间复杂度**：O(other.size())

**优化**：平凡类型使用 `memcpy` 加速

**示例**：

```cpp
Vector<int> v1{1, 2, 3};
Vector<int> v2 = v1;  // 深拷贝,v2 == [1, 2, 3]
v2[0] = 100;  // 不影响 v1
```

---

### 移动构造

```cpp
Vector(Vector&& other) noexcept;
```

**描述**：转移 `other` 的所有权,`other` 变为空容器。

**参数**：
- `other`：被移动的容器

**后置条件**：
- 当前容器接管 `other` 的数据指针/大小/容量
- `other.size() == 0`
- `other.data() == nullptr`

**时间复杂度**：O(1)

**示例**：

```cpp
Vector<int> v1{1, 2, 3};
Vector<int> v2 = std::move(v1);  // v2 == [1, 2, 3], v1 为空
```

---

### 析构函数

```cpp
~Vector() noexcept;
```

**描述**：析构所有元素并释放内存。

**行为**：
1. 调用所有元素的析构函数
2. 通过分配器释放内存块

**时间复杂度**：O(size())

---

## 赋值运算符

### 拷贝赋值

```cpp
Vector& operator=(const Vector& other);
```

**描述**：深拷贝 `other` 的所有元素,替换当前内容。

**参数**：
- `other`：被拷贝的容器

**行为**：
1. 销毁当前所有元素
2. 预分配足够容量
3. 拷贝 `other` 的所有元素

**自赋值安全**：检查 `this == &other`,安全返回

**约束**：`T` 必须可拷贝构造

**时间复杂度**：O(other.size())

**示例**：

```cpp
Vector<int> v1{1, 2, 3};
Vector<int> v2{10, 20};
v2 = v1;  // v2 == [1, 2, 3]
```

---

### 移动赋值

```cpp
Vector& operator=(Vector&& other) noexcept;
```

**描述**：转移 `other` 的所有权,`other` 变为空容器。

**参数**：
- `other`：被移动的容器

**行为**：
1. 销毁当前所有元素并释放内存
2. 接管 `other` 的数据指针/分配器
3. `other` 置空

**自赋值安全**：检查 `this == &other`,安全返回

**时间复杂度**：O(size()) 析构 + O(1) 转移

**示例**：

```cpp
Vector<int> v1{1, 2, 3};
Vector<int> v2{10, 20};
v2 = std::move(v1);  // v2 == [1, 2, 3], v1 为空
```

---

### 初始化列表赋值

```cpp
Vector& operator=(std::initializer_list<T> init);
```

**描述**：替换当前内容为初始化列表。

**参数**：
- `init`：初始化列表

**时间复杂度**：O(size() + init.size())

**示例**：

```cpp
Vector<int> v{1, 2, 3};
v = {10, 20, 30, 40};  // v == [10, 20, 30, 40]
```

---

## 元素访问

### operator[]

```cpp
reference operator[](size_type idx) noexcept;
const_reference operator[](size_type idx) const noexcept;
```

**描述**：访问指定索引的元素（Debug 模式下范围检查）。

**参数**：
- `idx`：元素索引（`0 <= idx < size()`）

**返回值**：对元素的引用

**前置条件**：`idx < size()`

**断言**：Debug 模式下检查 `idx < size()`

**时间复杂度**：O(1)

**示例**：

```cpp
Vector<int> v{10, 20, 30};
int x = v[1];  // 20
v[2] = 99;  // v == [10, 20, 99]

// Debug 模式下越界会触发断言
// v[10];  // MINE_ASSERT 失败
```

---

### at

```cpp
reference at(size_type idx) noexcept;
const_reference at(size_type idx) const noexcept;
```

**描述**：访问指定索引的元素（Release 模式也检查范围）。

**参数**：
- `idx`：元素索引

**返回值**：对元素的引用

**前置条件**：`idx < size()`

**检查**：Release 模式也通过 `MINE_CHECK` 检查范围,越界时终止程序

**时间复杂度**：O(1)

**示例**：

```cpp
Vector<int> v{10, 20, 30};
int x = v.at(1);  // 20

// Release 模式下越界也会终止程序
// v.at(10);  // MINE_CHECK 失败,程序终止
```

---

### front / back

```cpp
reference front() noexcept;
const_reference front() const noexcept;
reference back() noexcept;
const_reference back() const noexcept;
```

**描述**：访问首元素/尾元素。

**返回值**：对首/尾元素的引用

**前置条件**：`!empty()`

**断言**：Debug 模式下检查容器非空

**时间复杂度**：O(1)

**示例**：

```cpp
Vector<int> v{10, 20, 30};
int first = v.front();  // 10
int last = v.back();    // 30

v.front() = 1;  // v == [1, 20, 30]
v.back() = 99;  // v == [1, 20, 99]
```

---

### data

```cpp
pointer data() noexcept;
const_pointer data() const noexcept;
```

**描述**：返回指向底层数组的指针。

**返回值**：
- 非空容器：指向首元素的指针
- 空容器：`nullptr`

**时间复杂度**：O(1)

**示例**：

```cpp
Vector<int> v{1, 2, 3};
int* ptr = v.data();
std::memcpy(buffer, ptr, v.size() * sizeof(int));

// 与标准 C API 交互
void legacy_func(int* arr, size_t len);
legacy_func(v.data(), v.size());
```

---

## 迭代器

### begin / end

```cpp
iterator begin() noexcept;
const_iterator begin() const noexcept;
const_iterator cbegin() const noexcept;
iterator end() noexcept;
const_iterator end() const noexcept;
const_iterator cend() const noexcept;
```

**描述**：返回指向首元素/尾后位置的迭代器。

**返回值**：
- `begin()`：指向首元素的迭代器
- `end()`：指向尾后位置的迭代器

**迭代器类型**：裸指针（`T*`）

**时间复杂度**：O(1)

**示例**：

```cpp
Vector<int> v{1, 2, 3, 4, 5};

// 范围 for 循环
for (int x : v) {
    std::cout << x << " ";
}

// 标准算法
auto it = std::find(v.begin(), v.end(), 3);
std::sort(v.begin(), v.end());
```

---

## 容量

### empty / size / capacity

```cpp
bool empty() const noexcept;
size_type size() const noexcept;
size_type capacity() const noexcept;
```

**描述**：查询容器状态。

**返回值**：
- `empty()`：容器是否为空（等价于 `size() == 0`）
- `size()`：当前元素数量
- `capacity()`：已分配容量（可容纳的元素数量,无需重新分配）

**时间复杂度**：O(1)

**示例**：

```cpp
Vector<int> v;
assert(v.empty());
assert(v.size() == 0);
assert(v.capacity() == 0);

v.reserve(100);
assert(v.size() == 0);       // 预分配不改变 size
assert(v.capacity() >= 100);

v.push_back(42);
assert(!v.empty());
assert(v.size() == 1);
```

---

### reserve

```cpp
void reserve(size_type new_cap);
```

**描述**：预分配至少 `new_cap` 个元素的容量（不缩减容量）。

**参数**：
- `new_cap`：期望容量

**行为**：
- 若 `new_cap <= capacity()`：无操作
- 否则：重新分配 `new_cap` 大小的内存,移动所有元素

**后置条件**：`capacity() >= new_cap`

**迭代器失效**：若发生重新分配,所有迭代器/指针/引用失效

**时间复杂度**：
- 无重新分配：O(1)
- 发生重新分配：O(size())

**示例**：

```cpp
Vector<int> v;
v.reserve(1000);  // 预分配,避免多次扩容

for (int i = 0; i < 1000; ++i) {
    v.push_back(i);  // 无重新分配
}
```

---

### shrink_to_fit

```cpp
void shrink_to_fit();
```

**描述**：释放多余容量,收缩到当前 `size()`。

**后置条件**：`capacity() == size()`（或略大,取决于分配器）

**迭代器失效**：若发生重新分配,所有迭代器/指针/引用失效

**时间复杂度**：O(size())

**示例**：

```cpp
Vector<int> v;
v.reserve(1000);
v.push_back(1);
v.push_back(2);
v.push_back(3);
assert(v.size() == 3);
assert(v.capacity() == 1000);

v.shrink_to_fit();
assert(v.capacity() == 3);  // 释放多余内存
```

---

## 修改器

### clear

```cpp
void clear() noexcept;
```

**描述**：清空所有元素（不释放内存）。

**后置条件**：
- `size() == 0`
- `capacity()` 不变

**迭代器失效**：所有迭代器/指针/引用失效

**时间复杂度**：O(size())

**示例**：

```cpp
Vector<int> v{1, 2, 3, 4, 5};
size_t old_cap = v.capacity();

v.clear();
assert(v.size() == 0);
assert(v.capacity() == old_cap);  // 容量保留
```

---

### push_back

```cpp
void push_back(const T& value);  // 拷贝
void push_back(T&& value);       // 移动
```

**描述**：在尾部追加元素。

**参数**：
- `value`：要添加的元素（拷贝或移动）

**后置条件**：
- `size()` 增加 1
- `back() == value`

**迭代器失效**：若发生扩容,所有迭代器/指针/引用失效

**时间复杂度**：
- 无扩容：O(1)
- 发生扩容：O(size())（摊销 O(1)）

**示例**：

```cpp
Vector<std::string> v;
v.push_back("hello");  // 拷贝
std::string s = "world";
v.push_back(std::move(s));  // 移动,s 变为空

Vector<std::unique_ptr<int>> v2;
v2.push_back(std::make_unique<int>(42));  // 移动语义
```

---

### emplace_back

```cpp
template<typename... Args>
reference emplace_back(Args&&... args);
```

**描述**：在尾部原位构造元素,返回对新元素的引用。

**参数**：
- `args`：转发给 `T` 的构造函数的参数包

**返回值**：对新添加元素的引用

**后置条件**：`size()` 增加 1

**优势**：避免临时对象构造和移动

**时间复杂度**：摊销 O(1)

**示例**：

```cpp
struct Widget {
    int x;
    std::string name;
    Widget(int x_val, std::string name_val) : x{x_val}, name{std::move(name_val)} {}
};

Vector<Widget> v;
v.emplace_back(42, "test");  // 直接构造,无临时对象

// 返回值引用
Widget& w = v.emplace_back(99, "another");
w.x = 100;
```

---

### pop_back

```cpp
void pop_back() noexcept;
```

**描述**：移除尾部元素。

**前置条件**：`!empty()`

**后置条件**：`size()` 减少 1

**断言**：Debug 模式下检查容器非空

**迭代器失效**：指向尾元素的迭代器/指针/引用失效

**时间复杂度**：O(1)

**示例**：

```cpp
Vector<int> v{1, 2, 3};
v.pop_back();
assert(v.size() == 2);
assert(v.back() == 2);
```

---

### resize

```cpp
void resize(size_type new_size);
void resize(size_type new_size, const T& value);
```

**描述**：调整容器大小。

**参数**：
- `new_size`：新大小
- `value`：填充值（可选）

**行为**：
- 若 `new_size > size()`：
  - 无 `value`：新元素值初始化
  - 有 `value`：新元素拷贝自 `value`
- 若 `new_size < size()`：析构多余元素

**时间复杂度**：O(|new_size - size()|)

**示例**：

```cpp
Vector<int> v{1, 2, 3};

v.resize(5);  // v == [1, 2, 3, 0, 0]
v.resize(10, 42);  // v == [1, 2, 3, 0, 0, 42, 42, 42, 42, 42]
v.resize(3);  // v == [1, 2, 3]
```

---

### insert

```cpp
iterator insert(const_iterator pos, const T& value);
iterator insert(const_iterator pos, T&& value);
iterator insert(const_iterator pos, size_type count, const T& value);
```

**描述**：在 `pos` 处插入元素。

**参数**：
- `pos`：插入位置（`[begin(), end()]` 内的有效迭代器）
- `value`：要插入的元素
- `count`：插入数量

**返回值**：指向首个插入元素的迭代器

**迭代器失效**：若发生扩容,所有迭代器失效；否则 `pos` 及之后失效

**时间复杂度**：O(size() - pos + count)

**示例**：

```cpp
Vector<int> v{1, 2, 3};

v.insert(v.begin() + 1, 99);  // v == [1, 99, 2, 3]
v.insert(v.end(), 3, 42);     // v == [1, 99, 2, 3, 42, 42, 42]
```

---

### emplace

```cpp
template<typename... Args>
iterator emplace(const_iterator pos, Args&&... args);
```

**描述**：在 `pos` 处原位构造元素。

**参数**：
- `pos`：插入位置
- `args`：转发给 `T` 的构造函数的参数包

**返回值**：指向新元素的迭代器

**时间复杂度**：O(size() - pos)

**示例**：

```cpp
Vector<std::pair<int, std::string>> v;
v.emplace(v.begin(), 42, "test");  // 直接构造 pair
```

---

### erase

```cpp
iterator erase(const_iterator pos) noexcept;
iterator erase(const_iterator first, const_iterator last) noexcept;
```

**描述**：移除指定位置或区间的元素。

**参数**：
- `pos`：要移除的元素（`[begin(), end())` 内的有效迭代器）
- `first`, `last`：要移除的区间 `[first, last)`

**返回值**：指向移除元素后下一个元素的迭代器

**迭代器失效**：`pos` 及之后的所有迭代器失效

**时间复杂度**：O(size() - pos)

**示例**：

```cpp
Vector<int> v{1, 2, 3, 4, 5};

v.erase(v.begin() + 2);  // 移除3,v == [1, 2, 4, 5]
v.erase(v.begin() + 1, v.begin() + 3);  // 移除[2, 4],v == [1, 5]
```

---

### swap

```cpp
void swap(Vector& other) noexcept;
```

**描述**：交换两个容器的内容（O(1)）。

**参数**：
- `other`：要交换的容器

**后置条件**：两个容器的内容完全交换

**迭代器失效**：所有迭代器/指针/引用保持有效,但指向另一个容器

**时间复杂度**：O(1)

**示例**：

```cpp
Vector<int> v1{1, 2, 3};
Vector<int> v2{10, 20};

v1.swap(v2);
// v1 == [10, 20]
// v2 == [1, 2, 3]
```

---

## 转换

### as_span

```cpp
Span<T> as_span() noexcept;
Span<const T> as_span() const noexcept;
```

**描述**：将 `Vector` 转换为 `Span`,提供非拥有的数组视图。

**返回值**：指向底层数组的 `Span`

**时间复杂度**：O(1)

**示例**：

```cpp
Vector<int> v{1, 2, 3, 4, 5};
Span<int> span = v.as_span();

// 传递给接受 Span 的函数
void process(Span<int> data);
process(v.as_span());
```

---

## 性能分析

### 内存布局

```cpp
class Vector<T> {
    IAllocator* alloc_;  // 8 字节
    T* data_;            // 8 字节
    size_t size_;        // 8 字节
    size_t cap_;         // 8 字节
};
// sizeof(Vector<T>) == 32
```

### 扩容策略

| 操作 | 触发条件 | 新容量 | 说明 |
|------|---------|-------|------|
| 首次 `push_back` | `capacity() == 0` | 8 | 最小容量 |
| 后续扩容 | `size() == capacity()` | `capacity() * 2` | 2 倍增长 |

### 操作时间复杂度

| 操作 | 最坏情况 | 摊销 | 说明 |
|------|---------|------|------|
| `push_back` | O(n) | O(1) | 扩容时拷贝所有元素 |
| `pop_back` | O(1) | O(1) | 析构一个元素 |
| `insert(pos)` | O(n) | O(n) | 移动后续元素 |
| `erase(pos)` | O(n) | O(n) | 移动后续元素 |
| `operator[]` | O(1) | O(1) | 直接索引 |
| `clear` | O(n) | O(n) | 析构所有元素 |

### 类型优化

| T 的性质 | 优化 | 性能提升 |
|---------|-----|---------|
| `is_trivially_copyable` | `memcpy`/`memmove` | 拷贝速度提升 5-10 倍 |
| `is_trivially_destructible` | 批量析构优化 | 无需逐个调用析构函数 |

---

## 使用场景

### 1. 替代 std::vector

```cpp
// ❌ 不推荐：跨 DLL 边界传递 std::vector（ABI 不稳定）
class MINE_API Widget {
public:
    std::vector<int> get_data();  // 危险!
};

// ✅ 推荐：使用 Vector 或 Span
class MINE_API Widget {
public:
    Span<const int> get_data();  // ABI 稳定
private:
    Vector<int> data_;
};
```

---

### 2. 自定义分配器

```cpp
class PoolAllocator : public IAllocator {
    // ... 实现内存池
};

void example() {
    PoolAllocator pool;
    Vector<Widget> widgets{&pool};  // 使用内存池
    
    for (int i = 0; i < 1000; ++i) {
        widgets.emplace_back(i);  // 从池中分配
    }
}
```

---

### 3. 与标准算法配合

```cpp
Vector<int> v{5, 2, 8, 1, 9};

// 排序
std::sort(v.begin(), v.end());

// 查找
auto it = std::find(v.begin(), v.end(), 8);

// 转换
std::transform(v.begin(), v.end(), v.begin(),
               [](int x) { return x * 2; });
```

---

## 最佳实践

### 1. 预分配容量避免扩容

```cpp
// ✅ 推荐：已知大小时预分配
Vector<int> v;
v.reserve(1000);
for (int i = 0; i < 1000; ++i) {
    v.push_back(i);  // 无重新分配
}

// ❌ 不推荐：多次扩容
Vector<int> v2;
for (int i = 0; i < 1000; ++i) {
    v2.push_back(i);  // 多次扩容(log n 次)
}
```

---

### 2. 使用 emplace_back 避免临时对象

```cpp
struct Widget {
    Widget(int x, std::string name);
};

Vector<Widget> v;

// ✅ 推荐：直接构造
v.emplace_back(42, "test");

// ❌ 不推荐：构造临时对象然后移动
v.push_back(Widget{42, "test"});
```

---

### 3. 避免频繁 insert/erase

```cpp
// ❌ 不推荐：中间频繁插入/删除
Vector<int> v{1, 2, 3, 4, 5};
for (int i = 0; i < 100; ++i) {
    v.insert(v.begin(), i);  // O(n) 每次
}

// ✅ 推荐：批量操作或使用链表
```

---

### 4. 使用 as_span 传递视图

```cpp
// ✅ 推荐：传递 Span 避免拷贝
void process(Span<const int> data) {
    for (int x : data) { /* ... */ }
}

Vector<int> v{1, 2, 3};
process(v.as_span());  // 零拷贝

// ❌ 不推荐：传递 Vector 引用（限制灵活性）
void process2(const Vector<int>& data);
```

---

## 完整示例

### 示例：动态数组管理

```cpp
#include <mine/containers/Vector.h>
#include <algorithm>

using namespace mine::containers;

void example() {
    // 构造
    Vector<int> v1;
    Vector<int> v2{1, 2, 3, 4, 5};
    Vector<int> v3(10, 42);

    // 添加元素
    v1.push_back(1);
    v1.emplace_back(2);
    v1.insert(v1.begin() + 1, 99);

    // 访问元素
    int first = v1.front();
    int last = v1.back();
    int at_index = v1[1];

    // 修改
    v1.resize(10);
    v1.clear();
    v1.shrink_to_fit();

    // 算法
    std::sort(v2.begin(), v2.end());
    auto it = std::find(v2.begin(), v2.end(), 3);
    if (it != v2.end()) {
        v2.erase(it);
    }

    // 转换
    Span<int> span = v2.as_span();
}
```

---

## 总结

`Vector<T>` 是 MineFramework 中的动态数组容器,具备以下优势:

1. **ABI 稳定**：跨 DLL 边界安全
2. **无异常**：分配失败通过断言终止
3. **可替换分配器**：支持自定义内存管理策略
4. **类型优化**：平凡类型自动优化
5. **标准兼容**：迭代器与标准算法兼容

在需要动态扩展的数组时,优先使用 `Vector<T>` 而非 `std::vector`,这将确保代码在所有平台上正确运行。
