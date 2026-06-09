# WindowEvent 详细接口文档

## 概述

`WindowEvent` 是 `mine.platform.abi` 模块的**窗口事件数据结构**，采用扁平结构设计，所有事件字段共存，通过 `kind` 字段区分事件类型。

**核心特性：**
- **扁平结构**：无 `union` 或 `std::variant`，根据 `kind` 判断有效字段
- **21 种事件**：窗口生命周期、IME 输入、键盘、鼠标事件
- **ABI 安全**：POD 类型，跨 DLL 边界传递安全
- **事件合并**：单次回调传递多个相关信息（如 DPI 变化 + 建议窗口矩形）

---

## 文件位置

```
src/mine/platform/abi/api/include/mine/platform/WindowEvent.h
```

---

## 类型定义

### WindowEventKind

```cpp
enum class WindowEventKind : int {
    // 窗口生命周期事件
    Resized,                // 窗口客户区尺寸发生改变
    Moved,                  // 窗口位置发生改变
    Closing,                // 用户请求关闭（可阻止）
    Closed,                 // 窗口已销毁
    FocusGained,            // 窗口获得键盘焦点
    FocusLost,              // 窗口失去键盘焦点
    DpiChanged,             // 每英寸像素数改变
    Activated,              // 窗口切换为前台激活状态
    Deactivated,            // 窗口切换为后台非激活状态
    
    // IME 输入法事件
    ImeCompositionStarted,  // IME 组合输入开始
    ImeCompositionChanged,  // IME 组合字符串更新
    ImeCompositionCommitted,// IME 确认提交
    ImeCompositionEnded,    // IME 组合输入结束
    
    // 键盘输入事件
    KeyDown,                // 键盘按键按下（含重复）
    KeyUp,                  // 键盘按键释放
    Char,                   // 字符输入
    
    // 鼠标输入事件
    MouseMove,              // 鼠标在客户区移动
    MouseDown,              // 鼠标按键按下
    MouseUp,                // 鼠标按键释放
    MouseWheel,             // 鼠标滚轮滚动
};
```

---

### WindowEvent

```cpp
struct WindowEvent {
    WindowEventKind kind{WindowEventKind::Closed};
    
    // Resized 时有效
    math::Size new_size{};
    
    // Moved 时有效
    math::Point new_position{};
    
    // DpiChanged 时有效
    float      new_dpi{96.0f};
    math::Rect suggested_rect{};
    
    // Closing 时可设置为 true 以取消关闭
    mutable bool cancel{false};
    
    // ImeCompositionChanged / ImeCompositionCommitted 时有效
    char     ime_text_utf8[256]{};
    uint32_t ime_text_length{0};
    
    // 键盘事件字段
    uint32_t key_vk_code{0};
    uint32_t key_scan_code{0};
    uint32_t char_utf32{0};
    bool     key_is_repeat{false};
    
    // 鼠标事件字段
    float    mouse_x{0.0f};
    float    mouse_y{0.0f};
    uint8_t  mouse_button{0};
    float    mouse_wheel_delta{0.0f};
    
    // 修饰键状态
    bool mod_ctrl{false};
    bool mod_shift{false};
    bool mod_alt{false};
};
```

---

## 事件字段映射

### 窗口生命周期事件

#### Resized

**有效字段**：`new_size`

**描述**：窗口客户区尺寸发生改变。

**触发时机**：
- 用户拖动边框调整大小
- 程序调用 `IWindow::set_size()`
- 窗口最大化/还原

**示例**：
```cpp
void on_event(const WindowEvent& event) {
    if (event.kind == WindowEventKind::Resized) {
        math::Size new_size = event.new_size;
        log("窗口尺寸变化: {}x{}", new_size.width, new_size.height);
        renderer_->resize(new_size);
    }
}
```

---

#### Moved

**有效字段**：`new_position`

**描述**：窗口位置发生改变。

**触发时机**：
- 用户拖动窗口移动
- 程序调用 `IWindow::set_position()`

**示例**：
```cpp
void on_event(const WindowEvent& event) {
    if (event.kind == WindowEventKind::Moved) {
        math::Point new_position = event.new_position;
        log("窗口位置变化: ({}, {})", new_position.x, new_position.y);
    }
}
```

---

#### Closing

**有效字段**：`cancel`

**描述**：用户请求关闭窗口（点击关闭按钮、Alt+F4）。

**特征**：
- 设置 `cancel = true` 可阻止关闭
- 不阻止时窗口将关闭并触发 `Closed` 事件

**示例**：
```cpp
void on_event(const WindowEvent& event) {
    if (event.kind == WindowEventKind::Closing) {
        // 有未保存的数据，阻止关闭
        if (has_unsaved_data()) {
            event.cancel = true;
            show_save_confirmation_dialog();
        }
    }
}
```

---

#### Closed

**有效字段**：无

**描述**：窗口已销毁，不得再调用 `IWindow` 方法。

**注意**：此后 `IWindow*` 指针失效，必须释放资源。

**示例**：
```cpp
void on_event(const WindowEvent& event) {
    if (event.kind == WindowEventKind::Closed) {
        log("窗口已关闭");
        cleanup_resources();
        window_ = nullptr;  // 清除指针
    }
}
```

---

#### FocusGained / FocusLost

**有效字段**：无

**描述**：窗口获得/失去键盘焦点。

**用途**：
- 暂停/恢复动画
- 更新 UI 视觉状态

**示例**：
```cpp
void on_event(const WindowEvent& event) {
    if (event.kind == WindowEventKind::FocusGained) {
        log("窗口获得焦点");
        resume_animations();
    } else if (event.kind == WindowEventKind::FocusLost) {
        log("窗口失去焦点");
        pause_animations();
    }
}
```

---

#### DpiChanged

**有效字段**：`new_dpi`、`suggested_rect`

**描述**：窗口 DPI 发生改变（Windows 10+ per-monitor DPI v2）。

**特征**：
- `new_dpi`：新的 DPI 值（96 = 100% 缩放）
- `suggested_rect`：系统建议的新窗口矩形（屏幕坐标）

**用途**：
- 重新加载不同分辨率的资源
- 调整窗口大小和位置

**示例**：
```cpp
void on_event(const WindowEvent& event) {
    if (event.kind == WindowEventKind::DpiChanged) {
        float new_dpi = event.new_dpi;
        math::Rect suggested_rect = event.suggested_rect;
        log("DPI 变化: {} → {}", current_dpi_, new_dpi);
        
        // 应用系统建议的窗口矩形
        window_->set_position(suggested_rect.position());
        window_->set_size(suggested_rect.size());
        
        // 重新加载高分辨率资源
        reload_resources(new_dpi);
        current_dpi_ = new_dpi;
    }
}
```

---

#### Activated / Deactivated

**有效字段**：无

**描述**：窗口切换为前台激活/后台非激活状态。

**用途**：
- 更新窗口标题栏颜色
- 暂停后台窗口的渲染

**示例**：
```cpp
void on_event(const WindowEvent& event) {
    if (event.kind == WindowEventKind::Activated) {
        log("窗口激活");
        title_bar_->set_active(true);
    } else if (event.kind == WindowEventKind::Deactivated) {
        log("窗口非激活");
        title_bar_->set_active(false);
    }
}
```

---

### IME 输入法事件

#### ImeCompositionStarted

**有效字段**：无

**描述**：IME 组合输入开始（用户开始输入中文/日文等）。

**平台映射**：
- **Windows**：`WM_IME_STARTCOMPOSITION`
- **macOS**：`-[NSTextInputClient insertText:]`
- **Linux**：XIM / IBus 组合开始

**示例**：
```cpp
void on_event(const WindowEvent& event) {
    if (event.kind == WindowEventKind::ImeCompositionStarted) {
        log("IME 组合输入开始");
        text_input_->start_composition();
    }
}
```

---

#### ImeCompositionChanged

**有效字段**：`ime_text_utf8`、`ime_text_length`

**描述**：IME 组合字符串更新，`ime_text_utf8` 含当前预编辑文字。

**特征**：
- 预编辑文字通常带下划线显示
- 用户可继续修改或确认提交

**示例**：
```cpp
void on_event(const WindowEvent& event) {
    if (event.kind == WindowEventKind::ImeCompositionChanged) {
        std::string_view composition(event.ime_text_utf8, event.ime_text_length);
        log("IME 组合字符串: {}", composition);
        text_input_->set_composition(composition);
    }
}
```

---

#### ImeCompositionCommitted

**有效字段**：`ime_text_utf8`、`ime_text_length`

**描述**：IME 确认提交，`ime_text_utf8` 含最终提交的文字。

**特征**：
- 最终提交的文字插入到文本框
- 组合输入流程结束

**示例**：
```cpp
void on_event(const WindowEvent& event) {
    if (event.kind == WindowEventKind::ImeCompositionCommitted) {
        std::string_view committed(event.ime_text_utf8, event.ime_text_length);
        log("IME 提交文字: {}", committed);
        text_input_->insert_text(committed);
    }
}
```

---

#### ImeCompositionEnded

**有效字段**：无

**描述**：IME 组合输入结束。

**平台映射**：
- **Windows**：`WM_IME_ENDCOMPOSITION`

**示例**：
```cpp
void on_event(const WindowEvent& event) {
    if (event.kind == WindowEventKind::ImeCompositionEnded) {
        log("IME 组合输入结束");
        text_input_->end_composition();
    }
}
```

---

### 键盘输入事件

#### KeyDown / KeyUp

**有效字段**：`key_vk_code`、`key_scan_code`、`key_is_repeat`、修饰键

**描述**：键盘按键按下/释放。

**特征**：
- `key_vk_code`：平台虚拟按键码（Windows: VK_A、VK_RETURN）
- `key_scan_code`：硬件扫描码（平台相关）
- `key_is_repeat`：是否为自动重复（按住按键产生的重复按下消息）

**示例**：
```cpp
void on_event(const WindowEvent& event) {
    if (event.kind == WindowEventKind::KeyDown) {
        log("按键按下: VK={}, Scan={}, Repeat={}", 
            event.key_vk_code, event.key_scan_code, event.key_is_repeat);
        
        // Ctrl + S 保存
        if (event.key_vk_code == VK_S && event.mod_ctrl && !event.key_is_repeat) {
            save_document();
        }
    }
}
```

---

#### Char

**有效字段**：`char_utf32`、修饰键

**描述**：字符输入（已处理修饰键的可打印字符）。

**特征**：
- `char_utf32`：UTF-32 字符码点
- 适用于文本输入场景
- 自动处理 Shift、Caps Lock

**示例**：
```cpp
void on_event(const WindowEvent& event) {
    if (event.kind == WindowEventKind::Char) {
        uint32_t codepoint = event.char_utf32;
        log("字符输入: U+{:04X}", codepoint);
        text_editor_->insert_char(codepoint);
    }
}
```

---

### 鼠标输入事件

#### MouseMove

**有效字段**：`mouse_x`、`mouse_y`、修饰键

**描述**：鼠标在客户区移动。

**特征**：
- `mouse_x`、`mouse_y`：鼠标位置（逻辑像素，客户区坐标）

**示例**：
```cpp
void on_event(const WindowEvent& event) {
    if (event.kind == WindowEventKind::MouseMove) {
        log("鼠标移动: ({}, {})", event.mouse_x, event.mouse_y);
        cursor_->set_position({event.mouse_x, event.mouse_y});
    }
}
```

---

#### MouseDown / MouseUp

**有效字段**：`mouse_x`、`mouse_y`、`mouse_button`、修饰键

**描述**：鼠标按键按下/释放。

**特征**：
- `mouse_button`：0=左键, 1=右键, 2=中键, 3=X1, 4=X2

**示例**：
```cpp
void on_event(const WindowEvent& event) {
    if (event.kind == WindowEventKind::MouseDown) {
        log("鼠标按下: 按键{} 位置({}, {})", 
            event.mouse_button, event.mouse_x, event.mouse_y);
        
        if (event.mouse_button == 0) {  // 左键
            handle_left_click({event.mouse_x, event.mouse_y});
        } else if (event.mouse_button == 1) {  // 右键
            show_context_menu({event.mouse_x, event.mouse_y});
        }
    }
}
```

---

#### MouseWheel

**有效字段**：`mouse_x`、`mouse_y`、`mouse_wheel_delta`、修饰键

**描述**：鼠标滚轮滚动。

**特征**：
- `mouse_wheel_delta`：正值向上，负值向下，一格通常为 120.0f

**示例**：
```cpp
void on_event(const WindowEvent& event) {
    if (event.kind == WindowEventKind::MouseWheel) {
        float delta = event.mouse_wheel_delta;
        log("滚轮滚动: delta={}", delta);
        
        // 缩放
        if (event.mod_ctrl) {
            zoom_factor_ += delta / 1200.0f;  // 每格 10% 缩放
        } else {
            scroll_offset_ += delta;  // 滚动
        }
    }
}
```

---

## 修饰键状态

所有输入事件（KeyDown、KeyUp、Char、MouseDown、MouseUp、MouseWheel）都包含修饰键状态：

```cpp
bool mod_ctrl{false};   // Ctrl 键当前是否按下
bool mod_shift{false};  // Shift 键当前是否按下
bool mod_alt{false};    // Alt 键当前是否按下
```

**示例**：
```cpp
void on_event(const WindowEvent& event) {
    if (event.kind == WindowEventKind::KeyDown) {
        // Ctrl + Shift + S
        if (event.key_vk_code == VK_S && event.mod_ctrl && event.mod_shift) {
            save_as_document();
        }
    }
}
```

---

## 使用场景

### 1. 窗口尺寸变化响应

```cpp
class RenderTarget {
    void on_event(const WindowEvent& event) {
        if (event.kind == WindowEventKind::Resized) {
            resize_render_target(event.new_size);
        }
    }
};
```

---

### 2. 阻止窗口关闭

```cpp
class Editor {
    bool has_unsaved_changes_ = true;
    
    void on_event(const WindowEvent& event) {
        if (event.kind == WindowEventKind::Closing) {
            if (has_unsaved_changes_) {
                event.cancel = true;  // 阻止关闭
                show_save_dialog();
            }
        }
    }
};
```

---

### 3. DPI 变化处理

```cpp
class Application {
    void on_event(const WindowEvent& event) {
        if (event.kind == WindowEventKind::DpiChanged) {
            float scale = event.new_dpi / 96.0f;
            log("DPI 缩放: {}x", scale);
            
            // 应用系统建议的窗口矩形
            window_->set_position(event.suggested_rect.position());
            window_->set_size(event.suggested_rect.size());
            
            // 重新加载资源
            reload_high_dpi_assets(event.new_dpi);
        }
    }
};
```

---

### 4. IME 输入处理

```cpp
class TextEditor {
    std::string composition_;
    
    void on_event(const WindowEvent& event) {
        switch (event.kind) {
        case WindowEventKind::ImeCompositionStarted:
            composition_.clear();
            break;
            
        case WindowEventKind::ImeCompositionChanged:
            composition_ = std::string(event.ime_text_utf8, event.ime_text_length);
            render_composition_underline();
            break;
            
        case WindowEventKind::ImeCompositionCommitted:
            insert_text(event.ime_text_utf8, event.ime_text_length);
            composition_.clear();
            break;
            
        case WindowEventKind::ImeCompositionEnded:
            composition_.clear();
            break;
        }
    }
};
```

---

### 5. 键盘快捷键

```cpp
class Application {
    void on_event(const WindowEvent& event) {
        if (event.kind == WindowEventKind::KeyDown && !event.key_is_repeat) {
            // Ctrl + S
            if (event.key_vk_code == VK_S && event.mod_ctrl) {
                save_document();
            }
            // Ctrl + Z
            else if (event.key_vk_code == VK_Z && event.mod_ctrl) {
                undo();
            }
            // Ctrl + Y
            else if (event.key_vk_code == VK_Y && event.mod_ctrl) {
                redo();
            }
        }
    }
};
```

---

### 6. 鼠标交互

```cpp
class Canvas {
    bool is_dragging_ = false;
    math::Point drag_start_;
    
    void on_event(const WindowEvent& event) {
        if (event.kind == WindowEventKind::MouseDown && event.mouse_button == 0) {
            is_dragging_ = true;
            drag_start_ = {event.mouse_x, event.mouse_y};
        }
        else if (event.kind == WindowEventKind::MouseMove && is_dragging_) {
            math::Point current = {event.mouse_x, event.mouse_y};
            math::Point delta = current - drag_start_;
            move_selection(delta);
            drag_start_ = current;
        }
        else if (event.kind == WindowEventKind::MouseUp && event.mouse_button == 0) {
            is_dragging_ = false;
        }
    }
};
```

---

## 最佳实践

### 1. 使用 switch-case 处理事件

```cpp
// ✅ 推荐：使用 switch-case
void on_event(const WindowEvent& event) {
    switch (event.kind) {
    case WindowEventKind::Resized:
        handle_resize(event.new_size);
        break;
    case WindowEventKind::KeyDown:
        handle_key_down(event);
        break;
    // ...
    }
}

// ❌ 不推荐：大量 if-else
if (event.kind == WindowEventKind::Resized) {
    // ...
} else if (event.kind == WindowEventKind::KeyDown) {
    // ...
}
```

---

### 2. KeyDown 忽略重复事件

```cpp
// ✅ 推荐：忽略重复事件处理快捷键
if (event.kind == WindowEventKind::KeyDown && !event.key_is_repeat) {
    if (event.key_vk_code == VK_S && event.mod_ctrl) {
        save();
    }
}

// ❌ 不推荐：重复事件触发多次保存
if (event.kind == WindowEventKind::KeyDown) {
    if (event.key_vk_code == VK_S && event.mod_ctrl) {
        save();  // 按住 Ctrl+S 会重复触发
    }
}
```

---

### 3. Closing 事件及时清理对话框

```cpp
// ✅ 推荐：阻止关闭后立即显示对话框
if (event.kind == WindowEventKind::Closing) {
    if (has_unsaved_data()) {
        event.cancel = true;
        show_save_confirmation_dialog();  // 立即显示
    }
}

// ❌ 不推荐：阻止关闭但不提示用户
if (event.kind == WindowEventKind::Closing) {
    event.cancel = true;  // 用户困惑，窗口无法关闭
}
```

---

### 4. IME 组合文字使用 string_view

```cpp
// ✅ 推荐：使用 string_view 避免拷贝
if (event.kind == WindowEventKind::ImeCompositionCommitted) {
    std::string_view text(event.ime_text_utf8, event.ime_text_length);
    insert_text(text);
}

// ❌ 不推荐：使用 strlen（ime_text_length 更高效）
std::string text(event.ime_text_utf8);  // strlen 扫描整个数组
```

---

## 常见陷阱

### 1. Closed 事件后使用 IWindow 指针

```cpp
// ❌ 错误：Closed 事件后调用 IWindow 方法
if (event.kind == WindowEventKind::Closed) {
    window_->show();  // 崩溃：窗口已销毁
}

// ✅ 正确：Closed 事件后清除指针
if (event.kind == WindowEventKind::Closed) {
    window_ = nullptr;
}
```

---

### 2. 忘记检查事件类型读取字段

```cpp
// ❌ 错误：不检查事件类型直接读取字段
void on_event(const WindowEvent& event) {
    math::Size size = event.new_size;  // 可能全零
}

// ✅ 正确：检查事件类型
if (event.kind == WindowEventKind::Resized) {
    math::Size size = event.new_size;
}
```

---

### 3. KeyDown 和 Char 混用

```cpp
// ❌ 错误：KeyDown 事件处理文本输入
if (event.kind == WindowEventKind::KeyDown) {
    // 无法处理 Shift + A = 'A'
    char ch = event.key_vk_code;  // VK_A 不等于 'A'
}

// ✅ 正确：Char 事件处理文本输入
if (event.kind == WindowEventKind::Char) {
    uint32_t codepoint = event.char_utf32;
    insert_char(codepoint);
}
```

---

### 4. 修饰键状态滞后

```cpp
// ❌ 问题：修饰键状态可能滞后
if (event.kind == WindowEventKind::MouseDown) {
    if (event.mod_ctrl) {
        // Ctrl 键可能在鼠标按下前已释放
    }
}

// ✅ 解决：使用平台 API 查询当前修饰键状态
if (event.kind == WindowEventKind::MouseDown) {
    bool ctrl_down = (GetKeyState(VK_CONTROL) & 0x8000) != 0;
}
```

---

## 完整示例

```cpp
#include <mine/platform/IWindow.h>
#include <mine/platform/WindowEvent.h>

class Application : public IWindowEventSink {
    IWindow* window_;
    bool has_unsaved_data_ = false;
    
public:
    void on_event(const WindowEvent& event) override {
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
            window_ = nullptr;
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
            
        case WindowEventKind::ImeCompositionCommitted:
            insert_text(event.ime_text_utf8, event.ime_text_length);
            break;
            
        default:
            break;
        }
    }
    
private:
    void handle_resize(math::Size new_size) {
        renderer_->resize(new_size);
    }
    
    void handle_dpi_change(float new_dpi, math::Rect suggested_rect) {
        window_->set_position(suggested_rect.position());
        window_->set_size(suggested_rect.size());
        reload_resources(new_dpi);
    }
    
    void handle_shortcut(const WindowEvent& event) {
        if (event.mod_ctrl && event.key_vk_code == VK_S) {
            save();
        } else if (event.mod_ctrl && event.key_vk_code == VK_Z) {
            undo();
        }
    }
};
```

---

## 总结

`WindowEvent` 是 `mine.platform.abi` 模块的窗口事件数据结构，具备：

1. **扁平结构**：无 `union` 或 `std::variant`，根据 `kind` 判断有效字段
2. **21 种事件**：窗口生命周期、IME 输入、键盘、鼠标事件
3. **ABI 安全**：POD 类型，跨 DLL 边界传递安全
4. **事件合并**：单次回调传递多个相关信息（如 DPI 变化 + 建议窗口矩形）

**使用建议**：
- 使用 `switch-case` 处理事件
- `KeyDown` 忽略重复事件（`!key_is_repeat`）
- `Closing` 事件阻止关闭后立即显示对话框
- `Closed` 事件后清除 `IWindow*` 指针
- IME 组合文字使用 `string_view` 避免拷贝
- `KeyDown` 处理快捷键，`Char` 处理文本输入
- 修饰键状态可能滞后，使用平台 API 查询最新状态
