# mine.ui.controls 模块元数据文档

## 1. 概述

`mine.ui.controls` 模块是 MineFramework 的核心 UI 控件库,提供 MD3(Material Design 3)风格的原生控件、容器控件、导航控件等。本文档介绍模块的元数据、导出宏、模块标签以及如何在项目中正确引用和链接此模块。

**模块职责:**

- **控件库提供**:提供 Button、CheckBox、TextBox、ListView 等常用控件
- **跨平台 API 导出**:通过 `MINE_UI_CONTROLS_API` 宏统一处理 Windows/Linux/macOS 的 DLL 导出/导入
- **模块标签系统**:通过 `ModuleTag` 结构体声明模块元数据(名称、版本、依赖等)
- **统一头文件**:提供 `ControlsAll.h` 一次性引入所有控件头文件

**模块结构:**

```
mine.ui.controls/
    ├── Api.h               # API 导出宏定义
    ├── ModuleTag.h         # 模块标签(元数据)
    ├── ControlsAll.h       # 统一头文件(包含所有控件)
    ├── ContentControl.h/.cpp
    ├── Button.h/.cpp
    ├── CheckBox.h/.cpp
    ├── TextBox.h/.cpp
    ├── Page.h/.cpp
    ├── UserControl.h/.cpp
    └── ... (其他控件)
```

**相关类型:**

- `MINE_UI_CONTROLS_API`:模块 API 导出/导入宏
- `mine::ui::controls::ModuleTag`:模块标签结构体
- `ControlsAll.h`:统一头文件

**使用场景:**

- 在 DLL/Shared Library 模式下正确导出/导入符号
- 查询模块版本和依赖信息
- 快速引入所有控件头文件

---

## 2. 文件位置

**API 导出宏头文件:**
```
src/mine/ui/controls/Api.h
```

**模块标签头文件:**
```
src/mine/ui/controls/ModuleTag.h
```

**统一头文件:**
```
src/mine/ui/controls/ControlsAll.h
```

**模块归属:**
```
mine.ui.controls
```

**命名空间:**
```cpp
namespace mine::ui
namespace mine::ui::controls  // 模块标签命名空间
```

**链接依赖:**
```
mine.ui.controls.lib (Windows)
libmine.ui.controls.so (Linux)
libmine.ui.controls.dylib (macOS)
```

---

## 3. API 导出宏

### 3.1 MINE_UI_CONTROLS_API 宏定义

**Api.h 完整内容:**

```cpp
#pragma once

// ========== 跨平台 API 导出/导入宏 ==========

#if defined(_WIN32) || defined(_WIN64)
    // Windows 平台
    #ifdef MINE_UI_CONTROLS_EXPORTS
        // 构建 DLL 时导出符号
        #define MINE_UI_CONTROLS_API __declspec(dllexport)
    #else
        // 使用 DLL 时导入符号
        #define MINE_UI_CONTROLS_API __declspec(dllimport)
    #endif
#elif defined(__GNUC__) && __GNUC__ >= 4
    // Linux/macOS 平台(GCC/Clang)
    #ifdef MINE_UI_CONTROLS_EXPORTS
        // 构建共享库时导出符号
        #define MINE_UI_CONTROLS_API __attribute__((visibility("default")))
    #else
        // 使用共享库时无需特殊标记
        #define MINE_UI_CONTROLS_API
    #endif
#else
    // 其他平台或静态库
    #define MINE_UI_CONTROLS_API
#endif
```

**宏说明:**

| 宏名称 | 平台 | 构建模式 | 展开结果 |
|--------|------|----------|----------|
| MINE_UI_CONTROLS_API | Windows | DLL 导出(MINE_UI_CONTROLS_EXPORTS 已定义) | `__declspec(dllexport)` |
| MINE_UI_CONTROLS_API | Windows | DLL 导入(MINE_UI_CONTROLS_EXPORTS 未定义) | `__declspec(dllimport)` |
| MINE_UI_CONTROLS_API | Linux/macOS | 共享库导出(MINE_UI_CONTROLS_EXPORTS 已定义) | `__attribute__((visibility("default")))` |
| MINE_UI_CONTROLS_API | Linux/macOS | 共享库导入(MINE_UI_CONTROLS_EXPORTS 未定义) | *(空宏,无额外标记)* |
| MINE_UI_CONTROLS_API | 任意平台 | 静态库 | *(空宏,无额外标记)* |

**使用方式:**

```cpp
// 在类声明前添加
class MINE_UI_CONTROLS_API Button : public ContentControl {
    // ...
};

// 在全局函数声明前添加
MINE_UI_CONTROLS_API void register_controls();
```

---

### 3.2 构建配置

**Windows (Visual Studio):**

在构建 DLL 时,通过预处理器定义 `MINE_UI_CONTROLS_EXPORTS`:

```xml
<!-- mine.ui.controls.vcxproj -->
<PropertyGroup>
    <ConfigurationType>DynamicLibrary</ConfigurationType>
    <PreprocessorDefinitions>MINE_UI_CONTROLS_EXPORTS;%(PreprocessorDefinitions)</PreprocessorDefinitions>
</PropertyGroup>
```

**Linux/macOS (CMake):**

```cmake
# CMakeLists.txt
add_library(mine.ui.controls SHARED
    src/ContentControl.cpp
    src/Button.cpp
    src/CheckBox.cpp
    # ...
)

# 构建时定义导出宏
target_compile_definitions(mine.ui.controls PRIVATE MINE_UI_CONTROLS_EXPORTS)

# 设置符号可见性
set_target_properties(mine.ui.controls PROPERTIES
    CXX_VISIBILITY_PRESET hidden
    VISIBILITY_INLINES_HIDDEN YES
)
```

**xmake(MineFramework 官方构建工具):**

```lua
-- xmake.lua
target("mine.ui.controls")
    set_kind("shared")  -- 共享库
    add_defines("MINE_UI_CONTROLS_EXPORTS")
    
    -- 设置符号可见性(Linux/macOS)
    add_cxflags("-fvisibility=hidden", {force = true})
    
    add_files("src/*.cpp")
    add_headerfiles("src/*.h")
```

---

## 4. 模块标签

### 4.1 ModuleTag 结构体定义

**ModuleTag.h 完整内容:**

```cpp
#pragma once
#include <mine/core/StringView.h>
#include <mine/core/Version.h>

namespace mine::ui::controls {

/// @brief mine.ui.controls 模块标签(元数据)
///
/// 声明模块的名称、版本、依赖等元数据信息。
/// 由 MineFramework 模块系统在运行时读取,用于依赖检查、版本验证等。
struct ModuleTag {
    /// @brief 模块名称
    static constexpr core::StringView name = "mine.ui.controls";
    
    /// @brief 模块版本
    /// @note 遵循语义化版本(SemVer): MAJOR.MINOR.PATCH
    static constexpr core::Version version{0, 1, 0};
    
    /// @brief 模块描述
    static constexpr core::StringView description = 
        "MineFramework UI 控件库,提供 MD3 风格的原生控件";
    
    /// @brief 依赖模块列表
    /// @note 格式: 模块名@最小版本
    static constexpr core::StringView dependencies[] = {
        "mine.core@0.1.0",
        "mine.ui@0.1.0",
        "mine.ui.layout@0.1.0",
        "mine.ui.visual@0.1.0",
        "mine.ui.property@0.1.0",
        "mine.ui.event@0.1.0",
        "mine.ui.animation@0.1.0",
        "mine.paint@0.1.0"
    };
    
    /// @brief 依赖模块数量
    static constexpr size_t dependency_count = 
        sizeof(dependencies) / sizeof(dependencies[0]);
    
    /// @brief 模块作者
    static constexpr core::StringView author = "MineFramework Team";
    
    /// @brief 许可证
    static constexpr core::StringView license = "MIT";
    
    /// @brief 仓库地址
    static constexpr core::StringView repository = 
        "https://github.com/mineframework/mine";
};

} // namespace mine::ui::controls
```

**字段说明:**

| 字段 | 类型 | 说明 |
|------|------|------|
| `name` | `StringView` | 模块名称,遵循 `mine.<category>.<module>` 命名规则 |
| `version` | `Version` | 模块版本,遵循语义化版本(SemVer) |
| `description` | `StringView` | 模块简介 |
| `dependencies` | `StringView[]` | 依赖模块列表,格式为 `模块名@最小版本` |
| `dependency_count` | `size_t` | 依赖模块数量 |
| `author` | `StringView` | 模块作者 |
| `license` | `StringView` | 许可证类型 |
| `repository` | `StringView` | 源代码仓库地址 |

---

### 4.2 模块系统集成

**模块注册(自动):**

MineFramework 模块系统在启动时自动扫描并注册所有 `ModuleTag`:

```cpp
// 由 mine.core 模块系统自动执行
namespace mine::core {
    void module_system_init() {
        // 注册所有模块
        register_module(ui::controls::ModuleTag::name, 
                        ui::controls::ModuleTag::version,
                        ui::controls::ModuleTag::dependencies,
                        ui::controls::ModuleTag::dependency_count);
        // ...
    }
}
```

**依赖检查:**

应用启动时,模块系统自动检查依赖版本是否满足要求:

```cpp
// 伪代码
bool check_dependencies() {
    for (auto& dep : ModuleTag::dependencies) {
        auto [module_name, min_version] = parse_dependency(dep);
        auto loaded_version = get_loaded_module_version(module_name);
        
        if (loaded_version < min_version) {
            throw ModuleDependencyError(
                std::format("模块 {} 需要 {} 版本 >= {},当前版本 {}",
                            ModuleTag::name, module_name, 
                            min_version, loaded_version)
            );
        }
    }
    return true;
}
```

---

## 5. 统一头文件

### 5.1 ControlsAll.h 内容

**ControlsAll.h 完整内容:**

```cpp
#pragma once

/// @file ControlsAll.h
/// @brief mine.ui.controls 模块统一头文件
///
/// 一次性引入所有控件头文件,便于快速开发。
/// 注意:引入此文件会增加编译时间,生产环境建议按需引入。

// ========== API 导出宏 ==========
#include <mine/ui/controls/Api.h>

// ========== 模块标签 ==========
#include <mine/ui/controls/ModuleTag.h>

// ========== 基础控件类 ==========
#include <mine/ui/controls/ContentControl.h>
#include <mine/ui/controls/UserControl.h>
#include <mine/ui/controls/Page.h>

// ========== 按钮类控件 ==========
#include <mine/ui/controls/Button.h>
#include <mine/ui/controls/RepeatButton.h>
#include <mine/ui/controls/ToggleButton.h>
#include <mine/ui/controls/CheckBox.h>
#include <mine/ui/controls/RadioButton.h>

// ========== 文本类控件 ==========
#include <mine/ui/controls/TextBox.h>
#include <mine/ui/controls/PasswordBox.h>
#include <mine/ui/controls/TextBlock.h>
#include <mine/ui/controls/Label.h>

// ========== 列表类控件 ==========
#include <mine/ui/controls/ListView.h>
#include <mine/ui/controls/ListBox.h>
#include <mine/ui/controls/ComboBox.h>
#include <mine/ui/controls/TreeView.h>

// ========== 容器类控件 ==========
#include <mine/ui/controls/ScrollViewer.h>
#include <mine/ui/controls/TabControl.h>
#include <mine/ui/controls/Expander.h>
#include <mine/ui/controls/GroupBox.h>

// ========== 范围类控件 ==========
#include <mine/ui/controls/Slider.h>
#include <mine/ui/controls/ProgressBar.h>
#include <mine/ui/controls/ScrollBar.h>

// ========== 菜单类控件 ==========
#include <mine/ui/controls/Menu.h>
#include <mine/ui/controls/MenuItem.h>
#include <mine/ui/controls/ContextMenu.h>

// ========== 对话框类控件 ==========
#include <mine/ui/controls/Dialog.h>
#include <mine/ui/controls/MessageBox.h>

// ========== 其他控件 ==========
#include <mine/ui/controls/Image.h>
#include <mine/ui/controls/Separator.h>
#include <mine/ui/controls/ToolTip.h>
```

**使用方式:**

```cpp
// 方式 1:引入统一头文件(快速开发)
#include <mine/ui/controls/ControlsAll.h>

// 方式 2:按需引入(生产环境推荐)
#include <mine/ui/controls/Button.h>
#include <mine/ui/controls/TextBox.h>
```

---

## 6. 使用场景

### 6.1 静态库模式

**场景:** 将 `mine.ui.controls` 编译为静态库(.lib/.a),链接到应用程序。

**CMakeLists.txt:**

```cmake
# 构建静态库
add_library(mine.ui.controls STATIC
    src/ContentControl.cpp
    src/Button.cpp
    src/CheckBox.cpp
    # ...
)

# 不定义导出宏(静态库无需导出符号)
# target_compile_definitions(mine.ui.controls PRIVATE MINE_UI_CONTROLS_EXPORTS)

# 链接依赖
target_link_libraries(mine.ui.controls
    PUBLIC mine.core
    PUBLIC mine.ui
    PUBLIC mine.paint
)
```

**应用程序 CMakeLists.txt:**

```cmake
add_executable(MyApp main.cpp)

# 链接静态库
target_link_libraries(MyApp PRIVATE mine.ui.controls)
```

**代码中使用:**

```cpp
#include <mine/ui/controls/Button.h>

int main() {
    auto button = std::make_unique<mine::ui::Button>();
    button->set_text("静态库模式");
    // ...
}
```

**特点:**

- **优点**:无需 DLL 文件,部署简单,符号内联优化
- **缺点**:可执行文件体积大,多个模块重复链接

---

### 6.2 动态库模式(Windows DLL)

**场景:** 将 `mine.ui.controls` 编译为 DLL,运行时动态加载。

**CMakeLists.txt:**

```cmake
# 构建 DLL
add_library(mine.ui.controls SHARED
    src/ContentControl.cpp
    src/Button.cpp
    src/CheckBox.cpp
    # ...
)

# 定义导出宏
target_compile_definitions(mine.ui.controls PRIVATE MINE_UI_CONTROLS_EXPORTS)

# 链接依赖(同样是 DLL)
target_link_libraries(mine.ui.controls
    PUBLIC mine.core
    PUBLIC mine.ui
    PUBLIC mine.paint
)
```

**应用程序 CMakeLists.txt:**

```cmake
add_executable(MyApp main.cpp)

# 链接导入库(mine.ui.controls.lib)
target_link_libraries(MyApp PRIVATE mine.ui.controls)
```

**部署:**

```
MyApp.exe
mine.core.dll
mine.ui.dll
mine.ui.controls.dll  ← 需要与 exe 在同一目录
mine.paint.dll
...
```

**代码中使用:**

```cpp
#include <mine/ui/controls/Button.h>

int main() {
    // MINE_UI_CONTROLS_API 自动展开为 __declspec(dllimport)
    auto button = std::make_unique<mine::ui::Button>();
    button->set_text("DLL 模式");
    // ...
}
```

**特点:**

- **优点**:可执行文件体积小,多个应用共享 DLL,模块化更新
- **缺点**:需要部署 DLL 文件,启动时加载 DLL 有性能开销

---

### 6.3 共享库模式(Linux .so / macOS .dylib)

**场景:** 在 Linux/macOS 上编译为共享库。

**CMakeLists.txt:**

```cmake
# 构建共享库
add_library(mine.ui.controls SHARED
    src/ContentControl.cpp
    src/Button.cpp
    src/CheckBox.cpp
    # ...
)

# 定义导出宏
target_compile_definitions(mine.ui.controls PRIVATE MINE_UI_CONTROLS_EXPORTS)

# 设置符号可见性(隐藏未导出符号)
set_target_properties(mine.ui.controls PROPERTIES
    CXX_VISIBILITY_PRESET hidden
    VISIBILITY_INLINES_HIDDEN YES
)

# 链接依赖
target_link_libraries(mine.ui.controls
    PUBLIC mine.core
    PUBLIC mine.ui
    PUBLIC mine.paint
)
```

**应用程序 CMakeLists.txt:**

```cmake
add_executable(MyApp main.cpp)

# 链接共享库
target_link_libraries(MyApp PRIVATE mine.ui.controls)

# 设置 RPATH(Linux)
set_target_properties(MyApp PROPERTIES
    INSTALL_RPATH "$ORIGIN"  # 在可执行文件同目录查找 .so
)
```

**部署:**

```
MyApp (可执行文件)
libmine.core.so
libmine.ui.so
libmine.ui.controls.so  ← 需要与 exe 在同一目录或 LD_LIBRARY_PATH 中
libmine.paint.so
...
```

**代码中使用:**

```cpp
#include <mine/ui/controls/Button.h>

int main() {
    // MINE_UI_CONTROLS_API 自动展开为 __attribute__((visibility("default")))
    auto button = std::make_unique<mine::ui::Button>();
    button->set_text("共享库模式");
    // ...
}
```

**特点:**

- **优点**:模块化,多个应用共享库,节省内存
- **缺点**:需要配置 LD_LIBRARY_PATH 或 RPATH

---

### 6.4 模块版本查询

**场景:** 运行时查询模块版本和依赖信息。

**代码:**

```cpp
#include <mine/ui/controls/ModuleTag.h>
#include <iostream>

void print_module_info() {
    using Tag = mine::ui::controls::ModuleTag;
    
    std::cout << "模块名称: " << Tag::name << std::endl;
    std::cout << "版本: " << Tag::version.major() << "." 
              << Tag::version.minor() << "." 
              << Tag::version.patch() << std::endl;
    std::cout << "描述: " << Tag::description << std::endl;
    std::cout << "作者: " << Tag::author << std::endl;
    std::cout << "许可证: " << Tag::license << std::endl;
    std::cout << "仓库: " << Tag::repository << std::endl;
    
    std::cout << "\n依赖模块:" << std::endl;
    for (size_t i = 0; i < Tag::dependency_count; ++i) {
        std::cout << "  - " << Tag::dependencies[i] << std::endl;
    }
}

int main() {
    print_module_info();
}
```

**输出:**

```
模块名称: mine.ui.controls
版本: 0.1.0
描述: MineFramework UI 控件库,提供 MD3 风格的原生控件
作者: MineFramework Team
许可证: MIT
仓库: https://github.com/mineframework/mine

依赖模块:
  - mine.core@0.1.0
  - mine.ui@0.1.0
  - mine.ui.layout@0.1.0
  - mine.ui.visual@0.1.0
  - mine.ui.property@0.1.0
  - mine.ui.event@0.1.0
  - mine.ui.animation@0.1.0
  - mine.paint@0.1.0
```

---

### 6.5 跨平台构建脚本(xmake)

**场景:** 使用 xmake 构建跨平台 DLL/SO/DYLIB。

**xmake.lua:**

```lua
-- mine.ui.controls 模块
target("mine.ui.controls")
    -- 设置为共享库
    set_kind("shared")
    
    -- 定义导出宏
    add_defines("MINE_UI_CONTROLS_EXPORTS")
    
    -- 源文件
    add_files("src/**.cpp")
    add_headerfiles("src/**.h")
    
    -- 头文件搜索路径
    add_includedirs("src", {public = true})
    
    -- 依赖模块
    add_deps("mine.core", "mine.ui", "mine.paint")
    
    -- Windows 特定设置
    if is_plat("windows") then
        add_defines("_USRDLL")
    end
    
    -- Linux/macOS 特定设置
    if is_plat("linux", "macosx") then
        -- 隐藏未导出符号
        add_cxflags("-fvisibility=hidden", {force = true})
        add_cxxflags("-fvisibility-inlines-hidden", {force = true})
    end
    
    -- 安装规则
    on_install(function (target)
        -- 安装头文件
        os.cp(path.join(target:targetdir(), "*.h"), 
              path.join(target:installdir(), "include/mine/ui/controls"))
        
        -- 安装库文件
        os.cp(target:targetfile(), target:installdir("lib"))
        
        -- Windows: 安装 DLL 到 bin 目录
        if is_plat("windows") then
            os.cp(path.join(target:targetdir(), "*.dll"), 
                  target:installdir("bin"))
        end
    end)
```

**构建命令:**

```bash
# 构建
xmake build mine.ui.controls

# 安装到指定目录
xmake install -o /opt/mineframework mine.ui.controls

# 清理
xmake clean mine.ui.controls
```

---

## 7. 最佳实践

### 7.1 优先使用静态库(开发阶段)

**原因:**
- 无需管理 DLL 路径,调试方便
- 符号内联优化,性能更好

**推荐:**

```cmake
# 开发阶段使用静态库
add_library(mine.ui.controls STATIC ...)
```

**发布阶段切换为动态库:**

```cmake
# 发布阶段使用动态库
add_library(mine.ui.controls SHARED ...)
```

---

### 7.2 使用 CMake 选项控制链接模式

**原因:**
- 同一套代码支持静态库和动态库
- 用户可根据需求选择

**CMakeLists.txt:**

```cmake
# 添加构建选项
option(BUILD_SHARED_LIBS "构建共享库" OFF)

if(BUILD_SHARED_LIBS)
    add_library(mine.ui.controls SHARED ...)
    target_compile_definitions(mine.ui.controls PRIVATE MINE_UI_CONTROLS_EXPORTS)
else()
    add_library(mine.ui.controls STATIC ...)
endif()
```

**使用:**

```bash
# 构建静态库(默认)
cmake -B build

# 构建动态库
cmake -B build -DBUILD_SHARED_LIBS=ON
```

---

### 7.3 按需引入头文件

**原因:**
- 引入 `ControlsAll.h` 会增加编译时间
- 生产环境只需要用到的控件

**反模式:**

```cpp
// ❌ 不推荐:引入所有控件(编译慢)
#include <mine/ui/controls/ControlsAll.h>

int main() {
    auto button = std::make_unique<mine::ui::Button>();
    // 只用了 Button,但引入了 50+ 个控件头文件
}
```

**推荐模式:**

```cpp
// ✅ 推荐:按需引入
#include <mine/ui/controls/Button.h>
#include <mine/ui/controls/TextBox.h>

int main() {
    auto button = std::make_unique<mine::ui::Button>();
    auto textbox = std::make_unique<mine::ui::TextBox>();
}
```

---

### 7.4 使用 ModuleTag 验证依赖版本

**原因:**
- 运行时检测版本不匹配,避免崩溃
- 提供友好的错误提示

**代码:**

```cpp
#include <mine/ui/controls/ModuleTag.h>
#include <mine/core/ModuleSystem.h>

void validate_dependencies() {
    using Tag = mine::ui::controls::ModuleTag;
    
    for (size_t i = 0; i < Tag::dependency_count; ++i) {
        auto dep = Tag::dependencies[i];
        auto [name, min_version] = parse_dependency(dep);
        
        auto loaded_version = mine::core::ModuleSystem::get_version(name);
        if (loaded_version < min_version) {
            throw std::runtime_error(
                std::format("模块 {} 需要 {} >= {},当前 {}",
                            Tag::name, name, min_version, loaded_version)
            );
        }
    }
}

int main() {
    try {
        validate_dependencies();
        // 继续初始化...
    }
    catch (const std::exception& e) {
        std::cerr << "依赖检查失败: " << e.what() << std::endl;
        return 1;
    }
}
```

---

## 8. 常见陷阱

### 8.1 忘记定义 MINE_UI_CONTROLS_EXPORTS

**问题:** 构建 DLL 时忘记定义导出宏,导致符号未导出。

**错误现象(Windows):**

```
链接错误 LNK2019: 无法解析的外部符号 "public: __cdecl mine::ui::Button::Button(void)"
```

**错误现象(Linux):**

```
undefined reference to `mine::ui::Button::Button()'
```

**原因:**

```cmake
# ❌ 忘记定义导出宏
add_library(mine.ui.controls SHARED ...)
# target_compile_definitions(mine.ui.controls PRIVATE MINE_UI_CONTROLS_EXPORTS)
```

**后果:** 符号未导出,链接失败。

**正确做法:**

```cmake
# ✅ 定义导出宏
add_library(mine.ui.controls SHARED ...)
target_compile_definitions(mine.ui.controls PRIVATE MINE_UI_CONTROLS_EXPORTS)
```

---

### 8.2 DLL/SO 路径配置错误

**问题:** 运行时找不到 DLL/SO 文件。

**错误现象(Windows):**

```
系统找不到指定的文件:mine.ui.controls.dll
```

**错误现象(Linux):**

```
error while loading shared libraries: libmine.ui.controls.so: cannot open shared object file
```

**原因:** DLL/SO 不在系统搜索路径中。

**解决方案(Windows):**

1. 将 DLL 放在 exe 同目录
2. 或添加到 PATH 环境变量

**解决方案(Linux):**

1. 将 .so 放在 exe 同目录,并设置 RPATH:
```cmake
set_target_properties(MyApp PROPERTIES
    INSTALL_RPATH "$ORIGIN"
)
```

2. 或添加到 LD_LIBRARY_PATH:
```bash
export LD_LIBRARY_PATH=/path/to/libs:$LD_LIBRARY_PATH
./MyApp
```

3. 或安装到系统路径(需 root):
```bash
sudo cp libmine.ui.controls.so /usr/local/lib
sudo ldconfig
```

---

### 8.3 静态库与动态库混用

**问题:** 部分模块使用静态库,部分使用动态库,导致符号重复。

**错误现象:**

```
multiple definition of `mine::ui::Button::Button()'
```

**原因:**

```cmake
# ❌ 混用静态库和动态库
add_library(mine.core STATIC ...)       # 静态库
add_library(mine.ui SHARED ...)         # 动态库,链接 mine.core
add_library(mine.ui.controls SHARED ...)# 动态库,链接 mine.core

# 结果:mine.core 的符号在 mine.ui.dll 和 mine.ui.controls.dll 中都有定义
```

**后果:** 符号重复定义,链接失败。

**正确做法:**

```cmake
# ✅ 统一使用动态库
add_library(mine.core SHARED ...)
add_library(mine.ui SHARED ...)
add_library(mine.ui.controls SHARED ...)

# 或统一使用静态库
add_library(mine.core STATIC ...)
add_library(mine.ui STATIC ...)
add_library(mine.ui.controls STATIC ...)
```

---

### 8.4 跨平台符号可见性问题

**问题:** Linux/macOS 上未设置符号可见性,导致所有符号都导出,增加库体积和加载时间。

**错误做法:**

```cmake
# ❌ 未设置符号可见性
add_library(mine.ui.controls SHARED ...)
target_compile_definitions(mine.ui.controls PRIVATE MINE_UI_CONTROLS_EXPORTS)
# 缺少:set_target_properties(... CXX_VISIBILITY_PRESET hidden)
```

**后果:** 所有符号都导出,库体积大,加载慢。

**正确做法:**

```cmake
# ✅ 设置符号可见性
add_library(mine.ui.controls SHARED ...)
target_compile_definitions(mine.ui.controls PRIVATE MINE_UI_CONTROLS_EXPORTS)
set_target_properties(mine.ui.controls PROPERTIES
    CXX_VISIBILITY_PRESET hidden
    VISIBILITY_INLINES_HIDDEN YES
)
```

**验证(Linux):**

```bash
# 列出导出符号
nm -D libmine.ui.controls.so | grep " T "

# 预期:只有标记 MINE_UI_CONTROLS_API 的符号
```

---

## 9. 总结

### 9.1 核心要点

- **MINE_UI_CONTROLS_API 宏**:跨平台处理 DLL/SO 导出/导入,Windows 使用 `__declspec(dllexport/dllimport)`,Linux/macOS 使用 `__attribute__((visibility("default")))`。
- **ModuleTag 结构体**:声明模块元数据(名称、版本、依赖),由模块系统读取,用于依赖检查和版本验证。
- **ControlsAll.h 统一头文件**:一次性引入所有控件头文件,适合快速开发,生产环境建议按需引入。
- **构建模式**:支持静态库和动态库两种模式,通过 CMake 选项或 xmake 配置切换。

### 9.2 使用建议

- **开发阶段使用静态库**,调试方便,无需管理 DLL 路径。
- **发布阶段使用动态库**,模块化,节省内存,支持独立更新。
- **按需引入头文件**,避免引入 `ControlsAll.h` 增加编译时间。
- **使用 ModuleTag 验证依赖版本**,运行时检测版本不匹配。

### 9.3 扩展方向

- **插件系统**:基于 ModuleTag 实现插件加载和版本检查。
- **热重载**:动态库模式下支持热重载,无需重启应用。
- **模块市场**:发布模块到 NuGet/Conan,供其他项目使用。
- **版本升级工具**:自动检查依赖模块版本,提示升级。

### 9.4 相关文档

- [CMake 构建指南](../../15-build-xmake.md)
- [模块系统架构](../../01-architecture.md)
- [SDK 打包与发布](../../16-sdk-packaging.md)
- [跨平台编译配置](../../18-coding-conventions.md)

---

**文档版本:** v0.1.0  
**最后更新:** 2026-06-11  
**维护者:** MineFramework Team
