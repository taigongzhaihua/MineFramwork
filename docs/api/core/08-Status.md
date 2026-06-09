# Status 详细接口文档

## 概述

`Status` 是 MineFramework 中用于表示操作结果的轻量级类型,仅携带 `StatusCode` 错误码。它是 `Result<T, E>` 的简化版本,适用于不需要返回值、仅需表示成功/失败的场景。

### 核心特性

- **轻量级**：仅包含一个 `int32_t` 错误码,可在寄存器中传递
- **零开销**：无堆分配,`sizeof(Status) == 4`
- **类型安全**：通过 `StatusCode` 枚举避免魔数
- **强制检查**：标记 `[[nodiscard]]`,防止遗漏错误检查
- **便捷传播**：`MINE_TRY` 宏自动传播错误

### 设计动机

对于无返回值的操作（如初始化、清理、配置），使用 `Result<void>` 无意义。`Status` 提供了一种轻量级的错误表示：

- 比 `bool` 更具表达力（携带具体错误类型）
- 比 `Result<void>` 更简洁（无需处理值类型）
- 比异常更高效（无栈展开开销）

---

## StatusCode 枚举

```cpp
enum class StatusCode : int32_t {
    Ok            =  0,   ///< 操作成功
    Unknown       = -1,   ///< 未知错误
    InvalidArg    = -2,   ///< 参数非法
    OutOfRange    = -3,   ///< 索引或值超出范围
    OutOfMemory   = -4,   ///< 内存分配失败
    NotFound      = -5,   ///< 资源或键不存在
    AlreadyExists = -6,   ///< 资源已存在（重复创建）
    NotSupported  = -7,   ///< 功能在当前平台/配置下不支持
    Cancelled     = -8,   ///< 操作被主动取消
    Timeout       = -9,   ///< 操作超时
    IoError       = -10,  ///< 文件或流 I/O 错误
    ParseError    = -11,  ///< 解析/反序列化失败
    InvalidState  = -12,  ///< 对象处于非法状态（状态机保护）
    PermissionDenied = -13, ///< 权限不足
};
```

### 错误码设计原则

1. **成功为零**：`Ok == 0`,方便布尔判断
2. **负值表示错误**：所有错误码为负数,避免与成功值混淆
3. **顺序递减**：新增错误码递减编号,保持兼容性
4. **语义明确**：每个错误码有清晰的语义和使用场景

### 常见错误码用途

| 错误码 | 典型场景 | 示例 |
|--------|---------|------|
| `InvalidArg` | 参数验证失败 | 空指针、非法索引 |
| `OutOfRange` | 数组越界、数值溢出 | `vec[100]` 当 `size() == 10` |
| `OutOfMemory` | 堆分配失败 | `malloc()` 返回 `nullptr` |
| `NotFound` | 查找资源失败 | 文件不存在、键不存在 |
| `AlreadyExists` | 重复创建 | 文件已存在、重复注册 |
| `NotSupported` | 平台不支持 | Linux 上调用 Win32 API |
| `IoError` | 文件/网络 I/O 错误 | 读取失败、连接断开 |
| `ParseError` | 解析失败 | JSON 格式错误 |
| `InvalidState` | 状态机错误 | 未初始化时调用方法 |

---

## Status 类

### 类定义

```cpp
class [[nodiscard]] Status {
public:
    constexpr Status() noexcept = default;
    constexpr explicit Status(StatusCode code) noexcept;

    [[nodiscard]] static constexpr Status success() noexcept;
    [[nodiscard]] static constexpr Status from_code(StatusCode c) noexcept;

    [[nodiscard]] constexpr bool ok() const noexcept;
    [[nodiscard]] constexpr StatusCode code() const noexcept;

    constexpr explicit operator bool() const noexcept;

    constexpr bool operator==(Status other) const noexcept;
    constexpr bool operator!=(Status other) const noexcept;
    constexpr bool operator==(StatusCode c) const noexcept;
    constexpr bool operator!=(StatusCode c) const noexcept;

private:
    StatusCode code_{StatusCode::Ok};
};
```

### 构造函数

#### 默认构造

```cpp
constexpr Status() noexcept = default;
```

**描述**：构造成功状态,等价于 `Status{StatusCode::Ok}`。

**时间复杂度**：O(1)

**示例**：

```cpp
Status s;
MINE_ASSERT(s.ok());
MINE_ASSERT(s.code() == StatusCode::Ok);
```

---

#### 以错误码构造

```cpp
constexpr explicit Status(StatusCode code) noexcept;
```

**描述**：以指定错误码构造状态。

**参数**：
- `code`：状态码（可为 `Ok` 或任意错误码）

**显式构造**：标记 `explicit`,避免意外隐式转换。

**时间复杂度**：O(1)

**示例**：

```cpp
Status err{StatusCode::NotFound};
MINE_ASSERT(!err.ok());
MINE_ASSERT(err.code() == StatusCode::NotFound);
```

---

### 工厂方法

#### success

```cpp
[[nodiscard]] static constexpr Status success() noexcept;
```

**描述**：返回成功状态,等价于 `Status{}`。

**返回值**：`Status{StatusCode::Ok}`

**时间复杂度**：O(1)

**示例**：

```cpp
Status initialize() {
    // ... 初始化逻辑
    return Status::success();
}
```

---

#### from_code

```cpp
[[nodiscard]] static constexpr Status from_code(StatusCode c) noexcept;
```

**描述**：以 `StatusCode` 快速构造状态（等价于构造函数,但语法更简洁）。

**参数**：
- `c`：状态码

**返回值**：`Status{c}`

**时间复杂度**：O(1)

**示例**：

```cpp
StatusCode code = check_permission();
return Status::from_code(code);
```

---

### 查询方法

#### ok

```cpp
[[nodiscard]] constexpr bool ok() const noexcept;
```

**描述**：检查是否表示成功。

**返回值**：
- `true`：`code_ == StatusCode::Ok`
- `false`：持有错误码

**时间复杂度**：O(1)

**示例**：

```cpp
Status s = some_operation();
if (!s.ok()) {
    handle_error(s);
    return;
}
```

---

#### code

```cpp
[[nodiscard]] constexpr StatusCode code() const noexcept;
```

**描述**：获取原始错误码。

**返回值**：当前持有的 `StatusCode`

**时间复杂度**：O(1)

**示例**：

```cpp
Status s = parse_file("data.txt");
if (!s.ok()) {
    switch (s.code()) {
        case StatusCode::NotFound:
            log_error("File not found");
            break;
        case StatusCode::ParseError:
            log_error("Invalid file format");
            break;
        default:
            log_error("Unknown error");
    }
}
```

---

### 类型转换

#### operator bool

```cpp
constexpr explicit operator bool() const noexcept;
```

**描述**：显式布尔转换,等价于 `ok()`。

**返回值**：
- `true`：成功
- `false`：失败

**显式转换**：防止意外的隐式转换。

**时间复杂度**：O(1)

**示例**：

```cpp
Status s = initialize();
if (s) {  // 显式转换
    // 成功路径
}
```

---

### 比较运算符

```cpp
constexpr bool operator==(Status other) const noexcept;
constexpr bool operator!=(Status other) const noexcept;
constexpr bool operator==(StatusCode c) const noexcept;
constexpr bool operator!=(StatusCode c) const noexcept;
```

**描述**：比较两个 `Status` 或 `Status` 与 `StatusCode`。

**时间复杂度**：O(1)

**示例**：

```cpp
Status s1 = Status::success();
Status s2{StatusCode::NotFound};

MINE_ASSERT(s1 == Status::success());
MINE_ASSERT(s1 != s2);
MINE_ASSERT(s2 == StatusCode::NotFound);
MINE_ASSERT(s2 != StatusCode::Ok);
```

---

## MINE_TRY 宏

```cpp
#define MINE_TRY(expr)                              \
    do {                                            \
        auto _mine_status_ = (expr);                \
        if (!_mine_status_.ok()) {                  \
            return _mine_status_;                   \
        }                                           \
    } while (false)
```

**描述**：对 `expr`（须返回 `Status`）求值,若失败则立即 `return` 该 `Status`。

**约束**：仅适用于返回类型为 `Status` 的函数体内。

**用途**：简化错误传播,避免手写重复的检查代码。

**示例**：

```cpp
Status initialize_all() {
    MINE_TRY(init_graphics());
    MINE_TRY(init_audio());
    MINE_TRY(init_input());
    return Status::success();
}

// 展开后等价于：
Status initialize_all() {
    {
        auto _mine_status_ = init_graphics();
        if (!_mine_status_.ok()) return _mine_status_;
    }
    {
        auto _mine_status_ = init_audio();
        if (!_mine_status_.ok()) return _mine_status_;
    }
    {
        auto _mine_status_ = init_input();
        if (!_mine_status_.ok()) return _mine_status_;
    }
    return Status::success();
}
```

---

## 使用场景

### 1. 初始化/清理操作

```cpp
Status initialize_graphics() {
    if (!create_device())
        return Status{StatusCode::NotSupported};
    
    if (!create_swapchain())
        return Status{StatusCode::OutOfMemory};
    
    return Status::success();
}

void example() {
    auto status = initialize_graphics();
    if (!status) {
        log_error("Graphics init failed: {}", status.code());
        terminate();
    }
}
```

---

### 2. 文件操作

```cpp
Status save_config(StringView path, const Config& cfg) {
    FILE* file = fopen(path.data(), "wb");
    if (!file)
        return Status{StatusCode::IoError};
    
    size_t written = fwrite(&cfg, sizeof(cfg), 1, file);
    fclose(file);
    
    if (written != 1)
        return Status{StatusCode::IoError};
    
    return Status::success();
}
```

---

### 3. 多步骤操作（MINE_TRY）

```cpp
Status setup_application() {
    MINE_TRY(load_config());
    MINE_TRY(init_subsystems());
    MINE_TRY(create_main_window());
    return Status::success();
}
```

---

### 4. 条件错误

```cpp
Status validate_user_input(int value) {
    if (value < 0)
        return Status{StatusCode::InvalidArg};
    
    if (value > 100)
        return Status{StatusCode::OutOfRange};
    
    return Status::success();
}
```

---

## 性能分析

### 内存布局

```cpp
class Status {
    StatusCode code_;  // int32_t
};
```

**大小**：

```cpp
sizeof(Status) == 4  // 单个 int32_t
alignof(Status) == 4
```

### 操作时间复杂度

| 操作 | 时间复杂度 | 说明 |
|------|-----------|------|
| 构造 | O(1) | 初始化一个 int32_t |
| 拷贝 | O(1) | 拷贝一个 int32_t |
| `ok()` | O(1) | 比较 int32_t |
| `code()` | O(1) | 返回 int32_t |
| 比较 | O(1) | 比较 int32_t |

### 编译器优化

**寄存器传递**：`Status` 完全在寄存器中传递（单个 32 位值）。

**返回值优化**：编译器通常在返回寄存器（x86-64 的 `eax`）中直接返回状态码。

**示例（x86-64）**：

```cpp
Status foo() {
    return Status{StatusCode::NotFound};
}

// 生成汇编
foo():
    mov eax, -5  ; StatusCode::NotFound == -5
    ret
```

---

## 最佳实践

### 1. 优先使用 Status::success()

**推荐**：使用命名常量而非默认构造。

```cpp
// ✅ 清晰
return Status::success();

// ❌ 不够明确
return Status{};
```

---

### 2. 使用 MINE_TRY 简化错误传播

**推荐**：在多步骤操作中使用 `MINE_TRY`。

```cpp
// ✅ 简洁
Status init() {
    MINE_TRY(step1());
    MINE_TRY(step2());
    return Status::success();
}

// ❌ 冗长
Status init() {
    auto s1 = step1();
    if (!s1.ok()) return s1;
    auto s2 = step2();
    if (!s2.ok()) return s2;
    return Status::success();
}
```

---

### 3. 区分不同错误类型

**推荐**：返回语义明确的错误码。

```cpp
Status load_file(StringView path) {
    if (!file_exists(path))
        return Status{StatusCode::NotFound};
    
    FILE* file = fopen(path.data(), "rb");
    if (!file)
        return Status{StatusCode::PermissionDenied};
    
    // ...
}
```

---

### 4. 日志记录错误上下文

**推荐**：检查错误时记录足够的上下文信息。

```cpp
Status s = load_texture("icon.png");
if (!s) {
    log_error("Failed to load texture 'icon.png': {}",
              status_code_to_string(s.code()));
}
```

---

## 与 Result<T> 的选择

| 场景 | 推荐类型 | 理由 |
|------|---------|------|
| 无返回值操作 | `Status` | 更轻量,语义更清晰 |
| 有返回值操作 | `Result<T>` | 同时携带值和错误 |
| 查询操作 | `Result<T>` 或 `std::optional<T>` | 区分"未找到"和"查询失败" |
| void 函数 | `Status` | 避免 `Result<void>` |

**示例**：

```cpp
// ✅ 使用 Status（无返回值）
Status initialize();

// ✅ 使用 Result<T>（有返回值）
Result<int> parse_int(StringView str);

// ❌ 不推荐
Result<void> bad_init();  // 应使用 Status
```

---

## 完整示例

### 示例 1：配置加载

```cpp
Status load_application_config(const char* path) {
    // 步骤 1：读取文件
    FILE* file = fopen(path, "rb");
    if (!file) {
        if (errno == ENOENT)
            return Status{StatusCode::NotFound};
        return Status{StatusCode::IoError};
    }
    
    // 步骤 2：解析 JSON
    auto json = parse_json(file);
    fclose(file);
    
    if (!json)
        return Status{StatusCode::ParseError};
    
    // 步骤 3：应用配置
    if (!apply_config(*json))
        return Status{StatusCode::InvalidArg};
    
    return Status::success();
}

void main() {
    auto status = load_application_config("config.json");
    if (!status) {
        log_error("Config load failed: {}", status.code());
        use_default_config();
    }
}
```

---

### 示例 2：子系统初始化

```cpp
Status init_graphics() {
    if (!platform_supports_vulkan())
        return Status{StatusCode::NotSupported};
    
    if (!create_device())
        return Status{StatusCode::OutOfMemory};
    
    return Status::success();
}

Status init_audio() {
    if (!open_audio_device())
        return Status{StatusCode::IoError};
    
    return Status::success();
}

Status init_all_subsystems() {
    MINE_TRY(init_graphics());
    MINE_TRY(init_audio());
    // 更多子系统...
    return Status::success();
}

void application_main() {
    auto status = init_all_subsystems();
    if (!status) {
        show_error_dialog("Initialization failed");
        return;
    }
    
    run_main_loop();
}
```

---

## 总结

`Status` 是 MineFramework 中表示操作成功/失败的轻量级类型,具备以下优势:

1. **零开销**：4 字节大小,寄存器传递
2. **类型安全**：通过 `StatusCode` 枚举避免魔数
3. **强制检查**：`[[nodiscard]]` 确保调用方处理错误
4. **便捷传播**：`MINE_TRY` 宏简化错误传播
5. **语义明确**：`ok()` 方法和显式错误码

在设计无返回值的 API 时,优先使用 `Status` 而非 `bool` 或异常,这将显著提升代码的健壮性和可维护性。对于需要返回值的场景,使用 `Result<T>` 配合 `Status` 作为默认错误类型。
