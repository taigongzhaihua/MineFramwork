# StringView 类详细接口文档

## 概述

**模块**：`mine.core`  
**头文件**：`<mine/core/StringView.h>`  
**命名空间**：`mine::core`

**用途**：非拥有的 UTF-8 字符串视图，轻量、可拷贝、零分配。

**核心特性**：
- 仅持有指针和长度（16 字节，适合 SBO）
- 不分配堆内存，不持有所有权
- 始终假设 UTF-8 编码
- 长度以字节为单位（非 Unicode 码点数）
- 允许内部包含 `'\0'`（不依赖 null 终止）
- 可从字符串字面量、`const char*`、容器构造

---

## 类定义

```cpp
namespace mine::core {

class StringView {
public:
    using size_type = size_t;
    using value_type = char;
    
    static constexpr size_type npos = static_cast<size_type>(-1);
    
    // 构造
    constexpr StringView() noexcept = default;
    /*implicit*/ StringView(const char* str) noexcept;
    constexpr StringView(const char* str, size_type len) noexcept;
    
    // 容量
    [[nodiscard]] constexpr const char* data() const noexcept;
    [[nodiscard]] constexpr size_type   size() const noexcept;
    [[nodiscard]] constexpr bool        empty() const noexcept;
    
    // 元素访问
    [[nodiscard]] constexpr char operator[](size_type i) const noexcept;
    [[nodiscard]] constexpr char front() const noexcept;
    [[nodiscard]] constexpr char back() const noexcept;
    
    // 迭代器
    [[nodiscard]] constexpr const char* begin() const noexcept;
    [[nodiscard]] constexpr const char* end() const noexcept;
    
    // 子视图
    [[nodiscard]] StringView substr(size_type offset, size_type count = npos) const noexcept;
    
    // 搜索
    [[nodiscard]] size_type find(char c, size_type from = 0) const noexcept;
    [[nodiscard]] size_type find(StringView sub, size_type from = 0) const noexcept;
    
    // 比较
    [[nodiscard]] bool operator==(StringView other) const noexcept;
    [[nodiscard]] bool operator!=(StringView other) const noexcept;
    [[nodiscard]] bool operator<(StringView other) const noexcept;
    
    // 前缀/后缀
    [[nodiscard]] bool starts_with(StringView prefix) const noexcept;
    [[nodiscard]] bool ends_with(StringView suffix) const noexcept;
    
    // 辅助
    [[nodiscard]] const char* c_str() const noexcept;
};

} // namespace mine::core
```

---

## 构造函数

### 默认构造

**签名**：
```cpp
constexpr StringView() noexcept = default;
```

**描述**：构造空字符串视图。

**后置条件**：
- `data() == nullptr`
- `size() == 0`
- `empty() == true`

**示例**：
```cpp
StringView sv;
assert(sv.empty());
assert(sv.size() == 0);
```

---

### 从 C 字符串构造

**签名**：
```cpp
/*implicit*/ StringView(const char* str) noexcept;
```

**描述**：从 null 结尾的 C 字符串构造视图（长度由 `strlen` 计算）。

**参数**：
- `str`：null 结尾的 C 字符串指针（可为 `nullptr`，此时等价于默认构造）

**后置条件**：
- `data() == str`
- `size() == strlen(str)`（若 `str != nullptr`）
- `size() == 0`（若 `str == nullptr`）

**时间复杂度**：O(n)，其中 n 为字符串长度（需调用 `strlen`）

**隐式转换**：允许从 `const char*` 隐式转换

**示例**：
```cpp
StringView sv1{"hello"};          // 字符串字面量
assert(sv1.size() == 5);

const char* cstr = "world";
StringView sv2 = cstr;            // 隐式转换
assert(sv2.size() == 5);

StringView sv3{nullptr};          // 空视图
assert(sv3.empty());
```

---

### 从指针+长度构造

**签名**：
```cpp
constexpr StringView(const char* str, size_type len) noexcept;
```

**描述**：从指针和字节长度构造视图（内容可含 `'\0'`，不依赖 null 终止）。

**参数**：
- `str`：字符缓冲区指针（若 `len == 0` 可为 `nullptr`）
- `len`：字节长度（非码点数）

**前置条件**：
- `str != nullptr || len == 0`（否则触发断言）
- `str` 指向的内存至少有 `len` 字节可读

**时间复杂度**：O(1)

**示例**：
```cpp
const char buf[] = {'a', 'b', '\0', 'c'};
StringView sv{buf, 4};  // 包含嵌入的 '\0'
assert(sv.size() == 4);
assert(sv[2] == '\0');
```

---

## 容量查询方法

### `data()`

**签名**：
```cpp
[[nodiscard]] constexpr const char* data() const noexcept;
```

**描述**：返回指向字符缓冲区的指针。

**返回值**：
- 指向首字符的指针（若为空视图，可能为 `nullptr`）

**注意**：
- 返回的指针不保证 null 终止（除非从字符串字面量构造）
- 不可通过返回指针修改内容（const 限定）

**时间复杂度**：O(1)

**示例**：
```cpp
StringView sv{"hello"};
const char* p = sv.data();
assert(std::memcmp(p, "hello", 5) == 0);
```

---

### `size()`

**签名**：
```cpp
[[nodiscard]] constexpr size_type size() const noexcept;
```

**描述**：返回字符串字节长度（非 Unicode 码点数）。

**返回值**：
- 字节数（UTF-8 编码下，多字节字符计为多个字节）

**时间复杂度**：O(1)

**示例**：
```cpp
StringView sv1{"hello"};
assert(sv1.size() == 5);

StringView sv2{"你好"};  // UTF-8：每个汉字 3 字节
assert(sv2.size() == 6);
```

---

### `empty()`

**签名**：
```cpp
[[nodiscard]] constexpr bool empty() const noexcept;
```

**描述**：检查字符串视图是否为空。

**返回值**：
- `true`：`size() == 0`
- `false`：`size() > 0`

**时间复杂度**：O(1)

**示例**：
```cpp
StringView sv1;
assert(sv1.empty());

StringView sv2{""};
assert(sv2.empty());

StringView sv3{"a"};
assert(!sv3.empty());
```

---

## 元素访问方法

### `operator[]`

**签名**：
```cpp
[[nodiscard]] constexpr char operator[](size_type i) const noexcept;
```

**描述**：访问指定位置的字符（无边界检查版本）。

**参数**：
- `i`：字节偏移（0-based）

**前置条件**：
- `i < size()`（否则触发断言）

**返回值**：
- 第 `i` 个字节的字符

**时间复杂度**：O(1)

**示例**：
```cpp
StringView sv{"hello"};
assert(sv[0] == 'h');
assert(sv[4] == 'o');

// assert(sv[5] == '\0');  // 断言失败：越界访问
```

---

### `front()`

**签名**：
```cpp
[[nodiscard]] constexpr char front() const noexcept;
```

**描述**：返回首字符。

**前置条件**：
- `!empty()`（否则触发断言）

**返回值**：
- `(*this)[0]`

**时间复杂度**：O(1)

**示例**：
```cpp
StringView sv{"hello"};
assert(sv.front() == 'h');

// StringView empty;
// char c = empty.front();  // 断言失败：空视图
```

---

### `back()`

**签名**：
```cpp
[[nodiscard]] constexpr char back() const noexcept;
```

**描述**：返回末字符。

**前置条件**：
- `!empty()`（否则触发断言）

**返回值**：
- `(*this)[size() - 1]`

**时间复杂度**：O(1)

**示例**：
```cpp
StringView sv{"hello"};
assert(sv.back() == 'o');
```

---

## 迭代器

### `begin()` / `end()`

**签名**：
```cpp
[[nodiscard]] constexpr const char* begin() const noexcept;
[[nodiscard]] constexpr const char* end() const noexcept;
```

**描述**：返回指向首字符和尾后位置的指针（支持范围 for 循环）。

**返回值**：
- `begin()`：等价于 `data()`
- `end()`：等价于 `data() + size()`

**时间复杂度**：O(1)

**示例**：
```cpp
StringView sv{"hello"};

// 范围 for
for (char c : sv) {
    printf("%c ", c);  // 输出：h e l l o
}

// 算法
size_t count_a = std::count(sv.begin(), sv.end(), 'l');
assert(count_a == 2);
```

---

## 子视图方法

### `substr()`

**签名**：
```cpp
[[nodiscard]] StringView substr(size_type offset, size_type count = npos) const noexcept;
```

**描述**：返回从 `offset` 开始、长度为 `count` 的子视图。

**参数**：
- `offset`：起始字节偏移（闭区间）
- `count`：字节数（默认 `npos` 表示到末尾）

**前置条件**：
- `offset <= size()`（否则触发断言）

**返回值**：
- 子视图 `StringView{data() + offset, actual_count}`
- `actual_count = min(count, size() - offset)`

**时间复杂度**：O(1)

**示例**：
```cpp
StringView sv{"hello world"};

StringView sub1 = sv.substr(0, 5);
assert(sub1 == "hello");

StringView sub2 = sv.substr(6);
assert(sub2 == "world");

StringView sub3 = sv.substr(6, 100);
assert(sub3 == "world");  // 超出长度自动截断
```

---

## 搜索方法

### `find(char)`

**签名**：
```cpp
[[nodiscard]] size_type find(char c, size_type from = 0) const noexcept;
```

**描述**：在视图中查找字符 `c` 首次出现的位置。

**参数**：
- `c`：待查找的字符
- `from`：起始搜索位置（字节偏移，默认为 0）

**返回值**：
- 找到：返回字节偏移
- 未找到：返回 `npos`

**时间复杂度**：O(n)，其中 n = `size() - from`

**示例**：
```cpp
StringView sv{"hello"};

size_t pos1 = sv.find('l');
assert(pos1 == 2);  // 首次出现

size_t pos2 = sv.find('l', 3);
assert(pos2 == 3);  // 从位置 3 开始

size_t pos3 = sv.find('x');
assert(pos3 == StringView::npos);  // 未找到
```

---

### `find(StringView)`

**签名**：
```cpp
[[nodiscard]] size_type find(StringView sub, size_type from = 0) const noexcept;
```

**描述**：在视图中查找子串 `sub` 首次出现的位置。

**参数**：
- `sub`：待查找的子串
- `from`：起始搜索位置（字节偏移，默认为 0）

**返回值**：
- 找到：返回字节偏移
- 未找到：返回 `npos`

**特殊情况**：
- 若 `sub.empty()`：返回 `from`
- 若 `sub.size() > size()`：返回 `npos`

**时间复杂度**：O(n * m)，其中 n = `size() - from`，m = `sub.size()`（朴素算法）

**示例**：
```cpp
StringView sv{"hello world"};

size_t pos1 = sv.find("world");
assert(pos1 == 6);

size_t pos2 = sv.find("lo");
assert(pos2 == 3);

size_t pos3 = sv.find("xyz");
assert(pos3 == StringView::npos);

size_t pos4 = sv.find("");
assert(pos4 == 0);  // 空子串在起始位置
```

---

## 比较方法

### `operator==` / `operator!=`

**签名**：
```cpp
[[nodiscard]] bool operator==(StringView other) const noexcept;
[[nodiscard]] bool operator!=(StringView other) const noexcept;
```

**描述**：比较两个字符串视图是否相等。

**参数**：
- `other`：待比较的另一个视图

**返回值**：
- `operator==`：长度相等且内容逐字节相同返回 `true`
- `operator!=`：不相等返回 `true`

**时间复杂度**：
- 最好情况（长度不等）：O(1)
- 最坏情况（长度相等）：O(n)

**示例**：
```cpp
StringView sv1{"hello"};
StringView sv2{"hello"};
StringView sv3{"world"};

assert(sv1 == sv2);
assert(sv1 != sv3);
assert(sv1 == "hello");  // 隐式转换
```

---

### `operator<`

**签名**：
```cpp
[[nodiscard]] bool operator<(StringView other) const noexcept;
```

**描述**：字典序比较（用于排序）。

**参数**：
- `other`：待比较的另一个视图

**返回值**：
- `true`：`*this` 字典序小于 `other`
- `false`：`*this` 字典序大于等于 `other`

**比较规则**：
1. 按字节逐个比较（`memcmp`）
2. 若前缀相同，短者小于长者

**时间复杂度**：O(min(size(), other.size()))

**示例**：
```cpp
StringView sv1{"apple"};
StringView sv2{"banana"};
StringView sv3{"app"};

assert(sv1 < sv2);   // "apple" < "banana"
assert(sv3 < sv1);   // "app" < "apple"（前缀相同，短者更小）

// 排序
std::vector<StringView> words{"dog", "cat", "bird"};
std::sort(words.begin(), words.end());
// 结果：["bird", "cat", "dog"]
```

---

## 前缀/后缀检测

### `starts_with()`

**签名**：
```cpp
[[nodiscard]] bool starts_with(StringView prefix) const noexcept;
```

**描述**：检查字符串是否以指定前缀开头。

**参数**：
- `prefix`：待检查的前缀

**返回值**：
- `true`：字符串以 `prefix` 开头
- `false`：否

**时间复杂度**：O(prefix.size())

**示例**：
```cpp
StringView sv{"hello world"};

assert(sv.starts_with("hello"));
assert(sv.starts_with(""));      // 空前缀始终匹配
assert(!sv.starts_with("world"));
```

---

### `ends_with()`

**签名**：
```cpp
[[nodiscard]] bool ends_with(StringView suffix) const noexcept;
```

**描述**：检查字符串是否以指定后缀结尾。

**参数**：
- `suffix`：待检查的后缀

**返回值**：
- `true`：字符串以 `suffix` 结尾
- `false`：否

**时间复杂度**：O(suffix.size())

**示例**：
```cpp
StringView sv{"hello world"};

assert(sv.ends_with("world"));
assert(sv.ends_with(""));        // 空后缀始终匹配
assert(!sv.ends_with("hello"));
```

---

## 辅助方法

### `c_str()`

**签名**：
```cpp
[[nodiscard]] const char* c_str() const noexcept;
```

**描述**：返回 C 风格字符串指针（等价于 `data()`）。

**⚠️ 警告**：
- 不保证返回的指针指向 null 终止字符串
- 仅对从字符串字面量或 null 终止缓冲区构造的视图安全
- 若视图通过 `substr()` 或 `{ptr, len}` 构造，可能不以 `'\0'` 结尾

**返回值**：
- 等价于 `data()`

**安全用法**：
```cpp
// ✅ 安全：字符串字面量保证 null 终止
StringView sv1{"hello"};
printf("%s\n", sv1.c_str());

// ⚠️ 不安全：子视图不保证 null 终止
StringView sv2 = sv1.substr(0, 3);
// printf("%s\n", sv2.c_str());  // 可能读取越界内存
```

---

## 常量

### `npos`

**声明**：
```cpp
static constexpr size_type npos = static_cast<size_type>(-1);
```

**描述**："无效位置"常量，用于表示查找失败。

**值**：`size_t` 最大值（通常为 `0xFFFFFFFFFFFFFFFF`）

**用途**：
- `find()` 方法未找到时的返回值
- `substr()` 方法的默认参数（表示到末尾）

**示例**：
```cpp
StringView sv{"hello"};

if (sv.find('x') == StringView::npos) {
    printf("未找到字符 'x'\n");
}

StringView tail = sv.substr(2, StringView::npos);
assert(tail == "llo");
```

---

## 使用场景

### 1. 函数参数（避免字符串拷贝）

```cpp
// ❌ 不推荐：每次调用拷贝字符串
void process_string(const std::string& str);

// ✅ 推荐：零拷贝传参
void process_string(StringView sv) {
    // sv 仅持有指针，无拷贝开销
    if (sv.starts_with("prefix_")) {
        auto name = sv.substr(7);
        // ...
    }
}

// 调用
process_string("literal");            // OK：隐式转换
process_string(std::string{"str"});   // OK：需 std::string 提供 StringView 转换
```

### 2. 字符串解析

```cpp
std::vector<StringView> split(StringView str, char delim) {
    std::vector<StringView> result;
    size_t start = 0;
    
    while (start < str.size()) {
        size_t pos = str.find(delim, start);
        if (pos == StringView::npos) {
            result.push_back(str.substr(start));
            break;
        }
        result.push_back(str.substr(start, pos - start));
        start = pos + 1;
    }
    
    return result;
}

// 使用
StringView csv{"apple,banana,cherry"};
auto tokens = split(csv, ',');
// tokens = ["apple", "banana", "cherry"]（无拷贝，仅视图）
```

### 3. 配置键值对

```cpp
class Config {
private:
    HashMap<StringView, StringView> kvs_;
    
public:
    void set(StringView key, StringView value) {
        // 警告：需确保 key/value 底层字符串生命周期足够长
        kvs_[key] = value;
    }
    
    StringView get(StringView key, StringView default_value) const {
        auto it = kvs_.find(key);
        return it != kvs_.end() ? it->second : default_value;
    }
};
```

---

## 性能特征

| 操作 | 时间复杂度 | 空间复杂度 |
|------|------------|------------|
| 构造（`const char*`） | O(n) | O(1) |
| 构造（`ptr + len`） | O(1) | O(1) |
| 拷贝 | O(1) | O(1) |
| `size()` / `empty()` | O(1) | O(1) |
| `operator[]` | O(1) | O(1) |
| `substr()` | O(1) | O(1) |
| `find(char)` | O(n) | O(1) |
| `find(StringView)` | O(n*m) | O(1) |
| `operator==` | O(n) | O(1) |
| `operator<` | O(n) | O(1) |
| `starts_with()` / `ends_with()` | O(m) | O(1) |

**内存占用**：
- `sizeof(StringView) = 16` 字节（64 位平台：指针 8 字节 + size_t 8 字节）
- 无堆分配

---

## 线程安全

**完全线程安全**（只读操作）：
- StringView 对象本身不可变（immutable）
- 多线程同时读取同一 StringView 无竞争
- 前提：底层字符串内存在所有线程访问期间保持有效

**注意事项**：
- 需确保底层字符串在所有 StringView 生命周期内不被修改或释放
- 若底层字符串在另一线程被修改，所有持有该视图的线程将观察到数据竞争

---

## 限制与注意事项

### 生命周期陷阱

**问题**：StringView 不拥有字符串，必须确保底层内存有效。

```cpp
// ❌ 危险：返回悬空视图
StringView create_view() {
    std::string temp = "temporary";
    return StringView{temp};  // temp 析构后，视图悬空
}

StringView sv = create_view();
// sv.data() 指向已释放内存 → 未定义行为
```

**解决方案**：返回拥有型字符串
```cpp
// ✅ 安全：返回拥有所有权的字符串
InlineString<32> create_string() {
    return InlineString<32>{"owned"};
}

auto str = create_string();
StringView sv = str.view();  // 安全：str 在作用域内有效
```

### 不保证 null 终止

```cpp
const char buf[] = {'a', 'b', 'c'};
StringView sv{buf, 3};

// ❌ 不安全：可能读取越界内存
printf("%s\n", sv.c_str());

// ✅ 安全：指定长度
printf("%.*s\n", (int)sv.size(), sv.data());
```

### UTF-8 码点计数

```cpp
StringView sv{"你好"};
assert(sv.size() == 6);  // 字节数，非码点数

// 若需码点数，须遍历解码
size_t count_codepoints(StringView sv) {
    // 简化版：假设所有字节为 UTF-8 起始字节
    size_t count = 0;
    for (char c : sv) {
        if ((c & 0xC0) != 0x80) ++count;  // 非 UTF-8 续字节
    }
    return count;
}
assert(count_codepoints(sv) == 2);  // 2 个汉字
```

---

## 调试技巧

### 格式化输出

```cpp
void print(StringView sv) {
    printf("StringView{\"%.*s\", len=%zu}\n",
           (int)sv.size(), sv.data(), sv.size());
}

StringView sv{"hello"};
print(sv);  // 输出：StringView{"hello", len=5}
```

### 断点条件

```cpp
// Visual Studio / GDB 断点条件
sv.size() == 5
sv.starts_with("prefix")
std::string{sv.data(), sv.size()} == "expected"
```

### 内存可视化

```cpp
void dump_hex(StringView sv) {
    for (size_t i = 0; i < sv.size(); ++i) {
        printf("%02X ", (unsigned char)sv[i]);
    }
    printf("\n");
}

StringView sv{"你好"};
dump_hex(sv);  // 输出：E4 BD A0 E5 A5 BD（UTF-8 编码）
```

---

## 示例：完整用法

```cpp
#include <mine/core/StringView.h>
#include <cassert>
#include <vector>

using namespace mine;

void example_basic() {
    // 构造
    core::StringView sv1{"hello"};
    core::StringView sv2{"world", 5};
    core::StringView sv3;
    
    // 查询
    assert(sv1.size() == 5);
    assert(!sv1.empty());
    assert(sv3.empty());
    
    // 元素访问
    assert(sv1[0] == 'h');
    assert(sv1.front() == 'h');
    assert(sv1.back() == 'o');
}

void example_operations() {
    core::StringView sv{"hello world"};
    
    // 子视图
    auto word1 = sv.substr(0, 5);
    assert(word1 == "hello");
    
    auto word2 = sv.substr(6);
    assert(word2 == "world");
    
    // 搜索
    size_t pos = sv.find(' ');
    assert(pos == 5);
    
    // 前缀/后缀
    assert(sv.starts_with("hello"));
    assert(sv.ends_with("world"));
}

void example_parsing() {
    core::StringView path{"/path/to/file.txt"};
    
    // 提取文件名
    size_t last_slash = path.find('/');
    while (last_slash != core::StringView::npos) {
        size_t next = path.find('/', last_slash + 1);
        if (next == core::StringView::npos) break;
        last_slash = next;
    }
    
    auto filename = (last_slash != core::StringView::npos)
        ? path.substr(last_slash + 1)
        : path;
    
    assert(filename == "file.txt");
    
    // 提取扩展名
    size_t dot = filename.find('.');
    if (dot != core::StringView::npos) {
        auto ext = filename.substr(dot + 1);
        assert(ext == "txt");
    }
}

int main() {
    example_basic();
    example_operations();
    example_parsing();
    return 0;
}
```

---

## 相关类型

- `mine::containers::InlineString`：拥有型字符串，可提供 StringView
- `mine::core::Variant`：可存储 StringView（需注意生命周期）
- `mine::core::Span<const char>`：字节视图，可与 StringView 互换

---

## 最佳实践

### ✅ 推荐用法

```cpp
// 1. 函数参数（零拷贝）
void process(StringView sv);

// 2. 字符串字面量
StringView sv = "literal";

// 3. 与拥有型字符串配合
InlineString<32> str{"owned"};
StringView sv = str.view();

// 4. 格式化输出
printf("%.*s\n", (int)sv.size(), sv.data());
```

### ❌ 避免的用法

```cpp
// ❌ 返回局部字符串的视图
StringView bad() {
    std::string temp = "temp";
    return StringView{temp};  // 悬空
}

// ❌ 假设 null 终止
StringView sv = get_substr();
printf("%s\n", sv.c_str());  // 可能越界

// ❌ 长期持有临时字符串的视图
StringView sv;
{
    InlineString<32> temp{"temp"};
    sv = temp.view();
}  // temp 析构
// sv 现在悬空
```
