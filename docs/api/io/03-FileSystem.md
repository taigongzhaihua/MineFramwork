# FileSystem 命名空间详细接口文档

## 概述

**模块**：`mine.io`  
**头文件**：`<mine/io/FileSystem.h>`  
**命名空间**：`mine::io::FileSystem`

**用途**：提供文件系统操作函数集合,包括文件/目录的查询、创建、删除、遍历等。

**核心特性**：
- **无状态函数**：所有操作为命名空间级别的静态函数
- **显式错误处理**：返回 `Result<T>` 或 `Status`,无异常
- **递归安全**：递归操作（如 `remove_dir_all`）有深度限制防护
- **跨平台一致**：统一 API,内部处理 Windows 和 POSIX 差异
- **高效遍历**：`DirectoryIterator` 使用平台原生 API（Windows `FindFirstFile`、POSIX `readdir`）
- **符号链接感知**：明确区分文件、目录、符号链接类型

**设计动机**：

标准库 `std::filesystem` 存在以下问题:
1. **ABI 不稳定**：跨编译器/版本 ABI 不兼容
2. **异常依赖**：错误处理强制使用异常或 `error_code`
3. **性能问题**：某些实现存在不必要的 stat 调用
4. **符号链接处理**：行为在不同平台不一致

`mine::io::FileSystem` 设计原则:
- 禁用异常,错误通过 `Result<T>` 返回
- 所有操作明确指定是否跟随符号链接
- 遍历接口简洁,减少冗余系统调用
- ABI 稳定,通过 PIMPL 封装实现细节

---

## 类型定义

### FileType

**枚举定义**：
```cpp
enum class FileType : uint8_t {
    Unknown     = 0,  ///< 未知或无法确定
    Regular     = 1,  ///< 普通文件
    Directory   = 2,  ///< 目录
    Symlink     = 3,  ///< 符号链接
    Junction    = 4,  ///< Windows 交接点
    BlockDevice = 5,  ///< 块设备（Unix）
    CharDevice  = 6,  ///< 字符设备（Unix）
    Pipe        = 7,  ///< 命名管道
    Socket      = 8,  ///< Unix 域套接字
};
```

**描述**：文件系统条目类型。

---

### FileTimestamp

**结构体定义**：
```cpp
struct FileTimestamp {
    int64_t seconds = 0;  ///< 自 1970-01-01 00:00:00 UTC 以来的秒数
};
```

**描述**：文件系统时间戳,以 Unix 纪元为基准的秒级精度时间。

---

### FileEntry

**类定义**：
```cpp
class MINE_IO_API FileEntry {
public:
    FileEntry() noexcept;
    ~FileEntry() noexcept;

    FileEntry(const FileEntry&) noexcept;
    FileEntry& operator=(const FileEntry&) noexcept;
    FileEntry(FileEntry&&) noexcept;
    FileEntry& operator=(FileEntry&&) noexcept;

    [[nodiscard]] const Path& path() const noexcept;
    [[nodiscard]] FileType type() const noexcept;
    [[nodiscard]] bool is_regular() const noexcept;
    [[nodiscard]] bool is_directory() const noexcept;
    [[nodiscard]] bool is_symlink() const noexcept;
    [[nodiscard]] uint64_t file_size() const noexcept;
    [[nodiscard]] FileTimestamp creation_time() const noexcept;
    [[nodiscard]] FileTimestamp last_access_time() const noexcept;
    [[nodiscard]] FileTimestamp last_write_time() const noexcept;
    [[nodiscard]] bool is_hidden() const noexcept;
    [[nodiscard]] bool is_read_only() const noexcept;
};
```

**描述**：文件系统条目的元数据,通过 `FileSystem::stat()` 或 `DirectoryIterator` 获取。

#### path()

**签名**：
```cpp
[[nodiscard]] const Path& path() const noexcept;
```

**描述**：返回条目的完整路径。

**返回值**：条目路径的常量引用。

**示例**：
```cpp
auto entry = FileSystem::stat("config.json").value();
std::cout << "路径: " << entry.path().c_str() << std::endl;
```

---

#### type()

**签名**：
```cpp
[[nodiscard]] FileType type() const noexcept;
```

**描述**：返回条目的文件类型。

**返回值**：`FileType` 枚举值。

**示例**：
```cpp
auto entry = FileSystem::stat("data").value();
if (entry.type() == FileType::Directory) {
    std::cout << "是目录" << std::endl;
}
```

---

#### is_regular()

**签名**：
```cpp
[[nodiscard]] bool is_regular() const noexcept;
```

**描述**：检查条目是否为普通文件。

**返回值**：`true` 表示普通文件,`false` 否则。

**示例**：
```cpp
auto entry = FileSystem::stat("document.txt").value();
if (entry.is_regular()) {
    std::cout << "文件大小: " << entry.file_size() << " 字节" << std::endl;
}
```

---

#### is_directory()

**签名**：
```cpp
[[nodiscard]] bool is_directory() const noexcept;
```

**描述**：检查条目是否为目录。

**返回值**：`true` 表示目录,`false` 否则。

**示例**：
```cpp
auto entry = FileSystem::stat("build").value();
if (entry.is_directory()) {
    // 遍历目录...
}
```

---

#### is_symlink()

**签名**：
```cpp
[[nodiscard]] bool is_symlink() const noexcept;
```

**描述**：检查条目是否为符号链接或 Windows 交接点。

**返回值**：`true` 表示符号链接/交接点,`false` 否则。

**示例**：
```cpp
auto entry = FileSystem::stat("link").value();
if (entry.is_symlink()) {
    std::cout << "这是一个符号链接" << std::endl;
}
```

---

#### file_size()

**签名**：
```cpp
[[nodiscard]] uint64_t file_size() const noexcept;
```

**描述**：返回文件大小(字节)。目录通常返回 0。

**返回值**：文件大小（字节）。

**示例**：
```cpp
auto entry = FileSystem::stat("large.bin").value();
std::cout << "文件大小: " << entry.file_size() / (1024 * 1024) << " MB" << std::endl;
```

---

#### creation_time()

**签名**：
```cpp
[[nodiscard]] FileTimestamp creation_time() const noexcept;
```

**描述**：返回文件创建时间。

**返回值**：`FileTimestamp` 结构体,包含秒级时间戳。

**注意**：POSIX 系统可能不支持创建时间,返回最后修改时间。

**示例**：
```cpp
auto entry = FileSystem::stat("log.txt").value();
auto ct = entry.creation_time();
std::cout << "创建时间戳: " << ct.seconds << std::endl;
```

---

#### last_access_time()

**签名**：
```cpp
[[nodiscard]] FileTimestamp last_access_time() const noexcept;
```

**描述**：返回文件最后访问时间。

**返回值**：`FileTimestamp` 结构体。

**示例**：
```cpp
auto entry = FileSystem::stat("cache.dat").value();
auto at = entry.last_access_time();
std::cout << "最后访问: " << at.seconds << std::endl;
```

---

#### last_write_time()

**签名**：
```cpp
[[nodiscard]] FileTimestamp last_write_time() const noexcept;
```

**描述**：返回文件最后修改时间。

**返回值**：`FileTimestamp` 结构体。

**示例**：
```cpp
auto entry = FileSystem::stat("config.json").value();
auto mt = entry.last_write_time();
std::cout << "最后修改: " << mt.seconds << std::endl;
```

---

#### is_hidden()

**签名**：
```cpp
[[nodiscard]] bool is_hidden() const noexcept;
```

**描述**：检查文件是否为隐藏文件。

**返回值**：`true` 表示隐藏,`false` 否则。

**平台差异**：
- **Windows**：检查 `FILE_ATTRIBUTE_HIDDEN` 属性
- **Unix/Linux**：检查文件名是否以 `.` 开头

**示例**：
```cpp
auto entry = FileSystem::stat(".gitignore").value();
if (entry.is_hidden()) {
    std::cout << "这是隐藏文件" << std::endl;
}
```

---

#### is_read_only()

**签名**：
```cpp
[[nodiscard]] bool is_read_only() const noexcept;
```

**描述**：检查文件是否为只读。

**返回值**：`true` 表示只读,`false` 否则。

**示例**：
```cpp
auto entry = FileSystem::stat("readonly.txt").value();
if (entry.is_read_only()) {
    std::cout << "文件为只读" << std::endl;
}
```

---

### DirectoryIterator

**类定义**：
```cpp
class MINE_IO_API DirectoryIterator {
public:
    DirectoryIterator() noexcept;
    ~DirectoryIterator() noexcept;

    DirectoryIterator(DirectoryIterator&&) noexcept;
    DirectoryIterator& operator=(DirectoryIterator&&) noexcept;

    DirectoryIterator(const DirectoryIterator&)            = delete;
    DirectoryIterator& operator=(const DirectoryIterator&) = delete;

    [[nodiscard]] bool done() const noexcept;
    [[nodiscard]] const FileEntry& entry() const noexcept;
    mine::core::Status next() noexcept;
};
```

**描述**：目录遍历迭代器,用于逐个访问目录下的条目。

#### done()

**签名**：
```cpp
[[nodiscard]] bool done() const noexcept;
```

**描述**：检查遍历是否完成。

**返回值**：`true` 表示没有更多条目,`false` 表示还有条目。

**示例**：
```cpp
auto iter = FileSystem::list_dir("data").value();
while (!iter.done()) {
    auto& e = iter.entry();
    std::cout << e.path().c_str() << std::endl;
    iter.next();
}
```

---

#### entry()

**签名**：
```cpp
[[nodiscard]] const FileEntry& entry() const noexcept;
```

**描述**：获取当前条目的元数据。

**返回值**：当前 `FileEntry` 的常量引用。

**前提条件**：`done()` 返回 `false`。

**示例**：
```cpp
auto iter = FileSystem::list_dir("logs").value();
if (!iter.done()) {
    auto& e = iter.entry();
    if (e.is_regular() && e.path().extension() == ".log") {
        process_log(e.path());
    }
}
```

---

#### next()

**签名**：
```cpp
mine::core::Status next() noexcept;
```

**描述**：前进到下一个条目。

**返回值**：`Status` 对象,成功返回 `ok()`,失败返回错误信息。

**示例**：
```cpp
auto iter = FileSystem::list_dir("configs").value();
while (!iter.done()) {
    auto& e = iter.entry();
    // 处理条目...
    auto st = iter.next();
    if (!st.ok()) {
        std::cerr << "遍历错误: " << st.message() << std::endl;
        break;
    }
}
```

---

## 查询操作

### exists

**签名**：
```cpp
[[nodiscard]] mine::core::Result<bool>
exists(const Path& path) noexcept;
```

**描述**：检查路径是否存在（文件或目录）。

**参数**：
- `path`：要检查的路径

**返回值**：`Result<bool>`,成功包含 `true`（存在）或 `false`（不存在）,失败返回错误。

**示例**：
```cpp
if (FileSystem::exists("config.json").value_or(false)) {
    // 配置文件存在
    load_config("config.json");
}
```

---

### stat

**签名**：
```cpp
[[nodiscard]] mine::core::Result<FileEntry>
stat(const Path& path) noexcept;
```

**描述**：获取文件或目录的详细元数据（等价于 POSIX `stat`）。

**参数**：
- `path`：要查询的路径

**返回值**：`Result<FileEntry>`,成功返回文件元数据,失败返回错误。

**示例**：
```cpp
auto entry = FileSystem::stat("data.bin");
if (entry.ok()) {
    std::cout << "大小: " << entry.value().file_size() << " 字节" << std::endl;
    std::cout << "类型: " << (entry.value().is_directory() ? "目录" : "文件") << std::endl;
} else {
    std::cerr << "stat 失败: " << entry.status().message() << std::endl;
}
```

---

### is_directory

**签名**：
```cpp
[[nodiscard]] mine::core::Result<bool>
is_directory(const Path& path) noexcept;
```

**描述**：检查路径是否为目录。

**参数**：
- `path`：要检查的路径

**返回值**：`Result<bool>`,成功包含 `true`（是目录）或 `false`（不是目录）。

**示例**：
```cpp
if (FileSystem::is_directory("build").value_or(false)) {
    // 清理构建目录
    FileSystem::remove_dir_all("build");
}
```

---

### is_regular_file

**签名**：
```cpp
[[nodiscard]] mine::core::Result<bool>
is_regular_file(const Path& path) noexcept;
```

**描述**：检查路径是否为普通文件。

**参数**：
- `path`：要检查的路径

**返回值**：`Result<bool>`,成功包含 `true`（是文件）或 `false`（不是文件）。

**示例**：
```cpp
if (FileSystem::is_regular_file("config.json").value_or(false)) {
    auto file = File::open("config.json");
    // 读取配置...
}
```

---

### file_size

**签名**：
```cpp
[[nodiscard]] mine::core::Result<uint64_t>
file_size(const Path& path) noexcept;
```

**描述**：获取文件大小（字节）。

**参数**：
- `path`：文件路径

**返回值**：`Result<uint64_t>`,成功返回文件大小,失败返回错误。

**注意**：如果路径是目录,行为未定义（通常返回 0 或错误）。

**示例**：
```cpp
auto size = FileSystem::file_size("video.mp4");
if (size.ok()) {
    std::cout << "视频大小: " << size.value() / (1024 * 1024) << " MB" << std::endl;
}
```

---

## 目录操作

### list_dir

**签名**：
```cpp
[[nodiscard]] mine::core::Result<DirectoryIterator>
list_dir(const Path& path) noexcept;
```

**描述**：列出目录下的所有条目。

**参数**：
- `path`：目录路径

**返回值**：`Result<DirectoryIterator>`,成功返回迭代器,失败返回错误。

**示例**：
```cpp
auto iter_result = FileSystem::list_dir("data");
if (!iter_result.ok()) {
    std::cerr << "打开目录失败: " << iter_result.status().message() << std::endl;
    return;
}

auto iter = std::move(iter_result.value());
while (!iter.done()) {
    auto& entry = iter.entry();
    std::cout << entry.path().c_str();
    if (entry.is_directory()) {
        std::cout << "/";
    }
    std::cout << std::endl;
    iter.next();
}
```

**完整遍历示例**：
```cpp
// 统计目录下所有文件的总大小
auto iter = FileSystem::list_dir("logs").value();
uint64_t total_size = 0;
size_t file_count = 0;

while (!iter.done()) {
    auto& entry = iter.entry();
    if (entry.is_regular()) {
        total_size += entry.file_size();
        ++file_count;
    }
    iter.next();
}

std::cout << "共 " << file_count << " 个文件, 总大小 " 
          << total_size / 1024 << " KB" << std::endl;
```

---

### create_dir

**签名**：
```cpp
[[nodiscard]] mine::core::Status
create_dir(const Path& path) noexcept;
```

**描述**：创建单层目录（不创建父目录）。

**参数**：
- `path`：要创建的目录路径

**返回值**：`Status` 对象,成功返回 `ok()`,失败返回错误。

**注意**：如果父目录不存在,操作会失败。使用 `create_dir_all` 创建完整路径。

**示例**：
```cpp
auto st = FileSystem::create_dir("output");
if (!st.ok()) {
    std::cerr << "创建目录失败: " << st.message() << std::endl;
}
```

---

### create_dir_all

**签名**：
```cpp
[[nodiscard]] mine::core::Status
create_dir_all(const Path& path) noexcept;
```

**描述**：创建目录及所有必需的父目录（类似 `mkdir -p`）。

**参数**：
- `path`：要创建的目录路径

**返回值**：`Status` 对象,成功返回 `ok()`。

**示例**：
```cpp
// 创建深层嵌套目录
auto st = FileSystem::create_dir_all("build/windows/x64/debug");
if (!st.ok()) {
    std::cerr << "创建目录树失败: " << st.message() << std::endl;
}
```

---

### remove_dir

**签名**：
```cpp
[[nodiscard]] mine::core::Status
remove_dir(const Path& path) noexcept;
```

**描述**：删除空目录。

**参数**：
- `path`：要删除的目录路径

**返回值**：`Status` 对象,成功返回 `ok()`,失败返回错误。

**注意**：如果目录非空,操作会失败。使用 `remove_dir_all` 递归删除。

**示例**：
```cpp
auto st = FileSystem::remove_dir("temp");
if (!st.ok()) {
    std::cerr << "删除目录失败: " << st.message() << std::endl;
}
```

---

### remove_dir_all

**签名**：
```cpp
[[nodiscard]] mine::core::Status
remove_dir_all(const Path& path) noexcept;
```

**描述**：递归删除目录及其所有内容（类似 `rm -rf`）。

**参数**：
- `path`：要删除的目录路径

**返回值**：`Status` 对象,成功返回 `ok()`。

**警告**：此操作不可逆,请谨慎使用。

**示例**：
```cpp
// 清理构建输出
auto st = FileSystem::remove_dir_all("build/intermediates");
if (!st.ok()) {
    std::cerr << "清理失败: " << st.message() << std::endl;
}
```

---

## 文件操作

### remove_file

**签名**：
```cpp
[[nodiscard]] mine::core::Status
remove_file(const Path& path) noexcept;
```

**描述**：删除文件。

**参数**：
- `path`：要删除的文件路径

**返回值**：`Status` 对象,成功返回 `ok()`,失败返回错误。

**注意**：如果路径是目录,操作会失败。

**示例**：
```cpp
auto st = FileSystem::remove_file("temp.txt");
if (!st.ok()) {
    std::cerr << "删除文件失败: " << st.message() << std::endl;
}
```

---

### rename

**签名**：
```cpp
[[nodiscard]] mine::core::Status
rename(const Path& from, const Path& to) noexcept;
```

**描述**：重命名或移动文件/目录。

**参数**：
- `from`：源路径
- `to`：目标路径

**返回值**：`Status` 对象,成功返回 `ok()`。

**平台差异**：
- **Windows**：可以跨卷移动
- **Unix/Linux**：不能跨文件系统移动,需要使用复制+删除

**示例**：
```cpp
// 重命名文件
auto st = FileSystem::rename("old_name.txt", "new_name.txt");
if (!st.ok()) {
    std::cerr << "重命名失败: " << st.message() << std::endl;
}

// 移动文件到另一个目录
st = FileSystem::rename("source.txt", "backup/source.txt");
```

---

### copy_file

**签名**：
```cpp
[[nodiscard]] mine::core::Status
copy_file(const Path& from, const Path& to) noexcept;
```

**描述**：复制文件（不覆盖已存在的目标文件）。

**参数**：
- `from`：源文件路径
- `to`：目标文件路径

**返回值**：`Status` 对象,成功返回 `ok()`,失败返回错误。

**注意**：如果目标文件已存在,操作会失败。

**示例**：
```cpp
auto st = FileSystem::copy_file("template.json", "config.json");
if (!st.ok()) {
    std::cerr << "复制文件失败: " << st.message() << std::endl;
}
```

---

### copy_file_overwrite

**签名**：
```cpp
[[nodiscard]] mine::core::Status
copy_file_overwrite(const Path& from, const Path& to) noexcept;
```

**描述**：复制文件（覆盖已存在的目标文件）。

**参数**：
- `from`：源文件路径
- `to`：目标文件路径

**返回值**：`Status` 对象,成功返回 `ok()`。

**示例**：
```cpp
// 强制覆盖备份
auto st = FileSystem::copy_file_overwrite("data.db", "backup/data.db");
if (!st.ok()) {
    std::cerr << "备份失败: " << st.message() << std::endl;
}
```

---

## 路径工具

### current_dir

**签名**：
```cpp
[[nodiscard]] mine::core::Result<Path>
current_dir() noexcept;
```

**描述**：获取当前工作目录。

**返回值**：`Result<Path>`,成功返回当前目录路径,失败返回错误。

**示例**：
```cpp
auto cwd = FileSystem::current_dir();
if (cwd.ok()) {
    std::cout << "当前目录: " << cwd.value().c_str() << std::endl;
}
```

---

### set_current_dir

**签名**：
```cpp
[[nodiscard]] mine::core::Status
set_current_dir(const Path& path) noexcept;
```

**描述**：设置当前工作目录。

**参数**：
- `path`：新的工作目录路径

**返回值**：`Status` 对象,成功返回 `ok()`。

**示例**：
```cpp
auto st = FileSystem::set_current_dir("build");
if (!st.ok()) {
    std::cerr << "切换目录失败: " << st.message() << std::endl;
}
```

---

### temp_dir

**签名**：
```cpp
[[nodiscard]] mine::core::Result<Path>
temp_dir() noexcept;
```

**描述**：获取系统临时目录路径。

**返回值**：`Result<Path>`,成功返回临时目录路径。

**平台差异**：
- **Windows**：通常是 `%TEMP%` 环境变量指向的路径
- **Unix/Linux**：通常是 `/tmp` 或 `$TMPDIR`

**示例**：
```cpp
auto temp = FileSystem::temp_dir().value();
auto temp_file = temp / "cache_12345.tmp";
// 创建临时文件...
```

---

### home_dir

**签名**：
```cpp
[[nodiscard]] mine::core::Result<Path>
home_dir() noexcept;
```

**描述**：获取当前用户的主目录路径。

**返回值**：`Result<Path>`,成功返回主目录路径。

**平台差异**：
- **Windows**：通常是 `%USERPROFILE%`（如 `C:\Users\username`）
- **Unix/Linux**：通常是 `$HOME`（如 `/home/username`）

**示例**：
```cpp
auto home = FileSystem::home_dir().value();
auto config_path = home / ".config" / "myapp" / "config.json";
```

---

### exe_dir

**签名**：
```cpp
[[nodiscard]] mine::core::Result<Path>
exe_dir() noexcept;
```

**描述**：获取当前可执行文件所在的目录。

**返回值**：`Result<Path>`,成功返回可执行文件目录路径。

**用途**：常用于定位应用程序的资源文件。

**示例**：
```cpp
auto exe_dir = FileSystem::exe_dir().value();
auto assets_path = exe_dir / "assets" / "textures";
```

---

## 综合示例

### 递归遍历目录树

```cpp
#include <mine/io/FileSystem.h>
#include <mine/io/Path.h>
#include <mine/core/Array.h>
#include <iostream>

using namespace mine::io;

// 递归收集所有 .cpp 文件
void collect_cpp_files(const Path& dir, mine::core::Array<Path>& out_files) {
    auto iter_result = FileSystem::list_dir(dir);
    if (!iter_result.ok()) {
        std::cerr << "打开目录失败: " << dir.c_str() << std::endl;
        return;
    }

    auto iter = std::move(iter_result.value());
    while (!iter.done()) {
        auto& entry = iter.entry();
        
        if (entry.is_directory()) {
            // 递归子目录
            collect_cpp_files(entry.path(), out_files);
        } else if (entry.is_regular() && entry.path().extension() == ".cpp") {
            out_files.push_back(entry.path());
        }
        
        iter.next();
    }
}

int main() {
    mine::core::Array<Path> cpp_files;
    collect_cpp_files("src", cpp_files);
    
    std::cout << "找到 " << cpp_files.size() << " 个 .cpp 文件:" << std::endl;
    for (auto& path : cpp_files) {
        std::cout << "  " << path.c_str() << std::endl;
    }
}
```

---

### 安全文件替换（原子写）

```cpp
#include <mine/io/FileSystem.h>
#include <mine/io/File.h>
#include <mine/io/Path.h>

using namespace mine::io;

// 原子更新文件（先写临时文件，再重命名）
mine::core::Status atomic_update_file(const Path& target, 
                                      const char* data, size_t len) {
    // 1. 写入临时文件
    auto temp_path = target.parent_path() / (target.stem().string() + ".tmp");
    {
        auto file = File::open(temp_path, FileMode::Write | FileMode::Create | FileMode::Truncate);
        if (!file.ok()) {
            return file.status();
        }
        
        auto st = file.value().write_all(mine::core::Span<const char>(data, len));
        if (!st.ok()) {
            FileSystem::remove_file(temp_path);  // 清理临时文件
            return st;
        }
    }

    // 2. 原子替换
    auto st = FileSystem::rename(temp_path, target);
    if (!st.ok()) {
        FileSystem::remove_file(temp_path);  // 清理临时文件
    }
    return st;
}
```

---

### 清理旧日志文件

```cpp
#include <mine/io/FileSystem.h>
#include <ctime>

using namespace mine::io;

// 删除超过 N 天的日志文件
void cleanup_old_logs(const Path& log_dir, int days) {
    auto now = std::time(nullptr);
    int64_t threshold = now - (days * 86400);  // 转为秒

    auto iter = FileSystem::list_dir(log_dir).value();
    while (!iter.done()) {
        auto& entry = iter.entry();
        
        if (entry.is_regular() && entry.path().extension() == ".log") {
            auto mtime = entry.last_write_time().seconds;
            if (mtime < threshold) {
                auto st = FileSystem::remove_file(entry.path());
                if (st.ok()) {
                    std::cout << "已删除旧日志: " << entry.path().filename() << std::endl;
                }
            }
        }
        
        iter.next();
    }
}
```

---

## 性能考虑

### 减少 stat 调用

遍历目录时,`DirectoryIterator` 已在 `entry()` 中返回完整的 `FileEntry`,包含类型、大小、时间戳等信息。**避免**额外调用 `FileSystem::stat()`:

```cpp
// ❌ 低效：重复 stat
auto iter = FileSystem::list_dir("data").value();
while (!iter.done()) {
    auto& entry = iter.entry();
    auto stat_entry = FileSystem::stat(entry.path()).value();  // 冗余!
    if (stat_entry.is_regular()) { ... }
    iter.next();
}

// ✅ 高效：直接使用 entry
auto iter = FileSystem::list_dir("data").value();
while (!iter.done()) {
    auto& entry = iter.entry();
    if (entry.is_regular()) { ... }  // 无额外系统调用
    iter.next();
}
```

---

### 批量操作

删除或复制大量文件时,尽量减少错误检查的粒度:

```cpp
// 批量删除,先收集再删除
mine::core::Array<Path> files_to_delete;
auto iter = FileSystem::list_dir("temp").value();
while (!iter.done()) {
    auto& e = iter.entry();
    if (e.is_regular() && should_delete(e)) {
        files_to_delete.push_back(e.path());
    }
    iter.next();
}

// 批量删除
for (auto& path : files_to_delete) {
    FileSystem::remove_file(path);
}
```

---

## 错误处理

所有操作返回 `Result<T>` 或 `Status`,推荐使用以下模式:

### 使用 value_or 提供默认值

```cpp
bool exists = FileSystem::exists("config.json").value_or(false);
if (exists) {
    // 加载配置...
}
```

---

### 显式检查错误

```cpp
auto result = FileSystem::create_dir_all("output/logs");
if (!result.ok()) {
    std::cerr << "创建目录失败: " << result.message() << std::endl;
    return 1;
}
```

---

### 链式操作

```cpp
FileSystem::create_dir_all("backup")
    .and_then([&]() {
        return FileSystem::copy_file("data.db", "backup/data.db");
    })
    .or_else([](const mine::core::Status& st) {
        std::cerr << "备份失败: " << st.message() << std::endl;
    });
```

---

## 最佳实践

### 1. 使用 DirectoryIterator 遍历目录

```cpp
auto iter = FileSystem::list_dir("data").value();
while (!iter.done()) {
    auto& entry = iter.entry();
    // 处理条目...
    iter.next();
}
```

---

### 2. 创建目录前检查存在性

```cpp
if (!FileSystem::exists("output").value_or(false)) {
    FileSystem::create_dir_all("output");
}
```

---

### 3. 使用临时文件 + 重命名实现原子写

```cpp
auto temp = FileSystem::temp_dir().value() / "temp_12345.tmp";
// 写入 temp...
FileSystem::rename(temp, "target.dat");
```

---

### 4. 递归删除前备份

```cpp
FileSystem::copy_file_overwrite("important.db", "important.db.bak");
FileSystem::remove_dir_all("cache");
```

---

## 平台差异

### Windows 特有

- **交接点**（Junction）：Windows 特有的目录符号链接,类型为 `FileType::Junction`
- **盘符**：路径可能包含盘符（如 `C:`）,`FileEntry::path()` 会保留盘符
- **大小写不敏感**：文件名比较不区分大小写（内部已处理）

---

### Unix/Linux 特有

- **符号链接**：完整支持软链接,类型为 `FileType::Symlink`
- **设备文件**：支持 `FileType::BlockDevice`、`FileType::CharDevice`
- **权限系统**：`is_read_only()` 检查文件权限（而非 Windows 的属性标志）
- **大小写敏感**：文件名严格区分大小写

---

### 跨平台建议

- **路径分隔符**：始终使用 `/`,`Path` 内部会自动转换
- **文件名**：避免使用平台特殊字符（如 Windows 的 `<>:"|?*`）
- **隐藏文件**：Unix 依赖 `.` 前缀,Windows 依赖属性,使用 `is_hidden()` 统一检查

---

## 另见

- [Path 类](01-Path.md) - 路径表示和操作
- [File 类](02-File.md) - 文件读写
- [MemMap 类](04-MemMap.md) - 内存映射文件
- [PipeStream 类](05-PipeStream.md) - 缓冲流式 I/O
