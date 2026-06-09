# HashMap 详细接口文档

## 概述

`HashMap<K, V>` 是 MineFramework 中的开放寻址哈希表,采用 **Robin Hood 线性探测** 策略实现高效键值对存储。相比标准库的 `std::unordered_map`,它提供了 ABI 稳定的接口,避免了跨 DLL 边界的兼容性问题。

### 核心特性

- **Robin Hood 线性探测**：平衡探测链长度,显著降低最坏情况查找时间
- **PSL 优化**：每个槽位记录探测序列长度(Probe Sequence Length),插入时"劫富济贫"
- **开放寻址**：所有元素存储在连续数组中,无额外节点分配,缓存友好
- **2 的幂次容量**：使用位掩码快速取模,避免昂贵的除法运算
- **0.75 负载因子**：默认在负载因子超过 0.75 时自动扩容为 2 倍
- **ABI 稳定**：公共接口不暴露 STL 容器,跨 DLL 边界安全
- **可替换分配器**：通过 `IAllocator*` 支持自定义内存管理

### 设计动机

使用 `std::unordered_map` 存在以下问题:
1. **ABI 不稳定**：链表节点布局在不同标准库实现间不兼容
2. **内存碎片**：链式哈希每个元素单独分配,导致大量小块内存碎片
3. **缓存不友好**：链表节点散布在堆中,缓存局部性差

`HashMap<K, V>` 通过开放寻址和 Robin Hood 策略解决这些问题:
- 所有槽位存储在连续数组中,缓存友好
- Robin Hood 平衡探测链,保证最坏情况性能
- ABI 稳定的接口设计

---

## 类定义

```cpp
template<typename K, typename V,
         typename THash  = Hash<K>,
         typename TEqual = std::equal_to<K>>
class HashMap {
public:
    using key_type   = K;
    using mapped_type = V;
    using size_type  = size_t;
    using hasher     = THash;
    using key_equal  = TEqual;

    class iterator;
    class const_iterator;

    // 构造/析构
    explicit HashMap(size_type init_cap = 0,
                     IAllocator* alloc = default_allocator());
    HashMap(const HashMap& other);
    HashMap(HashMap&& other) noexcept;
    ~HashMap() noexcept;

    // 赋值
    HashMap& operator=(const HashMap& other);
    HashMap& operator=(HashMap&& other) noexcept;

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
    size_type bucket_count() const noexcept;
    double load_factor() const noexcept;
    void reserve(size_type count);
    void clear() noexcept;

    // 查找
    iterator find(const K& key) noexcept;
    const_iterator find(const K& key) const noexcept;
    bool contains(const K& key) const noexcept;
    V* try_get(const K& key) noexcept;
    const V* try_get(const K& key) const noexcept;

    // 插入/更新
    bool insert(const K& key, const V& value);
    bool insert(const K& key, V&& value);
    template<typename... Args>
    bool emplace(const K& key, Args&&... args);
    V& operator[](const K& key);

    // 删除
    bool erase(const K& key) noexcept;
    iterator erase(iterator pos) noexcept;

    // 工具
    void swap(HashMap& other) noexcept;
};
```

---

## 迭代器

### iterator 类

```cpp
class iterator {
public:
    K& key() noexcept;
    V& value() noexcept;
    iterator& operator++() noexcept;
    bool operator==(const iterator&) const noexcept;
    bool operator!=(const iterator&) const noexcept;
};
```

**描述**：可变迭代器,遍历所有非空槽位。

**访问**：
- `key()`：返回键的引用（可修改,但不推荐）
- `value()`：返回值的引用

**迭代**：
- `operator++()`：移动到下一个非空槽位
- 自动跳过空槽位和已删除槽位

**示例**：

```cpp
HashMap<int, std::string> map;
map.insert(1, "one");
map.insert(2, "two");

for (auto it = map.begin(); it != map.end(); ++it) {
    std::cout << it.key() << ": " << it.value() << "\n";
}
```

---

## 构造函数

### 默认构造

```cpp
explicit HashMap(size_type init_cap = 0,
                 IAllocator* alloc = default_allocator());
```

**描述**：构造空哈希表,可选预分配容量。

**参数**：
- `init_cap`：初始容量（实际桶数会向上取到 2 的幂次）
- `alloc`：分配器指针

**后置条件**：
- `size() == 0`
- `bucket_count() >= init_cap`（若 `init_cap > 0`）

**时间复杂度**：
- `init_cap == 0`：O(1)
- `init_cap > 0`：O(bucket_count)

**示例**：

```cpp
HashMap<int, std::string> map1;  // 空表,首次插入时分配
HashMap<int, std::string> map2{100};  // 预分配至少 100 个桶
```

---

### 拷贝构造

```cpp
HashMap(const HashMap& other);
```

**描述**：深拷贝另一个哈希表的所有键值对。

**参数**：
- `other`：被拷贝的哈希表

**后置条件**：
- `size() == other.size()`
- 包含 `other` 的所有键值对（顺序可能不同）

**约束**：
- `K` 必须可拷贝构造
- `V` 必须可拷贝构造

**时间复杂度**：O(other.bucket_count())

**示例**：

```cpp
HashMap<int, std::string> map1;
map1.insert(1, "one");
HashMap<int, std::string> map2 = map1;  // 深拷贝
```

---

### 移动构造

```cpp
HashMap(HashMap&& other) noexcept;
```

**描述**：转移另一个哈希表的所有权,`other` 变为空表。

**参数**：
- `other`：被移动的哈希表

**后置条件**：
- 当前哈希表接管 `other` 的所有资源
- `other.size() == 0`
- `other.bucket_count() == 0`

**时间复杂度**：O(1)

**示例**：

```cpp
HashMap<int, std::string> map1;
map1.insert(1, "one");
HashMap<int, std::string> map2 = std::move(map1);  // 转移所有权
```

---

## 容量

### empty / size / bucket_count

```cpp
bool empty() const noexcept;
size_type size() const noexcept;
size_type bucket_count() const noexcept;
```

**描述**：查询哈希表状态。

**返回值**：
- `empty()`：是否为空（等价于 `size() == 0`）
- `size()`：当前键值对数量
- `bucket_count()`：桶数量（槽位数组长度）

**时间复杂度**：O(1)

**示例**：

```cpp
HashMap<int, std::string> map;
assert(map.empty());
map.insert(1, "one");
assert(map.size() == 1);
```

---

### load_factor

```cpp
double load_factor() const noexcept;
```

**描述**：返回当前负载因子（`size() / bucket_count()`）。

**返回值**：负载因子（0.0 到 1.0）

**说明**：
- 负载因子超过 0.75 时,下次插入会触发扩容
- 保持较低负载因子可提高查找性能

**时间复杂度**：O(1)

**示例**：

```cpp
HashMap<int, int> map{100};
for (int i = 0; i < 50; ++i) {
    map.insert(i, i * 2);
}
std::cout << map.load_factor();  // ~0.5
```

---

### reserve

```cpp
void reserve(size_type count);
```

**描述**：预分配足够容纳 `count` 个元素的桶数（考虑负载因子）。

**参数**：
- `count`：期望容纳的元素数量

**行为**：
- 计算所需桶数：`buckets = count / 0.75 + 1`
- 向上取到 2 的幂次
- 重新哈希所有现有元素

**迭代器失效**：所有迭代器失效

**时间复杂度**：O(size() + new_bucket_count)

**示例**：

```cpp
HashMap<int, std::string> map;
map.reserve(1000);  // 预分配,避免多次扩容
for (int i = 0; i < 1000; ++i) {
    map.insert(i, "value");  // 无重新哈希
}
```

---

### clear

```cpp
void clear() noexcept;
```

**描述**：清空所有键值对（不释放内存）。

**后置条件**：
- `size() == 0`
- `bucket_count()` 不变

**迭代器失效**：所有迭代器失效

**时间复杂度**：O(bucket_count())

**示例**：

```cpp
HashMap<int, std::string> map;
map.insert(1, "one");
map.clear();
assert(map.empty());
```

---

## 查找

### find

```cpp
iterator find(const K& key) noexcept;
const_iterator find(const K& key) const noexcept;
```

**描述**：查找指定键,返回指向对应元素的迭代器。

**参数**：
- `key`：要查找的键

**返回值**：
- 找到：指向该键值对的迭代器
- 未找到：`end()`

**时间复杂度**：
- 平均：O(1)
- 最坏：O(n)（退化为线性探测）

**示例**：

```cpp
HashMap<int, std::string> map;
map.insert(1, "one");

auto it = map.find(1);
if (it != map.end()) {
    std::cout << it.value();  // "one"
}
```

---

### contains

```cpp
bool contains(const K& key) const noexcept;
```

**描述**：检查是否包含指定键。

**参数**：
- `key`：要查找的键

**返回值**：
- `true`：包含该键
- `false`：不包含

**时间复杂度**：平均 O(1)

**示例**：

```cpp
HashMap<int, std::string> map;
map.insert(1, "one");
assert(map.contains(1));
assert(!map.contains(2));
```

---

### try_get

```cpp
V* try_get(const K& key) noexcept;
const V* try_get(const K& key) const noexcept;
```

**描述**：查找指定键,返回指向值的指针（不存在时返回 `nullptr`）。

**参数**：
- `key`：要查找的键

**返回值**：
- 找到：指向值的指针
- 未找到：`nullptr`

**时间复杂度**：平均 O(1)

**示例**：

```cpp
HashMap<int, std::string> map;
map.insert(1, "one");

if (std::string* val = map.try_get(1)) {
    std::cout << *val;  // "one"
}
if (map.try_get(999) == nullptr) {
    std::cout << "不存在";
}
```

---

## 插入/更新

### insert

```cpp
bool insert(const K& key, const V& value);
bool insert(const K& key, V&& value);
```

**描述**：插入键值对（若键已存在则不覆盖）。

**参数**：
- `key`：键
- `value`：值（拷贝或移动）

**返回值**：
- `true`：插入成功（键不存在）
- `false`：键已存在,未插入

**行为**：
- 若负载因子超过 0.75,先扩容再插入
- 使用 Robin Hood 策略插入

**迭代器失效**：若发生扩容,所有迭代器失效

**时间复杂度**：
- 平均：O(1)
- 扩容时：O(size())

**示例**：

```cpp
HashMap<int, std::string> map;
bool ok1 = map.insert(1, "one");    // true
bool ok2 = map.insert(1, "ONE");    // false (键已存在)
assert(map[1] == "one");  // 保持原值
```

---

### emplace

```cpp
template<typename... Args>
bool emplace(const K& key, Args&&... args);
```

**描述**：原位构造值并插入（若键已存在则不覆盖）。

**参数**：
- `key`：键
- `args`：转发给 `V` 的构造函数的参数包

**返回值**：
- `true`：插入成功
- `false`：键已存在

**优势**：避免临时对象构造

**时间复杂度**：平均 O(1)

**示例**：

```cpp
HashMap<int, std::vector<int>> map;
map.emplace(1, 10, 42);  // 直接构造 vector<int>(10, 42)
```

---

### operator[]

```cpp
V& operator[](const K& key);
```

**描述**：访问或插入键对应的值（若键不存在则值初始化）。

**参数**：
- `key`：键

**返回值**：对值的引用

**行为**：
- 若键存在：返回对应值的引用
- 若键不存在：插入 `{key, V{}}`,返回新值的引用

**约束**：`V` 必须可默认构造

**时间复杂度**：平均 O(1)

**示例**：

```cpp
HashMap<int, std::string> map;
map[1] = "one";  // 插入
map[1] = "ONE";  // 更新
std::string& val = map[2];  // 插入 {2, ""},返回引用
val = "two";
```

---

## 删除

### erase (按键)

```cpp
bool erase(const K& key) noexcept;
```

**描述**：删除指定键的键值对。

**参数**：
- `key`：要删除的键

**返回值**：
- `true`：删除成功（键存在）
- `false`：键不存在

**行为**：
- 将槽位标记为已删除（tombstone）
- 不触发收缩

**迭代器失效**：指向被删除元素的迭代器失效

**时间复杂度**：平均 O(1)

**示例**：

```cpp
HashMap<int, std::string> map;
map.insert(1, "one");
bool ok = map.erase(1);  // true
assert(!map.contains(1));
```

---

### erase (按迭代器)

```cpp
iterator erase(iterator pos) noexcept;
```

**描述**：删除迭代器指向的元素。

**参数**：
- `pos`：指向要删除元素的迭代器

**返回值**：指向下一个元素的迭代器

**前置条件**：`pos != end()`

**时间复杂度**：O(1)

**示例**：

```cpp
HashMap<int, std::string> map;
map.insert(1, "one");
auto it = map.find(1);
it = map.erase(it);  // 返回下一个元素
```

---

## 工具

### swap

```cpp
void swap(HashMap& other) noexcept;
```

**描述**：交换两个哈希表的内容（O(1)）。

**参数**：
- `other`：要交换的哈希表

**后置条件**：两个哈希表的内容完全交换

**迭代器失效**：所有迭代器保持有效,但指向另一个容器

**时间复杂度**：O(1)

**示例**：

```cpp
HashMap<int, std::string> map1, map2;
map1.insert(1, "one");
map2.insert(2, "two");
map1.swap(map2);
// map1 包含 {2: "two"}
// map2 包含 {1: "one"}
```

---

## 性能分析

### Robin Hood 线性探测原理

**传统线性探测问题**：
- 聚集效应：连续的哈希冲突导致探测链越来越长
- 最坏情况：某些键的探测链可能达到 O(n)

**Robin Hood 策略**：
1. 每个槽位记录 **PSL**（Probe Sequence Length）：当前元素距离理想位置的距离
2. 插入时遇到 PSL 更小的元素,则"劫富济贫"：
   - 将当前元素插入此槽位
   - 被替换的元素继续向后探测
3. 效果：平衡所有元素的 PSL,降低最坏情况

**示例**：

```
初始状态（空表，容量 8）：
Index:  0   1   2   3   4   5   6   7
Slot:   -   -   -   -   -   -   -   -
PSL:    -   -   -   -   -   -   -   -

插入 key=10 (hash=2):
Index:  0   1   2   3   4   5   6   7
Slot:   -   -  10   -   -   -   -   -
PSL:    -   -   0   -   -   -   -   -

插入 key=18 (hash=2, 冲突):
Index:  0   1   2   3   4   5   6   7
Slot:   -   -  10  18   -   -   -   -
PSL:    -   -   0   1   -   -   -   -

插入 key=26 (hash=2, 冲突):
Index:  0   1   2   3   4   5   6   7
Slot:   -   -  10  18  26   -   -   -
PSL:    -   -   0   1   2   -   -   -

插入 key=11 (hash=3, 冲突, PSL=0 < 1):
  - 11 替换掉 18 (PSL=1 > 0)
  - 18 继续向后探测
Index:  0   1   2   3   4   5   6   7
Slot:   -   -  10  11  18  26   -   -
PSL:    -   -   0   0   2   3   -   -
```

### 内存布局

```cpp
template<typename K, typename V>
struct HashSlot {
    uint8_t psl;    // Probe Sequence Length (0-255)
    bool occupied;  // 是否已占用
    K key;
    V value;
};

class HashMap<K, V> {
    IAllocator* alloc_;  // 8 字节
    Slot* slots_;        // 8 字节
    size_t bucket_count_; // 8 字节
    size_t size_;        // 8 字节
    THash hash_;         // 通常 1 字节（空基类优化）
    TEqual equal_;       // 通常 1 字节（空基类优化）
};
// sizeof(HashMap) ≈ 32-40 字节
```

### 操作时间复杂度

| 操作 | 平均 | 最坏 | 说明 |
|------|------|------|------|
| `find` | O(1) | O(n) | Robin Hood 降低最坏情况概率 |
| `insert` | O(1) | O(n) | 可能触发扩容 |
| `erase` | O(1) | O(n) | 标记为墓碑 |
| `operator[]` | O(1) | O(n) | 查找 + 可能插入 |
| `clear` | O(bucket_count) | O(bucket_count) | 逐槽位析构 |

### 负载因子影响

| 负载因子 | 平均 PSL | 最大 PSL | 缓存命中率 |
|---------|---------|---------|-----------|
| 0.5 | ~1.5 | ~5 | 高 |
| 0.75 | ~2.5 | ~8 | 中 |
| 0.9 | ~5.0 | ~20 | 低 |

MineFramework 默认使用 0.75 以平衡内存使用和性能。

---

## 使用场景

### 1. 替代 std::unordered_map

```cpp
// ❌ 不推荐：跨 DLL 边界传递 std::unordered_map
class MINE_API Widget {
public:
    std::unordered_map<int, std::string> get_config();  // ABI 不稳定
};

// ✅ 推荐：使用 HashMap 或返回迭代器
class MINE_API Widget {
public:
    const HashMap<int, std::string>& get_config() const;
private:
    HashMap<int, std::string> config_;
};
```

---

### 2. 配置字典

```cpp
HashMap<StringView, Variant> config;
config.insert("width", 1920);
config.insert("height", 1080);
config.insert("fullscreen", true);

if (auto* val = config.try_get("width")) {
    int width = val->get<int>();
}
```

---

### 3. 对象池/缓存

```cpp
HashMap<int, std::unique_ptr<Texture>> texture_cache;

Texture* get_texture(int id) {
    if (auto* ptr = texture_cache.try_get(id)) {
        return ptr->get();
    }
    auto tex = load_texture(id);
    texture_cache.insert(id, std::move(tex));
    return texture_cache[id].get();
}
```

---

## 最佳实践

### 1. 预分配容量避免扩容

```cpp
// ✅ 推荐：已知大小时预分配
HashMap<int, std::string> map;
map.reserve(1000);
for (int i = 0; i < 1000; ++i) {
    map.insert(i, "value");  // 无重新哈希
}

// ❌ 不推荐：多次扩容
HashMap<int, std::string> map2;
for (int i = 0; i < 1000; ++i) {
    map2.insert(i, "value");  // log(n) 次扩容
}
```

---

### 2. 使用 try_get 避免不必要的插入

```cpp
// ✅ 推荐：只查找
if (auto* val = map.try_get(key)) {
    std::cout << *val;
}

// ❌ 不推荐：operator[] 会插入默认值
if (map.contains(key)) {
    std::cout << map[key];  // 多余的查找
}
```

---

### 3. 选择合适的哈希函数

```cpp
// 内置类型：使用默认 Hash<K>
HashMap<int, std::string> map1;

// 自定义类型：特化 Hash
struct Point { int x, y; };

template<>
struct Hash<Point> {
    size_t operator()(const Point& p) const noexcept {
        return hash_combine(std::hash<int>{}(p.x),
                           std::hash<int>{}(p.y));
    }
};

HashMap<Point, std::string> map2;
```

---

## 完整示例

### 示例：单词计数

```cpp
#include <mine/containers/HashMap.h>
#include <mine/containers/Vector.h>

using namespace mine::containers;

void word_count_example() {
    HashMap<std::string, int> word_count;
    
    Vector<std::string> words = {"hello", "world", "hello", "mine"};
    
    for (const auto& word : words) {
        word_count[word]++;  // 自动插入并递增
    }
    
    // 遍历统计结果
    for (auto it = word_count.begin(); it != word_count.end(); ++it) {
        std::cout << it.key() << ": " << it.value() << "\n";
    }
    // 输出：
    // hello: 2
    // world: 1
    // mine: 1
}
```

---

## 总结

`HashMap<K, V>` 是 MineFramework 中的高性能哈希表,具备以下优势:

1. **Robin Hood 优化**：平衡探测链,降低最坏情况
2. **开放寻址**：缓存友好,无额外节点分配
3. **ABI 稳定**：跨 DLL 边界安全
4. **可替换分配器**：支持自定义内存管理

在需要键值对存储时,优先使用 `HashMap` 而非 `std::unordered_map`,这将确保代码在所有平台上正确运行并获得更好的性能。
