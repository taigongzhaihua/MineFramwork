# IntrusiveList 详细接口文档

## 概述

`IntrusiveList<T>` 是侵入式双向链表,要求节点类型 `T` 继承自 `IntrusiveListNode<T>`。与传统链表（如 `std::list`）不同,侵入式链表不为节点分配额外内存,链表信息直接嵌入对象本身。

### 核心特性

- **零额外分配**：链表不管理节点内存,节点由对象本身持有
- **O(1) 自移除**：对象持有 Hook,可直接从链表中移除自身
- **多链表支持**：对象可同时存在于多个链表（使用不同 Hook 成员）
- **哨兵节点**：使用哨兵简化边界处理,空链表仍有 head/tail 节点
- **类型安全**：通过 CRTP 确保 Hook 与容器类型匹配

### 设计动机

传统链表（如 `std::list<T*>`）存在以下问题:
1. **额外分配**：每个元素需要分配链表节点包装
2. **间接访问**：从节点到对象需要额外解引用
3. **无法自移除**：对象无法主动从链表中脱离

侵入式链表通过将 Hook 嵌入对象解决这些问题:
```cpp
// 传统链表：需要为链表节点分配内存
std::list<Widget*> list;
list.push_back(new Widget);  // malloc(sizeof(list_node) + 24)

// 侵入式链表：无额外分配
struct Widget : IntrusiveListNode<Widget> { ... };
IntrusiveList<Widget> list;
list.push_back(w);  // 无 malloc,直接修改 w 的 prev_/next_
```

---

## 类定义

### IntrusiveListNode<T>

```cpp
template<typename T>
class IntrusiveListNode {
public:
    IntrusiveListNode() noexcept = default;
    
    // 不可拷贝/移动（防止 Hook 意外复制）
    IntrusiveListNode(const IntrusiveListNode&) = delete;
    IntrusiveListNode& operator=(const IntrusiveListNode&) = delete;
    
    ~IntrusiveListNode() noexcept;  // 自动从链表中脱离

    bool is_linked() const noexcept;  // 是否已链入某个链表
    void unlink() noexcept;           // 从链表中移除自身（O(1)）
};
```

---

### IntrusiveList<T>

```cpp
template<typename T>
    requires std::is_base_of_v<IntrusiveListNode<T>, T>
class IntrusiveList {
public:
    using value_type = T;
    using size_type  = size_t;

    class iterator;
    class const_iterator;

    // 构造/析构
    IntrusiveList() noexcept;
    IntrusiveList(const IntrusiveList&) = delete;  // 不可拷贝
    IntrusiveList(IntrusiveList&& other) noexcept;
    ~IntrusiveList() noexcept;

    // 赋值
    IntrusiveList& operator=(const IntrusiveList&) = delete;
    IntrusiveList& operator=(IntrusiveList&& other) noexcept;

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

    // 元素访问
    T& front() noexcept;
    const T& front() const noexcept;
    T& back() noexcept;
    const T& back() const noexcept;

    // 修改器
    void push_front(T& item) noexcept;
    void push_back(T& item) noexcept;
    void pop_front() noexcept;
    void pop_back() noexcept;
    iterator insert(const_iterator pos, T& item) noexcept;
    iterator erase(const_iterator pos) noexcept;
    void clear() noexcept;
    void swap(IntrusiveList& other) noexcept;
};
```

---

## IntrusiveListNode 方法

### is_linked

```cpp
bool is_linked() const noexcept;
```

**描述**：判断此节点是否已链入某个链表。

**返回值**：
- `true`：已链入链表
- `false`：未链入任何链表

**时间复杂度**：O(1)

**示例**：

```cpp
struct Widget : IntrusiveListNode<Widget> {
    int id;
};

Widget w{42};
assert(!w.is_linked());  // 未链入

IntrusiveList<Widget> list;
list.push_back(w);
assert(w.is_linked());  // 已链入
```

---

### unlink

```cpp
void unlink() noexcept;
```

**描述**：将自身从所在链表中移除（O(1)）。

**前置条件**：`is_linked() == true`

**后置条件**：`is_linked() == false`

**时间复杂度**：O(1)

**示例**：

```cpp
Widget w1{1}, w2{2}, w3{3};
IntrusiveList<Widget> list;
list.push_back(w1);
list.push_back(w2);
list.push_back(w3);

w2.unlink();  // O(1) 移除 w2
assert(list.size() == 2);  // 仅剩 w1, w3
```

---

### 析构函数

```cpp
~IntrusiveListNode() noexcept;
```

**描述**：析构时自动从链表中脱离（若尚在链表中）。

**行为**：
- 若 `is_linked()`：调用 `unlink()`
- 否则：无操作

**示例**：

```cpp
IntrusiveList<Widget> list;
{
    Widget w{42};
    list.push_back(w);
}  // w 析构,自动从 list 中移除
```

---

## IntrusiveList 构造/赋值

### 默认构造

```cpp
IntrusiveList() noexcept;
```

**描述**：构造空链表。

**后置条件**：
- `empty() == true`
- `size() == 0`

**时间复杂度**：O(1)

---

### 移动构造

```cpp
IntrusiveList(IntrusiveList&& other) noexcept;
```

**描述**：转移另一个链表的所有节点。

**参数**：
- `other`：被移动的链表

**后置条件**：
- 当前链表包含 `other` 的所有节点
- `other.empty() == true`

**时间复杂度**：O(1)

**示例**：

```cpp
IntrusiveList<Widget> list1;
list1.push_back(w1);
IntrusiveList<Widget> list2 = std::move(list1);
assert(list1.empty());
assert(list2.size() == 1);
```

---

### 析构函数

```cpp
~IntrusiveList() noexcept;
```

**描述**：清空链表（所有节点从链表中脱离）。

**注意**：不析构节点对象本身,仅断开链接

**时间复杂度**：O(size())

---

## 迭代器

### iterator 类

```cpp
class iterator {
public:
    T& operator*() noexcept;
    T* operator->() noexcept;
    iterator& operator++() noexcept;
    iterator& operator--() noexcept;
    bool operator==(const iterator&) const noexcept;
    bool operator!=(const iterator&) const noexcept;
};
```

**描述**：双向迭代器。

**示例**：

```cpp
IntrusiveList<Widget> list;
for (Widget& w : list) {
    std::cout << w.id << "\n";
}
```

---

## 容量

### empty / size

```cpp
bool empty() const noexcept;
size_type size() const noexcept;
```

**描述**：查询链表状态。

**返回值**：
- `empty()`：是否为空
- `size()`：节点数量

**时间复杂度**：
- `empty()`：O(1)
- `size()`：O(n)（需要遍历统计）

**示例**：

```cpp
IntrusiveList<Widget> list;
assert(list.empty());
list.push_back(w);
assert(list.size() == 1);
```

---

## 元素访问

### front / back

```cpp
T& front() noexcept;
const T& front() const noexcept;
T& back() noexcept;
const T& back() const noexcept;
```

**描述**：访问首/尾节点。

**前置条件**：`!empty()`

**时间复杂度**：O(1)

**示例**：

```cpp
IntrusiveList<Widget> list;
list.push_back(w1);
list.push_back(w2);
assert(&list.front() == &w1);
assert(&list.back() == &w2);
```

---

## 修改器

### push_front / push_back

```cpp
void push_front(T& item) noexcept;
void push_back(T& item) noexcept;
```

**描述**：在首/尾部插入节点。

**参数**：
- `item`：要插入的节点（引用）

**前置条件**：`!item.is_linked()`（节点未链入任何链表）

**后置条件**：`item.is_linked() == true`

**时间复杂度**：O(1)

**示例**：

```cpp
IntrusiveList<Widget> list;
Widget w1{1}, w2{2};
list.push_back(w1);
list.push_front(w2);  // list: [w2, w1]
```

---

### pop_front / pop_back

```cpp
void pop_front() noexcept;
void pop_back() noexcept;
```

**描述**：移除首/尾节点。

**前置条件**：`!empty()`

**后置条件**：被移除节点的 `is_linked() == false`

**时间复杂度**：O(1)

**示例**：

```cpp
IntrusiveList<Widget> list;
list.push_back(w1);
list.push_back(w2);
list.pop_front();  // 移除 w1
assert(&list.front() == &w2);
```

---

### insert

```cpp
iterator insert(const_iterator pos, T& item) noexcept;
```

**描述**：在 `pos` 之前插入节点。

**参数**：
- `pos`：插入位置
- `item`：要插入的节点

**返回值**：指向插入节点的迭代器

**前置条件**：`!item.is_linked()`

**时间复杂度**：O(1)

**示例**：

```cpp
IntrusiveList<Widget> list;
list.push_back(w1);
list.push_back(w3);
list.insert(list.end(), w2);  // [w1, w3, w2]
```

---

### erase

```cpp
iterator erase(const_iterator pos) noexcept;
```

**描述**：移除指定位置的节点。

**参数**：
- `pos`：要移除的节点

**返回值**：指向下一个节点的迭代器

**前置条件**：`pos != end()`

**时间复杂度**：O(1)

**示例**：

```cpp
IntrusiveList<Widget> list;
list.push_back(w1);
auto it = list.begin();
list.erase(it);
assert(list.empty());
```

---

### clear

```cpp
void clear() noexcept;
```

**描述**：清空链表（所有节点脱离）。

**后置条件**：
- `empty() == true`
- 所有之前的节点 `is_linked() == false`

**时间复杂度**：O(size())

---

### swap

```cpp
void swap(IntrusiveList& other) noexcept;
```

**描述**：交换两个链表的内容（O(1)）。

**时间复杂度**：O(1)

---

## 使用场景

### 1. 对象生命周期管理

```cpp
class EventManager {
    IntrusiveList<EventHandler> handlers_;
public:
    void register_handler(EventHandler& handler) {
        handlers_.push_back(handler);
    }
    
    void fire_event(const Event& evt) {
        for (EventHandler& h : handlers_) {
            h.handle(evt);
        }
    }
};

class EventHandler : public IntrusiveListNode<EventHandler> {
public:
    ~EventHandler() {
        // 自动从所有注册的 EventManager 中移除
    }
};
```

---

### 2. 多链表成员

```cpp
struct Task : IntrusiveListNode<Task> {
    int priority;
    IntrusiveListNode<Task> priority_hook;  // 第二个 Hook
};

class TaskScheduler {
    IntrusiveList<Task> all_tasks_;    // 所有任务
    IntrusiveList<Task> ready_tasks_;  // 就绪队列（使用 priority_hook）
};
```

---

### 3. LRU 缓存

```cpp
struct CacheEntry : IntrusiveListNode<CacheEntry> {
    int key;
    std::string value;
};

class LRUCache {
    IntrusiveList<CacheEntry> lru_list_;
    HashMap<int, CacheEntry*> map_;
    
public:
    void access(int key) {
        auto* entry = map_[key];
        entry->unlink();  // O(1) 从 LRU 列表中移除
        lru_list_.push_front(*entry);  // 移到最前
    }
};
```

---

## 最佳实践

### 1. 使用 CRTP 确保类型安全

```cpp
// ✅ 推荐：继承 IntrusiveListNode<Self>
struct Widget : IntrusiveListNode<Widget> { ... };

// ❌ 错误：类型不匹配
struct Gadget : IntrusiveListNode<Widget> { ... };  // 编译错误
```

---

### 2. 对象析构时自动脱离

```cpp
// ✅ 推荐：利用自动脱离特性
{
    Widget w;
    list.push_back(w);
}  // w 析构,自动从 list 中移除

// ❌ 不推荐：手动移除
{
    Widget w;
    list.push_back(w);
    list.pop_back();  // 多余
}
```

---

### 3. 避免在多个链表间移动未脱离的节点

```cpp
IntrusiveList<Widget> list1, list2;
Widget w;

list1.push_back(w);
list2.push_back(w);  // ❌ 错误：w 已在 list1 中

// ✅ 正确：先脱离
w.unlink();
list2.push_back(w);
```

---

## 完整示例

### 示例：窗口 Z-Order 管理

```cpp
#include <mine/containers/IntrusiveList.h>

using namespace mine::containers;

class Window : public IntrusiveListNode<Window> {
public:
    std::string title;
    bool visible = true;
    
    Window(std::string t) : title{std::move(t)} {}
    
    ~Window() {
        // 自动从 WindowManager 的 Z-Order 列表中移除
    }
};

class WindowManager {
    IntrusiveList<Window> z_order_;  // 前到后顺序
    
public:
    void add_window(Window& win) {
        z_order_.push_back(win);  // 新窗口在最后（最前）
    }
    
    void bring_to_front(Window& win) {
        win.unlink();  // O(1) 从当前位置移除
        z_order_.push_back(win);  // 移到最前
    }
    
    void paint() {
        // 从后往前绘制（后面的窗口在下层）
        for (auto it = z_order_.end(); it != z_order_.begin(); ) {
            --it;
            if (it->visible) {
                draw_window(*it);
            }
        }
    }
};

void example() {
    WindowManager wm;
    
    Window win1{"Editor"};
    Window win2{"Console"};
    Window win3{"Inspector"};
    
    wm.add_window(win1);
    wm.add_window(win2);
    wm.add_window(win3);
    
    // 将 win1 提到最前
    wm.bring_to_front(win1);
    
    wm.paint();  // 绘制顺序: win2, win3, win1
    
    // win1 析构时自动从 z_order_ 中移除
}
```

---

## 总结

`IntrusiveList<T>` 是侵入式双向链表,具备以下优势:

1. **零额外分配**：链表不管理节点内存
2. **O(1) 自移除**：对象可主动脱离链表
3. **多链表支持**：对象可同时存在于多个链表
4. **RAII 友好**：对象析构时自动脱离

在对象生命周期管理、LRU 缓存、窗口 Z-Order 等场景下,侵入式链表提供了比传统链表更高效的实现。
