# mine.containers —— 容器与数据结构模块

## 模块概述

`mine.containers` 在 `mine.core` 之上提供不依赖 STL 容器的自有实现，所有容器均使用 `mine::core::IAllocator` 进行内存管理（可替换），分配失败通过断言终止（不抛异常）。

| 容器 | 对标 STL | 主要特性 |
|------|---------|---------|
| `InlineString` | `std::string` | SSO ≤23 字节，UTF-8，隐式转 `StringView` |
| `Vector<T>` | `std::vector` | 动态数组，2x 扩容，裸指针迭代器 |
| `SmallVector<T,N>` | `llvm::SmallVector` | 内联 N 元素，溢出后切堆分配 |
| `HashMap<K,V>` | `std::unordered_map` | Robin Hood 开放寻址，负载 0.75 |
| `IntrusiveList<T>` | `boost::intrusive::list` | 侵入式双向链表，O(1) 脱链 |

**伞形头文件**：`<mine/containers/Containers.h>` 一次性引入所有容器。

**依赖**：仅 `mine.core`（`StringView`、`IAllocator`、`Assert`）。

---

## 1. InlineString —— 小字符串优化字符串

**文件**：`<mine/containers/InlineString.h>`

拥有式 UTF-8 字符串，长度 ≤23 字节时完全栈上存储（零堆分配）。

### 类摘要

```cpp
namespace mine::containers {

class InlineString {
public:
    using size_type       = size_t;
    using iterator        = char*;
    using const_iterator  = const char*;
    static constexpr size_type kInlineCap = 23;

    // 构造（可指定分配器，默认 default_allocator）
    explicit InlineString(IAllocator* alloc = default_allocator()) noexcept;
    InlineString(const char* str, IAllocator* alloc = default_allocator());
    InlineString(StringView sv, IAllocator* alloc = default_allocator());
    InlineString(const char* data, size_type len, IAllocator* alloc = default_allocator());

    // 拷贝 / 移动
    InlineString(const InlineString& other);
    InlineString(InlineString&& other) noexcept;
    InlineString& operator=(const InlineString& other);
    InlineString& operator=(InlineString&& other) noexcept;
    InlineString& operator=(const char* str);
    InlineString& operator=(StringView sv);

    // 容量
    size_type size()      const noexcept;
    size_type length()    const noexcept;
    bool      empty()     const noexcept;
    size_type capacity()  const noexcept;

    // 元素访问
    char        operator[](size_type i) const noexcept;
    char        front() const noexcept;
    char        back()  const noexcept;
    const char* data()  const noexcept;
    const char* c_str() const noexcept;

    // 隐式转换
    operator StringView() const noexcept;

    // 修改
    void clear() noexcept;
    void resize(size_type n, char ch = '\0');
    void reserve(size_type new_cap);

    // 追加
    InlineString& append(const char* str, size_type len);
    InlineString& append(const char* str);
    InlineString& append(StringView sv);
    InlineString& operator+=(const char* str);
    InlineString& operator+=(StringView sv);

    // 比较
    bool operator==(const InlineString&) const noexcept;
    bool operator!=(const InlineString&) const noexcept;
    bool operator==(StringView)          const noexcept;
    bool operator!=(StringView)          const noexcept;

    // 迭代器
    const_iterator begin() const noexcept;
    const_iterator end()   const noexcept;
};

} // namespace mine::containers
```

### 使用示例

```cpp
InlineString s{"hello"};                 // SSO，无堆分配
InlineString s2{s.data(), s.size()};     // 同内容
InlineString s3 = s + " world";          // 若 ≤23 字节仍 SSO

StringView sv = s;                       // 隐式转换
```

### 内存布局

| 模式 | 布局 | 条件 |
|------|------|------|
| 内联 | `char buf[23]` + `len(1 byte)` = 24 字节 | `size ≤ 23` |
| 堆 | `char* ptr` + `size` + `cap` + 标志位 = 24 字节 | `size > 23` |

---

## 2. Vector\<T\> —— 动态数组

**文件**：`<mine/containers/Vector.h>`

动态数组容器，行为等价 `std::vector` 的无异常子集。容量策略：2 倍增长，最小 8。

### 类摘要

```cpp
namespace mine::containers {

template<typename T>
class Vector {
public:
    using value_type      = T;
    using iterator        = T*;
    using const_iterator  = const T*;
    using size_type       = size_t;

    // 构造
    explicit Vector(IAllocator* alloc = default_allocator()) noexcept;
    explicit Vector(size_type count, IAllocator* alloc = default_allocator());
    Vector(size_type count, const T& value, IAllocator* alloc = default_allocator());
    Vector(std::initializer_list<T> init, IAllocator* alloc = default_allocator());

    // 拷贝 / 移动
    Vector(const Vector& other);
    Vector(Vector&& other) noexcept;
    Vector& operator=(const Vector& other);
    Vector& operator=(Vector&& other) noexcept;

    ~Vector() noexcept;

    // 容量
    size_type size()     const noexcept;
    size_type capacity() const noexcept;
    bool      empty()    const noexcept;

    // 元素访问
    T&       operator[](size_type i) noexcept;
    const T& operator[](size_type i) const noexcept;
    T&       front() noexcept;
    const T& front() const noexcept;
    T&       back()  noexcept;
    const T& back()  const noexcept;
    T*       data()  noexcept;
    const T* data()  const noexcept;

    // 修改
    void clear() noexcept;
    void reserve(size_type new_cap);
    void resize(size_type n);
    void resize(size_type n, const T& value);
    void push_back(const T& value);
    void push_back(T&& value);
    template<typename... Args> T& emplace_back(Args&&... args);
    void pop_back() noexcept;

    // 迭代器
    iterator       begin()        noexcept;
    const_iterator begin()        const noexcept;
    const_iterator cbegin()       const noexcept;
    iterator       end()          noexcept;
    const_iterator end()          const noexcept;
    const_iterator cend()         const noexcept;

    // 视图
    Span<T>       span()       noexcept;
    Span<const T> span()       const noexcept;
};

} // namespace mine::containers
```

### 平凡类型优化

对于 `std::is_trivially_copyable_v<T>` 且 `std::is_trivially_destructible_v<T>` 的类型，`Vector` 在扩容/移动时使用 `memcpy`/`memmove` 而非逐元素构造/析构。

---

## 3. SmallVector\<T, N\> —— 小对象优化数组

**文件**：`<mine/containers/SmallVector.h>`

与 `Vector<T>` API 一致，但额外提供内联 N 个元素的栈上存储。元素数 ≤ N 时零堆分配。

### 类摘要

```cpp
namespace mine::containers {

template<typename T, size_t N>
    requires (N > 0)
class SmallVector {
public:
    // 与 Vector<T> 完全相同的类型别名与容量/访问/修改接口
    // ...

    // 特有查询
    bool is_inline() const noexcept;   // 当前是否使用内联存储
};

} // namespace mine::containers
```

### 使用示例

```cpp
SmallVector<int, 8> v;             // 内联 8 个 int 的栈空间
v.push_back(1);                    // 仍在内联模式
v.push_back(2);
// ... 若 push_back 超过 8 个，自动切换为堆分配

// 适合函数内临时集合
SmallVector<UIElement*, 16> hits;  // 命中测试候选，典型 ≤16 个
```

---

## 4. HashMap\<K, V\> —— 哈希映射

**文件**：`<mine/containers/HashMap.h>`

Robin Hood 开放寻址哈希表，负载因子阈值 0.75，自动 2x 扩容。

### 类摘要

```cpp
namespace mine::containers {

template<typename K, typename V,
         typename THash  = Hash<K>,
         typename TEqual = Equal<K>>
class HashMap {
public:
    using key_type    = K;
    using mapped_type = V;
    using size_type   = size_t;

    class iterator {
    public:
        K& key()   noexcept;
        V& value() noexcept;
        iterator& operator++() noexcept;
        bool operator==(const iterator&) const noexcept;
        bool operator!=(const iterator&) const noexcept;
    };

    // 构造
    explicit HashMap(IAllocator* alloc = default_allocator()) noexcept;
    HashMap(std::initializer_list</*...*/> init, IAllocator* alloc = default_allocator());

    // 移动（不可拷贝——需定义 Key/Value 的拷贝策略）
    HashMap(HashMap&& other) noexcept;
    HashMap& operator=(HashMap&& other) noexcept;

    ~HashMap() noexcept;

    // 容量
    size_type size()     const noexcept;
    bool      empty()    const noexcept;
    size_type bucket_count() const noexcept;

    // 查找
    iterator  find(const K& key);
    bool      contains(const K& key) const noexcept;
    V&        operator[](const K& key);            // 不存在时插入默认值

    // 插入 / 删除
    // insert 返回 pair<iterator, bool>（bool 表示是否为新插入）
    std::pair<iterator, bool> insert(const K& key, const V& value);
    std::pair<iterator, bool> insert(K&& key, V&& value);
    bool erase(const K& key) noexcept;             // 成功返回 true
    void clear() noexcept;

    // 迭代器
    iterator begin() noexcept;
    iterator end()   noexcept;
};

} // namespace mine::containers
```

### 哈希函数 —— `Hash<K>`

**文件**：`<mine/containers/Hash.h>`

FNV-1a 32 位算法，默认特化：

| 类型 | 哈希方式 |
|------|---------|
| 整数类型（`uint8_t` ~ `uint64_t`） | 直接按字节 FNV-1a |
| 指针类型 | `reinterpret_cast<uintptr_t>` FNV-1a |
| `float` / `double` | 按位解释为 `uint32_t`/`uint64_t` 后 FNV-1a |
| `StringView` | 逐字节 FNV-1a |

自定义类型需特化 `mine::containers::Hash<T>`：

```cpp
template<>
struct mine::containers::Hash<MyKey> {
    size_t operator()(const MyKey& k) const noexcept {
        return /* ... */;
    }
};
```

---

## 5. IntrusiveList\<T\> —— 侵入式双向链表

**文件**：`<mine/containers/IntrusiveList.h>`

节点内存由外部管理，链表不分配/释放节点。节点析构时自动脱链。

### 类摘要

```cpp
namespace mine::containers {

// 节点基类（CRTP）
template<typename T>
class IntrusiveListNode {
public:
    IntrusiveListNode() noexcept;
    ~IntrusiveListNode() noexcept;         // 自动 unlink

    IntrusiveListNode(const IntrusiveListNode&) = delete;   // 不可拷贝
    IntrusiveListNode(IntrusiveListNode&&)      = delete;   // 不可移动

    bool is_linked() const noexcept;
    void unlink() noexcept;                // O(1) 脱链
};

// 链表
template<typename T>
class IntrusiveList {
public:
    class iterator { /* 双向迭代器 */ };

    IntrusiveList() noexcept;

    // 不可拷贝，可移动
    IntrusiveList(IntrusiveList&&) noexcept;
    IntrusiveList& operator=(IntrusiveList&&) noexcept;

    bool   empty() const noexcept;
    size_t size()  const noexcept;          // O(n)

    T& front() noexcept;
    T& back()  noexcept;

    void push_back(T& item) noexcept;
    void push_front(T& item) noexcept;
    void pop_back() noexcept;
    void pop_front() noexcept;
    void clear() noexcept;

    iterator insert(iterator pos, T& item) noexcept;
    iterator erase(iterator pos) noexcept;

    iterator begin() noexcept;
    iterator end()   noexcept;
};

} // namespace mine::containers
```

### 使用示例

```cpp
struct Widget : public IntrusiveListNode<Widget> {
    int id;
};

IntrusiveList<Widget> list;
Widget w1{1}, w2{2};
list.push_back(w1);
list.push_back(w2);

for (Widget& w : list) {
    // ...
}

// 节点析构时自动脱链（无需手动 erase）
```

---

## 6. 伞形头文件 —— `<mine/containers/Containers.h>`

一次性引入所有容器类型：

```cpp
#include <mine/containers/Containers.h>
// 等价于逐一包含：
//   InlineString.h, Vector.h, SmallVector.h,
//   HashMap.h, Hash.h, IntrusiveList.h
```

---

## 相关模块

| 模块 | 关系 |
|------|------|
| `mine.core` | `containers` 的基础依赖（`StringView`、`IAllocator`、`Assert`） |
| `mine.ui.property` | 使用 `SmallVector` 存储属性槽 |
| `mine.ui.style` | 使用 `SmallVector` 存储 setter 列表 |
