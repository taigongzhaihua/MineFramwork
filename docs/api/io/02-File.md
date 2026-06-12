# File 类详细接口文档

## 概述

**模块**：`mine.io`  
**头文件**：`<mine/io/File.h>`  
**命名空间**：`mine::io`

**用途**：跨平台文件读写操作,基于操作系统原生句柄,提供 RAII 生命周期管理。

**核心特性**：
- **RAII 管理**：析构自动关闭文件句柄
- **零开销抽象**：直接调用系统 API,无 C stdio 间接层
- **显式错误处理**：所有操作返回 `Result<T>` 或 `Status`,无异常
- **移动语义**：禁用拷贝,通过移动转移所有权
- **随机访问**：支持 seek/tell 定位文件位置
- **模式组合**：通过位标志灵活组合打开模式

**设计动机**：

标准库 `std::fstream` 和 C stdio 在以下方面不满足需求:
1. **异常依赖**：错误处理依赖异常机制
2. **性能开销**：stdio 额外的缓冲层增加开销
3. **ABI 不稳定**：跨编译器二进制兼容性差
4. **平台差异**：文本模式 `\n`/`\r\n` 转换不可控

`mine::io::File` 设计原则:
- 禁用异常,错误通过 `Result<T>` 返回
- 直接封装系统句柄（Windows `HANDLE`、POSIX `int fd`）
- 无用户态缓冲（需要缓冲请使用 `PipeStream`）
- 统一 UTF-8 编码,平台转换在内部处理

---

## 类定义

```cpp
namespace mine::io {

// 打开模式
enum class FileMode : uint32_t {
    Read        = 1 << 0,
    Write       = 1 << 1,
    ReadWrite   = Read | Write,
    Append      = 1 << 2,
    Create      = 1 << 3,
    Truncate    = 1 << 4,
    Exclusive   = 1 << 5,
    
    DefaultRead  = Read,
    DefaultWrite = Write | Create | Truncate,
};

// 访问权限（POSIX 风格）
enum class FileAccess : uint32_t {
    OwnerRead   = 0400,
    OwnerWrite  = 0200,
    OwnerExec   = 0100,
    // ... 其他权限 ...
    Default     = OwnerRead | OwnerWrite,
};

// 定位原点
enum class FileSeekOrigin : int32_t {
    Begin   = 0,
    Current = 1,
    End     = 2,
};

// 文件句柄
class MINE_IO_API File {
public:
    // 构造与析构
    File() noexcept;
    File(File&& other) noexcept;
    File& operator=(File&& other) noexcept;
    File(const File&) = delete;
    File& operator=(const File&) = delete;
    ~File() noexcept;
    
    // 打开与关闭
    [[nodiscard]] static mine::core::Result<File>
    open(const Path& path, FileMode mode = FileMode::DefaultRead) noexcept;
    
    [[nodiscard]] static mine::core::Result<File>
    open_with_access(const Path& path, FileMode mode, FileAccess access) noexcept;
    
    [[nodiscard]] bool is_open() const noexcept;
    mine::core::Status close() noexcept;
    
    // 读取
    [[nodiscard]] mine::core::Result<size_t>
    read(mine::core::Span<char> buf) noexcept;
    
    [[nodiscard]] mine::core::Result<mine::core::Status>
    read_all(mine::core::Span<char> out_buffer) noexcept;
    
    [[nodiscard]] mine::core::Result<size_t>
    read_exact(mine::core::Span<char> buf) noexcept;
    
    // 写入
    [[nodiscard]] mine::core::Result<size_t>
    write(mine::core::Span<const char> buf) noexcept;
    
    mine::core::Status write_all(mine::core::Span<const char> buf) noexcept;
    mine::core::Status flush() noexcept;
    
    // 定位
    [[nodiscard]] mine::core::Result<size_t>
    seek(int64_t offset, FileSeekOrigin origin = FileSeekOrigin::Begin) noexcept;
    
    [[nodiscard]] mine::core::Result<size_t> tell() const noexcept;
    
    // 大小
    [[nodiscard]] mine::core::Result<size_t> size() const noexcept;
    mine::core::Status set_size(size_t new_size) noexcept;
    
    // 原始句柄
    [[nodiscard]] void* native_handle() const noexcept;
};

} // namespace mine::io
```

---

## 枚举类型

### FileMode

**用途**：指定文件打开模式,可通过位运算组合。

**枚举值**：

| 标志 | 值 | 说明 |
|------|----|----|
| `Read` | `1 << 0` | 允许读取 |
| `Write` | `1 << 1` | 允许写入 |
| `ReadWrite` | `Read \| Write` | 同时读写 |
| `Append` | `1 << 2` | 追加模式（写入总在末尾） |
| `Create` | `1 << 3` | 若文件不存在则创建 |
| `Truncate` | `1 << 4` | 打开时清空文件内容 |
| `Exclusive` | `1 << 5` | 若文件已存在则失败 |
| `DefaultRead` | `Read` | 默认只读模式 |
| `DefaultWrite` | `Write \| Create \| Truncate` | 默认写入模式（创建并覆盖） |

**位运算操作**：

```cpp
// 组合标志
FileMode mode = FileMode::Write | FileMode::Create | FileMode::Truncate;

// 检查标志
bool can_read = has_flag(mode, FileMode::Read);

// 掩码操作
FileMode masked = mode & FileMode::Write;
```

**常见组合**：

| 场景 | 模式组合 |
|------|----------|
| 只读打开 | `FileMode::Read` |
| 写入覆盖 | `FileMode::Write \| Create \| Truncate` |
| 追加日志 | `FileMode::Write \| Create \| Append` |
| 读写更新 | `FileMode::ReadWrite \| Create` |
| 创建新文件 | `FileMode::Write \| Create \| Exclusive` |

---

### FileAccess

**用途**：指定文件访问权限（POSIX 风格,Windows 下效果有限）。

**枚举值**：

| 标志 | 八进制值 | 说明 |
|------|----------|------|
| `OwnerRead` | `0400` | 所有者可读 |
| `OwnerWrite` | `0200` | 所有者可写 |
| `OwnerExec` | `0100` | 所有者可执行 |
| `GroupRead` | `0040` | 组可读 |
| `GroupWrite` | `0020` | 组可写 |
| `GroupExec` | `0010` | 组可执行 |
| `OtherRead` | `0004` | 其他用户可读 |
| `OtherWrite` | `0002` | 其他用户可写 |
| `OtherExec` | `0001` | 其他用户可执行 |
| `Default` | `0600` | 默认权限（所有者读写） |

**平台说明**：
- **POSIX**：直接映射为 `chmod` 权限
- **Windows**：仅区分只读/可写,其他标志被忽略

**示例**：
```cpp
// 创建仅所有者可读写的文件
FileAccess access = FileAccess::OwnerRead | FileAccess::OwnerWrite;
auto file = File::open_with_access("secret.txt", 
                                   FileMode::Write | FileMode::Create,
                                   access);
```

---

### FileSeekOrigin

**用途**：指定 `seek()` 操作的参考位置。

**枚举值**：

| 值 | 说明 |
|----|------|
| `Begin` | 从文件起始处（偏移 0） |
| `Current` | 从当前位置 |
| `End` | 从文件末尾 |

**示例**：
```cpp
File file = File::open("data.bin", FileMode::ReadWrite).value();

// 定位到文件开头第 100 字节
file.seek(100, FileSeekOrigin::Begin);

// 向后移动 50 字节
file.seek(50, FileSeekOrigin::Current);

// 定位到文件末尾前 10 字节
file.seek(-10, FileSeekOrigin::End);
```

---

## 构造与析构

### 默认构造

**签名**：
```cpp
File() noexcept;
```

**描述**：构造空文件句柄（未打开任何文件）。

**后置条件**：
- `is_open() == false`
- `native_handle() == nullptr`

**示例**：
```cpp
File file;  // 空句柄
assert(!file.is_open());
```

---

### 移动构造

**签名**：
```cpp
File(File&& other) noexcept;
```

**描述**：移动构造,转移 `other` 的文件句柄所有权。

**后置条件**：
- `other.is_open() == false`（句柄已被移走）
- 当前对象持有原句柄

**复杂度**：O(1)

**示例**：
```cpp
File source = File::open("data.txt", FileMode::Read).value();
File dest = std::move(source);

assert(dest.is_open());
assert(!source.is_open());  // 已被移动
```

---

### 移动赋值

**签名**：
```cpp
File& operator=(File&& other) noexcept;
```

**描述**：移动赋值,转移 `other` 的所有权。

**行为**：
1. 关闭当前句柄（若已打开）
2. 转移 `other` 的句柄
3. 将 `other` 置为空状态

**示例**：
```cpp
File file1 = File::open("file1.txt", FileMode::Read).value();
File file2 = File::open("file2.txt", FileMode::Read).value();

file1 = std::move(file2);  // file1 关闭原句柄,接管 file2 的句柄
```

---

### 析构

**签名**：
```cpp
~File() noexcept;
```

**描述**：析构时自动关闭文件句柄。

**行为**：
- 若 `is_open() == true`,调用 `close()`
- 忽略关闭错误（析构函数不能返回错误）

**最佳实践**：
- 关键文件操作应显式调用 `close()` 并检查错误
- 析构时关闭仅作为安全兜底

**示例**：
```cpp
{
    File file = File::open("temp.txt", FileMode::Write | FileMode::Create).value();
    file.write_all("data");
    // 析构时自动关闭
}
// file 超出作用域,句柄已关闭
```

---

## 打开与关闭

### open (静态工厂方法)

**签名**：
```cpp
[[nodiscard]] static mine::core::Result<File>
open(const Path& path, FileMode mode = FileMode::DefaultRead) noexcept;
```

**描述**：以指定模式打开文件。

**参数**：
- `path`：文件路径（支持 UTF-8）
- `mode`：打开模式（默认只读）

**返回值**：
- **成功**：`Result<File>` 包含已打开的文件句柄
- **失败**：错误码（如 `StatusCode::NotFound`、`StatusCode::PermissionDenied`）

**错误情况**：

| 错误码 | 原因 |
|--------|------|
| `NotFound` | 文件不存在且未指定 `Create` 标志 |
| `AlreadyExists` | 文件已存在且指定 `Exclusive` 标志 |
| `PermissionDenied` | 无访问权限 |
| `InvalidArgument` | 路径格式非法 |
| `IoError` | 其他 I/O 错误 |

**示例**：
```cpp
// 只读打开
auto result = File::open(Path::from("config.json"));
if (!result) {
    printf("打开失败: %s\n", result.error().message());
    return;
}
File file = std::move(result.value());

// 写入模式（创建并覆盖）
auto file2 = File::open("output.txt", FileMode::DefaultWrite);

// 追加模式
auto file3 = File::open("log.txt", 
                        FileMode::Write | FileMode::Create | FileMode::Append);

// 创建新文件（若已存在则失败）
auto file4 = File::open("unique.dat",
                        FileMode::Write | FileMode::Create | FileMode::Exclusive);
```

---

### open_with_access

**签名**：
```cpp
[[nodiscard]] static mine::core::Result<File>
open_with_access(const Path& path, FileMode mode, FileAccess access) noexcept;
```

**描述**：以指定模式和权限打开文件。

**参数**：
- `path`：文件路径
- `mode`：打开模式
- `access`：访问权限（POSIX 有效,Windows 部分支持）

**返回值**：同 `open()`

**平台差异**：
- **Linux/macOS**：`access` 直接映射为 `chmod` 权限
- **Windows**：仅区分只读/可写,其他位被忽略

**示例**：
```cpp
// 创建仅所有者可访问的私密文件
auto file = File::open_with_access(
    "private.key",
    FileMode::Write | FileMode::Create | FileMode::Exclusive,
    FileAccess::OwnerRead | FileAccess::OwnerWrite
);
```

---

### is_open

**签名**：
```cpp
[[nodiscard]] bool is_open() const noexcept;
```

**描述**：检查文件是否已打开。

**返回值**：
- `true`：文件句柄有效
- `false`：未打开或已关闭

**示例**：
```cpp
File file;
assert(!file.is_open());

file = File::open("data.txt", FileMode::Read).value();
assert(file.is_open());

file.close();
assert(!file.is_open());
```

---

### close

**签名**：
```cpp
mine::core::Status close() noexcept;
```

**描述**：显式关闭文件,释放系统句柄。

**返回值**：
- **成功**：`Status::success()`
- **失败**：错误状态（如 `IoError`）

**后置条件**：
- `is_open() == false`
- 后续 I/O 操作将失败

**幂等性**：多次调用 `close()` 是安全的（第二次返回成功）。

**最佳实践**：
- 写入操作后显式 `close()` 并检查错误
- 析构时自动关闭作为兜底,但会忽略错误

**示例**：
```cpp
File file = File::open("output.txt", FileMode::DefaultWrite).value();
file.write_all("important data");

Status s = file.close();
if (!s) {
    printf("关闭失败,数据可能丢失: %s\n", s.message());
}
```

---

## 读取操作

### read

**签名**：
```cpp
[[nodiscard]] mine::core::Result<size_t>
read(mine::core::Span<char> buf) noexcept;
```

**描述**：从当前位置读取最多 `buf.size()` 字节到缓冲区。

**参数**：
- `buf`：目标缓冲区（`Span` 封装指针和长度）

**返回值**：
- **成功**：实际读取的字节数（可能小于 `buf.size()`）
- **失败**：错误状态

**特殊情况**：
- 返回 `0` 表示已到达文件末尾（EOF）
- 返回值 `< buf.size()` 不代表错误（短读是正常的）

**示例**：
```cpp
File file = File::open("data.bin", FileMode::Read).value();

char buffer[1024];
auto result = file.read(buffer);

if (!result) {
    printf("读取失败: %s\n", result.error().message());
    return;
}

size_t bytes_read = result.value();
if (bytes_read == 0) {
    printf("已到达文件末尾\n");
} else {
    printf("读取了 %zu 字节\n", bytes_read);
}
```

---

### read_all

**签名**：
```cpp
[[nodiscard]] mine::core::Result<mine::core::Status>
read_all(mine::core::Span<char> out_buffer) noexcept;
```

**描述**：读取文件全部内容到预分配的缓冲区。

**参数**：
- `out_buffer`：输出缓冲区（需预先分配足够大小）

**返回值**：
- **成功**：`Status::success()`
- **失败**：错误状态

**前置条件**：
- `out_buffer.size()` 必须 ≥ 文件大小

**适用场景**：
- 小文件一次性读取
- 已知文件大小的配置文件

**不适用场景**：
- 大文件（应使用流式读取）
- 文件大小未知

**示例**：
```cpp
File file = File::open("config.json", FileMode::Read).value();

// 获取文件大小
size_t file_size = file.size().value();

// 分配缓冲区
std::vector<char> buffer(file_size);

// 读取全部内容
auto result = file.read_all(buffer);
if (!result) {
    printf("读取失败: %s\n", result.error().message());
    return;
}

// buffer 现在包含文件全部内容
String json{buffer.data(), buffer.size()};
```

---

### read_exact

**签名**：
```cpp
[[nodiscard]] mine::core::Result<size_t>
read_exact(mine::core::Span<char> buf) noexcept;
```

**描述**：精确读取 `buf.size()` 字节,循环读取直到填满缓冲区或遇到 EOF/错误。

**参数**：
- `buf`：目标缓冲区

**返回值**：
- **成功**：实际读取的字节数（等于 `buf.size()`）
- **失败**：错误状态或提前 EOF

**与 `read()` 的区别**：
- `read()`：单次调用,可能短读
- `read_exact()`：循环读取,保证填满缓冲区（除非 EOF）

**示例**：
```cpp
File file = File::open("structured.dat", FileMode::Read).value();

// 读取固定大小的头部（32 字节）
struct Header {
    uint32_t magic;
    uint32_t version;
    uint64_t timestamp;
    // ... 共 32 字节
};

Header header;
auto result = file.read_exact(Span<char>(reinterpret_cast<char*>(&header), sizeof(header)));

if (!result || result.value() < sizeof(header)) {
    printf("头部读取失败或文件过小\n");
    return;
}

// header 已完整读取
```

---

## 写入操作

### write

**签名**：
```cpp
[[nodiscard]] mine::core::Result<size_t>
write(mine::core::Span<const char> buf) noexcept;
```

**描述**：向当前位置写入缓冲区内容。

**参数**：
- `buf`：源缓冲区

**返回值**：
- **成功**：实际写入的字节数（可能小于 `buf.size()`）
- **失败**：错误状态

**特殊情况**：
- 返回值 `< buf.size()` 不代表错误（短写是正常的）
- 需要循环写入以保证全部写入（或使用 `write_all()`）

**示例**：
```cpp
File file = File::open("output.txt", FileMode::DefaultWrite).value();

const char* msg = "Hello, World!";
auto result = file.write(Span<const char>(msg, strlen(msg)));

if (!result) {
    printf("写入失败: %s\n", result.error().message());
    return;
}

printf("写入了 %zu 字节\n", result.value());
```

---

### write_all

**签名**：
```cpp
mine::core::Status write_all(mine::core::Span<const char> buf) noexcept;
```

**描述**：写入缓冲区全部内容,循环写入直到全部写入或出错。

**参数**：
- `buf`：源缓冲区

**返回值**：
- **成功**：`Status::success()`
- **失败**：错误状态

**推荐场景**：大部分写入操作应使用此方法而非 `write()`。

**示例**：
```cpp
File file = File::open("data.bin", FileMode::DefaultWrite).value();

std::vector<uint8_t> data = {0x89, 0x50, 0x4E, 0x47, /*...*/};
Status s = file.write_all(Span<const char>(
    reinterpret_cast<const char*>(data.data()), 
    data.size()
));

if (!s) {
    printf("写入失败: %s\n", s.message());
}
```

---

### flush

**签名**：
```cpp
mine::core::Status flush() noexcept;
```

**描述**：强制将缓冲数据刷入物理存储。

**行为**：
- 调用系统 API（Windows `FlushFileBuffers`、POSIX `fsync`）
- 确保数据持久化到磁盘

**性能影响**：
- 同步操作,会阻塞直到 I/O 完成
- 频繁调用会显著降低性能

**使用场景**：
- 关键数据写入后（如数据库事务提交）
- 日志文件写入后（确保不丢失）

**示例**：
```cpp
File log = File::open("critical.log", 
                      FileMode::Write | FileMode::Create | FileMode::Append)
                      .value();

log.write_all("CRITICAL: System failure\n");
log.flush();  // 立即持久化,避免崩溃时丢失
```

---

## 定位操作

### seek

**签名**：
```cpp
[[nodiscard]] mine::core::Result<size_t>
seek(int64_t offset, FileSeekOrigin origin = FileSeekOrigin::Begin) noexcept;
```

**描述**：移动文件内部位置指针。

**参数**：
- `offset`：偏移量（字节,可为负数）
- `origin`：参考位置

**返回值**：
- **成功**：新位置（从文件头计数,单位：字节）
- **失败**：错误状态

**特殊情况**：
- 允许 seek 到文件末尾之后（扩展文件）
- 负偏移量仅在 `Current`/`End` 有效

**示例**：
```cpp
File file = File::open("data.bin", FileMode::ReadWrite).value();

// 定位到文件开头第 100 字节
auto pos1 = file.seek(100, FileSeekOrigin::Begin);

// 向后移动 50 字节
auto pos2 = file.seek(50, FileSeekOrigin::Current);

// 定位到文件末尾前 10 字节
auto pos3 = file.seek(-10, FileSeekOrigin::End);

// 定位到文件末尾
auto end = file.seek(0, FileSeekOrigin::End);
```

---

### tell

**签名**：
```cpp
[[nodiscard]] mine::core::Result<size_t> tell() const noexcept;
```

**描述**：获取当前文件位置。

**返回值**：
- **成功**：从文件头开始的偏移量（字节）
- **失败**：错误状态

**示例**：
```cpp
File file = File::open("data.bin", FileMode::Read).value();

auto pos1 = file.tell();  // 初始位置：0

file.read(buffer);
auto pos2 = file.tell();  // 读取后位置

printf("当前位置: %zu\n", pos2.value());
```

---

## 大小操作

### size

**签名**：
```cpp
[[nodiscard]] mine::core::Result<size_t> size() const noexcept;
```

**描述**：获取文件大小。

**返回值**：
- **成功**：文件大小（字节）
- **失败**：错误状态

**前置条件**：文件必须已打开

**示例**：
```cpp
File file = File::open("data.bin", FileMode::Read).value();

auto size_result = file.size();
if (size_result) {
    printf("文件大小: %zu 字节\n", size_result.value());
}
```

---

### set_size

**签名**：
```cpp
mine::core::Status set_size(size_t new_size) noexcept;
```

**描述**：设置文件大小（截断或扩展）。

**参数**：
- `new_size`：新的文件大小（字节）

**返回值**：
- **成功**：`Status::success()`
- **失败**：错误状态

**行为**：
- `new_size < 当前大小`：截断文件
- `new_size > 当前大小`：扩展文件（填充 0）

**前置条件**：文件必须以写模式打开

**示例**：
```cpp
File file = File::open("sparse.dat", FileMode::ReadWrite | FileMode::Create).value();

// 创建 1MB 稀疏文件
file.set_size(1024 * 1024);

// 截断到 512KB
file.set_size(512 * 1024);
```

---

## 原始句柄

### native_handle

**签名**：
```cpp
[[nodiscard]] void* native_handle() const noexcept;
```

**描述**：获取原生操作系统文件句柄。

**返回值**：
- **Windows**：`HANDLE` 转换为 `void*`
- **POSIX**：`int fd` 转换为 `void*`（需强制转换回 `int`）

**用途**：
- 调用平台特定 API
- 与操作系统交互（如 `ioctl`、`DeviceIoControl`）

**警告**：
- 不要关闭此句柄（`File` 析构时会关闭）
- 不要在句柄上直接 I/O（会破坏 `File` 内部状态）

**示例**：
```cpp
#ifdef _WIN32
File file = File::open("device.dat", FileMode::ReadWrite).value();
HANDLE h = static_cast<HANDLE>(file.native_handle());

// 调用 Windows API
DWORD bytes_returned;
DeviceIoControl(h, IOCTL_CUSTOM_CMD, /*...*/);
#endif
```

---

## 性能考虑

### 缓冲 vs 无缓冲

**File 特性**：无用户态缓冲,直接调用系统 API。

| 场景 | 推荐方案 |
|------|----------|
| 大块读写（≥4KB） | `File::read()` / `write()` |
| 小块频繁读写 | `PipeStream`（内置 8KB 缓冲） |
| 行处理 | `PipeStream::read_line()` |
| 随机访问 | `MemMap`（内存映射） |

**示例**：
```cpp
// ❌ 低效：频繁小写入
File file = File::open("log.txt", FileMode::DefaultWrite).value();
for (int i = 0; i < 10000; ++i) {
    file.write_all("log entry\n");  // 每次一个系统调用
}

// ✅ 高效：使用 PipeStream 缓冲
PipeStream stream = PipeStream::open_write("log.txt").value();
for (int i = 0; i < 10000; ++i) {
    stream.write_line("log entry");  // 缓冲后批量写入
}
```

### 短读/短写处理

**短读/短写**：实际读写字节数小于请求数,但不是错误。

**原因**：
- 管道/套接字缓冲区不足
- 信号中断
- 底层设备限制

**推荐**：
- 使用 `read_all()` / `write_all()` 处理短读/短写
- 或手动循环调用 `read()` / `write()`

---

## 错误处理

### 常见错误码

| 错误码 | 原因 | 处理建议 |
|--------|------|----------|
| `NotFound` | 文件不存在 | 检查路径,添加 `Create` 标志 |
| `PermissionDenied` | 无访问权限 | 检查文件权限,以管理员运行 |
| `AlreadyExists` | 文件已存在（`Exclusive` 模式） | 删除旧文件或使用其他文件名 |
| `InvalidArgument` | 路径非法 | 检查路径格式 |
| `IoError` | 磁盘满/硬件故障 | 释放磁盘空间,检查硬件 |

### 错误检查模式

```cpp
// 方式 1：Result 模式
auto result = File::open("data.txt", FileMode::Read);
if (!result) {
    handle_error(result.error());
    return;
}
File file = std::move(result.value());

// 方式 2：MINE_TRY 宏（需要当前函数返回 Result）
File file = MINE_TRY(File::open("data.txt", FileMode::Read));

// 方式 3：value_or 提供回退
File file = File::open("data.txt", FileMode::Read)
    .value_or(File());  // 打开失败返回空句柄
```

---

## 最佳实践

### 1. 显式检查错误

```cpp
// ✅ 推荐
auto result = file.write_all(data);
if (!result) {
    log_error("写入失败: %s", result.message());
    return result;
}

// ❌ 不推荐
file.write_all(data);  // 忽略错误
```

### 2. 关键操作后 flush

```cpp
// ✅ 确保数据持久化
file.write_all(transaction_data);
file.flush();  // 立即同步
file.close();  // 显式关闭并检查错误
```

### 3. 利用 RAII 自动关闭

```cpp
// ✅ 作用域自动管理
{
    File temp = File::open("temp.dat", FileMode::DefaultWrite).value();
    temp.write_all("data");
    // 自动关闭
}

// ✅ 异常安全（虽然禁用异常,但模式一致）
Result<void> process() {
    File file = MINE_TRY(File::open("data.txt", FileMode::Read));
    // ... 处理 ...
    return success();
    // 自动关闭,即使提前返回
}
```

### 4. 选择合适的打开模式

```cpp
// ✅ 明确意图
File log = File::open("app.log", 
                      FileMode::Write | FileMode::Create | FileMode::Append);

// ❌ 模糊意图
File log = File::open("app.log", FileMode::Write);  // 会覆盖原内容！
```

---

## 平台差异

### 文本模式

**`mine::io::File` 无文本模式**：
- 所有读写都是二进制模式
- 不会自动转换 `\n` ↔ `\r\n`
- 需要手动处理平台换行符差异

**示例**：
```cpp
// 跨平台换行符
#ifdef _WIN32
constexpr const char* newline = "\r\n";
#else
constexpr const char* newline = "\n";
#endif

file.write_all("line 1");
file.write_all(newline);
file.write_all("line 2");
```

### 文件锁

**当前不支持文件锁**：
- 并发访问需要应用层协调
- 未来版本可能添加 `lock()` / `unlock()` 方法

---

## 相关类型

- **`mine::io::Path`**：路径抽象
- **`mine::io::FileSystem`**：文件系统操作（删除/复制/移动）
- **`mine::io::PipeStream`**：缓冲流（适合频繁小读写）
- **`mine::io::MemMap`**：内存映射文件（适合随机访问）
- **`mine::core::Result<T>`**：错误返回类型
- **`mine::core::Span<T>`**：缓冲区视图

---

## 另见

- [Path.md](01-Path.md) — 路径操作
- [FileSystem.md](03-FileSystem.md) — 文件系统操作
- [MemMap.md](04-MemMap.md) — 内存映射
- [PipeStream.md](05-PipeStream.md) — 缓冲流
- [Io.md](06-Io.md) — 模块总览
