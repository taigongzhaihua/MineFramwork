# PropertyInfo 结构详细接口文档

## 概述

**模块**：`mine.reflect`  
**头文件**：`<mine/reflect/PropertyInfo.h>`  
**命名空间**：`mine::reflect`

**用途**：类型属性的运行期元数据描述符。每个已注册的属性对应一个 `PropertyInfo` 实例，通过擦除类型的函数指针（`void*` + `Variant`）实现反射读写。

**核心特性**：
- 存储属性的名称、值类型、getter/setter 函数指针等元信息
- getter/setter 通过 `void*` + `Variant` 的擦除类型形式实现，避免模板膨胀
- 支持运行期通过名称查找属性并进行读写操作
- 纯编译期静态数据，不拥有任何堆分配内存
- 通过 `MINE_PROP_ENTRY` 宏自动生成，无需手动构造

**设计动机**：

在无 RTTI 的 C++ 环境中，要实现运行期属性访问（如序列化、属性面板、数据绑定），需要一种类型擦除的属性描述机制。`PropertyInfo` 通过函数指针 + `Variant` 的方式：
- **消除模板依赖**：调用方无需知道具体类型即可读写属性
- **类型安全**：Variant 内部保存 TypeId，setter 中 `any_cast<T>()` 进行编译期已确定的类型断言
- **零开销**：getter/setter 函数指针指向的 Lambda 直接调用成员函数，无虚函数开销

---

## 结构定义

```cpp
namespace mine::reflect {

struct PropertyInfo {
    // ── 元数据 ────────────────────────────────────────────────────────────
    mine::core::StringView  name;
    mine::core::TypeId      type;
    const TypeInfo*         owner;

    // ── 访问器 ────────────────────────────────────────────────────────────
    mine::core::Variant (*getter)(const void* obj);
    void                (*setter)(void* obj, const mine::core::Variant& value);
};

} // namespace mine::reflect
```

---

## 数据成员

### `name`

**类型**：`mine::core::StringView`

**描述**：属性名称（编译期常量字符串，无需释放）。

**来源**：由 `MINE_PROP_ENTRY` 宏中的 `#PROP_NAME` 字符串化操作生成。

**示例值**：`"text"`、`"enabled"`、`"breed"`

**约束**：
- 指向编译期字符串字面量（静态存储期），永不失效
- 类型内属性名称应唯一

---

### `type`

**类型**：`mine::core::TypeId`

**描述**：属性值类型的 TypeId。

**来源**：由 `MINE_PROP_ENTRY` 宏中显式指定的 `VALUE_TYPE` 参数生成。

**用途**：
- 类型安全检查：`prop->type == TypeId::of<int>()`
- 反序列化时根据目标类型选择正确的解析方式
- 属性面板中根据类型选择合适的编辑控件

**示例**：
```cpp
const PropertyInfo* prop = info->find_property("age");
if (prop->type == TypeId::of<int>()) {
    int age = prop->getter(obj).get<int>();
}
```

---

### `owner`

**类型**：`const TypeInfo*`

**描述**：所属类型的 TypeInfo 指针，指向拥有该属性的类型的 TypeInfo 实例。

**来源**：当前版本由 `MINE_PROP_ENTRY` 宏设置为 `nullptr`（预留字段）。

**用途**（预留）：
- 从属性反向查找所属类型
- 属性面板中显示 "类型名.属性名" 格式

**约束**：
- 当前恒为 `nullptr`，后续版本将在 `MINE_REFLECT_IMPL` 展开时自动填充

---

### `getter`

**类型**：`mine::core::Variant (*)(const void* obj)`

**描述**：从对象指针读取属性值的函数指针。

**签名**：
```cpp
Variant getter(const void* obj);
```

**参数**：
- `obj`：指向类型实例的 `const void*` 指针。调用方负责保证指针有效且类型匹配。

**返回值**：
- 属性当前值的 Variant 副本。通过 `Variant{...}` 包装原始返回值。

**前置条件**：
- `obj != nullptr` 且 `obj` 的实际类型与 `owner` 一致
- 若 `getter == nullptr`，表示该属性不可读

**示例**：
```cpp
// 内部实现（由 MINE_PROP_ENTRY 宏生成）
[](const void* obj) -> Variant {
    auto* self = static_cast<const Animal*>(obj);
    return Variant{self->name()};  // 调用实际 getter
}

// 使用
Animal a;
a.set_name("test");
const PropertyInfo* prop = info->find_property("name");
Variant v = prop->getter(&a);  // v = Variant{"test"}
```

---

### `setter`

**类型**：`void (*)(void* obj, const mine::core::Variant& value)`

**描述**：向对象指针写入属性值的函数指针。

**签名**：
```cpp
void setter(void* obj, const Variant& value);
```

**参数**：
- `obj`：指向类型实例的 `void*` 指针（非 const）
- `value`：待写入的新值，其 `type_id()` 应与 `PropertyInfo::type` 匹配

**前置条件**：
- `obj != nullptr` 且 `obj` 的实际类型与 `owner` 一致
- `value.type_id() == type`（类型不匹配会触发 `MINE_ASSERT` 断言）
- 若 `setter == nullptr`，表示该属性不可写（只读）

**示例**：
```cpp
// 内部实现（由 MINE_PROP_ENTRY 宏生成）
[](void* obj, const Variant& v) {
    auto* self = static_cast<Animal*>(obj);
    self->set_name(any_cast<const char*>(v));  // 类型安全断言
}

// 使用
const PropertyInfo* prop = info->find_property("name");
prop->setter(&a, Variant{"新名称"});  // 写入成功
prop->setter(&a, Variant{42});        // 断言失败：类型不匹配
```

---

## 获取 PropertyInfo

### 通过 TypeInfo 查找

```cpp
const TypeInfo* info = MyType::static_type_info();
const PropertyInfo* prop = info->find_property("my_field");
```

### 通过索引访问

```cpp
for (size_t i = 0; i < info->properties.size(); ++i) {
    const PropertyInfo& prop = info->properties[i];
}
```

### 通过范围 for 遍历

```cpp
for (auto& prop : info->properties) {
    printf("  %s : %s\n", prop.name.data(),
           type_name_by_id(prop.type).data());
}
```

---

## 使用场景

### 1. 通用序列化器

```cpp
void serialize_object(const TypeInfo* type, void* obj, JsonWriter& out) {
    out.begin_object();
    for (auto& prop : type->properties) {
        if (!prop.getter) continue;  // 跳过不可读属性

        Variant val = prop.getter(obj);
        out.key(prop.name);
        write_variant(out, val, prop.type);
    }

    // 递归处理基类属性
    if (type->base) {
        serialize_object(type->base, obj, out);
    }
    out.end_object();
}
```

### 2. 通用反序列化器

```cpp
Result<void> deserialize_object(const TypeInfo* type, void* obj, JsonReader& in) {
    MINE_TRY(in.expect_object_begin());

    while (in.has_next_key()) {
        StringView key = in.next_key();
        const PropertyInfo* prop = type->find_property(key);
        if (prop && prop->setter) {
            Variant val = MINE_TRY(read_variant(in, prop->type));
            prop->setter(obj, val);
        } else if (type->base) {
            // 不在当前类型，尝试基类
            deserialize_object(type->base, obj, in);
        } else {
            in.skip_value();
        }
    }

    return ok_tag;
}
```

### 3. 属性编辑器生成

```cpp
void build_property_editor(const TypeInfo* type, void* obj, Container* parent) {
    for (auto& prop : type->properties) {
        if (!prop.getter || !prop.setter) continue;

        // 根据类型选择编辑器控件
        UIElement* editor = nullptr;
        if (prop.type == TypeId::of<int>()) {
            editor = new NumberEditor(prop.getter(obj).get<int>(),
                [prop, obj](int v) { prop->setter(obj, Variant{v}); });
        } else if (prop.type == TypeId::of<bool>()) {
            editor = new CheckBoxEditor(prop.getter(obj).get<bool>(),
                [prop, obj](bool v) { prop->setter(obj, Variant{v}); });
        }

        if (editor) {
            add_row(prop.name, editor, parent);
        }
    }
}
```

### 4. 对象拷贝（深度克隆）

```cpp
void copy_properties(const TypeInfo* type, void* src, void* dst) {
    for (auto& prop : type->properties) {
        if (prop.getter && prop.setter) {
            Variant val = prop.getter(src);
            prop.setter(dst, val);
        }
    }
    if (type->base) {
        copy_properties(type->base, src, dst);
    }
}
```

### 5. 运行期属性绑定

```cpp
void bind_property(void* source, const PropertyInfo* src_prop,
                   void* target, const PropertyInfo* tgt_prop) {
    // 建立单向绑定：源属性变更 → 同步到目标属性
    Variant val = src_prop->getter(source);
    tgt_prop->setter(target, val);
}
```

---

## 性能特征

| 操作 | 时间复杂度 | 备注 |
|------|------------|------|
| 读取属性值 | O(1) | 函数指针调用 + Variant 构造 |
| 写入属性值 | O(1) | 函数指针调用 + any_cast 断言 |
| 获取 name/type | O(1) | 直接成员访问 |
| 属性比较 | O(1) | 指针/TypeId 比较 |

**内存占用**：
- PropertyInfo 实例：约 56 字节（64 位平台）
- 全部存储在编译期静态数组中，无运行时堆分配
- getter/setter Lambda 为编译期生成的独立函数（代码段）

---

## 线程安全

**完全线程安全**：
- PropertyInfo 实例在静态初始化阶段构造完毕后即为不可变（immutable）
- 所有成员访问为纯只读
- getter/setter 函数指针指向的 Lambda 为纯函数（无共享状态）
- 并发读写同一对象的同一属性需由调用方同步（与 PropertyInfo 无关）

---

## 限制与注意事项

### getter/setter 可为空

```cpp
const PropertyInfo* prop = info->find_property("name");
if (prop->getter) {
    Variant v = prop->getter(obj);  // 安全
}
if (prop->setter) {
    prop->setter(obj, new_value);  // 安全
}
```

始终检查 getter/setter 是否为空，避免对只读/只写属性的错误访问。

### 类型不匹配触发断言

setter 内部通过 `any_cast<VALUE_TYPE>(v)` 进行类型检查。若 Variant 持有的实际类型与声明的 VALUE_TYPE 不匹配，将触发 `MINE_ASSERT` 断言终止（非异常）。

```cpp
// prop->type == TypeId::of<int>()
prop->setter(obj, Variant{"hello"});  // 断言失败！
```

### owner 字段未填充

当前版本 `owner` 字段固定为 `nullptr`。若需要从属性反查所属类型，请保留 PropertyInfo 来源的 TypeInfo 指针。

### 不可动态添加

PropertyInfo 在编译期静态注册后不可修改。不支持运行时添加/移除属性。

---

## 相关类型

- `mine::reflect::TypeInfo`：类型元数据描述符（包含属性列表）
- `mine::reflect::MethodInfo`：方法元数据描述符
- `mine::reflect::TypeRegistry`：全局类型注册表
- `mine::core::TypeId`：轻量类型标识符
- `mine::core::Variant`：类型擦除值容器（getter 返回值/setter 参数）
- `mine::core::StringView`：非拥有字符串视图（属性名）

---

## 示例：完整用法

```cpp
#include <mine/reflect/Reflect.h>

struct Person {
    MINE_REFLECT_DECL();

    const char* name() const { return name_; }
    void set_name(const char* n) { name_ = n; }

    int age() const { return age_; }
    void set_age(int a) { age_ = a; }

private:
    const char* name_{""};
    int age_{0};
};

MINE_REFLECT_IMPL(Person, void,
    MINE_PROP_ENTRY(Person, name, const char*, name, set_name),
    MINE_PROP_ENTRY(Person, age,  int,        age,  set_age)
);

void demo() {
    Person p;
    const TypeInfo* info = Person::static_type_info();

    // ── 反射读 ────────────────────────────────────────────────
    p.set_name("张三");
    p.set_age(30);

    const PropertyInfo* name_prop = info->find_property("name");
    Variant name_val = name_prop->getter(&p);
    printf("姓名: %s\n", name_val.get<const char*>());  // "张三"

    // ── 反射写 ────────────────────────────────────────────────
    const PropertyInfo* age_prop = info->find_property("age");
    age_prop->setter(&p, Variant{25});
    printf("年龄: %d\n", p.age());  // 25

    // ── 遍历所有属性 ──────────────────────────────────────────
    for (auto& prop : info->properties) {
        Variant v = prop.getter(&p);
        printf("  %s = ...\n", prop.name.data());
    }

    // ── 类型检查 ──────────────────────────────────────────────
    if (age_prop->type == TypeId::of<int>()) {
        printf("age 是整数类型\n");
    }
}
```
