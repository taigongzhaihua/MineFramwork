# ModuleTag 详细接口文档

## 概述

`ModuleTag` 是 `mine.text` 模块的**模块标识常量**，提供编译期模块名称字符串。

**核心特性：**
- **编译期常量**：`inline constexpr` 修饰
- **模块识别**：用于日志、诊断、依赖管理
- **零开销**：编译期字符串常量

---

## 文件位置

```
src/mine/text/api/include/mine/text/ModuleTag.h
```

---

## 常量定义

### kModuleName

```cpp
inline constexpr const char* kModuleName = "mine.text";
```

**描述**：`mine.text` 模块的唯一标识字符串。

**类型**：`const char*`

**修饰符**：`inline constexpr`

**特征**：
- 编译期常量（可用于 `constexpr` 上下文）
- 全局唯一（模块命名空间内）
- 零运行时开销（编译期内联）

---

## 使用场景

### 1. 日志输出

```cpp
#include <mine/text/ModuleTag.h>
#include <iostream>

void log_info(const char* msg) {
    std::cout << "[" << mine::text::kModuleName << "] " << msg << std::endl;
}

int main() {
    log_info("字体加载成功");
    // 输出: [mine.text] 字体加载成功
}
```

---

### 2. 模块注册

```cpp
struct ModuleInfo {
    const char* name;
    const char* version;
};

ModuleInfo text_module{
    mine::text::kModuleName,
    "1.0.0"
};

void register_module(const ModuleInfo& info) {
    std::cout << "注册模块: " << info.name << " v" << info.version << std::endl;
}

int main() {
    register_module(text_module);
    // 输出: 注册模块: mine.text v1.0.0
}
```

---

### 3. 依赖声明

```cpp
struct DependencyDecl {
    const char* module_name;
    const char* required_by;
};

constexpr DependencyDecl deps[] = {
    {mine::text::kModuleName, "mine.ui"},
    {"mine.math", mine::text::kModuleName}
};
```

---

### 4. 编译期验证

```cpp
static_assert(
    std::string_view{mine::text::kModuleName} == "mine.text",
    "模块名称不匹配"
);
```

---

### 5. 错误报告

```cpp
void report_error(const char* module, const char* msg) {
    std::cerr << "[错误] " << module << ": " << msg << std::endl;
}

void load_font(const char* path) {
    if (!file_exists(path)) {
        report_error(mine::text::kModuleName, "字体文件不存在");
    }
}
```

---

### 6. 调试信息

```cpp
void print_module_info() {
    std::cout << "当前模块: " << mine::text::kModuleName << std::endl;
    std::cout << "版本: " << __DATE__ << std::endl;
}
```

---

## 设计考虑

### inline constexpr

**特征**：
- `constexpr`：编译期常量
- `inline`：多个编译单元包含同一头文件时不会产生重复定义

**优势**：
- 零运行时开销
- 可用于 `constexpr` 上下文
- 可用于模板参数

---

### 模块命名约定

**格式**：`mine.<模块名>`

**示例**：
- `mine.core`
- `mine.containers`
- `mine.math`
- `mine.text`
- `mine.platform.abi`

---

## 与其他模块对比

```cpp
// mine.core
namespace mine::core {
    inline constexpr const char* kModuleName = "mine.core";
}

// mine.containers
namespace mine::containers {
    inline constexpr const char* kModuleName = "mine.containers";
}

// mine.math
namespace mine::math {
    inline constexpr const char* kModuleName = "mine.math";
}

// mine.text
namespace mine::text {
    inline constexpr const char* kModuleName = "mine.text";
}

// mine.platform.abi
namespace mine::platform {
    inline constexpr const char* kModuleName = "mine.platform.abi";
}
```

---

## 最佳实践

### 1. 统一日志接口

```cpp
class Logger {
    const char* module_name_;
    
public:
    explicit Logger(const char* module_name)
        : module_name_(module_name) {}
    
    void info(const char* msg) {
        std::cout << "[INFO] [" << module_name_ << "] " << msg << std::endl;
    }
    
    void error(const char* msg) {
        std::cerr << "[ERROR] [" << module_name_ << "] " << msg << std::endl;
    }
};

Logger text_logger{mine::text::kModuleName};

void load_font(const char* path) {
    text_logger.info("开始加载字体");
    // ...
    text_logger.info("字体加载成功");
}
```

---

### 2. 模块版本管理

```cpp
struct ModuleRegistry {
    struct Entry {
        const char* name;
        const char* version;
        const char* build_date;
    };
    
    static constexpr Entry entries[] = {
        {mine::text::kModuleName, "1.0.0", __DATE__}
    };
};
```

---

### 3. 跨模块日志

```cpp
#include <mine/core/ModuleTag.h>
#include <mine/text/ModuleTag.h>

void log_with_module(const char* module, const char* msg) {
    std::cout << "[" << module << "] " << msg << std::endl;
}

int main() {
    log_with_module(mine::core::kModuleName, "核心初始化");
    log_with_module(mine::text::kModuleName, "字体系统初始化");
}
```

---

## 常见陷阱

### 1. 忘记命名空间

```cpp
// ❌ 问题：忘记命名空间
const char* name = kModuleName;  // 编译错误

// ✅ 解决：完整命名空间
const char* name = mine::text::kModuleName;
```

---

### 2. 运行时修改

```cpp
// ❌ 问题：尝试修改常量
const_cast<char*>(mine::text::kModuleName)[0] = 'X';  // 未定义行为

// ✅ 解决：只读使用
const char* name = mine::text::kModuleName;
```

---

### 3. 混淆模块名

```cpp
// ❌ 问题：使用错误的模块名
log_error(mine::core::kModuleName, "字体加载失败");  // 应使用 mine::text::kModuleName

// ✅ 解决：使用正确的模块名
log_error(mine::text::kModuleName, "字体加载失败");
```

---

## 完整示例

```cpp
#include <mine/text/ModuleTag.h>
#include <iostream>
#include <string_view>

class TextModule {
public:
    static constexpr const char* get_name() noexcept {
        return mine::text::kModuleName;
    }
    
    static void print_info() {
        std::cout << "模块名称: " << get_name() << std::endl;
        std::cout << "模块描述: Unicode 文字处理与字体渲染" << std::endl;
    }
    
    static bool verify_name() {
        return std::string_view{get_name()} == "mine.text";
    }
};

int main() {
    // 打印模块信息
    TextModule::print_info();
    
    // 验证模块名
    if (TextModule::verify_name()) {
        std::cout << "模块名称验证通过" << std::endl;
    }
    
    // 日志输出
    std::cout << "[" << mine::text::kModuleName << "] 模块初始化成功" << std::endl;
    
    return 0;
}
```

**输出**：
```
模块名称: mine.text
模块描述: Unicode 文字处理与字体渲染
模块名称验证通过
[mine.text] 模块初始化成功
```

---

## 总结

`ModuleTag` 是 `mine.text` 模块的模块标识常量，具备：

1. **编译期常量**：`inline constexpr` 修饰
2. **模块识别**：用于日志、诊断、依赖管理
3. **零开销**：编译期字符串常量

**使用建议**：
- 用于日志输出和错误报告
- 用于模块注册和依赖声明
- 不要尝试修改常量值
- 使用完整命名空间限定（`mine::text::kModuleName`）
