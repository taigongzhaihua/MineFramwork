# mine.ui.binding 元文件详细接口文档

## 概述

本文档涵盖 `mine.ui.binding` 模块的元文件：
- **Binding.h**：伞形头文件（包含所有公共类型）
- **ModuleTag.h**：模块标识常量
- **Api.h**：符号导出宏

---

## 1. Binding.h（伞形头文件）

### 文件位置

```
src/mine/ui/binding/api/include/mine/ui/binding/Binding.h
```

### 概述

`Binding.h` 是 `mine.ui.binding` 模块的伞形头文件，包含所有公共类型。

**核心特性：**
- 用户只需引入此单一头文件即可使用绑定模块的全部功能

---

### 包含的头文件

```cpp
#include <mine/ui/binding/Api.h>
#include <mine/ui/binding/BindingConfig.h>
#include <mine/ui/binding/BindingExpression.h>
#include <mine/ui/binding/BindingMode.h>
#include <mine/ui/binding/IConverter.h>
#include <mine/ui/binding/INotifyPropertyChanged.h>
#include <mine/ui/binding/ModuleTag.h>
#include <mine/ui/binding/PropertyDependency.h>
```

**包含的类型**：
- `BindingMode`：绑定方向枚举
- `IConverter`：值转换器接口
- `INotifyPropertyChanged`：属性变更通知接口
- `PropertyDependency`：依赖源描述符
- `Binding` (BindingConfig)：绑定配置结构体
- `BindingExpression`：运行时绑定表达式
- `ModuleTag`：模块标识常量
- `MINE_UI_BINDING_API`：符号导出宏

---

### 使用示例

```cpp
// 只需引入一个头文件
#include <mine/ui/binding/Binding.h>

using namespace mine::ui;

// 可以使用所有绑定类型
BindingMode mode = BindingMode::OneWay;
IConverter* converter = &my_converter;
INotifyPropertyChanged* vm = &my_view_model;
PropertyDependency dep = PropertyDependency::from_inpc(*vm, "name");
BindingExpression expr;
BindingExpression::bind(expr, *vm, "name", label, TextBlock::TextProperty);
```

---

### 最佳实践

#### 1. 优先使用伞形头文件

```cpp
// ✅ 推荐：使用伞形头文件
#include <mine/ui/binding/Binding.h>

// ❌ 不推荐：单独包含多个头文件
#include <mine/ui/binding/BindingMode.h>
#include <mine/ui/binding/IConverter.h>
#include <mine/ui/binding/INotifyPropertyChanged.h>
#include <mine/ui/binding/PropertyDependency.h>
#include <mine/ui/binding/BindingExpression.h>
```

---

#### 2. 减少编译依赖时使用前向声明

```cpp
// 头文件中使用前向声明
namespace mine::ui {
    class BindingExpression;
    class INotifyPropertyChanged;
}

// 实现文件中包含完整头文件
#include <mine/ui/binding/Binding.h>
```

---

## 2. ModuleTag.h（模块标识常量）

### 文件位置

```
src/mine/ui/binding/api/include/mine/ui/binding/ModuleTag.h
```

### 概述

`ModuleTag.h` 定义 `mine.ui.binding` 模块标识常量。

---

### 常量定义

```cpp
namespace mine::ui {

inline constexpr const char* kBindingModuleName = "mine.ui.binding";

} // namespace mine::ui
```

**描述**：`mine.ui.binding` 模块名称常量。

---

### 使用场景

#### 1. 日志记录

```cpp
#include <mine/ui/binding/ModuleTag.h>
#include <mine/core/Log.h>

using namespace mine::ui;

void log_binding_error(const char* message) {
    MINE_LOG_ERROR(kBindingModuleName, message);
}
```

---

#### 2. 模块识别

```cpp
#include <mine/ui/binding/ModuleTag.h>

using namespace mine::ui;

const char* get_module_name() {
    return kBindingModuleName;  // "mine.ui.binding"
}
```

---

#### 3. 反射系统

```cpp
#include <mine/ui/binding/ModuleTag.h>
#include <mine/reflect/Module.h>

using namespace mine::ui;

void register_binding_module() {
    Module module;
    module.set_name(kBindingModuleName);
    // 注册类型...
}
```

---

## 3. Api.h（符号导出宏）

### 文件位置

```
src/mine/ui/binding/api/include/mine/ui/binding/Api.h
```

### 概述

`Api.h` 定义 `mine.ui.binding` 模块的符号导出宏。

---

### 宏定义

```cpp
#if defined(_WIN32)
#    if defined(MINE_SHARED_BUILD)
#        if defined(MINE_BUILDING_MINE_UI_BINDING)
#            define MINE_UI_BINDING_API __declspec(dllexport)
#        else
#            define MINE_UI_BINDING_API __declspec(dllimport)
#        endif
#    else
#        define MINE_UI_BINDING_API
#    endif
#else
#    if defined(MINE_SHARED_BUILD)
#        define MINE_UI_BINDING_API __attribute__((visibility("default")))
#    else
#        define MINE_UI_BINDING_API
#    endif
#endif
```

**描述**：`MINE_UI_BINDING_API` 符号导出宏。

**平台行为**：
- **Windows（MINE_SHARED_BUILD）**：
  - 构建 mine.ui.binding 时：`__declspec(dllexport)`
  - 使用 mine.ui.binding 时：`__declspec(dllimport)`
- **Windows（静态链接）**：空宏
- **Linux/macOS（MINE_SHARED_BUILD）**：`__attribute__((visibility("default")))`
- **Linux/macOS（静态链接）**：空宏

---

### 使用场景

#### 1. 导出类

```cpp
#include <mine/ui/binding/Api.h>

namespace mine::ui {

class MINE_UI_BINDING_API BindingExpression {
    // 类定义...
};

} // namespace mine::ui
```

---

#### 2. 导出函数

```cpp
#include <mine/ui/binding/Api.h>

namespace mine::ui {

MINE_UI_BINDING_API void initialize_binding_module();

} // namespace mine::ui
```

---

#### 3. 导出结构体

```cpp
#include <mine/ui/binding/Api.h>

namespace mine::ui {

struct MINE_UI_BINDING_API Binding {
    // 结构体定义...
};

} // namespace mine::ui
```

---

### 最佳实践

#### 1. 所有公共 API 使用导出宏

```cpp
// ✅ 推荐：公共 API 使用导出宏
class MINE_UI_BINDING_API BindingExpression { /* ... */ };

// ❌ 不推荐：公共 API 忘记导出宏（Windows 动态链接时链接失败）
class BindingExpression { /* ... */ };
```

---

#### 2. 内部实现不使用导出宏

```cpp
// ✅ 推荐：内部实现不使用导出宏
namespace mine::ui::detail {

class BindingExpressionImpl {  // 无导出宏
    // 内部实现...
};

} // namespace mine::ui::detail

// ❌ 不推荐：内部实现使用导出宏（不必要的符号暴露）
namespace mine::ui::detail {

class MINE_UI_BINDING_API BindingExpressionImpl {  // ❌ 不必要
    // 内部实现...
};

} // namespace mine::ui::detail
```

---

#### 3. 模板类不使用导出宏

```cpp
// ✅ 推荐：模板类不使用导出宏（模板在头文件中完整定义）
template<typename T>
class BindingHelper {  // 无导出宏
    // 模板定义...
};

// ❌ 不推荐：模板类使用导出宏（Windows 可能警告）
template<typename T>
class MINE_UI_BINDING_API BindingHelper {  // ❌ 不必要
    // 模板定义...
};
```

---

## 完整示例

```cpp
#include <mine/ui/binding/Binding.h>  // 伞形头文件
#include <mine/ui/binding/ModuleTag.h>
#include <mine/core/Log.h>

using namespace mine::ui;
using namespace mine::core;

// ────────────────────────────────────────────────────────────────────────────
// 使用伞形头文件
// ────────────────────────────────────────────────────────────────────────────

void setup_binding(INotifyPropertyChanged& vm,
                   DependencyObject& target,
                   const DependencyProperty& target_prop) {
    // 所有绑定类型都已包含
    BindingExpression expr;
    BindingExpression::bind(expr, vm, "name", target, target_prop);
    
    MINE_LOG_INFO(kBindingModuleName, "Binding setup complete");
}

// ────────────────────────────────────────────────────────────────────────────
// 使用模块标识常量
// ────────────────────────────────────────────────────────────────────────────

void log_binding_error(const char* message) {
    MINE_LOG_ERROR(kBindingModuleName, message);
}

// ────────────────────────────────────────────────────────────────────────────
// 使用符号导出宏（库实现侧）
// ────────────────────────────────────────────────────────────────────────────

class MINE_UI_BINDING_API MyConverter : public IConverter {
public:
    Variant convert(
        const Variant& value,
        TypeId target_type,
        StringView parameter) const noexcept override {
        // 转换逻辑...
    }
    
    Variant convert_back(
        const Variant& value,
        TypeId target_type,
        StringView parameter) const noexcept override {
        // 反向转换逻辑...
    }
};

int main() {
    // 使用所有绑定功能
    return 0;
}
```

---

## 总结

### Binding.h（伞形头文件）

- **作用**：包含所有公共类型，简化引用
- **使用建议**：优先使用伞形头文件，减少编译依赖时使用前向声明

### ModuleTag.h（模块标识常量）

- **作用**：定义模块名称常量 `kBindingModuleName`
- **使用场景**：日志记录、模块识别、反射系统

### Api.h（符号导出宏）

- **作用**：定义符号导出宏 `MINE_UI_BINDING_API`
- **使用建议**：
  - 所有公共 API 使用导出宏
  - 内部实现不使用导出宏
  - 模板类不使用导出宏
