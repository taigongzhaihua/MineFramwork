# Pimpl 详细接口文档

## 概述

`Pimpl<T>` 是 MineFramework 中用于实现 PIMPL（Pointer to IMPLementation）惯用法的辅助类。PIMPL 模式是跨 DLL 边界类必须使用的设计模式,通过将实现细节隐藏在前向声明的类型中,确保 ABI 稳定性和编译时间优化。

### 核心特性

- **ABI 稳定**：公共头不暴露实现细节,修改实现不破坏二进制兼容性
- **编译时间优化**：公共头无需包含大量实现相关头文件
- **类型安全**：通过 `OwnedPtr` 管理实现对象生命周期
- **便捷访问**：`operator->` / `operator*` 提供直接访问
- **仅移动语义**：实现对象不可拷贝（通常也不应拷贝）

### 设计动机

跨 DLL 边界的类若直接暴露成员变量,会导致以下问题:

1. **ABI 不稳定**：修改成员变量顺序/类型会破坏二进制兼容性
2. **STL 不兼容**：不同编译器/版本的 STL 实现不兼容
3. **编译依赖**：公共头包含大量实现头文件,拖慢编译速度
4. **实现泄漏**：内部实现细节暴露给调用方

`Pimpl<T>` 通过以下方式解决:

- 公共头仅声明前向类型 `struct Impl;`
- 所有成员变量移至 `Impl` 中（在 `.cpp` 文件定义）
- 通过 `Pimpl<Impl>` 持有指向实现的指针
- 公共接口通过 `pimpl_->` 访问实现

---

## Pimpl 类

### 类定义

```cpp
template<typename Impl>
class Pimpl {
public:
    constexpr Pimpl() noexcept = default;
    explicit Pimpl(OwnedPtr<Impl> ptr) noexcept;
    ~Pimpl() noexcept = default;

    Pimpl(Pimpl&&) noexcept            = default;
    Pimpl& operator=(Pimpl&&) noexcept = default;

    Pimpl(const Pimpl&)            = delete;
    Pimpl& operator=(const Pimpl&) = delete;

    [[nodiscard]] Impl* operator->() noexcept;
    [[nodiscard]] const Impl* operator->() const noexcept;
    [[nodiscard]] Impl& operator*() noexcept;
    [[nodiscard]] const Impl& operator*() const noexcept;

    [[nodiscard]] Impl* get() noexcept;
    [[nodiscard]] const Impl* get() const noexcept;

    [[nodiscard]] explicit operator bool() const noexcept;

    void reset() noexcept;

private:
    OwnedPtr<Impl> ptr_;
};
```

### 构造函数

#### 默认构造

```cpp
constexpr Pimpl() noexcept = default;
```

**描述**：构造空的 `Pimpl`,`get()` 返回 `nullptr`。

**用途**：延迟初始化,需在 `.cpp` 中通过 `make_pimpl` 初始化。

**时间复杂度**：O(1)

**示例**：

```cpp
// Widget.h
class Widget {
    struct Impl;
    Pimpl<Impl> pimpl_;  // 默认构造为空
public:
    Widget();  // 在 .cpp 中初始化
};
```

---

#### 从 OwnedPtr 构造

```cpp
explicit Pimpl(OwnedPtr<Impl> ptr) noexcept;
```

**描述**：从已有 `OwnedPtr<Impl>` 构造（通常由 `make_pimpl` 产生）。

**参数**：
- `ptr`：指向 `Impl` 的拥有指针

**显式构造**：防止意外隐式转换。

**时间复杂度**：O(1)

**示例**：

```cpp
// Widget.cpp
Widget::Widget() : pimpl_{make_pimpl<Impl>()} {}
```

---

### 析构函数

```cpp
~Pimpl() noexcept = default;
```

**描述**：析构 `Pimpl`,自动析构并释放 `Impl`。

**约束**：`Impl` 必须在析构点为完整类型,否则链接错误。

**实现要点**：
- 析构函数必须在 `.cpp` 文件中定义（`Impl` 完整类型可见处）
- 若在头文件中内联定义,`Impl` 不完整会导致编译错误

**示例**：

```cpp
// Widget.h
class Widget {
    struct Impl;
    Pimpl<Impl> pimpl_;
public:
    ~Widget();  // 声明在头文件
};

// Widget.cpp
struct Widget::Impl {
    // ...
};

Widget::~Widget() = default;  // 定义在 .cpp（Impl 完整类型可见）
```

---

### 移动语义

```cpp
Pimpl(Pimpl&&) noexcept            = default;
Pimpl& operator=(Pimpl&&) noexcept = default;
```

**描述**：支持移动构造和移动赋值,转移实现对象所有权。

**时间复杂度**：O(1)

**示例**：

```cpp
Widget w1;
Widget w2 = std::move(w1);  // 移动构造
Widget w3;
w3 = std::move(w2);  // 移动赋值
```

---

### 访问运算符

#### operator->

```cpp
[[nodiscard]] Impl* operator->() noexcept;
[[nodiscard]] const Impl* operator->() const noexcept;
```

**描述**：返回指向 `Impl` 的指针,支持成员访问。

**返回值**：`Impl*`

**前置条件**：`Pimpl` 已初始化（`ptr_` 非空）

**断言**：Debug 模式下检查 `ptr_` 非空:

```
MINE_ASSERT_MSG(ptr_, "Pimpl 未初始化,请先调用 make_pimpl<Impl>()");
```

**时间复杂度**：O(1)

**示例**：

```cpp
// Widget.cpp
struct Widget::Impl {
    int value{0};
    void increment() { ++value; }
};

void Widget::do_something() {
    pimpl_->increment();  // 调用 Impl 方法
    int x = pimpl_->value;  // 访问 Impl 成员
}
```

---

#### operator*

```cpp
[[nodiscard]] Impl& operator*() noexcept;
[[nodiscard]] const Impl& operator*() const noexcept;
```

**描述**：返回 `Impl` 的引用,支持解引用访问。

**返回值**：`Impl&`

**前置条件**：`Pimpl` 已初始化

**断言**：Debug 模式下检查 `ptr_` 非空

**时间复杂度**：O(1)

**示例**：

```cpp
void Widget::copy_impl(const Widget& other) {
    *pimpl_ = *other.pimpl_;  // 拷贝 Impl 内容（需 Impl 支持拷贝）
}
```

---

### 其他方法

#### get

```cpp
[[nodiscard]] Impl* get() noexcept;
[[nodiscard]] const Impl* get() const noexcept;
```

**描述**：返回裸指针,允许检查是否为空。

**返回值**：`Impl*`（可能为 `nullptr`）

**时间复杂度**：O(1)

**示例**：

```cpp
void Widget::conditional_operation() {
    if (pimpl_.get()) {  // 检查是否已初始化
        pimpl_->do_work();
    }
}
```

---

#### operator bool

```cpp
[[nodiscard]] explicit operator bool() const noexcept;
```

**描述**：显式布尔转换,检查是否持有有效的 `Impl`。

**返回值**：
- `true`：已初始化
- `false`：未初始化（`nullptr`）

**时间复杂度**：O(1)

**示例**：

```cpp
if (pimpl_) {  // 显式布尔转换
    pimpl_->do_work();
}
```

---

#### reset

```cpp
void reset() noexcept;
```

**描述**：释放 `Impl`,`Pimpl` 变为空状态。

**时间复杂度**：O(1) + `Impl` 的析构开销

**示例**：

```cpp
void Widget::cleanup() {
    pimpl_.reset();  // 释放实现对象
}
```

---

## make_pimpl 工厂函数

```cpp
template<typename Impl, typename... Args>
    requires std::is_constructible_v<Impl, Args...>
[[nodiscard]] Pimpl<Impl> make_pimpl(Args&&... args) noexcept;
```

**描述**：在堆上构造 `Impl` 并返回封装它的 `Pimpl<Impl>`。

**模板参数**：
- `Impl`：实现类型
- `Args`：转发到 `Impl` 构造函数的参数包

**约束**：`Impl` 必须可从 `Args...` 构造

**返回值**：`Pimpl<Impl>`

**前置条件**：`Impl` 必须为完整类型（在 `.cpp` 文件中可见）

**时间复杂度**：O(1) + `Impl` 的构造开销

**实现**：

```cpp
template<typename Impl, typename... Args>
[[nodiscard]] Pimpl<Impl> make_pimpl(Args&&... args) noexcept {
    return Pimpl<Impl>{make_owned<Impl>(std::forward<Args>(args)...)};
}
```

**示例**：

```cpp
// Widget.cpp
struct Widget::Impl {
    int x;
    std::string name;
    Impl(int x_val, std::string name_val) : x{x_val}, name{std::move(name_val)} {}
};

Widget::Widget(int x, std::string name)
    : pimpl_{make_pimpl<Impl>(x, std::move(name))} {}
```

---

## 使用模式

### 基本模式

**头文件（`Widget.h`）**：

```cpp
#pragma once
#include <mine/core/Pimpl.h>

class MINE_API Widget {
public:
    Widget();
    ~Widget();  // 必须声明（在 .cpp 中定义）

    Widget(Widget&&) noexcept;
    Widget& operator=(Widget&&) noexcept;

    void do_work();
    int get_value() const;

private:
    struct Impl;  // 前向声明
    mine::core::Pimpl<Impl> pimpl_;
};
```

**实现文件（`Widget.cpp`）**：

```cpp
#include "Widget.h"
#include <vector>  // 实现细节可自由引入 STL
#include <string>

struct Widget::Impl {
    std::vector<int> data;
    std::string name;
    int value{0};

    void increment() { ++value; }
};

Widget::Widget() : pimpl_{mine::core::make_pimpl<Impl>()} {}
Widget::~Widget() = default;  // 必须在 .cpp 中定义

Widget::Widget(Widget&&) noexcept = default;
Widget& Widget::operator=(Widget&&) noexcept = default;

void Widget::do_work() {
    pimpl_->increment();
}

int Widget::get_value() const {
    return pimpl_->value;
}
```

---

### 带参数构造

```cpp
// Widget.h
class Widget {
    struct Impl;
    Pimpl<Impl> pimpl_;
public:
    Widget(int x, std::string name);
    ~Widget();
};

// Widget.cpp
struct Widget::Impl {
    int x;
    std::string name;
    Impl(int x_val, std::string name_val) : x{x_val}, name{std::move(name_val)} {}
};

Widget::Widget(int x, std::string name)
    : pimpl_{make_pimpl<Impl>(x, std::move(name))} {}
```

---

### 延迟初始化

```cpp
// Widget.h
class Widget {
    struct Impl;
    Pimpl<Impl> pimpl_;  // 默认构造为空
public:
    void initialize();
    void do_work();
};

// Widget.cpp
void Widget::initialize() {
    if (!pimpl_) {
        pimpl_ = make_pimpl<Impl>();
    }
}

void Widget::do_work() {
    if (pimpl_) {
        pimpl_->work();
    }
}
```

---

## 使用场景

### 1. 跨 DLL 边界的类

**问题**：直接暴露成员变量会破坏 ABI 兼容性。

**解决方案**：使用 PIMPL 模式隐藏实现。

```cpp
// 公共头（stable ABI）
class MINE_API Visual {
    struct Impl;
    Pimpl<Impl> p_;
public:
    Visual();
    ~Visual();
    void render();
};

// 实现（可自由修改）
struct Visual::Impl {
    std::vector<Child*> children;  // 可修改成员类型
    bool dirty{true};              // 可添加新成员
    // 修改不破坏 ABI
};
```

---

### 2. 减少编译依赖

**问题**：公共头包含大量实现相关头文件,拖慢编译。

**解决方案**：将依赖移至 `.cpp` 文件。

```cpp
// Widget.h（无需包含 <vector>/<string>）
class Widget {
    struct Impl;
    Pimpl<Impl> pimpl_;
public:
    Widget();
    ~Widget();
};

// Widget.cpp（实现细节在此）
#include <vector>
#include <string>
struct Widget::Impl {
    std::vector<std::string> data;
};
```

---

### 3. 隐藏第三方库依赖

**问题**：公共头暴露第三方库类型,限制调用方。

**解决方案**：将第三方类型封装在 `Impl` 中。

```cpp
// AudioEngine.h（不暴露 FMOD 类型）
class MINE_API AudioEngine {
    struct Impl;
    Pimpl<Impl> pimpl_;
public:
    void play_sound(const char* path);
};

// AudioEngine.cpp
#include <fmod.hpp>  // 第三方库
struct AudioEngine::Impl {
    FMOD::System* system;
    FMOD::Sound* sound;
};
```

---

## 性能分析

### 内存布局

```cpp
class Widget {
    Pimpl<Impl> pimpl_;  // OwnedPtr<Impl>
};

sizeof(Pimpl<Impl>) == sizeof(OwnedPtr<Impl>) == 16  // 指针(8) + 删除器(8)
```

### 操作时间复杂度

| 操作 | 时间复杂度 | 说明 |
|------|-----------|------|
| 构造 | O(1) + `Impl` 构造 | 堆分配 + placement new |
| 析构 | O(1) + `Impl` 析构 | 调用 `Impl::~Impl()` + `dealloc` |
| 移动构造/赋值 | O(1) | 转移指针所有权 |
| `operator->` | O(1) | 一次指针解引用 |
| `operator*` | O(1) | 一次指针解引用 |

### 间接访问开销

**指针解引用**：每次通过 `pimpl_->` 访问成员涉及一次额外的指针解引用。

**缓存局部性**：`Impl` 在堆上分配,可能导致缓存未命中。

**示例**：

```cpp
// 不使用 PIMPL
class Direct {
    int value_;
public:
    int get() const { return value_; }  // 直接访问
};

// 使用 PIMPL
class Indirect {
    struct Impl { int value; };
    Pimpl<Impl> pimpl_;
public:
    int get() const { return pimpl_->value; }  // 间接访问
};
```

**结论**：PIMPL 有轻微性能开销（额外指针解引用）,但 ABI 稳定性和编译时间优化通常值得。

---

## 最佳实践

### 1. 析构函数必须在 .cpp 中定义

**推荐**：在头文件声明,在 `.cpp` 文件定义。

```cpp
// ✅ 正确
// Widget.h
class Widget {
    struct Impl;
    Pimpl<Impl> pimpl_;
public:
    ~Widget();  // 声明
};

// Widget.cpp
Widget::~Widget() = default;  // 定义在 Impl 完整类型可见处
```

**错误示例**：

```cpp
// ❌ 错误：内联析构函数
// Widget.h
class Widget {
    struct Impl;
    Pimpl<Impl> pimpl_;
public:
    ~Widget() = default;  // Impl 不完整,编译错误
};
```

---

### 2. 移动构造/赋值也需在 .cpp 中定义

**推荐**：与析构函数一样,在 `.cpp` 中定义。

```cpp
// Widget.h
class Widget {
    struct Impl;
    Pimpl<Impl> pimpl_;
public:
    Widget(Widget&&) noexcept;
    Widget& operator=(Widget&&) noexcept;
};

// Widget.cpp
Widget::Widget(Widget&&) noexcept = default;
Widget& Widget::operator=(Widget&&) noexcept = default;
```

---

### 3. 构造函数使用 make_pimpl

**推荐**：通过 `make_pimpl` 初始化,而非手动分配。

```cpp
// ✅ 推荐
Widget::Widget() : pimpl_{make_pimpl<Impl>()} {}

// ❌ 不推荐（手动分配）
Widget::Widget() {
    void* mem = allocator->alloc(sizeof(Impl));
    pimpl_.reset(new (mem) Impl{});
}
```

---

### 4. Impl 结构体定义在 .cpp 文件顶部

**推荐**：将 `Impl` 定义放在 `.cpp` 文件开头,方便查看。

```cpp
// Widget.cpp
#include "Widget.h"

// Impl 定义在顶部
struct Widget::Impl {
    int value{0};
    std::vector<Child> children;
    
    void increment() { ++value; }
};

// 公共方法实现
Widget::Widget() : pimpl_{make_pimpl<Impl>()} {}
void Widget::do_work() { pimpl_->increment(); }
```

---

### 5. 考虑提供拷贝语义（可选）

**推荐**：若 `Impl` 可拷贝,可实现深拷贝构造。

```cpp
// Widget.h
class Widget {
    struct Impl;
    Pimpl<Impl> pimpl_;
public:
    Widget(const Widget& other);  // 深拷贝
};

// Widget.cpp
Widget::Widget(const Widget& other)
    : pimpl_{make_pimpl<Impl>(*other.pimpl_)} {}  // 拷贝 Impl 内容
```

---

## 完整示例

### 示例：文本编辑器组件

```cpp
// TextEditor.h
#pragma once
#include <mine/core/Pimpl.h>
#include <mine/core/StringView.h>

class MINE_API TextEditor {
public:
    TextEditor();
    ~TextEditor();

    TextEditor(TextEditor&&) noexcept;
    TextEditor& operator=(TextEditor&&) noexcept;

    void insert_text(StringView text);
    void delete_selection();
    void undo();
    void redo();

    StringView get_text() const;

private:
    struct Impl;
    mine::core::Pimpl<Impl> pimpl_;
};
```

```cpp
// TextEditor.cpp
#include "TextEditor.h"
#include <string>
#include <vector>

struct TextEditor::Impl {
    std::string content;
    std::vector<std::string> undo_stack;
    std::vector<std::string> redo_stack;
    
    void push_undo() {
        undo_stack.push_back(content);
        redo_stack.clear();
    }
};

TextEditor::TextEditor() : pimpl_{mine::core::make_pimpl<Impl>()} {}
TextEditor::~TextEditor() = default;

TextEditor::TextEditor(TextEditor&&) noexcept = default;
TextEditor& TextEditor::operator=(TextEditor&&) noexcept = default;

void TextEditor::insert_text(StringView text) {
    pimpl_->push_undo();
    pimpl_->content.append(text.data(), text.size());
}

void TextEditor::undo() {
    if (!pimpl_->undo_stack.empty()) {
        pimpl_->redo_stack.push_back(std::move(pimpl_->content));
        pimpl_->content = std::move(pimpl_->undo_stack.back());
        pimpl_->undo_stack.pop_back();
    }
}

StringView TextEditor::get_text() const {
    return StringView{pimpl_->content};
}
```

---

## 总结

`Pimpl<T>` 是 MineFramework 中实现 PIMPL 模式的标准方式,具备以下优势:

1. **ABI 稳定**：修改实现不破坏二进制兼容性
2. **编译时间优化**：公共头无需包含大量实现头文件
3. **类型安全**：通过 `OwnedPtr` 管理生命周期
4. **便捷访问**：`operator->` / `operator*` 提供直接访问

在设计跨 DLL 边界的类时,优先使用 PIMPL 模式,这将显著提升代码的稳定性和可维护性。结合 `make_pimpl` 工厂函数,实现 PIMPL 模式变得简单而高效。
