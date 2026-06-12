# PipeStream 类详细接口文档

## 概述

**模块**：`mine.io`  
**头文件**：`<mine/io/PipeStream.h>`  
**命名空间**：`mine::io`

**用途**：提供缓冲式顺序流 I/O 抽象,适用于文件、管道、网络等顺序数据源的读写。

**核心特性**：
- **双缓冲设计**：内部维护读缓冲区和写缓冲区（默认 8KB）
- **减少系统调用**：批量读写,显著降低 `read`/`write` 调用次数
- **RAII 管理**：析构自动刷新写缓冲区并关闭底层资源
- **仅移动语义**：禁用拷贝,通过移动转移所有权
- **行级操作**：提供 `read_line`/`write_line` 便捷接口
- **错误状态跟踪**：记录最近一次操作的错误,支持错误恢复

**设计动机**：

直接使用 `File` 进行小块读写效率低下:
1. **系统调用开销**：每次 `read`/`write` 都陷入内核
2. **上下文切换**：频繁的用户态/内核态切换
3. **缓存失效**：小块 I/O 无法充分利用磁盘缓存

`mine::io::PipeStream` 通过用户态缓冲区解决这些问题:
- **读缓冲**：一次性读取多个块（8KB）到缓冲区,后续 `read` 直接从缓冲区取
- **写缓冲**：累积多次 `write` 到缓冲区,满时或显式 `flush` 时一次性写入
- **行缓冲**：特别优化行级操作,避免逐字符读写

**适用场景**：
- 顺序读取/写入文件（日志、配置、文本处理）
- 行协议的网络通信（HTTP、SMTP、FTP）
- 管道通信（进程间通信）
- CSV、JSON 等文本格式解析

**不适用场景**：
- 随机访问：使用 `File` + `seek` 或 `MemMap`
- 大块 I/O（> 缓冲区大小）：直接使用 `File`
- 无缓冲需求：使用 `File` 避免额外开销

---

## 类定义

### StreamAccess

**枚举定义**：
```cpp
enum class StreamAccess : uint8_t {
    Read   = 1,  ///< 只读
    Write  = 2,  ///< 只写
    Both   = 3,  ///< 读写
};
```

**描述**：流访问方向。

---

### PipeStream

**类定义**：
```cpp
class MINE_IO_API PipeStream {
public:
    static constexpr size_t kDefaultBufferSize = 8192;

    PipeStream() noexcept;
    PipeStream(PipeStream&& other) noexcept;
    PipeStream& operator=(PipeStream&& other) noexcept;
    PipeStream(const PipeStream&)            = delete;
    PipeStream& operator=(const PipeStream&) = delete;
    ~PipeStream() noexcept;

    [[nodiscard]] static mine::core::Result<PipeStream>
    from_file_read(const Path& path, size_t buffer_size = kDefaultBufferSize) noexcept;

    [[nodiscard]] static mine::core::Result<PipeStream>
    from_file_write(const Path& path, size_t buffer_size = kDefaultBufferSize) noexcept;

    [[nodiscard]] static mine::core::Result<PipeStream>
    from_file_append(const Path& path, size_t buffer_size = kDefaultBufferSize) noexcept;

    [[nodiscard]] mine::core::Result<size_t>
    read_some(mine::core::Span<char> buf) noexcept;

    [[nodiscard]] mine::core::Result<size_t>
    read_exact(mine::core::Span<char> buf) noexcept;

    [[nodiscard]] mine::core::Result<size_t>
    read_line(mine::core::Span<char> buf, char delimiter = '\n') noexcept;

    [[nodiscard]] mine::core::Result<size_t>
    write_some(mine::core::Span<const char> buf) noexcept;

    mine::core::Status write_all(mine::core::Span<const char> buf) noexcept;

    mine::core::Status write_line(mine::core::Span<const char> line) noexcept;

    mine::core::Status flush() noexcept;

    [[nodiscard]] bool is_open() const noexcept;
    [[nodiscard]] bool eof() const noexcept;
    [[nodiscard]] bool has_error() const noexcept;
    [[nodiscard]] mine::core::Status last_error() const noexcept;

    mine::core::Status close() noexcept;
};
```

---

## 构造与析构

### 默认构造

**签名**：
```cpp
PipeStream() noexcept;
```

**描述**：创建一个空的未打开流对象。

**示例**：
```cpp
PipeStream stream;  // 空流
assert(!stream.is_open());
```

---

### 移动构造/赋值

**签名**：
```cpp
PipeStream(PipeStream&& other) noexcept;
PipeStream& operator=(PipeStream&& other) noexcept;
```

**描述**：转移流所有权,原对象变为空流。

**示例**：
```cpp
auto stream1 = PipeStream::from_file_read("data.txt").value();
PipeStream stream2 = std::move(stream1);  // stream1 不再有效
assert(!stream1.is_open());
assert(stream2.is_open());
```

---

### 析构

**签名**：
```cpp
~PipeStream() noexcept;
```

**描述**：自动刷新写缓冲区并关闭底层资源。

**示例**：
```cpp
{
    auto stream = PipeStream::from_file_write("log.txt").value();
    stream.write_all(mine::core::make_span("message"));
}  // 此处自动 flush 并关闭
```

---

## 打开流

### from_file_read

**签名**：
```cpp
[[nodiscard]] static mine::core::Result<PipeStream>
from_file_read(const Path& path, size_t buffer_size = kDefaultBufferSize) noexcept;
```

**描述**：以只读方式打开文件流。

**参数**：
- `path`：文件路径
- `buffer_size`：内部缓冲区大小（字节）,默认 8192

**返回值**：`Result<PipeStream>`,成功返回流对象,失败返回错误。

**示例**：
```cpp
auto stream = PipeStream::from_file_read("config.txt");
if (!stream.ok()) {
    std::cerr << "打开文件失败: " << stream.status().message() << std::endl;
    return;
}

char buf[256];
while (auto n = stream.value().read_some(mine::core::make_span(buf)).value()) {
    process(buf, n);
}
```

---

### from_file_write

**签名**：
```cpp
[[nodiscard]] static mine::core::Result<PipeStream>
from_file_write(const Path& path, size_t buffer_size = kDefaultBufferSize) noexcept;
```

**描述**：以只写方式打开文件流（创建或截断）。

**参数**：
- `path`：文件路径
- `buffer_size`：内部缓冲区大小（字节）

**返回值**：`Result<PipeStream>`,成功返回流对象。

**注意**：如果文件已存在,会被清空。

**示例**：
```cpp
auto stream = PipeStream::from_file_write("output.txt").value();
stream.write_all(mine::core::make_span("Hello, World!\n"));
stream.flush();
```

---

### from_file_append

**签名**：
```cpp
[[nodiscard]] static mine::core::Result<PipeStream>
from_file_append(const Path& path, size_t buffer_size = kDefaultBufferSize) noexcept;
```

**描述**：以追加方式打开文件流（创建或追加到末尾）。

**参数**：
- `path`：文件路径
- `buffer_size`：内部缓冲区大小（字节）

**返回值**：`Result<PipeStream>`,成功返回流对象。

**示例**：
```cpp
auto stream = PipeStream::from_file_append("log.txt").value();
stream.write_line(mine::core::make_span("New log entry"));
```

---

## 读取操作

### read_some

**签名**：
```cpp
[[nodiscard]] mine::core::Result<size_t>
read_some(mine::core::Span<char> buf) noexcept;
```

**描述**：读取一些数据到缓冲区（尽量填满,但可短读）。

**参数**：
- `buf`：目标缓冲区

**返回值**：`Result<size_t>`,成功返回读取字节数,`0` 表示 EOF,失败返回错误。

**注意**：返回字节数可能小于 `buf.size()`,不保证填满缓冲区。

**示例**：
```cpp
auto stream = PipeStream::from_file_read("data.bin").value();
char buffer[1024];
auto span = mine::core::make_span(buffer);

auto result = stream.read_some(span);
if (result.ok()) {
    size_t n = result.value();
    if (n == 0) {
        std::cout << "EOF" << std::endl;
    } else {
        std::cout << "读取了 " << n << " 字节" << std::endl;
        process(buffer, n);
    }
}
```

---

### read_exact

**签名**：
```cpp
[[nodiscard]] mine::core::Result<size_t>
read_exact(mine::core::Span<char> buf) noexcept;
```

**描述**：读取精确字节数到缓冲区（循环读取直到填满或出错/EOF）。

**参数**：
- `buf`：目标缓冲区

**返回值**：`Result<size_t>`,成功返回 `buf.size()`,失败或 EOF 返回错误。

**注意**：如果遇到 EOF 且未读满,返回错误。

**示例**：
```cpp
auto stream = PipeStream::from_file_read("binary.dat").value();
char header[16];
auto span = mine::core::make_span(header);

auto result = stream.read_exact(span);
if (result.ok()) {
    // 保证读取了完整的 16 字节
    parse_header(header);
} else {
    std::cerr << "读取头部失败: " << result.status().message() << std::endl;
}
```

---

### read_line

**签名**：
```cpp
[[nodiscard]] mine::core::Result<size_t>
read_line(mine::core::Span<char> buf, char delimiter = '\n') noexcept;
```

**描述**：读取一行（直到遇到分隔符或缓冲区满或 EOF）。

**参数**：
- `buf`：目标缓冲区
- `delimiter`：行分隔符,默认 `'\n'`

**返回值**：`Result<size_t>`,返回读取字节数（不含分隔符）,`0` 表示 EOF。

**注意**：
- 分隔符会被消费但不包含在返回的数据中
- 如果行长度超过缓冲区大小,返回部分行

**示例**：
```cpp
auto stream = PipeStream::from_file_read("text.txt").value();
char line_buffer[256];
auto span = mine::core::make_span(line_buffer);

while (true) {
    auto result = stream.read_line(span);
    if (!result.ok()) {
        std::cerr << "读取失败: " << result.status().message() << std::endl;
        break;
    }

    size_t n = result.value();
    if (n == 0) {
        break;  // EOF
    }

    // 处理行（不含 '\n'）
    std::cout << "行: " << std::string_view(line_buffer, n) << std::endl;
}
```

---

## 写入操作

### write_some

**签名**：
```cpp
[[nodiscard]] mine::core::Result<size_t>
write_some(mine::core::Span<const char> buf) noexcept;
```

**描述**：写入一些数据（尽量写入,但可短写）。

**参数**：
- `buf`：源缓冲区

**返回值**：`Result<size_t>`,成功返回写入字节数,失败返回错误。

**注意**：返回字节数可能小于 `buf.size()`,需要循环调用以确保全部写入。

**示例**：
```cpp
auto stream = PipeStream::from_file_write("output.txt").value();
const char* data = "Hello, World!";
auto span = mine::core::make_span(data, std::strlen(data));

auto result = stream.write_some(span);
if (result.ok()) {
    std::cout << "写入了 " << result.value() << " 字节" << std::endl;
}
```

---

### write_all

**签名**：
```cpp
mine::core::Status write_all(mine::core::Span<const char> buf) noexcept;
```

**描述**：写入全部数据（循环写入直到全写完或出错）。

**参数**：
- `buf`：源缓冲区

**返回值**：`Status` 对象,成功返回 `ok()`,失败返回错误。

**示例**：
```cpp
auto stream = PipeStream::from_file_write("message.txt").value();
const char* message = "This is a complete message.\n";
auto span = mine::core::make_span(message, std::strlen(message));

auto st = stream.write_all(span);
if (!st.ok()) {
    std::cerr << "写入失败: " << st.message() << std::endl;
}
```

---

### write_line

**签名**：
```cpp
mine::core::Status write_line(mine::core::Span<const char> line) noexcept;
```

**描述**：写入一行并自动追加换行符 `'\n'`。

**参数**：
- `line`：行内容（不含换行符）

**返回值**：`Status` 对象,成功返回 `ok()`。

**示例**：
```cpp
auto stream = PipeStream::from_file_append("log.txt").value();
const char* log_entry = "2024-06-12 10:30:45 INFO Server started";
auto span = mine::core::make_span(log_entry, std::strlen(log_entry));

stream.write_line(span);
stream.flush();
```

---

## 刷新操作

### flush

**签名**：
```cpp
mine::core::Status flush() noexcept;
```

**描述**：强制将写缓冲区刷入底层文件/流。

**返回值**：`Status` 对象,成功返回 `ok()`。

**注意**：
- 仅对写流有效
- 析构时会自动调用,但建议关键数据显式刷新

**示例**：
```cpp
auto stream = PipeStream::from_file_write("critical.log").value();
stream.write_line(mine::core::make_span("Critical event occurred"));
stream.flush();  // 立即写入磁盘
```

---

## 状态查询

### is_open

**签名**：
```cpp
[[nodiscard]] bool is_open() const noexcept;
```

**描述**：检查流是否已打开。

**返回值**：`true` 表示流已打开,`false` 表示未打开。

**示例**：
```cpp
PipeStream stream;
assert(!stream.is_open());

stream = PipeStream::from_file_read("data.txt").value();
assert(stream.is_open());
```

---

### eof

**签名**：
```cpp
[[nodiscard]] bool eof() const noexcept;
```

**描述**：检查是否已到达流末尾（仅读流有意义）。

**返回值**：`true` 表示已到达 EOF,`false` 否则。

**示例**：
```cpp
auto stream = PipeStream::from_file_read("text.txt").value();
char buffer[256];

while (!stream.eof()) {
    auto n = stream.read_some(mine::core::make_span(buffer)).value();
    if (n > 0) {
        process(buffer, n);
    }
}
```

---

### has_error

**签名**：
```cpp
[[nodiscard]] bool has_error() const noexcept;
```

**描述**：检查流中是否发生了错误。

**返回值**：`true` 表示有错误,`false` 表示无错误。

**示例**：
```cpp
auto stream = PipeStream::from_file_read("data.bin").value();
char buffer[1024];

stream.read_some(mine::core::make_span(buffer));
if (stream.has_error()) {
    std::cerr << "读取错误: " << stream.last_error().message() << std::endl;
}
```

---

### last_error

**签名**：
```cpp
[[nodiscard]] mine::core::Status last_error() const noexcept;
```

**描述**：获取最近一次操作的错误状态。

**返回值**：`Status` 对象,包含错误信息。

**示例**：
```cpp
auto stream = PipeStream::from_file_write("output.txt").value();
auto st = stream.write_all(mine::core::make_span("data"));

if (!st.ok()) {
    std::cerr << "写入失败: " << stream.last_error().message() << std::endl;
}
```

---

## 关闭操作

### close

**签名**：
```cpp
mine::core::Status close() noexcept;
```

**描述**：显式关闭流（刷新缓冲区并释放资源）。

**返回值**：`Status` 对象,成功返回 `ok()`。

**注意**：析构时会自动调用,通常无需手动调用。

**示例**：
```cpp
auto stream = PipeStream::from_file_write("temp.txt").value();
stream.write_all(mine::core::make_span("temporary data"));
auto st = stream.close();
if (!st.ok()) {
    std::cerr << "关闭流失败: " << st.message() << std::endl;
}
```

---

## 综合示例

### 逐行读取文本文件

```cpp
#include <mine/io/PipeStream.h>
#include <mine/io/Path.h>
#include <iostream>

using namespace mine::io;

void process_text_file(const Path& path) {
    auto stream = PipeStream::from_file_read(path);
    if (!stream.ok()) {
        std::cerr << "打开文件失败: " << stream.status().message() << std::endl;
        return;
    }

    char line_buffer[1024];
    auto span = mine::core::make_span(line_buffer);
    size_t line_number = 0;

    while (true) {
        auto result = stream.value().read_line(span);
        if (!result.ok()) {
            if (stream.value().has_error()) {
                std::cerr << "读取错误: " << result.status().message() << std::endl;
            }
            break;
        }

        size_t n = result.value();
        if (n == 0) {
            break;  // EOF
        }

        ++line_number;
        std::string_view line(line_buffer, n);
        std::cout << line_number << ": " << line << std::endl;
    }
}

int main() {
    process_text_file("example.txt");
}
```

---

### 写入日志文件

```cpp
#include <mine/io/PipeStream.h>
#include <mine/io/Path.h>
#include <ctime>
#include <sstream>

using namespace mine::io;

class Logger {
public:
    Logger(const Path& log_path) {
        stream_ = PipeStream::from_file_append(log_path).value();
    }

    void log(const char* level, const char* message) {
        // 生成时间戳
        auto now = std::time(nullptr);
        char time_buf[32];
        std::strftime(time_buf, sizeof(time_buf), "%Y-%m-%d %H:%M:%S", std::localtime(&now));

        // 格式化日志行
        std::ostringstream oss;
        oss << time_buf << " [" << level << "] " << message;
        std::string log_line = oss.str();

        // 写入并刷新
        stream_.write_line(mine::core::make_span(log_line.data(), log_line.size()));
        stream_.flush();
    }

private:
    PipeStream stream_;
};

int main() {
    Logger logger("app.log");
    logger.log("INFO", "Application started");
    logger.log("WARN", "Low memory");
    logger.log("ERROR", "Connection failed");
}
```

---

### CSV 文件解析

```cpp
#include <mine/io/PipeStream.h>
#include <mine/core/Array.h>
#include <sstream>
#include <string>

using namespace mine::io;

struct CsvRow {
    mine::core::Array<std::string> fields;
};

// 解析 CSV 文件
mine::core::Array<CsvRow> parse_csv(const Path& path) {
    auto stream = PipeStream::from_file_read(path).value();
    mine::core::Array<CsvRow> rows;

    char line_buffer[4096];
    auto span = mine::core::make_span(line_buffer);

    while (true) {
        auto result = stream.read_line(span);
        if (!result.ok() || result.value() == 0) {
            break;  // EOF 或错误
        }

        size_t n = result.value();
        std::string_view line(line_buffer, n);

        // 简单 CSV 解析（不处理引号）
        CsvRow row;
        std::istringstream iss(std::string(line));
        std::string field;

        while (std::getline(iss, field, ',')) {
            row.fields.push_back(field);
        }

        rows.push_back(std::move(row));
    }

    return rows;
}

int main() {
    auto rows = parse_csv("data.csv");
    std::cout << "读取了 " << rows.size() << " 行" << std::endl;

    for (auto& row : rows) {
        for (auto& field : row.fields) {
            std::cout << field << " | ";
        }
        std::cout << std::endl;
    }
}
```

---

### 生成配置文件

```cpp
#include <mine/io/PipeStream.h>
#include <mine/io/Path.h>

using namespace mine::io;

void generate_config(const Path& path) {
    auto stream = PipeStream::from_file_write(path).value();

    // 写入配置头
    stream.write_line(mine::core::make_span("# Application Configuration"));
    stream.write_line(mine::core::make_span(""));

    // 写入配置项
    stream.write_line(mine::core::make_span("[Server]"));
    stream.write_line(mine::core::make_span("host = 127.0.0.1"));
    stream.write_line(mine::core::make_span("port = 8080"));
    stream.write_line(mine::core::make_span(""));

    stream.write_line(mine::core::make_span("[Database]"));
    stream.write_line(mine::core::make_span("url = sqlite:data.db"));
    stream.write_line(mine::core::make_span("pool_size = 10"));

    // 刷新到磁盘
    stream.flush();
}

int main() {
    generate_config("config.ini");
}
```

---

### 复制文件（带缓冲）

```cpp
#include <mine/io/PipeStream.h>
#include <mine/io/Path.h>

using namespace mine::io;

mine::core::Status copy_file_buffered(const Path& src, const Path& dst) {
    auto reader = PipeStream::from_file_read(src);
    if (!reader.ok()) {
        return reader.status();
    }

    auto writer = PipeStream::from_file_write(dst);
    if (!writer.ok()) {
        return writer.status();
    }

    char buffer[8192];
    auto span = mine::core::make_span(buffer);

    while (true) {
        auto read_result = reader.value().read_some(span);
        if (!read_result.ok()) {
            return read_result.status();
        }

        size_t n = read_result.value();
        if (n == 0) {
            break;  // EOF
        }

        auto write_span = mine::core::Span<const char>(buffer, n);
        auto st = writer.value().write_all(write_span);
        if (!st.ok()) {
            return st;
        }
    }

    return writer.value().flush();
}

int main() {
    auto st = copy_file_buffered("source.txt", "destination.txt");
    if (!st.ok()) {
        std::cerr << "复制失败: " << st.message() << std::endl;
    }
}
```

---

### 流水线处理

```cpp
#include <mine/io/PipeStream.h>
#include <mine/io/Path.h>

using namespace mine::io;

// 读取 -> 转换 -> 写入流水线
void transform_file(const Path& input, const Path& output) {
    auto reader = PipeStream::from_file_read(input).value();
    auto writer = PipeStream::from_file_write(output).value();

    char line_buffer[1024];
    auto span = mine::core::make_span(line_buffer);

    while (true) {
        auto result = reader.read_line(span);
        if (!result.ok() || result.value() == 0) {
            break;
        }

        size_t n = result.value();

        // 转换：转为大写
        for (size_t i = 0; i < n; ++i) {
            if (line_buffer[i] >= 'a' && line_buffer[i] <= 'z') {
                line_buffer[i] = line_buffer[i] - 'a' + 'A';
            }
        }

        // 写入转换后的行
        writer.write_line(mine::core::Span<const char>(line_buffer, n));
    }

    writer.flush();
}

int main() {
    transform_file("input.txt", "output.txt");
}
```

---

## 性能考虑

### 缓冲区大小

默认 8KB 适合大多数场景,但可根据文件大小和访问模式调整:

```cpp
// 小文件（< 64KB）：使用默认 8KB
auto stream = PipeStream::from_file_read("small.txt");

// 大文件（> 1MB）：增大缓冲区
auto stream_large = PipeStream::from_file_read("large.dat", 64 * 1024);  // 64 KB
```

---

### 减少 flush 调用

频繁 `flush()` 会抵消缓冲的优势:

```cpp
// ❌ 低效：每次写入都刷新
for (auto& line : lines) {
    stream.write_line(line);
    stream.flush();  // 过度刷新!
}

// ✅ 高效：批量写入后刷新
for (auto& line : lines) {
    stream.write_line(line);
}
stream.flush();  // 一次性刷新
```

---

### read_exact vs read_some

`read_exact` 循环读取,有额外开销。已知数据长度时使用,否则用 `read_some`:

```cpp
// 读取固定长度头部
char header[16];
stream.read_exact(mine::core::make_span(header));

// 读取不定长数据
char buffer[4096];
while (auto n = stream.read_some(mine::core::make_span(buffer)).value()) {
    process(buffer, n);
}
```

---

### 行缓冲 vs 块缓冲

逐行处理用 `read_line`,批量处理用 `read_some`:

```cpp
// 文本处理：逐行读取
while (auto n = stream.read_line(line_buf).value()) {
    process_line(line_buf, n);
}

// 二进制处理：块读取
while (auto n = stream.read_some(block_buf).value()) {
    process_block(block_buf, n);
}
```

---

## 错误处理

### 检查 Result

所有读写操作返回 `Result<T>` 或 `Status`:

```cpp
auto result = stream.read_some(buffer);
if (!result.ok()) {
    std::cerr << "读取失败: " << result.status().message() << std::endl;
    return;
}

size_t n = result.value();
```

---

### 使用 has_error 快速检测

```cpp
stream.read_some(buffer);
if (stream.has_error()) {
    std::cerr << "错误: " << stream.last_error().message() << std::endl;
}
```

---

### EOF 处理

`read_some` 返回 `0` 表示 EOF,需要与错误区分:

```cpp
while (true) {
    auto result = stream.read_some(buffer);
    if (!result.ok()) {
        std::cerr << "读取错误: " << result.status().message() << std::endl;
        break;
    }

    size_t n = result.value();
    if (n == 0) {
        std::cout << "到达文件末尾" << std::endl;
        break;
    }

    process(buffer, n);
}
```

---

## 最佳实践

### 1. 使用 RAII 管理生命周期

```cpp
{
    auto stream = PipeStream::from_file_write("output.txt").value();
    stream.write_all(data);
}  // 自动 flush 并关闭
```

---

### 2. 关键数据显式 flush

```cpp
stream.write_line(mine::core::make_span("transaction commit"));
stream.flush();  // 确保写入磁盘
```

---

### 3. 批量操作减少系统调用

```cpp
// 一次性写入多行
for (auto& line : log_lines) {
    stream.write_line(line);
}
stream.flush();
```

---

### 4. 检查错误状态

```cpp
auto st = stream.write_all(data);
if (!st.ok()) {
    handle_error(st);
}
```

---

### 5. 根据场景选择合适的缓冲区大小

```cpp
// 网络流：小缓冲区（减少延迟）
auto stream = PipeStream::from_file_write("socket.log", 1024);

// 大文件：大缓冲区（提高吞吐）
auto stream_large = PipeStream::from_file_read("backup.tar", 128 * 1024);
```

---

## 平台差异

### 行分隔符

- **Unix/Linux**：`\n`（LF）
- **Windows**：`\r\n`（CRLF）
- **Mac（旧）**：`\r`（CR）

`read_line` 默认使用 `\n`,Windows 文本文件需要手动处理 `\r`:

```cpp
auto result = stream.read_line(buffer);
size_t n = result.value();

// 去除末尾的 \r（Windows CRLF）
if (n > 0 && buffer[n - 1] == '\r') {
    --n;
}
```

---

### 缓冲区对齐

某些平台（如 Linux with O_DIRECT）要求缓冲区地址对齐,`PipeStream` 内部已处理。

---

### 跨平台建议

- **统一使用 `\n`**：输出时强制使用 `\n`,读取时兼容 `\r\n`
- **二进制模式**：避免平台自动转换,使用 `File` 或 `MemMap`
- **编码**：始终使用 UTF-8,避免平台相关编码

---

## 另见

- [Path 类](01-Path.md) - 路径表示和操作
- [File 类](02-File.md) - 文件读写
- [FileSystem 命名空间](03-FileSystem.md) - 文件系统操作
- [MemMap 类](04-MemMap.md) - 内存映射文件
