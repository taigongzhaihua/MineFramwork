# TypeInfo 结构详细接口文档

## 概述

**模块**：`mine.reflect`  
**头文件**：`<mine/reflect/TypeInfo.h>`  
**命名空间**：`mine::reflect`

**用途**：类型的完整反射元数据描述符，汇集一个 C++ 类型的所有编译期可获知的元信息。

**核心特性**：
- 纯静态数据，无需堆分配（属性/方法列表通过 Span 引用外部静态数组）
- 支持按名称查找属性/方法（线性搜索，适合数百个属性以内的场景）
- 支持继承链遍历（通过 `base` 指针和 `is_a()` 方法）
- 所有字段均为编译期常量或指向静态数据的指针，TypeInfo 自身不拥有任何需要释放的资源
- 通过 `MINE_REFLECT_IMPL` 宏自动生成实例，无需手动构造

**设计动机**：

MineFramework 禁用 RTTI（`/GR-`），但上层模块（依赖属性绑定、DI 容器、ORM 映射、序列化）需要运行期类型信息。`TypeInfo` 提供了一种比 `std::type_info` 更丰富、更可控的替代方案：

- **更丰富**：包含属性列表、方法列表、基类链等结构化元数据
- **更可控**：编译期静态注册，无运行时开销，无全局锁
- **跨 DLL 安全**：通过 `TypeRegistry` 按名称查找，绕过 `TypeId` 的 DLL 边界限制
- **可扩展**：后续可添加工厂函数、属性变更钩子、自定义注解等

---

## 结构定义

```cpp
namespace mine::reflect {

struct TypeInfo {
    // ── 数据成员 ──────────────────────────────────────────────────────────
    mine::core::StringView                name;
    mine::core::TypeId                    type_id;
    const TypeInfo*                       base;
    mine::core::Span<const PropertyInfo>  properties;
    mine::core::Span<const MethodInfo>    methods;
    void* (*factory)();

    // ── 查询方法 ──────────────────────────────────────────────────────────
    const PropertyInfo* find_property(mine::core::StringView prop_name) const noexcept;
    const MethodInfo*   find_method(mine::core::StringView method_name) const noexcept;

    [[nodiscard]] bool is_a(const TypeInfo* base_type) const noexcept;
};

} // namespace mine::reflect
```

---

## 数据成员

### `name`

**类型**：`mine::core::StringView`

**描述**：类型名称（编译期常量字符串，无需释放）。

**来源**：由 `MINE_REFLECT_IMPL` 宏中的 `#TYPE` 字符串化操作生成。

**示例值**：`"Button"`、`"test::Dog"`、`"mine::ui::TextBlock"`

**约束**：
- 指向编译期字符串字面量（静态存储期），永不失效
- 命名空间完全限定名取决于 `TYPE` 参数的传递方式

**用途**：
- 序列化/反序列化时的类型标签
- 日志和调试输出
- 通过 `TypeRegistry::find_by_name()` 查找类型

---

### `type_id`

**类型**：`mine::core::TypeId`

**描述**：类型的 TypeId，通过 `TypeId::of<T>()` 获取。

**用途**：
- O(1) 类型比较（`info->type_id == TypeId::of<Button>()`）
- 通过 `TypeRegistry::find_by_id()` 查找类型
- 可作为哈希表键（通过 `hash()` 方法）

**约束**：
- 仅在当前 DLL/可执行文件内保证唯一性
- 跨 DLL 边界比较需通过 `TypeRegistry` 按名称查找

---

### `base`

**类型**：`const TypeInfo*`

**描述**：直接基类的 TypeInfo 指针。若类型无基类（在 `MINE_REFLECT_IMPL` 中 `BASE` 参数为 `void`），则为 `nullptr`。

**用途**：
- 继承链向上遍历
- `is_a()` 方法依赖此字段实现继承判断
- DI 容器进行注入时可用于向上查找接口类型

**示例**：
```cpp
const TypeInfo* dog = Dog::static_type_info();
const TypeInfo* animal = Animal::static_type_info();
// dog->base == animal
// animal->base == nullptr
```

---

### `properties`

**类型**：`mine::core::Span<const PropertyInfo>`

**描述**：属性元数据列表视图，按 `MINE_REFLECT_IMPL` 中声明的顺序排列。

**来源**：由 `MINE_REFLECT_IMPL` 宏展开生成静态 `PropertyInfo` 数组，Span 引用该数组。

**特性**：
- 零堆分配：Span 指向编译期静态数组
- 支持范围 for 遍历：`for (auto& prop : info->properties) { ... }`
- 支持索引访问：`info->properties[0]`

**查找属性**：使用 `find_property()` 方法比手动遍历更便捷。

---

### `methods`

**类型**：`mine::core::Span<const MethodInfo>`

**描述**：方法元数据列表视图，按 `MINE_REFLECT_IMPL` 中声明的顺序排列。

**当前状态**：方法注册功能预留，当前版本 `methods` 始终为空 Span。后续版本将通过 `MINE_METHOD_ENTRY` 宏支持方法注册。

---

### `factory`

**类型**：`void* (*)()`

**描述**：默认构造工厂函数指针。

- 若为 `nullptr`：该类型不支持通过反射创建实例
- 若有效：调用返回堆分配的对象指针

**调用方职责**：通过合适的 `delete` 或类型自身的销毁机制管理返回对象的生命周期。

**当前状态**：MINE_REFLECT_IMPL 生成的 TypeInfo 中 `factory` 固定为 `nullptr`，后续版本将通过 `MINE_FACTORY` 宏支持工厂注册。

---

## 查询方法

### `find_property(prop_name)`

**签名**：
```cpp
const PropertyInfo* find_property(mine::core::StringView prop_name) const noexcept;
```

**描述**：按名称在 `properties` 列表中查找属性。

**参数**：
- `prop_name`：属性名称（如 `"text"`、`"enabled"`）

**返回值**：
- 匹配的 `PropertyInfo` 指针
- 未找到则返回 `nullptr`

**时间复杂度**：O(n)，n = `properties.size()`。对于 UI 控件典型属性数（< 50），线性搜索完全可接受。

**示例**：
```cpp
const TypeInfo* info = Button::static_type_info();
const PropertyInfo* prop = info->find_property("text");
if (prop) {
    printf("属性类型: %s\n", type_name_by_id(prop->type).data());
}
```

---

### `find_method(method_name)`

**签名**：
```cpp
const MethodInfo* find_method(mine::core::StringView method_name) const noexcept;
```

**描述**：按名称在 `methods` 列表中查找方法。

**参数**：
- `method_name`：方法名称

**返回值**：
- 匹配的 `MethodInfo` 指针
- 未找到则返回 `nullptr`

**时间复杂度**：O(n)，n = `methods.size()`。

**当前状态**：由于 `methods` 始终为空，此方法始终返回 `nullptr`。后续版本扩展。

---

### `is_a(base_type)`

**签名**：
```cpp
[[nodiscard]] bool is_a(const TypeInfo* base_type) const noexcept;
```

**描述**：沿 `base` 指针链向上遍历，检查当前类型是否为指定基类的子类型（含自身）。

**参数**：
- `base_type`：候选基类的 TypeInfo 指针

**返回值**：
- `true`：当前类型是 `base_type` 自身或其派生类
- `false`：不是，或 `base_type` 为 `nullptr`

**时间复杂度**：O(h)，h 为继承链深度

**示例**：
```cpp
const TypeInfo* dog = Dog::static_type_info();
const TypeInfo* animal = Animal::static_type_info();
const TypeInfo* button = Button::static_type_info();

CHECK(dog->is_a(animal));    // true：Dog 继承自 Animal
CHECK(dog->is_a(dog));       // true：自身
CHECK(!animal->is_a(dog));   // false：Animal 不是 Dog
CHECK(!dog->is_a(button));   // false：无关类型
CHECK(!dog->is_a(nullptr));  // false：nullptr 参数
```

**实现**：
```cpp
const TypeInfo* current = this;
while (current) {
    if (current == base_type) return true;
    current = current->base;
}
return false;
```

---

## 获取 TypeInfo

### 通过 static_type_info()

每个使用了 `MINE_REFLECT_DECL()` 的类型都提供一个静态方法：

```cpp
const TypeInfo* info = MyType::static_type_info();
```

这是最常用的方式，编译期即可确定类型。

### 通过 _get_reflect_type_info()

每个使用了 `MINE_REFLECT_DECL()` 的类型都提供一个虚方法：

```cpp
MyType obj;
const TypeInfo* info = obj._get_reflect_type_info();
```

通过虚函数分派，可以在不知道具体类型时获取正确的 TypeInfo：

```cpp
void print_type_name(Animal* ptr) {
    auto* info = ptr->_get_reflect_type_info();
    printf("实际类型: %s\n", info->name.data());
}

Dog dog;
print_type_name(&dog);  // 输出 "实际类型: Dog"
```

### 通过 TypeRegistry

```cpp
const TypeInfo* info = TypeRegistry::instance().find_by_name("Dog");
const TypeInfo* info2 = TypeRegistry::instance().find_by_id(TypeId::of<Dog>());
```

---

## 使用场景

### 1. 序列化：按属性名写入

```cpp
void serialize(const TypeInfo* type, void* obj, JsonWriter& out) {
    out.begin_object();
    out.key("_type").string(type->name);

    for (auto& prop : type->properties) {
        Variant val = prop.getter(obj);
        out.key(prop.name);
        write_variant(out, val);
    }

    out.end_object();
}
```

### 2. 反序列化：按属性名读取并设置

```cpp
Result<void> deserialize(const TypeInfo* type, void* obj, JsonReader& in) {
    MINE_TRY(in.expect_object_begin());

    while (in.has_next_key()) {
        StringView key = in.next_key();
        const PropertyInfo* prop = type->find_property(key);
        if (prop && prop->setter) {
            Variant val = read_variant(in, prop->type);
            prop->setter(obj, val);
        } else {
            in.skip_value();  // 跳过未知属性
        }
    }

    return ok_tag;
}
```

### 3. DI 容器：按接口类型查找实现

```cpp
class ServiceCollection {
    HashMap<const TypeInfo*, const TypeInfo*> bindings_;

public:
    template<typename TInterface, typename TImpl>
    void bind() {
        bindings_[TInterface::static_type_info()] = TImpl::static_type_info();
    }

    template<typename TInterface>
    TInterface* resolve() const {
        auto* impl_type = bindings_[TInterface::static_type_info()];
        if (impl_type && impl_type->factory) {
            return static_cast<TInterface*>(impl_type->factory());
        }
        return nullptr;
    }
};
```

### 4. 属性面板：枚举控件属性

```cpp
void populate_property_grid(const TypeInfo* type, void* obj) {
    for (auto& prop : type->properties) {
        // 为每个属性创建编辑控件
        add_property_row(prop.name, prop.type, prop.getter(obj),
            [prop, obj](const Variant& v) { prop->setter(obj, v); });
    }

    // 沿继承链向上添加基类属性
    if (type->base) {
        populate_property_grid(type->base, obj);
    }
}
```

---

## 性能特征

| 操作 | 时间复杂度 | 备注 |
|------|------------|------|
| 获取 TypeInfo 指针 | O(1) | 静态变量地址 |
| `find_property()` | O(n) | n ≤ 典型控件属性数（< 50） |
| `find_method()` | O(n) | 当前 n = 0 |
| `is_a()` | O(h) | h ≤ 典型继承深度（< 10） |
| 遍历 properties | O(n) | Span 视图，无拷贝 |

**内存占用**：
- TypeInfo 自身：约 72 字节（64 位平台）
- 每个属性的 PropertyInfo：约 56 字节（在静态数组中，非堆分配）
- 无运行时堆分配（所有数据均为编译期静态）

---

## 线程安全

**完全线程安全**：
- TypeInfo 及其所有成员在静态初始化阶段构造完毕后即为不可变（immutable）
- `find_property()`、`find_method()`、`is_a()` 均为纯只读查询
- 多线程并发读取无需任何同步

---

## 限制与注意事项

### 不可修改

TypeInfo 实例在静态初始化阶段构造完毕后不可修改。不支持运行时动态添加/移除属性或方法。

**替代方案**：若需要动态可扩展的元数据，可通过外部 `HashMap<const TypeInfo*, CustomData>` 实现。

### 线性搜索性能

当属性数超过数百时，`find_property()` 的线性搜索可能成为瓶颈。对于 UI 控件的典型使用场景（< 50 个属性），这是最优策略（无哈希表开销）。

**未来优化**：若观察到实际瓶颈，可为属性数量多的类型提供 `HashMap<StringView, PropertyInfo*>` 索引。

### 方法注册未实现

当前版本 `methods` 始终为空。方法注册功能将在后续版本中通过 `MINE_METHOD_ENTRY` 宏实现。

### 工厂函数未实现

当前版本 `factory` 固定为 `nullptr`。对象工厂注册将在后续版本中通过 `MINE_FACTORY` 宏实现。

---

## 相关类型

- `mine::reflect::PropertyInfo`：属性元数据描述符
- `mine::reflect::MethodInfo`：方法元数据描述符
- `mine::reflect::TypeRegistry`：全局类型注册表
- `mine::core::TypeId`：轻量类型标识符
- `mine::core::Span<T>`：非拥有连续内存视图

---

## 示例：完整用法

```cpp
#include <mine/reflect/Reflect.h>

// ── 定义类型 ────────────────────────────────────────────────────

struct Animal {
    MINE_REFLECT_DECL();
    virtual ~Animal() = default;

    const char* name() const { return name_; }
    void set_name(const char* n) { name_ = n; }

    int age() const { return age_; }
    void set_age(int a) { age_ = a; }

private:
    const char* name_{"未命名"};
    int age_{0};
};

struct Dog : Animal {
    MINE_REFLECT_DECL();

    const char* breed() const { return breed_; }
    void set_breed(const char* b) { breed_ = b; }

private:
    const char* breed_{"混种"};
};

// ── 注册反射 ────────────────────────────────────────────────────

MINE_REFLECT_IMPL(Animal, void,
    MINE_PROP_ENTRY(Animal, name, const char*, name, set_name),
    MINE_PROP_ENTRY(Animal, age,  int,        age,  set_age)
);

MINE_REFLECT_IMPL(Dog, Animal,
    MINE_PROP_ENTRY(Dog, breed, const char*, breed, set_breed)
);

// ── 使用反射 ────────────────────────────────────────────────────

void demo() {
    // 获取类型信息
    const TypeInfo* info = Dog::static_type_info();
    printf("类型: %s\n", info->name.data());  // "Dog"

    // 遍历属性（含继承）
    for (auto& prop : info->properties) {
        printf("  .%s\n", prop.name.data());  // ".breed"
    }
    for (auto& prop : info->base->properties) {
        printf("  .%s\n", prop.name.data());  // ".name" ".age"
    }

    // is_a 检查
    const TypeInfo* animal = Animal::static_type_info();
    printf("Dog is Animal: %s\n", info->is_a(animal) ? "是" : "否");  // "是"

    // 反射读写属性
    Dog dog;
    info->find_property("breed")->setter(&dog, Variant{"金毛"});
    Variant v = info->find_property("breed")->getter(&dog);
    printf("品种: %s\n", v.get<const char*>());  // "金毛"
}
```
