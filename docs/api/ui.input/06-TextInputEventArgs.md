# TextInputEventArgs 详细接口文档

## 概述

`TextInputEventArgs` 是 `mine.ui.input` 模块的**字符输入路由事件参数类**。

**核心特性：**
- **路由事件参数**：继承自 `RoutedEventArgs`，支持事件冒泡
- **Unicode 字符**：包含 UTF-32 编码的 Unicode 码点（可打印字符）
- **焦点目标**：派发到当前键盘焦点元素
- **栈分配**：在调用栈上分配，不可拷贝

**与 KeyDown 的区别：**
- `KeyDown`：硬件按键事件，包含虚拟键枚举（Key）
- `TextInput`：字符输入事件，包含 UTF-32 码点，已处理修饰键
- 控制字符（< 32，如退格、回车）仅通过 `KeyDown` 传递，不触发 `TextInput`

**继承关系：**
```
RoutedEventArgs
    └── TextInputEventArgs
```

---

## 文件位置

```
src/mine/ui/input/api/include/mine/ui/input/TextInputEventArgs.h
```

---

## 类定义

```cpp
class MINE_UI_INPUT_API TextInputEventArgs : public RoutedEventArgs {
public:
    explicit TextInputEventArgs(const RoutedEvent& event,
                                uint32_t           char_utf32) noexcept;

    [[nodiscard]] uint32_t char_utf32() const noexcept;

private:
    uint32_t char_utf32_;
};
```

---

## 构造函数

### TextInputEventArgs

```cpp
explicit TextInputEventArgs(const RoutedEvent& event,
                            uint32_t           char_utf32) noexcept;
```

**描述**：构造字符输入事件参数。

**参数**：
- `event`：路由事件描述符（`TextInputEvent()`）
- `char_utf32`：Unicode 码点（UTF-32），保证为可打印字符

**特征**：
- `noexcept`：保证不抛出异常
- `explicit`：禁止隐式转换

**说明**：
- `char_utf32` 为可打印字符的 Unicode 码点（≥ 32）
- 控制字符（< 32，如退格、回车）不通过此事件传递

**示例**：
```cpp
#include <mine/ui/input/TextInputEventArgs.h>
#include <mine/ui/input/InputEvents.h>

using namespace mine::ui::input;

// 构造 TextInput 事件参数
TextInputEventArgs args(
    TextInputEvent(),         // 事件描述符
    0x4E2D                    // Unicode 码点 '中'
);

// 触发事件
EventManager::raise(*element, args);
```

---

## 成员方法

### char_utf32

```cpp
[[nodiscard]] uint32_t char_utf32() const noexcept;
```

**描述**：获取字符 Unicode 码点（UTF-32）。

**返回值**：
- `uint32_t`：Unicode 码点（UTF-32），保证为可打印字符（≥ 32）

**说明**：
- UTF-32 编码：每个字符占用 4 字节，直接表示 Unicode 码点
- 可打印字符：排除控制字符（0x00-0x1F）

**示例**：
```cpp
void on_text_input(TextInputEventArgs& args) {
    uint32_t codepoint = args.char_utf32();
    
    // 转换为 UTF-8
    std::string utf8_str = codepoint_to_utf8(codepoint);
    std::cout << "输入字符: " << utf8_str << std::endl;
    
    // 插入到文本框
    insert_char(codepoint);
}
```

---

## 使用场景

### 1. 文本输入控件

```cpp
#include <mine/ui/input/TextInputEventArgs.h>
#include <mine/ui/input/InputEvents.h>

using namespace mine::ui::input;

class TextBox : public Visual {
public:
    TextBox() {
        // 注册 TextInput 事件处理器
        add_handler(TextInputEvent(), &TextBox::on_text_input_router, this);
    }

protected:
    void on_text_input(TextInputEventArgs& args) {
        uint32_t codepoint = args.char_utf32();
        
        // 插入字符到文本缓冲区
        insert_char(codepoint);
        
        // 标记已处理
        args.set_handled(true);
    }

private:
    void on_text_input_router(RoutedEventArgs& args) {
        on_text_input(static_cast<TextInputEventArgs&>(args));
    }
    
    void insert_char(uint32_t codepoint) {
        // 转换为 UTF-8 并插入
        std::string utf8_str = codepoint_to_utf8(codepoint);
        m_text += utf8_str;
        std::cout << "插入字符: " << utf8_str << " (U+" 
                  << std::hex << codepoint << ")" << std::endl;
    }
    
    std::string codepoint_to_utf8(uint32_t cp) {
        std::string result;
        if (cp <= 0x7F) {
            result.push_back(static_cast<char>(cp));
        } else if (cp <= 0x7FF) {
            result.push_back(static_cast<char>(0xC0 | (cp >> 6)));
            result.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
        } else if (cp <= 0xFFFF) {
            result.push_back(static_cast<char>(0xE0 | (cp >> 12)));
            result.push_back(static_cast<char>(0x80 | ((cp >> 6) & 0x3F)));
            result.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
        } else if (cp <= 0x10FFFF) {
            result.push_back(static_cast<char>(0xF0 | (cp >> 18)));
            result.push_back(static_cast<char>(0x80 | ((cp >> 12) & 0x3F)));
            result.push_back(static_cast<char>(0x80 | ((cp >> 6) & 0x3F)));
            result.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
        }
        return result;
    }
    
    std::string m_text;
};
```

---

### 2. 字符输入日志

```cpp
void on_text_input(TextInputEventArgs& args) {
    uint32_t codepoint = args.char_utf32();
    
    // 输出字符信息
    std::cout << "字符输入: U+" << std::hex << codepoint;
    
    // ASCII 范围
    if (codepoint <= 0x7F) {
        std::cout << " ('" << static_cast<char>(codepoint) << "')";
    }
    
    std::cout << std::endl;
}
```

---

### 3. 字符过滤

```cpp
void on_text_input(TextInputEventArgs& args) {
    uint32_t codepoint = args.char_utf32();
    
    // 仅允许数字字符（0-9）
    if (codepoint >= '0' && codepoint <= '9') {
        insert_char(codepoint);
        args.set_handled(true);
    } else {
        std::cout << "非数字字符被过滤: U+" << std::hex << codepoint << std::endl;
        args.set_handled(true);  // 阻止冒泡
    }
}
```

---

### 4. 中文输入处理

```cpp
void on_text_input(TextInputEventArgs& args) {
    uint32_t codepoint = args.char_utf32();
    
    // 检查是否为中文字符（CJK Unified Ideographs）
    if (codepoint >= 0x4E00 && codepoint <= 0x9FFF) {
        std::cout << "中文字符输入: U+" << std::hex << codepoint << std::endl;
    }
    
    insert_char(codepoint);
    args.set_handled(true);
}
```

---

### 5. 表情符号处理

```cpp
void on_text_input(TextInputEventArgs& args) {
    uint32_t codepoint = args.char_utf32();
    
    // 检查是否为表情符号（Emoticons、Emoji）
    if ((codepoint >= 0x1F600 && codepoint <= 0x1F64F) ||  // Emoticons
        (codepoint >= 0x1F300 && codepoint <= 0x1F5FF)) {  // Misc Symbols
        std::cout << "表情符号输入: U+" << std::hex << codepoint << std::endl;
    }
    
    insert_char(codepoint);
    args.set_handled(true);
}
```

---

### 6. 字符转换为 std::string

```cpp
void on_text_input(TextInputEventArgs& args) {
    uint32_t codepoint = args.char_utf32();
    
    // 转换为 UTF-8 字符串
    std::string utf8_str = codepoint_to_utf8(codepoint);
    
    // 追加到文本缓冲区
    m_text_buffer += utf8_str;
    
    std::cout << "文本缓冲区: " << m_text_buffer << std::endl;
    
    args.set_handled(true);
}
```

---

### 7. 字符计数

```cpp
class TextBox : public Visual {
protected:
    void on_text_input(TextInputEventArgs& args) {
        uint32_t codepoint = args.char_utf32();
        
        // 插入字符
        insert_char(codepoint);
        
        // 更新字符计数
        m_char_count++;
        std::cout << "字符计数: " << m_char_count << std::endl;
        
        args.set_handled(true);
    }

private:
    int m_char_count = 0;
};
```

---

## 最佳实践

### 1. 区分 KeyDown 和 TextInput

```cpp
// ✅ 推荐：区分按键事件和字符输入事件

// KeyDown 处理控制字符（退格、回车、方向键等）
void on_key_down(KeyEventArgs& args) override {
    switch (args.key()) {
        case Key::Backspace:
            delete_previous_char();
            args.set_handled(true);
            break;
            
        case Key::Enter:
            insert_newline();
            args.set_handled(true);
            break;
            
        default:
            Visual::on_key_down(args);
            break;
    }
}

// TextInput 处理可打印字符
void on_text_input(TextInputEventArgs& args) override {
    insert_char(args.char_utf32());
    args.set_handled(true);
}

// ❌ 不推荐：在 KeyDown 中处理字符输入
void on_key_down(KeyEventArgs& args) override {
    // ❌ 无法正确处理非 ASCII 字符、IME 输入
    if (args.key() >= Key::A && args.key() <= Key::Z) {
        char ch = 'A' + (static_cast<int>(args.key()) - static_cast<int>(Key::A));
        insert_char(ch);  // ❌ 未考虑修饰键（Shift 大小写）
    }
}
```

---

### 2. 使用 UTF-8 存储文本

```cpp
// ✅ 推荐：使用 UTF-8 存储文本（跨平台、节省空间）
void on_text_input(TextInputEventArgs& args) {
    uint32_t codepoint = args.char_utf32();
    std::string utf8_str = codepoint_to_utf8(codepoint);
    m_text += utf8_str;  // UTF-8 字符串
}

// ❌ 不推荐：使用 UTF-32 存储（占用空间大）
void on_text_input(TextInputEventArgs& args) {
    uint32_t codepoint = args.char_utf32();
    m_text_utf32.push_back(codepoint);  // ❌ UTF-32 字符串（4 倍空间）
}
```

---

### 3. 调用 set_handled 阻止事件冒泡

```cpp
// ✅ 推荐：处理完事件后调用 set_handled
void on_text_input(TextInputEventArgs& args) {
    insert_char(args.char_utf32());
    args.set_handled(true);  // 阻止事件继续冒泡
}

// ❌ 不推荐：忘记调用 set_handled（事件会继续冒泡）
void on_text_input(TextInputEventArgs& args) {
    insert_char(args.char_utf32());
    // ❌ 忘记调用 set_handled
}
```

---

### 4. 处理多字节字符

```cpp
// ✅ 推荐：正确处理 UTF-32 码点（支持所有 Unicode 字符）
void on_text_input(TextInputEventArgs& args) {
    uint32_t codepoint = args.char_utf32();
    
    // 支持所有 Unicode 字符（包括 CJK、表情符号）
    insert_char(codepoint);
}

// ❌ 不推荐：假设所有字符为 ASCII
void on_text_input(TextInputEventArgs& args) {
    uint32_t codepoint = args.char_utf32();
    
    // ❌ 无法正确处理非 ASCII 字符
    char ch = static_cast<char>(codepoint);  // ❌ 截断
    insert_char(ch);
}
```

---

## 常见陷阱

### 1. 混淆 KeyDown 和 TextInput

```cpp
// ❌ 问题：在 KeyDown 中处理字符输入
void on_key_down(KeyEventArgs& args) override {
    if (args.key() == Key::A) {
        insert_char('A');  // ❌ 未考虑修饰键（Shift 大小写）
    }
}

// ✅ 解决：使用 TextInput 处理字符输入
void on_text_input(TextInputEventArgs& args) override {
    insert_char(args.char_utf32());  // ✅ 自动处理修饰键
}
```

---

### 2. 忘记转换 UTF-32 到 UTF-8

```cpp
// ❌ 问题：直接将 UTF-32 码点转换为 char
void on_text_input(TextInputEventArgs& args) override {
    uint32_t codepoint = args.char_utf32();
    char ch = static_cast<char>(codepoint);  // ❌ 截断非 ASCII 字符
    m_text.push_back(ch);
}

// ✅ 解决：转换为 UTF-8
void on_text_input(TextInputEventArgs& args) override {
    uint32_t codepoint = args.char_utf32();
    std::string utf8_str = codepoint_to_utf8(codepoint);
    m_text += utf8_str;
}
```

---

### 3. 忘记调用 set_handled

```cpp
// ❌ 问题：忘记调用 set_handled
void on_text_input(TextInputEventArgs& args) override {
    insert_char(args.char_utf32());
    // ❌ 忘记调用 set_handled，事件会继续冒泡
}

// ✅ 解决：调用 set_handled
void on_text_input(TextInputEventArgs& args) override {
    insert_char(args.char_utf32());
    args.set_handled(true);  // 阻止事件冒泡
}
```

---

### 4. 假设所有字符为 ASCII

```cpp
// ❌ 问题：假设所有字符为 ASCII
void on_text_input(TextInputEventArgs& args) override {
    uint32_t codepoint = args.char_utf32();
    
    // ❌ 仅检查 ASCII 范围
    if (codepoint >= 'a' && codepoint <= 'z') {
        handle_lowercase_letter();
    }
    // ❌ 忽略非 ASCII 字符（如中文、日文、韩文）
}

// ✅ 解决：正确处理所有 Unicode 字符
void on_text_input(TextInputEventArgs& args) override {
    uint32_t codepoint = args.char_utf32();
    
    // 处理所有可打印字符
    insert_char(codepoint);
}
```

---

## 完整示例

```cpp
#include <mine/ui/input/TextInputEventArgs.h>
#include <mine/ui/input/KeyEventArgs.h>
#include <mine/ui/input/InputEvents.h>
#include <mine/ui/visual/Visual.h>
#include <iostream>
#include <string>

using namespace mine::ui::input;
using namespace mine::ui::visual;

// ────────────────────────────────────────────────────────────────────────────
// 文本输入框控件
// ────────────────────────────────────────────────────────────────────────────

class TextBox : public Visual {
public:
    TextBox() {
        // 注册 TextInput 事件处理器
        add_handler(TextInputEvent(), &TextBox::on_text_input_router, this);
    }

protected:
    // ════════════════════════════════════════════════════════════════════════
    // KeyDown 事件处理（控制字符）
    // ════════════════════════════════════════════════════════════════════════
    void on_key_down(KeyEventArgs& args) override {
        switch (args.key()) {
            case Key::Backspace:
                std::cout << "Backspace 删除前一个字符" << std::endl;
                delete_previous_char();
                args.set_handled(true);
                break;
                
            case Key::Delete:
                std::cout << "Delete 删除后一个字符" << std::endl;
                delete_next_char();
                args.set_handled(true);
                break;
                
            case Key::Enter:
                std::cout << "Enter 插入换行" << std::endl;
                insert_newline();
                args.set_handled(true);
                break;
                
            case Key::Tab:
                std::cout << "Tab 插入制表符" << std::endl;
                insert_tab();
                args.set_handled(true);
                break;
                
            default:
                Visual::on_key_down(args);
                break;
        }
    }
    
    // ════════════════════════════════════════════════════════════════════════
    // TextInput 事件处理（可打印字符）
    // ════════════════════════════════════════════════════════════════════════
    void on_text_input(TextInputEventArgs& args) {
        uint32_t codepoint = args.char_utf32();
        
        // 转换为 UTF-8
        std::string utf8_str = codepoint_to_utf8(codepoint);
        
        // 输出字符信息
        std::cout << "字符输入: U+" << std::hex << codepoint << " ('" 
                  << utf8_str << "')" << std::dec << std::endl;
        
        // 插入字符到文本缓冲区
        m_text += utf8_str;
        m_char_count++;
        
        // 输出文本缓冲区
        std::cout << "  文本缓冲区: " << m_text << std::endl;
        std::cout << "  字符计数: " << m_char_count << std::endl;
        
        args.set_handled(true);
    }

private:
    std::string m_text;
    int m_char_count = 0;
    
    void on_text_input_router(RoutedEventArgs& args) {
        on_text_input(static_cast<TextInputEventArgs&>(args));
    }
    
    void delete_previous_char() {
        if (!m_text.empty()) {
            // 简化实现：删除最后一个字节
            m_text.pop_back();
            m_char_count--;
            std::cout << "  文本缓冲区: " << m_text << std::endl;
        }
    }
    
    void delete_next_char() {
        std::cout << "  （简化实现：未实现）" << std::endl;
    }
    
    void insert_newline() {
        m_text += '\n';
        m_char_count++;
        std::cout << "  文本缓冲区: " << m_text << std::endl;
    }
    
    void insert_tab() {
        m_text += '\t';
        m_char_count++;
        std::cout << "  文本缓冲区: " << m_text << std::endl;
    }
    
    std::string codepoint_to_utf8(uint32_t cp) {
        std::string result;
        if (cp <= 0x7F) {
            // 1 字节（ASCII）
            result.push_back(static_cast<char>(cp));
        } else if (cp <= 0x7FF) {
            // 2 字节
            result.push_back(static_cast<char>(0xC0 | (cp >> 6)));
            result.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
        } else if (cp <= 0xFFFF) {
            // 3 字节
            result.push_back(static_cast<char>(0xE0 | (cp >> 12)));
            result.push_back(static_cast<char>(0x80 | ((cp >> 6) & 0x3F)));
            result.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
        } else if (cp <= 0x10FFFF) {
            // 4 字节
            result.push_back(static_cast<char>(0xF0 | (cp >> 18)));
            result.push_back(static_cast<char>(0x80 | ((cp >> 12) & 0x3F)));
            result.push_back(static_cast<char>(0x80 | ((cp >> 6) & 0x3F)));
            result.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
        }
        return result;
    }
};

// ────────────────────────────────────────────────────────────────────────────
// 使用示例
// ────────────────────────────────────────────────────────────────────────────

int main() {
    TextBox textbox;
    
    // ════════════════════════════════════════════════════════════════════════
    // 1. ASCII 字符输入
    // ════════════════════════════════════════════════════════════════════════
    
    TextInputEventArgs args1(TextInputEvent(), 'A');  // U+0041
    textbox.on_text_input(args1);
    // 输出：字符输入: U+41 ('A')
    //       文本缓冲区: A
    //       字符计数: 1
    
    TextInputEventArgs args2(TextInputEvent(), 'b');  // U+0062
    textbox.on_text_input(args2);
    // 输出：字符输入: U+62 ('b')
    //       文本缓冲区: Ab
    //       字符计数: 2
    
    // ════════════════════════════════════════════════════════════════════════
    // 2. 中文字符输入
    // ════════════════════════════════════════════════════════════════════════
    
    TextInputEventArgs args3(TextInputEvent(), 0x4E2D);  // '中'
    textbox.on_text_input(args3);
    // 输出：字符输入: U+4e2d ('中')
    //       文本缓冲区: Ab中
    //       字符计数: 3
    
    TextInputEventArgs args4(TextInputEvent(), 0x6587);  // '文'
    textbox.on_text_input(args4);
    // 输出：字符输入: U+6587 ('文')
    //       文本缓冲区: Ab中文
    //       字符计数: 4
    
    // ════════════════════════════════════════════════════════════════════════
    // 3. 表情符号输入
    // ════════════════════════════════════════════════════════════════════════
    
    TextInputEventArgs args5(TextInputEvent(), 0x1F600);  // 😀
    textbox.on_text_input(args5);
    // 输出：字符输入: U+1f600 ('😀')
    //       文本缓冲区: Ab中文😀
    //       字符计数: 5
    
    // ════════════════════════════════════════════════════════════════════════
    // 4. 控制字符（Backspace、Enter）
    // ════════════════════════════════════════════════════════════════════════
    
    KeyEventArgs args6(KeyDownEvent(), Key::Backspace, 0, ModifierKeys::None, false);
    textbox.on_key_down(args6);
    // 输出：Backspace 删除前一个字符
    //       文本缓冲区: Ab中文
    
    KeyEventArgs args7(KeyDownEvent(), Key::Enter, 0, ModifierKeys::None, false);
    textbox.on_key_down(args7);
    // 输出：Enter 插入换行
    //       文本缓冲区: Ab中文
    //                   （换行）
    
    return 0;
}
```

---

## 总结

`TextInputEventArgs` 是 `mine.ui.input` 模块的字符输入路由事件参数类，具备：

1. **路由事件参数**：继承自 `RoutedEventArgs`，支持事件冒泡
2. **Unicode 字符**：包含 UTF-32 编码的 Unicode 码点（可打印字符）
3. **焦点目标**：派发到当前键盘焦点元素
4. **栈分配**：在调用栈上分配，不可拷贝

**与 KeyDown 的区别**：
- `KeyDown`：硬件按键事件，包含虚拟键枚举（Key），处理控制字符（退格、回车等）
- `TextInput`：字符输入事件，包含 UTF-32 码点，已处理修饰键，仅用于可打印字符

**使用建议**：
- **区分 KeyDown 和 TextInput**（KeyDown 处理控制字符，TextInput 处理可打印字符）
- 使用 **UTF-8 存储文本**（跨平台、节省空间）
- 调用 **set_handled** 阻止事件冒泡
- **正确处理多字节字符**（UTF-32 → UTF-8 转换）
- 避免混淆 KeyDown 和 TextInput
- 避免忘记转换 UTF-32 到 UTF-8
- 避免忘记调用 set_handled
- 避免假设所有字符为 ASCII
