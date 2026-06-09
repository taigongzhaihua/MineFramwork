# InlineString 详细接口文档

## 概述

`InlineString` 是带有 **Small String Optimization (SSO)** 的 UTF-8 字符串类型。它在字符串长度 ≤ 23 字节时完全使用内联存储（栈上或对象内），无需堆分配；超过阈值时自动切换到堆分配。

### 核心特性

- **小字符串优化**：23 字节以内完全内联,零堆分配
- **拥有式字符串**：拥有字符串数据,自动管理内存
- **C 字符串兼容**：始终以 `\0` 结尾,可传递给 C API
- **UTF-8 编码**：处理 UTF-8 字节序列（不做 Unicode 验证）
- **隐式转换 StringView**：可无缝传递给接受 `StringView` 的函数
- **可替换分配器**：堆分配使用 `IAllocator*`

### 设计动机

标准库 `std::string` 存在以下问题:
1. **ABI 不稳定**：不同标准库实现的 SSO 阈值不同（MSVC 15, libstdc++ 15, libc++ 22）
2. **异常依赖**：MineFramework 禁用异常,`std::string` 的错误处理不适用
3. **跨 DLL 传递不安全**：跨边界传递可能导致内存管理错误

`InlineString` 通过统一的 24 字节布局和 ABI 稳定的接口解决这些问题:
- 所有平台统一 23 字节 SSO 阈值
- 分配失败通过断言终止
- 跨 DLL 边界安全

---

## 内存布局

```cpp
class InlineString {
    IAllocator* alloc_;  // 8 字节（不计入 24 字节布局）
    
    union {
        // 内联模式（≤ 23 字节）
        struct {
            char buf[23];  // 内联缓冲区
            uint8_t len;   // 长度（0-23）
        } inline_;
        
        // 堆模式（> 23 字节）
        struct {
            char* ptr;     // 8 字节：堆指针
            size_t size;   // 8 字节：当前长度
            size_t cap;    // 8 字节：容量（最高位为 1 表示堆模式）
        } heap_;
    };
};
// sizeof(InlineString) = 8 + 24 = 32 字节
```

### 模式判断

通过 `heap_.cap` 的最高位判断当前模式:
- **内联模式**：`inline_.len` ≤ 23
- **堆模式**：`heap_.cap` 最高位为 1

---

## 类定义

```cpp
class InlineString {
public:
    using size_type      = size_t;
    using iterator       = char*;
    using const_iterator = const char*;
    
    static constexpr size_type kInlineCap = 23;

    // 构造/析构
    explicit InlineString(IAllocator* alloc = default_allocator()) noexcept;
    InlineString(const char* str, IAllocator* alloc = default_allocator());
    /*implicit*/ InlineString(StringView sv, IAllocator* alloc = default_allocator());
    InlineString(const char* data, size_type len, IAllocator* alloc = default_allocator());
    InlineString(const InlineString& other);
    InlineString(InlineString&& other) noexcept;
    ~InlineString() noexcept;

    // 赋值
    InlineString& operator=(const InlineString& other);
    InlineString& operator=(InlineString&& other) noexcept;
    InlineString& operator=(const char* str);
    InlineString& operator=(StringView sv);

    // 元素访问
    char& operator[](size_type idx) noexcept;
    const char& operator[](size_type idx) const noexcept;
    char& at(size_type idx) noexcept;
    const char& at(size_type idx) const noexcept;
    char* data() noexcept;
    const char* data() const noexcept;
    const char* c_str() const noexcept;

    // 容量
    bool empty() const noexcept;
    size_type size() const noexcept;
    size_type length() const noexcept;
    size_type capacity() const noexcept;
    bool is_heap() const noexcept;
    void reserve(size_type new_cap);
    void shrink_to_fit();

    // 修改器
    void clear() noexcept;
    void push_back(char ch);
    void pop_back() noexcept;
    void resize(size_type new_size, char ch = '\0');
    void assign(const char* data, size_type len);
    void append(const char* data, size_type len);
    void append(StringView sv);
    InlineString& operator+=(char ch);
    InlineString& operator+=(const char* str);
    InlineString& operator+=(StringView sv);

    // 转换
    /*implicit*/ operator StringView() const noexcept;
    StringView view() const noexcept;
    
    // 比较
    bool operator==(const InlineString& other) const noexcept;
    bool operator!=(const InlineString& other) const noexcept;
    bool operator==(StringView sv) const noexcept;
    bool operator!=(StringView sv) const noexcept;
};
```

---

## 构造函数

### 默认构造

```cpp
explicit InlineString(IAllocator* alloc = default_allocator()) noexcept;
```

**描述**：构造空字符串（内联模式）。

**后置条件**：
- `size() == 0`
- `c_str()[0] == '\0'`
- `!is_heap()`

**时间复杂度**：O(1)

**示例**：

```cpp
InlineString s;  // ""
assert(s.empty());
assert(!s.is_heap());
```

---

### 从 C 字符串构造

```cpp
InlineString(const char* str, IAllocator* alloc = default_allocator());
```

**描述**：从 C 字符串构造。

**参数**：
- `str`：C 字符串指针（可为 `nullptr`,视为空字符串）

**时间复杂度**：O(len)

**示例**：

```cpp
InlineString s1{"hello"};  // 内联存储
InlineString s2{"This is a very long string that exceeds 23 bytes"};  // 堆分配
```

---

### 从 StringView 构造

```cpp
/*implicit*/ InlineString(StringView sv, IAllocator* alloc = default_allocator());
```

**描述**：从 `StringView` 构造（隐式转换）。

**参数**：
- `sv`：字符串视图

**时间复杂度**：O(sv.size())

**示例**：

```cpp
StringView sv = "hello";
InlineString s = sv;  // 隐式转换
```

---

### 从字节区间构造

```cpp
InlineString(const char* data, size_type len, IAllocator* alloc = default_allocator());
```

**描述**：从字节区间构造（可包含 `\0`）。

**参数**：
- `data`：字节数据指针
- `len`：字节长度

**时间复杂度**：O(len)

**示例**：

```cpp
const char* data = "hello\0world";
InlineString s{data, 11};  // 包含内嵌 \0
assert(s.size() == 11);
```

---

### 拷贝构造

```cpp
InlineString(const InlineString& other);
```

**描述**：深拷贝另一个字符串。

**时间复杂度**：O(other.size())

**示例**：

```cpp
InlineString s1{"hello"};
InlineString s2 = s1;  // 深拷贝
s2[0] = 'H';  // 不影响 s1
```

---

### 移动构造

```cpp
InlineString(InlineString&& other) noexcept;
```

**描述**：转移另一个字符串的所有权。

**行为**：
- 若 `other.is_heap()`：转移堆指针（O(1)）
- 否则：拷贝内联数据（O(kInlineCap)）

**时间复杂度**：
- 堆模式：O(1)
- 内联模式：O(kInlineCap)

**示例**：

```cpp
InlineString s1{"hello"};
InlineString s2 = std::move(s1);  // s1 变为空
```

---

## 元素访问

### operator[] / at

```cpp
char& operator[](size_type idx) noexcept;
const char& operator[](size_type idx) const noexcept;
char& at(size_type idx) noexcept;
const char& at(size_type idx) const noexcept;
```

**描述**：访问指定索引的字符。

**参数**：
- `idx`：字符索引（`0 <= idx < size()`）

**返回值**：对字符的引用

**区别**：
- `operator[]`：Debug 模式下范围检查
- `at()`：Release 模式也检查范围

**时间复杂度**：O(1)

**示例**：

```cpp
InlineString s{"hello"};
char ch = s[1];  // 'e'
s[0] = 'H';  // "Hello"
```

---

### data / c_str

```cpp
char* data() noexcept;
const char* data() const noexcept;
const char* c_str() const noexcept;
```

**描述**：返回底层 C 字符串指针。

**返回值**：指向以 `\0` 结尾的字符串的指针

**区别**：`c_str()` 是 `data()` 的 const 版本,语义上更明确

**时间复杂度**：O(1)

**示例**：

```cpp
InlineString s{"hello"};
const char* ptr = s.c_str();
printf("%s", ptr);  // 与 C API 交互

void legacy_func(const char* str);
legacy_func(s.c_str());
```

---

## 容量

### empty / size / length

```cpp
bool empty() const noexcept;
size_type size() const noexcept;
size_type length() const noexcept;
```

**描述**：查询字符串状态。

**返回值**：
- `empty()`：是否为空
- `size()`：字节长度（不含终止符）
- `length()`：`size()` 的别名

**时间复杂度**：O(1)

**示例**：

```cpp
InlineString s{"hello"};
assert(!s.empty());
assert(s.size() == 5);
assert(s.length() == 5);
```

---

### capacity

```cpp
size_type capacity() const noexcept;
```

**描述**：返回当前容量（不含终止符）。

**返回值**：
- 内联模式：23
- 堆模式：已分配的容量

**时间复杂度**：O(1)

**示例**：

```cpp
InlineString s{"hello"};
assert(s.capacity() == 23);  // 内联模式

s.reserve(100);
assert(s.capacity() >= 100);  // 堆模式
```

---

### is_heap

```cpp
bool is_heap() const noexcept;
```

**描述**：判断当前是否使用堆分配。

**返回值**：
- `true`：堆模式
- `false`：内联模式

**时间复杂度**：O(1)

**示例**：

```cpp
InlineString s1{"short"};
assert(!s1.is_heap());  // 内联

InlineString s2{"This is a very long string that exceeds 23 bytes"};
assert(s2.is_heap());  // 堆分配
```

---

### reserve

```cpp
void reserve(size_type new_cap);
```

**描述**：预分配容量。

**参数**：
- `new_cap`：期望容量

**行为**：
- 若 `new_cap <= kInlineCap`：无操作
- 否则：分配堆内存并拷贝数据

**时间复杂度**：
- 无分配：O(1)
- 有分配：O(size())

**示例**：

```cpp
InlineString s;
s.reserve(100);  // 预分配,避免多次扩容
for (int i = 0; i < 100; ++i) {
    s.push_back('a');  // 无重新分配
}
```

---

### shrink_to_fit

```cpp
void shrink_to_fit();
```

**描述**：释放多余容量。

**行为**：
- 若 `size() <= kInlineCap` 且当前在堆：移回内联存储
- 否则：收缩堆内存到 `size()`

**时间复杂度**：O(size())

**示例**：

```cpp
InlineString s(100, 'a');  // 堆分配
s.resize(5);
s.shrink_to_fit();  // 移回内联
assert(!s.is_heap());
```

---

## 修改器

### clear

```cpp
void clear() noexcept;
```

**描述**：清空字符串内容（不释放内存）。

**后置条件**：
- `size() == 0`
- `c_str()[0] == '\0'`
- `capacity()` 不变

**时间复杂度**：O(1)

**示例**：

```cpp
InlineString s{"hello"};
s.clear();
assert(s.empty());
```

---

### push_back / pop_back

```cpp
void push_back(char ch);
void pop_back() noexcept;
```

**描述**：在尾部追加/移除字符。

**参数**：
- `ch`：要追加的字符

**前置条件**：`pop_back` 要求 `!empty()`

**时间复杂度**：
- `push_back`：摊销 O(1)
- `pop_back`：O(1)

**示例**：

```cpp
InlineString s{"hello"};
s.push_back('!');  // "hello!"
s.pop_back();  // "hello"
```

---

### resize

```cpp
void resize(size_type new_size, char ch = '\0');
```

**描述**：调整字符串大小。

**参数**：
- `new_size`：新大小
- `ch`：填充字符（可选）

**行为**：
- 若 `new_size > size()`：追加 `ch`
- 若 `new_size < size()`：截断

**时间复杂度**：O(|new_size - size()|)

**示例**：

```cpp
InlineString s{"hello"};
s.resize(10, '.');  // "hello....."
s.resize(3);  // "hel"
```

---

### assign

```cpp
void assign(const char* data, size_type len);
```

**描述**：替换字符串内容。

**参数**：
- `data`：新数据指针
- `len`：新长度

**时间复杂度**：O(len)

**示例**：

```cpp
InlineString s{"hello"};
s.assign("world", 5);  // "world"
```

---

### append

```cpp
void append(const char* data, size_type len);
void append(StringView sv);
```

**描述**：在尾部追加数据。

**参数**：
- `data`：要追加的数据
- `len`：长度
- `sv`：字符串视图

**时间复杂度**：O(len)

**示例**：

```cpp
InlineString s{"hello"};
s.append(" world", 6);  // "hello world"
s.append(StringView{", bye"});  // "hello world, bye"
```

---

### operator+=

```cpp
InlineString& operator+=(char ch);
InlineString& operator+=(const char* str);
InlineString& operator+=(StringView sv);
```

**描述**：追加运算符。

**示例**：

```cpp
InlineString s{"hello"};
s += ' ';
s += "world";
s += StringView{"!"};  // "hello world!"
```

---

## 转换

### operator StringView

```cpp
/*implicit*/ operator StringView() const noexcept;
StringView view() const noexcept;
```

**描述**：隐式转换为 `StringView`。

**返回值**：指向当前字符串的非拥有视图

**时间复杂度**：O(1)

**示例**：

```cpp
InlineString s{"hello"};
StringView sv = s;  // 隐式转换

void process(StringView sv);
process(s);  // 自动转换
```

---

## 比较

### operator== / operator!=

```cpp
bool operator==(const InlineString& other) const noexcept;
bool operator!=(const InlineString& other) const noexcept;
bool operator==(StringView sv) const noexcept;
bool operator!=(StringView sv) const noexcept;
```

**描述**：字符串相等性比较。

**时间复杂度**：O(min(size(), other.size()))

**示例**：

```cpp
InlineString s1{"hello"};
InlineString s2{"hello"};
assert(s1 == s2);
assert(s1 == StringView{"hello"});
```

---

## 性能分析

### SSO 阈值选择

| 字符串长度 | InlineString (23B) | std::string (MSVC 15B) | 内存分配 |
|-----------|-------------------|----------------------|---------|
| "hello" (5B) | 内联 | 内联 | 0 次 malloc |
| "hello, world!" (13B) | 内联 | 内联 | 0 次 malloc |
| "hello, world, 12345!" (20B) | 内联 | 堆 | 0 vs 1 次 malloc |
| "This is a very long string" (27B) | 堆 | 堆 | 1 次 malloc |

### 内存占用对比

| 类型 | 空字符串 | 5 字节 | 20 字节 | 100 字节 |
|------|---------|-------|--------|---------|
| `InlineString` | 32B | 32B | 32B | 32B + 101B |
| `std::string` (MSVC) | 32B | 32B | 32B + 21B | 32B + 101B |
| `std::string` (libc++) | 24B | 24B | 24B | 24B + 101B |

### 操作时间复杂度

| 操作 | 内联模式 | 堆模式 |
|------|---------|-------|
| `push_back` | O(1) | 摊销 O(1) |
| `append` | O(len) | 摊销 O(len) |
| `resize` | O(|delta|) | O(|delta|) |
| `assign` | O(len) | O(len) |
| `c_str()` | O(1) | O(1) |

---

## 使用场景

### 1. 配置键值对

```cpp
HashMap<InlineString, Variant> config;
config.insert(InlineString{"width"}, 1920);  // 内联,无堆分配
config.insert(InlineString{"height"}, 1080);
```

---

### 2. 日志消息

```cpp
void log(StringView msg);

InlineString msg;
msg += "User ";
msg += username;  // 若总长度 ≤ 23,无堆分配
msg += " logged in";
log(msg);  // 隐式转换为 StringView
```

---

### 3. 路径拼接

```cpp
InlineString path = base_dir;
path += "/assets/";
path += filename;
if (path.is_heap()) {
    // 路径较长,已切换到堆
}
```

---

## 最佳实践

### 1. 已知字符串很长时使用 reserve

```cpp
// ✅ 推荐：预分配
InlineString s;
s.reserve(1000);
s.append(long_data);

// ❌ 不推荐：多次扩容
InlineString s2;
s2.append(long_data);  // 可能多次 realloc
```

---

### 2. 传递参数时使用 StringView

```cpp
// ✅ 推荐：接受 StringView
void process(StringView sv);

InlineString s{"hello"};
process(s);  // 隐式转换,零拷贝

// ❌ 不推荐：按值传递 InlineString
void process2(InlineString s);  // 拷贝开销
```

---

### 3. 需要拥有权时使用 InlineString

```cpp
// ✅ 推荐：InlineString 拥有数据
class Widget {
    InlineString name_;  // 拥有字符串
};

// ❌ 不推荐：StringView 不拥有数据
class Gadget {
    StringView name_;  // 悬挂指针风险
};
```

---

## 完整示例

### 示例：构建 JSON 路径

```cpp
#include <mine/containers/InlineString.h>
#include <mine/containers/Vector.h>

using namespace mine::containers;

class JSONPath {
    Vector<InlineString> segments_;  // 路径段,如 ["root", "user", "name"]
    
public:
    void push(StringView seg) {
        segments_.emplace_back(seg);  // 短段无堆分配
    }
    
    InlineString to_string() const {
        if (segments_.empty()) {
            return InlineString{"$"};
        }
        
        InlineString result;
        result.reserve(segments_.size() * 10);  // 预估
        
        result += "$";
        for (const auto& seg : segments_) {
            result += ".";
            result += seg;
        }
        
        return result;
    }
};

void example() {
    JSONPath path;
    path.push("root");
    path.push("user");
    path.push("name");
    
    InlineString str = path.to_string();  // "$.root.user.name"
    std::cout << str.c_str();
}
```

---

## 总结

`InlineString` 是带有 SSO 优化的拥有式字符串,具备以下优势:

1. **小字符串零开销**：23 字节以内无堆分配
2. **C 字符串兼容**：始终以 `\0` 结尾
3. **ABI 稳定**：跨 DLL 边界安全
4. **隐式转换 StringView**：与非拥有视图无缝配合

在需要拥有字符串数据且字符串长度较短时,优先使用 `InlineString` 获得更好的性能。
