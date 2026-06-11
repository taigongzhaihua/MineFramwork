# ModuleMetadata - 模块元数据

## 概述

`mine.ui.animation` 模块的元数据文件包含模块级别的配置和导出信息，主要包括：

1. **Api.h**：定义 `MINE_UI_ANIMATION_API` 宏，用于 DLL 导出/导入
2. **AnimationAll.h**：伞形头文件，一次性包含所有公开接口

这些元数据文件是模块化架构的重要组成部分，确保正确的符号可见性和便捷的头文件引用。

### 核心特性

- **DLL 导出宏**：`MINE_UI_ANIMATION_API` 支持跨平台 DLL 导出/导入
- **伞形头文件**：`AnimationAll.h` 简化头文件引用
- **编译配置**：通过 `MINE_UI_ANIMATION_EXPORTS` 宏控制导出/导入行为
- **跨平台支持**：兼容 Windows (MSVC/MinGW)、Linux、macOS
- **模块化设计**：独立模块，可单独编译和链接

### 文件结构

```
src/mine/ui/animation/
├── Api.h                   # DLL 导出宏定义
├── AnimationAll.h          # 伞形头文件（包含所有公开接口）
├── Duration.h              # 时长类型
├── EasingFunction.h        # 缓动函数库
├── SpringEasing.h          # 弹簧缓动
├── AnimationClock.h        # 全局动画时钟
├── PropertyAnimation.h     # 属性动画描述
└── Storyboard.h            # 动画时间线
```

---

## 文件位置

```
src/mine/ui/animation/Api.h
src/mine/ui/animation/AnimationAll.h
```

---

## Api.h 详解

### 文件内容

```cpp
#pragma once

/// @file Api.h
/// @brief mine.ui.animation 模块的 API 导出宏定义

// Windows DLL 导出/导入
#if defined(_WIN32) || defined(_WIN64)
    #ifdef MINE_UI_ANIMATION_EXPORTS
        // 编译 DLL 时：导出符号
        #define MINE_UI_ANIMATION_API __declspec(dllexport)
    #else
        // 使用 DLL 时：导入符号
        #define MINE_UI_ANIMATION_API __declspec(dllimport)
    #endif
#else
    // Linux/macOS：使用 visibility 属性
    #ifdef MINE_UI_ANIMATION_EXPORTS
        #define MINE_UI_ANIMATION_API __attribute__((visibility("default")))
    #else
        #define MINE_UI_ANIMATION_API
    #endif
#endif
```

### 宏定义说明

#### MINE_UI_ANIMATION_API

用于标记需要导出的类、函数、变量。

- **Windows（MSVC/MinGW）**：
  - 编译 DLL 时（定义了 `MINE_UI_ANIMATION_EXPORTS`）：展开为 `__declspec(dllexport)`
  - 使用 DLL 时（未定义 `MINE_UI_ANIMATION_EXPORTS`）：展开为 `__declspec(dllimport)`
- **Linux/macOS（GCC/Clang）**：
  - 编译时：展开为 `__attribute__((visibility("default")))`
  - 使用时：展开为空（默认可见）

**示例**：

```cpp
// 导出类
class MINE_UI_ANIMATION_API AnimationClock {
public:
    static AnimationClock& instance() noexcept;
    // ...
};

// 导出函数
MINE_UI_ANIMATION_API void init_animation_system();

// 导出全局变量
MINE_UI_ANIMATION_API extern EasingFn Linear;
```

### 编译配置

#### CMake 配置

```cmake
# CMakeLists.txt
add_library(mine.ui.animation SHARED
    src/Duration.cpp
    src/EasingFunction.cpp
    src/SpringEasing.cpp
    src/AnimationClock.cpp
    src/Storyboard.cpp
)

# 定义导出宏（仅在编译 DLL 时）
target_compile_definitions(mine.ui.animation PRIVATE
    MINE_UI_ANIMATION_EXPORTS
)

# 设置符号可见性（Linux/macOS）
set_target_properties(mine.ui.animation PROPERTIES
    CXX_VISIBILITY_PRESET hidden
    VISIBILITY_INLINES_HIDDEN ON
)
```

#### xmake 配置

```lua
-- xmake.lua
target("mine.ui.animation")
    set_kind("shared")  -- 动态库
    add_files("src/*.cpp")
    add_defines("MINE_UI_ANIMATION_EXPORTS")  -- 导出宏
    set_symbols("hidden")  -- 隐藏未标记的符号
```

---

## AnimationAll.h 详解

### 文件内容

```cpp
#pragma once

/// @file AnimationAll.h
/// @brief mine.ui.animation 模块的伞形头文件
/// @details 一次性包含所有公开接口，简化头文件引用。
///          应用程序可以直接包含此文件，无需逐个包含各个头文件。

// API 导出宏
#include "Api.h"

// 核心类型
#include "Duration.h"
#include "EasingFunction.h"
#include "SpringEasing.h"

// 动画系统
#include "AnimationClock.h"
#include "PropertyAnimation.h"
#include "Storyboard.h"

/// @namespace mine::ui::animation
/// @brief MineUI 动画系统命名空间
namespace mine::ui::animation {

// 命名空间别名（可选）
// using namespace mine::ui::animation;

} // namespace mine::ui::animation
```

### 使用方式

#### 方式 1：包含伞形头文件（推荐）

```cpp
// 应用程序代码
#include <mine/ui/animation/AnimationAll.h>

using namespace mine::ui::animation;

void setup_animation() {
    Duration d = Duration::milliseconds(300);
    EasingFn easing = CubicEaseOut;
    AnimationClock& clock = AnimationClock::instance();
    // ...
}
```

#### 方式 2：按需包含单个头文件

```cpp
// 仅使用 Duration 和 EasingFunction
#include <mine/ui/animation/Duration.h>
#include <mine/ui/animation/EasingFunction.h>

using namespace mine::ui::animation;

void simple_animation() {
    Duration d = Duration::seconds(1.0f);
    float t = CubicEaseOut(0.5f);
}
```

---

## 使用场景

### 1. 应用程序引用

应用程序通过包含 `AnimationAll.h` 使用动画模块。

```cpp
// main.cpp
#include <mine/ui/animation/AnimationAll.h>

using namespace mine::ui::animation;

int main() {
    // 初始化动画系统
    AnimationClock& clock = AnimationClock::instance();

    // 创建动画
    Storyboard storyboard;
    storyboard.animate_dp(
        button, Visual::OpacityProperty,
        Duration::milliseconds(300), CubicEaseOut
    );

    // 推进动画
    while (!storyboard.is_complete()) {
        clock.tick_all(0.016f);
        storyboard.tick(0.016f);
    }

    return 0;
}
```

---

### 2. 导出新 API

在模块中新增公开 API 时，需要标记 `MINE_UI_ANIMATION_API`。

```cpp
// NewFeature.h
#pragma once
#include "Api.h"

namespace mine::ui::animation {

/// @brief 新功能类
class MINE_UI_ANIMATION_API NewFeature {
public:
    void do_something();
};

/// @brief 新功能函数
MINE_UI_ANIMATION_API void new_feature_function();

} // namespace mine::ui::animation
```

同时更新 `AnimationAll.h`：

```cpp
// AnimationAll.h
#pragma once

#include "Api.h"
#include "Duration.h"
// ...其他头文件...
#include "NewFeature.h"  // ✅ 添加新头文件
```

---

### 3. 模块初始化

模块级别的初始化函数。

```cpp
// Init.h
#pragma once
#include "Api.h"

namespace mine::ui::animation {

/// @brief 初始化动画模块
/// @note 应用程序启动时调用一次
MINE_UI_ANIMATION_API void init();

/// @brief 清理动画模块
/// @note 应用程序退出前调用
MINE_UI_ANIMATION_API void shutdown();

} // namespace mine::ui::animation
```

```cpp
// Init.cpp
#include "Init.h"
#include "AnimationClock.h"

namespace mine::ui::animation {

void init() {
    // 初始化全局时钟
    AnimationClock::instance();
    std::cout << "Animation module initialized\n";
}

void shutdown() {
    // 清理资源
    std::cout << "Animation module shutdown\n";
}

} // namespace mine::ui::animation
```

应用程序使用：

```cpp
// main.cpp
#include <mine/ui/animation/AnimationAll.h>

int main() {
    mine::ui::animation::init();

    // ...应用程序逻辑...

    mine::ui::animation::shutdown();
    return 0;
}
```

---

### 4. 单元测试

单元测试项目引用模块。

```cpp
// test_duration.cpp
#include <gtest/gtest.h>
#include <mine/ui/animation/Duration.h>

using namespace mine::ui::animation;

TEST(DurationTest, Milliseconds) {
    Duration d = Duration::milliseconds(500);
    EXPECT_FLOAT_EQ(d.to_seconds(), 0.5f);
    EXPECT_FLOAT_EQ(d.to_milliseconds(), 500.0f);
}

TEST(DurationTest, Seconds) {
    Duration d = Duration::seconds(1.5f);
    EXPECT_FLOAT_EQ(d.to_seconds(), 1.5f);
    EXPECT_FLOAT_EQ(d.to_milliseconds(), 1500.0f);
}
```

CMake 配置：

```cmake
# tests/CMakeLists.txt
add_executable(animation_tests
    test_duration.cpp
    test_easing.cpp
)

# 链接 mine.ui.animation 动态库
target_link_libraries(animation_tests
    mine.ui.animation
    gtest_main
)
```

---

### 5. 预编译头

将 `AnimationAll.h` 加入预编译头以加速编译。

```cpp
// pch.h（预编译头）
#pragma once

// 标准库
#include <vector>
#include <string>
#include <memory>

// MineUI 核心
#include <mine/core/CoreAll.h>
#include <mine/ui/animation/AnimationAll.h>  // ✅ 包含动画模块

// 其他常用头文件
#include <mine/ui/visual/VisualAll.h>
```

CMake 配置：

```cmake
target_precompile_headers(myapp PRIVATE pch.h)
```

---

### 6. 第三方集成

第三方项目通过 SDK 使用动画模块。

```cpp
// third_party_app.cpp
// 假设 MineUI SDK 已安装到系统路径

#include <mine/ui/animation/AnimationAll.h>

using namespace mine::ui::animation;

void create_fade_in_effect() {
    Storyboard storyboard;
    storyboard.animate_dp_from_to(
        widget, OpacityProperty,
        Variant::from<float>(0.0f),
        Variant::from<float>(1.0f),
        Duration::milliseconds(500), CubicEaseOut
    );

    storyboard.capture_from_values();
    storyboard.resolve_and_begin();

    while (!storyboard.is_complete()) {
        storyboard.tick(0.016f);
    }
}
```

CMake 配置（使用已安装的 SDK）：

```cmake
# 查找 MineUI 包
find_package(MineFramework REQUIRED COMPONENTS ui.animation)

# 链接动画模块
target_link_libraries(third_party_app
    MineFramework::ui.animation
)
```

---

### 7. 编译配置检查

检查 `MINE_UI_ANIMATION_EXPORTS` 宏是否正确配置。

```cpp
// CheckExports.cpp（仅用于调试）
#include "Api.h"
#include <iostream>

#ifdef MINE_UI_ANIMATION_EXPORTS
    #define EXPORT_MODE "EXPORT (dllexport)"
#else
    #define EXPORT_MODE "IMPORT (dllimport)"
#endif

void check_export_mode() {
    std::cout << "MINE_UI_ANIMATION_API mode: " << EXPORT_MODE << "\n";
}
```

编译输出：

```
# 编译 DLL 时
MINE_UI_ANIMATION_API mode: EXPORT (dllexport)

# 使用 DLL 时
MINE_UI_ANIMATION_API mode: IMPORT (dllimport)
```

---

## 最佳实践

### ✅ 推荐做法

#### 1. 应用程序使用伞形头文件

```cpp
// ✅ 使用 AnimationAll.h 简化引用
#include <mine/ui/animation/AnimationAll.h>

using namespace mine::ui::animation;

void use_animation() {
    Duration d = Duration::milliseconds(300);
    EasingFn easing = CubicEaseOut;
    // ...
}
```

#### 2. 新增 API 时正确标记导出宏

```cpp
// ✅ 公开类使用 MINE_UI_ANIMATION_API
class MINE_UI_ANIMATION_API MyAnimationHelper {
public:
    void do_something();
};

// ✅ 公开函数使用 MINE_UI_ANIMATION_API
MINE_UI_ANIMATION_API void helper_function();
```

#### 3. 编译 DLL 时定义导出宏

```cmake
# ✅ CMake 正确配置
target_compile_definitions(mine.ui.animation PRIVATE
    MINE_UI_ANIMATION_EXPORTS
)
```

#### 4. 使用命名空间别名简化代码

```cpp
// ✅ 使用命名空间别名
namespace anim = mine::ui::animation;

void use_animation() {
    anim::Duration d = anim::Duration::milliseconds(300);
    anim::EasingFn easing = anim::CubicEaseOut;
}
```

---

### ❌ 避免的做法

#### 1. 忘记标记导出宏

```cpp
// ❌ 公开类忘记 MINE_UI_ANIMATION_API
class NewFeature {  // 符号不会导出，链接时失败
public:
    void do_something();
};

// ✅ 正确做法
class MINE_UI_ANIMATION_API NewFeature {
public:
    void do_something();
};
```

#### 2. 错误的宏定义时机

```cmake
# ❌ 错误：在应用程序中定义导出宏
target_compile_definitions(myapp PRIVATE
    MINE_UI_ANIMATION_EXPORTS  # 错误！应用程序不应定义此宏
)

# ✅ 正确：仅在编译 DLL 时定义
target_compile_definitions(mine.ui.animation PRIVATE
    MINE_UI_ANIMATION_EXPORTS
)
```

#### 3. 忘记更新 AnimationAll.h

```cpp
// NewFeature.h（新增文件）
class MINE_UI_ANIMATION_API NewFeature { /*...*/ };

// ❌ 忘记在 AnimationAll.h 中包含
// AnimationAll.h
#pragma once
#include "Api.h"
#include "Duration.h"
// ...
// 缺少 #include "NewFeature.h"

// ✅ 正确做法
// AnimationAll.h
#pragma once
#include "Api.h"
#include "Duration.h"
// ...
#include "NewFeature.h"  // ✅ 添加新头文件
```

#### 4. 在头文件中使用 using namespace

```cpp
// ❌ 在头文件中使用 using namespace
// AnimationAll.h
#pragma once
#include "Duration.h"
using namespace mine::ui::animation;  // 污染全局命名空间

// ✅ 正确做法：仅在 .cpp 文件中使用
// myapp.cpp
#include <mine/ui/animation/AnimationAll.h>
using namespace mine::ui::animation;
```

---

## 常见陷阱

### ❌ 陷阱 1：链接错误（符号未导出）

```
error LNK2019: 无法解析的外部符号 "public: static class AnimationClock & __cdecl AnimationClock::instance(void)"
```

**原因**：

- 忘记在类/函数前添加 `MINE_UI_ANIMATION_API`
- 编译 DLL 时未定义 `MINE_UI_ANIMATION_EXPORTS`

**✅ 解决方案**：

```cpp
// 1. 确保类定义有导出宏
class MINE_UI_ANIMATION_API AnimationClock { /*...*/ };

// 2. 确保 CMakeLists.txt 正确配置
target_compile_definitions(mine.ui.animation PRIVATE
    MINE_UI_ANIMATION_EXPORTS
)
```

---

### ❌ 陷阱 2：重复定义符号

```
error LNK1169: 找到一个或多个多重定义的符号
```

**原因**：

- 应用程序编译时错误定义了 `MINE_UI_ANIMATION_EXPORTS`
- 导致符号既导出又导入

**✅ 解决方案**：

```cmake
# ❌ 错误：应用程序不应定义导出宏
target_compile_definitions(myapp PRIVATE
    MINE_UI_ANIMATION_EXPORTS  # 删除此行
)

# ✅ 正确：仅 DLL 定义导出宏
target_compile_definitions(mine.ui.animation PRIVATE
    MINE_UI_ANIMATION_EXPORTS
)
```

---

### ❌ 陷阱 3：头文件循环依赖

```cpp
// AnimationAll.h 包含 Storyboard.h
#include "Storyboard.h"

// Storyboard.h 包含 AnimationAll.h
#include "AnimationAll.h"  // ❌ 循环依赖
```

**✅ 解决方案**：

```cpp
// Storyboard.h 仅包含必要的头文件
#pragma once
#include "Api.h"
#include "Duration.h"
#include "EasingFunction.h"
#include "PropertyAnimation.h"
// 不要包含 AnimationAll.h

// AnimationAll.h 包含所有公开头文件
#pragma once
#include "Api.h"
#include "Duration.h"
// ...
#include "Storyboard.h"
```

---

### ❌ 陷阱 4：Linux 上符号可见性错误

```
error: undefined reference to `AnimationClock::instance()'
```

**原因**：

- Linux/macOS 上未设置符号可见性
- 所有符号默认可见，导致冲突

**✅ 解决方案**：

```cmake
# CMakeLists.txt
set_target_properties(mine.ui.animation PROPERTIES
    CXX_VISIBILITY_PRESET hidden  # ✅ 隐藏未标记的符号
    VISIBILITY_INLINES_HIDDEN ON
)
```

---

## 完整示例

### 示例 1：模块导出配置

```cmake
# CMakeLists.txt
cmake_minimum_required(VERSION 3.15)
project(mine.ui.animation)

# 创建动态库
add_library(mine.ui.animation SHARED
    src/Duration.cpp
    src/EasingFunction.cpp
    src/SpringEasing.cpp
    src/AnimationClock.cpp
    src/Storyboard.cpp
)

# 定义导出宏（仅编译 DLL 时）
target_compile_definitions(mine.ui.animation PRIVATE
    MINE_UI_ANIMATION_EXPORTS
)

# 设置符号可见性（Linux/macOS）
set_target_properties(mine.ui.animation PROPERTIES
    CXX_VISIBILITY_PRESET hidden
    VISIBILITY_INLINES_HIDDEN ON
)

# 包含目录
target_include_directories(mine.ui.animation PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)

# 安装规则
install(TARGETS mine.ui.animation
    EXPORT mine.ui.animation-targets
    LIBRARY DESTINATION lib
    ARCHIVE DESTINATION lib
    RUNTIME DESTINATION bin
)

install(DIRECTORY include/mine/ui/animation
    DESTINATION include/mine/ui
)

install(EXPORT mine.ui.animation-targets
    FILE mine.ui.animation-targets.cmake
    NAMESPACE MineFramework::
    DESTINATION lib/cmake/MineFramework
)
```

---

### 示例 2：应用程序使用

```cpp
// main.cpp
#include <iostream>
#include <mine/ui/animation/AnimationAll.h>

using namespace mine::ui::animation;

class SimpleButton {
public:
    void animate_press() {
        Storyboard storyboard;

        // 按下效果：缩放到 0.95
        storyboard.animate_dp_from_to(
            this, ScaleProperty,
            Variant::from<float>(1.0f),
            Variant::from<float>(0.95f),
            Duration::milliseconds(100), CubicEaseOut
        );

        storyboard.capture_from_values();
        storyboard.resolve_and_begin();

        std::cout << "开始按下动画\n";

        while (!storyboard.is_complete()) {
            storyboard.tick(0.016f);
        }

        std::cout << "按下动画完成\n";
        storyboard.stop();
    }

private:
    static const DependencyProperty* ScaleProperty;
};

int main() {
    std::cout << "MineUI Animation Demo\n";

    SimpleButton button;
    button.animate_press();

    return 0;
}
```

CMake 配置（应用程序）：

```cmake
# CMakeLists.txt (app)
cmake_minimum_required(VERSION 3.15)
project(AnimationDemo)

add_executable(AnimationDemo main.cpp)

# 查找 MineFramework 包
find_package(MineFramework REQUIRED COMPONENTS ui.animation)

# 链接动画模块（不定义 MINE_UI_ANIMATION_EXPORTS）
target_link_libraries(AnimationDemo
    MineFramework::ui.animation
)
```

---

### 示例 3：CMake 包配置文件

```cmake
# MineFrameworkConfig.cmake
@PACKAGE_INIT@

include(CMakeFindDependencyMacro)

# 查找依赖
find_dependency(mine.core)

# 包含目标
if(NOT TARGET MineFramework::ui.animation)
    include("${CMAKE_CURRENT_LIST_DIR}/mine.ui.animation-targets.cmake")
endif()

# 设置变量
set(MineFramework_ui.animation_FOUND TRUE)

check_required_components(MineFramework)
```

---

## 总结

`mine.ui.animation` 模块的元数据文件提供了模块级别的配置和导出信息。主要组成部分包括：

1. **Api.h**：定义 `MINE_UI_ANIMATION_API` 宏，支持跨平台 DLL 导出/导入
2. **AnimationAll.h**：伞形头文件，一次性包含所有公开接口

**关键要点**：

- ✅ 应用程序使用 `AnimationAll.h` 简化头文件引用
- ✅ 新增公开 API 时标记 `MINE_UI_ANIMATION_API`
- ✅ 编译 DLL 时定义 `MINE_UI_ANIMATION_EXPORTS`
- ✅ Linux/macOS 设置符号可见性为 `hidden`
- ❌ 不要在应用程序中定义 `MINE_UI_ANIMATION_EXPORTS`
- ❌ 不要忘记更新 `AnimationAll.h` 包含新头文件
- ❌ 不要在头文件中使用 `using namespace`
- ❌ 不要创建头文件循环依赖

模块元数据文件是 MineUI 模块化架构的重要基础设施，确保正确的符号可见性和便捷的头文件管理。与其他模块（`mine.core`、`mine.ui.visual` 等）保持一致的设计模式。
