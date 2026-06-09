# TypeId 类详细接口文档

## 概述

**模块**：`mine.core`  
**头文件**：`<mine/core/TypeId.h>`  
**命名空间**：`mine::core`

**用途**：无 RTTI 的编译期类型标识符，基于 C++17 inline 变量地址唯一性实现 O(1) 类型判断。

**核心特性**：
- 零运行时开销的类型标识（编译期确定地址）
- 不依赖 `std::type_info` 或 RTTI（可在 `/GR-` 下使用）
- 可作为哈希表键（通过 `hash()` 方法）
- DLL 边界内类型标识稳定（跨 DLL 需额外注册）
- cv 限定符与引用自动剥除（`TypeId::of<T>()` == `TypeId::of<const T&>()`）

---

## 类定义

```cpp
namespace mine::core {

class TypeId {
public:
    constexpr TypeId() noexcept = default;

    template<typename T>
    [[nodiscard]] static constexpr TypeId of() noexcept;

    [[nodiscard]] constexpr bool valid() const noexcept;
    [[nodiscard]] uintptr_t hash() const noexcept;

    [[nodiscard]] constexpr bool operator==(TypeId other) const noexcept;
    [[nodiscard]] constexpr bool operator!=(TypeId other) const noexcept;

private:
    const void* anchor_{nullptr};
};

} // namespace mine::core
```

---

## 构造函数

### `TypeId()`

**签名**：
```cpp
constexpr TypeId() noexcept = default;
```

**描述**：默认构造一个无效的 TypeId。

**后置条件**：
- `valid()` 返回 `false`
- `hash()` 返回实现定义的值（通常为 0）

**示例**：
```cpp
TypeId invalid_id;
assert(!invalid_id.valid());
```

---

## 静态工厂方法

### `TypeId::of<T>()`

**签名**：
```cpp
template<typename T>
[[nodiscard]] static constexpr TypeId of() noexcept;
```

**描述**：获取类型 `T` 的 TypeId。

**模板参数**：
- `T`：任意完整类型（cv 限定符、引用会被自动剥除）

**返回值**：
- 返回类型 `T` 的唯一标识符
- 同一类型在同一 DLL/可执行文件内保证返回相同 TypeId

**类型归一化**：
- `TypeId::of<int>()` == `TypeId::of<const int>()`
- `TypeId::of<Button>()` == `TypeId::of<Button&>()`
- `TypeId::of<const Button*>()` == `TypeId::of<Button*>()` (指针本身的 cv)

**约束**：
- `T` 必须为完整类型（不可为前向声明）
- `T` 不可为 `void`

**时间复杂度**：O(1)，编译期常量

**示例**：
```cpp
TypeId button_id = TypeId::of<Button>();
TypeId int_id    = TypeId::of<int>();

// cv 限定符不影响
static_assert(TypeId::of<int>() == TypeId::of<const int>());

// 继承层次中每个类型有独立 TypeId
class Base {};
class Derived : public Base {};
assert(TypeId::of<Base>() != TypeId::of<Derived>());
```

---

## 成员方法

### `valid()`

**签名**：
```cpp
[[nodiscard]] constexpr bool valid() const noexcept;
```

**描述**：检查 TypeId 是否有效（非默认构造的空状态）。

**返回值**：
- `true`：有效的类型标识符（通过 `of<T>()` 构造）
- `false`：默认构造的空 TypeId

**无副作用**。

**示例**：
```cpp
TypeId empty;
assert(!empty.valid());

TypeId valid_id = TypeId::of<int>();
assert(valid_id.valid());
```

---

### `hash()`

**签名**：
```cpp
[[nodiscard]] uintptr_t hash() const noexcept;
```

**描述**：返回可用于哈希表的整数值。

**返回值**：
- 无符号整数，同一类型的 TypeId 返回相同值
- 不同类型返回不同值（在链接单元内保证唯一性）
- 数值本身无语义，不可序列化或跨进程传递

**用途**：
- 作为 `HashMap<TypeId, Value>` 的键
- 作为 `switch` 语句的 case 值（性能优于虚函数）

**时间复杂度**：O(1)

**示例**：
```cpp
std::unordered_map<uintptr_t, std::string> type_names;
type_names[TypeId::of<Button>().hash()] = "Button";
type_names[TypeId::of<TextBlock>().hash()] = "TextBlock";
```

---

## 比较运算符

### `operator==` / `operator!=`

**签名**：
```cpp
[[nodiscard]] constexpr bool operator==(TypeId other) const noexcept;
[[nodiscard]] constexpr bool operator!=(TypeId other) const noexcept;
```

**描述**：比较两个 TypeId 是否代表相同类型。

**参数**：
- `other`：待比较的另一个 TypeId

**返回值**：
- `operator==`：相同类型返回 `true`
- `operator!=`：不同类型返回 `true`

**约束**：
- 仅在同一 DLL/可执行文件内比较有效
- 跨 DLL 边界需通过 `mine.reflect` 模块注册类型名

**时间复杂度**：O(1)（指针比较）

**示例**：
```cpp
if (element->type_id() == TypeId::of<Button>()) {
    auto* btn = static_cast<Button*>(element);
    btn->set_text("确定");
}
```

---

## 实现原理

### 内部锚点变量

```cpp
namespace detail {
    template<typename T>
    inline constexpr char kTypeAnchor = 0;
}
```

**机制**：
- 每个类型 `T` 实例化一个 `inline constexpr` 变量
- C++17 保证同一链接单元内地址唯一
- `TypeId::of<T>()` 返回 `&kTypeAnchor<std::remove_cvref_t<T>>`

**优势**：
- 无运行时成本（地址编译期确定）
- 无需虚函数表（不占对象空间）
- 不依赖 RTTI（可在 `/GR-` 或 `-fno-rtti` 下工作）

---

## 使用场景

### 1. 依赖属性系统

```cpp
class DependencyProperty {
public:
    TypeId owner_type() const noexcept;
    TypeId value_type() const noexcept;
};

// 注册属性时记录拥有者类型
DependencyProperty::Register(
    "Text",
    TypeId::of<String>(),     // 值类型
    TypeId::of<TextBlock>(),  // 拥有者类型
    {});
```

### 2. 类型安全的向下转型

```cpp
UIElement* element = find_element_by_name("OkButton");

if (element->type_id() == TypeId::of<Button>()) {
    auto* btn = static_cast<Button*>(element);  // 安全：已验证类型
    btn->set_content("确定");
}
```

### 3. 事件路由类型过滤

```cpp
void handle_event(RoutedEvent evt, UIElement* sender) {
    if (sender->type_id() == TypeId::of<TextBox>()) {
        // 仅处理来自 TextBox 的事件
    }
}
```

### 4. 序列化类型标签

```cpp
void serialize_element(UIElement* element, Stream& out) {
    uintptr_t type_hash = element->type_id().hash();
    out.write(&type_hash, sizeof(type_hash));
    // ...写入具体属性
}
```

---

## 限制与注意事项

### DLL 边界问题

**问题**：跨 DLL 边界比较 TypeId 可能不可靠。

**原因**：
- 不同 DLL 中 `kTypeAnchor<T>` 有不同地址
- 若类型 `T` 仅在一个 DLL 中定义，则比较有效
- 若两个 DLL 各自实例化 `T`，则 TypeId 不同

**解决方案**：
- 对于公共接口类型（如 `IDevice`），确保仅在一个 DLL 中实例化模板
- 或使用 `mine.reflect` 模块通过类型名字符串注册跨 DLL 映射

### 不可序列化

**约束**：TypeId 的 `hash()` 值不可持久化到磁盘或跨进程传递。

**原因**：
- 地址值在不同程序运行、不同编译产物中会变化
- ASLR（地址空间布局随机化）导致每次运行地址不同

**替代方案**：
- 使用 `mine.reflect::TypeRegistry` 注册类型名字符串
- 序列化时保存类型名，反序列化时查表获取 TypeId

---

## 性能特征

| 操作 | 时间复杂度 | 备注 |
|------|------------|------|
| `TypeId::of<T>()` | O(1) | 编译期常量 |
| `valid()` | O(1) | 空指针判断 |
| `hash()` | O(1) | 返回指针值 |
| `operator==` | O(1) | 指针比较 |

**内存占用**：
- 每个 TypeId 对象：`sizeof(void*) = 8` 字节（64 位）
- 每个类型的锚点变量：1 字节（编译器可能优化为 0 空间）

---

## 相关类型

- `mine::core::Variant`：使用 TypeId 标识存储的值类型
- `mine::ui::DependencyProperty`：使用 TypeId 标识属性所属类型
- `mine::reflect::TypeInfo`：扩展 TypeId 为带元数据的类型信息
- `mine::ui::RoutedEvent`：事件的拥有者类型标识

---

## 线程安全

**完全线程安全**：
- TypeId 对象为不可变（immutable）
- `of<T>()` 返回编译期常量
- 所有操作为纯函数（无全局状态修改）

---

## 示例：完整用法

```cpp
#include <mine/core/TypeId.h>
#include <mine/ui/Button.h>
#include <mine/ui/TextBlock.h>
#include <cassert>
#include <unordered_map>

using namespace mine;

void example_basic_usage() {
    // 获取类型标识
    TypeId btn_id  = core::TypeId::of<ui::Button>();
    TypeId text_id = core::TypeId::of<ui::TextBlock>();
    
    // 类型比较
    assert(btn_id != text_id);
    assert(btn_id == core::TypeId::of<ui::Button>());
    
    // cv 限定符不影响
    assert(btn_id == core::TypeId::of<const ui::Button>());
}

void example_hashmap() {
    // 作为哈希表键
    std::unordered_map<uintptr_t, const char*> names;
    
    names[core::TypeId::of<ui::Button>().hash()]    = "Button";
    names[core::TypeId::of<ui::TextBlock>().hash()] = "TextBlock";
    
    // 查询
    auto it = names.find(core::TypeId::of<ui::Button>().hash());
    assert(it != names.end());
    assert(std::string{it->second} == "Button");
}

void example_type_check(ui::UIElement* element) {
    // 向下转型前验证类型
    if (element->type_id() == core::TypeId::of<ui::Button>()) {
        auto* btn = static_cast<ui::Button*>(element);
        btn->set_content("确定");
    } else if (element->type_id() == core::TypeId::of<ui::TextBlock>()) {
        auto* text = static_cast<ui::TextBlock*>(element);
        text->set_text("Hello");
    }
}

int main() {
    example_basic_usage();
    example_hashmap();
    return 0;
}
```

---

## 最佳实践

### ✅ 推荐用法

```cpp
// 1. 用于类型判断
if (obj->type_id() == TypeId::of<Button>()) { ... }

// 2. 作为哈希表键
HashMap<TypeId, Handler> handlers;

// 3. 记录类型元数据
struct PropertyInfo {
    TypeId owner_type;
    TypeId value_type;
};
```

### ❌ 避免的用法

```cpp
// ❌ 不要序列化 hash() 值
file.write(&type_id.hash(), 8);  // 下次运行地址变化

// ❌ 不要跨 DLL 比较未注册类型
if (dll_a_type_id == dll_b_type_id) { ... }  // 可能错误

// ❌ 不要用于运行时类型信息
const char* name = type_id.name();  // TypeId 不提供此接口
```

---

## 调试技巧

### 查看 TypeId 地址

```cpp
TypeId id = TypeId::of<Button>();
printf("TypeId hash: 0x%zx\n", id.hash());
// 输出：TypeId hash: 0x7ff6abc12340
```

### 断点条件

```cpp
// Visual Studio / GDB 断点条件
element->type_id().hash() == 0x7ff6abc12340
```

### 类型名查询（需 mine.reflect）

```cpp
#include <mine/reflect/TypeRegistry.h>

const char* name = reflect::TypeRegistry::name(id);
// 输出："mine::ui::Button"
```
