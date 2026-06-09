# Result<T, E> 详细接口文档

## 概述

`Result<T, E>` 是 MineFramework 中用于表示操作可能成功（返回值 `T`）或失败（返回错误 `E`）的判别联合类型。它是 C++ 异常机制的替代方案,符合项目禁用异常的设计约束。

### 核心特性

- **显式错误处理**：错误通过返回值传递,调用方无法忽略（标记 `[[nodiscard]]`）
- **零开销**：使用无名联合存储,无堆分配,可在寄存器中传递
- **类型安全**：编译期区分成功值 `T` 和错误值 `E`
- **不依赖 RTTI**：通过 `bool` 判别位区分成功/失败
- **移动优化**：支持移动语义,避免昂贵的拷贝
- **标签构造**：通过 `ok_tag` / `err_tag` 消除构造歧义

### 设计动机

MineFramework 禁用 C++ 异常（编译标志 `/GR-`）,原因包括:

1. **性能开销**：异常展开机制在热路径上引入不可预测的开销
2. **ABI 复杂性**：跨 DLL 边界抛出异常可能导致运行时崩溃
3. **代码膨胀**：异常支持增加二进制大小和栈帧开销
4. **确定性**：游戏引擎/UI 框架需要可预测的控制流

`Result<T, E>` 提供了一种显式、类型安全的错误处理机制:

- 编译器强制错误检查（`[[nodiscard]]`）
- 零运行时开销（无异常表）
- 可组合的错误传播（`MINE_TRY` 宏）
- 明确的成功/失败路径

### 典型使用场景

| 场景 | 用法示例 |
|------|----------|
| 文件操作 | `Result<FileHandle> open_file(StringView path)` |
| 数值解析 | `Result<int> parse_int(StringView str)` |
| 资源加载 | `Result<Texture> load_texture(StringView name)` |
| 数据库查询 | `Result<Row> query(StringView sql)` |
| 网络请求 | `Result<Response, HttpError> http_get(StringView url)` |

---

## 类定义

```cpp
namespace mine::core {

// 标签类型
struct OkTag  { explicit OkTag()  = default; };
struct ErrTag { explicit ErrTag() = default; };

inline constexpr OkTag  ok_tag{};
inline constexpr ErrTag err_tag{};

// Result 类
template<typename T, typename E = Status>
class [[nodiscard]] Result {
public:
    // 构造
    template<typename... Args>
    explicit Result(OkTag, Args&&... args) noexcept;
    
    template<typename... Args>
    explicit Result(ErrTag, Args&&... args) noexcept;

    ~Result() noexcept;

    // 拷贝/移动
    Result(const Result& other) noexcept(/*conditional*/);
    Result(Result&& other) noexcept(/*conditional*/);
    Result& operator=(const Result& other) noexcept(/*conditional*/);
    Result& operator=(Result&& other) noexcept(/*conditional*/);

    // 查询
    [[nodiscard]] bool ok() const noexcept;
    explicit operator bool() const noexcept;

    // 值访问
    [[nodiscard]] T& value() & noexcept;
    [[nodiscard]] const T& value() const& noexcept;
    [[nodiscard]] T&& value() && noexcept;
    
    [[nodiscard]] T& operator*() & noexcept;
    [[nodiscard]] const T& operator*() const& noexcept;
    [[nodiscard]] T* operator->() noexcept;
    [[nodiscard]] const T* operator->() const noexcept;

    // 错误访问
    [[nodiscard]] E& error() & noexcept;
    [[nodiscard]] const E& error() const& noexcept;

    // 安全取值
    template<typename U = T>
    [[nodiscard]] T value_or(U&& default_val) const& noexcept;

private:
    union Data {
        T value;
        E error;
    } data_;
    bool has_value_{false};
};

// 工厂函数
template<typename T, typename... Args>
[[nodiscard]] Result<T> make_ok(Args&&... args) noexcept;

template<typename T, typename E = Status>
[[nodiscard]] Result<T, E> make_err(E error) noexcept;

template<typename T>
[[nodiscard]] Result<T> make_err(StatusCode code) noexcept;

} // namespace mine::core
```

---

## 标签类型

### OkTag / ErrTag

```cpp
struct OkTag  { explicit OkTag()  = default; };
struct ErrTag { explicit ErrTag() = default; };

inline constexpr OkTag  ok_tag{};
inline constexpr ErrTag err_tag{};
```

**用途**：消除成功/失败构造的歧义。

**动机**：若 `T` 和 `E` 类型相同（如 `Result<int, int>`）,无标签时无法区分:

```cpp
// ❌ 歧义：1 是成功值还是错误码？
Result<int, int> ambiguous{1};  // 编译错误

// ✅ 通过标签明确语义
Result<int, int> success{ok_tag, 1};   // 成功值 = 1
Result<int, int> failure{err_tag, 1};  // 错误码 = 1
```

**使用约定**：
- 使用全局常量 `ok_tag` 和 `err_tag` 而非手动构造
- 标签必须作为第一个参数传递

---

## 构造函数

### 成功构造

```cpp
template<typename... Args>
    requires std::is_constructible_v<T, Args...>
explicit Result(OkTag, Args&&... args) noexcept;
```

**描述**：以成功标签和转发参数就地构造值 `T`。

**模板参数**：
- `Args`：转发到 `T` 构造函数的参数包

**约束**：`T` 必须可从 `Args...` 构造

**参数**：
- 第一个参数必须为 `ok_tag`
- `args`：转发给 `T` 构造函数的参数

**效果**：
- `has_value_ = true`
- 在联合体内就地构造 `data_.value`（使用 `std::construct_at`）

**时间复杂度**：O(1) + `T` 的构造开销

**示例**：

```cpp
// 基本类型
Result<int> r1{ok_tag, 42};
MINE_ASSERT(r1.ok());
MINE_ASSERT(r1.value() == 42);

// 复杂类型（转发构造参数）
struct Point { float x, y; };
Result<Point> r2{ok_tag, 1.0f, 2.0f};
MINE_ASSERT(r2->x == 1.0f);

// 使用 make_ok 工厂函数（更简洁）
auto r3 = mine::core::make_ok<int>(42);
```

---

### 失败构造

```cpp
template<typename... Args>
    requires std::is_constructible_v<E, Args...>
explicit Result(ErrTag, Args&&... args) noexcept;
```

**描述**：以失败标签和转发参数就地构造错误 `E`。

**模板参数**：
- `Args`：转发到 `E` 构造函数的参数包

**约束**：`E` 必须可从 `Args...` 构造

**参数**：
- 第一个参数必须为 `err_tag`
- `args`：转发给 `E` 构造函数的参数

**效果**：
- `has_value_ = false`
- 在联合体内就地构造 `data_.error`

**时间复杂度**：O(1) + `E` 的构造开销

**示例**：

```cpp
// 使用默认错误类型 Status
Result<int> r1{err_tag, Status{StatusCode::NotFound}};
MINE_ASSERT(!r1.ok());
MINE_ASSERT(r1.error().code() == StatusCode::NotFound);

// 自定义错误类型
enum class ParseError { InvalidFormat, Overflow };
Result<int, ParseError> r2{err_tag, ParseError::InvalidFormat};
MINE_ASSERT(r2.error() == ParseError::InvalidFormat);

// 使用 make_err 工厂函数
auto r3 = mine::core::make_err<int>(StatusCode::InvalidArg);
```

---

### 析构函数

```cpp
~Result() noexcept;
```

**描述**：根据 `has_value_` 判别位,析构联合体内的活跃对象。

**效果**：
- 若 `has_value_ == true`：调用 `std::destroy_at(&data_.value)`
- 否则：调用 `std::destroy_at(&data_.error)`

**noexcept 保证**：要求 `T` 和 `E` 的析构函数均为 `noexcept`。

**实现细节**：

```cpp
~Result() noexcept {
    if (has_value_) {
        std::destroy_at(std::addressof(data_.value));
    } else {
        std::destroy_at(std::addressof(data_.error));
    }
}
```

---

### 拷贝构造

```cpp
Result(const Result& other) noexcept(
    std::is_nothrow_copy_constructible_v<T> &&
    std::is_nothrow_copy_constructible_v<E>);
```

**描述**：拷贝构造,复制活跃对象。

**条件 noexcept**：当且仅当 `T` 和 `E` 均为 `noexcept` 拷贝构造时,此构造函数为 `noexcept`。

**效果**：
- 拷贝 `other.has_value_` 到 `this->has_value_`
- 根据判别位拷贝构造对应的联合体成员

**时间复杂度**：O(1) + `T` 或 `E` 的拷贝开销

**示例**：

```cpp
Result<std::string> r1{ok_tag, "hello"};
Result<std::string> r2 = r1;  // 拷贝构造
MINE_ASSERT(r2.value() == "hello");
MINE_ASSERT(r1.value() == "hello");  // r1 仍有效
```

---

### 移动构造

```cpp
Result(Result&& other) noexcept(
    std::is_nothrow_move_constructible_v<T> &&
    std::is_nothrow_move_constructible_v<E>);
```

**描述**：移动构造,转移活跃对象所有权。

**条件 noexcept**：当且仅当 `T` 和 `E` 均为 `noexcept` 移动构造时,此构造函数为 `noexcept`。

**效果**：
- 拷贝 `other.has_value_` 到 `this->has_value_`
- 根据判别位移动构造对应的联合体成员
- `other` 保持有效但未指定状态（联合体成员已移出）

**时间复杂度**：O(1) + `T` 或 `E` 的移动开销

**示例**：

```cpp
Result<OwnedPtr<Data>> r1{ok_tag, make_owned<Data>()};
Result<OwnedPtr<Data>> r2 = std::move(r1);  // 移动构造
MINE_ASSERT(r2.ok());
// r1 现在处于已移出状态,不应继续使用
```

---

### 拷贝赋值

```cpp
Result& operator=(const Result& other) noexcept(
    std::is_nothrow_copy_constructible_v<T> &&
    std::is_nothrow_copy_constructible_v<E>);
```

**描述**：拷贝赋值,先析构当前对象再拷贝构造。

**效果**：
1. 检查自赋值（`this != &other`）
2. 调用 `this->~Result()` 析构当前活跃对象
3. 以拷贝构造语义重建 `*this`

**时间复杂度**：O(1) + `T`/`E` 的析构和拷贝开销

**实现策略**：使用"析构 + placement new"而非逐成员赋值,简化实现并保证强异常安全（虽然项目禁用异常）。

**示例**：

```cpp
Result<int> r1{ok_tag, 10};
Result<int> r2{ok_tag, 20};

r2 = r1;  // 拷贝赋值
MINE_ASSERT(r2.value() == 10);
```

---

### 移动赋值

```cpp
Result& operator=(Result&& other) noexcept(
    std::is_nothrow_move_constructible_v<T> &&
    std::is_nothrow_move_constructible_v<E>);
```

**描述**：移动赋值,先析构当前对象再移动构造。

**效果**：
1. 检查自赋值
2. 调用 `this->~Result()` 析构当前活跃对象
3. 以移动构造语义重建 `*this`

**时间复杂度**：O(1) + `T`/`E` 的析构和移动开销

**示例**：

```cpp
Result<OwnedPtr<Data>> r1{ok_tag, make_owned<Data>()};
Result<OwnedPtr<Data>> r2{err_tag, Status{}};

r2 = std::move(r1);  // 移动赋值
MINE_ASSERT(r2.ok());
```

---

## 查询方法

### ok

```cpp
[[nodiscard]] bool ok() const noexcept;
```

**描述**：检查是否持有成功值。

**返回值**：
- `true`：持有成功值 `T`
- `false`：持有错误 `E`

**时间复杂度**：O(1)

**示例**：

```cpp
Result<int> r1{ok_tag, 42};
if (r1.ok()) {
    int val = r1.value();  // 安全访问
}

Result<int> r2{err_tag, Status{}};
MINE_ASSERT(!r2.ok());
```

---

### operator bool

```cpp
explicit operator bool() const noexcept;
```

**描述**：显式布尔转换,等价于 `ok()`。

**返回值**：`has_value_`

**显式转换**：标记 `explicit`,防止意外的隐式转换：

```cpp
Result<int> r{ok_tag, 42};
if (r) { /* OK */ }           // 显式转换
// bool b = r;                // 编译错误：隐式转换被禁止
```

**时间复杂度**：O(1)

**示例**：

```cpp
Result<int> divide(int a, int b) {
    if (b == 0) return {err_tag, StatusCode::InvalidArg};
    return {ok_tag, a / b};
}

auto r = divide(10, 2);
if (r) {  // 隐式调用 operator bool
    std::cout << *r << '\n';  // 输出 5
}
```

---

## 值访问方法

### value (左值引用)

```cpp
[[nodiscard]] T& value() & noexcept;
[[nodiscard]] const T& value() const& noexcept;
```

**描述**：获取成功值的引用。

**返回值**：`data_.value` 的左值引用

**前置条件**：`ok() == true`

**断言**：Debug 模式下检查 `has_value_`,失败时触发 `MINE_ASSERT`：

```
MINE_ASSERT_MSG(has_value_, "在失败的 Result 上调用 value()")
```

**未定义行为**：在失败的 `Result` 上调用 `value()` 且未触发断言时,行为未定义。

**时间复杂度**：O(1)

**示例**：

```cpp
Result<std::string> r{ok_tag, "hello"};
std::string& ref = r.value();
ref += " world";
MINE_ASSERT(r.value() == "hello world");

const Result<int> cr{ok_tag, 42};
int x = cr.value();  // const 左值引用
```

---

### value (右值引用)

```cpp
[[nodiscard]] T&& value() && noexcept;
```

**描述**：从右值 `Result` 移出成功值。

**返回值**：`data_.value` 的右值引用

**前置条件**：`ok() == true`

**用途**：避免不必要的拷贝,直接移出 `Result` 内的值。

**时间复杂度**：O(1)

**示例**：

```cpp
Result<OwnedPtr<Data>> create_data() {
    return {ok_tag, make_owned<Data>()};
}

OwnedPtr<Data> ptr = std::move(create_data()).value();  // 移出值
// 或使用 *&&
OwnedPtr<Data> ptr2 = *create_data();
```

---

### operator* (解引用)

```cpp
[[nodiscard]] T& operator*() & noexcept;
[[nodiscard]] const T& operator*() const& noexcept;
```

**描述**：解引用运算符,等价于 `value()`。

**语法糖**：提供类似智能指针的语法。

**前置条件**：`ok() == true`

**时间复杂度**：O(1)

**示例**：

```cpp
Result<int> r{ok_tag, 42};
int x = *r;  // 等价于 r.value()
MINE_ASSERT(x == 42);

Result<std::string> rs{ok_tag, "hello"};
*rs += " world";
MINE_ASSERT(*rs == "hello world");
```

---

### operator-> (成员访问)

```cpp
[[nodiscard]] T* operator->() noexcept;
[[nodiscard]] const T* operator->() const noexcept;
```

**描述**：成员访问运算符,返回值的指针。

**返回值**：`&value()`

**前置条件**：`ok() == true`

**用途**：直接访问 `T` 的成员（当 `T` 为类类型时）。

**时间复杂度**：O(1)

**示例**：

```cpp
struct Point { float x, y; };
Result<Point> r{ok_tag, 1.0f, 2.0f};

float x_val = r->x;  // 等价于 r.value().x
r->y = 3.0f;
MINE_ASSERT(r->y == 3.0f);
```

---

## 错误访问方法

### error

```cpp
[[nodiscard]] E& error() & noexcept;
[[nodiscard]] const E& error() const& noexcept;
```

**描述**：获取错误值的引用。

**返回值**：`data_.error` 的引用

**前置条件**：`ok() == false`

**断言**：Debug 模式下检查 `!has_value_`,成功时触发断言：

```
MINE_ASSERT_MSG(!has_value_, "在成功的 Result 上调用 error()")
```

**时间复杂度**：O(1)

**示例**：

```cpp
Result<int> r{err_tag, Status{StatusCode::NotFound}};
Status err = r.error();
MINE_ASSERT(err.code() == StatusCode::NotFound);

// 自定义错误类型
enum class MyError { A, B };
Result<int, MyError> r2{err_tag, MyError::A};
MINE_ASSERT(r2.error() == MyError::A);
```

---

## 安全取值方法

### value_or

```cpp
template<typename U = T>
    requires std::is_convertible_v<U, T>
[[nodiscard]] T value_or(U&& default_val) const& noexcept;
```

**描述**：安全取值,成功时返回值,失败时返回默认值。

**模板参数**：
- `U`：默认值类型,必须可转换为 `T`

**参数**：
- `default_val`：失败时返回的默认值

**返回值**：
- 若 `ok() == true`：返回 `data_.value` 的拷贝
- 否则：返回 `static_cast<T>(std::forward<U>(default_val))`

**时间复杂度**：O(1) + `T` 的拷贝/移动开销

**用途**：提供回退值,避免显式错误检查。

**示例**：

```cpp
Result<int> divide(int a, int b) {
    if (b == 0) return {err_tag, StatusCode::InvalidArg};
    return {ok_tag, a / b};
}

int result = divide(10, 2).value_or(0);  // result = 5
int fallback = divide(10, 0).value_or(0);  // fallback = 0

// 使用可转换类型
Result<std::string> r{err_tag, Status{}};
std::string s = r.value_or("default");  // s = "default"
```

---

## 工厂函数

### make_ok

```cpp
template<typename T, typename... Args>
    requires std::is_constructible_v<T, Args...>
[[nodiscard]] Result<T> make_ok(Args&&... args) noexcept;
```

**描述**：构造成功的 `Result<T>`,就地转发构造参数。

**模板参数**：
- `T`：成功值类型（必须显式指定）
- `Args`：转发到 `T` 构造函数的参数包

**约束**：`T` 必须可从 `Args...` 构造

**返回值**：`Result<T>{ok_tag, std::forward<Args>(args)...}`

**时间复杂度**：O(1) + `T` 的构造开销

**示例**：

```cpp
// 基本类型
auto r1 = mine::core::make_ok<int>(42);
MINE_ASSERT(r1.ok());

// 多参数构造
struct Rect { float x, y, w, h; };
auto r2 = mine::core::make_ok<Rect>(0.0f, 0.0f, 100.0f, 50.0f);
MINE_ASSERT(r2->w == 100.0f);

// 避免显式标签
// Result<int> r3{ok_tag, 42};     // 啰嗦
auto r3 = make_ok<int>(42);        // 简洁
```

---

### make_err (错误对象)

```cpp
template<typename T, typename E = Status>
[[nodiscard]] Result<T, E> make_err(E error) noexcept;
```

**描述**：构造失败的 `Result<T, E>`,接受已构造的错误对象。

**模板参数**：
- `T`：成功值类型（必须显式指定）
- `E`：错误类型（默认 `Status`）

**参数**：
- `error`：错误对象（移动传递）

**返回值**：`Result<T, E>{err_tag, std::move(error)}`

**时间复杂度**：O(1) + `E` 的移动开销

**示例**：

```cpp
// 使用默认错误类型 Status
auto r1 = mine::core::make_err<int>(Status{StatusCode::NotFound});
MINE_ASSERT(!r1.ok());

// 自定义错误类型
enum class ParseError { InvalidFormat };
auto r2 = mine::core::make_err<int, ParseError>(ParseError::InvalidFormat);
MINE_ASSERT(r2.error() == ParseError::InvalidFormat);
```

---

### make_err (错误码)

```cpp
template<typename T>
[[nodiscard]] Result<T> make_err(StatusCode code) noexcept;
```

**描述**：以 `StatusCode` 快速构造失败的 `Result<T>`。

**模板参数**：
- `T`：成功值类型（必须显式指定）

**参数**：
- `code`：状态码（自动包装为 `Status{code}`）

**返回值**：`Result<T>{err_tag, Status{code}}`

**时间复杂度**：O(1)

**用途**：简化常见错误构造,避免手动包装 `Status`。

**示例**：

```cpp
Result<int> parse_int(StringView str) {
    if (str.empty())
        return mine::core::make_err<int>(StatusCode::InvalidArg);
    
    // ... 解析逻辑
    return mine::core::make_ok<int>(result);
}
```

---

## 使用场景

### 1. 文件操作

**问题**：文件打开可能失败（不存在、权限不足等）。

**解决方案**：返回 `Result<FileHandle>`,调用方显式检查。

```cpp
Result<FileHandle> open_file(StringView path) {
    FILE* fp = fopen(path.data(), "rb");
    if (!fp) {
        return mine::core::make_err<FileHandle>(StatusCode::IoError);
    }
    return mine::core::make_ok<FileHandle>(fp);
}

void process() {
    auto result = open_file("config.txt");
    if (!result) {
        // 处理错误
        log_error("Failed to open file: {}", result.error());
        return;
    }
    
    FileHandle handle = *result;
    // 使用文件句柄...
}
```

---

### 2. 数值解析

**问题**：字符串转数值可能因格式错误或溢出失败。

**解决方案**：返回 `Result<T>` 而非抛出异常。

```cpp
Result<int> parse_int(StringView str) {
    if (str.empty())
        return mine::core::make_err<int>(StatusCode::InvalidArg);
    
    const char* end = nullptr;
    long val = std::strtol(str.data(), &end, 10);
    
    if (end != str.data() + str.size())
        return mine::core::make_err<int>(StatusCode::ParseError);
    
    if (val > INT_MAX || val < INT_MIN)
        return mine::core::make_err<int>(StatusCode::OutOfRange);
    
    return mine::core::make_ok<int>(static_cast<int>(val));
}

void example() {
    auto r1 = parse_int("123");
    MINE_ASSERT(r1.ok() && *r1 == 123);
    
    auto r2 = parse_int("abc");
    MINE_ASSERT(!r2.ok());
    
    auto r3 = parse_int("");
    MINE_ASSERT(!r3.ok());
}
```

---

### 3. 资源加载

**问题**：加载纹理/模型/音频可能因文件损坏或内存不足失败。

**解决方案**：返回 `Result<Resource>`,支持回退资源。

```cpp
Result<Texture> load_texture(StringView name) {
    auto file_result = read_file(name);
    if (!file_result)
        return mine::core::make_err<Texture>(file_result.error());
    
    auto decode_result = decode_png(file_result.value());
    if (!decode_result)
        return mine::core::make_err<Texture>(StatusCode::ParseError);
    
    return mine::core::make_ok<Texture>(decode_result.value());
}

Texture get_texture_or_placeholder(StringView name) {
    auto result = load_texture(name);
    return result.value_or(placeholder_texture());  // 回退到占位符
}
```

---

### 4. 错误传播（MINE_TRY 宏）

**问题**：多层函数调用需要逐层传播错误,手写检查冗长。

**解决方案**：使用 `MINE_TRY` 宏自动传播。

```cpp
Status initialize_subsystems() {
    MINE_TRY(init_graphics());
    MINE_TRY(init_audio());
    MINE_TRY(init_input());
    return Status::success();
}

// 展开后等价于：
Status initialize_subsystems() {
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

### 5. 数据库查询

**问题**：查询可能因 SQL 语法错误、连接丢失等失败。

**解决方案**：返回 `Result<Row>`,区分"无结果"和"查询失败"。

```cpp
Result<Row> query_user(int user_id) {
    auto conn = get_connection();
    if (!conn)
        return mine::core::make_err<Row>(StatusCode::NotFound);
    
    auto result = conn->execute("SELECT * FROM users WHERE id = ?", user_id);
    if (!result)
        return mine::core::make_err<Row>(StatusCode::IoError);
    
    if (result->empty())
        return mine::core::make_err<Row>(StatusCode::NotFound);
    
    return mine::core::make_ok<Row>(result->front());
}
```

---

## 性能分析

### 内存布局

```cpp
template<typename T, typename E>
class Result {
    union Data {
        T value;
        E error;
    } data_;
    bool has_value_;
    // 可能的填充字节
};
```

**大小**：

```cpp
sizeof(Result<T, E>) = max(sizeof(T), sizeof(E)) + sizeof(bool) + padding
```

**示例**：

```cpp
sizeof(Result<int, Status>)       // 8  (max(4, 4) + 1 + 3 padding)
sizeof(Result<void*, Status>)     // 16 (max(8, 4) + 1 + 3 padding)
sizeof(Result<int, int>)          // 8  (max(4, 4) + 1 + 3 padding)
```

### 操作时间复杂度

| 操作 | 时间复杂度 | 说明 |
|------|-----------|------|
| 构造 | O(1) + `T`/`E` 构造 | 就地构造联合体成员 |
| 析构 | O(1) + `T`/`E` 析构 | 条件析构活跃对象 |
| 拷贝 | O(1) + `T`/`E` 拷贝 | 拷贝活跃对象 |
| 移动 | O(1) + `T`/`E` 移动 | 移动活跃对象 |
| `ok()` | O(1) | 读取 `bool` 判别位 |
| `value()` | O(1) | 指针偏移 + 断言检查 |
| `error()` | O(1) | 指针偏移 + 断言检查 |
| `value_or()` | O(1) + `T` 拷贝/移动 | 条件返回值或默认值 |

### 编译器优化

**无堆分配**：所有数据内联存储,无 `new/delete` 开销。

**寄存器传递**：小型 `Result`（≤ 16 字节）通常完全在寄存器中传递。

**返回值优化（RVO）**：编译器消除临时对象拷贝。

**示例（x86-64）**：

```cpp
Result<int> foo() {
    return mine::core::make_ok<int>(42);
}

// 生成汇编（近似）
foo():
    mov eax, 42        ; value = 42
    mov edx, 1         ; has_value = true
    ret                ; 返回值在 rax:rdx
```

**零开销原则**：`Result<T, E>` 的开销仅为判别位（1 字节 + 对齐填充）,与手写联合体 + `bool` 完全相同。

---

## 线程安全性

### 不可变 Result

**安全**：多个线程同时读取 `const Result` 是安全的。

```cpp
const Result<int> shared{ok_tag, 42};

// 线程 1
int val1 = shared.value();

// 线程 2（同时执行）
int val2 = shared.value();
```

### 可变 Result

**不安全**：并发修改或一写多读需要外部同步。

```cpp
Result<int> mutable_result{ok_tag, 10};

// ❌ 数据竞争
// 线程 1: mutable_result = {ok_tag, 20};
// 线程 2: int x = mutable_result.value();

// ✅ 外部同步
std::mutex mtx;
// 线程 1: { std::lock_guard lock{mtx}; mutable_result = {ok_tag, 20}; }
// 线程 2: { std::lock_guard lock{mtx}; int x = mutable_result.value(); }
```

---

## 限制与注意事项

### 1. 不支持 void 类型

**限制**：`Result<void>` 无意义,使用 `Status` 代替。

```cpp
// ❌ 编译错误
// Result<void> bad();

// ✅ 使用 Status
Status good() {
    if (error_condition)
        return Status{StatusCode::IoError};
    return Status::success();
}
```

---

### 2. 不支持引用类型

**限制**：`Result<T&>` 不支持（联合体不能包含引用成员）。

**解决方案**：使用指针或 `std::reference_wrapper`。

```cpp
// ❌ 编译错误
// Result<int&> bad();

// ✅ 使用指针
Result<int*> good(int& ref) {
    return mine::core::make_ok<int*>(&ref);
}

// ✅ 使用 reference_wrapper
Result<std::reference_wrapper<int>> better(int& ref) {
    return mine::core::make_ok<std::reference_wrapper<int>>(ref);
}
```

---

### 3. [[nodiscard]] 强制检查

**限制**：忽略返回值会触发编译器警告（`-Wunused-result`）。

```cpp
Result<int> compute();

void bad() {
    compute();  // 警告：忽略了 Result 返回值
}

void good() {
    auto result = compute();
    if (!result) {
        // 处理错误
    }
}
```

---

### 4. 值访问前必须检查

**限制**：在失败的 `Result` 上调用 `value()` 在 Debug 模式触发断言,Release 模式为未定义行为。

**最佳实践**：

```cpp
// ❌ 不安全
int x = result.value();  // 若 result 失败,触发断言/UB

// ✅ 安全检查
if (result.ok()) {
    int x = result.value();
}

// ✅ 使用 value_or
int x = result.value_or(default_value);
```

---

### 5. 错误类型必须可拷贝/移动

**限制**：`E` 必须至少可移动构造,否则 `Result` 无法传递。

**推荐**：使用轻量级错误类型（枚举、状态码）,避免包含堆分配对象。

```cpp
// ✅ 轻量级错误
enum class MyError { A, B };
Result<int, MyError> r{err_tag, MyError::A};

// ⚠️ 重量级错误（可用但不推荐）
Result<int, std::string> heavy{err_tag, "error message"};  // 涉及堆分配
```

---

## 最佳实践

### 1. 优先使用工厂函数

**推荐**：使用 `make_ok` / `make_err` 而非直接构造。

```cpp
// ❌ 啰嗦
Result<int> r1{ok_tag, 42};
Result<int> r2{err_tag, Status{StatusCode::NotFound}};

// ✅ 简洁
auto r1 = mine::core::make_ok<int>(42);
auto r2 = mine::core::make_err<int>(StatusCode::NotFound);
```

---

### 2. 函数签名明确返回 Result

**推荐**：在函数签名中明确 `Result` 返回类型,不要使用 `auto`。

```cpp
// ❌ 不清晰
auto process(int x);  // 调用方不知道是否会失败

// ✅ 清晰
Result<int> process(int x);  // 明确可能失败
```

---

### 3. 使用 MINE_TRY 简化错误传播

**推荐**：在返回 `Status` 的函数中使用 `MINE_TRY` 宏。

```cpp
Status init_all() {
    MINE_TRY(init_step1());
    MINE_TRY(init_step2());
    MINE_TRY(init_step3());
    return Status::success();
}
```

---

### 4. 优先使用 value_or 提供默认值

**推荐**：当有合理默认值时,使用 `value_or` 避免显式检查。

```cpp
// ❌ 冗长
int x;
if (result.ok()) {
    x = result.value();
} else {
    x = 0;
}

// ✅ 简洁
int x = result.value_or(0);
```

---

### 5. 区分"无结果"和"失败"

**推荐**：对于查询操作,使用不同的错误码区分"未找到"和"查询失败"。

```cpp
Result<User> find_user(int id) {
    auto conn = get_connection();
    if (!conn)
        return make_err<User>(StatusCode::IoError);  // 查询失败
    
    auto user = conn->query(id);
    if (!user)
        return make_err<User>(StatusCode::NotFound);  // 未找到
    
    return make_ok<User>(*user);
}
```

---

### 6. 自定义错误类型时使用枚举

**推荐**：为模块定义专用错误枚举,而非复用通用 `Status`。

```cpp
enum class ParseError {
    InvalidFormat,
    UnexpectedToken,
    MissingDelimiter,
    Overflow
};

Result<Ast, ParseError> parse(StringView source) {
    // ...
    return make_err<Ast, ParseError>(ParseError::InvalidFormat);
}
```

---

### 7. 移出值时使用 std::move

**推荐**：从右值 `Result` 移出值时显式使用 `std::move`。

```cpp
Result<OwnedPtr<Data>> create();

// ✅ 显式移动
OwnedPtr<Data> ptr = std::move(create()).value();

// ✅ 或使用解引用
OwnedPtr<Data> ptr2 = *create();
```

---

## 完整示例

### 示例 1：配置文件解析

```cpp
#include <mine/core/Result.h>
#include <mine/core/StringView.h>

enum class ConfigError {
    FileNotFound,
    ParseError,
    MissingKey
};

Result<int, ConfigError> parse_config_int(StringView path, StringView key) {
    // 步骤 1：读取文件
    auto file_result = read_file(path);
    if (!file_result)
        return make_err<int, ConfigError>(ConfigError::FileNotFound);
    
    StringView content = file_result.value();
    
    // 步骤 2：解析键值对
    auto kv_result = parse_key_value(content, key);
    if (!kv_result)
        return make_err<int, ConfigError>(ConfigError::MissingKey);
    
    // 步骤 3：转换为整数
    auto int_result = parse_int(kv_result.value());
    if (!int_result)
        return make_err<int, ConfigError>(ConfigError::ParseError);
    
    return make_ok<int>(int_result.value());
}

void example() {
    auto result = parse_config_int("config.txt", "port");
    
    if (!result) {
        switch (result.error()) {
            case ConfigError::FileNotFound:
                log_error("Config file not found");
                break;
            case ConfigError::ParseError:
                log_error("Invalid port number");
                break;
            case ConfigError::MissingKey:
                log_error("Port key missing");
                break;
        }
        return;
    }
    
    int port = result.value();
    start_server(port);
}
```

---

### 示例 2：资源加载管道

```cpp
#include <mine/core/Result.h>

Result<Texture> load_texture_pipeline(StringView name) {
    // 阶段 1：读取文件
    auto file = read_file(name);
    if (!file) return make_err<Texture>(file.error());
    
    // 阶段 2：解码 PNG
    auto image = decode_png(file.value());
    if (!image) return make_err<Texture>(StatusCode::ParseError);
    
    // 阶段 3：上传 GPU
    auto texture = upload_to_gpu(image.value());
    if (!texture) return make_err<Texture>(StatusCode::OutOfMemory);
    
    return make_ok<Texture>(texture.value());
}

class TextureManager {
    HashMap<StringView, Texture> cache_;
    Texture placeholder_;

public:
    Texture get_or_load(StringView name) {
        if (auto it = cache_.find(name); it != cache_.end()) {
            return it->second;  // 缓存命中
        }
        
        auto result = load_texture_pipeline(name);
        if (!result) {
            log_warning("Failed to load texture: {}", name);
            return placeholder_;  // 回退
        }
        
        Texture tex = result.value();
        cache_.insert({name, tex});
        return tex;
    }
};
```

---

### 示例 3：链式操作与 map/and_then

虽然 `Result` 未内置 `map`/`and_then` 方法,但可通过辅助函数实现函数式风格：

```cpp
template<typename T, typename E, typename F>
auto map(Result<T, E>&& r, F&& f) -> Result<std::invoke_result_t<F, T>, E> {
    using U = std::invoke_result_t<F, T>;
    if (!r.ok()) return make_err<U, E>(r.error());
    return make_ok<U>(std::forward<F>(f)(std::move(r).value()));
}

// 使用示例
auto result = parse_int("123")
    .map([](int x) { return x * 2; })
    .map([](int x) { return std::to_string(x); });
// result: Result<std::string> = "246"
```

---

## 与其他错误处理机制的比较

| 机制 | 优点 | 缺点 | 适用场景 |
|------|------|------|----------|
| **Result<T, E>** | 零开销、显式检查、类型安全 | 需手动传播错误 | 性能关键路径、跨 DLL 边界 |
| **C++ 异常** | 自动传播、栈展开清理资源 | 性能不可预测、ABI 复杂 | 非性能关键代码、标准库 |
| **错误码（errno）** | 传统、简单 | 全局状态、易忽略 | 底层 C API |
| **std::optional<T>** | 表示"有/无"值 | 不携带错误信息 | 查询操作、可选参数 |
| **断言（MINE_ASSERT）** | 编译期消除、零开销 | Release 下无检查 | 不变式检查、前置条件 |

**推荐**：
- 公共 API：`Result<T, E>`（强制检查、跨边界安全）
- 内部逻辑：`Result<T, E>` 或断言（取决于错误可恢复性）
- 不可恢复错误：`MINE_CHECK` / `MINE_UNREACHABLE`（直接终止）

---

## 总结

`Result<T, E>` 是 MineFramework 中取代 C++ 异常的核心错误处理机制,具备以下优势:

1. **零运行时开销**：无异常表,内联存储,寄存器传递
2. **强制错误检查**：`[[nodiscard]]` 确保调用方处理错误
3. **类型安全**：编译期区分成功/失败类型
4. **可组合**：支持 `MINE_TRY` 宏、`value_or` 等便捷操作
5. **跨边界安全**：POD 布局,ABI 稳定

在设计返回可能失败的 API 时,优先使用 `Result<T>` 而非裸返回值 + 全局错误状态,这将显著提升代码的健壮性和可维护性。
