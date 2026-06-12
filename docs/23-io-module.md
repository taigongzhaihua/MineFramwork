# 03 — mine.io 模块：文件与 I/O

> `mine.io` 是 MineFramework L0 基础模块之一，提供跨平台的文件系统操作、文件读写、
> 内存映射和流式 I/O 能力。所有 API 均采用 UTF-8 编码，遵循无异常错误处理约定。

---

## 3.1 模块职责

`mine.io` 模块负责提供统一的文件系统访问抽象，隔离底层平台差异（Windows/macOS/Linux），
核心职责包括：

- **路径操作**：跨平台路径构造、拼接、组件解析、规范化
- **文件读写**：顺序/随机访问、二进制/文本、只读/读写/追加模式
- **文件系统查询**：目录遍历、属性查询、存在性判断、路径解析
- **文件系统操作**：创建/删除文件/目录、复制/移动文件、符号链接
- **内存映射**：高性能零拷贝文件访问、只读/读写/写时复制映射
- **流式缓冲**：行读取/写入、追加操作、8KB 双缓冲优化

---

## 3.2 核心类型概览

| 类型 | 职责 | 头文件 |
|------|------|--------|
| `Path` | 路径字符串封装（SBO 31 字节） | `mine/io/Path.h` |
| `File` | 文件句柄与读写操作 | `mine/io/File.h` |
| `FileSystem` | 文件系统查询与操作 | `mine/io/FileSystem.h` |
| `MemMap` | 内存映射文件视图 | `mine/io/MemMap.h` |
| `PipeStream` | 缓冲流式读写（行处理） | `mine/io/PipeStream.h` |

所有类型均支持移动语义，禁用拷贝（RAII 生命周期管理）。

---

## 3.3 Path — 路径操作

### 3.3.1 设计要点

- **SBO 优化**：内部使用 31 字节小字符串优化，短路径无堆分配
- **自动规范化**：Windows 平台自动转换 `/` 为 `\`，其他平台保持 `/`
- **UTF-8 编码**：所有路径以 UTF-8 存储，Windows API 调用时自动转 UTF-16
- **值语义**：支持移动构造/赋值，禁用拷贝（需要显式 `clone()`）

### 3.3.2 核心 API

#### 构造与转换

```cpp
// 从字符串视图构造
Path p1 = Path::from("C:/Users/alice/doc.txt");
Path p2 = Path::from(u8"文档/测试.txt");

// 拼接路径
Path base = Path::from("C:/Projects");
Path full = base.append("mine/src");  // C:/Projects/mine/src

// 移动语义
Path moved = std::move(full);
```

#### 组件查询

```cpp
Path p = Path::from("/home/bob/report.pdf");

p.filename();       // "report.pdf"
p.stem();          // "report"
p.extension();     // ".pdf"
p.parent();        // "/home/bob"
p.root();          // "/" (POSIX) / "C:\" (Windows)

p.is_absolute();   // true
p.is_relative();   // false
```

#### 路径修改

```cpp
Path p = Path::from("data/old_name.txt");

p.replace_filename("new_name.txt");  // data/new_name.txt
p.replace_extension(".md");          // data/new_name.md
p.remove_filename();                 // data/
```

#### 比较与规范化

```cpp
Path p1 = Path::from("./docs/../src/main.cpp");
Path p2 = Path::from("src/main.cpp");

// 字符串比较（区分大小写）
p1.compare(p2);              // 非 0（未规范化不相等）

// 规范化后比较
Path norm = p1.normalized(); // "src/main.cpp"
norm.compare(p2);            // 0（相等）

// 转换为字符串
StringView sv = p2.as_string_view();
String s = p2.to_string();
```

---

## 3.4 File — 文件读写

### 3.4.1 设计要点

- **RAII 管理**：析构自动关闭文件句柄
- **模式显式**：通过 `AccessMode` 和 `CreateMode` 明确读写权限和创建行为
- **无缓冲 I/O**：直接调用系统 API，无用户态缓冲（需要缓冲请使用 `PipeStream`）
- **错误返回**：所有操作返回 `Result<T>` 或 `Status`，无异常

### 3.4.2 打开模式

```cpp
enum class AccessMode {
    Read,        // 只读
    Write,       // 只写
    ReadWrite    // 读写
};

enum class CreateMode {
    Open,        // 打开已存在文件（不存在则失败）
    Create,      // 创建新文件（已存在则失败）
    OpenOrCreate,// 打开或创建
    Truncate,    // 截断已存在文件（不存在则创建）
    Append       // 追加模式（自动定位到末尾）
};
```

### 3.4.3 核心 API

#### 打开与创建

```cpp
// 只读打开
Result<File> f1 = File::open(Path::from("config.json"));

// 写入模式创建
Result<File> f2 = File::create(Path::from("output.log"));

// 追加模式
Result<File> f3 = File::append(Path::from("trace.log"));

// 自定义模式
Result<File> f4 = File::open_with_access(
    Path::from("data.bin"),
    AccessMode::ReadWrite,
    CreateMode::OpenOrCreate
);

if (!f4) {
    // 错误处理
    return f4.error();
}
```

#### 读取操作

```cpp
File file = File::open(Path::from("input.dat")).value();

// 读取固定字节
char buffer[256];
Result<size_t> read_result = file.read(buffer, sizeof(buffer));
if (read_result) {
    size_t bytes_read = read_result.value();
    // 处理数据...
}

// 读取全部内容（注意：大文件应使用流式读取）
Result<Vector<uint8_t>> content = file.read_all();

// 获取文件大小
Result<uint64_t> size = file.size();
```

#### 写入操作

```cpp
File file = File::create(Path::from("output.txt")).value();

// 写入字节
const char* msg = "Hello, MineFramework!";
Status s1 = file.write(msg, strlen(msg));

// 写入 Span
Span<const uint8_t> data = {...};
Status s2 = file.write(data);

// 刷新到磁盘
file.flush();  // 确保数据持久化
```

#### 随机访问

```cpp
File file = File::open_with_access(
    Path::from("random.bin"),
    AccessMode::ReadWrite,
    CreateMode::Open
).value();

// 定位到指定位置
Status s1 = file.seek(1024);  // 从文件开头偏移 1024 字节

// 获取当前位置
Result<uint64_t> pos = file.position();

// 定位到末尾
Result<uint64_t> end_pos = file.size();
file.seek(end_pos.value());
```

---

## 3.5 FileSystem — 文件系统操作

### 3.5.1 设计要点

- **静态方法**：所有 API 均为静态函数，无需实例化
- **原子操作**：支持事务性操作（如 `copy_with_overwrite`）
- **符号链接感知**：可选择是否跟随符号链接
- **迭代器模式**：目录遍历使用回调函数而非迭代器（避免跨 DLL ABI 问题）

### 3.5.2 核心 API

#### 路径查询

```cpp
// 存在性判断
bool exists = FileSystem::exists(Path::from("config.json"));

// 类型判断
bool is_dir = FileSystem::is_directory(Path::from("src"));
bool is_file = FileSystem::is_file(Path::from("main.cpp"));

// 绝对路径转换
Result<Path> abs = FileSystem::absolute(Path::from("./docs"));
// 成功："/home/alice/project/docs"

// 规范路径（解析 . 和 ..）
Result<Path> canonical = FileSystem::canonical(Path::from("../src"));
// 成功："/home/alice/src"（必须存在）
```

#### 属性查询

```cpp
// 文件大小
Result<uint64_t> size = FileSystem::file_size(Path::from("data.bin"));

// 最后修改时间（Unix 时间戳秒）
Result<uint64_t> mtime = FileSystem::last_write_time(Path::from("log.txt"));
```

#### 目录遍历

```cpp
Path dir = Path::from("C:/Projects");

// 枚举目录项
Status s = FileSystem::enumerate_directory(
    dir,
    [](const FileSystem::DirectoryEntry& entry) -> bool {
        // entry.name: 文件名（不含路径）
        // entry.is_directory: 是否为目录
        // entry.size: 文件大小（目录为 0）
        
        if (entry.is_directory) {
            printf("[DIR]  %s\n", entry.name.c_str());
        } else {
            printf("[FILE] %s (%llu bytes)\n", 
                   entry.name.c_str(), entry.size);
        }
        
        return true;  // 继续枚举（返回 false 停止）
    }
);
```

#### 创建与删除

```cpp
// 创建目录（父目录必须存在）
Status s1 = FileSystem::create_directory(Path::from("output"));

// 递归创建目录
Status s2 = FileSystem::create_directories(Path::from("a/b/c"));

// 删除文件
Status s3 = FileSystem::remove(Path::from("temp.txt"));

// 删除目录（必须为空）
Status s4 = FileSystem::remove(Path::from("empty_dir"));

// 递归删除目录树
Status s5 = FileSystem::remove_all(Path::from("old_data"));
```

#### 复制与移动

```cpp
Path src = Path::from("original.txt");
Path dst = Path::from("backup.txt");

// 复制文件（目标不存在）
Status s1 = FileSystem::copy(src, dst);

// 强制覆盖复制
Status s2 = FileSystem::copy(src, dst, 
                             FileSystem::CopyOptions::OverwriteExisting);

// 移动/重命名文件
Status s3 = FileSystem::rename(src, dst);
```

---

## 3.6 MemMap — 内存映射文件

### 3.6.1 设计要点

- **零拷贝访问**：文件内容直接映射到进程地址空间，避免 `read()` 系统调用
- **多种模式**：只读、读写、写时复制（COW）
- **RAII 生命周期**：析构自动解除映射并同步修改（读写模式）
- **性能优化**：适合大文件随机访问、多次访问同一数据的场景

### 3.6.2 映射模式

```cpp
enum class MapMode {
    ReadOnly,       // 只读映射（最常用）
    ReadWrite,      // 读写映射（修改同步到文件）
    CopyOnWrite     // 写时复制（修改不影响文件）
};
```

### 3.6.3 核心 API

#### 创建映射

```cpp
// 只读映射（推荐用于配置文件、资源文件）
Result<MemMap> map1 = MemMap::map_file(
    Path::from("large_data.bin"),
    MemMap::MapMode::ReadOnly
);

if (map1) {
    Span<const uint8_t> data = map1->data();
    // 直接访问文件内容，无需 read() 调用
    uint32_t magic = *reinterpret_cast<const uint32_t*>(data.data());
}

// 读写映射（修改会写回文件）
Result<MemMap> map2 = MemMap::map_file(
    Path::from("cache.dat"),
    MemMap::MapMode::ReadWrite
);

if (map2) {
    Span<uint8_t> writable = map2->data_mut();
    writable[0] = 0xFF;  // 修改会反映到文件
    map2->flush();       // 显式同步（析构时自动同步）
}
```

#### 访问数据

```cpp
MemMap map = MemMap::map_file(
    Path::from("index.bin"),
    MemMap::MapMode::ReadOnly
).value();

// 只读访问
Span<const uint8_t> bytes = map.data();
size_t size = map.size();

// 类型化访问（需保证对齐）
struct Header {
    uint32_t magic;
    uint32_t version;
};
const Header* hdr = reinterpret_cast<const Header*>(bytes.data());
```

#### 同步修改

```cpp
MemMap map = MemMap::map_file(
    Path::from("mutable.dat"),
    MemMap::MapMode::ReadWrite
).value();

Span<uint8_t> data = map.data_mut();
// ... 修改数据 ...

// 强制刷新到磁盘（析构时会自动调用）
Status s = map.flush();
```

---

## 3.7 PipeStream — 缓冲流

### 3.7.1 设计要点

- **双缓冲**：内部维护 8KB 读缓冲 + 8KB 写缓冲
- **行处理**：支持按分隔符读取行（默认 `\n`，支持 `\r\n`）
- **自动刷新**：析构时自动刷新写缓冲
- **追加模式**：自动定位到文件末尾写入

### 3.7.2 核心 API

#### 创建流

```cpp
// 读取流
Result<PipeStream> reader = PipeStream::open_read(
    Path::from("input.txt")
);

// 写入流
Result<PipeStream> writer = PipeStream::open_write(
    Path::from("output.txt")
);

// 追加流
Result<PipeStream> appender = PipeStream::open_append(
    Path::from("log.txt")
);
```

#### 行读取

```cpp
PipeStream stream = PipeStream::open_read(
    Path::from("config.ini")
).value();

String line_buffer;
while (true) {
    Result<bool> result = stream.read_line(line_buffer);
    
    if (!result) {
        // 错误处理
        break;
    }
    
    if (!result.value()) {
        // EOF
        break;
    }
    
    // 处理行内容（line_buffer 不包含 \n）
    process_line(line_buffer);
}
```

#### 行写入

```cpp
PipeStream stream = PipeStream::open_write(
    Path::from("report.txt")
).value();

// 写入行（自动添加 \n）
Status s1 = stream.write_line("第一行内容");
Status s2 = stream.write_line("第二行内容");

// 手动刷新（析构时自动刷新）
stream.flush();
```

#### 二进制读写

```cpp
// 读取
PipeStream reader = PipeStream::open_read(
    Path::from("data.bin")
).value();

uint8_t buffer[1024];
Result<size_t> read_result = reader.read(buffer, sizeof(buffer));

// 写入
PipeStream writer = PipeStream::open_write(
    Path::from("output.bin")
).value();

Span<const uint8_t> data = {...};
Status write_status = writer.write(data);
```

---

## 3.8 错误处理

### 3.8.1 错误类型

所有 I/O 操作返回 `Result<T>` 或 `Status`，错误码定义在 `mine.core` 模块：

```cpp
enum class StatusCode {
    Success,            // 成功
    NotFound,           // 文件/目录不存在
    AlreadyExists,      // 文件/目录已存在
    PermissionDenied,   // 权限不足
    InvalidArgument,    // 参数无效
    IoError,            // 通用 I/O 错误
    OutOfMemory,        // 内存不足
    // ...
};
```

### 3.8.2 错误检查模式

```cpp
// 方式 1：显式检查
Result<File> result = File::open(path);
if (result) {
    File file = std::move(result.value());
    // 使用文件...
} else {
    Status err = result.error();
    printf("打开失败: %s\n", err.message());
}

// 方式 2：value_or 提供默认值
File file = File::open(path).value_or(fallback_file);

// 方式 3：MINE_TRY 宏传播错误（需要当前函数返回 Result<T>）
File file = MINE_TRY(File::open(path));
```

---

## 3.9 平台支持

| 平台 | 实现后端 | 编码转换 | 状态 |
|------|----------|----------|------|
| Windows | Win32 API (`CreateFileW`, `FindFirstFileW`) | UTF-8 ↔ UTF-16LE | ✅ 已实现 |
| macOS | POSIX API (`open`, `readdir`) | UTF-8 直通 | 🚧 规划中 |
| Linux | POSIX API | UTF-8 直通 | 🚧 规划中 |
| Android | POSIX + Asset Manager | UTF-8 直通 | 🚧 规划中 |
| iOS | POSIX + NSFileManager | UTF-8 直通 | 🚧 规划中 |

当前版本仅实现 Windows 平台，其他平台后续通过条件编译添加（使用相同公共 API）。

---

## 3.10 性能考虑

### 3.10.1 选择合适的 I/O 方式

| 场景 | 推荐类型 | 原因 |
|------|----------|------|
| 小文件一次性读取 | `File::read_all()` | 简单高效，一次系统调用 |
| 大文件顺序读取 | `PipeStream` | 8KB 缓冲减少系统调用次数 |
| 行处理（日志、配置） | `PipeStream::read_line()` | 自动处理行边界，减少字符串拷贝 |
| 随机访问（索引、数据库） | `MemMap` | 零拷贝，按需分页加载 |
| 频繁小写入 | `PipeStream` | 缓冲聚合写入，减少系统调用 |
| 大块写入 | `File::write()` | 直接写入，无缓冲开销 |

### 3.10.2 Path 内存分配

```cpp
// ✅ 推荐：SBO 优化，无堆分配
Path short_path = Path::from("config.json");  // 11 字符 < 31

// ⚠️ 注意：长路径会触发堆分配
Path long_path = Path::from("C:/Very/Long/Path/To/Some/Deep/Directory/file.txt");

// 建议：频繁使用的路径用 Path 缓存，避免重复构造
static Path cache_dir = Path::from("C:/AppData/Cache");
```

### 3.10.3 MemMap 适用场景

**适合**：
- 大文件（> 1MB）随机访问（如索引、查找表）
- 多次访问同一数据（如配置文件、资源包）
- 跨进程共享只读数据

**不适合**：
- 小文件（< 4KB）：页粒度浪费内存
- 顺序单次读取：`read()` 更简单高效
- 频繁小修改：页同步开销大

---

## 3.11 使用示例

### 3.11.1 读取配置文件

```cpp
Result<String> read_config(const Path& config_path) {
    // 打开文件
    auto file_result = File::open(config_path);
    if (!file_result) {
        return file_result.error();
    }
    
    File file = std::move(file_result.value());
    
    // 读取全部内容
    auto content_result = file.read_all();
    if (!content_result) {
        return content_result.error();
    }
    
    Vector<uint8_t> bytes = std::move(content_result.value());
    
    // 转换为字符串（假设 UTF-8 编码）
    return String(reinterpret_cast<const char*>(bytes.data()), 
                  bytes.size());
}
```

### 3.11.2 写入日志

```cpp
Status append_log(const Path& log_path, StringView message) {
    // 以追加模式打开
    auto stream_result = PipeStream::open_append(log_path);
    if (!stream_result) {
        return stream_result.error();
    }
    
    PipeStream stream = std::move(stream_result.value());
    
    // 添加时间戳
    auto now = std::time(nullptr);
    char timestamp[64];
    std::strftime(timestamp, sizeof(timestamp), 
                  "[%Y-%m-%d %H:%M:%S] ", std::localtime(&now));
    
    // 写入日志行
    MINE_TRY(stream.write_line(String(timestamp) + message));
    
    return Status::success();
}
```

### 3.11.3 遍历目录树

```cpp
void walk_directory_tree(const Path& root, int depth = 0) {
    String indent(depth * 2, ' ');
    
    FileSystem::enumerate_directory(
        root,
        [&](const FileSystem::DirectoryEntry& entry) -> bool {
            printf("%s- %s\n", indent.c_str(), entry.name.c_str());
            
            if (entry.is_directory) {
                Path subdir = root.clone().append(entry.name);
                walk_directory_tree(subdir, depth + 1);
            }
            
            return true;  // 继续枚举
        }
    );
}
```

### 3.11.4 内存映射大文件解析

```cpp
struct BinaryIndex {
    uint32_t magic;
    uint32_t entry_count;
    // 后续是 entry_count 个条目...
};

Result<void> parse_index_file(const Path& index_path) {
    // 只读映射
    auto map_result = MemMap::map_file(
        index_path, 
        MemMap::MapMode::ReadOnly
    );
    if (!map_result) {
        return map_result.error();
    }
    
    MemMap map = std::move(map_result.value());
    Span<const uint8_t> data = map.data();
    
    // 检查大小
    if (data.size() < sizeof(BinaryIndex)) {
        return Status::invalid_argument("文件过小");
    }
    
    // 类型安全访问
    const auto* index = reinterpret_cast<const BinaryIndex*>(
        data.data()
    );
    
    if (index->magic != 0x494E4458) {  // 'INDX'
        return Status::invalid_argument("魔数不匹配");
    }
    
    printf("索引包含 %u 个条目\n", index->entry_count);
    
    // 零拷贝访问条目数据...
    
    return Status::success();
}
```

---

## 3.12 编码约定

### 3.12.1 UTF-8 强制规则

- 所有路径字符串必须使用 UTF-8 编码
- Windows 平台自动转换为 UTF-16（`CreateFileW` 等宽字符 API）
- 源码文件必须以 UTF-8 + BOM 或 UTF-8 无 BOM 保存
- 字符串字面量使用 `u8"中文路径"` 前缀确保编码正确

### 3.12.2 路径分隔符

- API 接受 `/` 和 `\` 两种分隔符（Windows 自动规范化为 `\`）
- 推荐统一使用 `/`（跨平台兼容）
- `Path::normalized()` 会将 `..` 和 `.` 解析为绝对路径

### 3.12.3 错误处理

- 禁止使用异常传播错误
- 必须检查所有 `Result<T>` 返回值
- 使用 `MINE_TRY` 宏简化错误传播链

---

## 3.13 测试覆盖

### 3.13.1 单元测试统计

| 测试套件 | 测试用例数 | 覆盖率 |
|----------|-----------|--------|
| Path | 23 | 构造/组件/拼接/比较/修改/规范化/移动语义 |
| File | 7 | 读写往返/定位/大小/全部读取/移动语义/错误处理 |
| FileSystem | 12 | 目录操作/文件操作/路径工具/枚举/属性 |
| MemMap | 3 | 映射访问/字节视图/移动语义 |
| PipeStream | 5 | 流读/流写/行读/行写/追加 |
| **总计** | **50** | **全部通过** |

### 3.13.2 运行测试

```bash
# 构建测试
xmake build mine.io

# 运行所有测试
xmake run mine.io_test

# 运行特定测试套件
xmake run mine.io_test -- --test-case="Path/*"
```

---

## 3.14 构建配置

### 3.14.1 xmake 依赖声明

```lua
-- 在上层模块的 xmake.lua 中
target("my_app")
    add_deps("mine.io.api")  -- 添加 mine.io 依赖
```

### 3.14.2 CMake 依赖声明

```cmake
# 在 CMakeLists.txt 中
find_package(MineFramework REQUIRED COMPONENTS io)
target_link_libraries(my_app PRIVATE Mine::io)
```

---

## 3.15 已知限制

| 限制 | 影响 | 规划 |
|------|------|------|
| 仅支持 Windows | 其他平台暂不可用 | M0.2 添加 POSIX 实现 |
| 无异步 I/O | 阻塞式调用可能卡主线程 | M0.3 集成 `mine.async` |
| 无流式压缩 | 不支持 gzip/zstd 透明解压 | M1.0 添加压缩流装饰器 |
| 无网络流 | 不支持 HTTP/FTP 等网络路径 | M1.0 集成 `mine.net` |
| 无文件监视 | 不支持目录变更通知 | M1.1 添加 `FileWatcher` |

---

## 3.16 相关模块

- **mine.core**：提供 `Result<T>`、`Status`、`Span<T>`、`String` 等基础类型
- **mine.async**（规划中）：提供异步文件 I/O（`Task<T>`、`AsyncFile`）
- **mine.net**（规划中）：提供网络流（HTTP/FTP）
- **mine.compress**（规划中）：提供透明压缩流（gzip/zstd）

---

## 3.17 设计哲学

1. **简单优于复杂**：API 设计优先考虑常见场景，避免过度抽象
2. **显式优于隐式**：文件模式、编码、错误处理均显式声明
3. **性能与安全并重**：提供零拷贝 `MemMap` 的同时保证 RAII 生命周期
4. **平台透明**：公共 API 隔离平台差异，实现细节条件编译

---

## 3.18 参考资料

- [18-coding-conventions.md](18-coding-conventions.md) — 编码规范
- [02-modules.md](02-modules.md) — 模块架构
- [development-plan.md](development-plan.md) — M0.1 开发计划
- [Rust std::fs](https://doc.rust-lang.org/std/fs/) — API 设计参考
- [C++17 filesystem](https://en.cppreference.com/w/cpp/filesystem) — 标准库对比
