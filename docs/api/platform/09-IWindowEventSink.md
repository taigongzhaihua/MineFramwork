# IWindowEventSink 详细接口文档

## 概述

`IWindowEventSink` 是 `mine.platform.abi` 模块的**窗口事件接收器接口**（观察者），实现此接口并通过 `IWindow::events().subscribe()` 注册，即可接收窗口产生的所有事件。

**核心特性：**
- **观察者模式**：订阅窗口事件，自动接收通知
- **单一回调**：`on_window_event(WindowEvent&)` 接收所有事件
- **主线程回调**：回调发生在平台主线程（UI 线程）
- **可修改事件**：`Closing` 事件可通过设置 `event.cancel = true` 阻止窗口关闭

---

## 文件位置

```
src/mine/platform/abi/api/include/mine/platform/IWindowEventSink.h
```

---

## 类型定义

```cpp
namespace mine::platform {

class IWindowEventSink {
public:
    virtual ~IWindowEventSink() = default;
    
    /**
     * @brief 窗口事件回调。
     * @param event 事件数据（可修改 event.cancel 以取消 Closing 事件）
     */
    virtual void on_window_event(WindowEvent& event) = 0;
};

}
```

---

## 成员方法

### on_window_event()

```cpp
virtual void on_window_event(WindowEvent& event) = 0;
```

**描述**：窗口事件回调。

**参数**：
- `event`：事件数据（引用类型，可修改）

**特征**：
- **所有事件**：通过 `event.kind` 区分事件类型
- **主线程回调**：发生在平台主线程（通常为应用主线程）
- **可修改事件**：`Closing` 事件可设置 `event.cancel = true` 阻止关闭
- **不得销毁 sink**：回调中不得销毁或取消订阅自身

**示例**：
```cpp
class MyEventSink : public IWindowEventSink {
public:
    void on_window_event(WindowEvent& event) override {
        switch (event.kind) {
        case WindowEventKind::Resized:
            handle_resize(event.new_size);
            break;
        case WindowEventKind::Closing:
            if (has_unsaved_data()) {
                event.cancel = true;  // 阻止关闭
                show_save_dialog();
            }
            break;
        case WindowEventKind::KeyDown:
            handle_key_down(event);
            break;
        default:
            break;
        }
    }
};
```

---

## 使用场景

### 1. 基本订阅和取消订阅

```cpp
class Application : public IWindowEventSink {
    IWindow* window_;
    
public:
    void init() {
        // 订阅窗口事件
        window_->events().subscribe(this);
    }
    
    void cleanup() {
        // 取消订阅
        window_->events().unsubscribe(this);
    }
    
    void on_window_event(WindowEvent& event) override {
        // 处理事件
        log("收到事件: {}", static_cast<int>(event.kind));
    }
};
```

---

### 2. RAII 自动管理订阅

```cpp
class ScopedEventSubscription {
    IWindowEventSource* source_;
    IWindowEventSink* sink_;
    
public:
    ScopedEventSubscription(IWindowEventSource* source, IWindowEventSink* sink)
        : source_(source), sink_(sink) {
        source_->subscribe(sink_);
    }
    
    ~ScopedEventSubscription() {
        source_->unsubscribe(sink_);
    }
    
    // 禁止拷贝和移动
    ScopedEventSubscription(const ScopedEventSubscription&) = delete;
    ScopedEventSubscription& operator=(const ScopedEventSubscription&) = delete;
};

// 使用
class Application : public IWindowEventSink {
    ScopedEventSubscription subscription_;
    
public:
    Application(IWindow* window)
        : subscription_(&window->events(), this) {
        // 自动订阅
    }
    
    // 析构时自动取消订阅
    
    void on_window_event(WindowEvent& event) override {
        // 处理事件
    }
};
```

---

### 3. 多窗口事件处理

```cpp
class MultiWindowManager : public IWindowEventSink {
    std::map<IWindow*, WindowState> windows_;
    
public:
    void add_window(IWindow* window) {
        windows_[window] = WindowState{};
        window->events().subscribe(this);
    }
    
    void remove_window(IWindow* window) {
        window->events().unsubscribe(this);
        windows_.erase(window);
    }
    
    void on_window_event(WindowEvent& event) override {
        // 识别事件来源窗口（需要平台实现传递窗口指针）
        log("窗口事件: {}", static_cast<int>(event.kind));
    }
};
```

---

### 4. 事件过滤器

```cpp
class EventFilter : public IWindowEventSink {
    std::function<bool(const WindowEvent&)> predicate_;
    std::function<void(const WindowEvent&)> handler_;
    
public:
    EventFilter(std::function<bool(const WindowEvent&)> predicate,
                std::function<void(const WindowEvent&)> handler)
        : predicate_(std::move(predicate))
        , handler_(std::move(handler)) {}
    
    void on_window_event(WindowEvent& event) override {
        if (predicate_(event)) {
            handler_(event);
        }
    }
};

// 使用：仅处理 Resized 事件
EventFilter resize_filter(
    [](const WindowEvent& e) { return e.kind == WindowEventKind::Resized; },
    [](const WindowEvent& e) { log("尺寸变化: {}x{}", e.new_size.width, e.new_size.height); }
);
window->events().subscribe(&resize_filter);
```

---

### 5. 阻止窗口关闭

```cpp
class DocumentEditor : public IWindowEventSink {
    bool has_unsaved_changes_ = false;
    
public:
    void on_window_event(WindowEvent& event) override {
        if (event.kind == WindowEventKind::Closing) {
            if (has_unsaved_changes_) {
                event.cancel = true;  // 阻止关闭
                show_save_confirmation_dialog();
            }
        }
    }
    
private:
    void show_save_confirmation_dialog() {
        // 显示保存确认对话框
        // 用户选择保存后设置 has_unsaved_changes_ = false
        // 然后调用 window->close() 手动关闭
    }
};
```

---

### 6. 事件日志记录

```cpp
class EventLogger : public IWindowEventSink {
public:
    void on_window_event(WindowEvent& event) override {
        switch (event.kind) {
        case WindowEventKind::Resized:
            log("Resized: {}x{}", event.new_size.width, event.new_size.height);
            break;
        case WindowEventKind::Moved:
            log("Moved: ({}, {})", event.new_position.x, event.new_position.y);
            break;
        case WindowEventKind::DpiChanged:
            log("DpiChanged: {} → {}", current_dpi_, event.new_dpi);
            current_dpi_ = event.new_dpi;
            break;
        case WindowEventKind::KeyDown:
            log("KeyDown: VK={}, Repeat={}", event.key_vk_code, event.key_is_repeat);
            break;
        case WindowEventKind::MouseMove:
            log("MouseMove: ({}, {})", event.mouse_x, event.mouse_y);
            break;
        default:
            log("Event: {}", static_cast<int>(event.kind));
            break;
        }
    }
    
private:
    float current_dpi_ = 96.0f;
};
```

---

## 线程模型

### 主线程回调

**特征**：
- `on_window_event()` 在平台主线程（UI 线程）被调用
- Windows：窗口消息循环线程
- macOS：主 RunLoop 线程
- Linux：X11 事件循环线程

**示例**：
```cpp
class Application : public IWindowEventSink {
public:
    void on_window_event(WindowEvent& event) override {
        // 此处在主线程执行
        assert(is_main_thread());
        
        // 可以直接调用 UI API
        window_->set_title("New Title");
        
        // 不需要线程同步
        some_ui_state_ = event.new_size;
    }
};
```

---

### 异步处理

**特征**：
- 事件回调是同步的，阻塞主线程
- 长时间操作应异步处理

**示例**：
```cpp
class Application : public IWindowEventSink {
    async::EventLoop* event_loop_;
    
public:
    void on_window_event(WindowEvent& event) override {
        if (event.kind == WindowEventKind::Closing) {
            if (has_unsaved_data()) {
                event.cancel = true;  // 阻止关闭
                
                // 异步保存数据
                event_loop_->post([this]() {
                    save_data();
                    window_->close();  // 保存完成后手动关闭
                });
            }
        }
    }
};
```

---

## 最佳实践

### 1. 使用 RAII 管理订阅

```cpp
// ✅ 推荐：RAII 自动管理订阅
class Application : public IWindowEventSink {
    ScopedEventSubscription subscription_;
    
public:
    Application(IWindow* window)
        : subscription_(&window->events(), this) {}
    
    // 析构时自动取消订阅
};

// ❌ 不推荐：手动管理订阅（容易忘记取消订阅）
class Application : public IWindowEventSink {
public:
    ~Application() {
        window_->events().unsubscribe(this);  // 容易忘记
    }
};
```

---

### 2. switch-case 处理事件

```cpp
// ✅ 推荐：使用 switch-case
void on_window_event(WindowEvent& event) override {
    switch (event.kind) {
    case WindowEventKind::Resized:
        handle_resize(event);
        break;
    case WindowEventKind::KeyDown:
        handle_key_down(event);
        break;
    default:
        break;
    }
}

// ❌ 不推荐：大量 if-else
void on_window_event(WindowEvent& event) override {
    if (event.kind == WindowEventKind::Resized) {
        handle_resize(event);
    } else if (event.kind == WindowEventKind::KeyDown) {
        handle_key_down(event);
    }
    // ...
}
```

---

### 3. 不在回调中销毁 sink

```cpp
// ❌ 错误：回调中销毁 sink
class Application : public IWindowEventSink {
public:
    void on_window_event(WindowEvent& event) override {
        if (event.kind == WindowEventKind::Closed) {
            delete this;  // 崩溃！
        }
    }
};

// ✅ 正确：回调后异步销毁
class Application : public IWindowEventSink {
    async::EventLoop* event_loop_;
    
public:
    void on_window_event(WindowEvent& event) override {
        if (event.kind == WindowEventKind::Closed) {
            event_loop_->post([this]() {
                delete this;  // 安全
            });
        }
    }
};
```

---

### 4. 不在回调中取消订阅自身

```cpp
// ❌ 错误：回调中取消订阅自身
class Application : public IWindowEventSink {
public:
    void on_window_event(WindowEvent& event) override {
        if (event.kind == WindowEventKind::Closed) {
            window_->events().unsubscribe(this);  // 可能导致迭代器失效
        }
    }
};

// ✅ 正确：回调后异步取消订阅
class Application : public IWindowEventSink {
    async::EventLoop* event_loop_;
    
public:
    void on_window_event(WindowEvent& event) override {
        if (event.kind == WindowEventKind::Closed) {
            event_loop_->post([this]() {
                window_->events().unsubscribe(this);
            });
        }
    }
};
```

---

### 5. 长时间操作异步处理

```cpp
// ❌ 错误：回调中执行长时间操作
void on_window_event(WindowEvent& event) override {
    if (event.kind == WindowEventKind::Closing) {
        save_large_file();  // 阻塞主线程
    }
}

// ✅ 正确：异步处理
void on_window_event(WindowEvent& event) override {
    if (event.kind == WindowEventKind::Closing) {
        event.cancel = true;  // 阻止关闭
        
        std::thread([this]() {
            save_large_file();
            // 保存完成后关闭窗口
            window_->close();
        }).detach();
    }
}
```

---

## 常见陷阱

### 1. 忘记取消订阅

```cpp
// ❌ 问题：忘记取消订阅，sink 已销毁但仍被调用
class TempSink : public IWindowEventSink {
public:
    ~TempSink() {
        // 忘记取消订阅
    }
};

TempSink sink;
window->events().subscribe(&sink);
// sink 析构，但窗口仍持有指针
// 下次事件触发时崩溃

// ✅ 解决：析构时取消订阅
class TempSink : public IWindowEventSink {
    IWindow* window_;
    
public:
    TempSink(IWindow* window) : window_(window) {
        window_->events().subscribe(this);
    }
    
    ~TempSink() {
        window_->events().unsubscribe(this);
    }
};
```

---

### 2. 多次订阅同一 sink

```cpp
// ❌ 问题：多次订阅同一 sink（浪费资源）
window->events().subscribe(&sink);
window->events().subscribe(&sink);  // 重复订阅

// ✅ 解决：检查是否已订阅
// 平台实现：subscribe() 自动去重，不会重复通知
window->events().subscribe(&sink);
window->events().subscribe(&sink);  // 自动忽略
```

---

### 3. 回调中销毁窗口

```cpp
// ❌ 问题：回调中销毁窗口
void on_window_event(WindowEvent& event) override {
    if (event.kind == WindowEventKind::Closing) {
        delete window_;  // 崩溃：回调返回后访问已销毁的窗口
    }
}

// ✅ 解决：异步销毁
void on_window_event(WindowEvent& event) override {
    if (event.kind == WindowEventKind::Closed) {
        event_loop_->post([this]() {
            delete window_;
        });
    }
}
```

---

### 4. 事件处理顺序依赖

```cpp
// ❌ 问题：多个 sink 的处理顺序未定义
class SinkA : public IWindowEventSink {
    void on_window_event(WindowEvent& event) override {
        if (event.kind == WindowEventKind::Resized) {
            // 假设 SinkB 已处理
        }
    }
};

class SinkB : public IWindowEventSink {
    void on_window_event(WindowEvent& event) override {
        if (event.kind == WindowEventKind::Resized) {
            // 处理逻辑
        }
    }
};

// ✅ 解决：不依赖 sink 处理顺序，或使用单一 sink 协调
class CoordinatorSink : public IWindowEventSink {
    SinkA* sink_a_;
    SinkB* sink_b_;
    
    void on_window_event(WindowEvent& event) override {
        if (event.kind == WindowEventKind::Resized) {
            sink_b_->handle(event);  // 先处理 B
            sink_a_->handle(event);  // 再处理 A
        }
    }
};
```

---

## 完整示例

```cpp
#include <mine/platform/IWindow.h>
#include <mine/platform/IWindowEventSink.h>
#include <mine/platform/WindowEvent.h>

class Application : public IWindowEventSink {
    IWindow* window_;
    bool has_unsaved_data_ = false;
    
public:
    Application(IWindow* window) : window_(window) {
        // 订阅窗口事件
        window_->events().subscribe(this);
    }
    
    ~Application() {
        // 取消订阅
        window_->events().unsubscribe(this);
    }
    
    void on_window_event(WindowEvent& event) override {
        switch (event.kind) {
        case WindowEventKind::Resized:
            handle_resize(event.new_size);
            break;
            
        case WindowEventKind::Moved:
            log("窗口移动到: ({}, {})", event.new_position.x, event.new_position.y);
            break;
            
        case WindowEventKind::Closing:
            if (has_unsaved_data_) {
                event.cancel = true;  // 阻止关闭
                show_save_confirmation_dialog();
            }
            break;
            
        case WindowEventKind::Closed:
            cleanup();
            break;
            
        case WindowEventKind::DpiChanged:
            handle_dpi_change(event.new_dpi, event.suggested_rect);
            break;
            
        case WindowEventKind::KeyDown:
            if (!event.key_is_repeat) {
                handle_shortcut(event);
            }
            break;
            
        case WindowEventKind::Char:
            insert_char(event.char_utf32);
            break;
            
        case WindowEventKind::MouseDown:
            handle_mouse_down({event.mouse_x, event.mouse_y}, event.mouse_button);
            break;
            
        default:
            break;
        }
    }
    
private:
    void handle_resize(math::Size new_size) {
        log("窗口尺寸变化: {}x{}", new_size.width, new_size.height);
        renderer_->resize(new_size);
    }
    
    void handle_dpi_change(float new_dpi, math::Rect suggested_rect) {
        log("DPI 变化: {} → {}", current_dpi_, new_dpi);
        window_->set_position(suggested_rect.position());
        window_->set_size(suggested_rect.size());
        reload_resources(new_dpi);
        current_dpi_ = new_dpi;
    }
    
    void handle_shortcut(const WindowEvent& event) {
        if (event.mod_ctrl && event.key_vk_code == VK_S) {
            save();
        } else if (event.mod_ctrl && event.key_vk_code == VK_Z) {
            undo();
        }
    }
    
    void show_save_confirmation_dialog() {
        // 显示对话框
        // 用户确认后：
        has_unsaved_data_ = false;
        window_->close();
    }
    
    void cleanup() {
        log("清理资源");
        window_ = nullptr;
    }
    
    float current_dpi_ = 96.0f;
};
```

---

## 总结

`IWindowEventSink` 是 `mine.platform.abi` 模块的窗口事件接收器接口，具备：

1. **观察者模式**：订阅窗口事件，自动接收通知
2. **单一回调**：`on_window_event(WindowEvent&)` 接收所有事件
3. **主线程回调**：回调发生在平台主线程（UI 线程）
4. **可修改事件**：`Closing` 事件可通过设置 `event.cancel = true` 阻止窗口关闭

**使用建议**：
- 使用 RAII 自动管理订阅（避免忘记取消订阅）
- 使用 `switch-case` 处理事件
- 不在回调中销毁 sink 或取消订阅自身
- 长时间操作异步处理（避免阻塞主线程）
- 不依赖多个 sink 的处理顺序
- `Closed` 事件后清除 `IWindow*` 指针
