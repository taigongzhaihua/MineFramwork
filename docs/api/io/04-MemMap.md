# MemMap 类详细接口文档

## 概述

**模块**：`mine.io`  
**头文件**：`<mine/io/MemMap.h>`  
**命名空间**：`mine::io`

**用途**：将文件内容映射到进程虚拟地址空间,提供类似数组的零拷贝随机访问。

**核心特性**：
- **零拷贝访问**：直接访问文件内容,无需读入内存
- **RAII 管理**：析构自动解除映射,无需手动管理生命周期
- **仅移动语义**：禁用拷贝,通过移动转移所有权
- **多种映射模式**：支持只读、读写、写时复制（COW）
- **部分映射**：可以只映射文件的指定范围（offset + length）
- **同步控制**：提供显式 flush 操作将修改写回磁盘

**设计动机**：

传统文件 I/O（`read`/`write`）存在以下开销:
1. **数据拷贝**：数据需要从内核缓冲区拷贝到用户空间
2. **系统调用**：每次读写需要陷入内核
3. **缓冲管理**：需要手动管理缓冲区大小和位置

`mine::io::MemMap` 利用操作系统的内存映射机制:
- 文件内容直接映射到虚拟地址空间,访问时由 OS 按需分页加载
- 多个进程可以共享同一个映射（共享内存）
- 写时复制（COW）模式支持私有修改,不影响原文件

**适用场景**：
- 大文件随机访问（如数据库索引、大型配置文件）
- 进程间共享内存通信
- 只读资源文件加载（纹理、模型、音频）
- 性能关键的数据处理（避免拷贝）

**不适用场景**：
- 小文件（< 4KB）：映射开销大于直接读取
- 顺序读取：`PipeStream` 或 `File` 更高效
- 需要压缩/加密的文件：映射无法透明处理

---

## 类定义

### MemMapMode

**枚举定义**：
```cpp
enum class MemMapMode : uint32_t {
    Read        = 1 << 0,  ///< 只读映射
    Write       = 1 << 1,  ///< 读写映射
    ReadWrite   = Read | Write,
    CopyOnWrite = 1 << 2,  ///< 写时复制（修改不写回文件）
    Exec        = 1 << 3,  ///< 允许执行（JIT 代码）
    Private     = CopyOnWrite,
    PrivateCopy = Read | CopyOnWrite,
};
```

**描述**：内存映射访问模式。

**模式说明**：
- **Read**：只读,尝试写入会导致段错误
- **Write**：可写,修改会同步到文件
- **ReadWrite**：读写,修改同步到文件
- **CopyOnWrite**：写时复制,修改不影响原文件（私有拷贝）
- **Exec**：允许执行映射区域的代码（用于 JIT 编译器）
- **Private**：同 `CopyOnWrite`
- **PrivateCopy**：只读 + 写时复制（常用于加载只读资源）

---

### MemMap

**类定义**：
```cpp
class MINE_IO_API MemMap {
public:
    MemMap() noexcept;
    MemMap(MemMap&& other) noexcept;
    MemMap& operator=(MemMap&& other) noexcept;
    MemMap(const MemMap&)            = delete;
    MemMap& operator=(const MemMap&) = delete;
    ~MemMap() noexcept;

    [[nodiscard]] static mine::core::Result<MemMap>
    open_read(const Path& path) noexcept;

    [[nodiscard]] static mine::core::Result<MemMap>
    open_read_write(const Path& path) noexcept;

    [[nodiscard]] static mine::core::Result<MemMap>
    open_with_options(const Path& path, MemMapMode mode,
                      uint64_t offset = 0, uint64_t length = 0) noexcept;

    [[nodiscard]] bool is_mapped() const noexcept;
    [[nodiscard]] void* data() noexcept;
    [[nodiscard]] const void* data() const noexcept;
    [[nodiscard]] uint64_t size() const noexcept;
    [[nodiscard]] mine::core::Span<char> span() noexcept;
    [[nodiscard]] mine::core::Span<const char> span() const noexcept;
    [[nodiscard]] mine::core::Span<uint8_t> byte_span() noexcept;
    [[nodiscard]] mine::core::Span<const uint8_t> byte_span() const noexcept;

    mine::core::Status flush(uint64_t offset = 0, uint64_t length = 0) noexcept;
    mine::core::Status flush_sync(uint64_t offset = 0, uint64_t length = 0) noexcept;
    mine::core::Status close() noexcept;
};
```

---

## 构造与析构

### 默认构造

**签名**：
```cpp
MemMap() noexcept;
```

**描述**：创建一个空的未映射对象。

**示例**：
```cpp
MemMap mmap;  // 空映射
assert(!mmap.is_mapped());
```

---

### 移动构造/赋值

**签名**：
```cpp
MemMap(MemMap&& other) noexcept;
MemMap& operator=(MemMap&& other) noexcept;
```

**描述**：转移映射所有权,原对象变为空映射。

**示例**：
```cpp
auto mmap1 = MemMap::open_read("data.bin").value();
MemMap mmap2 = std::move(mmap1);  // mmap1 不再有效
assert(!mmap1.is_mapped());
assert(mmap2.is_mapped());
```

---

### 析构

**签名**：
```cpp
~MemMap() noexcept;
```

**描述**：自动解除映射并释放资源。

**示例**：
```cpp
{
    auto mmap = MemMap::open_read("config.json").value();
    // 使用 mmap...
}  // 此处自动解除映射
```

---

## 打开映射

### open_read

**签名**：
```cpp
[[nodiscard]] static mine::core::Result<MemMap>
open_read(const Path& path) noexcept;
```

**描述**：以只读模式映射整个文件。

**参数**：
- `path`：要映射的文件路径

**返回值**：`Result<MemMap>`,成功返回映射对象,失败返回错误。

**示例**：
```cpp
auto mmap = MemMap::open_read("data.bin");
if (!mmap.ok()) {
    std::cerr << "映射文件失败: " << mmap.status().message() << std::endl;
    return;
}

auto data = mmap.value().span();
// 只读访问 data...
```

---

### open_read_write

**签名**：
```cpp
[[nodiscard]] static mine::core::Result<MemMap>
open_read_write(const Path& path) noexcept;
```

**描述**：以读写模式映射整个文件。

**参数**：
- `path`：要映射的文件路径

**返回值**：`Result<MemMap>`,成功返回映射对象。

**注意**：修改会同步到原文件。使用 `flush()` 控制同步时机。

**示例**：
```cpp
auto mmap = MemMap::open_read_write("config.dat").value();
auto data = mmap.span();

// 修改数据
std::memcpy(data.data(), "HEADER", 6);

// 立即同步到磁盘
mmap.flush();
```

---

### open_with_options

**签名**：
```cpp
[[nodiscard]] static mine::core::Result<MemMap>
open_with_options(const Path& path, MemMapMode mode,
                  uint64_t offset = 0, uint64_t length = 0) noexcept;
```

**描述**：以指定模式和范围映射文件。

**参数**：
- `path`：要映射的文件路径
- `mode`：映射模式（`MemMapMode` 枚举）
- `offset`：文件偏移（字节）,必须是页大小的倍数（通常 4KB）
- `length`：映射长度（字节）,0 表示从 offset 到文件末尾

**返回值**：`Result<MemMap>`,成功返回映射对象。

**示例**：
```cpp
// 只映射文件的前 1MB
auto mmap = MemMap::open_with_options(
    "large.bin", 
    MemMapMode::Read, 
    0,           // 从文件开头
    1024 * 1024  // 映射 1MB
).value();

// 写时复制模式（修改不影响原文件）
auto mmap_cow = MemMap::open_with_options(
    "template.dat",
    MemMapMode::CopyOnWrite,
    0, 0
).value();
auto data = mmap_cow.span();
data[0] = 'X';  // 修改不会写回原文件
```

---

## 查询操作

### is_mapped

**签名**：
```cpp
[[nodiscard]] bool is_mapped() const noexcept;
```

**描述**：检查映射是否有效。

**返回值**：`true` 表示映射有效,`false` 表示未映射。

**示例**：
```cpp
MemMap mmap;
assert(!mmap.is_mapped());

mmap = MemMap::open_read("data.bin").value();
assert(mmap.is_mapped());
```

---

### data

**签名**：
```cpp
[[nodiscard]] void* data() noexcept;
[[nodiscard]] const void* data() const noexcept;
```

**描述**：返回映射区域的起始地址指针。

**返回值**：映射内存的指针,未映射时返回 `nullptr`。

**注意**：推荐使用 `span()` 或 `byte_span()` 以获得边界检查。

**示例**：
```cpp
auto mmap = MemMap::open_read("binary.dat").value();
const char* ptr = static_cast<const char*>(mmap.data());
// 直接访问 ptr[0], ptr[1], ...
```

---

### size

**签名**：
```cpp
[[nodiscard]] uint64_t size() const noexcept;
```

**描述**：返回映射区域的大小（字节）。

**返回值**：映射大小,未映射时返回 0。

**示例**：
```cpp
auto mmap = MemMap::open_read("data.bin").value();
std::cout << "映射大小: " << mmap.size() / 1024 << " KB" << std::endl;
```

---

### span

**签名**：
```cpp
[[nodiscard]] mine::core::Span<char> span() noexcept;
[[nodiscard]] mine::core::Span<const char> span() const noexcept;
```

**描述**：以 `Span<char>` 视图访问映射区域。

**返回值**：包含指针和大小的 `Span` 对象,提供边界检查。

**示例**：
```cpp
auto mmap = MemMap::open_read("text.txt").value();
auto data = mmap.span();

// 安全访问,自动边界检查
for (size_t i = 0; i < data.size(); ++i) {
    std::cout << data[i];
}
```

---

### byte_span

**签名**：
```cpp
[[nodiscard]] mine::core::Span<uint8_t> byte_span() noexcept;
[[nodiscard]] mine::core::Span<const uint8_t> byte_span() const noexcept;
```

**描述**：以 `Span<uint8_t>` 视图访问映射区域（字节操作）。

**返回值**：包含字节指针和大小的 `Span` 对象。

**示例**：
```cpp
auto mmap = MemMap::open_read("image.png").value();
auto bytes = mmap.byte_span();

// 读取文件头
if (bytes.size() >= 8) {
    uint8_t png_sig[8] = {137, 80, 78, 71, 13, 10, 26, 10};
    if (std::memcmp(bytes.data(), png_sig, 8) == 0) {
        std::cout << "有效的 PNG 文件" << std::endl;
    }
}
```

---

## 同步操作

### flush

**签名**：
```cpp
mine::core::Status flush(uint64_t offset = 0, uint64_t length = 0) noexcept;
```

**描述**：将映射中的修改异步同步到原文件。

**参数**：
- `offset`：映射内偏移（字节）,默认 0（从头开始）
- `length`：刷新的字节数,默认 0（直到末尾）

**返回值**：`Status` 对象,成功返回 `ok()`。

**注意**：
- 仅对可写映射有效（`ReadWrite` 模式）
- 同步可能是异步的,OS 决定实际写入时机
- 写时复制模式无效果（修改不会写回原文件）

**示例**：
```cpp
auto mmap = MemMap::open_read_write("log.txt").value();
auto data = mmap.span();

// 修改数据
std::memcpy(data.data(), "LOG: ", 5);

// 异步刷新到磁盘
mmap.flush();
```

---

### flush_sync

**签名**：
```cpp
mine::core::Status flush_sync(uint64_t offset = 0, uint64_t length = 0) noexcept;
```

**描述**：将映射中的修改同步到原文件并阻塞等待完成。

**参数**：
- `offset`：映射内偏移（字节）
- `length`：刷新的字节数

**返回值**：`Status` 对象,成功返回 `ok()`。

**注意**：会阻塞直到数据真正写入磁盘,用于关键数据的持久化保证。

**示例**：
```cpp
auto mmap = MemMap::open_read_write("critical.db").value();
auto data = mmap.span();

// 写入关键数据
write_transaction(data);

// 同步刷新,确保数据持久化
auto st = mmap.flush_sync();
if (!st.ok()) {
    std::cerr << "刷新失败: " << st.message() << std::endl;
}
```

---

## 关闭操作

### close

**签名**：
```cpp
mine::core::Status close() noexcept;
```

**描述**：显式解除映射并释放资源。

**返回值**：`Status` 对象,成功返回 `ok()`。

**注意**：
- 析构时会自动调用,通常无需手动调用
- 关闭后 `is_mapped()` 返回 `false`,访问 `data()` 返回 `nullptr`

**示例**：
```cpp
auto mmap = MemMap::open_read("temp.dat").value();
// 使用 mmap...

auto st = mmap.close();
if (!st.ok()) {
    std::cerr << "关闭映射失败: " << st.message() << std::endl;
}
```

---

## 综合示例

### 大文件搜索

```cpp
#include <mine/io/MemMap.h>
#include <mine/io/Path.h>
#include <cstring>

using namespace mine::io;

// 在大文件中搜索字符串
size_t search_in_file(const Path& path, const char* needle) {
    auto mmap = MemMap::open_read(path);
    if (!mmap.ok()) {
        return 0;
    }

    auto data = mmap.value().span();
    size_t needle_len = std::strlen(needle);
    size_t count = 0;

    // 零拷贝搜索
    for (size_t i = 0; i + needle_len <= data.size(); ++i) {
        if (std::memcmp(data.data() + i, needle, needle_len) == 0) {
            ++count;
        }
    }

    return count;
}

int main() {
    size_t count = search_in_file("large_log.txt", "ERROR");
    std::cout << "找到 " << count << " 个错误" << std::endl;
}
```

---

### 修改二进制文件头

```cpp
#include <mine/io/MemMap.h>
#include <mine/io/Path.h>
#include <cstring>

using namespace mine::io;

// 原地修改二进制文件的头部
mine::core::Status patch_file_header(const Path& path, 
                                      const void* new_header, 
                                      size_t header_size) {
    auto mmap = MemMap::open_read_write(path);
    if (!mmap.ok()) {
        return mmap.status();
    }

    auto data = mmap.value().span();
    if (data.size() < header_size) {
        return mine::core::Status::error("文件太小");
    }

    // 原地修改
    std::memcpy(data.data(), new_header, header_size);

    // 同步到磁盘
    return mmap.value().flush_sync(0, header_size);
}
```

---

### 进程间共享内存

```cpp
#include <mine/io/MemMap.h>
#include <mine/io/FileSystem.h>
#include <mine/io/File.h>

using namespace mine::io;

// 创建共享内存文件
mine::core::Result<MemMap> create_shared_memory(const Path& path, size_t size) {
    // 1. 创建指定大小的文件
    {
        auto file = File::open(path, FileMode::Write | FileMode::Create | FileMode::Truncate);
        if (!file.ok()) {
            return file.status();
        }

        // 扩展文件到指定大小
        mine::core::Array<char> zeros(size, 0);
        auto st = file.value().write_all(mine::core::Span<const char>(zeros.data(), zeros.size()));
        if (!st.ok()) {
            return st;
        }
    }

    // 2. 映射为读写共享内存
    return MemMap::open_read_write(path);
}

// 进程 A：写入数据
void process_a() {
    auto mmap = create_shared_memory("/tmp/shared.mem", 4096).value();
    auto data = mmap.span();
    std::memcpy(data.data(), "Hello from process A", 21);
    mmap.flush_sync();
}

// 进程 B：读取数据
void process_b() {
    auto mmap = MemMap::open_read("/tmp/shared.mem").value();
    auto data = mmap.span();
    std::cout << "收到消息: " << data.data() << std::endl;
}
```

---

### 部分映射大文件

```cpp
#include <mine/io/MemMap.h>
#include <mine/io/FileSystem.h>

using namespace mine::io;

// 分块处理大文件
void process_large_file_in_chunks(const Path& path) {
    auto file_size = FileSystem::file_size(path).value();
    constexpr size_t chunk_size = 64 * 1024 * 1024;  // 64 MB

    for (uint64_t offset = 0; offset < file_size; offset += chunk_size) {
        size_t len = std::min(chunk_size, static_cast<size_t>(file_size - offset));
        
        // 只映射 64 MB 块
        auto mmap = MemMap::open_with_options(path, MemMapMode::Read, offset, len).value();
        auto data = mmap.span();
        
        // 处理当前块
        process_chunk(data);
        
        // 映射自动解除,继续下一块
    }
}
```

---

### 写时复制（私有修改）

```cpp
#include <mine/io/MemMap.h>

using namespace mine::io;

// 加载模板文件并私有修改
void process_template(const Path& template_path) {
    // 写时复制模式：修改不影响原文件
    auto mmap = MemMap::open_with_options(
        template_path,
        MemMapMode::CopyOnWrite,
        0, 0
    ).value();

    auto data = mmap.span();

    // 在内存中修改（不写回原文件）
    replace_placeholder(data, "{{NAME}}", "MineFramework");
    replace_placeholder(data, "{{VERSION}}", "1.0.0");

    // 使用修改后的数据...
    render(data);

    // 映射解除,修改丢失
}
```

---

## 性能考虑

### 映射粒度

- **小文件（< 4KB）**：映射开销大于直接读取,推荐使用 `File::read_all()`
- **中等文件（4KB - 1MB）**：视访问模式选择,随机访问用映射,顺序读取用流
- **大文件（> 1MB）**：映射优势明显,尤其是随机访问场景

---

### 页对齐

文件偏移（`offset` 参数）必须是页大小的倍数（通常 4KB = 4096）:

```cpp
// ❌ 错误：offset 不是页对齐
auto mmap = MemMap::open_with_options("file.bin", MemMapMode::Read, 1000, 8192);

// ✅ 正确：offset 页对齐
auto mmap = MemMap::open_with_options("file.bin", MemMapMode::Read, 4096, 8192);
```

---

### 预读优化

操作系统会自动预读后续页,但可以通过顺序访问触发更积极的预读:

```cpp
auto mmap = MemMap::open_read("sequential.dat").value();
auto data = mmap.span();

// 顺序访问触发预读
for (size_t i = 0; i < data.size(); ++i) {
    process(data[i]);
}
```

---

### 内存压力

映射不会立即占用物理内存,但访问时会触发缺页中断。大量映射可能导致页面抖动:

```cpp
// ❌ 避免：同时映射多个大文件
std::vector<MemMap> mmaps;
for (auto& path : large_files) {
    mmaps.push_back(MemMap::open_read(path).value());
}

// ✅ 推荐：按需映射,用完解除
for (auto& path : large_files) {
    auto mmap = MemMap::open_read(path).value();
    process(mmap.span());
    mmap.close();  // 显式解除映射
}
```

---

## 错误处理

### 映射失败原因

- **文件不存在**：`open_read()` 返回错误
- **权限不足**：无读/写权限
- **地址空间不足**：32 位进程映射大文件可能失败
- **文件被锁定**：某些平台不允许映射已锁定的文件

---

### 访问越界

`data()` 返回原始指针,无边界检查。推荐使用 `span()`:

```cpp
auto mmap = MemMap::open_read("small.txt").value();

// ❌ 不安全：可能越界
char* ptr = static_cast<char*>(mmap.data());
char c = ptr[1000000];  // 越界!

// ✅ 安全：自动边界检查
auto data = mmap.span();
if (1000000 < data.size()) {
    char c = data[1000000];
}
```

---

### 同步失败

`flush()` 或 `flush_sync()` 可能因磁盘满、权限等原因失败:

```cpp
auto mmap = MemMap::open_read_write("data.db").value();
auto data = mmap.span();

// 修改数据
modify(data);

// 检查同步结果
auto st = mmap.flush_sync();
if (!st.ok()) {
    std::cerr << "同步失败: " << st.message() << std::endl;
    // 回滚或重试...
}
```

---

## 最佳实践

### 1. 优先使用只读映射

只读映射更安全,且允许多个进程共享同一物理页:

```cpp
// ✅ 只读映射
auto mmap = MemMap::open_read("assets/texture.png").value();
```

---

### 2. 大文件使用部分映射

避免一次映射整个大文件,按需映射所需部分:

```cpp
// 只映射需要的范围
auto mmap = MemMap::open_with_options(
    "huge.bin", 
    MemMapMode::Read, 
    offset, 
    length
).value();
```

---

### 3. 写入后显式同步

可写映射修改后,调用 `flush_sync()` 确保持久化:

```cpp
auto mmap = MemMap::open_read_write("config.dat").value();
modify(mmap.span());
mmap.flush_sync();
```

---

### 4. 使用 Span 而非原始指针

`span()` 提供边界检查和类型安全:

```cpp
// ✅ 推荐
auto data = mmap.span();
for (char c : data) { ... }

// ❌ 避免
char* ptr = static_cast<char*>(mmap.data());
```

---

### 5. 写时复制用于模板处理

需要修改但不影响原文件时,使用 `CopyOnWrite`:

```cpp
auto mmap = MemMap::open_with_options(
    "template.html",
    MemMapMode::CopyOnWrite,
    0, 0
).value();
// 修改内存副本,原文件不变
```

---

## 平台差异

### Windows

- **实现**：使用 `CreateFileMapping` + `MapViewOfFile`
- **页大小**：通常 4KB（64KB 对齐性能更好）
- **限制**：32 位进程地址空间有限（约 2GB）

---

### Unix/Linux

- **实现**：使用 `mmap()` 系统调用
- **页大小**：通常 4KB（可通过 `getpagesize()` 查询）
- **特性**：支持 `MAP_HUGETLB`（大页）以提高 TLB 命中率

---

### 跨平台建议

- **页对齐**：始终使用 4KB 对齐的 offset
- **大文件**：64 位进程无地址空间限制,32 位进程需分块映射
- **共享内存**：两个平台都支持,但需确保文件路径可访问

---

## 另见

- [Path 类](01-Path.md) - 路径表示和操作
- [File 类](02-File.md) - 文件读写
- [FileSystem 命名空间](03-FileSystem.md) - 文件系统操作
- [PipeStream 类](05-PipeStream.md) - 缓冲流式 I/O
