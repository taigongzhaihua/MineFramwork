# mine.ui.layout 模块元数据文档

## 概述

本文档介绍 `mine.ui.layout` 模块的**元数据文件**，包括 API 导出宏、模块标识常量和伞形头文件。

**核心文件：**
- **Api.h**：DLL 导出宏定义（MINE_UI_LAYOUT_API）
- **ModuleTag.h**：模块名称常量（kLayoutModuleName）
- **LayoutAll.h**：伞形头文件（包含所有公开类型）

**用途：**
- **Api.h**：跨平台 DLL 导出/导入支持
- **ModuleTag.h**：日志和诊断信息标识
- **LayoutAll.h**：一次性包含所有布局类型

---

## 文件位置

```
src/mine/ui/layout/api/include/mine/ui/layout/Api.h
src/mine/ui/layout/api/include/mine/ui/layout/ModuleTag.h
src/mine/ui/layout/api/include/mine/ui/layout/LayoutAll.h
```

---

## Api.h - DLL 导出宏

### 文件内容

```cpp
/**
 * @file Api.h
 * @brief mine.ui.layout 模块 DLL 导出宏定义。
 */

#pragma once

#if defined(_MSC_VER) || defined(__MINGW32__)
#    if defined(MINE_BUILDING_MINE_UI_LAYOUT)
#        define MINE_UI_LAYOUT_API __declspec(dllexport)
#    elif defined(MINE_BUILD_SHARED)
#        define MINE_UI_LAYOUT_API __declspec(dllimport)
#    else
#        define MINE_UI_LAYOUT_API
#    endif
#elif defined(__GNUC__) || defined(__clang__)
#    if defined(MINE_BUILDING_MINE_UI_LAYOUT)
#        define MINE_UI_LAYOUT_API __attribute__((visibility("default")))
#    else
#        define MINE_UI_LAYOUT_API
#    endif
#else
#    define MINE_UI_LAYOUT_API
#endif
```

---

### 宏定义说明

#### MINE_UI_LAYOUT_API

**描述**：mine.ui.layout 模块的 DLL 导出/导入宏。

**平台支持：**
- **Windows（MSVC / MinGW）**：
  - 编译模块时（MINE_BUILDING_MINE_UI_LAYOUT 定义）：`__declspec(dllexport)`
  - 使用模块时（MINE_BUILD_SHARED 定义）：`__declspec(dllimport)`
  - 静态链接（无宏定义）：空宏

- **Linux / macOS（GCC / Clang）**：
  - 编译模块时（MINE_BUILDING_MINE_UI_LAYOUT 定义）：`__attribute__((visibility("default")))`
  - 使用模块时：空宏

- **其他平台**：空宏

---

### 使用场景

#### 1. 导出类

```cpp
#include <mine/ui/layout/Api.h>
#include <mine/ui/layout/Panel.h>

namespace mine::ui {

// Panel 类使用 MINE_UI_LAYOUT_API 导出
class MINE_UI_LAYOUT_API Panel : public FrameworkElement {
public:
    Panel();
    ~Panel() override;
    
    void add_child(UIElement* child);
    void remove_child(UIElement* child);
    // ...
};

} // namespace mine::ui
```

---

#### 2. 导出函数

```cpp
#include <mine/ui/layout/Api.h>

namespace mine::ui {

// 导出全局函数
MINE_UI_LAYOUT_API void initialize_layout_system();
MINE_UI_LAYOUT_API void shutdown_layout_system();

} // namespace mine::ui
```

---

#### 3. 导出全局变量

```cpp
#include <mine/ui/layout/Api.h>

namespace mine::ui {

// 导出全局变量（不推荐）
MINE_UI_LAYOUT_API extern int g_layout_version;

} // namespace mine::ui
```

---

### 最佳实践

#### ✅ 推荐：所有公开类使用 MINE_UI_LAYOUT_API

```cpp
#include <mine/ui/layout/Api.h>

namespace mine::ui {

// ✅ 公开类使用导出宏
class MINE_UI_LAYOUT_API Panel : public FrameworkElement {
public:
    // 公开方法
    void add_child(UIElement* child);
};

// ❌ 内部类不使用导出宏
class PanelPrivateImpl {
    // 仅内部使用
};

} // namespace mine::ui
```

**理由**：
- 公开类需要跨模块访问，必须导出
- 内部类仅在模块内部使用，无需导出

---

#### ❌ 不推荐：内部类使用导出宏

```cpp
#include <mine/ui/layout/Api.h>

namespace mine::ui {

// ❌ 内部类不应导出
class MINE_UI_LAYOUT_API PanelPrivateImpl {
    // 仅内部使用
};

} // namespace mine::ui
```

**问题**：
- 增加 DLL 体积
- 暴露内部实现细节
- 可能导致 ABI 兼容性问题

---

## ModuleTag.h - 模块标识常量

### 文件内容

```cpp
/**
 * @file ModuleTag.h
 * @brief mine.ui.layout 模块标识常量。
 */

#pragma once

namespace mine::ui {

/// 模块名称常量，用于日志和诊断信息标识
inline constexpr const char* kLayoutModuleName = "mine.ui.layout";

} // namespace mine::ui
```

---

### 常量说明

#### kLayoutModuleName

**描述**：mine.ui.layout 模块名称常量。

**类型**：`const char*`

**值**：`"mine.ui.layout"`

**用途**：
- 日志输出（标识日志来源模块）
- 诊断信息（错误消息、警告）
- 模块版本信息
- 性能分析

---

### 使用场景

#### 1. 日志输出

```cpp
#include <mine/ui/layout/ModuleTag.h>
#include <mine/core/Logger.h>

namespace mine::ui {

void Panel::add_child(UIElement* child) {
    if (!child) {
        LOG_ERROR(kLayoutModuleName, "add_child: child is null");
        return;
    }
    
    LOG_INFO(kLayoutModuleName, "add_child: child added");
    children_.push_back(child);
}

} // namespace mine::ui
```

---

#### 2. 错误消息

```cpp
#include <mine/ui/layout/ModuleTag.h>
#include <stdexcept>
#include <string>

namespace mine::ui {

void Grid::add_row(RowDefinition def) {
    if (def.height.value < 0) {
        throw std::invalid_argument(
            std::string(kLayoutModuleName) + 
            ": row height cannot be negative"
        );
    }
    // ...
}

} // namespace mine::ui
```

---

#### 3. 性能分析

```cpp
#include <mine/ui/layout/ModuleTag.h>
#include <mine/core/Profiler.h>

namespace mine::ui {

math::Size Grid::measure_override(math::Size available) {
    PROFILE_SCOPE(kLayoutModuleName, "Grid::measure_override");
    
    // 测量逻辑...
    
    return desired_size;
}

} // namespace mine::ui
```

---

#### 4. 模块版本信息

```cpp
#include <mine/ui/layout/ModuleTag.h>
#include <iostream>

namespace mine::ui {

void print_module_info() {
    std::cout << "Module: " << kLayoutModuleName << "\n";
    std::cout << "Version: 1.0.0\n";
    std::cout << "Author: MineUI Team\n";
}

} // namespace mine::ui
```

---

### 最佳实践

#### ✅ 推荐：使用常量标识模块

```cpp
#include <mine/ui/layout/ModuleTag.h>
#include <mine/core/Logger.h>

namespace mine::ui {

void Panel::add_child(UIElement* child) {
    LOG_INFO(kLayoutModuleName, "add_child called");
    // ...
}

} // namespace mine::ui
```

**理由**：
- 统一模块标识
- 便于日志过滤和分析
- 避免硬编码字符串

---

#### ❌ 不推荐：硬编码模块名称

```cpp
#include <mine/core/Logger.h>

namespace mine::ui {

void Panel::add_child(UIElement* child) {
    LOG_INFO("mine.ui.layout", "add_child called");  // ❌ 硬编码
    // ...
}

} // namespace mine::ui
```

**问题**：
- 容易拼写错误
- 修改模块名称时需要全局替换
- 不利于维护

---

## LayoutAll.h - 伞形头文件

### 文件内容

```cpp
/**
 * @file LayoutAll.h
 * @brief mine.ui.layout 模块伞形头文件，包含所有公开类型。
 */

#pragma once

#include <mine/ui/layout/Api.h>
#include <mine/ui/layout/ModuleTag.h>
#include <mine/ui/visual/HorizontalAlignment.h>
#include <mine/ui/visual/VerticalAlignment.h>
#include <mine/ui/layout/Orientation.h>
#include <mine/ui/layout/GridLength.h>
#include <mine/ui/visual/FrameworkElement.h>
#include <mine/ui/layout/Panel.h>
#include <mine/ui/layout/StackPanel.h>
#include <mine/ui/layout/Grid.h>
```

---

### 包含的头文件

| 头文件 | 描述 |
|--------|------|
| `Api.h` | DLL 导出宏定义 |
| `ModuleTag.h` | 模块标识常量 |
| `HorizontalAlignment.h` | 水平对齐枚举（来自 mine.ui.visual） |
| `VerticalAlignment.h` | 垂直对齐枚举（来自 mine.ui.visual） |
| `Orientation.h` | 布局方向枚举 |
| `GridLength.h` | Grid 行列尺寸描述符 |
| `FrameworkElement.h` | 框架元素基类（来自 mine.ui.visual） |
| `Panel.h` | 布局面板基类 |
| `StackPanel.h` | 线性堆叠布局面板 |
| `Grid.h` | 行列网格布局面板 |

---

### 使用场景

#### 1. 快速包含所有布局类型

```cpp
#include <mine/ui/layout/LayoutAll.h>

using namespace mine::ui;

int main() {
    // 直接使用所有布局类型
    StackPanel* stack = new StackPanel();
    Grid* grid = new Grid();
    Panel* panel = new Panel();
    
    // ...
    
    return 0;
}
```

---

#### 2. 简化头文件包含

```cpp
// ❌ 不使用 LayoutAll.h
#include <mine/ui/layout/Panel.h>
#include <mine/ui/layout/StackPanel.h>
#include <mine/ui/layout/Grid.h>
#include <mine/ui/layout/Orientation.h>
#include <mine/ui/layout/GridLength.h>

// ✅ 使用 LayoutAll.h
#include <mine/ui/layout/LayoutAll.h>
```

---

#### 3. 库接口头文件

```cpp
// MyLibrary.h
#pragma once

#include <mine/ui/layout/LayoutAll.h>

namespace mylibrary {

class MyCustomPanel : public mine::ui::Panel {
    // 使用 mine.ui.layout 中的所有类型
};

} // namespace mylibrary
```

---

### 最佳实践

#### ✅ 推荐：应用代码使用 LayoutAll.h

```cpp
// main.cpp
#include <mine/ui/layout/LayoutAll.h>

using namespace mine::ui;

int main() {
    StackPanel* panel = new StackPanel();
    // ...
    return 0;
}
```

**理由**：
- 简化头文件包含
- 编译速度影响小（应用代码）
- 代码更简洁

---

#### ❌ 不推荐：库头文件使用 LayoutAll.h

```cpp
// MyLibrary.h
#pragma once

#include <mine/ui/layout/LayoutAll.h>  // ❌ 库头文件不应包含伞形头文件

namespace mylibrary {

class MyCustomPanel : public mine::ui::Panel {
    // ...
};

} // namespace mylibrary
```

**问题**：
- 增加编译时间（所有包含 MyLibrary.h 的文件都需要编译伞形头文件）
- 增加依赖（MyLibrary.h 依赖 LayoutAll.h 中的所有头文件）
- 不利于模块化

**✅ 解决方案**：库头文件仅包含必要的头文件

```cpp
// MyLibrary.h
#pragma once

#include <mine/ui/layout/Panel.h>  // ✅ 仅包含必要的头文件

namespace mylibrary {

class MyCustomPanel : public mine::ui::Panel {
    // ...
};

} // namespace mylibrary
```

---

#### ✅ 推荐：实现文件使用 LayoutAll.h

```cpp
// MyLibrary.cpp
#include "MyLibrary.h"
#include <mine/ui/layout/LayoutAll.h>  // ✅ 实现文件可以使用伞形头文件

namespace mylibrary {

void MyCustomPanel::do_something() {
    StackPanel* stack = new StackPanel();
    Grid* grid = new Grid();
    // ...
}

} // namespace mylibrary
```

**理由**：
- 实现文件的编译时间不影响其他文件
- 简化头文件包含

---

## 模块依赖关系

### 依赖图

```
mine.ui.layout
    ├─ mine.ui.visual (FrameworkElement, UIElement, Visual)
    ├─ mine.ui.property (DependencyProperty)
    ├─ mine.math (Size, Rect, Point)
    ├─ mine.containers (SmallVector)
    └─ mine.core (Pimpl)
```

---

### 依赖说明

| 模块 | 用途 |
|------|------|
| `mine.ui.visual` | 提供 Visual、UIElement、FrameworkElement 基类 |
| `mine.ui.property` | 提供依赖属性系统（DependencyProperty） |
| `mine.math` | 提供数学类型（Size、Rect、Point） |
| `mine.containers` | 提供容器类型（SmallVector） |
| `mine.core` | 提供核心工具（Pimpl） |

---

## 编译配置

### xmake.lua 示例

```lua
-- mine.ui.layout 模块
target("mine.ui.layout")
    set_kind("$(kind)")  -- shared / static
    add_files("src/mine/ui/layout/src/*.cpp")
    add_headerfiles("src/mine/ui/layout/api/include/(**.h)")
    add_includedirs("src/mine/ui/layout/api/include", {public = true})
    
    -- 定义编译宏
    add_defines("MINE_BUILDING_MINE_UI_LAYOUT", {public = false})
    if is_kind("shared") then
        add_defines("MINE_BUILD_SHARED", {public = true})
    end
    
    -- 依赖
    add_deps("mine.ui.visual")
    add_deps("mine.ui.property")
    add_deps("mine.math")
    add_deps("mine.containers")
    add_deps("mine.core")
```

---

### CMakeLists.txt 示例

```cmake
# mine.ui.layout 模块
add_library(mine.ui.layout)

target_sources(mine.ui.layout PRIVATE
    src/mine/ui/layout/src/Panel.cpp
    src/mine/ui/layout/src/StackPanel.cpp
    src/mine/ui/layout/src/Grid.cpp
)

target_include_directories(mine.ui.layout PUBLIC
    src/mine/ui/layout/api/include
)

target_compile_definitions(mine.ui.layout PRIVATE
    MINE_BUILDING_MINE_UI_LAYOUT
)

if(BUILD_SHARED_LIBS)
    target_compile_definitions(mine.ui.layout PUBLIC
        MINE_BUILD_SHARED
    )
endif()

target_link_libraries(mine.ui.layout PUBLIC
    mine.ui.visual
    mine.ui.property
    mine.math
    mine.containers
    mine.core
)
```

---

## 完整示例

以下示例演示如何使用 mine.ui.layout 模块的所有类型。

```cpp
#include <mine/ui/layout/LayoutAll.h>
#include <memory>
#include <iostream>

using namespace mine::ui;
using namespace mine::math;

/**
 * @brief 演示所有布局类型
 */
class LayoutDemo {
public:
    void demo_orientation() {
        std::cout << "=== Orientation Demo ===\n";
        
        Orientation horizontal = Orientation::Horizontal;
        Orientation vertical = Orientation::Vertical;
        
        std::cout << "Horizontal: " << static_cast<int>(horizontal) << "\n";
        std::cout << "Vertical: " << static_cast<int>(vertical) << "\n";
    }

    void demo_grid_length() {
        std::cout << "\n=== GridLength Demo ===\n";
        
        GridLength pixel = GridLength::pixel(100);
        GridLength auto_len = GridLength::auto_();
        GridLength star = GridLength::star(2.0f);
        
        std::cout << "Pixel: value=" << pixel.value 
                  << ", type=" << static_cast<int>(pixel.type) << "\n";
        std::cout << "Auto: type=" << static_cast<int>(auto_len.type) << "\n";
        std::cout << "Star: value=" << star.value 
                  << ", type=" << static_cast<int>(star.type) << "\n";
    }

    void demo_panel() {
        std::cout << "\n=== Panel Demo ===\n";
        
        auto panel = std::make_unique<Panel>();
        auto child1 = std::make_unique<UIElement>();
        auto child2 = std::make_unique<UIElement>();
        
        panel->add_child(child1.get());
        panel->add_child(child2.get());
        
        std::cout << "Panel children count: " << panel->children_count() << "\n";
        
        panel->remove_child(child1.get());
        std::cout << "After remove: " << panel->children_count() << "\n";
        
        children_.push_back(std::move(child1));
        children_.push_back(std::move(child2));
    }

    void demo_stack_panel() {
        std::cout << "\n=== StackPanel Demo ===\n";
        
        auto stack = std::make_unique<StackPanel>();
        stack->set_orientation(Orientation::Vertical);
        
        std::cout << "StackPanel orientation: " 
                  << static_cast<int>(stack->orientation()) << "\n";
        
        // 测量
        Size available{400, 600};
        stack->measure(available);
        
        Size desired = stack->desired_size();
        std::cout << "Desired size: " << desired.width 
                  << " × " << desired.height << "\n";
    }

    void demo_grid() {
        std::cout << "\n=== Grid Demo ===\n";
        
        auto grid = std::make_unique<Grid>();
        
        grid->add_row({GridLength::pixel(100)});
        grid->add_row({GridLength::auto_()});
        grid->add_row({GridLength::star()});
        
        grid->add_column({GridLength::pixel(200)});
        grid->add_column({GridLength::star(2)});
        
        std::cout << "Grid rows: " << grid->row_count() << "\n";
        std::cout << "Grid columns: " << grid->column_count() << "\n";
        
        auto child = std::make_unique<UIElement>();
        Grid::set_row(*child, 1);
        Grid::set_column(*child, 0);
        Grid::set_row_span(*child, 2);
        Grid::set_column_span(*child, 2);
        
        std::cout << "Child row: " << Grid::get_row(*child) << "\n";
        std::cout << "Child column: " << Grid::get_column(*child) << "\n";
        std::cout << "Child row span: " << Grid::get_row_span(*child) << "\n";
        std::cout << "Child column span: " << Grid::get_column_span(*child) << "\n";
        
        grid->add_child(child.get());
        children_.push_back(std::move(child));
    }

    void demo_module_tag() {
        std::cout << "\n=== Module Tag Demo ===\n";
        std::cout << "Module name: " << kLayoutModuleName << "\n";
    }

    void run_all_demos() {
        demo_orientation();
        demo_grid_length();
        demo_panel();
        demo_stack_panel();
        demo_grid();
        demo_module_tag();
    }

private:
    std::vector<std::unique_ptr<UIElement>> children_;
};

int main() {
    std::cout << "mine.ui.layout Module Demo\n";
    std::cout << "===========================\n\n";
    
    LayoutDemo demo;
    demo.run_all_demos();
    
    std::cout << "\n===========================\n";
    std::cout << "All demos completed!\n";
    
    return 0;
}

/*
 * 预期输出：
 *
 * mine.ui.layout Module Demo
 * ===========================
 *
 * === Orientation Demo ===
 * Horizontal: 0
 * Vertical: 1
 *
 * === GridLength Demo ===
 * Pixel: value=100, type=0
 * Auto: type=1
 * Star: value=2, type=2
 *
 * === Panel Demo ===
 * Panel children count: 2
 * After remove: 1
 *
 * === StackPanel Demo ===
 * StackPanel orientation: 1
 * Desired size: 0 × 0
 *
 * === Grid Demo ===
 * Grid rows: 3
 * Grid columns: 2
 * Child row: 1
 * Child column: 0
 * Child row span: 2
 * Child column span: 2
 *
 * === Module Tag Demo ===
 * Module name: mine.ui.layout
 *
 * ===========================
 * All demos completed!
 */
```

---

## 总结

**核心要点**：
1. **Api.h**：定义 MINE_UI_LAYOUT_API 宏，支持跨平台 DLL 导出/导入。
2. **ModuleTag.h**：定义 kLayoutModuleName 常量，用于日志和诊断信息标识。
3. **LayoutAll.h**：伞形头文件，包含所有布局类型。
4. **公开类使用 MINE_UI_LAYOUT_API**：确保跨模块访问。
5. **内部类不使用导出宏**：避免暴露内部实现。
6. **应用代码使用 LayoutAll.h**：简化头文件包含。
7. **库头文件不使用 LayoutAll.h**：避免增加编译时间和依赖。

**使用建议**：
- ✅ 所有公开类使用 MINE_UI_LAYOUT_API
- ✅ 使用 kLayoutModuleName 标识模块
- ✅ 应用代码使用 LayoutAll.h
- ✅ 库实现文件使用 LayoutAll.h
- ❌ 不要在内部类使用导出宏
- ❌ 不要在库头文件使用 LayoutAll.h
- ❌ 不要硬编码模块名称

**模块依赖**：
- mine.ui.visual（FrameworkElement、UIElement、Visual）
- mine.ui.property（DependencyProperty）
- mine.math（Size、Rect、Point）
- mine.containers（SmallVector）
- mine.core（Pimpl）
