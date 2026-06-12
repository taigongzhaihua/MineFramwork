# 反射注册宏详细文档

## 概述

**模块**：`mine.reflect`  
**头文件**：`<mine/reflect/ReflectMacros.h>`（含在 `<mine/reflect/Reflect.h>` 中）

`mine.reflect` 模块通过以下三个核心宏实现声明式编译期类型注册：

| 宏 | 使用位置 | 作用 |
|----|----------|------|
| `MINE_REFLECT_DECL()` | 头文件（类定义内） | 声明反射接口 |
| `MINE_REFLECT_IMPL(TYPE, BASE, ...)` | cpp 文件 | 注册类型元数据 |
| `MINE_PROP_ENTRY(OWNER, NAME, TYPE, GET, SET)` | `MINE_REFLECT_IMPL` 内 | 定义单个属性条目 |

---

## MINE_REFLECT_DECL()

### 声明

**签名**：
```cpp
#define MINE_REFLECT_DECL() \
    static const ::mine::reflect::TypeInfo* static_type_info(); \
    virtual const ::mine::reflect::TypeInfo* _get_reflect_type_info() const { return static_type_info(); }
```

### 描述

在类型定义中声明反射支持。展开后为类型添加两个方法：

- `static_type_info()`：静态方法，返回类型的 TypeInfo 指针。可用于编译期静态已知的类型访问。
- `_get_reflect_type_info()`：**虚**方法，返回运行时类型的 TypeInfo 指针。通过虚函数分派，可在不知道具体类型（仅持有基类指针）时获取正确的类型信息。

### 使用位置

类定义的 `public` 区域（对于 `struct`，默认即为 public）。

### 示例

```cpp
// ── 头文件：Animal.h ────────────────────────────────────────────
#pragma once
#include <mine/reflect/ReflectMacros.h>

struct Animal {
    MINE_REFLECT_DECL();  // 声明反射支持
    virtual ~Animal() = default;

    const char* name() const;
    void set_name(const char* n);
};

// ── 编译后等价于 ────────────────────────────────────────────
struct Animal {
    static const mine::reflect::TypeInfo* static_type_info();
    virtual const mine::reflect::TypeInfo* _get_reflect_type_info() const {
        return static_type_info();
    }
    virtual ~Animal() = default;
    const char* name() const;
    void set_name(const char* n);
};
```

### 注意事项

1. **必须放在类定义的 public 区域**（或 struct 的默认 public 区域）：
   ```cpp
   // ✅ struct：默认 public
   struct MyType { MINE_REFLECT_DECL(); /* ... */ };

   // ✅ class：显式 public
   class MyType { public: MINE_REFLECT_DECL(); /* ... */ };

   // ❌ 错误：class 默认 private
   class MyType { MINE_REFLECT_DECL(); };  // 外部无法调用 static_type_info()
   ```

2. **`_get_reflect_type_info()` 是虚函数**：派生类会自动继承此方法，但返回的是自身的 `static_type_info()`（因为 `MINE_REFLECT_DECL()` 在每个派生类中重新展开）。

3. **析构函数应声明为 virtual**：对于将 `_get_reflect_type_info()` 用于多态的类型，建议声明虚析构函数以确保安全。

### 行为细节

**虚函数分派行为**：

```cpp
struct Animal {
    MINE_REFLECT_DECL();  // _get_reflect_type_info 返回 Animal::static_type_info()
};

struct Dog : Animal {
    MINE_REFLECT_DECL();  // _get_reflect_type_info 返回 Dog::static_type_info()
};

// 测试
Dog dog;
Animal* ptr = &dog;
ptr->_get_reflect_type_info();  // → Dog::static_type_info()（虚函数分派到 Dog）
```

---

## MINE_REFLECT_IMPL(TYPE, BASE, ...)

### 声明

**签名**：
```cpp
#define MINE_REFLECT_IMPL(TYPE, BASE, ...)
```

### 描述

在 cpp 实现文件中注册类型的反射元数据。展开后生成：
1. 属性元数据静态数组（包含哑元条目避免零大小数组）
2. 静态 `TypeInfo` 实例（包含名称、TypeId、基类、属性视图）
3. 匿名命名空间中的自动注册对象（在静态初始化阶段向 `TypeRegistry` 注册）
4. `TYPE::static_type_info()` 方法实现

### 参数

| 参数 | 类型 | 说明 |
|------|------|------|
| `TYPE` | 类型名 | 完整类型名（允许包含 `::` 命名空间限定符，如 `test::Animal`）。此值同时用于 `static_type_info()` 的类作用域解析。 |
| `BASE` | 类型名 或 `void` | 直接基类类型名。若类型无基类，必须传入 `void`。用于建立继承链和基类 TypeInfo 指针。 |
| `...` | 零个或多个 `MINE_PROP_ENTRY()` | 逗号分隔的属性条目宏调用列表。可为空（仅注册类型标识）。 |

### 要求

**每个 `MINE_REFLECT_IMPL` 宏调用必须独占一行**。宏内部使用 `__LINE__` 生成唯一的符号名，若两个 `MINE_REFLECT_IMPL` 在同一行（如通过其他宏间接展开在同一行），会导致符号名冲突。

### 示例

```cpp
// ── 实现文件：Animal.cpp ────────────────────────────────────────
#include "Animal.h"
#include <mine/reflect/Reflect.h>

MINE_REFLECT_IMPL(Animal, void,
    MINE_PROP_ENTRY(Animal, name, const char*, name, set_name)
);

// ── 带多个属性的注册 ────────────────────────────────────────
MINE_REFLECT_IMPL(Button, Control,
    MINE_PROP_ENTRY(Button, text,    StringView, text,    set_text),
    MINE_PROP_ENTRY(Button, enabled, bool,       enabled, set_enabled),
    MINE_PROP_ENTRY(Button, visible, bool,       visible, set_visible)
);

// ── 仅注册类型标识（无属性） ─────────────────────────────────
MINE_REFLECT_IMPL(EmptyComponent, void);
```

### 展开后生成的代码结构

以 `MINE_REFLECT_IMPL(Animal, void, MINE_PROP_ENTRY(...))` 为例，在行号 53 处展开后等价于：

```cpp
// 属性元数据数组（哑元在前，避免零大小数组）
static const PropertyInfo _s_rf_props_53[] = {
    PropertyInfo{ /* 哑元 */ },
    PropertyInfo{ StringView{"name"}, TypeId::of<const char*>(), nullptr,
        [](const void* obj) -> Variant { /* getter */ },
        [](void* obj, const Variant& v) { /* setter */ }
    }
};

// 属性计数（排除哑元）
static constexpr size_t _s_rf_nprops_53 =
    (sizeof(_s_rf_props_53) / sizeof(PropertyInfo)) - 1;  // = 1

// 静态 TypeInfo
static const TypeInfo _s_rf_type_53 = {
    StringView{"Animal"},
    TypeId::of<Animal>(),
    detail::resolve_base_type_info<void>(),  // → nullptr
    Span<const PropertyInfo>(_s_rf_props_53 + 1, _s_rf_nprops_53),
    Span<const MethodInfo>{},
    nullptr
};

// 自动注册
namespace {
    struct _s_rf_auto_reg_53 {
        _s_rf_auto_reg_53() noexcept {
            TypeRegistry::instance().register_type(&_s_rf_type_53);
        }
    };
    static const _s_rf_auto_reg_53 _s_rf_auto_inst_53;
}

// static_type_info() 实现
const TypeInfo* Animal::static_type_info() {
    return &_s_rf_type_53;
}
```

### 内部机制

#### 哑元条目

当 `__VA_ARGS__` 为空时（如 `MINE_REFLECT_IMPL(EmptyComponent, void)`），C++ 标准不允许零大小数组。宏在属性数组开头插入一个哑元 `PropertyInfo`，计数时减 1，Span 跳过哑元条目。对外表现与零属性列表完全一致。

#### BASE 解析

使用 `mine::reflect::detail::resolve_base_type_info<BASE>()` 模板函数处理基类指针：
```cpp
template<typename Base>
inline const TypeInfo* resolve_base_type_info() {
    if constexpr (!std::is_same_v<Base, void>) {
        return Base::static_type_info();
    } else {
        return nullptr;
    }
}
```

#### 符号唯一性

所有内部符号名通过 `MINE_REFL_ID(NAME)` 拼接 `__LINE__` 生成（如 `_s_rf_props_53`）。这确保：
- 同一翻译单元内多次 `MINE_REFLECT_IMPL` 调用不冲突（行号不同）
- 不同翻译单元间无需避免冲突（static + 匿名命名空间限定作用域）

### 注意事项

1. **BASE 为 `void` 而非省略**：即使类型无基类，也必须显式传入 `void`。

2. **属性顺序**：属性在 `properties` Span 中的顺序与 `MINE_REFLECT_IMPL` 中 `MINE_PROP_ENTRY` 的书写顺序一致。

3. **`__VA_ARGS__` 中逗号安全**：当 `__VA_ARGS__` 为非空时，宏靠 `,` 与哑元条目分隔；当为空时，数组只有哑元一项。两种情况均合法。

---

## MINE_PROP_ENTRY(OWNER, PROP_NAME, VALUE_TYPE, GETTER_FN, SETTER_FN)

### 声明

**签名**：
```cpp
#define MINE_PROP_ENTRY(OWNER, PROP_NAME, VALUE_TYPE, GETTER_FN, SETTER_FN)
```

### 描述

生成一条属性元数据条目（`PropertyInfo` 聚合初始化表达式）。作为 `MINE_REFLECT_IMPL` 的参数数组元素使用，不单独调用。

### 参数

| 参数 | 类型 | 说明 |
|------|------|------|
| `OWNER` | 类型名 | 拥有该属性的类型 |
| `PROP_NAME` | 标识符 | 属性名称（不带引号，宏内部用 `#PROP_NAME` 字符串化） |
| `VALUE_TYPE` | 类型名 | 属性值类型（如 `int`、`const char*`、`StringView`） |
| `GETTER_FN` | 标识符 | 获取属性值的 const 成员函数名（如 `name`、`text`） |
| `SETTER_FN` | 标识符 | 设置属性值的成员函数名（如 `set_name`、`set_text`） |

### 展开后等价于

```cpp
PropertyInfo{
    StringView{"NAME"},                   // name
    TypeId::of<VALUE_TYPE>(),            // type
    nullptr,                              // owner（预留，当前未填充）
    /* getter */ [](const void* obj) -> Variant {
        auto* self = static_cast<const OWNER*>(obj);
        return Variant{self->GETTER_FN()};
    },
    /* setter */ [](void* obj, const Variant& v) {
        auto* self = static_cast<OWNER*>(obj);
        self->SETTER_FN(any_cast<VALUE_TYPE>(v));
    }
}
```

### 示例

```cpp
struct Person {
    const char* name() const;
    void set_name(const char* n);
    int age() const;
    void set_age(int a);
    bool married() const;
    // 注意：married 没有 public setter
};

MINE_REFLECT_IMPL(Person, void,
    MINE_PROP_ENTRY(Person, name,    const char*, name,    set_name),
    MINE_PROP_ENTRY(Person, age,     int,         age,     set_age),
    MINE_PROP_ENTRY(Person, married, bool,        married, married)  // 无专用 setter
);
```

### VALUE_TYPE 必须精确匹配

`VALUE_TYPE` 声明必须与 getter 返回类型和 setter 参数类型完全一致（包括 cv 限定符）：

```cpp
// ✅ 正确：const char* 匹配
const char* name() const;
void set_name(const char* n);
MINE_PROP_ENTRY(Person, name, const char*, name, set_name);

// ❌ 错误：类型不匹配
StringView name() const;
void set_name(StringView n);
MINE_PROP_ENTRY(Person, name, const char*, name, set_name);  // 类型不匹配！
```

---

## 完整工作流

### 步骤 1：头文件中声明反射

```cpp
// ── MyControl.h ─────────────────────────────────────────────────
#pragma once
#include <mine/reflect/ReflectMacros.h>

struct MyControl {
    MINE_REFLECT_DECL();

    const char* title() const;
    void set_title(const char* t);

    int width() const;
    void set_width(int w);

    bool visible() const;
    void set_visible(bool v);
};
```

### 步骤 2：cpp 文件中注册元数据

```cpp
// ── MyControl.cpp ───────────────────────────────────────────────
#include "MyControl.h"
#include <mine/reflect/Reflect.h>

MINE_REFLECT_IMPL(MyControl, void,
    MINE_PROP_ENTRY(MyControl, title,   const char*, title,   set_title),
    MINE_PROP_ENTRY(MyControl, width,   int,         width,   set_width),
    MINE_PROP_ENTRY(MyControl, visible, bool,        visible, set_visible)
);
```

### 步骤 3：使用反射

```cpp
// ── 任意位置使用 ──────────────────────────────────────────────

void demo() {
    MyControl ctrl;
    ctrl.set_title("Hello");
    ctrl.set_width(300);

    // 获取类型信息
    const TypeInfo* info = MyControl::static_type_info();

    // 反射读写属性
    const PropertyInfo* tp = info->find_property("title");
    Variant v = tp->getter(&ctrl);
    printf("title = %s\n", v.get<const char*>());

    tp->setter(&ctrl, Variant{"World"});
    printf("title = %s\n", ctrl.title());  // "World"

    // 通过 TypeRegistry 查找
    const TypeInfo* found = TypeRegistry::instance().find_by_name("MyControl");
    CHECK(found == info);
}
```

---

## 与 MML 预编译器的关系

`mmlc` 预编译器在处理 `.mml` 文件中的组件定义时，自动生成对应的 `MINE_REFLECT_DECL()` 和 `MINE_REFLECT_IMPL(...)` 调用。纯 C++ 手写代码也可以直接使用这些宏，无需依赖 mmlc。

```mml
<!-- MML 输入 -->
component LoginView : UserControl {
    property title: String;
    property busy: bool;
}
```

```cpp
// mmlc 生成的 C++ 等价代码
class LoginView : public UserControl {
    MINE_REFLECT_DECL();
    // ...
};

MINE_REFLECT_IMPL(LoginView, UserControl,
    MINE_PROP_ENTRY(LoginView, title, String, title, set_title),
    MINE_PROP_ENTRY(LoginView, busy,  bool,   busy,  set_busy)
);
```

---

## 性能特征

| 宏 | 编译期开销 | 运行时开销 |
|----|-----------|-----------|
| `MINE_REFLECT_DECL()` | 两个方法声明 | 无 |
| `MINE_REFLECT_IMPL(...)` | 静态数组 + TypeInfo 聚合初始化 | 单次 `register_type()` 调用（静态初始化） |
| `MINE_PROP_ENTRY(...)` | 两个 Lambda 生成 | 函数指针间接调用 |

**总体评估**：
- 编译期：每个类型增加少量模板实例化（可忽略）
- 运行时：零额外开销（静态初始化完成后，所有操作均为 O(1) 指针间接调用）
- 二进制体积：每个属性条目约增加 100-200 字节（代码段 + 数据段）

---

## 限制与注意事项

### 宏必须独占一行

```cpp
// ✅ 正确
MINE_REFLECT_IMPL(Animal, void,
    MINE_PROP_ENTRY(Animal, name, const char*, name, set_name)
);

// ❌ 错误：与其他代码同行
int x = 0; MINE_REFLECT_IMPL(Animal, void, MINE_PROP_ENTRY(...));
// 会导致 __LINE__ 冲突
```

### 不支持嵌套的宏展开

不要在另一个宏内部调用 `MINE_REFLECT_IMPL`，因为 `__LINE__` 将展开为外层宏的行号，可能导致符号冲突。

### 属性名不可包含空格或特殊字符

`PROP_NAME` 参数直接作为 C++ 标识符和字符串字面量使用。

### VALUE_TYPE 不可包含逗号

由于 `MINE_PROP_ENTRY` 使用逗号分隔参数，`VALUE_TYPE` 不可为包含逗号的模板类型（如 `HashMap<int, float>`）。对于复杂类型，使用 `using` 别名：

```cpp
using ScoreMap = HashMap<int, float>;
MINE_PROP_ENTRY(Player, scores, ScoreMap, scores, set_scores);
```

---

## 相关类型

- `mine::reflect::TypeInfo`：类型元数据描述符
- `mine::reflect::PropertyInfo`：属性元数据描述符
- `mine::reflect::TypeRegistry`：全局类型注册表
- `mine::core::Variant`：类型擦除值容器
- `mine::core::TypeId`：轻量类型标识符
