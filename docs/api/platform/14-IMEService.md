# IMEService 详细接口文档

## 概述

`IMEService` 是 `mine.platform.abi` 模块的**输入法（IME）服务接口**，控制 IME 候选框位置与状态。

**核心特性：**
- **4 个方法**：enable、disable、is_enabled、set_composition_rect
- **候选框定位**：控制 IME 候选框位置
- **占位接口**：M0 阶段为占位接口，方法体以 `MINE_TODO_NOT_IMPLEMENTED` 标记
- **完整实现**：在 `mine.ui.input` 模块（L4）中与输入事件流对接

---

## 文件位置

```
src/mine/platform/abi/api/include/mine/platform/IMEService.h
```

---

## 类型定义

```cpp
namespace mine::platform {

class IMEService {
public:
    virtual ~IMEService() = default;
    
    /**
     * @brief 启用 IME 输入（接收组合输入事件）。
     * @param composition_rect 候选框的建议位置（逻辑像素，窗口坐标系）
     */
    virtual void enable(math::Rect composition_rect) = 0;
    
    /// 禁用 IME 输入（如纯英文输入框）
    virtual void disable() = 0;
    
    /// 当前 IME 是否处于启用状态
    [[nodiscard]] virtual bool is_enabled() const = 0;
    
    /// 更新候选框位置（光标移动时调用）
    virtual void set_composition_rect(math::Rect composition_rect) = 0;
};

}
```

---

## 成员方法

### enable()

```cpp
virtual void enable(math::Rect composition_rect) = 0;
```

**描述**：启用 IME 输入（接收组合输入事件）。

**参数**：
- `composition_rect`：候选框的建议位置（逻辑像素，窗口坐标系）

**特征**：
- 启用 IME 后开始接收组合输入事件
- `composition_rect` 指定候选框位置（相对于窗口左上角）
- 逻辑像素坐标

**示例**：
```cpp
IMEService* ime_service = host->ime_service();

// 启用 IME，候选框位于光标位置
math::Rect composition_rect = {
    cursor_x, cursor_y,  // 光标位置
    200, 30              // 候选框尺寸
};
ime_service->enable(composition_rect);
```

---

### disable()

```cpp
virtual void disable() = 0;
```

**描述**：禁用 IME 输入（如纯英文输入框）。

**特征**：
- 禁用 IME 后停止接收组合输入事件
- 适用于纯英文输入框

**示例**：
```cpp
IMEService* ime_service = host->ime_service();
ime_service->disable();
```

---

### is_enabled()

```cpp
[[nodiscard]] virtual bool is_enabled() const = 0;
```

**描述**：当前 IME 是否处于启用状态。

**返回**：`true` 表示启用，`false` 表示禁用

**示例**：
```cpp
IMEService* ime_service = host->ime_service();
if (ime_service->is_enabled()) {
    log("IME 已启用");
} else {
    log("IME 已禁用");
}
```

---

### set_composition_rect()

```cpp
virtual void set_composition_rect(math::Rect composition_rect) = 0;
```

**描述**：更新候选框位置（光标移动时调用）。

**参数**：
- `composition_rect`：候选框的建议位置（逻辑像素，窗口坐标系）

**特征**：
- 光标移动时更新候选框位置
- 逻辑像素坐标

**示例**：
```cpp
IMEService* ime_service = host->ime_service();

// 光标移动，更新候选框位置
void on_cursor_moved(float cursor_x, float cursor_y) {
    math::Rect composition_rect = {
        cursor_x, cursor_y,
        200, 30
    };
    ime_service->set_composition_rect(composition_rect);
}
```

---

## 使用场景

### 1. 文本输入框启用 IME

```cpp
class TextBox {
    IMEService* ime_service_;
    float cursor_x_ = 0.0f;
    float cursor_y_ = 0.0f;
    
public:
    void on_focus_gained() {
        // 启用 IME
        math::Rect composition_rect = {
            cursor_x_, cursor_y_,
            200, 30
        };
        ime_service_->enable(composition_rect);
    }
    
    void on_focus_lost() {
        // 禁用 IME
        ime_service_->disable();
    }
    
    void on_cursor_moved() {
        // 更新候选框位置
        if (ime_service_->is_enabled()) {
            math::Rect composition_rect = {
                cursor_x_, cursor_y_,
                200, 30
            };
            ime_service_->set_composition_rect(composition_rect);
        }
    }
};
```

---

### 2. 纯英文输入框禁用 IME

```cpp
class EmailTextBox {
    IMEService* ime_service_;
    
public:
    void on_focus_gained() {
        // 纯英文输入，禁用 IME
        ime_service_->disable();
    }
};
```

---

### 3. 多行文本编辑器

```cpp
class TextEditor {
    IMEService* ime_service_;
    float cursor_x_ = 0.0f;
    float cursor_y_ = 0.0f;
    float line_height_ = 20.0f;
    
public:
    void on_focus_gained() {
        enable_ime();
    }
    
    void on_focus_lost() {
        ime_service_->disable();
    }
    
    void on_cursor_moved() {
        if (ime_service_->is_enabled()) {
            update_composition_rect();
        }
    }
    
private:
    void enable_ime() {
        math::Rect composition_rect = {
            cursor_x_, 
            cursor_y_ + line_height_,  // 候选框显示在光标下方
            200, 30
        };
        ime_service_->enable(composition_rect);
    }
    
    void update_composition_rect() {
        math::Rect composition_rect = {
            cursor_x_, 
            cursor_y_ + line_height_,
            200, 30
        };
        ime_service_->set_composition_rect(composition_rect);
    }
};
```

---

## IME 事件

### 组合输入事件

**事件类型**：
- `WindowEventKind::ImeCompositionStarted`：IME 组合输入开始
- `WindowEventKind::ImeCompositionChanged`：IME 组合文本变化
- `WindowEventKind::ImeCompositionCommitted`：IME 组合输入提交
- `WindowEventKind::ImeCompositionEnded`：IME 组合输入结束

**示例**：
```cpp
class TextBox : public IWindowEventSink {
public:
    void on_window_event(WindowEvent& event) override {
        switch (event.kind) {
        case WindowEventKind::ImeCompositionStarted:
            handle_composition_started();
            break;
            
        case WindowEventKind::ImeCompositionChanged:
            handle_composition_changed(event);
            break;
            
        case WindowEventKind::ImeCompositionCommitted:
            handle_composition_committed(event);
            break;
            
        case WindowEventKind::ImeCompositionEnded:
            handle_composition_ended();
            break;
            
        default:
            break;
        }
    }
    
private:
    void handle_composition_started() {
        log("IME 组合输入开始");
    }
    
    void handle_composition_changed(const WindowEvent& event) {
        std::string composition_text(
            event.ime_text_utf8, 
            event.ime_text_length);
        log("IME 组合文本: {}", composition_text);
        // 显示组合文本（下划线）
    }
    
    void handle_composition_committed(const WindowEvent& event) {
        std::string committed_text(
            event.ime_text_utf8, 
            event.ime_text_length);
        log("IME 提交文本: {}", committed_text);
        // 插入文本到编辑器
    }
    
    void handle_composition_ended() {
        log("IME 组合输入结束");
    }
};
```

---

## 平台实现

### Windows

```cpp
// Windows 实现使用 ImmSetCompositionWindow API
void IMEService_Win32::enable(math::Rect composition_rect) {
    HIMC hImc = ImmGetContext(hwnd_);
    if (!hImc) {
        return;
    }
    
    COMPOSITIONFORM cf{};
    cf.dwStyle = CFS_POINT;
    cf.ptCurrentPos.x = static_cast<LONG>(composition_rect.x);
    cf.ptCurrentPos.y = static_cast<LONG>(composition_rect.y);
    ImmSetCompositionWindow(hImc, &cf);
    
    ImmReleaseContext(hwnd_, hImc);
}

void IMEService_Win32::set_composition_rect(math::Rect composition_rect) {
    enable(composition_rect);  // 相同实现
}
```

---

### macOS

```objc
// macOS 实现使用 NSTextInputClient
- (NSRect)firstRectForCharacterRange:(NSRange)range 
                         actualRange:(NSRangePointer)actualRange {
    // 返回候选框位置
    return NSMakeRect(
        composition_rect_.x,
        composition_rect_.y,
        composition_rect_.width,
        composition_rect_.height
    );
}

void IMEService_macOS::enable(math::Rect composition_rect) {
    composition_rect_ = composition_rect;
    // NSTextInputClient 自动处理 IME
}
```

---

### Linux (X11/IBus)

```cpp
// Linux 实现使用 IBus
void IMEService_X11::enable(math::Rect composition_rect) {
    if (ibus_context_) {
        // 设置候选框位置
        ibus_input_context_set_cursor_location(
            ibus_context_,
            static_cast<int>(composition_rect.x),
            static_cast<int>(composition_rect.y),
            static_cast<int>(composition_rect.width),
            static_cast<int>(composition_rect.height)
        );
    }
}
```

---

## 最佳实践

### 1. 聚焦时启用 IME

```cpp
// ✅ 推荐：聚焦时启用 IME
void on_focus_gained() {
    math::Rect composition_rect = get_cursor_rect();
    ime_service_->enable(composition_rect);
}

// ❌ 不推荐：忘记启用 IME
void on_focus_gained() {
    // 忘记启用 IME，无法输入中文
}
```

---

### 2. 失焦时禁用 IME

```cpp
// ✅ 推荐：失焦时禁用 IME
void on_focus_lost() {
    ime_service_->disable();
}

// ❌ 不推荐：忘记禁用 IME
void on_focus_lost() {
    // 忘记禁用 IME，候选框可能残留
}
```

---

### 3. 光标移动时更新候选框位置

```cpp
// ✅ 推荐：光标移动时更新候选框位置
void on_cursor_moved() {
    if (ime_service_->is_enabled()) {
        math::Rect composition_rect = get_cursor_rect();
        ime_service_->set_composition_rect(composition_rect);
    }
}

// ❌ 不推荐：忘记更新候选框位置
void on_cursor_moved() {
    // 候选框位置不更新，与光标不同步
}
```

---

### 4. 检查 IME 是否启用

```cpp
// ✅ 推荐：检查 IME 是否启用
if (ime_service_->is_enabled()) {
    // IME 已启用，处理组合输入
}

// ❌ 不推荐：假设 IME 已启用
// 直接处理组合输入，可能崩溃
```

---

## 常见陷阱

### 1. 忘记启用 IME

```cpp
// ❌ 问题：忘记启用 IME
void on_focus_gained() {
    // 忘记启用 IME
}
// 无法输入中文

// ✅ 解决：启用 IME
void on_focus_gained() {
    math::Rect composition_rect = get_cursor_rect();
    ime_service_->enable(composition_rect);
}
```

---

### 2. 忘记禁用 IME

```cpp
// ❌ 问题：忘记禁用 IME
void on_focus_lost() {
    // 忘记禁用 IME
}
// 候选框可能残留

// ✅ 解决：禁用 IME
void on_focus_lost() {
    ime_service_->disable();
}
```

---

### 3. 候选框位置不更新

```cpp
// ❌ 问题：候选框位置不更新
void on_cursor_moved() {
    cursor_x_ += 10.0f;
    // 忘记更新候选框位置
}
// 候选框与光标不同步

// ✅ 解决：更新候选框位置
void on_cursor_moved() {
    cursor_x_ += 10.0f;
    if (ime_service_->is_enabled()) {
        math::Rect composition_rect = get_cursor_rect();
        ime_service_->set_composition_rect(composition_rect);
    }
}
```

---

### 4. 纯英文输入框未禁用 IME

```cpp
// ❌ 问题：纯英文输入框未禁用 IME
class EmailTextBox {
public:
    void on_focus_gained() {
        // 忘记禁用 IME
    }
};
// 用户可能输入中文

// ✅ 解决：禁用 IME
void on_focus_gained() {
    ime_service_->disable();
}
```

---

## 完整示例

```cpp
#include <mine/platform/IApplicationHost.h>
#include <mine/platform/IMEService.h>
#include <mine/platform/IWindow.h>

class TextBox : public IWindowEventSink {
    IMEService* ime_service_;
    IWindow* window_;
    float cursor_x_ = 10.0f;
    float cursor_y_ = 10.0f;
    float line_height_ = 20.0f;
    std::string text_;
    std::string composition_text_;
    
public:
    TextBox(IMEService* ime_service, IWindow* window)
        : ime_service_(ime_service), window_(window) {
        window_->events().subscribe(this);
    }
    
    ~TextBox() {
        window_->events().unsubscribe(this);
    }
    
    void on_focus_gained() {
        // 启用 IME
        math::Rect composition_rect = {
            cursor_x_, 
            cursor_y_ + line_height_,
            200, 30
        };
        ime_service_->enable(composition_rect);
    }
    
    void on_focus_lost() {
        // 禁用 IME
        ime_service_->disable();
    }
    
    void on_cursor_moved(float new_x, float new_y) {
        cursor_x_ = new_x;
        cursor_y_ = new_y;
        
        // 更新候选框位置
        if (ime_service_->is_enabled()) {
            math::Rect composition_rect = {
                cursor_x_, 
                cursor_y_ + line_height_,
                200, 30
            };
            ime_service_->set_composition_rect(composition_rect);
        }
    }
    
    void on_window_event(WindowEvent& event) override {
        switch (event.kind) {
        case WindowEventKind::ImeCompositionStarted:
            composition_text_.clear();
            log("IME 组合输入开始");
            break;
            
        case WindowEventKind::ImeCompositionChanged:
            composition_text_ = std::string(
                event.ime_text_utf8, 
                event.ime_text_length);
            log("IME 组合文本: {}", composition_text_);
            break;
            
        case WindowEventKind::ImeCompositionCommitted:
            {
                std::string committed_text(
                    event.ime_text_utf8, 
                    event.ime_text_length);
                text_ += committed_text;
                composition_text_.clear();
                log("IME 提交文本: {}", committed_text);
            }
            break;
            
        case WindowEventKind::ImeCompositionEnded:
            composition_text_.clear();
            log("IME 组合输入结束");
            break;
            
        default:
            break;
        }
    }
};

int main() {
    auto host = platform::initialize();
    IMEService* ime_service = host->ime_service();
    
    WindowDesc desc;
    auto window = host->create_window(desc);
    
    TextBox textbox(ime_service, window.get());
    textbox.on_focus_gained();
    
    host->run();
    return 0;
}
```

---

## 总结

`IMEService` 是 `mine.platform.abi` 模块的输入法（IME）服务接口，具备：

1. **4 个方法**：enable、disable、is_enabled、set_composition_rect
2. **候选框定位**：控制 IME 候选框位置
3. **占位接口**：M0 阶段为占位接口，方法体以 `MINE_TODO_NOT_IMPLEMENTED` 标记
4. **完整实现**：在 `mine.ui.input` 模块（L4）中与输入事件流对接

**使用建议**：
- 聚焦时启用 IME（`enable()`）
- 失焦时禁用 IME（`disable()`）
- 光标移动时更新候选框位置（`set_composition_rect()`）
- 纯英文输入框禁用 IME
- 检查 IME 是否启用（`is_enabled()`）
- 处理 IME 组合输入事件（`ImeComposition*`）
