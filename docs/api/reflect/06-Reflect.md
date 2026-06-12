# mine.reflect 模块 API 总览

## 概述

**模块**：`mine.reflect`  
**头文件**：`<mine/reflect/Reflect.h>` （统一包含头）  
**命名空间**：`mine::reflect`

`mine.reflect` 是 MineFramework 的 L0 基础模块，提供编译期静态反射能力——在不依赖 RTTI（`/GR-`）的前提下，实现运行期类型元数据查询、属性访问和方法调用。

---

## 模块组成

| 类型 | 头文件 | 用途 |
|------|--------|------|
| `TypeInfo` | `<mine/reflect/TypeInfo.h>` | 类型完整反射元数据（名称、TypeId、基类、属性/方法列表） |
| `PropertyInfo` | `<mine/reflect/PropertyInfo.h>` | 属性元数据描述符（名称、类型、getter/setter 函数指针） |
| `MethodInfo` | `<mine/reflect/MethodInfo.h>` | 方法元数据描述符（名称、返回类型、参数列表、invoke 指针） |
| `TypeRegistry` | `<mine/reflect/TypeRegistry.h>` | 全局类型注册表单例（按名称/按 TypeId 查找） |
| `MINE_REFLECT_*` 宏 | `<mine/reflect/ReflectMacros.h>` | 声明式反射注册宏（DECL/IMPL/PROP_ENTRY） |

---

## 快速索引

### TypeInfo — 类型元数据
- **用途**：类型的完整反射信息容器
- **核心 API**：
  - 数据成员：`name`、`type_id`、`base`、`properties`、`methods`、`factory`
  - 查询：`find_property()`、`find_method()`、`is_a()`
- **详细文档**：[01-TypeInfo.md](01-TypeInfo.md)

**示例**：
```cpp
const TypeInfo* info = Button::static_type_info();
printf("类型: %s\n", info->name.data());
printf("属性数: %zu\n", info->properties.size());

if (info->is_a(Control::static_type_info())) {
    printf("Button 是 Control 的子类\n");
}
```

---

### PropertyInfo — 属性描述符
- **用途**：单个属性的元数据和反射访问器
- **核心 API**：
  - 元数据：`name`、`type`、`owner`
  - 访问器：`getter(obj)` → Variant、`setter(obj, value)`
- **详细文档**：[02-PropertyInfo.md](02-PropertyInfo.md)

**示例**：
```cpp
const PropertyInfo* prop = info->find_property("text");
if (prop && prop->getter) {
    Variant val = prop->getter(&button);
    printf("text = %s\n", val.get<const char*>());
}

if (prop && prop->setter) {
    prop->setter(&button, Variant{"新文本"});
}
```

---

### MethodInfo — 方法描述符
- **用途**：单个方法的元数据和反射调用接口（预留）
- **特性**：可变参数调用、类型安全检查
- **当前状态**：方法注册功能预留，`methods` 始终为空
- **详细文档**：[03-MethodInfo.md](03-MethodInfo.md)

---

### TypeRegistry — 全局注册表
- **用途**：进程级类型索引
- **核心 API**：
  - 单例：`TypeRegistry::instance()`
  - 查询：`find_by_name()`、`find_by_id()`、`all_types()`
  - 便捷函数：`type_name_by_id()`、`type_id_by_name()`
- **详细文档**：[04-TypeRegistry.md](04-TypeRegistry.md)

**示例**：
```cpp
// 按名称查找类型
const TypeInfo* type = TypeRegistry::instance().find_by_name("Dog");
if (type) {
    printf("找到 %zu 个属性\n", type->properties.size());
}

// 枚举所有已注册类型
for (auto* info : TypeRegistry::instance().all_types()) {
    printf("  %s\n", info->name.data());
}

// 便捷函数
StringView name = type_name_by_id(TypeId::of<Dog>());  // "Dog"
TypeId id = type_id_by_name("Animal");                  // TypeId::of<Animal>()
```

---

### 反射注册宏 — MINE_REFLECT_*
- **用途**：声明式编译期类型注册
- **核心宏**：
  - `MINE_REFLECT_DECL()` — 头文件中声明反射接口
  - `MINE_REFLECT_IMPL(TYPE, BASE, ...)` — cpp 中注册元数据
  - `MINE_PROP_ENTRY(OWNER, NAME, TYPE, GET, SET)` — 定义属性条目
- **详细文档**：[05-ReflectMacros.md](05-ReflectMacros.md)

**示例**：
```cpp
// ── 头文件 ────────────────────────────────────────────────────
struct Animal {
    MINE_REFLECT_DECL();
    const char* name() const;
    void set_name(const char* n);
};

// ── 实现文件 ──────────────────────────────────────────────────
MINE_REFLECT_IMPL(Animal, void,
    MINE_PROP_ENTRY(Animal, name, const char*, name, set_name)
);
```

---

## 使用决策树

### 如何选择合适的组件？

```
需要运行期读取/写入类型属性?
├─ 是 → 需要属性元数据?
│  ├─ 是 → 使用 PropertyInfo::getter/setter（通过 TypeInfo 获取）
│  └─ 否 → 仅需类型识别?
│     └─ 是 → 使用 TypeId（mine.core）
│
├─ 需要按名称查找类型（跨 DLL/序列化）?
│  └─ 是 → TypeRegistry::find_by_name()
│
├─ 需要枚举所有已注册类型?
│  └─ 是 → TypeRegistry::all_types()
│
├─ 需要检查继承关系?
│  └─ 是 → TypeInfo::is_a()
│
└─ 需要将自定义类型注册到反射系统?
   ├─ 是 → MINE_REFLECT_DECL() + MINE_REFLECT_IMPL()
   └─ 否 → 无需使用 mine.reflect
```

---

## 设计原则

### 1. 无异常错误处理

所有操作均为 `noexcept`。类型不匹配时通过 `MINE_ASSERT` 断言终止（debug）/ 裁剪（release）：

```cpp
PropertyInfo* prop = info->find_property("age");
prop->setter(obj, Variant{"hello"});  // 断言：any_cast<int> 类型不匹配
```

### 2. 纯静态编译期数据

TypeInfo、PropertyInfo、MethodInfo 均为编译期静态数据，不涉及堆分配。所有指针指向的字符串为编译期字面量。

### 3. 自动注册

通过 `MINE_REFLECT_IMPL` 注册的类型在静态初始化阶段自动加入 `TypeRegistry`，无需手动调用 `register_type()`。

### 4. 类型擦除访问

属性 getter/setter 通过 `void*` + `Variant` 的擦除类型形式实现，调用方无需知道具体 C++ 类型即可读写属性。

### 5. 宏驱动声明

反射注册通过宏完成，减少样板代码。MML 预编译器自动生成这些宏调用，纯 C++ 代码也可手动使用。

---

## 典型使用模式

### 模式 1：声明式类型注册

```cpp
// ── 头文件 ────────────────────────────────────────────────────
struct Person {
    MINE_REFLECT_DECL();
    virtual ~Person() = default;

    const char* name() const { return name_; }
    void set_name(const char* n) { name_ = n; }

    int age() const { return age_; }
    void set_age(int a) { age_ = a; }

private:
    const char* name_{""};
    int age_{0};
};

// ── 实现文件 ──────────────────────────────────────────────────
MINE_REFLECT_IMPL(Person, void,
    MINE_PROP_ENTRY(Person, name, const char*, name, set_name),
    MINE_PROP_ENTRY(Person, age,  int,         age,  set_age)
);
```

### 模式 2：反射序列化

```cpp
void write_json(const TypeInfo* type, void* obj, FILE* out) {
    fprintf(out, "{");

    // 写类型标签
    fprintf(out, "\"_type\":\"%s\"", type->name.data());

    // 写属性
    for (auto& prop : type->properties) {
        if (!prop.getter) continue;
        Variant v = prop.getter(obj);

        fprintf(out, ",\"%s\":", prop.name.data());
        if (v.has<int>()) fprintf(out, "%d", v.get<int>());
        else if (v.has<const char*>()) fprintf(out, "\"%s\"", v.get<const char*>());
    }

    // 递归基类
    if (type->base) {
        fprintf(out, ",");
        write_json(type->base, obj, out);
    }

    fprintf(out, "}");
}
```

### 模式 3：通用对象拷贝

```cpp
void deep_copy(const TypeInfo* type, const void* src, void* dst) {
    for (auto& prop : type->properties) {
        if (prop.getter && prop.setter) {
            prop.setter(dst, prop.getter(src));
        }
    }
    if (type->base) {
        deep_copy(type->base, src, dst);
    }
}
```

### 模式 4：按名称查找类型

```cpp
void* create_by_name(StringView type_name) {
    const TypeInfo* type = TypeRegistry::instance().find_by_name(type_name);
    if (!type) {
        printf("未知类型: %s\n", type_name.data());
        return nullptr;
    }
    if (!type->factory) {
        printf("'%s' 不支持工厂创建\n", type_name.data());
        return nullptr;
    }
    return type->factory();
}
```

### 模式 5：继承感知的属性遍历

```cpp
void dump_all_properties(const TypeInfo* type, void* obj) {
    const TypeInfo* current = type;
    while (current) {
        printf("[%s]\n", current->name.data());
        for (auto& prop : current->properties) {
            if (prop.getter) {
                Variant v = prop.getter(obj);
                printf("  %s = ...\n", prop.name.data());
            }
        }
        current = current->base;
    }
}
```

---

## 类型注册快速参考

| 需求 | 做法 |
|------|------|
| 单属性类型 | `MINE_REFLECT_IMPL(T, void, MINE_PROP_ENTRY(T, x, int, x, set_x));` |
| 多属性类型 | `MINE_REFLECT_IMPL(T, void, MINE_PROP_ENTRY(...), MINE_PROP_ENTRY(...));` |
| 有基类的类型 | `MINE_REFLECT_IMPL(Derived, Base, ...);` |
| 无属性的类型 | `MINE_REFLECT_IMPL(T, void);` |
| 仅类型标识（无基类无属性） | `MINE_REFLECT_IMPL(EmptyComponent, void);` |

---

## 测试覆盖

| 测试套件 | 用例数 | 断言数 | 覆盖范围 |
|----------|--------|--------|----------|
| mine.reflect | 18 | 43 | TypeInfo 构造/查询、PropertyInfo 读写、TypeRegistry CRUD、反射宏验证、继承链 is_a、空的属性列表 |

运行测试：
```bash
xmake build mine.reflect.test
xmake run mine.reflect.test
```

---

## 构建配置

### xmake

```lua
-- 在上层模块的 xmake.lua 中
target("my_app")
    add_deps("mine.reflect")
```

### CMake

```cmake
find_package(MineFramework REQUIRED COMPONENTS reflect)
target_link_libraries(my_app PRIVATE Mine::reflect)
```

---

## 依赖

```
mine.reflect
  ├─ mine.core       (TypeId, StringView, Variant, Span)
  └─ mine.containers (Vector, 用于 TypeRegistry 内部)
```

---

## 已知限制

| 限制 | 影响 | 规划 |
|------|------|------|
| 方法注册未实现 | 无法通过反射调用方法 | 后续版本集成 MINE_METHOD_ENTRY 到 IMPL |
| 工厂函数未实现 | 无法通过反射创建实例 | 后续版本添加 MINE_FACTORY 宏 |
| owner 字段未填充 | 无法从属性反向查找所属类型 | M0.2 完善 |
| 不支持属性注解 | 无法标记只读、必填、范围等约束 | M1.0 添加 |
| 不支持运行时添加属性 | 所有元数据编译期固定 | 通过外部 Map 扩展 |

---

## 相关模块

- **`mine.core`**：提供 `TypeId`、`StringView`、`Variant`、`Span<T>` 等基础类型
- **`mine.meta`**（规划中）：类型工厂、序列化器，依赖 `mine.reflect`
- **`mine.ui.property`**：依赖属性系统，使用 `TypeInfo` 进行类型注册
- **`mine.ui.binding`**：数据绑定，依赖反射桥进行属性路径解析
- **`mine.di`**（规划中）：DI/IoC 容器，通过反射进行类型解析和注入
- **`mine.data.orm`**（规划中）：ORM 实体映射，基于反射进行字段枚举

---

## 参考资料

- [01-architecture.md](../../01-architecture.md) — 分层架构
- [02-modules.md](../../02-modules.md) — 模块清单
- [09-property-binding.md](../../09-property-binding.md) — 依赖属性系统中的反射桥
- [04-precompiler.md](../../04-precompiler.md) — MML 预编译器如何生成反射注册
- [development-plan.md](../../development-plan.md) — 开发计划

---

## 详细文档索引

1. **[01-TypeInfo.md](01-TypeInfo.md)** — TypeInfo 结构详细接口文档
2. **[02-PropertyInfo.md](02-PropertyInfo.md)** — PropertyInfo 结构详细接口文档
3. **[03-MethodInfo.md](03-MethodInfo.md)** — MethodInfo 结构详细接口文档
4. **[04-TypeRegistry.md](04-TypeRegistry.md)** — TypeRegistry 类详细接口文档
5. **[05-ReflectMacros.md](05-ReflectMacros.md)** — 反射注册宏详细文档
