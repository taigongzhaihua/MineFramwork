# 元文件详细接口文档

## 概述

本文档记录 `mine.ui.event` 模块的**元文件**（Meta Files）。

**包含文件：**
1. **ModuleTag.h**：模块标识常量
2. **Api.h**：DLL 导出/导入宏定义
3. **Event.h**：伞形头文件（包含所有公共 API）

---

## 1. ModuleTag.h

### 文件位置

```
src/mine/ui/event/api/include/mine/ui/event/ModuleTag.h
```

---

### 模块名称常量

#### kEventModuleName

```cpp
inline constexpr const char* kEventModuleName = "mine.ui.event";
```

**描述**：模块名称常量，用于日志和诊断信息标识。

**使用场景**：
- 日志输出（标识日志来源模块）
- 诊断信息（错误报告、调试信息）
- 模块识别（运行时模块查询）

**示例**：
```cpp
#include <mine/ui/event/ModuleTag.h>
#include <iostream>

using namespace mine::ui;

void log_message(const char* message) {
    std::cout << "[" << kEventModuleName << "] " << message << std::endl;
}

int main() {
    log_message("Event system initialized");
    // 输出：[mine.ui.event] Event system initialized
    
    return 0;
}
```

---

## 2. Api.h

### 文件位置

```
src/mine/ui/event/api/include/mine/ui/event/Api.h
```

---

### DLL 导出/导入宏

#### MINE_UI_EVENT_API

```cpp
#if defined(_MSC_VER) || defined(__MINGW32__)
#   if defined(MINE_BUILDING_MINE_UI_EVENT)
#       define MINE_UI_EVENT_API __declspec(dllexport)
#   elif defined(MINE_BUILD_SHARED)
#       define MINE_UI_EVENT_API __declspec(dllimport)
#   else
#       define MINE_UI_EVENT_API
#   endif
#elif defined(__GNUC__) || defined(__clang__)
#   if defined(MINE_BUILDING_MINE_UI_EVENT)
#       define MINE_UI_EVENT_API __attribute__((visibility("default")))
#   else
#       define MINE_UI_EVENT_API
#   endif
#else
#   define MINE_UI_EVENT_API
#endif
```

**描述**：mine.ui.event 模块 DLL 导出/导入宏定义。

**宏说明**：
- `MINE_BUILDING_MINE_UI_EVENT`：构建 mine.ui.event 模块时定义（导出符号）
- `MINE_BUILD_SHARED`：构建共享库时定义（导入符号）
- 未定义上述宏：静态链接（无需导出/导入）

**平台支持**：
- **MSVC / MinGW**：使用 `__declspec(dllexport)` / `__declspec(dllimport)`
- **GCC / Clang**：使用 `__attribute__((visibility("default")))`
- **其他平台**：定义为空（静态链接）

**使用场景**：
- 标记需要跨 DLL 边界可见的类、函数、变量
- 确保 ABI 稳定性

**示例**：
```cpp
#include <mine/ui/event/Api.h>

namespace mine::ui {

// 导出类
class MINE_UI_EVENT_API EventManager {
public:
    static void raise(IRoutedEventTarget& source, RoutedEventArgs& args) noexcept;
};

// 导出函数
MINE_UI_EVENT_API
const RoutedEvent& register_event(core::StringView name,
                                   core::TypeId     owner_type,
                                   RoutingStrategy  strategy);

} // namespace mine::ui
```

---

## 3. Event.h

### 文件位置

```
src/mine/ui/event/api/include/mine/ui/event/Event.h
```

---

### 伞形头文件

```cpp
#pragma once

#include <mine/ui/event/Api.h>
#include <mine/ui/event/EventManager.h>
#include <mine/ui/event/ICommand.h>
#include <mine/ui/event/IRoutedEventTarget.h>
#include <mine/ui/event/ModuleTag.h>
#include <mine/ui/event/RelayCommand.h>
#include <mine/ui/event/RoutedEvent.h>
#include <mine/ui/event/RoutedEventArgs.h>
#include <mine/ui/event/RoutingStrategy.h>
```

**描述**：mine.ui.event 模块公共头聚合。

**包含的头文件**：
1. `Api.h`：DLL 导出/导入宏定义
2. `EventManager.h`：路由事件派发器
3. `ICommand.h`：命令接口
4. `IRoutedEventTarget.h`：路由事件目标接口
5. `ModuleTag.h`：模块标识常量
6. `RelayCommand.h`：中继命令
7. `RoutedEvent.h`：路由事件描述符
8. `RoutedEventArgs.h`：路由事件参数
9. `RoutingStrategy.h`：路由策略

**使用场景**：
- 一次性包含所有公共 API（简化头文件包含）
- 减少编译依赖（统一包含点）

**示例**：
```cpp
#include <mine/ui/event/Event.h>

using namespace mine::ui;

int main() {
    // 现在可以使用所有 mine.ui.event API
    
    // 注册路由事件
    const RoutedEvent& ClickEvent =
        register_event<Button>("Click", RoutingStrategy::Bubble);
    
    // 创建命令
    RelayCommand save_command{
        [](const core::Variant&) { std::cout << "Save" << std::endl; }
    };
    
    // 派发事件
    RoutedEventArgs args{ClickEvent};
    EventManager::raise(*button, args);
    
    return 0;
}
```

---

## 使用场景

### 1. 使用模块名称常量记录日志

```cpp
#include <mine/ui/event/ModuleTag.h>
#include <mine/core/Logger.h>

using namespace mine::ui;

void init_event_system() {
    Logger::info(kEventModuleName, "Initializing event system");
    // 输出：[mine.ui.event] Initializing event system
}
```

---

### 2. 使用 DLL 导出宏标记公共 API

```cpp
#include <mine/ui/event/Api.h>

namespace mine::ui {

// ✅ 导出类
class MINE_UI_EVENT_API MyEventHandler {
public:
    void handle(RoutedEventArgs& args);
};

// ✅ 导出函数
MINE_UI_EVENT_API
void register_custom_event(const char* name);

// ✅ 导出全局变量
MINE_UI_EVENT_API extern const RoutedEvent& CustomEvent;

} // namespace mine::ui
```

---

### 3. 使用伞形头文件简化包含

```cpp
// ❌ 不推荐：逐个包含头文件
#include <mine/ui/event/RoutingStrategy.h>
#include <mine/ui/event/RoutedEventArgs.h>
#include <mine/ui/event/RoutedEvent.h>
#include <mine/ui/event/EventManager.h>
#include <mine/ui/event/ICommand.h>
#include <mine/ui/event/RelayCommand.h>

// ✅ 推荐：使用伞形头文件
#include <mine/ui/event/Event.h>
```

---

## 最佳实践

### 1. 使用模块名称常量而非硬编码字符串

```cpp
// ✅ 推荐：使用模块名称常量
Logger::info(kEventModuleName, "Event triggered");

// ❌ 不推荐：硬编码字符串
Logger::info("mine.ui.event", "Event triggered");
```

---

### 2. 仅在公共 API 使用导出宏

```cpp
// ✅ 推荐：公共 API 使用导出宏
class MINE_UI_EVENT_API EventManager {
public:
    static void raise(IRoutedEventTarget& source, RoutedEventArgs& args);
};

// ✅ 推荐：内部实现不使用导出宏
namespace internal {
    class EventManagerImpl {
        // 内部实现，不导出
    };
} // namespace internal
```

---

### 3. 在实现文件中使用伞形头文件

```cpp
// MyEventHandler.cpp

// ✅ 推荐：使用伞形头文件
#include <mine/ui/event/Event.h>

namespace mine::ui {

void MyEventHandler::handle(RoutedEventArgs& args) {
    // 实现逻辑
}

} // namespace mine::ui
```

---

### 4. 避免在头文件中使用伞形头文件

```cpp
// MyEventHandler.h

// ❌ 不推荐：在头文件中使用伞形头文件（增加编译依赖）
#include <mine/ui/event/Event.h>

// ✅ 推荐：在头文件中仅包含必要的头文件
#include <mine/ui/event/IRoutedEventTarget.h>
#include <mine/ui/event/RoutedEventArgs.h>

namespace mine::ui {

class MyEventHandler : public IRoutedEventTarget {
    // ...
};

} // namespace mine::ui
```

---

## 常见陷阱

### 1. 硬编码模块名称字符串

```cpp
// ❌ 问题：硬编码字符串
Logger::info("mine.ui.event", "Event triggered");

// 如果模块名称变更，需要修改所有硬编码字符串

// ✅ 解决：使用模块名称常量
Logger::info(kEventModuleName, "Event triggered");
```

---

### 2. 忘记使用导出宏

```cpp
// ❌ 问题：忘记使用导出宏
class EventManager {  // ❌ 未标记为导出
public:
    static void raise(IRoutedEventTarget& source, RoutedEventArgs& args);
};

// 链接错误：无法解析外部符号

// ✅ 解决：使用导出宏
class MINE_UI_EVENT_API EventManager {
public:
    static void raise(IRoutedEventTarget& source, RoutedEventArgs& args);
};
```

---

### 3. 在头文件中使用伞形头文件

```cpp
// ❌ 问题：在头文件中使用伞形头文件（增加编译依赖）
// MyEventHandler.h
#include <mine/ui/event/Event.h>

// 任何包含 MyEventHandler.h 的文件都会传递包含所有 event 头文件

// ✅ 解决：在头文件中仅包含必要的头文件
#include <mine/ui/event/IRoutedEventTarget.h>
```

---

### 4. 在静态库中使用错误的导出宏

```cpp
// ❌ 问题：在静态库中定义 MINE_BUILD_SHARED
// CMakeLists.txt
add_library(mine_ui_event STATIC ...)
target_compile_definitions(mine_ui_event PRIVATE MINE_BUILD_SHARED)  # ❌ 错误

// 静态库不需要导入符号

// ✅ 解决：静态库不定义 MINE_BUILD_SHARED
add_library(mine_ui_event STATIC ...)
# 不定义 MINE_BUILD_SHARED，MINE_UI_EVENT_API 为空
```

---

## 完整示例

```cpp
// ════════════════════════════════════════════════════════════════════════════
// MyEventHandler.h
// ════════════════════════════════════════════════════════════════════════════

#pragma once

#include <mine/ui/event/Api.h>
#include <mine/ui/event/IRoutedEventTarget.h>
#include <mine/ui/event/RoutedEventArgs.h>

namespace mine::ui {

class MINE_UI_EVENT_API MyEventHandler : public IRoutedEventTarget {
public:
    MyEventHandler();
    
    [[nodiscard]] IRoutedEventTarget* parent_target() const noexcept override;
    void invoke_handlers(const RoutedEvent& event,
                         RoutedEventArgs&   args) noexcept override;

private:
    IRoutedEventTarget* parent_ = nullptr;
};

} // namespace mine::ui

// ════════════════════════════════════════════════════════════════════════════
// MyEventHandler.cpp
// ════════════════════════════════════════════════════════════════════════════

#include "MyEventHandler.h"
#include <mine/ui/event/Event.h>  // ✅ 在实现文件中使用伞形头文件
#include <mine/ui/event/ModuleTag.h>
#include <mine/core/Logger.h>
#include <iostream>

namespace mine::ui {

MyEventHandler::MyEventHandler() {
    // 使用模块名称常量记录日志
    std::cout << "[" << kEventModuleName << "] MyEventHandler created" << std::endl;
}

IRoutedEventTarget* MyEventHandler::parent_target() const noexcept {
    return parent_;
}

void MyEventHandler::invoke_handlers(const RoutedEvent& event,
                                       RoutedEventArgs&   args) noexcept {
    std::cout << "[" << kEventModuleName << "] Event: " << event.name() << std::endl;
}

} // namespace mine::ui

// ════════════════════════════════════════════════════════════════════════════
// main.cpp
// ════════════════════════════════════════════════════════════════════════════

#include <mine/ui/event/Event.h>  // ✅ 使用伞形头文件
#include <mine/ui/event/ModuleTag.h>
#include "MyEventHandler.h"

using namespace mine::ui;

int main() {
    // 使用模块名称常量
    std::cout << "Initializing " << kEventModuleName << " module" << std::endl;
    
    // 创建事件处理器
    MyEventHandler handler;
    
    // 注册路由事件
    const RoutedEvent& ClickEvent =
        register_event<MyEventHandler>("Click", RoutingStrategy::Bubble);
    
    // 触发事件
    RoutedEventArgs args{ClickEvent};
    args.set_source(&handler);
    args.set_original_source(&handler);
    EventManager::raise(handler, args);
    
    // 输出：
    // Initializing mine.ui.event module
    // [mine.ui.event] MyEventHandler created
    // [mine.ui.event] Event: Click
    
    return 0;
}
```

---

## 总结

mine.ui.event 模块的元文件包含：

1. **ModuleTag.h**：
   - `kEventModuleName`：模块名称常量（"mine.ui.event"）
   - 用于日志、诊断信息标识

2. **Api.h**：
   - `MINE_UI_EVENT_API`：DLL 导出/导入宏定义
   - 支持 MSVC/MinGW/GCC/Clang
   - 确保跨 DLL 边界的 ABI 稳定性

3. **Event.h**：
   - 伞形头文件（包含所有公共 API）
   - 简化头文件包含
   - 减少编译依赖

**使用建议**：
- 使用模块名称常量而非硬编码字符串
- 仅在公共 API 使用导出宏
- 在实现文件中使用伞形头文件
- 避免在头文件中使用伞形头文件（减少编译依赖）
- 静态库不定义 `MINE_BUILD_SHARED`
