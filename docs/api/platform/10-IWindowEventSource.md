# IWindowEventSource 详细接口文档

## 概述

`IWindowEventSource` 是 `mine.platform.abi` 模块的**窗口事件分发器接口**（被观察者），持有一组 `IWindowEventSink` 注册表，当窗口状态发生变化时，依次通知所有已订阅的接收器。

**核心特性：**
- **观察者模式**：维护订阅列表，分发事件到所有订阅者
- **自动去重**：同一 sink 重复订阅不会产生重复通知
- **安全取消订阅**：取消未订阅的 sink 为安全操作（静默忽略）
- **与 IWindow 配合**：通过 `IWindow::events()` 获取

---

## 文件位置

```
src/mine/platform/abi/api/include/mine/platform/IWindowEventSource.h
```

---

## 类型定义

```cpp
namespace mine::platform {

class IWindowEventSource {
public:
    virtual ~IWindowEventSource() = default;
    
    /**
     * @brief 订阅窗口事件。
     * @param sink 事件接收器指针（不得为 nullptr，生命周期须长于订阅期）
     * @note  同一 sink 重复订阅不会产生重复通知。
     */
    virtual void subscribe(IWindowEventSink* sink) = 0;
    
    /**
     * @brief 取消订阅窗口事件。
     * @param sink 之前通过 subscribe 注册的接收器指针
     * @note  取消未订阅的 sink 为安全操作（静默忽略）。
     */
    virtual void unsubscribe(IWindowEventSink* sink) = 0;
};

}
```

---

## 成员方法

### subscribe()

```cpp
virtual void subscribe(IWindowEventSink* sink) = 0;
```

**描述**：订阅窗口事件。

**参数**：
- `sink`：事件接收器指针（不得为 `nullptr`，生命周期须长于订阅期）

**特征**：
- **自动去重**：同一 `sink` 重复订阅不会产生重复通知
- **立即生效**：订阅后立即开始接收事件
- **线程安全**：平台实现保证线程安全（内部加锁）

**注意**：
- `sink` 指针必须有效（非 `nullptr`）
- `sink` 生命周期须长于订阅期（取消订阅前不得销毁）

**示例**：
```cpp
class Application : public IWindowEventSink {
public:
    void init(IWindow* window) {
        // 订阅窗口事件
        window->events().subscribe(this);
    }
    
    void on_window_event(WindowEvent& event) override {
        log("收到事件: {}", static_cast<int>(event.kind));
    }
};
```

---

### unsubscribe()

```cpp
virtual void unsubscribe(IWindowEventSink* sink) = 0;
```

**描述**：取消订阅窗口事件。

**参数**：
- `sink`：之前通过 `subscribe()` 注册的接收器指针

**特征**：
- **安全操作**：取消未订阅的 `sink` 为安全操作（静默忽略）
- **立即生效**：取消订阅后立即停止接收事件
- **线程安全**：平台实现保证线程安全（内部加锁）

**注意**：
- 取消订阅后不再接收事件
- 可多次调用（幂等操作）

**示例**：
```cpp
class Application : public IWindowEventSink {
public:
    void cleanup(IWindow* window) {
        // 取消订阅
        window->events().unsubscribe(this);
    }
};
```

---

## 使用场景

### 1. 基本订阅和取消订阅

```cpp
IWindow* window = host->create_window(desc).release();
IWindowEventSink* sink = new MyEventSink();

// 订阅
window->events().subscribe(sink);

// 取消订阅
window->events().unsubscribe(sink);

delete sink;
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

### 3. 多订阅者

```cpp
IWindow* window = host->create_window(desc).release();

IWindowEventSink* logger = new EventLogger();
IWindowEventSink* renderer = new Renderer();
IWindowEventSink* input_manager = new InputManager();

// 订阅多个接收器
window->events().subscribe(logger);
window->events().subscribe(renderer);
window->events().subscribe(input_manager);

// 窗口事件触发时，依次通知所有订阅者
// logger->on_window_event(event)
// renderer->on_window_event(event)
// input_manager->on_window_event(event)
```

---

### 4. 临时订阅

```cpp
class TempListener : public IWindowEventSink {
    IWindowEventSource* source_;
    
public:
    TempListener(IWindowEventSource* source) : source_(source) {
        source_->subscribe(this);
    }
    
    ~TempListener() {
        source_->unsubscribe(this);
    }
    
    void on_window_event(WindowEvent& event) override {
        if (event.kind == WindowEventKind::Resized) {
            log("尺寸变化: {}x{}", event.new_size.width, event.new_size.height);
        }
    }
};

// 临时监听尺寸变化
{
    TempListener listener(&window->events());
    // 在此作用域内接收事件
}
// 作用域结束，自动取消订阅
```

---

### 5. 事件转发

```cpp
class EventForwarder : public IWindowEventSink {
    std::vector<IWindowEventSink*> sinks_;
    
public:
    void add_sink(IWindowEventSink* sink) {
        sinks_.push_back(sink);
    }
    
    void on_window_event(WindowEvent& event) override {
        // 转发给所有 sink
        for (auto* sink : sinks_) {
            sink->on_window_event(event);
        }
    }
};

// 使用
EventForwarder forwarder;
forwarder.add_sink(&sink1);
forwarder.add_sink(&sink2);
window->events().subscribe(&forwarder);
```

---

## 订阅管理

### 重复订阅

**行为**：自动去重，不会产生重复通知。

**示例**：
```cpp
IWindowEventSink* sink = new MyEventSink();

// 重复订阅
window->events().subscribe(sink);
window->events().subscribe(sink);  // 自动忽略
window->events().subscribe(sink);  // 自动忽略

// 只会收到一次通知
```

---

### 取消未订阅的 sink

**行为**：安全操作，静默忽略。

**示例**：
```cpp
IWindowEventSink* sink = new MyEventSink();

// 取消未订阅的 sink
window->events().unsubscribe(sink);  // 静默忽略

// 多次取消订阅
window->events().subscribe(sink);
window->events().unsubscribe(sink);
window->events().unsubscribe(sink);  // 安全，静默忽略
```

---

### 订阅顺序

**行为**：通知顺序未定义（实现依赖）。

**示例**：
```cpp
window->events().subscribe(&sink1);
window->events().subscribe(&sink2);
window->events().subscribe(&sink3);

// 事件触发时，可能按任意顺序通知：
// sink1, sink2, sink3
// 或 sink3, sink1, sink2
// 等等
```

**建议**：
- 不依赖订阅者的通知顺序
- 需要顺序时使用单一 sink 协调

---

## 线程安全

### 线程安全保证

**特征**：
- `subscribe()` 和 `unsubscribe()` 线程安全（内部加锁）
- 可在任意线程订阅/取消订阅
- 事件分发在主线程（UI 线程）执行

**示例**：
```cpp
// 在后台线程订阅
std::thread([window, sink]() {
    window->events().subscribe(sink);  // 线程安全
}).join();

// 事件回调在主线程执行
void on_window_event(WindowEvent& event) override {
    assert(is_main_thread());
}
```

---

## 最佳实践

### 1. 使用 RAII 管理订阅

```cpp
// ✅ 推荐：RAII 自动管理
class Application : public IWindowEventSink {
    ScopedEventSubscription subscription_;
    
public:
    Application(IWindow* window)
        : subscription_(&window->events(), this) {}
};

// ❌ 不推荐：手动管理（容易忘记）
class Application : public IWindowEventSink {
public:
    ~Application() {
        window_->events().unsubscribe(this);  // 容易忘记
    }
};
```

---

### 2. 不依赖订阅者通知顺序

```cpp
// ❌ 错误：依赖通知顺序
window->events().subscribe(&sink_a);  // 假设 A 先通知
window->events().subscribe(&sink_b);  // 假设 B 后通知

// ✅ 正确：不依赖顺序，或使用单一 sink 协调
class CoordinatorSink : public IWindowEventSink {
    SinkA* sink_a_;
    SinkB* sink_b_;
    
    void on_window_event(WindowEvent& event) override {
        sink_a_->handle(event);  // 显式控制顺序
        sink_b_->handle(event);
    }
};
```

---

### 3. sink 生命周期长于订阅期

```cpp
// ❌ 错误：sink 生命周期短于订阅期
{
    MyEventSink sink;
    window->events().subscribe(&sink);
}
// sink 已销毁，但窗口仍持有指针
// 下次事件触发时崩溃

// ✅ 正确：取消订阅或延长生命周期
{
    MyEventSink sink;
    window->events().subscribe(&sink);
    // ...
    window->events().unsubscribe(&sink);
}
// 或使用堆分配
MyEventSink* sink = new MyEventSink();
window->events().subscribe(sink);
// ...
window->events().unsubscribe(sink);
delete sink;
```

---

### 4. 多次取消订阅安全

```cpp
// ✅ 多次取消订阅安全
window->events().unsubscribe(&sink);
window->events().unsubscribe(&sink);  // 安全，静默忽略
window->events().unsubscribe(&sink);  // 安全，静默忽略
```

---

## 常见陷阱

### 1. sink 生命周期短于订阅期

```cpp
// ❌ 问题：sink 已销毁但未取消订阅
void create_temp_sink(IWindow* window) {
    MyEventSink sink;
    window->events().subscribe(&sink);
    // 函数返回，sink 销毁，但窗口仍持有指针
}
// 下次事件触发时崩溃

// ✅ 解决：取消订阅
void create_temp_sink(IWindow* window) {
    MyEventSink sink;
    window->events().subscribe(&sink);
    // ...
    window->events().unsubscribe(&sink);
}
```

---

### 2. 在回调中取消订阅自身

```cpp
// ❌ 问题：回调中取消订阅自身（可能导致迭代器失效）
class Application : public IWindowEventSink {
public:
    void on_window_event(WindowEvent& event) override {
        if (event.kind == WindowEventKind::Closed) {
            window_->events().unsubscribe(this);  // 可能导致迭代器失效
        }
    }
};

// ✅ 解决：异步取消订阅
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

### 3. 多线程同时订阅/取消订阅

```cpp
// ⚠️ 注意：虽然线程安全，但可能产生竞态条件

// 线程 1
window->events().subscribe(&sink);

// 线程 2
window->events().unsubscribe(&sink);

// 竞态条件：不确定最终是否订阅

// ✅ 建议：在主线程管理订阅
```

---

### 4. 订阅 nullptr

```cpp
// ❌ 错误：订阅 nullptr
window->events().subscribe(nullptr);  // 未定义行为

// ✅ 正确：检查指针有效性
if (sink != nullptr) {
    window->events().subscribe(sink);
}
```

---

## 完整示例

```cpp
#include <mine/platform/IWindow.h>
#include <mine/platform/IWindowEventSource.h>
#include <mine/platform/IWindowEventSink.h>

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
    
    ScopedEventSubscription(const ScopedEventSubscription&) = delete;
    ScopedEventSubscription& operator=(const ScopedEventSubscription&) = delete;
};

class Application : public IWindowEventSink {
    IWindow* window_;
    ScopedEventSubscription subscription_;
    bool has_unsaved_data_ = false;
    
public:
    Application(IWindow* window)
        : window_(window)
        , subscription_(&window->events(), this) {
        // 自动订阅
    }
    
    // 析构时自动取消订阅
    
    void on_window_event(WindowEvent& event) override {
        switch (event.kind) {
        case WindowEventKind::Resized:
            handle_resize(event.new_size);
            break;
            
        case WindowEventKind::Closing:
            if (has_unsaved_data_) {
                event.cancel = true;
                show_save_dialog();
            }
            break;
            
        case WindowEventKind::Closed:
            cleanup();
            break;
            
        default:
            break;
        }
    }
    
private:
    void handle_resize(math::Size new_size) {
        log("尺寸变化: {}x{}", new_size.width, new_size.height);
    }
    
    void show_save_dialog() {
        // 显示保存对话框
        // 用户确认后：
        has_unsaved_data_ = false;
        window_->close();
    }
    
    void cleanup() {
        log("清理资源");
        window_ = nullptr;
    }
};

int main() {
    IApplicationHost* host = /* ... */;
    WindowDesc desc;
    auto window = host->create_window(desc);
    
    // 创建应用实例（自动订阅）
    Application app(window.get());
    
    // 运行消息循环
    host->run();
    
    // app 析构时自动取消订阅
    return 0;
}
```

---

## 总结

`IWindowEventSource` 是 `mine.platform.abi` 模块的窗口事件分发器接口，具备：

1. **观察者模式**：维护订阅列表，分发事件到所有订阅者
2. **自动去重**：同一 sink 重复订阅不会产生重复通知
3. **安全取消订阅**：取消未订阅的 sink 为安全操作（静默忽略）
4. **与 IWindow 配合**：通过 `IWindow::events()` 获取

**使用建议**：
- 使用 RAII 自动管理订阅（避免忘记取消订阅）
- 不依赖订阅者的通知顺序
- sink 生命周期须长于订阅期
- 多次取消订阅是安全的（幂等操作）
- 不在回调中取消订阅自身（可能导致迭代器失效）
- `subscribe()` 和 `unsubscribe()` 线程安全
- 事件回调在主线程（UI 线程）执行
