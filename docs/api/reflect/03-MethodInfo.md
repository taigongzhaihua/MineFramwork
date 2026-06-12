# MethodInfo 结构详细接口文档

## 概述

**模块**：`mine.reflect`  
**头文件**：`<mine/reflect/MethodInfo.h>`  
**命名空间**：`mine::reflect`

**用途**：类型方法的运行期元数据描述符。每个已注册的方法对应一个 `MethodInfo` 实例，通过擦除类型的函数指针（`void*` + `Span<Variant>`）实现反射调用。

**核心特性**：
- 存储方法的名称、返回类型、参数类型列表和调用函数指针等元信息
- invoke 通过 `void*` + `Span<const Variant>` 的擦除类型形式实现，支持可变参数调用
- 支持运行期通过名称查找方法并进行调用
- 纯编译期静态数据，不拥有任何堆分配内存

**当前状态**：方法注册功能预留。`TypeInfo::methods` 在当前版本中始终为空 Span，`MINE_METHOD_ENTRY` 宏已定义但尚未集成到 `MINE_REFLECT_IMPL` 中。本文档描述完整的接口设计，供后续实现参考。

---

## 结构定义

```cpp
namespace mine::reflect {

struct MethodInfo {
    // ── 元数据 ────────────────────────────────────────────────────────────
    mine::core::StringView           name;
    mine::core::TypeId               return_type;
    const mine::core::TypeId*        param_types;
    uint32_t                         param_count;
    const TypeInfo*                  owner;

    // ── 调用 ──────────────────────────────────────────────────────────────
    mine::core::Variant (*invoke)(void* obj,
                                  mine::core::Span<const mine::core::Variant> args);
};

} // namespace mine::reflect
```

---

## 数据成员

### `name`

**类型**：`mine::core::StringView`

**描述**：方法名称（编译期常量字符串，无需释放）。

**示例值**：`"set_text"`、`"navigate_to"`、`"on_clicked"`

---

### `return_type`

**类型**：`mine::core::TypeId`

**描述**：方法返回值的 TypeId。

- 对于返回 `void` 的方法，`return_type` 为 `TypeId::of<void>()`
- 调用后可通过 `Variant::has<T>()` 检查返回类型

---

### `param_types`

**类型**：`const mine::core::TypeId*`

**描述**：参数类型列表的 TypeId 数组指针。

- 数组长度为 `param_count`
- 元素依次对应第 1..N 个参数的类型
- 若方法无参数（`param_count == 0`），此指针为 `nullptr`

**用途**：调用前检查参数类型是否匹配。

---

### `param_count`

**类型**：`uint32_t`

**描述**：参数个数。0 表示无参方法。

---

### `owner`

**类型**：`const TypeInfo*`

**描述**：所属类型的 TypeInfo 指针（预留字段，当前恒为 `nullptr`）。

---

### `invoke`

**类型**：`mine::core::Variant (*)(void* obj, Span<const Variant> args)`

**描述**：调用此方法的函数指针。

**参数**：
- `obj`：指向类型实例的 `void*` 指针。对于静态方法可为 `nullptr`。
- `args`：参数列表 Variant 视图，长度应与 `param_count` 一致。各参数的 `type_id()` 应与 `param_types` 对应项匹配。

**返回值**：
- 方法返回值的 Variant
- 若返回类型为 `void`，返回空 Variant（`!v.has_value()`）

**前置条件**：
- `args.size() == param_count`（参数个数不匹配会触发断言）
- 对于非静态方法，`obj` 的实际类型与 `owner` 一致
- `args[i].type_id() == param_types[i]`（类型不匹配会触发断言）

---

## 使用场景（预留设计）

### 1. 通用命令调用

```cpp
Result<void> invoke_command(const TypeInfo* type, void* obj,
                            StringView method_name,
                            Span<const Variant> args) {
    const MethodInfo* method = type->find_method(method_name);
    if (!method) {
        return err_tag, Status::not_found("方法不存在");
    }
    if (method->param_count != args.size()) {
        return err_tag, Status::invalid_argument("参数个数不匹配");
    }

    method->invoke(obj, args);
    return ok_tag;
}
```

### 2. 事件处理分发

```cpp
void dispatch_event(const TypeInfo* type, void* obj,
                    StringView event_name, const Variant& event_args) {
    // 查找事件处理方法（如 OnClick、OnTextChanged）
    String handler_name = "on_" + event_name.to_string();
    const MethodInfo* handler = type->find_method(handler_name);
    if (handler && handler->param_count == 1) {
        Variant args[] = { event_args };
        handler->invoke(obj, Span<const Variant>(args, 1));
    }
}
```

### 3. RPC 调用桥接

```cpp
Variant handle_rpc_call(StringView type_name, StringView method_name,
                        Span<const Variant> args) {
    const TypeInfo* type = TypeRegistry::instance().find_by_name(type_name);
    if (!type) return Variant{};

    const MethodInfo* method = type->find_method(method_name);
    if (!method || method->param_count != args.size()) return Variant{};

    // 通过工厂创建实例 → 调用方法 → 销毁实例
    void* instance = type->factory ? type->factory() : nullptr;
    if (!instance) return Variant{};

    Variant result = method->invoke(instance, args);
    delete instance;  // 简化示例
    return result;
}
```

---

## 性能特征

| 操作 | 时间复杂度 | 备注 |
|------|------------|------|
| 获取 MethodInfo 指针 | O(n) | TypeInfo::find_method() 线性搜索 |
| 参数个数检查 | O(1) | 直接读取 param_count |
| 方法调用 | O(1) | 函数指针调用 + 实际业务逻辑 |

---

## 线程安全

**完全线程安全**：
- MethodInfo 实例在静态初始化阶段构造完毕后即为不可变（immutable）
- invoke 函数指针指向的 Lambda 为纯函数（线程安全性取决于被调用的实际方法）
- 对同一对象的并发调用需由调用方或目标方法自行同步

---

## 限制与注意事项

### 当前版本未实际启用

`TypeInfo::methods` 在当前版本中始终为空 Span。`MINE_METHOD_ENTRY` 宏已定义但未被 `MINE_REFLECT_IMPL` 集成。以下代码当前无效：

```cpp
// ❌ 当前不支持
MINE_REFLECT_IMPL(MyType, BaseType,
    MINE_METHOD_ENTRY(MyType, do_something, void, int)
);
```

### 参数类型严格匹配

invoke 内部通过 `any_cast<T>()` 进行类型检查。参数 Variant 的实际类型必须与 `param_types` 声明完全一致，否则触发断言。

### 不支持重载

当前设计不支持方法重载（同一名称不同参数）。`find_method` 按名称匹配，遇到重载方法时返回第一个匹配项。

**后续改进**：可通过参数个数和类型列表进行二次过滤。

### owner 字段未填充

与 PropertyInfo 类似，`owner` 字段当前固定为 `nullptr`。

---

## 相关类型

- `mine::reflect::TypeInfo`：类型元数据（包含 methods 列表）
- `mine::reflect::PropertyInfo`：属性元数据描述符
- `mine::reflect::TypeRegistry`：全局类型注册表
- `mine::core::TypeId`：轻量类型标识符
- `mine::core::Variant`：类型擦除值容器（返回值/参数）
- `mine::core::Span<T>`：非拥有连续内存视图（参数列表）

---

## 示例：完整用法（设计预览）

```cpp
#include <mine/reflect/Reflect.h>

// ── 类型定义 ────────────────────────────────────────────────────

struct Calculator {
    MINE_REFLECT_DECL();

    int add(int a, int b) const { return a + b; }
    int multiply(int a, int b) const { return a * b; }
};

// ── 注册反射（后续版本支持）────────────────────────────────────
// MINE_REFLECT_IMPL(Calculator, void,
//     MINE_METHOD_ENTRY(Calculator, add,       int, int, int),
//     MINE_METHOD_ENTRY(Calculator, multiply,  int, int, int)
// );

// ── 使用反射调用方法（预览）────────────────────────────────────

void demo_call_method() {
    Calculator calc;

    const TypeInfo* info = Calculator::static_type_info();

    // 查找方法
    const MethodInfo* method = info->find_method("add");
    if (!method) return;  // 当前版本返回 nullptr

    // 准备参数
    Variant args[] = { Variant{3}, Variant{5} };

    // 调用
    Variant result = method->invoke(&calc, Span<const Variant>(args, 2));
    if (result.has<int>()) {
        printf("3 + 5 = %d\n", result.get<int>());  // 8
    }
}
```
