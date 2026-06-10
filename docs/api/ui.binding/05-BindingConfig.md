# BindingConfig 详细接口文档

## 概述

`Binding` (BindingConfig) 是 `mine.ui.binding` 模块的**绑定描述符**（等价于 WPF 的 `new Binding("PropName") { ... }`）。

**核心特性：**
- **轻量配置对象**：不拥有资源
- **prop_name**：源属性名（必填）
- **mode**：绑定方向（OneWay/TwoWay/OneWayToSource/OneTime）
- **converter**：值转换器（可选）
- **conv_param**：转换器参数（可选）
- **fallback**：回退值（可选）

---

## 文件位置

```
src/mine/ui/binding/api/include/mine/ui/binding/BindingConfig.h
```

---

## 结构体定义

```cpp
struct MINE_UI_BINDING_API Binding {
    /// 源属性名（须与 MINE_OBSERVABLE 宏名称完全一致）—— 必填
    core::StringView prop_name;

    /// 绑定方向，默认 OneWay
    BindingMode mode = BindingMode::OneWay;

    /// 值转换器（可选；调用方负责其生命周期须覆盖绑定存活期）
    IConverter* converter = nullptr;

    /// 传递给 converter.convert() 的参数字符串（须为字符串字面量或长期存活的字符串）
    core::StringView conv_param;

    /// 回退值：当 getter 或 converter 返回空 Variant 时写入目标属性
    core::Variant fallback;
};
```

**描述**：绑定描述符（轻量配置对象，不拥有资源）。

**生命周期约定**：
- `prop_name` / `conv_param` 须为字符串字面量或长期存活的字符串
- `converter` 裸指针，调用方须保证其生命周期覆盖对应控件
- `fallback` 以值形式被 bind() 内部复制，描述符本身析构后仍安全
- 描述符本身在 set_binding() / bind() 返回后即可销毁

---

## 成员字段

### prop_name

```cpp
core::StringView prop_name;
```

**描述**：源属性名（须与 MINE_OBSERVABLE 宏名称完全一致）—— 必填。

**特征**：
- 须为字符串字面量或长期存活的字符串

---

### mode

```cpp
BindingMode mode = BindingMode::OneWay;
```

**描述**：绑定方向，默认 OneWay。

**可选值**：
- `BindingMode::OneWay`：单向绑定（源 → 目标）
- `BindingMode::TwoWay`：双向绑定（源 ↔ 目标）
- `BindingMode::OneWayToSource`：单向回写绑定（目标 → 源）
- `BindingMode::OneTime`：一次性绑定（仅初始化时读取一次）

---

### converter

```cpp
IConverter* converter = nullptr;
```

**描述**：值转换器（可选）。

**特征**：
- 调用方负责其生命周期须覆盖绑定存活期

---

### conv_param

```cpp
core::StringView conv_param;
```

**描述**：传递给 converter.convert() 的参数字符串（须为字符串字面量或长期存活的字符串）。

---

### fallback

```cpp
core::Variant fallback;
```

**描述**：回退值：当 getter 或 converter 返回空 Variant 时写入目标属性。

**特征**：
- 以值形式被 bind() 内部复制，描述符本身析构后仍安全

---

## 使用场景

### 1. 基本绑定（仅指定属性名）

```cpp
#include <mine/ui/binding/BindingConfig.h>
#include <mine/ui/controls/TextBlock.h>

using namespace mine::ui;

TextBlock label;
label.set_binding(TextBlock::TextProperty, Binding{
    .prop_name = "name"  // 绑定到 vm.name
});
```

---

### 2. 指定绑定方向

```cpp
// TwoWay 绑定（可编辑控件）
TextBox textBox;
textBox.set_binding(TextBox::TextProperty, Binding{
    .prop_name = "username",
    .mode = BindingMode::TwoWay
});

// OneTime 绑定（静态内容）
TextBlock appTitle;
appTitle.set_binding(TextBlock::TextProperty, Binding{
    .prop_name = "app_name",
    .mode = BindingMode::OneTime
});
```

---

### 3. 使用转换器

```cpp
// 布尔值转可见性
static BoolToVisibilityConverter bool_to_vis_converter;

Button button;
button.set_binding(Button::VisibilityProperty, Binding{
    .prop_name = "is_enabled",
    .converter = &bool_to_vis_converter
});

// 字节数格式化
static BytesToHumanReadableConverter bytes_converter;

TextBlock sizeLabel;
sizeLabel.set_binding(TextBlock::TextProperty, Binding{
    .prop_name = "file_size",
    .converter = &bytes_converter,
    .conv_param = "MB"  // 参数传递给转换器
});
```

---

### 4. 使用回退值

```cpp
// 回退值：当 vm.name 为空时显示 "未知用户"
TextBlock nameLabel;
nameLabel.set_binding(TextBlock::TextProperty, Binding{
    .prop_name = "name",
    .fallback = Variant{String{"未知用户"}}
});
```

---

### 5. 完整配置示例

```cpp
// 完整配置：绑定方向 + 转换器 + 回退值
static MyConverter my_converter;

TextBlock label;
label.set_binding(TextBlock::TextProperty, Binding{
    .prop_name = "status",
    .mode = BindingMode::OneWay,
    .converter = &my_converter,
    .conv_param = "format_param",
    .fallback = Variant{String{"默认状态"}}
});
```

---

## 最佳实践

### 1. 使用 C++20 指定初始化语法

```cpp
// ✅ 推荐：使用 C++20 指定初始化语法
label.set_binding(TextBlock::TextProperty, Binding{
    .prop_name = "name",
    .mode = BindingMode::OneWay
});

// ❌ 不推荐：手动构造（可读性差）
Binding config;
config.prop_name = "name";
config.mode = BindingMode::OneWay;
label.set_binding(TextBlock::TextProperty, config);
```

---

### 2. 属性名使用字符串字面量

```cpp
// ✅ 推荐：属性名使用字符串字面量
label.set_binding(TextBlock::TextProperty, Binding{
    .prop_name = "name"  // 字符串字面量
});

// ❌ 不推荐：属性名使用临时字符串
String prop_name = "name";
label.set_binding(TextBlock::TextProperty, Binding{
    .prop_name = prop_name  // 危险！临时字符串
});
```

---

### 3. 转换器使用静态对象或单例

```cpp
// ✅ 推荐：转换器使用静态对象
static BoolToVisibilityConverter bool_to_vis_converter;

label.set_binding(TextBlock::VisibilityProperty, Binding{
    .prop_name = "is_visible",
    .converter = &bool_to_vis_converter
});

// ❌ 不推荐：转换器使用局部对象
void bad_setup() {
    BoolToVisibilityConverter converter;  // 局部对象
    label.set_binding(TextBlock::VisibilityProperty, Binding{
        .prop_name = "is_visible",
        .converter = &converter  // 危险！函数返回后 converter 被销毁
    });
}
```

---

### 4. 可选字段按需填写

```cpp
// ✅ 推荐：可选字段按需填写
// 仅需要属性名
label.set_binding(TextBlock::TextProperty, Binding{
    .prop_name = "name"
});

// 需要绑定方向
textBox.set_binding(TextBox::TextProperty, Binding{
    .prop_name = "username",
    .mode = BindingMode::TwoWay
});

// 需要转换器
button.set_binding(Button::VisibilityProperty, Binding{
    .prop_name = "is_enabled",
    .converter = &bool_to_vis_converter
});
```

---

## 常见陷阱

### 1. 属性名使用临时字符串

```cpp
// ❌ 问题：属性名使用临时字符串
String prop_name = "name";
label.set_binding(TextBlock::TextProperty, Binding{
    .prop_name = prop_name  // 危险！prop_name 被销毁后成为悬空引用
});

// ✅ 解决：使用字符串字面量
label.set_binding(TextBlock::TextProperty, Binding{
    .prop_name = "name"
});
```

---

### 2. 转换器生命周期短于绑定

```cpp
// ❌ 问题：转换器生命周期短于绑定
void bad_setup() {
    MyConverter converter;  // 局部对象
    label.set_binding(TextBlock::TextProperty, Binding{
        .prop_name = "value",
        .converter = &converter  // 函数返回后 converter 被销毁
    });
}

// ✅ 解决：使用静态对象或单例
static MyConverter converter;  // 静态对象
void good_setup() {
    label.set_binding(TextBlock::TextProperty, Binding{
        .prop_name = "value",
        .converter = &converter
    });
}
```

---

### 3. conv_param 使用临时字符串

```cpp
// ❌ 问题：conv_param 使用临时字符串
String param = "MB";
label.set_binding(TextBlock::TextProperty, Binding{
    .prop_name = "file_size",
    .converter = &bytes_converter,
    .conv_param = param  // 危险！param 被销毁后成为悬空引用
});

// ✅ 解决：使用字符串字面量
label.set_binding(TextBlock::TextProperty, Binding{
    .prop_name = "file_size",
    .converter = &bytes_converter,
    .conv_param = "MB"
});
```

---

### 4. 忘记设置 prop_name

```cpp
// ❌ 问题：忘记设置 prop_name
label.set_binding(TextBlock::TextProperty, Binding{
    .mode = BindingMode::OneWay  // 缺少 prop_name
});  // 绑定失败

// ✅ 解决：必须设置 prop_name
label.set_binding(TextBlock::TextProperty, Binding{
    .prop_name = "name",  // 必填字段
    .mode = BindingMode::OneWay
});
```

---

## 完整示例

```cpp
#include <mine/ui/binding/BindingConfig.h>
#include <mine/ui/binding/IConverter.h>
#include <mine/ui/controls/TextBlock.h>
#include <mine/ui/controls/TextBox.h>
#include <mine/ui/controls/Button.h>

using namespace mine::ui;
using namespace mine::core;

// ────────────────────────────────────────────────────────────────────────────
// 转换器定义（静态对象）
// ────────────────────────────────────────────────────────────────────────────

static BoolToVisibilityConverter bool_to_vis_converter;
static BytesToHumanReadableConverter bytes_converter;

// ────────────────────────────────────────────────────────────────────────────
// 绑定配置示例
// ────────────────────────────────────────────────────────────────────────────

int main() {
    // ── 基本绑定 ────────────────────────────────────────────────────────
    TextBlock nameLabel;
    nameLabel.set_binding(TextBlock::TextProperty, Binding{
        .prop_name = "name"
    });
    
    // ── TwoWay 绑定 ─────────────────────────────────────────────────────
    TextBox usernameInput;
    usernameInput.set_binding(TextBox::TextProperty, Binding{
        .prop_name = "username",
        .mode = BindingMode::TwoWay
    });
    
    // ── OneTime 绑定 ────────────────────────────────────────────────────
    TextBlock appTitle;
    appTitle.set_binding(TextBlock::TextProperty, Binding{
        .prop_name = "app_name",
        .mode = BindingMode::OneTime
    });
    
    // ── 使用转换器 ──────────────────────────────────────────────────────
    Button button;
    button.set_binding(Button::VisibilityProperty, Binding{
        .prop_name = "is_enabled",
        .converter = &bool_to_vis_converter
    });
    
    // ── 使用转换器 + 参数 ───────────────────────────────────────────────
    TextBlock sizeLabel;
    sizeLabel.set_binding(TextBlock::TextProperty, Binding{
        .prop_name = "file_size",
        .converter = &bytes_converter,
        .conv_param = "MB"
    });
    
    // ── 使用回退值 ──────────────────────────────────────────────────────
    TextBlock statusLabel;
    statusLabel.set_binding(TextBlock::TextProperty, Binding{
        .prop_name = "status",
        .fallback = Variant{String{"未知状态"}}
    });
    
    // ── 完整配置 ────────────────────────────────────────────────────────
    TextBlock complexLabel;
    complexLabel.set_binding(TextBlock::TextProperty, Binding{
        .prop_name = "value",
        .mode = BindingMode::OneWay,
        .converter = &bytes_converter,
        .conv_param = "KB",
        .fallback = Variant{String{"无数据"}}
    });
    
    return 0;
}
```

---

## 总结

`Binding` (BindingConfig) 是 `mine.ui.binding` 模块的绑定描述符，具备：

1. **轻量配置对象**：不拥有资源，描述符本身在 set_binding() / bind() 返回后即可销毁
2. **prop_name**：源属性名（必填，须为字符串字面量）
3. **mode**：绑定方向（OneWay/TwoWay/OneWayToSource/OneTime，默认 OneWay）
4. **converter**：值转换器（可选，须保证生命周期覆盖绑定）
5. **conv_param**：转换器参数（可选，须为字符串字面量）
6. **fallback**：回退值（可选，当 getter 或 converter 返回空 Variant 时使用）

**使用建议**：
- 使用 C++20 指定初始化语法（可读性更好）
- 属性名和参数使用字符串字面量（避免生命周期问题）
- 转换器使用静态对象或单例（确保生命周期覆盖绑定）
- 可选字段按需填写（不需要的字段可以省略）
- prop_name 为必填字段（忘记设置将导致绑定失败）
