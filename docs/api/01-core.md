# mine.core —— 基础类型与工具模块

## 模块概述

`mine.core` 是 MineFramework 最底层的模块，提供所有其他模块共同依赖的基础类型、内存管理和工具设施。该模块设计上不依赖 RTTI（`/GR-`），不使用 C++ 异常，不引入 STL 容器到公共头文件。

| 特性 | 说明 |
|------|------|
| 无 RTTI | 所有类型鉴别使用 `TypeId`（编译期地址）替代 `std::type_info` |
| 无异常 | 错误通过 `Status` / `Result<T,E>` 返回值显式传递 |
| ABI 安全 | 智能指针 `OwnedPtr<T>` 删除器为函数指针，可安全跨 DLL 传递 |
| 无 STL 泄露 | 公共头不暴露 `std::vector`、`std::string` 等类型 |

**伞形头文件**：`<mine/core/Core.h>` 一次性引入所有公共类型。

---

## 1. 类型标识 —— `TypeId`

**文件**：`<mine/core/TypeId.h>`

不依赖 RTTI 的编译期类型标识符。利用 C++17 `inline` 变量地址唯一性，以 `&kTypeAnchor<T>` 的地址作为类型唯一 ID。

### 类摘要

```cpp
namespace mine::core {

class TypeId {
public:
    constexpr TypeId() noexcept;                          // 构造无效 TypeId

    // 工厂
    template<typename T>
    static constexpr TypeId of() noexcept;                // 获取类型 T 的 TypeId（剥除 cv/引用）

    // 查询
    constexpr bool valid() const noexcept;                // 是否有效
    uintptr_t       hash()  const noexcept;               // 哈希值（可用于 HashMap 键）

    // 比较
    constexpr bool operator==(TypeId) const noexcept;
    constexpr bool operator!=(TypeId) const noexcept;
    bool           operator< (TypeId) const noexcept;     // 有序比较
};

} // namespace mine::core
```

### 使用示例

```cpp
TypeId buttonId = TypeId::of<Button>();
TypeId sameId   = TypeId::of<const Button>();  // cv 限定符被剥除，相同

if (element->type_id() == TypeId::of<Button>()) {
    auto* btn = static_cast<Button*>(element);
}
```

### 约束

- `TypeId` 在单个可执行文件或 DLL 内唯一且稳定。
- 跨 DLL 比较需显式注册（见 `mine.reflect` 模块）。
- 默认构造的 `TypeId{}` 为无效标识符（`valid() == false`）。

---

## 2. 字符串视图 —— `StringView`

**文件**：`<mine/core/StringView.h>`

非拥有的 UTF-8 字符串视图。仅持有指针和长度，不分配堆内存。

### 类摘要

```cpp
namespace mine::core {

class StringView {
public:
    using size_type  = size_t;
    using value_type = char;

    constexpr StringView() noexcept;                            // 空视图
    StringView(const char* str) noexcept;                       // 从 C 字符串（strlen 计算长度）
    constexpr StringView(const char* str, size_type len) noexcept; // 指针+长度

    // 容量
    constexpr const char* data()  const noexcept;
    constexpr size_type   size()  const noexcept;
    constexpr bool        empty() const noexcept;

    // 元素访问
    constexpr char operator[](size_type i) const noexcept;
    constexpr char front() const noexcept;
    constexpr char back()  const noexcept;

    // 迭代器
    constexpr const char* begin() const noexcept;
    constexpr const char* end()   const noexcept;

    // 比较
    constexpr bool operator==(StringView) const noexcept;
    constexpr bool operator!=(StringView) const noexcept;
};

} // namespace mine::core
```

### 使用示例

```cpp
StringView sv{"hello"};          // strlen("hello") = 5
StringView sv2{ptr, 10};         // 显式长度，内容可含 '\0'
if (!sv.empty()) {
    char first = sv.front();     // 'h'
}
```

---

## 3. 类型擦除容器 —— `Variant`

**文件**：`<mine/core/Variant.h>`

无 RTTI、无异常的类 `std::any` 容器。使用 `TypeId` 进行类型鉴别，支持小对象优化（≤16 字节且 noexcept 移动的类型直接栈上存储）。

### 类摘要

```cpp
namespace mine::core {

class Variant {
public:
    Variant() noexcept;                                             // 空 Variant
    template<typename T> Variant(T&& value);                        // 从值构造

    // 查询
    TypeId type_id() const noexcept;                                // 持有值的 TypeId
    template<typename T> bool has() const noexcept;                 // 是否持有 T 类型
    bool empty() const noexcept;                                    // 是否为空

    // 取值
    template<typename T> T&              get() &;
    template<typename T> const T&        get() const&;
    template<typename T> T&&             get() &&;

    // 赋值
    template<typename T> void set_value(T&& value);
    void reset() noexcept;
};

// 自由函数
template<typename T>
T any_cast(const Variant& v);                                       // 类型不匹配时断言

template<typename T>
T any_cast(Variant& v);

} // namespace mine::core
```

### 使用示例

```cpp
Variant v = 42;                    // 存储 int
Variant v2 = 3.14f;                // 存储 float
Variant v3 = StringView{"hello"};  // StringView 转为内部存储

if (v.has<int>()) {
    int x = v.get<int>();          // 42
}
int x = any_cast<int>(v);          // 类型不匹配时触发断言
```

### SBO 规则

| 条件 | 存储方式 |
|------|---------|
| `sizeof(T) <= 16 && alignof(T) <= alignof(max_align_t) && noexcept 移动` | 栈上 SBO |
| 不满足 SBO 条件 | 堆分配（通过 `default_allocator()`） |

---

## 4. 内存视图 —— `Span`

**文件**：`<mine/core/Span.h>`

非拥有的连续内存视图（类 `std::span`）。持有指针和元素个数，支持迭代和子视图切片。

### 类摘要

```cpp
namespace mine::core {

template<typename T>
class Span {
public:
    constexpr Span() noexcept;
    constexpr Span(T* ptr, size_t count) noexcept;
    template<size_t N> constexpr Span(T (&arr)[N]) noexcept;

    // 容量
    constexpr size_t size()       const noexcept;
    constexpr bool   empty()      const noexcept;

    // 元素访问
    constexpr T& operator[](size_t i) const noexcept;
    constexpr T& front()              const noexcept;
    constexpr T& back()               const noexcept;
    constexpr T* data()               const noexcept;

    // 子视图
    constexpr Span<T> subspan(size_t offset, size_t count) const noexcept;
    constexpr Span<T> first(size_t count) const noexcept;
    constexpr Span<T> last(size_t count)  const noexcept;

    // 迭代器
    constexpr T* begin() const noexcept;
    constexpr T* end()   const noexcept;
};

} // namespace mine::core
```

---

## 5. 操作状态 —— `Status` 与 `Result<T,E>`

**文件**：`<mine/core/Status.h>`、`<mine/core/Result.h>`

无异常的显式错误传递机制。

### 5.1 Status

```cpp
namespace mine::core {

enum class StatusCode : uint8_t {
    Ok              = 0,
    Err             = 1,   // 通用错误
    InvalidArg      = 2,   // 参数无效
    OutOfMemory     = 3,   // 内存不足
    Unsupported     = 4,   // 不支持的操作
    NotFound        = 5,   // 资源未找到
};

class Status {
public:
    Status() noexcept;                                    // StatusCode::Ok
    Status(StatusCode code) noexcept;

    bool           ok()   const noexcept;                 // code == Ok
    StatusCode     code() const noexcept;
    const char*    message() const noexcept;              // 错误描述（可选）
};

} // namespace mine::core
```

### 5.2 Result<T, E>

携带成功值或错误的判别联合（类 `std::expected`），`[[nodiscard]]` 强制调用方检查。

```cpp
namespace mine::core {

// 标签：消除成功/失败构造歧义
struct OkTag  { explicit OkTag() = default; };
struct ErrTag { explicit ErrTag() = default; };
inline constexpr OkTag  ok_tag{};
inline constexpr ErrTag err_tag{};

template<typename T, typename E = Status>
class [[nodiscard]] Result {
public:
    // 构造
    template<typename... Args>
    explicit Result(OkTag, Args&&... args) noexcept;        // 成功值
    template<typename... Args>
    explicit Result(ErrTag, Args&&... args) noexcept;       // 错误值

    // 查询
    bool ok()       const noexcept;
    bool has_error() const noexcept;

    // 取值（ok() == false 时调用触发断言）
    T&        value() &;
    const T&  value() const&;

    // 取错误
    E&        error() &;
    const E&  error() const&;
};

} // namespace mine::core
```

### 使用示例

```cpp
Result<int> divide(int a, int b) {
    if (b == 0)
        return {err_tag, Status{StatusCode::InvalidArg}};
    return {ok_tag, a / b};
}

auto r = divide(10, 2);
if (r.ok()) {
    use(r.value());   // 5
} else {
    log(r.error());
}
```

---

## 6. 内存管理

**文件**：`<mine/core/Allocator.h>`、`<mine/core/Memory.h>`、`<mine/core/Pimpl.h>`

### 6.1 分配器接口 — `IAllocator`

```cpp
namespace mine::core {

class IAllocator {
public:
    virtual ~IAllocator() = default;

    // 分配 / 释放
    virtual void* alloc(size_t size, size_t align)           = 0;
    virtual void  dealloc(void* ptr, size_t size, size_t align) = 0;

    // 带额外参数的分配（用于调试/追踪）
    virtual void* alloc_extra(size_t size, size_t align,
                              const char* file, int line)    = 0;
};

// 获取全局默认分配器
IAllocator* default_allocator() noexcept;

} // namespace mine::core
```

### 6.2 分配宏

```cpp
// 通过默认分配器创建对象（返回裸指针，不自动析构）
#define MINE_NEW(T, ...)    /*= mine::core::detail::construct_new<T>(alloc, __VA_ARGS__) */

// 通过默认分配器析构 + 释放对象
#define MINE_DELETE(ptr)    /*= mine::core::detail::destroy_delete(alloc, ptr) */
```

### 6.3 智能指针 — `OwnedPtr<T>`

ABI 安全的独占所有权智能指针，删除器为函数指针（非模板参数），可安全跨 DLL 传递。

```cpp
namespace mine::core {

template<typename T>
class OwnedPtr {
public:
    constexpr OwnedPtr() noexcept;
    constexpr OwnedPtr(std::nullptr_t) noexcept;
    explicit OwnedPtr(T* ptr, detail::DeleterFn deleter) noexcept;
    ~OwnedPtr() noexcept;

    // 移动（不可拷贝）
    OwnedPtr(OwnedPtr&&) noexcept;
    OwnedPtr& operator=(OwnedPtr&&) noexcept;

    // 访问
    T*       get()        noexcept;
    const T* get()        const noexcept;
    T&       operator*()  noexcept;
    T*       operator->() noexcept;
    explicit operator bool() const noexcept;

    // 释放 / 重置
    T*   release() noexcept;               // 放弃所有权，返回裸指针
    void reset()   noexcept;               // 析构并置空
};

// 工厂函数
template<typename T, typename... Args>
OwnedPtr<T> make_owned(Args&&... args) noexcept;

} // namespace mine::core
```

### 使用示例

```cpp
// 创建
auto btn = make_owned<Button>(parent, "OK");
btn->set_text("确定");

// 跨 DLL 传递（安全）
OwnedPtr<IDevice> create_device();  // DLL 导出函数

// 释放
OwnedPtr<Button> taken = std::move(btn);  // 转移所有权
taken.reset();                             // 或自动析构
```

---

## 7. PIMPL 惯用法 —— `Pimpl<T>`

**文件**：`<mine/core/Pimpl.h>`

跨 DLL 边界类的标准实现私有化工具。

### 类摘要

```cpp
namespace mine::core {

template<typename Impl>
class Pimpl {
public:
    constexpr Pimpl() noexcept;
    explicit Pimpl(OwnedPtr<Impl> ptr) noexcept;
    ~Pimpl() noexcept;                                          // 需 Impl 为完整类型

    // 移动（不可拷贝）
    Pimpl(Pimpl&&) noexcept;
    Pimpl& operator=(Pimpl&&) noexcept;

    // 访问
    Impl*       operator->() noexcept;
    const Impl* operator->() const noexcept;
    Impl&       operator*()  noexcept;
    const Impl& operator*()  const noexcept;
    Impl*       get()        noexcept;
    const Impl* get()        const noexcept;
    explicit operator bool() const noexcept;
    void reset() noexcept;
};

// 工厂函数
template<typename Impl, typename... Args>
Pimpl<Impl> make_pimpl(Args&&... args) noexcept;

} // namespace mine::core
```

### 使用示例

```cpp
// 头文件 (MyClass.h)
class MINE_API MyClass {
public:
    MyClass();
    ~MyClass();                     // = default（在 .cpp 中，Impl 已完整定义）
private:
    struct Impl;
    mine::core::Pimpl<Impl> p_;
};

// 实现文件 (MyClass.cpp)
struct MyClass::Impl {
    std::vector<int> data;          // 实现细节可随意引入 STL
};
MyClass::MyClass() : p_{mine::core::make_pimpl<MyClass::Impl>()} {}
MyClass::~MyClass() = default;
```

---

## 8. 函数包装器 —— `Function<R(Args...)>`

**文件**：`<mine/core/Function.h>`

移动专用 SBO 函数包装器，不允许拷贝、不抛异常。捕获列表固定 ≤ 32 字节（超出编译期报错）。

### 类摘要

```cpp
namespace mine::core {

template<typename R, typename... Args>
class Function<R(Args...)> {
public:
    Function() noexcept;                                    // 空函数
    template<typename F> Function(F&& f) noexcept;          // 从可调用对象构造

    // 移动（不可拷贝）
    Function(Function&&) noexcept;
    Function& operator=(Function&&) noexcept;

    // 调用
    R operator()(Args... args) const;

    // 查询
    explicit operator bool() const noexcept;                // 是否有绑定的可调用对象
};

} // namespace mine::core
```

### 使用示例

```cpp
int x = 42;
Function<int()> fn = [x]() noexcept { return x; };
int result = fn();  // 42

// 检查是否已绑定
if (fn) { /* ... */ }
```

---

## 9. 断言 —— `<mine/core/Assert.h>`

```cpp
// 调试断言（Debug 构建有效，Release 中可能被编译掉）
#define MINE_ASSERT(condition)
#define MINE_ASSERT_MSG(condition, message)

// 强制断言（所有构建模式都生效）
#define MINE_CHECK(condition)
#define MINE_CHECK_MSG(condition, message)
```

---

## 10. 模块标识 —— `<mine/core/ModuleTag.h>`

```cpp
namespace mine::core {
    inline constexpr const char* kModuleName = "mine.core";
}
```

---

## 相关模块

| 模块 | 依赖关系 |
|------|---------|
| `mine.containers` | 依赖 `mine.core`（`InlineString`、`SmallVector` 均在 core 之上构建） |
| `mine.math` | 依赖 `mine.core` |
| `mine.ui.property` | 依赖 `mine.core`（`DependencyObject` 基类即 `TypeId` / `Variant` 的消费者） |
