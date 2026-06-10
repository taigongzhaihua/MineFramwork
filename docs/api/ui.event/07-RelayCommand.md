# RelayCommand 详细接口文档

## 概述

`RelayCommand` 是 `mine.ui.event` 模块的**基于函数对象的 ICommand 实现**。

**核心特性：**
- **函数对象封装**：接受任意可调用对象（lambda、函数指针）作为 `execute`/`can_execute` 逻辑
- **Pimpl 模式**：以 Pimpl 模式实现，确保跨 DLL 边界的 ABI 稳定性
- **SBO 优化**：内部用 `core::Function<>` 存储（SBO 32 字节，move-only）
- **手动通知**：支持 `notify_can_execute_changed()` 手动触发通知
- **便捷构造**：构造时注入 lambda，简化命令定义

---

## 文件位置

```
src/mine/ui/event/api/include/mine/ui/event/RelayCommand.h
```

---

## 类定义

```cpp
class MINE_UI_EVENT_API RelayCommand final : public ICommand {
public:
    using ExecuteFn    = core::Function<void(const core::Variant&)>;
    using CanExecuteFn = core::Function<bool(const core::Variant&)>;

    explicit RelayCommand(ExecuteFn    execute,
                          CanExecuteFn can_execute = {}) noexcept;

    ~RelayCommand() override;

    // move-only
    RelayCommand(RelayCommand&&) noexcept;
    RelayCommand& operator=(RelayCommand&&) noexcept;

    RelayCommand(const RelayCommand&)            = delete;
    RelayCommand& operator=(const RelayCommand&) = delete;

    // ICommand 接口实现
    [[nodiscard]] bool can_execute(
        const core::Variant& param) const noexcept override;

    void execute(const core::Variant& param) noexcept override;

    [[nodiscard]] uint32_t subscribe_can_execute_changed(
        ChangedFn fn, void* user_data) noexcept override;

    void unsubscribe_can_execute_changed(uint32_t token) noexcept override;

    // 额外 API
    void notify_can_execute_changed() noexcept;

private:
    struct Impl;
    core::Pimpl<Impl> p_;
};
```

**描述**：基于函数对象的 ICommand 具体实现。

**特征**：
- `final` 类（不可继承）
- move-only（不可拷贝）
- Pimpl 模式（公共头不暴露容器实现细节）

---

## 类型别名

### ExecuteFn

```cpp
using ExecuteFn = core::Function<void(const core::Variant&)>;
```

**描述**：执行函数类型（move-only，SBO 优化）。

**示例**：
```cpp
ExecuteFn execute = [this](const core::Variant&) { do_something(); };
```

---

### CanExecuteFn

```cpp
using CanExecuteFn = core::Function<bool(const core::Variant&)>;
```

**描述**：可执行判断函数类型（move-only，SBO 优化）。

**示例**：
```cpp
CanExecuteFn can_execute = [this](const core::Variant&) { return is_valid_; };
```

---

## 构造函数

### RelayCommand

```cpp
explicit RelayCommand(ExecuteFn    execute,
                      CanExecuteFn can_execute = {}) noexcept;
```

**描述**：构造 RelayCommand。

**参数**：
- `execute`：命令执行逻辑（必须非空）
- `can_execute`：可执行判断逻辑（可为空，空时始终返回 `true`）

**示例**：
```cpp
// 无参命令（始终可执行）
RelayCommand cmd{[this](const core::Variant&) { do_something(); }};

// 带 CanExecute 条件的命令
RelayCommand cmd{
    [this](const core::Variant&) { save(); },
    [this](const core::Variant&) { return has_unsaved_changes_; }
};
```

---

## ICommand 接口实现

### can_execute

```cpp
[[nodiscard]] bool can_execute(
    const core::Variant& param) const noexcept override;
```

**描述**：调用构造时传入的 `can_execute` 函数。

**参数**：
- `param`：可选命令参数

**返回值**：
- 若构造时 `can_execute` 为空，始终返回 `true`
- 否则返回 `can_execute(param)` 的结果

**示例**：
```cpp
RelayCommand cmd{
    [this](const core::Variant&) { save(); },
    [this](const core::Variant&) { return has_unsaved_changes_; }
};

bool can_exec = cmd.can_execute(core::Variant::null());
// 返回 has_unsaved_changes_
```

---

### execute

```cpp
void execute(const core::Variant& param) noexcept override;
```

**描述**：调用构造时传入的 `execute` 函数。

**参数**：
- `param`：可选命令参数

**示例**：
```cpp
RelayCommand cmd{[this](const core::Variant&) { do_something(); }};

cmd.execute(core::Variant::null());
// 调用 do_something()
```

---

### subscribe_can_execute_changed

```cpp
[[nodiscard]] uint32_t subscribe_can_execute_changed(
    ChangedFn fn, void* user_data) noexcept override;
```

**描述**：订阅 `can_execute` 变化通知。

**参数**：
- `fn`：通知回调函数
- `user_data`：回调时原样传回的用户数据

**返回值**：
- 订阅 token（传给 `unsubscribe_can_execute_changed` 取消）

**示例**：
```cpp
RelayCommand cmd{[this](const core::Variant&) { save(); }};

uint32_t token = cmd.subscribe_can_execute_changed(
    [](ICommand* sender, void* user_data) {
        std::cout << "Can execute changed" << std::endl;
    },
    nullptr
);
```

---

### unsubscribe_can_execute_changed

```cpp
void unsubscribe_can_execute_changed(uint32_t token) noexcept override;
```

**描述**：取消订阅。

**参数**：
- `token`：之前 `subscribe_can_execute_changed` 返回的 token

**示例**：
```cpp
cmd.unsubscribe_can_execute_changed(token);
```

---

## 额外 API

### notify_can_execute_changed

```cpp
void notify_can_execute_changed() noexcept;
```

**描述**：手动触发 `can_execute` 变化通知。

**使用场景**：
- 当 ViewModel 内部状态变化导致 `can_execute` 结果改变时，调用此方法以通知已订阅的 UI 元素重新查询 `can_execute()`

**示例**：
```cpp
RelayCommand cmd{
    [this](const core::Variant&) { save(); },
    [this](const core::Variant&) { return has_unsaved_changes_; }
};

// 状态变化
has_unsaved_changes_ = true;

// 通知 UI 刷新
cmd.notify_can_execute_changed();
```

---

## 使用场景

### 1. 无参命令（始终可执行）

```cpp
#include <mine/ui/event/RelayCommand.h>

using namespace mine::ui;

class MyViewModel {
public:
    MyViewModel()
        : close_command_{[this](const core::Variant&) { do_close(); }}
    {}

    ICommand* close_command() { return &close_command_; }

private:
    void do_close() {
        std::cout << "Closing..." << std::endl;
    }

    RelayCommand close_command_;
};
```

---

### 2. 带 CanExecute 条件的命令

```cpp
class MyViewModel {
public:
    MyViewModel()
        : save_command_{
            [this](const core::Variant&) { do_save(); },
            [this](const core::Variant&) { return has_unsaved_changes_; }
          }
    {}

    ICommand* save_command() { return &save_command_; }

    void set_has_unsaved_changes(bool has_changes) {
        if (has_unsaved_changes_ != has_changes) {
            has_unsaved_changes_ = has_changes;
            save_command_.notify_can_execute_changed();  // 通知 UI 刷新
        }
    }

private:
    void do_save() {
        std::cout << "Saving..." << std::endl;
        has_unsaved_changes_ = false;
        save_command_.notify_can_execute_changed();
    }

    RelayCommand save_command_;
    bool has_unsaved_changes_ = false;
};
```

---

### 3. 带参数的命令

```cpp
class MyViewModel {
public:
    MyViewModel()
        : delete_command_{
            [this](const core::Variant& param) { do_delete(param); },
            [this](const core::Variant& param) { return can_delete(param); }
          }
    {}

    ICommand* delete_command() { return &delete_command_; }

private:
    void do_delete(const core::Variant& param) {
        int id = param.as_int();
        std::cout << "Deleting item " << id << std::endl;
    }

    bool can_delete(const core::Variant& param) const {
        int id = param.as_int();
        return id > 0;  // 仅允许删除 ID > 0 的项
    }

    RelayCommand delete_command_;
};

// UI 元素使用带参数的命令
ICommand* delete_command = view_model.delete_command();
core::Variant param = core::Variant::from_int(123);

if (delete_command->can_execute(param)) {
    delete_command->execute(param);
}
```

---

### 4. 命令状态变化通知

```cpp
class MyViewModel {
public:
    MyViewModel()
        : undo_command_{
            [this](const core::Variant&) { do_undo(); },
            [this](const core::Variant&) { return can_undo(); }
          }
    {}

    ICommand* undo_command() { return &undo_command_; }

    void push_undo(const std::string& action) {
        undo_stack_.push_back(action);
        undo_command_.notify_can_execute_changed();  // 通知 UI 刷新
    }

private:
    void do_undo() {
        if (!undo_stack_.empty()) {
            std::cout << "Undo: " << undo_stack_.back() << std::endl;
            undo_stack_.pop_back();
            undo_command_.notify_can_execute_changed();
        }
    }

    bool can_undo() const {
        return !undo_stack_.empty();
    }

    RelayCommand undo_command_;
    std::vector<std::string> undo_stack_;
};
```

---

### 5. move-only 语义

```cpp
// ✅ 可以移动
RelayCommand cmd1{[this](const core::Variant&) { do_something(); }};
RelayCommand cmd2 = std::move(cmd1);  // ✅ 移动构造

// ❌ 不可拷贝
RelayCommand cmd3 = cmd2;  // ❌ 编译错误：禁止拷贝
```

---

## 最佳实践

### 1. 状态变化时调用 notify_can_execute_changed

```cpp
// ✅ 推荐：状态变化时调用 notify_can_execute_changed
void set_has_unsaved_changes(bool has_changes) {
    if (has_unsaved_changes_ != has_changes) {
        has_unsaved_changes_ = has_changes;
        save_command_.notify_can_execute_changed();  // ✅ 通知 UI 刷新
    }
}

// ❌ 不推荐：状态变化不调用 notify_can_execute_changed
void set_has_unsaved_changes(bool has_changes) {
    has_unsaved_changes_ = has_changes;
    // ❌ 忘记通知，UI 不会刷新
}
```

---

### 2. 构造时传入 lambda（捕获 this）

```cpp
// ✅ 推荐：构造时传入 lambda（捕获 this）
RelayCommand save_command_{
    [this](const core::Variant&) { do_save(); },
    [this](const core::Variant&) { return has_unsaved_changes_; }
};

// ❌ 不推荐：使用函数指针（无法访问成员）
static void do_save_static(const core::Variant&) {
    // ❌ 无法访问成员变量
}

RelayCommand save_command_{do_save_static};
```

---

### 3. 使用 move 语义（避免拷贝）

```cpp
// ✅ 推荐：使用 move 语义
RelayCommand cmd1{[this](const core::Variant&) { do_something(); }};
RelayCommand cmd2 = std::move(cmd1);  // ✅ 移动构造

// ❌ 不推荐：尝试拷贝（编译错误）
RelayCommand cmd3 = cmd2;  // ❌ 编译错误
```

---

### 4. 使用成员变量存储 RelayCommand（而非指针）

```cpp
// ✅ 推荐：使用成员变量存储 RelayCommand
class MyViewModel {
public:
    ICommand* save_command() { return &save_command_; }

private:
    RelayCommand save_command_{
        [this](const core::Variant&) { do_save(); }
    };
};

// ❌ 不推荐：使用指针存储（增加复杂性）
class MyViewModel {
public:
    MyViewModel() {
        save_command_ = new RelayCommand{
            [this](const core::Variant&) { do_save(); }
        };
    }

    ~MyViewModel() {
        delete save_command_;
    }

    ICommand* save_command() { return save_command_; }

private:
    RelayCommand* save_command_;
};
```

---

## 常见陷阱

### 1. 忘记调用 notify_can_execute_changed

```cpp
// ❌ 问题：状态变化不调用 notify_can_execute_changed
void set_has_unsaved_changes(bool has_changes) {
    has_unsaved_changes_ = has_changes;
    // ❌ 忘记通知，UI 不会刷新
}

// UI 元素状态不会更新
button.set_enabled(save_command_.can_execute(core::Variant::null()));
// ❌ 即使 has_unsaved_changes_ 变化，button 仍保持旧状态

// ✅ 解决：调用 notify_can_execute_changed
void set_has_unsaved_changes(bool has_changes) {
    if (has_unsaved_changes_ != has_changes) {
        has_unsaved_changes_ = has_changes;
        save_command_.notify_can_execute_changed();  // ✅ 通知 UI 刷新
    }
}
```

---

### 2. lambda 捕获临时变量

```cpp
// ❌ 问题：lambda 捕获临时变量
void create_command() {
    int temp_value = 42;
    
    save_command_ = RelayCommand{
        [&temp_value](const core::Variant&) {
            // ❌ temp_value 是临时变量，作用域结束后失效
            std::cout << "Value: " << temp_value << std::endl;
        }
    };
}

// 稍后调用
save_command_.execute(core::Variant::null());
// ❌ 访问已失效的 temp_value，未定义行为

// ✅ 解决：捕获成员变量或按值捕获
class MyViewModel {
public:
    MyViewModel()
        : value_(42),
          save_command_{
            [this](const core::Variant&) {
                std::cout << "Value: " << value_ << std::endl;  // ✅ 捕获 this，访问成员
            }
          }
    {}

private:
    int value_;
    RelayCommand save_command_;
};
```

---

### 3. 尝试拷贝 RelayCommand

```cpp
// ❌ 问题：尝试拷贝 RelayCommand
RelayCommand cmd1{[this](const core::Variant&) { do_something(); }};
RelayCommand cmd2 = cmd1;  // ❌ 编译错误：禁止拷贝

// ✅ 解决：使用 move 语义
RelayCommand cmd2 = std::move(cmd1);  // ✅ 移动构造
```

---

### 4. execute 函数为空

```cpp
// ❌ 问题：execute 函数为空
RelayCommand cmd{nullptr};  // ❌ execute 为空

cmd.execute(core::Variant::null());
// ❌ 调用空函数，未定义行为

// ✅ 解决：构造时传入有效的 execute 函数
RelayCommand cmd{[this](const core::Variant&) { do_something(); }};
```

---

## 完整示例

```cpp
#include <mine/ui/event/RelayCommand.h>
#include <mine/ui/event/ICommand.h>
#include <iostream>
#include <vector>

using namespace mine::ui;

// ────────────────────────────────────────────────────────────────────────────
// ViewModel 定义
// ────────────────────────────────────────────────────────────────────────────

class MyViewModel {
public:
    MyViewModel()
        : save_command_{
            [this](const core::Variant&) { do_save(); },
            [this](const core::Variant&) { return has_unsaved_changes_; }
          },
          undo_command_{
            [this](const core::Variant&) { do_undo(); },
            [this](const core::Variant&) { return can_undo(); }
          },
          close_command_{
            [this](const core::Variant&) { do_close(); }
          }
    {}

    ICommand* save_command() { return &save_command_; }
    ICommand* undo_command() { return &undo_command_; }
    ICommand* close_command() { return &close_command_; }

    void set_has_unsaved_changes(bool has_changes) {
        if (has_unsaved_changes_ != has_changes) {
            has_unsaved_changes_ = has_changes;
            std::cout << "Has unsaved changes: " << (has_unsaved_changes_ ? "Yes" : "No") << std::endl;
            save_command_.notify_can_execute_changed();
        }
    }

    void push_undo(const std::string& action) {
        undo_stack_.push_back(action);
        std::cout << "Pushed undo action: " << action << std::endl;
        undo_command_.notify_can_execute_changed();
    }

private:
    void do_save() {
        std::cout << "Saving..." << std::endl;
        has_unsaved_changes_ = false;
        save_command_.notify_can_execute_changed();
    }

    void do_undo() {
        if (!undo_stack_.empty()) {
            std::cout << "Undo: " << undo_stack_.back() << std::endl;
            undo_stack_.pop_back();
            undo_command_.notify_can_execute_changed();
        }
    }

    bool can_undo() const {
        return !undo_stack_.empty();
    }

    void do_close() {
        std::cout << "Closing..." << std::endl;
    }

    RelayCommand save_command_;
    RelayCommand undo_command_;
    RelayCommand close_command_;
    bool has_unsaved_changes_ = false;
    std::vector<std::string> undo_stack_;
};

// ────────────────────────────────────────────────────────────────────────────
// 使用示例
// ────────────────────────────────────────────────────────────────────────────

int main() {
    MyViewModel view_model;
    
    // 初始状态
    std::cout << "════════════════════════════════════════════════" << std::endl;
    std::cout << "初始状态" << std::endl;
    std::cout << "════════════════════════════════════════════════" << std::endl;
    std::cout << "Can save: " << (view_model.save_command()->can_execute(core::Variant::null()) ? "Yes" : "No") << std::endl;
    std::cout << "Can undo: " << (view_model.undo_command()->can_execute(core::Variant::null()) ? "Yes" : "No") << std::endl;
    std::cout << "Can close: " << (view_model.close_command()->can_execute(core::Variant::null()) ? "Yes" : "No") << std::endl;
    // 输出：
    // Can save: No
    // Can undo: No
    // Can close: Yes
    
    // 用户修改数据
    std::cout << std::endl;
    std::cout << "════════════════════════════════════════════════" << std::endl;
    std::cout << "用户修改数据" << std::endl;
    std::cout << "════════════════════════════════════════════════" << std::endl;
    view_model.set_has_unsaved_changes(true);
    view_model.push_undo("Edit text");
    std::cout << "Can save: " << (view_model.save_command()->can_execute(core::Variant::null()) ? "Yes" : "No") << std::endl;
    std::cout << "Can undo: " << (view_model.undo_command()->can_execute(core::Variant::null()) ? "Yes" : "No") << std::endl;
    // 输出：
    // Has unsaved changes: Yes
    // Pushed undo action: Edit text
    // Can save: Yes
    // Can undo: Yes
    
    // 用户保存
    std::cout << std::endl;
    std::cout << "════════════════════════════════════════════════" << std::endl;
    std::cout << "用户保存" << std::endl;
    std::cout << "════════════════════════════════════════════════" << std::endl;
    if (view_model.save_command()->can_execute(core::Variant::null())) {
        view_model.save_command()->execute(core::Variant::null());
    }
    std::cout << "Can save: " << (view_model.save_command()->can_execute(core::Variant::null()) ? "Yes" : "No") << std::endl;
    // 输出：
    // Saving...
    // Has unsaved changes: No
    // Can save: No
    
    // 用户撤销
    std::cout << std::endl;
    std::cout << "════════════════════════════════════════════════" << std::endl;
    std::cout << "用户撤销" << std::endl;
    std::cout << "════════════════════════════════════════════════" << std::endl;
    if (view_model.undo_command()->can_execute(core::Variant::null())) {
        view_model.undo_command()->execute(core::Variant::null());
    }
    std::cout << "Can undo: " << (view_model.undo_command()->can_execute(core::Variant::null()) ? "Yes" : "No") << std::endl;
    // 输出：
    // Undo: Edit text
    // Can undo: No
    
    // 用户关闭
    std::cout << std::endl;
    std::cout << "════════════════════════════════════════════════" << std::endl;
    std::cout << "用户关闭" << std::endl;
    std::cout << "════════════════════════════════════════════════" << std::endl;
    if (view_model.close_command()->can_execute(core::Variant::null())) {
        view_model.close_command()->execute(core::Variant::null());
    }
    // 输出：
    // Closing...
    
    return 0;
}
```

---

## 总结

`RelayCommand` 是 `mine.ui.event` 模块的基于函数对象的 ICommand 实现，具备：

1. **函数对象封装**：接受任意可调用对象（lambda、函数指针）作为 `execute`/`can_execute` 逻辑
2. **Pimpl 模式**：以 Pimpl 模式实现，确保跨 DLL 边界的 ABI 稳定性
3. **SBO 优化**：内部用 `core::Function<>` 存储（SBO 32 字节，move-only）
4. **手动通知**：支持 `notify_can_execute_changed()` 手动触发通知
5. **便捷构造**：构造时注入 lambda，简化命令定义

**使用建议**：
- 状态变化时调用 `notify_can_execute_changed`（UI 自动刷新）
- 构造时传入 lambda（捕获 `this`，访问成员变量）
- 使用 move 语义（避免拷贝）
- 使用成员变量存储 `RelayCommand`（而非指针）
- 避免 lambda 捕获临时变量（生命周期问题）
- 确保 `execute` 函数非空（否则未定义行为）
