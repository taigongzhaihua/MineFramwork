# Assert 详细接口文档

## 概述

`Assert` 模块提供了 MineFramework 的断言与不变式检查宏,用于验证程序的前置/后置条件和内部不变式。所有宏在触发时调用 `assertion_failed` 函数终止程序,不使用 C++ 异常。

### 核心特性

- **Debug 专用断言**：`MINE_ASSERT` / `MINE_ASSERT_MSG` 仅在 Debug 构建有效
- **Release 有效检查**：`MINE_CHECK` / `MINE_CHECK_MSG` 始终有效
- **不可达路径标记**：`MINE_UNREACHABLE` 标记不应到达的代码
- **未实现标记**：`MINE_TODO_NOT_IMPLEMENTED` 标记待实现功能
- **优化提示**：`MINE_ASSUME` 向编译器提供假设以优化代码

### 设计动机

MineFramework 禁用 C++ 异常,因此不能使用 `assert()` 或 `throw`。`Assert` 模块提供了一套无异常的断言机制:

- 失败时调用自定义处理函数（而非 `abort()`）
- 可记录详细诊断信息（文件、行号、表达式）
- 区分 Debug 专用断言和 Release 检查
- 支持平台调试陷阱（断点）

---

## 宏定义

### MINE_ASSERT

```cpp
#define MINE_ASSERT(cond)                                               \
    do {                                                                \
        if (!(cond)) {                                                  \
            ::mine::core::detail::assertion_failed(                     \
                #cond, __FILE__, __LINE__, nullptr);                     \
        }                                                               \
    } while (false)
```

**描述**：仅在 Debug 构建中检查条件,Release 构建下为空操作。

**参数**：
- `cond`：要检查的条件表达式

**行为**：
- Debug 模式（`!defined(NDEBUG)`）：若 `cond` 为假,调用 `assertion_failed` 终止程序
- Release 模式（`defined(NDEBUG)`）：展开为 `(void)sizeof(cond)`,无运行时代码

**用途**：
- 前置条件检查
- 后置条件检查
- 内部不变式验证

**时间复杂度**：
- Debug：O(1) + 条件求值
- Release：O(0)（编译期消除）

**示例**：

```cpp
void set_value(int* ptr, int value) {
    MINE_ASSERT(ptr != nullptr);  // 前置条件：指针非空
    *ptr = value;
}

T& Vector<T>::operator[](size_t index) {
    MINE_ASSERT(index < size_);  // 前置条件：索引有效
    return data_[index];
}
```

---

### MINE_ASSERT_MSG

```cpp
#define MINE_ASSERT_MSG(cond, msg)                                      \
    do {                                                                \
        if (!(cond)) {                                                  \
            ::mine::core::detail::assertion_failed(                     \
                #cond, __FILE__, __LINE__, msg);                        \
            }                                                           \
    } while (false)
```

**描述**：带附加说明的断言,仅在 Debug 构建有效。

**参数**：
- `cond`：要检查的条件表达式
- `msg`：失败时显示的附加说明（C 字符串字面量）

**示例**：

```cpp
void initialize(Config* cfg) {
    MINE_ASSERT_MSG(cfg != nullptr, "Config cannot be null");
    MINE_ASSERT_MSG(cfg->version >= 2, "Config version must be >= 2");
}
```

---

### MINE_CHECK

```cpp
#define MINE_CHECK(cond)                                                \
    do {                                                                \
        if (!(cond)) {                                                  \
            ::mine::core::detail::assertion_failed(                     \
                #cond, __FILE__, __LINE__, nullptr);                        \
        }                                                               \
    } while (false)
```

**描述**：Release/Debug 均有效的合约检查,触发时终止程序。

**参数**：
- `cond`：要检查的条件表达式

**行为**：Debug 和 Release 构建均生成检查代码,失败时调用 `assertion_failed`。

**用途**：
- 不可恢复的错误检查
- 公共 API 的参数验证
- 内存分配失败检查

**时间复杂度**：O(1) + 条件求值

**示例**：

```cpp
void* allocate(size_t size) {
    void* ptr = malloc(size);
    MINE_CHECK(ptr != nullptr);  // 分配失败不可恢复
    return ptr;
}

void process(Span<int> data) {
    MINE_CHECK(!data.empty());  // 公共 API 参数验证
    // ...
}
```

---

### MINE_CHECK_MSG

```cpp
#define MINE_CHECK_MSG(cond, msg)                                       \
    do {                                                                \
        if (!(cond)) {                                                  \
            ::mine::core::detail::assertion_failed(                     \
                #cond, __FILE__, __LINE__, msg);                        \
        }                                                               \
    } while (false)
```

**描述**：带附加说明的 Release/Debug 检查。

**参数**：
- `cond`：要检查的条件表达式
- `msg`：失败时显示的附加说明

**示例**：

```cpp
void load_plugin(const char* path) {
    MINE_CHECK_MSG(path != nullptr, "Plugin path cannot be null");
    MINE_CHECK_MSG(file_exists(path), "Plugin file not found");
}
```

---

### MINE_UNREACHABLE

```cpp
#define MINE_UNREACHABLE()                                              \
    do {                                                                \
        ::mine::core::detail::assertion_failed(                         \
            "UNREACHABLE", __FILE__, __LINE__, nullptr);                \
    } while (false)
```

**描述**：标记当前代码路径不可达,到达时终止程序并打印诊断信息。

**用途**：
- `switch` 语句的 `default` 分支（已覆盖所有枚举值）
- 不应返回的函数末尾
- 逻辑上不可能的代码路径

**优化提示**：在发布构建中同时向编译器提供 `unreachable` 提示以优化代码。

**示例**：

```cpp
enum class State { Idle, Running, Stopped };

void handle_state(State s) {
    switch (s) {
        case State::Idle:    /* ... */ break;
        case State::Running: /* ... */ break;
        case State::Stopped: /* ... */ break;
        default:
            MINE_UNREACHABLE();  // 所有值已覆盖
    }
}
```

---

### MINE_TODO_NOT_IMPLEMENTED

```cpp
#define MINE_TODO_NOT_IMPLEMENTED()                                     \
    do {                                                                \
        ::mine::core::detail::assertion_failed(                         \
            "NOT_IMPLEMENTED", __FILE__, __LINE__, nullptr);            \
    } while (false)
```

**描述**：在函数体内标记"此功能尚未实现",调用时立即终止程序。

**用途**：
- 标记待实现的虚函数
- 标记暂未支持的功能分支
- 禁止用空函数体静默通过

**编码规范**：禁止提交包含此宏的代码到生产分支,必须在实现前显式注册为 issue。

**示例**：

```cpp
class Future_Feature {
public:
    virtual void advanced_operation() {
        MINE_TODO_NOT_IMPLEMENTED();  // 显式标记未实现
    }
};

// ❌ 错误（静默失败）
class BadFeature {
public:
    virtual void advanced_operation() {}  // 空函数体,调用方不知道未实现
};
```

---

### MINE_ASSUME

```cpp
#if defined(_MSC_VER)
#    define MINE_ASSUME(cond) __assume(cond)
#elif defined(__clang__)
#    define MINE_ASSUME(cond) __builtin_assume(cond)
#elif defined(__GNUC__)
#    define MINE_ASSUME(cond)                                           \
        do {                                                            \
            if (!(cond)) __builtin_unreachable();                       \
        } while (false)
#else
#    define MINE_ASSUME(cond) ((void)0)
#endif
```

**描述**：向优化器断言条件成立,允许更激进的代码优化。

**参数**：
- `cond`：假设成立的条件

**警告**：
- 条件为假时行为未定义
- 仅在确有把握时使用
- 不检查条件,纯粹用于优化

**用途**：
- 向编译器提供额外信息以优化循环
- 消除冗余检查
- 指示指针对齐

**示例**：

```cpp
void process_aligned(float* ptr, size_t count) {
    MINE_ASSUME((uintptr_t(ptr) % 16) == 0);  // 假设 ptr 16 字节对齐
    // 编译器可生成向量化代码
    for (size_t i = 0; i < count; ++i) {
        ptr[i] *= 2.0f;
    }
}

void search(Span<int> data, int target) {
    MINE_ASSUME(data.size() > 0);  // 假设非空
    // 编译器可省略空检查
    for (int x : data) {
        if (x == target) return;
    }
}
```

---

## assertion_failed 函数

```cpp
namespace mine::core::detail {

[[noreturn]] void assertion_failed(
    const char* expr,
    const char* file,
    int         line,
    const char* msg) noexcept;

} // namespace mine::core::detail
```

**描述**：断言失败时调用,输出诊断信息并终止程序。

**参数**：
- `expr`：失败的表达式字符串（由宏自动填入）
- `file`：源文件路径（`__FILE__`）
- `line`：行号（`__LINE__`）
- `msg`：可选的附加说明（`nullptr` 时忽略）

**行为**：
1. 输出诊断信息到 `stderr` 或日志系统
2. 触发平台调试陷阱（若调试器附加）
3. 调用 `std::abort()` 终止程序

**实现示例**：

```cpp
[[noreturn]] void assertion_failed(
    const char* expr,
    const char* file,
    int         line,
    const char* msg) noexcept
{
    fprintf(stderr, "Assertion failed: %s\n", expr);
    fprintf(stderr, "  File: %s:%d\n", file, line);
    if (msg) {
        fprintf(stderr, "  Message: %s\n", msg);
    }
    
    MINE_DEBUG_BREAK();  // 触发断点
    std::abort();
}
```

---

## MINE_DEBUG_BREAK 宏

```cpp
#if defined(_MSC_VER)
#    define MINE_DEBUG_BREAK() __debugbreak()
#elif defined(__clang__) || defined(__GNUC__)
#    define MINE_DEBUG_BREAK() __builtin_trap()
#else
#    define MINE_DEBUG_BREAK() ::abort()
#endif
```

**描述**：触发平台特定的调试陷阱（断点）。

**行为**：
- MSVC：调用 `__debugbreak()`,触发 `INT 3` 指令
- GCC/Clang：调用 `__builtin_trap()`,触发 `SIGTRAP`
- 其他：回退到 `abort()`

**用途**：在断言失败时暂停调试器,允许检查调用栈和变量状态。

---

## 使用场景

### 1. 前置条件检查

```cpp
T& operator[](size_t index) {
    MINE_ASSERT(index < size());  // 前置条件：索引有效
    return data_[index];
}

void set_parent(Visual* parent) {
    MINE_ASSERT(parent != this);  // 前置条件：不能自己作为父节点
    parent_ = parent;
}
```

---

### 2. 后置条件检查

```cpp
void sort(Span<int> data) {
    // 排序逻辑...
    
    // 后置条件：结果有序
    MINE_ASSERT(is_sorted(data));
}
```

---

### 3. 内部不变式验证

```cpp
class BinaryTree {
    void check_invariants() const {
        MINE_ASSERT(size_ >= 0);  // 不变式 1：大小非负
        MINE_ASSERT(!root_ || root_->parent == nullptr);  // 不变式 2：根节点无父节点
    }
    
public:
    void insert(int value) {
        // ... 插入逻辑
        check_invariants();  // 插入后验证不变式
    }
};
```

---

### 4. 不可恢复错误检查

```cpp
void* allocate(size_t size) {
    void* ptr = malloc(size);
    MINE_CHECK(ptr != nullptr);  // Release 也检查
    return ptr;
}
```

---

### 5. 枚举覆盖检查

```cpp
const char* state_to_string(State s) {
    switch (s) {
        case State::Idle:    return "Idle";
        case State::Running: return "Running";
        case State::Stopped: return "Stopped";
        default:
            MINE_UNREACHABLE();  // 所有枚举值已覆盖
    }
}
```

---

## 性能分析

### Debug 模式

| 宏 | 开销 | 说明 |
|----|----|------|
| `MINE_ASSERT` | 条件检查 + 可能的函数调用 | 失败时才调用 `assertion_failed` |
| `MINE_CHECK` | 同上 | 始终生成检查代码 |

**估算**：每个断言 ~5-10 条指令（条件分支 + 可能的函数调用）。

### Release 模式

| 宏 | 开销 | 说明 |
|----|----|------|
| `MINE_ASSERT` | 0 | 完全消除 |
| `MINE_CHECK` | 条件检查 + 可能的函数调用 | 与 Debug 相同 |
| `MINE_UNREACHABLE` | 0 | 变为 `__builtin_unreachable()`,用于优化 |

---

## 最佳实践

### 1. 使用 MINE_ASSERT 验证内部假设

**推荐**：用 `MINE_ASSERT` 检查内部不变式,Release 下自动消除。

```cpp
// ✅ Debug 专用断言
void internal_operation(int* ptr) {
    MINE_ASSERT(ptr != nullptr);  // 内部假设
    *ptr = 42;
}
```

---

### 2. 使用 MINE_CHECK 验证公共 API

**推荐**：用 `MINE_CHECK` 检查公共 API 参数,Release 也保留检查。

```cpp
// ✅ 公共 API 检查
MINE_API void process_data(Span<int> data) {
    MINE_CHECK(!data.empty());  // 公共 API 参数验证
    // ...
}
```

---

### 3. 区分可恢复和不可恢复错误

**推荐**：不可恢复错误用 `MINE_CHECK`,可恢复错误用 `Result<T>`。

```cpp
// ✅ 不可恢复：内存分配失败
void* ptr = malloc(size);
MINE_CHECK(ptr != nullptr);

// ✅ 可恢复：文件打开失败
Result<File> open_file(StringView path) {
    FILE* fp = fopen(path.data(), "rb");
    if (!fp) return make_err<File>(StatusCode::NotFound);
    return make_ok<File>(fp);
}
```

---

### 4. 提供有意义的错误消息

**推荐**：使用 `_MSG` 变体提供上下文信息。

```cpp
// ❌ 不够清晰
MINE_ASSERT(config);

// ✅ 清晰
MINE_ASSERT_MSG(config != nullptr, "Config must be initialized before use");
```

---

### 5. 避免断言副作用

**推荐**：断言条件不应有副作用,因为 Release 模式下会被消除。

```cpp
// ❌ 错误：断言有副作用
MINE_ASSERT(++counter < 10);  // Release 下 counter 不递增

// ✅ 正确：先执行,再断言
++counter;
MINE_ASSERT(counter < 10);
```

---

## 完整示例

### 示例：Vector 容器

```cpp
template<typename T>
class Vector {
    T* data_{nullptr};
    size_t size_{0};
    size_t capacity_{0};

    // 内部不变式
    void check_invariants() const {
        MINE_ASSERT(size_ <= capacity_);
        MINE_ASSERT((data_ == nullptr) == (capacity_ == 0));
    }

public:
    T& operator[](size_t index) {
        MINE_ASSERT(index < size_);  // 前置条件：索引有效
        return data_[index];
    }

    void push_back(const T& value) {
        if (size_ == capacity_) {
            size_t new_cap = capacity_ ? capacity_ * 2 : 8;
            T* new_data = allocate(new_cap);
            MINE_CHECK(new_data != nullptr);  // 不可恢复错误
            
            // 移动元素...
            deallocate(data_, capacity_);
            data_ = new_data;
            capacity_ = new_cap;
        }
        
        new (&data_[size_++]) T(value);
        check_invariants();  // 后置条件：不变式仍成立
    }
};
```

---

## 常见问题

### Q1：何时使用 MINE_ASSERT vs MINE_CHECK？

**A**：
- `MINE_ASSERT`：内部假设、性能关键路径、Release 下可容忍的 UB
- `MINE_CHECK`：公共 API 验证、不可恢复错误、安全关键检查

---

### Q2：如何在生产环境中记录断言失败？

**A**：自定义 `assertion_failed` 实现:

```cpp
[[noreturn]] void assertion_failed(...) noexcept {
    // 记录到日志系统
    log_critical("Assertion failed: {}", expr);
    
    // 生成 crash dump
    generate_minidump();
    
    // 终止程序
    std::abort();
}
```

---

### Q3：MINE_ASSUME 何时安全？

**A**：仅在有数学/逻辑证明时使用:

```cpp
// ✅ 安全：算法保证
void binary_search(Span<int> sorted_data, int target) {
    MINE_ASSUME(!sorted_data.empty());  // 调用方保证
    // ...
}

// ❌ 不安全：无法保证
void risky(int* ptr) {
    MINE_ASSUME(ptr != nullptr);  // 外部输入,无法保证
}
```

---

## 总结

`Assert` 模块提供了 MineFramework 的无异常断言机制,具备以下优势:

1. **分层检查**：Debug 专用断言 + Release 有效检查
2. **详细诊断**：记录表达式、文件、行号、附加说明
3. **调试友好**：触发平台断点,暂停调试器
4. **零开销**：Debug 断言在 Release 下完全消除
5. **可扩展**：自定义 `assertion_failed` 实现

在编写代码时,合理使用断言验证前置/后置条件和内部不变式,这将显著提升代码的健壮性和可维护性。
