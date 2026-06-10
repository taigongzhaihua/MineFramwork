# 06 - ModuleMetadata（模块元数据）

## 概述

`mine.ui.style` 模块包含两个关键的元数据文件，用于模块的导出配置和公开接口管理：

| 文件 | 用途 |
|------|------|
| **Api.h** | 定义 `MINE_UI_STYLE_API` 宏，控制符号的 DLL 导出/导入 |
| **StyleAll.h** | 伞形头文件（umbrella header），统一包含所有公开接口 |

这两个文件是模块化设计的基础设施：
- **Api.h** 确保符号在 DLL 边界的可见性
- **StyleAll.h** 为外部使用者提供便捷的统一引用点

---

## 文件位置

```
src/mine/ui/style/
├── Api.h              ← DLL 导出/导入宏定义
├── StyleAll.h         ← 伞形头文件
├── HandlerId.h        ← 公开接口
├── StyleSetter.h      ← 公开接口
├── Style.h            ← 公开接口
├── ResourceDictionary.h  ← 公开接口
└── VisualStateManager.h  ← 公开接口
```

---

## Api.h 详解

### 完整定义

```cpp
#pragma once

#ifdef MINE_UI_STYLE_EXPORTS
    #define MINE_UI_STYLE_API __declspec(dllexport)
#else
    #define MINE_UI_STYLE_API __declspec(dllimport)
#endif
```

### 工作原理

#### 1. **DLL 导出机制**

在 Windows 平台，动态链接库（DLL）需要显式标记哪些符号可被外部访问：

```cpp
// 编译 mine.ui.style.dll 时（定义 MINE_UI_STYLE_EXPORTS）
#define MINE_UI_STYLE_API __declspec(dllexport)

// 使用 mine.ui.style.dll 的应用程序中（未定义 MINE_UI_STYLE_EXPORTS）
#define MINE_UI_STYLE_API __declspec(dllimport)
```

#### 2. **宏的应用**

```cpp
// 在公开类中使用
class MINE_UI_STYLE_API Style {
public:
    void ApplySetter(const StyleSetter& setter);
    // ...
};

// 在公开函数中使用
MINE_UI_STYLE_API void RegisterGlobalStyles();
```

#### 3. **跨平台兼容性**

在 Linux/macOS 等平台，`__declspec` 无效，可通过条件编译支持：

```cpp
#ifdef _WIN32
    #ifdef MINE_UI_STYLE_EXPORTS
        #define MINE_UI_STYLE_API __declspec(dllexport)
    #else
        #define MINE_UI_STYLE_API __declspec(dllimport)
    #endif
#else
    #define MINE_UI_STYLE_API __attribute__((visibility("default")))
#endif
```

**注意**：当前实现仅支持 Windows，未来版本需扩展跨平台支持。

---

## StyleAll.h 详解

### 完整定义

```cpp
#pragma once

#include <mine/ui/style/HandlerId.h>
#include <mine/ui/style/StyleSetter.h>
#include <mine/ui/style/Style.h>
#include <mine/ui/style/ResourceDictionary.h>
#include <mine/ui/style/VisualStateManager.h>
```

### 设计意图

#### 1. **伞形头文件模式**

提供单一引用点，避免用户手动包含多个头文件：

```cpp
// ❌ 繁琐的多次包含
#include <mine/ui/style/Style.h>
#include <mine/ui/style/StyleSetter.h>
#include <mine/ui/style/ResourceDictionary.h>
// ...

// ✅ 简洁的统一包含
#include <mine/ui/style/StyleAll.h>
```

#### 2. **包含顺序**

文件顺序遵循依赖关系（底层 → 高层）：

```
HandlerId.h           ← 基础类型（属性 ID）
    ↓
StyleSetter.h         ← 属性设置器（依赖 HandlerId）
    ↓
Style.h               ← 样式容器（依赖 StyleSetter）
    ↓
ResourceDictionary.h  ← 资源字典（依赖 Style）
    ↓
VisualStateManager.h  ← 状态机管理（依赖 Style）
```

#### 3. **公开接口清单**

| 文件 | 核心类型 |
|------|----------|
| `HandlerId.h` | `HandlerId` |
| `StyleSetter.h` | `StyleSetter` |
| `Style.h` | `Style` |
| `ResourceDictionary.h` | `ResourceDictionary` |
| `VisualStateManager.h` | `VisualStateManager`, `VisualState`, `VisualStateGroup`, `VisualTransition` |

---

## 使用场景

### 1️⃣ **应用程序引用**

最常见场景：应用程序代码引用样式系统。

```cpp
// main.cpp
#include <mine/ui/style/StyleAll.h>

int main() {
    mine::ui::Style buttonStyle;
    buttonStyle.AddSetter(mine::ui::StyleSetter{
        mine::ui::HandlerId::FromString("Background"),
        mine::paint::Brush::Solid(0xFF4CAF50)
    });
    
    // ...
}
```

### 2️⃣ **导出新 API**

模块内新增公开类时，需在 `StyleAll.h` 中添加：

```cpp
// 新增 StyleSheet.h 后
#pragma once

#include <mine/ui/style/HandlerId.h>
#include <mine/ui/style/StyleSetter.h>
#include <mine/ui/style/Style.h>
#include <mine/ui/style/ResourceDictionary.h>
#include <mine/ui/style/VisualStateManager.h>
#include <mine/ui/style/StyleSheet.h>  // ← 新增
```

### 3️⃣ **模块初始化**

框架内部初始化时，统一引用：

```cpp
// mine/ui/app/Application.cpp
#include <mine/ui/style/StyleAll.h>

void Application::Initialize() {
    // 注册内置样式
    mine::ui::ResourceDictionary globalDict;
    // ...
}
```

### 4️⃣ **单元测试**

测试代码引用完整 API：

```cpp
// tests/ui-style/test_style.cpp
#include <mine/ui/style/StyleAll.h>
#include <gtest/gtest.h>

TEST(StyleTest, ApplySetters) {
    mine::ui::Style style;
    // ...
}
```

### 5️⃣ **预编译头**

大型项目中，可将 `StyleAll.h` 纳入 PCH：

```cpp
// src/pch.h
#pragma once

// 核心依赖
#include <mine/core/CoreAll.h>

// 样式系统
#include <mine/ui/style/StyleAll.h>  // ← 加入 PCH

// 其他常用模块
// ...
```

### 6️⃣ **第三方集成**

第三方库需要样式功能时：

```cpp
// third_party/custom_widget/Widget.cpp
#include <mine/ui/style/StyleAll.h>

namespace custom {

class StyledWidget {
    mine::ui::Style style_;
    // ...
};

} // namespace custom
```

### 7️⃣ **编译配置检查**

验证 DLL 导出宏是否正确配置：

```cpp
// tools/verify_exports.cpp
#include <mine/ui/style/StyleAll.h>

#ifndef MINE_UI_STYLE_API
    #error "MINE_UI_STYLE_API not defined!"
#endif

int main() {
    // 运行时检查符号可见性
    mine::ui::Style* style = new mine::ui::Style();
    delete style;
    return 0;
}
```

### 8️⃣ **文档生成**

Doxygen 等工具可基于 `StyleAll.h` 生成完整 API 文档：

```doxygen
/**
 * @file StyleAll.h
 * @brief mine.ui.style 模块的统一接口入口
 * 
 * 包含所有公开的样式系统接口：
 * - HandlerId: 属性标识符
 * - StyleSetter: 属性设置器
 * - Style: 样式容器
 * - ResourceDictionary: 资源字典
 * - VisualStateManager: 视觉状态管理器
 */
```

---

## 最佳实践

### ✅ **1. 应用程序仅引用 StyleAll.h**

```cpp
// ✅ 推荐：统一引用点
#include <mine/ui/style/StyleAll.h>

// ❌ 避免：逐个引用（除非明确需要最小化编译依赖）
#include <mine/ui/style/Style.h>
#include <mine/ui/style/StyleSetter.h>
```

**原因**：
- 降低维护成本（模块新增接口时，应用代码无需修改）
- 避免遗漏依赖（`StyleAll.h` 保证正确的包含顺序）

### ✅ **2. 编译模块时定义导出宏**

```cmake
# CMakeLists.txt
add_library(mine.ui.style SHARED
    src/Style.cpp
    src/ResourceDictionary.cpp
    # ...
)

target_compile_definitions(mine.ui.style PRIVATE
    MINE_UI_STYLE_EXPORTS  # ← 关键：启用符号导出
)
```

### ✅ **3. 公开类统一添加 API 宏**

```cpp
// ✅ 正确：所有公开类都标记
class MINE_UI_STYLE_API Style { /* ... */ };
class MINE_UI_STYLE_API ResourceDictionary { /* ... */ };

// ❌ 错误：遗漏宏导致链接失败
class Style { /* ... */ };  // 外部无法链接到此类
```

### ✅ **4. 内部实现类不导出**

```cpp
// Style.cpp 内部辅助类
class StyleApplier {  // ← 无需 MINE_UI_STYLE_API
    void ApplyInternal(Visual* target);
};
```

**原因**：减少 DLL 导出符号表大小，避免暴露内部实现。

### ✅ **5. 跨平台兼容性预留**

```cpp
// Api.h（未来版本）
#pragma once

#ifdef _WIN32
    #ifdef MINE_UI_STYLE_EXPORTS
        #define MINE_UI_STYLE_API __declspec(dllexport)
    #else
        #define MINE_UI_STYLE_API __declspec(dllimport)
    #endif
#elif defined(__GNUC__) || defined(__clang__)
    #define MINE_UI_STYLE_API __attribute__((visibility("default")))
#else
    #define MINE_UI_STYLE_API
#endif
```

---

## 常见陷阱

### ❌ **1. 忘记定义导出宏导致链接失败**

**问题**：

```cmake
# CMakeLists.txt
add_library(mine.ui.style SHARED
    src/Style.cpp
)

# ❌ 缺少 MINE_UI_STYLE_EXPORTS 定义
```

**症状**：

```
error LNK2019: unresolved external symbol 
"public: __cdecl mine::ui::Style::Style(void)" 
referenced in function main
```

**修复**：

```cmake
target_compile_definitions(mine.ui.style PRIVATE
    MINE_UI_STYLE_EXPORTS
)
```

---

### ❌ **2. 静态库配置错误**

**问题**：

静态链接时不应使用 `__declspec(dllimport)`，否则链接失败。

**修复**：添加静态链接标志：

```cpp
// Api.h
#pragma once

#ifdef MINE_STATIC_LINK
    #define MINE_UI_STYLE_API  // 静态链接时为空
#else
    #ifdef MINE_UI_STYLE_EXPORTS
        #define MINE_UI_STYLE_API __declspec(dllexport)
    #else
        #define MINE_UI_STYLE_API __declspec(dllimport)
    #endif
#endif
```

```cmake
# 应用程序 CMakeLists.txt（静态链接）
target_compile_definitions(MyApp PRIVATE
    MINE_STATIC_LINK
)
```

---

### ❌ **3. StyleAll.h 包含顺序错误**

**问题**：

```cpp
// ❌ 错误顺序：VisualStateManager 依赖 Style
#include <mine/ui/style/VisualStateManager.h>
#include <mine/ui/style/Style.h>
```

**症状**：

```
error C2065: 'Style': undeclared identifier
```

**修复**：遵循依赖顺序（或直接使用 `StyleAll.h`）：

```cpp
// ✅ 正确顺序
#include <mine/ui/style/Style.h>
#include <mine/ui/style/VisualStateManager.h>

// ✅ 更推荐：使用伞形头文件
#include <mine/ui/style/StyleAll.h>
```

---

### ❌ **4. 模板类导出问题**

**问题**：

```cpp
// ❌ 模板类不能使用 __declspec(dllexport)
template <typename T>
class MINE_UI_STYLE_API StyleTemplate { /* ... */ };
```

**症状**：编译警告 C4251 或链接失败。

**修复**：模板类应在头文件中完整实现，不导出：

```cpp
// ✅ 模板类完整定义在头文件
template <typename T>
class StyleTemplate {  // 无需 MINE_UI_STYLE_API
public:
    void Apply(T value) {
        // 完整实现在头文件
    }
};
```

---

## 完整示例

### 示例 1：模块导出配置

```cpp
// ============================================================
// 文件：src/mine/ui/style/Api.h
// ============================================================
#pragma once

#ifdef MINE_UI_STYLE_EXPORTS
    #define MINE_UI_STYLE_API __declspec(dllexport)
#else
    #define MINE_UI_STYLE_API __declspec(dllimport)
#endif

// ============================================================
// 文件：src/mine/ui/style/StyleAll.h
// ============================================================
#pragma once

#include <mine/ui/style/HandlerId.h>
#include <mine/ui/style/StyleSetter.h>
#include <mine/ui/style/Style.h>
#include <mine/ui/style/ResourceDictionary.h>
#include <mine/ui/style/VisualStateManager.h>

// ============================================================
// 文件：src/mine/ui/style/Style.h
// ============================================================
#pragma once
#include <mine/ui/style/Api.h>
#include <mine/ui/style/StyleSetter.h>
#include <vector>

namespace mine::ui {

/// @brief 样式容器，封装属性设置器集合
class MINE_UI_STYLE_API Style {
public:
    Style() = default;

    /// @brief 添加属性设置器
    void AddSetter(const StyleSetter& setter);

    /// @brief 应用样式到目标对象
    void Apply(Visual* target) const;

    /// @brief 获取所有设置器
    const std::vector<StyleSetter>& GetSetters() const { return setters_; }

private:
    std::vector<StyleSetter> setters_;
};

} // namespace mine::ui

// ============================================================
// 文件：src/mine/ui/style/Style.cpp
// ============================================================
#include "Style.h"
#include <mine/ui/visual/Visual.h>

namespace mine::ui {

void Style::AddSetter(const StyleSetter& setter) {
    setters_.push_back(setter);
}

void Style::Apply(Visual* target) const {
    if (!target) return;

    for (const auto& setter : setters_) {
        target->SetPropertyValue(setter.propertyId, setter.value);
    }
}

} // namespace mine::ui
```

---

### 示例 2：应用程序使用

```cpp
// ============================================================
// 文件：samples/02-controls-demo/main.cpp
// ============================================================
#include <mine/ui/style/StyleAll.h>  // ← 统一引用点
#include <mine/ui/controls/Button.h>
#include <mine/ui/window/Window.h>
#include <mine/ui/app/Application.h>

using namespace mine;
using namespace mine::ui;

int main() {
    Application app;

    // 1. 创建按钮样式
    Style buttonStyle;
    buttonStyle.AddSetter(StyleSetter{
        HandlerId::FromString("Background"),
        paint::Brush::Solid(0xFF2196F3)
    });
    buttonStyle.AddSetter(StyleSetter{
        HandlerId::FromString("Foreground"),
        paint::Brush::Solid(0xFFFFFFFF)
    });
    buttonStyle.AddSetter(StyleSetter{
        HandlerId::FromString("Padding"),
        Thickness{10, 5, 10, 5}
    });

    // 2. 创建状态管理器
    auto vsm = std::make_shared<VisualStateManager>();

    // Normal 状态
    auto normalState = std::make_shared<VisualState>("Normal");
    normalState->AddSetter(StyleSetter{
        HandlerId::FromString("Background"),
        paint::Brush::Solid(0xFF2196F3)
    });

    // Hovered 状态
    auto hoveredState = std::make_shared<VisualState>("Hovered");
    hoveredState->AddSetter(StyleSetter{
        HandlerId::FromString("Background"),
        paint::Brush::Solid(0xFF1976D2)  // 悬停时变深
    });

    // 添加到状态组
    auto commonGroup = std::make_shared<VisualStateGroup>("CommonStates");
    commonGroup->AddState(normalState);
    commonGroup->AddState(hoveredState);
    vsm->AddGroup(commonGroup);

    // 3. 创建按钮并应用样式
    auto button = std::make_shared<controls::Button>();
    button->SetText("Click Me");
    button->SetStyle(buttonStyle);
    button->SetVisualStateManager(vsm);

    // 4. 创建窗口
    auto window = std::make_shared<Window>();
    window->SetTitle("样式演示");
    window->SetSize(math::Size{400, 300});
    window->SetContent(button);

    // 5. 运行应用
    return app.Run(window);
}
```

---

### 示例 3：CMake 构建配置

```cmake
# ============================================================
# 文件：src/mine/ui/style/CMakeLists.txt
# ============================================================
add_library(mine.ui.style SHARED
    # 元数据文件
    Api.h
    StyleAll.h

    # 公开接口
    HandlerId.h
    StyleSetter.h
    Style.h
    Style.cpp
    ResourceDictionary.h
    ResourceDictionary.cpp
    VisualStateManager.h
    VisualStateManager.cpp

    # 内部实现（不导出）
    internal/StyleApplier.h
    internal/StyleApplier.cpp
)

# ✅ 关键：编译模块时定义导出宏
target_compile_definitions(mine.ui.style PRIVATE
    MINE_UI_STYLE_EXPORTS
)

target_include_directories(mine.ui.style
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/src>
        $<INSTALL_INTERFACE:include>
)

target_link_libraries(mine.ui.style
    PUBLIC
        mine.core
        mine.ui.property
        mine.ui.visual
)

# 安装规则
install(TARGETS mine.ui.style
    EXPORT MineUIStyleTargets
    RUNTIME DESTINATION bin
    LIBRARY DESTINATION lib
    ARCHIVE DESTINATION lib
)

install(FILES
    Api.h
    StyleAll.h
    HandlerId.h
    StyleSetter.h
    Style.h
    ResourceDictionary.h
    VisualStateManager.h
    DESTINATION include/mine/ui/style
)

# ============================================================
# 文件：samples/02-controls-demo/CMakeLists.txt
# ============================================================
add_executable(02-controls-demo
    main.cpp
)

# ✅ 应用程序不定义 MINE_UI_STYLE_EXPORTS
# （自动使用 __declspec(dllimport)）
target_link_libraries(02-controls-demo
    PRIVATE
        mine.ui.style  # ← 链接到 DLL
        mine.ui.controls
        mine.ui.window
        mine.ui.app
)

# 复制 DLL 到输出目录
add_custom_command(TARGET 02-controls-demo POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        $<TARGET_FILE:mine.ui.style>
        $<TARGET_FILE_DIR:02-controls-demo>
)
```

---

### 示例 4：预编译头配置

```cpp
// ============================================================
// 文件：src/pch.h（全局预编译头）
// ============================================================
#pragma once

// ============================================================
// 标准库
// ============================================================
#include <memory>
#include <string>
#include <vector>
#include <unordered_map>
#include <functional>
#include <optional>

// ============================================================
// MineFramework 核心模块
// ============================================================
#include <mine/core/CoreAll.h>
#include <mine/math/MathAll.h>
#include <mine/containers/ContainersAll.h>

// ============================================================
// UI 基础设施
// ============================================================
#include <mine/ui/property/PropertyAll.h>
#include <mine/ui/style/StyleAll.h>       // ← 样式系统
#include <mine/ui/visual/VisualAll.h>

// ============================================================
// 平台抽象
// ============================================================
#include <mine/platform/PlatformAll.h>

// ============================================================
// 注意：预编译头仅包含稳定、频繁使用的接口
// ============================================================
```

```cmake
# ============================================================
# 文件：CMakeLists.txt（根目录）
# ============================================================
# 启用预编译头
option(USE_PRECOMPILED_HEADERS "使用预编译头加速编译" ON)

if(USE_PRECOMPILED_HEADERS)
    # 为所有模块设置 PCH
    target_precompile_headers(mine.ui.style
        PUBLIC
            <mine/ui/style/StyleAll.h>
    )

    target_precompile_headers(mine.ui.controls
        REUSE_FROM mine.ui.style  # ← 复用 PCH
    )
endif()
```

---

## 总结

### 核心要点

| 文件 | 职责 | 关键规则 |
|------|------|----------|
| **Api.h** | DLL 导出/导入控制 | • 编译模块时定义 `MINE_UI_STYLE_EXPORTS`<br>• 公开类/函数必须标记 `MINE_UI_STYLE_API`<br>• 静态链接需特殊处理 |
| **StyleAll.h** | 统一公开接口入口 | • 应用代码优先使用此头文件<br>• 新增公开接口必须添加到此文件<br>• 遵循依赖顺序包含 |

### 设计意图

1. **Api.h**：确保跨 DLL 边界的符号可见性，支持动态链接库的正确导出/导入。
2. **StyleAll.h**：提供便捷的统一引用点，降低外部使用者的维护成本，避免遗漏依赖。

### 最佳实践回顾

✅ **应用程序仅引用 StyleAll.h**  
✅ **编译模块时定义 MINE_UI_STYLE_EXPORTS**  
✅ **公开类统一添加 MINE_UI_STYLE_API**  
✅ **内部实现类不导出**  
✅ **跨平台兼容性预留**  

### 常见错误

❌ 忘记定义导出宏 → 链接失败  
❌ 静态库配置错误 → 链接失败  
❌ 包含顺序错误 → 编译失败  
❌ 模板类导出问题 → 编译警告/失败  

---

**相关文档**：
- [01-HandlerId.md](01-HandlerId.md) - 属性标识符
- [02-StyleSetter.md](02-StyleSetter.md) - 属性设置器
- [03-Style.md](03-Style.md) - 样式容器
- [04-ResourceDictionary.md](04-ResourceDictionary.md) - 资源字典
- [05-VisualStateManager.md](05-VisualStateManager.md) - 视觉状态管理器
