# ICommand 详细接口文档

## 概述

`ICommand` 是 `mine.ui.event` 模块的**命令接口**。

**核心特性：**
- **MVVM 命令模式**：遵循 WPF/MVVM 命令模式
- **可执行状态查询**：`can_execute()` 判断命令是否可执行（用于 UI 元素的 `IsEnabled` 绑定）
- **命令执行**：`execute()` 执行命令逻辑
- **状态变化通知**：`subscribe_can_execute_changed()` 订阅可执行状态变化（供 UI 重新查询 `can_execute`）
- **UI 框架解耦**：UI 元素通过此接口与 ViewModel 的命令逻辑解耦

---

## 文件位置

```
src/mine/ui/event/api/include/mine/ui/event/ICommand.h
```

---

## 类定义

```cpp
class MINE_UI_EVENT_API ICommand {
public:
    virtual ~ICommand() = default;

    [[nodiscard]] virtual bool can_execute(
        const core::Variant& param) const noexcept = 0;

    virtual void execute(const core::Variant& param) noexcept = 0;

    using ChangedFn = void(*)(ICommand* sender, void* user_data);

    [[nodiscard]] virtual uint32_t subscribe_can_execute_changed(
        ChangedFn fn, void* user_data) noexcept = 0;

    virtual void unsubscribe_can_execute_changed(
        uint32_t token) noexcept = 0;
};
```

**描述**：命令抽象接口。

**特征**：
- 纯虚接口（所有方法都是纯虚函数）
- 虚析构函数（支持多态）
- 与 UI 框架解耦（通过接口与 ViewModel 通信）

---

## 类型别名

### ChangedFn

```cpp
using ChangedFn = void(*)(ICommand* sender, void* user_data);
```

**描述**：可执行状态变化通知处理器类型。

**参数**：
- `sender`：触发通知的命令对象
- `user_data`：注册时传入的用户自定义数据

**示例**：
```cpp
void on_can_execute_changed(ICommand* sender, void* user_data) {
    // 重新查询 can_execute 状态
    bool can_exec = sender->can_execute(core::Variant::null());
    std::cout << "Can execute: " << (can_exec ? "Yes" : "No") << std::endl;
}
```

---

## 接口方法

### can_execute

```cpp
[[nodiscard]] virtual bool can_execute(
    const core::Variant& param) const noexcept = 0;
```

**描述**：判断当前参数下命令是否可执行。

**参数**：
- `param`：可选命令参数（无参数时传 `Variant::null()`）

**返回值**：
- `true`：命令可执行
- `false`：命令被禁用

**使用场景**：
- UI 元素调用此方法决定是否启用（例如 Button 的 `IsEnabled` 属性）
- UI 框架在可执行状态变化时重新查询

**示例**：
```cpp
// UI 元素查询命令是否可执行
bool can_exec = save_command->can_execute(core::Variant::null());
button.set_enabled(can_exec);
```

---

### execute

```cpp
virtual void execute(const core::Variant& param) noexcept = 0;
```

**描述**：执行命令。

**参数**：
- `param`：可选命令参数（与 `can_execute` 保持一致）

**注意**：
- 实现者应确保此函数不会抛出异常
- 建议在调用前先通过 `can_execute()` 检查是否可执行

**使用场景**：
- UI 元素在用户交互时调用（例如 Button 的 `Click` 事件）

**示例**：
```cpp
// UI 元素在用户点击时执行命令
void on_button_click() {
    if (save_command->can_execute(core::Variant::null())) {
        save_command->execute(core::Variant::null());
    }
}
```

---

### subscribe_can_execute_changed

```cpp
[[nodiscard]] virtual uint32_t subscribe_can_execute_changed(
    ChangedFn fn, void* user_data) noexcept = 0;
```

**描述**：订阅 `can_execute` 可能发生变化的通知。

**参数**：
- `fn`：通知回调函数
- `user_data`：回调时原样传回的用户数据

**返回值**：
- 订阅 token（用于取消订阅）

**使用场景**：
- 每次 ViewModel 内部状态改变导致 `can_execute()` 结果可能变化时，命令实现应触发此通知
- UI 框架重新调用 `can_execute()` 以刷新状态

**示例**：
```cpp
// UI 元素订阅可执行状态变化
uint32_t token = save_command->subscribe_can_execute_changed(
    [](ICommand* sender, void* user_data) {
        // 重新查询 can_execute 状态
        bool can_exec = sender->can_execute(core::Variant::null());
        auto* button = static_cast<Button*>(user_data);
        button->set_enabled(can_exec);
    },
    button_ptr
);
```

---

### unsubscribe_can_execute_changed

```cpp
virtual void unsubscribe_can_execute_changed(
    uint32_t token) noexcept = 0;
```

**描述**：取消 `can_execute` 变化通知订阅。

**参数**：
- `token`：`subscribe_can_execute_changed()` 返回的 token

**使用场景**：
- UI 元素销毁时取消订阅

**示例**：
```cpp
// UI 元素销毁时取消订阅
save_command->unsubscribe_can_execute_changed(token);
```

---

## 使用场景

### 1. ViewModel 定义命令（RelayCommand 实现）

```cpp
#include <mine/ui/event/ICommand.h>
#include <mine/ui/event/RelayCommand.h>

using namespace mine::ui;

class MyViewModel {
public:
    MyViewModel()
        : save_command_{
            [this](const core::Variant&) { do_save(); },
            [this](const core::Variant&) { return is_dirty_; }
          }
    {}

    ICommand* save_command() { return &save_command_; }

    void set_dirty(bool dirty) {
        if (is_dirty_ != dirty) {
            is_dirty_ = dirty;
            save_command_.raise_can_execute_changed();  // 触发可执行状态变化通知
        }
    }

private:
    void do_save() {
        std::cout << "Saving..." << std::endl;
        is_dirty_ = false;
        save_command_.raise_can_execute_changed();
    }

    RelayCommand save_command_;
    bool is_dirty_ = false;
};
```

---

### 2. UI 元素绑定命令

```cpp
// XAML 风格（伪代码）：
// <Button Command="{Binding SaveCommand}" Content="Save" />

// C++ 代码：
MyViewModel view_model;
Button save_button;

// 绑定命令
ICommand* save_command = view_model.save_command();

// 订阅可执行状态变化
uint32_t token = save_command->subscribe_can_execute_changed(
    [](ICommand* sender, void* user_data) {
        bool can_exec = sender->can_execute(core::Variant::null());
        auto* button = static_cast<Button*>(user_data);
        button->set_enabled(can_exec);
    },
    &save_button
);

// 初始化按钮状态
save_button.set_enabled(save_command->can_execute(core::Variant::null()));

// 绑定 Click 事件
save_button.add_handler(Button::ClickEvent, [save_command](RoutedEventArgs& args) {
    if (save_command->can_execute(core::Variant::null())) {
        save_command->execute(core::Variant::null());
    }
});

// 触发状态变化
view_model.set_dirty(true);  // save_button 自动变为启用状态
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

// 检查是否可执行
core::Variant param = core::Variant::from_int(123);
bool can_exec = delete_command->can_execute(param);

// 执行命令
if (can_exec) {
    delete_command->execute(param);
}
```

---

### 4. 自定义命令实现

```cpp
#include <mine/ui/event/ICommand.h>
#include <vector>

using namespace mine::ui;

class CustomCommand : public ICommand {
public:
    [[nodiscard]] bool can_execute(
        const core::Variant& param) const noexcept override {
        // 自定义逻辑
        return true;
    }

    void execute(const core::Variant& param) noexcept override {
        // 自定义逻辑
        std::cout << "Command executed" << std::endl;
        
        // 触发可执行状态变化通知
        raise_can_execute_changed();
    }

    [[nodiscard]] uint32_t subscribe_can_execute_changed(
        ChangedFn fn, void* user_data) noexcept override {
        uint32_t token = next_token_++;
        handlers_.push_back({token, fn, user_data});
        return token;
    }

    void unsubscribe_can_execute_changed(uint32_t token) noexcept override {
        handlers_.erase(
            std::remove_if(handlers_.begin(), handlers_.end(),
                [token](const HandlerEntry& entry) { return entry.token == token; }),
            handlers_.end()
        );
    }

private:
    void raise_can_execute_changed() {
        for (auto& entry : handlers_) {
            entry.fn(this, entry.user_data);
        }
    }

    struct HandlerEntry {
        uint32_t  token;
        ChangedFn fn;
        void*     user_data;
    };

    std::vector<HandlerEntry> handlers_;
    uint32_t next_token_ = 1;
};
```

---

## 最佳实践

### 1. 调用 execute 前先检查 can_execute

```cpp
// ✅ 推荐：调用 execute 前先检查 can_execute
if (save_command->can_execute(core::Variant::null())) {
    save_command->execute(core::Variant::null());
}

// ❌ 不推荐：直接调用 execute（可能在禁用状态下执行）
save_command->execute(core::Variant::null());
```

---

### 2. ViewModel 状态变化时触发 can_execute_changed

```cpp
// ✅ 推荐：状态变化时触发通知
void set_dirty(bool dirty) {
    if (is_dirty_ != dirty) {
        is_dirty_ = dirty;
        save_command_.raise_can_execute_changed();  // ✅ 触发通知
    }
}

// ❌ 不推荐：状态变化不触发通知（UI 不会刷新）
void set_dirty(bool dirty) {
    is_dirty_ = dirty;
    // ❌ 忘记触发通知
}
```

---

### 3. UI 元素销毁时取消订阅

```cpp
// ✅ 推荐：UI 元素销毁时取消订阅
class Button {
public:
    ~Button() {
        if (command_ && token_ != 0) {
            command_->unsubscribe_can_execute_changed(token_);  // ✅ 取消订阅
        }
    }

    void set_command(ICommand* command) {
        if (command_ && token_ != 0) {
            command_->unsubscribe_can_execute_changed(token_);
        }
        
        command_ = command;
        
        if (command_) {
            token_ = command_->subscribe_can_execute_changed(
                on_can_execute_changed, this);
        }
    }

private:
    ICommand* command_ = nullptr;
    uint32_t token_ = 0;
};

// ❌ 不推荐：忘记取消订阅（内存泄漏）
class Button {
public:
    void set_command(ICommand* command) {
        command_ = command;
        token_ = command_->subscribe_can_execute_changed(
            on_can_execute_changed, this);
        // ❌ 忘记取消订阅
    }

private:
    ICommand* command_ = nullptr;
    uint32_t token_ = 0;
};
```

---

### 4. execute 不抛出异常

```cpp
// ✅ 推荐：execute 不抛出异常
void execute(const core::Variant& param) noexcept override {
    try {
        // 可能抛出异常的代码
        do_something_risky();
    } catch (...) {
        // 捕获并处理异常
        std::cerr << "Command execution failed" << std::endl;
    }
}

// ❌ 不推荐：execute 可能抛出异常
void execute(const core::Variant& param) noexcept override {
    // ❌ 可能抛出异常，但声明为 noexcept
    do_something_risky();
}
```

---

## 常见陷阱

### 1. 忘记检查 can_execute

```cpp
// ❌ 问题：直接调用 execute（可能在禁用状态下执行）
save_command->execute(core::Variant::null());

// ✅ 解决：调用 execute 前先检查 can_execute
if (save_command->can_execute(core::Variant::null())) {
    save_command->execute(core::Variant::null());
}
```

---

### 2. ViewModel 状态变化不触发 can_execute_changed

```cpp
// ❌ 问题：状态变化不触发通知
void set_dirty(bool dirty) {
    is_dirty_ = dirty;
    // ❌ 忘记触发通知，UI 不会刷新
}

// UI 元素状态不会更新
button.set_enabled(save_command->can_execute(core::Variant::null()));
// ❌ 即使 is_dirty_ 变化，button 仍保持旧状态

// ✅ 解决：状态变化时触发通知
void set_dirty(bool dirty) {
    if (is_dirty_ != dirty) {
        is_dirty_ = dirty;
        save_command_.raise_can_execute_changed();  // ✅ 触发通知
    }
}
```

---

### 3. 忘记取消订阅

```cpp
// ❌ 问题：忘记取消订阅
class Button {
public:
    void set_command(ICommand* command) {
        command_ = command;
        token_ = command_->subscribe_can_execute_changed(
            on_can_execute_changed, this);
        // ❌ 忘记取消订阅
    }

private:
    ICommand* command_ = nullptr;
    uint32_t token_ = 0;
};

// Button 销毁后，command_ 仍持有 Button 的指针
// 当 command_ 触发 can_execute_changed 时，访问野指针！

// ✅ 解决：UI 元素销毁时取消订阅
class Button {
public:
    ~Button() {
        if (command_ && token_ != 0) {
            command_->unsubscribe_can_execute_changed(token_);  // ✅ 取消订阅
        }
    }
};
```

---

### 4. execute 抛出异常

```cpp
// ❌ 问题：execute 抛出异常（但声明为 noexcept）
void execute(const core::Variant& param) noexcept override {
    // ❌ 可能抛出异常
    do_something_risky();
}

// 结果：std::terminate() 被调用，程序崩溃

// ✅ 解决：捕获并处理异常
void execute(const core::Variant& param) noexcept override {
    try {
        do_something_risky();
    } catch (...) {
        std::cerr << "Command execution failed" << std::endl;
    }
}
```

---

## 完整示例

```cpp
#include <mine/ui/event/ICommand.h>
#include <mine/ui/event/RelayCommand.h>
#include <mine/ui/event/RoutedEvent.h>
#include <iostream>

using namespace mine::ui;

// ────────────────────────────────────────────────────────────────────────────
// ViewModel 定义
// ────────────────────────────────────────────────────────────────────────────

class MyViewModel {
public:
    MyViewModel()
        : save_command_{
            [this](const core::Variant&) { do_save(); },
            [this](const core::Variant&) { return is_dirty_; }
          },
          undo_command_{
            [this](const core::Variant&) { do_undo(); },
            [this](const core::Variant&) { return can_undo(); }
          }
    {}

    ICommand* save_command() { return &save_command_; }
    ICommand* undo_command() { return &undo_command_; }

    void set_dirty(bool dirty) {
        if (is_dirty_ != dirty) {
            is_dirty_ = dirty;
            std::cout << "Dirty state changed: " << (is_dirty_ ? "Yes" : "No") << std::endl;
            save_command_.raise_can_execute_changed();
        }
    }

private:
    void do_save() {
        std::cout << "Saving..." << std::endl;
        is_dirty_ = false;
        save_command_.raise_can_execute_changed();
    }

    void do_undo() {
        std::cout << "Undo..." << std::endl;
        undo_stack_.clear();
        undo_command_.raise_can_execute_changed();
    }

    bool can_undo() const {
        return !undo_stack_.empty();
    }

    RelayCommand save_command_;
    RelayCommand undo_command_;
    bool is_dirty_ = false;
    std::vector<int> undo_stack_;
};

// ────────────────────────────────────────────────────────────────────────────
// UI 元素定义（简化）
// ────────────────────────────────────────────────────────────────────────────

class Button {
public:
    explicit Button(const char* name) : name_(name) {}

    ~Button() {
        if (command_ && token_ != 0) {
            command_->unsubscribe_can_execute_changed(token_);
        }
    }

    void set_command(ICommand* command) {
        if (command_ && token_ != 0) {
            command_->unsubscribe_can_execute_changed(token_);
        }
        
        command_ = command;
        
        if (command_) {
            token_ = command_->subscribe_can_execute_changed(
                on_can_execute_changed, this);
            set_enabled(command_->can_execute(core::Variant::null()));
        }
    }

    void set_enabled(bool enabled) {
        enabled_ = enabled;
        std::cout << "[" << name_ << "] Enabled: " << (enabled_ ? "Yes" : "No") << std::endl;
    }

    void click() {
        std::cout << "[" << name_ << "] Clicked" << std::endl;
        if (command_ && enabled_) {
            if (command_->can_execute(core::Variant::null())) {
                command_->execute(core::Variant::null());
            }
        }
    }

private:
    static void on_can_execute_changed(ICommand* sender, void* user_data) {
        auto* button = static_cast<Button*>(user_data);
        bool can_exec = sender->can_execute(core::Variant::null());
        button->set_enabled(can_exec);
    }

    const char* name_;
    ICommand* command_ = nullptr;
    uint32_t token_ = 0;
    bool enabled_ = false;
};

// ────────────────────────────────────────────────────────────────────────────
// 使用示例
// ────────────────────────────────────────────────────────────────────────────

int main() {
    MyViewModel view_model;
    Button save_button("Save");
    Button undo_button("Undo");
    
    // 绑定命令
    save_button.set_command(view_model.save_command());
    undo_button.set_command(view_model.undo_command());
    
    // 初始状态
    std::cout << "════════════════════════════════════════════════" << std::endl;
    std::cout << "初始状态" << std::endl;
    std::cout << "════════════════════════════════════════════════" << std::endl;
    // 输出：
    // [Save] Enabled: No（is_dirty_ = false）
    // [Undo] Enabled: No（undo_stack_ 为空）
    
    // 用户修改数据
    std::cout << std::endl;
    std::cout << "════════════════════════════════════════════════" << std::endl;
    std::cout << "用户修改数据" << std::endl;
    std::cout << "════════════════════════════════════════════════" << std::endl;
    view_model.set_dirty(true);
    // 输出：
    // Dirty state changed: Yes
    // [Save] Enabled: Yes（自动刷新）
    
    // 用户点击保存按钮
    std::cout << std::endl;
    std::cout << "════════════════════════════════════════════════" << std::endl;
    std::cout << "用户点击保存按钮" << std::endl;
    std::cout << "════════════════════════════════════════════════" << std::endl;
    save_button.click();
    // 输出：
    // [Save] Clicked
    // Saving...
    // [Save] Enabled: No（自动刷新，is_dirty_ = false）
    
    return 0;
}
```

---

## 总结

`ICommand` 是 `mine.ui.event` 模块的命令接口，具备：

1. **MVVM 命令模式**：遵循 WPF/MVVM 命令模式
2. **可执行状态查询**：`can_execute()` 判断命令是否可执行
3. **命令执行**：`execute()` 执行命令逻辑
4. **状态变化通知**：`subscribe_can_execute_changed()` 订阅可执行状态变化
5. **UI 框架解耦**：UI 元素通过此接口与 ViewModel 的命令逻辑解耦

**使用建议**：
- 调用 `execute` 前先检查 `can_execute`（避免在禁用状态下执行）
- ViewModel 状态变化时触发 `can_execute_changed`（UI 自动刷新）
- UI 元素销毁时取消订阅（避免野指针）
- `execute` 不抛出异常（声明为 `noexcept`）
- 使用 `RelayCommand` 实现命令（简化命令定义）
