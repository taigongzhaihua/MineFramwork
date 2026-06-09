# IClipboard 详细接口文档

## 概述

`IClipboard` 是 `mine.platform.abi` 模块的**系统剪贴板访问接口**，提供文本读写能力。

**核心特性：**
- **5 个方法**：has_text、get_text、set_text、clear
- **UTF-8 编码**：所有文本使用 UTF-8 编码
- **同步调用**：所有方法均为同步调用
- **主线程操作**：应在主线程调用

---

## 文件位置

```
src/mine/platform/abi/api/include/mine/platform/IClipboard.h
```

---

## 类型定义

```cpp
namespace mine::platform {

class IClipboard {
public:
    virtual ~IClipboard() = default;
    
    /// 剪贴板当前是否包含纯文本内容
    [[nodiscard]] virtual bool has_text() const = 0;
    
    /**
     * @brief 将剪贴板文本写入调用方提供的缓冲区（UTF-8）。
     *
     * @param buffer       接收缓冲区（可为 nullptr 以查询所需长度）
     * @param buffer_size  缓冲区字节容量
     * @param out_size     [out] 实际写入或所需的字节数（不含 '\0'）
     * @return 成功返回 Status::ok()；缓冲区不足返回错误状态
     */
    virtual core::Status get_text(
        char*   buffer,
        size_t  buffer_size,
        size_t* out_size) const = 0;
    
    /**
     * @brief 将 UTF-8 文本写入剪贴板。
     * @param text 待写入的文本视图
     * @return 成功返回 Status::ok()；否则返回错误状态
     */
    virtual core::Status set_text(core::StringView text) = 0;
    
    /// 清空剪贴板内容
    virtual void clear() = 0;
};

}
```

---

## 成员方法

### has_text()

```cpp
[[nodiscard]] virtual bool has_text() const = 0;
```

**描述**：剪贴板当前是否包含纯文本内容。

**返回**：`true` 表示剪贴板包含文本，`false` 表示不包含

**示例**：
```cpp
IClipboard* clipboard = host->clipboard();
if (clipboard->has_text()) {
    log("剪贴板包含文本");
} else {
    log("剪贴板不包含文本");
}
```

---

### get_text()

```cpp
virtual core::Status get_text(
    char*   buffer,
    size_t  buffer_size,
    size_t* out_size) const = 0;
```

**描述**：将剪贴板文本写入调用方提供的缓冲区（UTF-8）。

**参数**：
- `buffer`：接收缓冲区（可为 `nullptr` 以查询所需长度）
- `buffer_size`：缓冲区字节容量
- `out_size`：[out] 实际写入或所需的字节数（不含 `'\0'`）

**返回**：
- 成功返回 `Status::ok()`
- 缓冲区不足返回错误状态

**特征**：
- **查询长度**：`buffer` 为 `nullptr` 时查询所需长度
- **UTF-8 编码**：返回的文本使用 UTF-8 编码
- **不含 '\0'**：`out_size` 不包含 `'\0'` 终止符

**示例**：
```cpp
IClipboard* clipboard = host->clipboard();

// 查询所需长度
size_t required_size = 0;
clipboard->get_text(nullptr, 0, &required_size);

// 分配缓冲区
std::vector<char> buffer(required_size + 1);  // +1 for '\0'

// 读取文本
size_t actual_size = 0;
core::Status status = clipboard->get_text(buffer.data(), buffer.size(), &actual_size);
if (status.is_ok()) {
    buffer[actual_size] = '\0';  // 添加终止符
    log("剪贴板文本: {}", buffer.data());
} else {
    log("读取失败: {}", status.message());
}
```

---

### set_text()

```cpp
virtual core::Status set_text(core::StringView text) = 0;
```

**描述**：将 UTF-8 文本写入剪贴板。

**参数**：
- `text`：待写入的文本视图（UTF-8）

**返回**：
- 成功返回 `Status::ok()`
- 失败返回错误状态

**示例**：
```cpp
IClipboard* clipboard = host->clipboard();
core::Status status = clipboard->set_text("Hello, World!");
if (status.is_ok()) {
    log("写入成功");
} else {
    log("写入失败: {}", status.message());
}
```

---

### clear()

```cpp
virtual void clear() = 0;
```

**描述**：清空剪贴板内容。

**示例**：
```cpp
IClipboard* clipboard = host->clipboard();
clipboard->clear();
log("剪贴板已清空");
```

---

## 使用场景

### 1. 检查剪贴板是否包含文本

```cpp
IClipboard* clipboard = host->clipboard();
if (clipboard->has_text()) {
    log("剪贴板包含文本");
} else {
    log("剪贴板不包含文本");
}
```

---

### 2. 读取剪贴板文本

```cpp
std::string read_clipboard_text(IClipboard* clipboard) {
    if (!clipboard->has_text()) {
        return "";
    }
    
    // 查询所需长度
    size_t required_size = 0;
    clipboard->get_text(nullptr, 0, &required_size);
    
    // 分配缓冲区
    std::vector<char> buffer(required_size + 1);
    
    // 读取文本
    size_t actual_size = 0;
    core::Status status = clipboard->get_text(buffer.data(), buffer.size(), &actual_size);
    if (!status.is_ok()) {
        return "";
    }
    
    buffer[actual_size] = '\0';
    return std::string(buffer.data(), actual_size);
}
```

---

### 3. 写入剪贴板文本

```cpp
void write_clipboard_text(IClipboard* clipboard, const std::string& text) {
    core::Status status = clipboard->set_text(text);
    if (status.is_ok()) {
        log("写入成功");
    } else {
        log("写入失败: {}", status.message());
    }
}
```

---

### 4. 复制选中文本

```cpp
void copy_selection(IClipboard* clipboard, const std::string& selected_text) {
    if (selected_text.empty()) {
        log("没有选中文本");
        return;
    }
    
    core::Status status = clipboard->set_text(selected_text);
    if (status.is_ok()) {
        log("已复制: {}", selected_text);
    } else {
        log("复制失败: {}", status.message());
    }
}
```

---

### 5. 粘贴文本

```cpp
void paste_text(IClipboard* clipboard, TextEditor* editor) {
    if (!clipboard->has_text()) {
        log("剪贴板不包含文本");
        return;
    }
    
    // 读取剪贴板文本
    std::string text = read_clipboard_text(clipboard);
    
    // 插入到编辑器
    editor->insert_text(text);
    log("已粘贴: {}", text);
}
```

---

### 6. 剪切文本

```cpp
void cut_selection(IClipboard* clipboard, TextEditor* editor) {
    std::string selected_text = editor->get_selected_text();
    if (selected_text.empty()) {
        log("没有选中文本");
        return;
    }
    
    // 复制到剪贴板
    core::Status status = clipboard->set_text(selected_text);
    if (status.is_ok()) {
        // 删除选中文本
        editor->delete_selection();
        log("已剪切: {}", selected_text);
    } else {
        log("剪切失败: {}", status.message());
    }
}
```

---

### 7. 清空剪贴板

```cpp
void clear_clipboard(IClipboard* clipboard) {
    clipboard->clear();
    log("剪贴板已清空");
}
```

---

## 错误处理

### get_text() 错误处理

```cpp
std::string read_clipboard_text_safe(IClipboard* clipboard) {
    if (!clipboard->has_text()) {
        return "";
    }
    
    // 查询所需长度
    size_t required_size = 0;
    core::Status status = clipboard->get_text(nullptr, 0, &required_size);
    if (!status.is_ok()) {
        log("查询长度失败: {}", status.message());
        return "";
    }
    
    // 分配缓冲区
    std::vector<char> buffer(required_size + 1);
    
    // 读取文本
    size_t actual_size = 0;
    status = clipboard->get_text(buffer.data(), buffer.size(), &actual_size);
    if (!status.is_ok()) {
        log("读取失败: {}", status.message());
        return "";
    }
    
    buffer[actual_size] = '\0';
    return std::string(buffer.data(), actual_size);
}
```

---

### set_text() 错误处理

```cpp
bool write_clipboard_text_safe(IClipboard* clipboard, const std::string& text) {
    core::Status status = clipboard->set_text(text);
    if (!status.is_ok()) {
        log("写入失败: {}", status.message());
        return false;
    }
    return true;
}
```

---

## 平台差异

### Windows

```cpp
// Windows 实现使用 Win32 API
bool IClipboard_Win32::has_text() const {
    return IsClipboardFormatAvailable(CF_UNICODETEXT);
}

core::Status IClipboard_Win32::set_text(core::StringView text) {
    if (!OpenClipboard(nullptr)) {
        return core::Status::error("无法打开剪贴板");
    }
    
    EmptyClipboard();
    
    // UTF-8 → UTF-16
    std::wstring wide_text = utf8_to_wide(text);
    
    // 分配全局内存
    HGLOBAL hMem = GlobalAlloc(GMEM_MOVEABLE, (wide_text.size() + 1) * sizeof(wchar_t));
    if (!hMem) {
        CloseClipboard();
        return core::Status::error("内存分配失败");
    }
    
    // 写入数据
    wchar_t* pMem = (wchar_t*)GlobalLock(hMem);
    memcpy(pMem, wide_text.data(), (wide_text.size() + 1) * sizeof(wchar_t));
    GlobalUnlock(hMem);
    
    // 设置剪贴板数据
    SetClipboardData(CF_UNICODETEXT, hMem);
    CloseClipboard();
    
    return core::Status::ok();
}
```

---

### macOS

```objc
// macOS 实现使用 NSPasteboard
bool IClipboard_macOS::has_text() const {
    NSPasteboard* pasteboard = [NSPasteboard generalPasteboard];
    return [pasteboard availableTypeFromArray:@[NSPasteboardTypeString]] != nil;
}

core::Status IClipboard_macOS::set_text(core::StringView text) {
    NSPasteboard* pasteboard = [NSPasteboard generalPasteboard];
    [pasteboard clearContents];
    
    NSString* nsstring = [NSString stringWithUTF8String:text.data()];
    BOOL success = [pasteboard setString:nsstring forType:NSPasteboardTypeString];
    
    if (success) {
        return core::Status::ok();
    } else {
        return core::Status::error("写入剪贴板失败");
    }
}
```

---

### Linux (X11)

```cpp
// Linux X11 实现使用 XClipboard API
bool IClipboard_X11::has_text() const {
    Display* display = XOpenDisplay(nullptr);
    Atom clipboard = XInternAtom(display, "CLIPBOARD", False);
    Window owner = XGetSelectionOwner(display, clipboard);
    XCloseDisplay(display);
    return owner != None;
}

core::Status IClipboard_X11::set_text(core::StringView text) {
    Display* display = XOpenDisplay(nullptr);
    Atom clipboard = XInternAtom(display, "CLIPBOARD", False);
    Atom utf8_string = XInternAtom(display, "UTF8_STRING", False);
    
    Window window = XCreateSimpleWindow(display, DefaultRootWindow(display), 
                                        0, 0, 1, 1, 0, 0, 0);
    
    // 设置选择内容
    XSetSelectionOwner(display, clipboard, window, CurrentTime);
    
    // 响应 SelectionRequest 事件
    // ...
    
    XCloseDisplay(display);
    return core::Status::ok();
}
```

---

## 最佳实践

### 1. 检查剪贴板是否包含文本

```cpp
// ✅ 推荐：检查是否包含文本
if (clipboard->has_text()) {
    std::string text = read_clipboard_text(clipboard);
    // ...
}

// ❌ 不推荐：直接读取（可能失败）
std::string text = read_clipboard_text(clipboard);  // 可能为空
```

---

### 2. 查询长度后分配缓冲区

```cpp
// ✅ 推荐：查询长度后分配缓冲区
size_t required_size = 0;
clipboard->get_text(nullptr, 0, &required_size);
std::vector<char> buffer(required_size + 1);
clipboard->get_text(buffer.data(), buffer.size(), &actual_size);

// ❌ 不推荐：固定大小缓冲区（可能不足）
char buffer[256];
clipboard->get_text(buffer, sizeof(buffer), &actual_size);  // 可能截断
```

---

### 3. 错误处理

```cpp
// ✅ 推荐：错误处理
core::Status status = clipboard->set_text(text);
if (!status.is_ok()) {
    log("写入失败: {}", status.message());
    return;
}

// ❌ 不推荐：忽略错误
clipboard->set_text(text);  // 可能失败
```

---

### 4. UTF-8 编码

```cpp
// ✅ 推荐：使用 UTF-8 编码
clipboard->set_text("你好，世界！");  // UTF-8

// ❌ 不推荐：使用其他编码
clipboard->set_text("你好，世界！");  // 可能是 GBK/GB2312
```

---

## 常见陷阱

### 1. 固定大小缓冲区

```cpp
// ❌ 问题：固定大小缓冲区，文本可能被截断
char buffer[256];
size_t actual_size = 0;
clipboard->get_text(buffer, sizeof(buffer), &actual_size);
// 文本超过 256 字节时被截断

// ✅ 解决：查询长度后分配缓冲区
size_t required_size = 0;
clipboard->get_text(nullptr, 0, &required_size);
std::vector<char> buffer(required_size + 1);
clipboard->get_text(buffer.data(), buffer.size(), &actual_size);
```

---

### 2. 忘记添加 '\0' 终止符

```cpp
// ❌ 问题：忘记添加 '\0' 终止符
std::vector<char> buffer(required_size);
clipboard->get_text(buffer.data(), buffer.size(), &actual_size);
log("文本: {}", buffer.data());  // 可能越界

// ✅ 解决：添加 '\0' 终止符
std::vector<char> buffer(required_size + 1);
clipboard->get_text(buffer.data(), buffer.size(), &actual_size);
buffer[actual_size] = '\0';
log("文本: {}", buffer.data());
```

---

### 3. 忽略 has_text() 检查

```cpp
// ❌ 问题：忽略 has_text() 检查
std::string text = read_clipboard_text(clipboard);  // 可能为空
editor->insert_text(text);

// ✅ 解决：检查是否包含文本
if (clipboard->has_text()) {
    std::string text = read_clipboard_text(clipboard);
    editor->insert_text(text);
} else {
    log("剪贴板不包含文本");
}
```

---

### 4. 忽略错误状态

```cpp
// ❌ 问题：忽略错误状态
clipboard->set_text(text);  // 可能失败
log("写入成功");

// ✅ 解决：检查错误状态
core::Status status = clipboard->set_text(text);
if (status.is_ok()) {
    log("写入成功");
} else {
    log("写入失败: {}", status.message());
}
```

---

## 完整示例

```cpp
#include <mine/platform/IApplicationHost.h>
#include <mine/platform/IClipboard.h>

class TextEditor {
    IClipboard* clipboard_;
    std::string content_;
    size_t selection_start_ = 0;
    size_t selection_end_ = 0;
    
public:
    TextEditor(IClipboard* clipboard) : clipboard_(clipboard) {}
    
    void copy() {
        if (selection_start_ == selection_end_) {
            log("没有选中文本");
            return;
        }
        
        std::string selected_text = content_.substr(
            selection_start_, 
            selection_end_ - selection_start_);
        
        core::Status status = clipboard_->set_text(selected_text);
        if (status.is_ok()) {
            log("已复制: {}", selected_text);
        } else {
            log("复制失败: {}", status.message());
        }
    }
    
    void cut() {
        if (selection_start_ == selection_end_) {
            log("没有选中文本");
            return;
        }
        
        std::string selected_text = content_.substr(
            selection_start_, 
            selection_end_ - selection_start_);
        
        core::Status status = clipboard_->set_text(selected_text);
        if (status.is_ok()) {
            // 删除选中文本
            content_.erase(selection_start_, selection_end_ - selection_start_);
            selection_end_ = selection_start_;
            log("已剪切: {}", selected_text);
        } else {
            log("剪切失败: {}", status.message());
        }
    }
    
    void paste() {
        if (!clipboard_->has_text()) {
            log("剪贴板不包含文本");
            return;
        }
        
        // 读取剪贴板文本
        std::string text = read_clipboard_text();
        
        // 删除选中文本
        if (selection_start_ != selection_end_) {
            content_.erase(selection_start_, selection_end_ - selection_start_);
            selection_end_ = selection_start_;
        }
        
        // 插入文本
        content_.insert(selection_start_, text);
        selection_start_ += text.size();
        selection_end_ = selection_start_;
        
        log("已粘贴: {}", text);
    }
    
private:
    std::string read_clipboard_text() {
        // 查询所需长度
        size_t required_size = 0;
        clipboard_->get_text(nullptr, 0, &required_size);
        
        // 分配缓冲区
        std::vector<char> buffer(required_size + 1);
        
        // 读取文本
        size_t actual_size = 0;
        core::Status status = clipboard_->get_text(
            buffer.data(), buffer.size(), &actual_size);
        if (!status.is_ok()) {
            return "";
        }
        
        buffer[actual_size] = '\0';
        return std::string(buffer.data(), actual_size);
    }
};

int main() {
    auto host = platform::initialize();
    IClipboard* clipboard = host->clipboard();
    
    TextEditor editor(clipboard);
    editor.copy();
    editor.paste();
    
    return 0;
}
```

---

## 总结

`IClipboard` 是 `mine.platform.abi` 模块的系统剪贴板访问接口，具备：

1. **5 个方法**：has_text、get_text、set_text、clear
2. **UTF-8 编码**：所有文本使用 UTF-8 编码
3. **同步调用**：所有方法均为同步调用
4. **主线程操作**：应在主线程调用

**使用建议**：
- 检查剪贴板是否包含文本（`has_text()`）
- 查询长度后分配缓冲区（避免固定大小缓冲区）
- 错误处理（检查 `Status`）
- 使用 UTF-8 编码
- 添加 `'\0'` 终止符
- 不忽略错误状态
