# IConverter 详细接口文档

## 概述

`IConverter` 是 `mine.ui.binding` 模块的**绑定值转换器接口**。

**核心特性：**
- **convert()**：正向转换（源 → 目标，OneWay 方向）
- **convert_back()**：反向转换（目标 → 源，TwoWay 回写方向）
- **转换失败处理**：返回空 Variant，BindingExpression 使用 fallback 值代替

---

## 文件位置

```
src/mine/ui/binding/api/include/mine/ui/binding/IConverter.h
```

---

## 接口定义

```cpp
struct MINE_UI_BINDING_API IConverter {
    virtual ~IConverter() = default;

    /**
     * @brief 正向转换：将源值转换为目标类型（OneWay 方向使用）。
     *
     * @param value       来自 getter 的源值
     * @param target_type 目标 DependencyProperty 的值类型（TypeId）
     * @param parameter   MML 中 converter 的 parameter 字符串（可为空）
     * @return 转换后的值；失败时返回空 Variant
     */
    [[nodiscard]] virtual core::Variant convert(
        const core::Variant& value,
        core::TypeId         target_type,
        core::StringView     parameter) const noexcept = 0;

    /**
     * @brief 反向转换：将目标值转换回源类型（TwoWay 回写时使用，M2 实现）。
     *
     * M1.1 中此方法不会被调用，实现者可返回空 Variant 作为占位。
     *
     * @param value       来自目标属性的当前值
     * @param target_type 源对象期望的值类型
     * @param parameter   MML 中 converter 的 parameter 字符串（可为空）
     * @return 转换后的源值；失败时返回空 Variant
     */
    [[nodiscard]] virtual core::Variant convert_back(
        const core::Variant& value,
        core::TypeId         target_type,
        core::StringView     parameter) const noexcept = 0;
};
```

**描述**：绑定值转换器接口。

**特征**：
- 实现者须确保两个方法均为 noexcept（不抛异常）
- 转换失败时应返回空 Variant（即 Variant{}）
- BindingExpression 将使用 fallback 值代替失败的转换结果

---

## 成员方法

### convert()

```cpp
[[nodiscard]] virtual core::Variant convert(
    const core::Variant& value,
    core::TypeId         target_type,
    core::StringView     parameter) const noexcept = 0;
```

**描述**：正向转换：将源值转换为目标类型（OneWay 方向使用）。

**参数**：
- `value`：来自 getter 的源值
- `target_type`：目标 DependencyProperty 的值类型（TypeId）
- `parameter`：MML 中 converter 的 parameter 字符串（可为空）

**返回值**：转换后的值；失败时返回空 Variant。

**使用场景**：
- OneWay 绑定：源值 → 转换 → 目标属性
- TwoWay 绑定（正向）：源值 → 转换 → 目标属性

---

### convert_back()

```cpp
[[nodiscard]] virtual core::Variant convert_back(
    const core::Variant& value,
    core::TypeId         target_type,
    core::StringView     parameter) const noexcept = 0;
```

**描述**：反向转换：将目标值转换回源类型（TwoWay 回写时使用，M2 实现）。

**参数**：
- `value`：来自目标属性的当前值
- `target_type`：源对象期望的值类型
- `parameter`：MML 中 converter 的 parameter 字符串（可为空）

**返回值**：转换后的源值；失败时返回空 Variant。

**使用场景**：
- TwoWay 绑定（反向）：目标属性 → 反向转换 → 源值

**注意**：
- M1.1 中此方法不会被调用，实现者可返回空 Variant 作为占位

---

## 使用场景

### 1. 布尔值转可见性（BoolToVisibilityConverter）

```cpp
#include <mine/ui/binding/IConverter.h>
#include <mine/ui/Visibility.h>

using namespace mine::ui;
using namespace mine::core;

class BoolToVisibilityConverter : public IConverter {
public:
    Variant convert(
        const Variant& value,
        TypeId target_type,
        StringView parameter) const noexcept override {
        
        // 转换：bool → Visibility
        if (!value.has<bool>()) return Variant{};
        
        bool is_visible = value.get<bool>();
        return Variant{is_visible ? Visibility::Visible : Visibility::Collapsed};
    }
    
    Variant convert_back(
        const Variant& value,
        TypeId target_type,
        StringView parameter) const noexcept override {
        
        // 反向转换：Visibility → bool
        if (!value.has<Visibility>()) return Variant{};
        
        Visibility vis = value.get<Visibility>();
        return Variant{vis == Visibility::Visible};
    }
};

// MML 用法：
// <Button visibility: bind(vm.IsEnabled, converter: BoolToVisibility) />
```

---

### 2. 字节数格式化（BytesToHumanReadableConverter）

```cpp
class BytesToHumanReadableConverter : public IConverter {
public:
    Variant convert(
        const Variant& value,
        TypeId target_type,
        StringView parameter) const noexcept override {
        
        // 转换：int64 → String（人类可读格式）
        if (!value.has<int64_t>()) return Variant{};
        
        int64_t bytes = value.get<int64_t>();
        String unit = parameter.empty() ? "KB" : String{parameter};
        
        if (unit == "MB") {
            return Variant{String{std::format("{:.2f} MB", bytes / 1024.0 / 1024.0)}};
        } else if (unit == "KB") {
            return Variant{String{std::format("{:.2f} KB", bytes / 1024.0)}};
        } else {
            return Variant{String{std::format("{} bytes", bytes)}};
        }
    }
    
    Variant convert_back(
        const Variant& value,
        TypeId target_type,
        StringView parameter) const noexcept override {
        
        // 反向转换不支持（只读转换）
        return Variant{};
    }
};

// MML 用法：
// <TextBlock text: bind(vm.FileSize, converter: BytesToHumanReadable, parameter: "MB") />
```

---

### 3. 枚举转字符串（EnumToStringConverter）

```cpp
enum class Status { Idle, Running, Completed, Failed };

class StatusToStringConverter : public IConverter {
public:
    Variant convert(
        const Variant& value,
        TypeId target_type,
        StringView parameter) const noexcept override {
        
        // 转换：Status → String
        if (!value.has<Status>()) return Variant{};
        
        Status status = value.get<Status>();
        switch (status) {
            case Status::Idle:      return Variant{String{"空闲"}};
            case Status::Running:   return Variant{String{"运行中"}};
            case Status::Completed: return Variant{String{"已完成"}};
            case Status::Failed:    return Variant{String{"失败"}};
            default:                return Variant{};
        }
    }
    
    Variant convert_back(
        const Variant& value,
        TypeId target_type,
        StringView parameter) const noexcept override {
        
        // 反向转换：String → Status
        if (!value.has<String>()) return Variant{};
        
        String str = value.get<String>();
        if (str == "空闲")       return Variant{Status::Idle};
        if (str == "运行中")     return Variant{Status::Running};
        if (str == "已完成")     return Variant{Status::Completed};
        if (str == "失败")       return Variant{Status::Failed};
        return Variant{};
    }
};

// MML 用法：
// <TextBlock text: bind(vm.Status, converter: StatusToString) />
```

---

### 4. 数值加法转换（AddValueConverter）

```cpp
class AddValueConverter : public IConverter {
public:
    Variant convert(
        const Variant& value,
        TypeId target_type,
        StringView parameter) const noexcept override {
        
        // 转换：double + offset → double
        if (!value.has<double>()) return Variant{};
        
        double offset = 0.0;
        if (!parameter.empty()) {
            offset = std::strtod(parameter.data(), nullptr);
        }
        
        double result = value.get<double>() + offset;
        return Variant{result};
    }
    
    Variant convert_back(
        const Variant& value,
        TypeId target_type,
        StringView parameter) const noexcept override {
        
        // 反向转换：double - offset → double
        if (!value.has<double>()) return Variant{};
        
        double offset = 0.0;
        if (!parameter.empty()) {
            offset = std::strtod(parameter.data(), nullptr);
        }
        
        double result = value.get<double>() - offset;
        return Variant{result};
    }
};

// MML 用法：
// <Slider value: bind(vm.Temperature, converter: AddValue, parameter: "273.15") />
// （将摄氏度转换为开尔文）
```

---

## 转换失败处理

### 返回空 Variant

```cpp
class MyConverter : public IConverter {
public:
    Variant convert(
        const Variant& value,
        TypeId target_type,
        StringView parameter) const noexcept override {
        
        // 转换失败时返回空 Variant
        if (!value.has<int>()) {
            return Variant{};  // ❌ 转换失败
        }
        
        // 转换成功
        return Variant{String{std::to_string(value.get<int>())}};
    }
    
    Variant convert_back(...) const noexcept override {
        return Variant{};  // 不支持反向转换
    }
};
```

### BindingExpression 使用 fallback 值

```cpp
// MML 用法：
// <TextBlock text: bind(vm.Value, converter: MyConverter, fallback: "默认值") />

// 如果转换失败（返回空 Variant），BindingExpression 使用 "默认值"
```

---

## 最佳实践

### 1. 确保 noexcept

```cpp
// ✅ 推荐：所有转换方法都标记 noexcept
class MyConverter : public IConverter {
public:
    Variant convert(...) const noexcept override {
        try {
            // 转换逻辑
        } catch (...) {
            return Variant{};  // 捕获异常，返回空 Variant
        }
    }
};

// ❌ 不推荐：抛出异常（违反 noexcept 约定）
class BadConverter : public IConverter {
public:
    Variant convert(...) const noexcept override {
        throw std::runtime_error("转换失败");  // ❌ 违反 noexcept
    }
};
```

---

### 2. 转换失败返回空 Variant

```cpp
// ✅ 推荐：转换失败返回空 Variant
Variant convert(const Variant& value, ...) const noexcept override {
    if (!value.has<int>()) {
        return Variant{};  // ✅ 转换失败
    }
    // 转换逻辑
}

// ❌ 不推荐：转换失败返回默认值（可能造成误解）
Variant convert(const Variant& value, ...) const noexcept override {
    if (!value.has<int>()) {
        return Variant{0};  // ❌ 可能造成误解
    }
    // 转换逻辑
}
```

---

### 3. 使用 parameter 传递转换参数

```cpp
// ✅ 推荐：使用 parameter 传递转换参数
class FormatConverter : public IConverter {
public:
    Variant convert(
        const Variant& value,
        TypeId target_type,
        StringView parameter) const noexcept override {
        
        // parameter 作为格式字符串
        String format = parameter.empty() ? "{}" : String{parameter};
        // 使用 format 格式化 value
    }
};

// MML 用法：
// <TextBlock text: bind(vm.Value, converter: Format, parameter: "{:.2f}") />
```

---

### 4. 只读转换不实现 convert_back()

```cpp
// ✅ 推荐：只读转换不实现 convert_back()
class ReadOnlyConverter : public IConverter {
public:
    Variant convert(...) const noexcept override {
        // 正向转换逻辑
    }
    
    Variant convert_back(...) const noexcept override {
        return Variant{};  // ✅ 不支持反向转换
    }
};
```

---

## 常见陷阱

### 1. 转换失败抛出异常

```cpp
// ❌ 问题：转换失败抛出异常
class BadConverter : public IConverter {
public:
    Variant convert(const Variant& value, ...) const noexcept override {
        if (!value.has<int>()) {
            throw std::runtime_error("转换失败");  // ❌ 违反 noexcept
        }
        // ...
    }
};

// ✅ 解决：返回空 Variant
class GoodConverter : public IConverter {
public:
    Variant convert(const Variant& value, ...) const noexcept override {
        if (!value.has<int>()) {
            return Variant{};  // ✅ 转换失败
        }
        // ...
    }
};
```

---

### 2. convert_back() 未实现但 TwoWay 绑定使用转换器

```cpp
// ❌ 问题：convert_back() 返回空 Variant，但 TwoWay 绑定使用转换器
class MyConverter : public IConverter {
public:
    Variant convert(...) const noexcept override {
        // 正向转换
    }
    
    Variant convert_back(...) const noexcept override {
        return Variant{};  // ❌ 不支持反向转换
    }
};

// <TextBox text: bind(vm.Value, mode: TwoWay, converter: MyConverter) />
// 用户输入时尝试反向转换，但返回空 Variant，回写失败

// ✅ 解决：实现 convert_back() 或使用 OneWay 绑定
// <TextBlock text: bind(vm.Value, converter: MyConverter) />
```

---

### 3. parameter 未处理空字符串

```cpp
// ❌ 问题：parameter 未处理空字符串
class MyConverter : public IConverter {
public:
    Variant convert(
        const Variant& value,
        TypeId target_type,
        StringView parameter) const noexcept override {
        
        // 直接使用 parameter，可能为空
        double offset = std::strtod(parameter.data(), nullptr);  // ❌ parameter 可能为空
        // ...
    }
};

// ✅ 解决：检查 parameter 是否为空
Variant convert(
    const Variant& value,
    TypeId target_type,
    StringView parameter) const noexcept override {
    
    double offset = 0.0;
    if (!parameter.empty()) {
        offset = std::strtod(parameter.data(), nullptr);
    }
    // ...
}
```

---

## 完整示例

```cpp
#include <mine/ui/binding/IConverter.h>
#include <mine/ui/binding/Binding.h>
#include <mine/ui/controls/TextBlock.h>
#include <mine/core/Variant.h>
#include <format>

using namespace mine::ui;
using namespace mine::core;

// ────────────────────────────────────────────────────────────────────────────
// 布尔值转可见性转换器
// ────────────────────────────────────────────────────────────────────────────

class BoolToVisibilityConverter : public IConverter {
public:
    Variant convert(
        const Variant& value,
        TypeId target_type,
        StringView parameter) const noexcept override {
        
        // 转换：bool → Visibility
        if (!value.has<bool>()) return Variant{};
        
        bool is_visible = value.get<bool>();
        return Variant{is_visible ? Visibility::Visible : Visibility::Collapsed};
    }
    
    Variant convert_back(
        const Variant& value,
        TypeId target_type,
        StringView parameter) const noexcept override {
        
        // 反向转换：Visibility → bool
        if (!value.has<Visibility>()) return Variant{};
        
        Visibility vis = value.get<Visibility>();
        return Variant{vis == Visibility::Visible};
    }
};

// ────────────────────────────────────────────────────────────────────────────
// 字节数格式化转换器
// ────────────────────────────────────────────────────────────────────────────

class BytesToHumanReadableConverter : public IConverter {
public:
    Variant convert(
        const Variant& value,
        TypeId target_type,
        StringView parameter) const noexcept override {
        
        // 转换：int64 → String（人类可读格式）
        if (!value.has<int64_t>()) return Variant{};
        
        int64_t bytes = value.get<int64_t>();
        String unit = parameter.empty() ? "KB" : String{parameter};
        
        try {
            if (unit == "MB") {
                return Variant{String{std::format("{:.2f} MB", bytes / 1024.0 / 1024.0)}};
            } else if (unit == "KB") {
                return Variant{String{std::format("{:.2f} KB", bytes / 1024.0)}};
            } else {
                return Variant{String{std::format("{} bytes", bytes)}};
            }
        } catch (...) {
            return Variant{};  // 格式化失败
        }
    }
    
    Variant convert_back(
        const Variant& value,
        TypeId target_type,
        StringView parameter) const noexcept override {
        
        // 反向转换不支持（只读转换）
        return Variant{};
    }
};

// ────────────────────────────────────────────────────────────────────────────
// 使用示例
// ────────────────────────────────────────────────────────────────────────────

int main() {
    // 注册转换器（全局单例）
    static BoolToVisibilityConverter bool_to_vis_converter;
    static BytesToHumanReadableConverter bytes_to_human_converter;
    
    // ── 布尔值转可见性 ──────────────────────────────────────────────────
    TextBlock label;
    label.set_binding(TextBlock::VisibilityProperty,
        Binding::create("vm.IsEnabled")
            .converter(&bool_to_vis_converter));
    
    // vm.IsEnabled = true -> label.visibility() = Visibility::Visible
    // vm.IsEnabled = false -> label.visibility() = Visibility::Collapsed
    
    // ── 字节数格式化 ────────────────────────────────────────────────────
    TextBlock sizeLabel;
    sizeLabel.set_binding(TextBlock::TextProperty,
        Binding::create("vm.FileSize")
            .converter(&bytes_to_human_converter)
            .parameter("MB"));
    
    // vm.FileSize = 1048576 -> sizeLabel.text() = "1.00 MB"
    // vm.FileSize = 2097152 -> sizeLabel.text() = "2.00 MB"
    
    return 0;
}
```

---

## 总结

`IConverter` 是 `mine.ui.binding` 模块的绑定值转换器接口，具备：

1. **convert()**：正向转换（源 → 目标，OneWay 方向）
2. **convert_back()**：反向转换（目标 → 源，TwoWay 回写方向）
3. **转换失败处理**：返回空 Variant，BindingExpression 使用 fallback 值代替

**使用建议**：
- 确保 noexcept（转换失败捕获异常，返回空 Variant）
- 转换失败返回空 Variant（不要返回默认值）
- 使用 parameter 传递转换参数
- 只读转换不实现 convert_back()（返回空 Variant）
- TwoWay 绑定使用转换器时必须实现 convert_back()
- 处理 parameter 为空的情况
