# Memory 模块详细接口文档（OwnedPtr / MINE_NEW / MINE_DELETE）

## 概述

**模块**：`mine.core`  
**头文件**：`<mine/core/Memory.h>`  
**命名空间**：`mine::core`

**用途**：ABI 安全的内存管理工具，包含独占所有权智能指针 `OwnedPtr<T>`、工厂函数 `make_owned<T>()` 和全局分配宏 `MINE_NEW`/`MINE_DELETE`。

**核心特性**：
- `OwnedPtr<T>`：删除器以函数指针编码（非模板参数），跨 DLL 边界安全
- `make_owned<T>()`：类似 `std::make_unique`，使用统一分配器
- `MINE_NEW` / `MINE_DELETE`：全局分配宏，确保走统一分配器路径
- 支持协变指针转换（`OwnedPtr<Derived>` → `OwnedPtr<Base>`）
- 不使用 C++ 异常（分配失败断言终止）

**与 `std::unique_ptr` 的关键区别**：
- 删除器为运行时函数指针（非类型的一部分）
- 跨 DLL 边界传递时析构在创建侧发生（ABI 安全）
- 无法自定义删除器类型（仅支持默认分配器）

---

## OwnedPtr 类定义

```cpp
namespace mine::core {

template<typename T>
class OwnedPtr {
public:
    using element_type = T;
    using deleter_type = void (*)(void*) noexcept;
    
    // 构造
    constexpr OwnedPtr() noexcept = default;
    constexpr OwnedPtr(std::nullptr_t) noexcept;
    explicit OwnedPtr(T* ptr, deleter_type deleter) noexcept;
    
    // 析构
    ~OwnedPtr() noexcept;
    
    // 移动语义
    OwnedPtr(OwnedPtr&& other) noexcept;
    OwnedPtr& operator=(OwnedPtr&& other) noexcept;
    
    // 协变转换
    template<typename U> requires std::is_convertible_v<U*, T*>
    OwnedPtr(OwnedPtr<U>&& other) noexcept;
    
    template<typename U> requires std::is_convertible_v<U*, T*>
    OwnedPtr& operator=(OwnedPtr<U>&& other) noexcept;
    
    // 禁用拷贝
    OwnedPtr(const OwnedPtr&) = delete;
    OwnedPtr& operator=(const OwnedPtr&) = delete;
    
    // 赋值 nullptr
    OwnedPtr& operator=(std::nullptr_t) noexcept;
    
    // 访问
    [[nodiscard]] T* get() const noexcept;
    [[nodiscard]] T* operator->() const noexcept;
    [[nodiscard]] T& operator*() const noexcept;
    [[nodiscard]] explicit operator bool() const noexcept;
    
    // 所有权转移
    [[nodiscard]] T* release() noexcept;
    void reset() noexcept;
    void reset(T* ptr, deleter_type deleter) noexcept;
    
    // 获取删除器
    [[nodiscard]] deleter_type get_deleter() const noexcept;
};

// 比较运算符
template<typename T, typename U>
bool operator==(const OwnedPtr<T>& a, const OwnedPtr<U>& b) noexcept;

template<typename T>
bool operator==(const OwnedPtr<T>& p, std::nullptr_t) noexcept;

template<typename T>
bool operator!=(const OwnedPtr<T>& p, std::nullptr_t) noexcept;

} // namespace mine::core
```

---

## 构造函数

### 默认构造

**签名**：
```cpp
constexpr OwnedPtr() noexcept = default;
constexpr OwnedPtr(std::nullptr_t) noexcept;
```

**描述**：构造空指针（不持有任何对象）。

**后置条件**：
- `get() == nullptr`
- `operator bool()` 返回 `false`

**示例**：
```cpp
OwnedPtr<Button> ptr1;
OwnedPtr<Button> ptr2 = nullptr;
assert(!ptr1);
assert(!ptr2);
```

---

### 从裸指针+删除器构造

**签名**：
```cpp
explicit OwnedPtr(T* ptr, deleter_type deleter) noexcept;
```

**描述**：从裸指针和删除器函数指针构造（通常不直接使用，而是通过 `make_owned` 工厂函数）。

**参数**：
- `ptr`：指向堆上已构造对象的指针
- `deleter`：删除器函数指针（析构+释放内存）

**前置条件**：
- `ptr == nullptr || deleter != nullptr`（否则触发断言）
- `ptr` 指向的对象必须由 `deleter` 对应的分配器分配

**后置条件**：
- `get() == ptr`
- `get_deleter() == deleter`

**示例**：
```cpp
// ⚠️ 不推荐直接使用此构造函数
Button* raw = MINE_NEW(Button, parent, "OK");
OwnedPtr<Button> ptr{raw, &detail::typed_deleter<Button>};

// ✅ 推荐使用工厂函数
OwnedPtr<Button> ptr = make_owned<Button>(parent, "OK");
```

---

### 移动构造

**签名**：
```cpp
OwnedPtr(OwnedPtr&& other) noexcept;
```

**描述**：从另一个 `OwnedPtr` 移动构造，转移所有权。

**参数**：
- `other`：待移动的源指针

**后置条件**：
- `*this` 持有 `other` 原指针
- `other.get() == nullptr`（源指针清空）

**时间复杂度**：O(1)

**示例**：
```cpp
OwnedPtr<Button> ptr1 = make_owned<Button>(parent, "OK");
OwnedPtr<Button> ptr2 = std::move(ptr1);
assert(!ptr1);  // ptr1 已清空
assert(ptr2);   // ptr2 持有对象
```

---

### 协变转换构造

**签名**：
```cpp
template<typename U>
    requires std::is_convertible_v<U*, T*>
OwnedPtr(OwnedPtr<U>&& other) noexcept;
```

**描述**：从 `OwnedPtr<Derived>` 移动构造 `OwnedPtr<Base>`（支持多态）。

**模板参数**：
- `U`：源类型（须为 `T` 的派生类或可转换类型）

**约束**：
- `U*` 必须可隐式转换为 `T*`（通常为继承关系）

**后置条件**：
- `*this` 持有 `other` 原指针（类型转换为 `T*`）
- `other.get() == nullptr`

**示例**：
```cpp
class Button : public Control {};

OwnedPtr<Button> btn = make_owned<Button>();
OwnedPtr<Control> ctrl = std::move(btn);  // ✅ 协变转换
assert(ctrl);
assert(!btn);

// OwnedPtr<Button> btn2 = std::move(ctrl);  // ❌ 编译错误：不能向下转型
```

---

## 析构函数

**签名**：
```cpp
~OwnedPtr() noexcept;
```

**描述**：析构 `OwnedPtr`，若持有对象则调用删除器（析构对象+释放内存）。

**行为**：
- 等价于调用 `reset()`（见下文）

**时间复杂度**：O(1) + 对象析构时间

**示例**：
```cpp
{
    OwnedPtr<Button> ptr = make_owned<Button>();
    // ...
}  // ptr 超出作用域，自动析构 Button 对象
```

---

## 赋值运算符

### 移动赋值

**签名**：
```cpp
OwnedPtr& operator=(OwnedPtr&& other) noexcept;
```

**描述**：从另一个 `OwnedPtr` 移动赋值。

**行为**：
1. 若 `this != &other`，先析构当前持有的对象（`reset()`）
2. 转移 `other` 的所有权
3. `other` 清空

**自赋值安全**：是（显式检查 `this != &other`）

**示例**：
```cpp
OwnedPtr<Button> ptr1 = make_owned<Button>();
OwnedPtr<Button> ptr2 = make_owned<Button>();
ptr1 = std::move(ptr2);  // ptr1 原对象析构，ptr2 对象转移到 ptr1
assert(ptr1);
assert(!ptr2);
```

---

### 协变移动赋值

**签名**：
```cpp
template<typename U>
    requires std::is_convertible_v<U*, T*>
OwnedPtr& operator=(OwnedPtr<U>&& other) noexcept;
```

**描述**：从 `OwnedPtr<Derived>` 移动赋值到 `OwnedPtr<Base>`。

**示例**：
```cpp
OwnedPtr<Control> ctrl = make_owned<Control>();
OwnedPtr<Button> btn = make_owned<Button>();
ctrl = std::move(btn);  // ✅ 协变赋值
```

---

### 赋值 nullptr

**签名**：
```cpp
OwnedPtr& operator=(std::nullptr_t) noexcept;
```

**描述**：析构当前持有的对象，变为空指针。

**行为**：
- 等价于调用 `reset()`

**示例**：
```cpp
OwnedPtr<Button> ptr = make_owned<Button>();
ptr = nullptr;  // Button 对象被析构
assert(!ptr);
```

---

## 访问方法

### `get()`

**签名**：
```cpp
[[nodiscard]] T* get() const noexcept;
```

**描述**：返回裸指针（不转移所有权）。

**返回值**：
- 指向持有对象的指针（可为 `nullptr`）

**用途**：
- 传递给需要裸指针的 API
- 检查指针是否有效

**时间复杂度**：O(1)

**示例**：
```cpp
OwnedPtr<Button> ptr = make_owned<Button>();
Button* raw = ptr.get();
if (raw) {
    raw->set_text("OK");
}
```

---

### `operator->()`

**签名**：
```cpp
[[nodiscard]] T* operator->() const noexcept;
```

**描述**：访问持有对象的成员（智能指针语法）。

**前置条件**：
- `get() != nullptr`（否则触发断言）

**返回值**：
- 等价于 `get()`

**示例**：
```cpp
OwnedPtr<Button> ptr = make_owned<Button>();
ptr->set_text("OK");  // 等价于 ptr.get()->set_text("OK")

OwnedPtr<Button> null_ptr;
// null_ptr->set_text("OK");  // 断言失败：nullptr 访问
```

---

### `operator*()`

**签名**：
```cpp
[[nodiscard]] T& operator*() const noexcept;
```

**描述**：解引用持有对象。

**前置条件**：
- `get() != nullptr`（否则触发断言）

**返回值**：
- 对持有对象的引用

**示例**：
```cpp
OwnedPtr<Button> ptr = make_owned<Button>();
Button& btn = *ptr;
btn.set_text("OK");
```

---

### `operator bool()`

**签名**：
```cpp
[[nodiscard]] explicit operator bool() const noexcept;
```

**描述**：检查指针是否有效（非空）。

**返回值**：
- `true`：`get() != nullptr`
- `false`：`get() == nullptr`

**示例**：
```cpp
OwnedPtr<Button> ptr = make_owned<Button>();
if (ptr) {  // 等价于 if (ptr.get() != nullptr)
    ptr->set_text("OK");
}
```

---

## 所有权转移方法

### `release()`

**签名**：
```cpp
[[nodiscard]] T* release() noexcept;
```

**描述**：放弃所有权，返回裸指针（调用方负责后续析构）。

**返回值**：
- 裸指针（原持有对象）

**后置条件**：
- `get() == nullptr`
- 调用方必须手动调用 `MINE_DELETE` 或通过其他方式管理内存

**⚠️ 警告**：
- 容易导致内存泄漏（忘记释放）
- 仅在需要将所有权转移给 C API 时使用

**示例**：
```cpp
OwnedPtr<Button> ptr = make_owned<Button>();
Button* raw = ptr.release();  // 所有权转移到 raw
assert(!ptr);  // ptr 已清空

// 调用方负责释放
MINE_DELETE(raw);
```

---

### `reset()`

**签名**：
```cpp
void reset() noexcept;
```

**描述**：析构当前持有的对象（若有），变为空指针。

**行为**：
- 若持有对象：调用删除器（析构+释放内存）
- 若为空：无操作

**后置条件**：
- `get() == nullptr`

**时间复杂度**：O(1) + 对象析构时间

**示例**：
```cpp
OwnedPtr<Button> ptr = make_owned<Button>();
assert(ptr);

ptr.reset();
assert(!ptr);  // Button 对象已析构
```

---

### `reset(T*, deleter)`

**签名**：
```cpp
void reset(T* ptr, deleter_type deleter) noexcept;
```

**描述**：重置为新的指针+删除器组合。

**参数**：
- `ptr`：新的裸指针
- `deleter`：新的删除器函数指针

**行为**：
1. 先析构当前持有的对象（调用 `reset()`）
2. 接管新指针的所有权

**⚠️ 警告**：
- 不推荐直接使用，易出错
- 优先使用 `make_owned` 或赋值

**示例**：
```cpp
OwnedPtr<Button> ptr = make_owned<Button>();

Button* new_btn = MINE_NEW(Button, parent, "Cancel");
ptr.reset(new_btn, &detail::typed_deleter<Button>);
// 原 Button 对象析构，ptr 现在持有 new_btn
```

---

## 其他方法

### `get_deleter()`

**签名**：
```cpp
[[nodiscard]] deleter_type get_deleter() const noexcept;
```

**描述**：返回删除器函数指针。

**返回值**：
- 删除器函数指针（可为 `nullptr`，若 `OwnedPtr` 为空）

**用途**：
- 内部调试
- 协变转换时传递删除器

**示例**：
```cpp
OwnedPtr<Button> ptr = make_owned<Button>();
auto deleter = ptr.get_deleter();
assert(deleter != nullptr);
```

---

## 比较运算符

### `operator==` / `operator!=`

**签名**：
```cpp
template<typename T, typename U>
bool operator==(const OwnedPtr<T>& a, const OwnedPtr<U>& b) noexcept;

template<typename T>
bool operator==(const OwnedPtr<T>& p, std::nullptr_t) noexcept;

template<typename T>
bool operator!=(const OwnedPtr<T>& p, std::nullptr_t) noexcept;
```

**描述**：比较两个 `OwnedPtr` 或与 `nullptr` 比较。

**行为**：
- 比较内部裸指针值（非对象内容）

**示例**：
```cpp
OwnedPtr<Button> ptr1 = make_owned<Button>();
OwnedPtr<Button> ptr2 = make_owned<Button>();
OwnedPtr<Button> ptr3;

assert(ptr1 != ptr2);  // 不同对象
assert(ptr1 != nullptr);
assert(ptr3 == nullptr);
```

---

## 工厂函数

### `make_owned<T>()`

**签名**：
```cpp
template<typename T, typename... Args>
    requires std::is_constructible_v<T, Args...>
[[nodiscard]] OwnedPtr<T> make_owned(Args&&... args) noexcept;
```

**描述**：通过默认分配器创建 `OwnedPtr<T>`，转发构造参数。

**模板参数**：
- `T`：目标类型（须为具体类型，可构造）
- `Args...`：构造参数类型包

**参数**：
- `args...`：转发给 `T` 构造函数的参数

**返回值**：
- `OwnedPtr<T>` 持有新创建的对象

**约束**：
- `T` 必须可从 `Args...` 构造

**分配失败**：
- 触发 `MINE_CHECK` 断言终止（不抛异常）

**时间复杂度**：O(1) + 对象构造时间

**示例**：
```cpp
// 基本类型
OwnedPtr<int> ptr_int = make_owned<int>(42);
assert(*ptr_int == 42);

// 复杂对象
OwnedPtr<Button> btn = make_owned<Button>(parent, "OK");
btn->set_text("确定");

// 无参构造
OwnedPtr<Button> btn2 = make_owned<Button>();
```

---

## 全局分配宏

### `MINE_NEW(T, ...)`

**签名**：
```cpp
#define MINE_NEW(T, ...) \
    (::mine::core::detail::construct_new<T>(::mine::core::default_allocator(), ##__VA_ARGS__))
```

**描述**：通过默认分配器分配并构造类型 `T` 的对象，返回 `T*`。

**参数**：
- `T`：目标类型
- `...`：构造参数（可选）

**返回值**：
- `T*` 裸指针（所有权由调用方管理）

**⚠️ 警告**：
- 返回裸指针，须手动调用 `MINE_DELETE` 释放
- 推荐使用 `make_owned` 代替（RAII 管理）

**示例**：
```cpp
// 手动管理内存
Button* btn = MINE_NEW(Button, parent, "OK");
btn->set_text("确定");
MINE_DELETE(btn);

// ✅ 推荐：RAII 管理
OwnedPtr<Button> btn = make_owned<Button>(parent, "OK");
// 自动析构，无需手动释放
```

---

### `MINE_DELETE(ptr)`

**签名**：
```cpp
#define MINE_DELETE(ptr) \
    (::mine::core::detail::destroy_delete(::mine::core::default_allocator(), (ptr)))
```

**描述**：调用析构函数并通过默认分配器释放由 `MINE_NEW` 分配的对象。

**参数**：
- `ptr`：类型化指针（须为 `MINE_NEW` 分配的对象）

**前置条件**：
- `ptr` 为 `MINE_NEW` 分配的对象，或 `nullptr`（安全）
- `ptr` 不可为野指针

**行为**：
- 对 `nullptr` 调用是安全的（无操作）

**示例**：
```cpp
Button* btn = MINE_NEW(Button);
MINE_DELETE(btn);

MINE_DELETE(nullptr);  // ✅ 安全：无操作
```

---

## 使用场景

### 1. PIMPL 模式

```cpp
// Visual.h（公共头）
class MINE_API Visual {
public:
    Visual();
    ~Visual();
    void invalidate();
    
private:
    struct Impl;
    OwnedPtr<Impl> pimpl_;  // ✅ ABI 安全：删除器编码在运行时
};

// Visual.cpp（实现文件）
#include <vector>
struct Visual::Impl {
    std::vector<Child*> children;
    bool dirty{true};
};

Visual::Visual() : pimpl_{make_owned<Impl>()} {}
Visual::~Visual() = default;  // OwnedPtr 自动析构
```

### 2. 跨 DLL 边界传递

```cpp
// DLL 导出函数
MINE_API OwnedPtr<IDevice> create_device() {
    return make_owned<D3D11Device>();  // ✅ 删除器在 DLL 内注册
}

// 调用侧（不同编译单元）
OwnedPtr<IDevice> device = create_device();
// device 析构时，调用 DLL 内的删除器 → ABI 安全
```

### 3. 工厂模式

```cpp
class ControlFactory {
public:
    static OwnedPtr<Control> create(ControlType type) {
        switch (type) {
        case ControlType::Button:
            return make_owned<Button>();
        case ControlType::TextBox:
            return make_owned<TextBox>();
        default:
            return nullptr;
        }
    }
};

OwnedPtr<Control> ctrl = ControlFactory::create(ControlType::Button);
if (ctrl) {
    ctrl->set_visibility(Visibility::Visible);
}
```

### 4. 资源管理

```cpp
class TextureManager {
private:
    HashMap<StringView, OwnedPtr<Texture>> textures_;
    
public:
    Texture* load(StringView path) {
        auto it = textures_.find(path);
        if (it != textures_.end()) {
            return it->second.get();
        }
        
        auto tex = make_owned<Texture>(path);
        Texture* ptr = tex.get();
        textures_[path] = std::move(tex);
        return ptr;
    }
    
    void unload(StringView path) {
        textures_.erase(path);  // OwnedPtr 析构 → Texture 释放
    }
};
```

---

## 性能特征

| 操作 | 时间复杂度 | 空间复杂度 |
|------|------------|------------|
| 默认构造 | O(1) | O(1) |
| `make_owned<T>()` | O(1) + T 构造 | sizeof(T) |
| 移动构造/赋值 | O(1) | O(1) |
| 析构 | O(1) + T 析构 | O(1) |
| `get()` / `operator->` | O(1) | O(1) |
| `reset()` | O(1) + T 析构 | O(1) |
| 协变转换 | O(1) | O(1) |

**内存占用**：
- `sizeof(OwnedPtr<T>) = 16` 字节（64 位：指针 8 字节 + 删除器 8 字节）
- 比 `std::unique_ptr<T, std::function<void(T*)>>` 更轻量（后者约 32 字节）

---

## 线程安全

**非线程安全**：
- `OwnedPtr` 对象本身无内部锁
- 多线程同时访问同一 `OwnedPtr` 需外部同步

**线程安全操作**（多线程只读）：
- `get()` / `operator->` / `operator*`：只读访问，无竞争（前提：不修改 `OwnedPtr` 本身）

**需要同步的操作**：
- 任何修改操作（赋值、`reset()`、析构）与其他操作并发执行

---

## 限制与注意事项

### 不支持自定义删除器类型

**问题**：`OwnedPtr` 仅支持默认分配器，无法自定义删除器。

```cpp
// ❌ 不支持：自定义删除器
// OwnedPtr<FILE, file_closer> file{fopen("a.txt", "r"), file_closer{}};
```

**原因**：
- 删除器类型编码为运行时函数指针（非模板参数）
- 保证跨 DLL 边界 ABI 兼容

**替代方案**：
- 使用 `std::unique_ptr` + 自定义删除器（内部使用）
- 或包装为 RAII 类

### 不可拷贝

**问题**：`OwnedPtr` 是独占所有权智能指针，无法拷贝。

```cpp
OwnedPtr<Button> ptr1 = make_owned<Button>();
// OwnedPtr<Button> ptr2 = ptr1;  // ❌ 编译错误：拷贝被删除
```

**解决方案**：
- 若需共享所有权，使用 `std::shared_ptr`（但需注意 ABI 问题）
- 或传递裸指针 `ptr1.get()`（不转移所有权）

### 协变转换限制

**问题**：仅支持向上转型（派生→基类），不支持向下转型。

```cpp
OwnedPtr<Button> btn = make_owned<Button>();
OwnedPtr<Control> ctrl = std::move(btn);  // ✅ 向上转型

// OwnedPtr<Button> btn2 = std::move(ctrl);  // ❌ 编译错误：向下转型
```

**解决方案**：
- 需要向下转型时，先 `release()` 再手动转型（不安全）
- 或使用 `dynamic_cast` + `std::unique_ptr`（若启用 RTTI）

---

## 调试技巧

### 检查指针有效性

```cpp
OwnedPtr<Button> ptr = make_owned<Button>();
assert(ptr);  // 检查非空
assert(ptr.get() != nullptr);
```

### 打印指针地址

```cpp
printf("OwnedPtr: %p\n", ptr.get());
```

### 断点条件

```cpp
// Visual Studio / GDB 断点条件
ptr.get() != nullptr
ptr.get() == (Button*)0x12345678
```

### 内存泄漏检测

```cpp
// 在分配器中加入追踪逻辑
class TrackingAllocator : public IAllocator {
    void* alloc(size_t bytes, size_t align) noexcept override {
        void* mem = malloc(bytes);
        printf("Alloc: %p (%zu bytes)\n", mem, bytes);
        return mem;
    }
    void dealloc(void* ptr, size_t bytes, size_t align) noexcept override {
        printf("Dealloc: %p (%zu bytes)\n", ptr, bytes);
        free(ptr);
    }
};

// 启动时替换默认分配器
set_default_allocator(&tracking_allocator);
```

---

## 示例：完整用法

```cpp
#include <mine/core/Memory.h>
#include <cassert>

using namespace mine;

class Button {
public:
    explicit Button(const char* text = "") : text_{text} {
        printf("Button(\"%s\")\n", text_);
    }
    ~Button() {
        printf("~Button(\"%s\")\n", text_);
    }
    void set_text(const char* text) { text_ = text; }
    
private:
    const char* text_;
};

void example_basic() {
    // 创建与销毁
    core::OwnedPtr<Button> btn = core::make_owned<Button>("OK");
    btn->set_text("确定");
    
    // 超出作用域自动析构
}  // 输出：~Button("确定")

void example_ownership_transfer() {
    core::OwnedPtr<Button> ptr1 = core::make_owned<Button>("A");
    core::OwnedPtr<Button> ptr2 = std::move(ptr1);
    
    assert(!ptr1);  // ptr1 已清空
    assert(ptr2);   // ptr2 持有对象
}

void example_reset() {
    core::OwnedPtr<Button> ptr = core::make_owned<Button>("Old");
    assert(ptr);
    
    ptr.reset();  // 析构旧对象
    assert(!ptr);
    
    ptr = core::make_owned<Button>("New");  // 创建新对象
    assert(ptr);
}

void example_release() {
    core::OwnedPtr<Button> ptr = core::make_owned<Button>("Temp");
    Button* raw = ptr.release();  // 所有权转移
    
    assert(!ptr);
    assert(raw != nullptr);
    
    // 手动释放
    MINE_DELETE(raw);
}

class Control {};
class Button : public Control {};

void example_covariance() {
    core::OwnedPtr<Button> btn = core::make_owned<Button>();
    core::OwnedPtr<Control> ctrl = std::move(btn);  // ✅ 协变转换
    
    assert(ctrl);
    assert(!btn);
}

int main() {
    example_basic();
    example_ownership_transfer();
    example_reset();
    example_release();
    example_covariance();
    return 0;
}
```

---

## 相关类型

- `mine::core::Pimpl<T>`：PIMPL 模式辅助类，内部使用 `OwnedPtr`
- `mine::core::IAllocator`：分配器接口，`OwnedPtr` 通过默认分配器管理内存
- `std::unique_ptr<T>`：标准库独占指针，但删除器类型为模板参数（不 ABI 安全）

---

## 最佳实践

### ✅ 推荐用法

```cpp
// 1. 使用 make_owned 创建对象
OwnedPtr<Button> btn = make_owned<Button>();

// 2. PIMPL 模式
class Widget {
private:
    struct Impl;
    OwnedPtr<Impl> pimpl_;
};

// 3. 工厂函数返回值
OwnedPtr<IDevice> create_device();

// 4. 协变转换（向上转型）
OwnedPtr<Control> ctrl = std::move(btn);
```

### ❌ 避免的用法

```cpp
// ❌ 直接使用 MINE_NEW（易忘记释放）
Button* btn = MINE_NEW(Button);
// ... 忘记 MINE_DELETE(btn) → 内存泄漏

// ❌ 手动构造 OwnedPtr（易出错）
Button* raw = MINE_NEW(Button);
OwnedPtr<Button> ptr{raw, &detail::typed_deleter<Button>};

// ❌ release() 后忘记释放
Button* raw = ptr.release();
// ... 忘记 MINE_DELETE(raw) → 内存泄漏

// ❌ 多个 OwnedPtr 持有同一指针
Button* raw = MINE_NEW(Button);
OwnedPtr<Button> ptr1{raw, ...};
OwnedPtr<Button> ptr2{raw, ...};  // ❌ 重复释放
```

---

## 与 std::unique_ptr 对比

| 特性 | OwnedPtr | std::unique_ptr |
|------|----------|-----------------|
| ABI 安全（跨 DLL） | ✅ 是 | ❌ 否（删除器类型影响 ABI） |
| 自定义删除器 | ❌ 否 | ✅ 是（模板参数） |
| 大小 | 16 字节 | 8-32 字节（取决于删除器） |
| 性能 | 等价 | 等价 |
| 协变转换 | ✅ 支持 | ✅ 支持 |
| 标准库兼容 | ❌ 否 | ✅ 是 |

**何时使用 `OwnedPtr`**：
- 跨 DLL 边界传递智能指针
- 公共 API 返回智能指针
- PIMPL 模式

**何时使用 `std::unique_ptr`**：
- 内部实现（不跨 DLL 边界）
- 需要自定义删除器（如 FILE*、HANDLE）
- 与标准库算法配合

---

## ABI 安全原理

### std::unique_ptr 的 ABI 问题

```cpp
// DLL 内部（mylib.dll）
std::unique_ptr<Button> create_button() {
    return std::make_unique<Button>();
}

// 调用侧（app.exe，不同编译器/标准库版本）
auto btn = create_button();
// btn 析构时调用 app.exe 的删除器 → 在 app.exe 的堆上释放 mylib.dll 分配的内存
// 若两侧堆分配器不同 → 崩溃
```

### OwnedPtr 的 ABI 安全

```cpp
// DLL 内部（mylib.dll）
OwnedPtr<Button> create_button() {
    return make_owned<Button>();  // 删除器 = mylib.dll 内的 typed_deleter<Button>
}

// 调用侧（app.exe）
auto btn = create_button();
// btn 析构时调用 mylib.dll 的删除器 → 在 mylib.dll 的堆上释放
// ✅ 安全：析构在分配侧发生
```

**核心差异**：
- `std::unique_ptr`：删除器类型是模板参数 → 在调用侧实例化
- `OwnedPtr`：删除器类型是函数指针 → 在定义侧注册，运行时调用
