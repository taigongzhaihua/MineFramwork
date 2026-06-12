# Path 类详细接口文档

## 概述

**模块**：`mine.io`  
**头文件**：`<mine/io/Path.h>`  
**命名空间**：`mine::io`

**用途**：跨平台文件系统路径抽象,提供路径解析、组件查询和拼接功能。

**核心特性**：
- **SBO 优化**：短路径（≤31 字节）无堆分配
- **UTF-8 编码**：内部统一使用 UTF-8,Windows API 调用时自动转 UTF-16
- **规范化分隔符**：内部统一使用 `/`,Windows 下自动处理 `\` → `/`
- **零拷贝组件查询**：filename/extension/stem 返回 `StringView`
- **值语义**：支持拷贝和移动,可安全传递和存储
- **平台透明**：公共 API 跨平台一致,内部处理平台差异

**设计动机**：

标准库 `std::filesystem::path` 在以下方面不满足 MineFramework 需求:
1. **ABI 不稳定**：跨编译器/版本的 ABI 不兼容
2. **平台差异**：Windows 和 POSIX 路径语义差异大
3. **性能问题**：频繁的字符串分配和转换
4. **异常依赖**：部分操作可能抛出异常

`mine::io::Path` 设计原则:
- 禁用异常,错误通过返回值传递
- 内部统一路径表示（UTF-8 + `/`）
- SBO 优化减少短路径的堆分配
- PIMPL 封装保证跨版本 ABI 稳定

---

## 类定义

```cpp
namespace mine::io {

class MINE_IO_API Path {
public:
    // 构造
    Path() noexcept;
    /*implicit*/ Path(const char* str) noexcept;
    /*implicit*/ Path(mine::core::StringView sv) noexcept;
    Path(const char* str, size_t len) noexcept;
    
    // 拷贝/移动
    Path(const Path& other) noexcept;
    Path(Path&& other) noexcept;
    Path& operator=(const Path& other) noexcept;
    Path& operator=(Path&& other) noexcept;
    ~Path() noexcept;
    
    // 路径组件查询
    [[nodiscard]] mine::core::StringView string() const noexcept;
    [[nodiscard]] const char* c_str() const noexcept;
    [[nodiscard]] bool empty() const noexcept;
    [[nodiscard]] bool is_absolute() const noexcept;
    [[nodiscard]] bool is_relative() const noexcept;
    [[nodiscard]] Path parent_path() const noexcept;
    [[nodiscard]] mine::core::StringView filename() const noexcept;
    [[nodiscard]] mine::core::StringView extension() const noexcept;
    [[nodiscard]] mine::core::StringView stem() const noexcept;
    
    // 路径修改
    void replace_extension(mine::core::StringView new_ext) noexcept;
    void remove_extension() noexcept;
    void replace_filename(mine::core::StringView new_name) noexcept;
    
    // 路径拼接
    [[nodiscard]] Path join(mine::core::StringView sub) const noexcept;
    [[nodiscard]] friend Path operator/(const Path& lhs, mine::core::StringView rhs) noexcept;
    
    // 比较
    friend bool operator==(const Path& lhs, const Path& rhs) noexcept;
    friend bool operator!=(const Path& lhs, const Path& rhs) noexcept;
    friend bool operator<(const Path& lhs, const Path& rhs) noexcept;
    
    // 工具
    [[nodiscard]] bool ends_with(mine::core::StringView suffix) const noexcept;
    [[nodiscard]] bool starts_with(mine::core::StringView prefix) const noexcept;
    [[nodiscard]] Path relative_to(const Path& base) const noexcept;
    [[nodiscard]] static Path normalize(mine::core::StringView raw) noexcept;
};

} // namespace mine::io
```

---

## 构造函数

### 默认构造

**签名**：
```cpp
Path() noexcept;
```

**描述**：构造空路径（等同于 `"."`）。

**后置条件**：
- `empty() == true`
- `string() == ""`
- `is_relative() == true`

**示例**：
```cpp
Path p;
assert(p.empty());
assert(p.is_relative());
```

---

### 从 C 字符串构造

**签名**：
```cpp
/*implicit*/ Path(const char* str) noexcept;
```

**描述**：从 null 终止的 UTF-8 字符串构造路径。

**参数**：
- `str`：UTF-8 编码的路径字符串

**前置条件**：
- `str != nullptr`

**后置条件**：
- `string()` 返回规范化后的路径（分隔符统一为 `/`）
- 若原路径长度 ≤31 字节,无堆分配

**示例**：
```cpp
Path p1("/home/user/file.txt");
Path p2("C:\\Windows\\System32");  // Windows: 自动转为 "C:/Windows/System32"

// 隐式转换
void open_file(Path path);
open_file("/tmp/data.bin");  // 合法
```

---

### 从 StringView 构造

**签名**：
```cpp
/*implicit*/ Path(mine::core::StringView sv) noexcept;
```

**描述**：从字符串视图构造路径。

**参数**：
- `sv`：UTF-8 编码的路径字符串视图

**特性**：
- 允许内部包含 `'\0'`（以 `sv.size()` 为准）
- 隐式转换,支持 `Path p = some_string_view;`

**示例**：
```cpp
StringView sv = "/usr/local/bin";
Path p1 = sv;  // 隐式转换

String s = "/opt/app";
Path p2 = s;  // String 可转换为 StringView
```

---

### 从长度限定字符串构造

**签名**：
```cpp
Path(const char* str, size_t len) noexcept;
```

**描述**：从指定长度的字符串构造路径。

**参数**：
- `str`：UTF-8 字符串指针
- `len`：字符串长度（字节数）

**用途**：
- 处理不以 null 结尾的字符串
- 避免 `strlen()` 开销

**示例**：
```cpp
const char* data = "path/to/fileGARBAGE";
Path p{data, 12};  // 仅取前 12 字节 "path/to/file"
```

---

### 拷贝构造

**签名**：
```cpp
Path(const Path& other) noexcept;
```

**描述**：拷贝构造另一个路径。

**复杂度**：
- 若 `other` 使用 SBO: O(1) 栈拷贝
- 若 `other` 在堆: O(n) 字符串拷贝

**示例**：
```cpp
Path original("/home/user");
Path copy = original;
assert(copy == original);
```

---

### 移动构造

**签名**：
```cpp
Path(Path&& other) noexcept;
```

**描述**：移动构造,转移 `other` 的路径所有权。

**后置条件**：
- `other` 变为空路径
- 原路径资源转移到当前对象

**复杂度**：O(1)

**示例**：
```cpp
Path source("/var/log/app.log");
Path dest = std::move(source);

assert(dest.string() == "/var/log/app.log");
assert(source.empty());  // 已被移动
```

---

## 路径组件查询

### string

**签名**：
```cpp
[[nodiscard]] mine::core::StringView string() const noexcept;
```

**描述**：返回路径的字符串视图。

**返回值**：
- 规范化后的路径字符串视图（分隔符为 `/`）
- 视图生命周期与 `Path` 对象绑定

**注意**：
- 返回的 `StringView` 在 `Path` 对象析构或修改后失效
- 若需要持久化字符串,使用 `String(p.string())`

**示例**：
```cpp
Path p("/home/user/docs/report.pdf");
StringView sv = p.string();
printf("Path: %.*s\n", (int)sv.size(), sv.data());
```

---

### c_str

**签名**：
```cpp
[[nodiscard]] const char* c_str() const noexcept;
```

**描述**：返回 null 终止的 C 字符串。

**返回值**：
- 指向内部路径缓冲区的指针
- 保证以 `'\0'` 结尾

**用途**：
- 传递给 C API（如 `fopen(path.c_str(), "r")`）
- 日志输出

**生命周期**：指针在 `Path` 对象析构或修改后失效。

**示例**：
```cpp
Path p("/etc/config.ini");
FILE* f = fopen(p.c_str(), "r");
```

---

### empty

**签名**：
```cpp
[[nodiscard]] bool empty() const noexcept;
```

**描述**：检查路径是否为空。

**返回值**：
- `true`：路径为空（默认构造或被移动后）
- `false`：路径非空

**示例**：
```cpp
Path p1;
assert(p1.empty());

Path p2("/tmp");
assert(!p2.empty());
```

---

### is_absolute

**签名**：
```cpp
[[nodiscard]] bool is_absolute() const noexcept;
```

**描述**：检查路径是否为绝对路径。

**判断规则**：
- **POSIX**：以 `/` 开头
- **Windows**：以盘符开头（如 `C:/`）

**返回值**：
- `true`：绝对路径
- `false`：相对路径

**示例**：
```cpp
Path p1("/usr/bin/gcc");
assert(p1.is_absolute());

Path p2("src/main.cpp");
assert(p2.is_relative());

#ifdef _WIN32
Path p3("C:/Windows");
assert(p3.is_absolute());
#endif
```

---

### is_relative

**签名**：
```cpp
[[nodiscard]] bool is_relative() const noexcept;
```

**描述**：检查路径是否为相对路径。

**返回值**：`!is_absolute()`

**示例**：
```cpp
Path p("../config/settings.json");
assert(p.is_relative());
```

---

### parent_path

**签名**：
```cpp
[[nodiscard]] Path parent_path() const noexcept;
```

**描述**：返回父目录路径。

**返回值**：
- 移除最后一个路径组件后的路径
- 若无父目录（如 `/` 或 `file.txt`）,返回空路径

**示例**：
```cpp
Path p("/home/user/docs/report.pdf");
Path parent = p.parent_path();
assert(parent.string() == "/home/user/docs");

Path p2("/file.txt");
Path parent2 = p2.parent_path();
assert(parent2.string() == "/");

Path p3("file.txt");
assert(p3.parent_path().empty());
```

---

### filename

**签名**：
```cpp
[[nodiscard]] mine::core::StringView filename() const noexcept;
```

**描述**：返回文件名（含扩展名）。

**返回值**：
- 路径的最后一个组件
- 若路径以 `/` 结尾,返回空字符串

**示例**：
```cpp
Path p("/home/user/report.pdf");
assert(p.filename() == "report.pdf");

Path p2("/usr/bin/");
assert(p2.filename() == "");

Path p3("../config");
assert(p3.filename() == "config");
```

---

### extension

**签名**：
```cpp
[[nodiscard]] mine::core::StringView extension() const noexcept;
```

**描述**：返回文件扩展名（含点号）。

**返回值**：
- 文件名中最后一个 `.` 及其后的内容
- 若无扩展名,返回空字符串
- 隐藏文件（如 `.bashrc`）不被视为扩展名

**示例**：
```cpp
Path p1("report.pdf");
assert(p1.extension() == ".pdf");

Path p2("archive.tar.gz");
assert(p2.extension() == ".gz");  // 仅最后一个扩展

Path p3("README");
assert(p3.extension() == "");

Path p4(".gitignore");
assert(p4.extension() == "");  // 隐藏文件无扩展名
```

---

### stem

**签名**：
```cpp
[[nodiscard]] mine::core::StringView stem() const noexcept;
```

**描述**：返回文件名（不含扩展名）。

**返回值**：
- `filename()` 移除 `extension()` 后的部分

**示例**：
```cpp
Path p1("report.pdf");
assert(p1.stem() == "report");

Path p2("archive.tar.gz");
assert(p2.stem() == "archive.tar");

Path p3(".gitignore");
assert(p3.stem() == ".gitignore");
```

---

## 路径修改

### replace_extension

**签名**：
```cpp
void replace_extension(mine::core::StringView new_ext) noexcept;
```

**描述**：替换或添加扩展名。

**参数**：
- `new_ext`：新的扩展名（可带或不带前导点号）

**行为**：
- 若原路径有扩展名,替换之
- 若原路径无扩展名,追加之
- 若 `new_ext` 不以 `.` 开头,自动添加

**示例**：
```cpp
Path p("report.txt");
p.replace_extension(".pdf");
assert(p.string() == "report.pdf");

Path p2("archive");
p2.replace_extension("tar.gz");  // 自动添加点号
assert(p2.string() == "archive.tar.gz");
```

---

### remove_extension

**签名**：
```cpp
void remove_extension() noexcept;
```

**描述**：移除扩展名。

**等价于**：`replace_extension("")`

**示例**：
```cpp
Path p("config.json");
p.remove_extension();
assert(p.string() == "config");
```

---

### replace_filename

**签名**：
```cpp
void replace_filename(mine::core::StringView new_name) noexcept;
```

**描述**：替换路径的文件名部分。

**参数**：
- `new_name`：新的文件名

**示例**：
```cpp
Path p("/home/user/old_file.txt");
p.replace_filename("new_file.md");
assert(p.string() == "/home/user/new_file.md");
```

---

## 路径拼接

### join

**签名**：
```cpp
[[nodiscard]] Path join(mine::core::StringView sub) const noexcept;
```

**描述**：拼接子路径,返回新路径。

**参数**：
- `sub`：要追加的子路径

**返回值**：
- 新的 `Path` 对象

**行为**：
- 自动插入分隔符 `/`
- 若 `sub` 为绝对路径,直接返回 `sub`

**示例**：
```cpp
Path base("/home/user");
Path full = base.join("docs/report.pdf");
assert(full.string() == "/home/user/docs/report.pdf");
```

---

### operator/

**签名**：
```cpp
[[nodiscard]] friend Path operator/(const Path& lhs, mine::core::StringView rhs) noexcept;
```

**描述**：路径拼接运算符（等价于 `join`）。

**参数**：
- `lhs`：基路径
- `rhs`：子路径

**返回值**：
- 拼接后的新路径

**示例**：
```cpp
Path base("/usr");
Path bin = base / "local" / "bin";
assert(bin.string() == "/usr/local/bin");

// 链式拼接
Path full = Path("/home") / "user" / "docs" / "file.txt";
```

---

## 比较操作

### operator==

**签名**：
```cpp
friend bool operator==(const Path& lhs, const Path& rhs) noexcept;
```

**描述**：字符串相等比较。

**返回值**：
- `true`：路径字符串完全相同
- `false`：路径不同

**注意**：
- **大小写敏感**（Windows NTFS 实际不区分,但比较时区分）
- 未规范化路径不相等（如 `./a` ≠ `a`）

**示例**：
```cpp
Path p1("/home/user");
Path p2("/home/user");
assert(p1 == p2);

Path p3("./dir");
Path p4("dir");
assert(p3 != p4);  // 需要先规范化
```

---

### operator!=

**签名**：
```cpp
friend bool operator!=(const Path& lhs, const Path& rhs) noexcept;
```

**描述**：字符串不等比较。

**返回值**：`!(lhs == rhs)`

---

### operator<

**签名**：
```cpp
friend bool operator<(const Path& lhs, const Path& rhs) noexcept;
```

**描述**：字典序比较。

**用途**：
- 作为 `std::map<Path, T>` 的键
- 路径排序

**示例**：
```cpp
Path p1("/a/b");
Path p2("/a/c");
assert(p1 < p2);

std::map<Path, int> path_map;
path_map[Path("/etc")] = 1;
```

---

## 工具方法

### ends_with

**签名**：
```cpp
[[nodiscard]] bool ends_with(mine::core::StringView suffix) const noexcept;
```

**描述**：检查路径是否以指定后缀结尾。

**参数**：
- `suffix`：要检查的后缀字符串

**示例**：
```cpp
Path p("/home/user/file.txt");
assert(p.ends_with(".txt"));
assert(p.ends_with("file.txt"));
assert(!p.ends_with(".pdf"));
```

---

### starts_with

**签名**：
```cpp
[[nodiscard]] bool starts_with(mine::core::StringView prefix) const noexcept;
```

**描述**：检查路径是否以指定前缀开头。

**参数**：
- `prefix`：要检查的前缀字符串

**示例**：
```cpp
Path p("/home/user/docs");
assert(p.starts_with("/home"));
assert(!p.starts_with("/var"));
```

---

### relative_to

**签名**：
```cpp
[[nodiscard]] Path relative_to(const Path& base) const noexcept;
```

**描述**：计算从 `base` 到达当前路径的相对路径。

**参数**：
- `base`：基路径

**返回值**：
- 相对路径
- 若无法计算（不同根），返回原路径

**示例**：
```cpp
Path full("/home/user/docs/report.pdf");
Path base("/home/user");
Path rel = full.relative_to(base);
assert(rel.string() == "docs/report.pdf");
```

---

### normalize (静态方法)

**签名**：
```cpp
[[nodiscard]] static Path normalize(mine::core::StringView raw) noexcept;
```

**描述**：规范化路径字符串。

**参数**：
- `raw`：原始路径字符串

**返回值**：
- 规范化后的路径

**行为**：
- 统一分隔符为 `/`
- 解析 `.` 和 `..`
- 移除冗余分隔符
- 移除结尾分隔符

**示例**：
```cpp
Path p1 = Path::normalize("./dir/../file.txt");
assert(p1.string() == "file.txt");

Path p2 = Path::normalize("a//b///c");
assert(p2.string() == "a/b/c");

Path p3 = Path::normalize("dir/");
assert(p3.string() == "dir");
```

---

## 性能考虑

### SBO 优化

**阈值**：31 字节（含 null 终止符）

**性能对比**：

| 路径长度 | 存储方式 | 构造开销 | 拷贝开销 |
|----------|----------|----------|----------|
| ≤31 字节 | 栈内联 | O(1) | O(1) |
| >31 字节 | 堆分配 | O(n) | O(n) |

**建议**：
- 短路径（如 `config.json`）无需担心性能
- 长路径（如深层嵌套目录）考虑使用移动语义

**示例**：
```cpp
// ✅ 短路径,无堆分配
Path p1("data.txt");  // 8 字节

// ⚠️ 长路径,堆分配
Path p2("/very/long/path/to/some/deep/directory/file.txt");

// ✅ 避免拷贝
Path moved = std::move(p2);
```

### 组件查询开销

**零拷贝方法**（返回 `StringView`）：
- `filename()`
- `extension()`
- `stem()`
- `string()`

**分配方法**（返回新 `Path`）：
- `parent_path()`
- `join()` / `operator/`

**建议**：
- 频繁查询组件时缓存 `StringView`
- 避免在循环中重复调用 `parent_path()`

---

## 平台差异

### Windows 特殊处理

| 输入 | 内部表示 | 说明 |
|------|----------|------|
| `C:\Windows\System32` | `C:/Windows/System32` | 反斜杠→正斜杠 |
| `\\server\share\file` | `//server/share/file` | UNC 路径保留 |
| `file.txt` | `file.txt` | 相对路径不变 |

**绝对路径判断**：
- POSIX: `/` 开头
- Windows: 盘符开头（如 `C:/`）或 UNC（`//server/`）

---

## 最佳实践

### 1. 使用隐式转换

```cpp
// ✅ 推荐
void process_file(Path path);
process_file("/tmp/data.bin");

// ❌ 不推荐
process_file(Path("/tmp/data.bin"));
```

### 2. 利用移动语义

```cpp
// ✅ 避免拷贝
Path build_path() {
    Path base("/usr");
    return base / "local" / "bin";  // 自动移动
}

Path p = build_path();  // 无拷贝
```

### 3. 缓存组件查询

```cpp
// ❌ 重复查询
for (int i = 0; i < 1000; ++i) {
    if (path.extension() == ".txt") { /*...*/ }
}

// ✅ 缓存结果
StringView ext = path.extension();
for (int i = 0; i < 1000; ++i) {
    if (ext == ".txt") { /*...*/ }
}
```

### 4. 规范化后比较

```cpp
// ❌ 直接比较可能不相等
Path p1("./dir/../file.txt");
Path p2("file.txt");
assert(p1 != p2);  // 可能失败

// ✅ 规范化后比较
Path n1 = Path::normalize(p1.string());
Path n2 = Path::normalize(p2.string());
assert(n1 == n2);
```

---

## 相关类型

- **`mine::core::StringView`**：路径字符串视图
- **`mine::io::File`**：使用 `Path` 打开文件
- **`mine::io::FileSystem`**：使用 `Path` 查询文件系统

---

## 另见

- [File.md](02-File.md) — 文件读写
- [FileSystem.md](03-FileSystem.md) — 文件系统操作
- [Io.md](06-Io.md) — 模块总览
