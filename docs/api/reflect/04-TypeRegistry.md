# TypeRegistry 类详细接口文档

## 概述

**模块**：`mine.reflect`  
**头文件**：`<mine/reflect/TypeRegistry.h>`  
**命名空间**：`mine::reflect`

**用途**：进程级单例类型注册表，维护所有通过 `MINE_REFLECT_IMPL` 注册的类型元信息索引。支持按类型名（StringView）和 TypeId 双向查找，以及枚举所有已注册类型。

**核心特性**：
- Meyer's Singleton 模式（C++11 线程安全初始化）
- 按名称/TypeId 双向查找（线性搜索，适合数百个类型的场景）
- 重复注册检测（触发 `MINE_ASSERT` 断言）
- 延迟初始化内部存储（首次注册时分配）
- 静态初始化阶段自动注册（通过 `MINE_REFLECT_IMPL` 生成的自动注册对象）

**设计动机**：

在多模块、跨 DLL 的大型应用中，需要一种机制在不知道具体类型头文件的情况下，仅凭类型名称（字符串）就能找到类型的反射信息。`TypeRegistry` 解决了以下问题：
- **跨 DLL 类型映射**：不同 DLL 中 `TypeId::of<T>()` 可能不同（地址空间独立），但类型名字符串不变
- **序列化/反序列化**：从 JSON/TOML 中读取类型名字符串 → 查找 TypeInfo → 调用工厂创建实例
- **UI 设计器**：枚举所有可用组件类型，填充工具箱面板
- **脚本绑定**：向脚本引擎暴露所有已注册类型

---

## 类定义

```cpp
namespace mine::reflect {

class MINE_REFLECT_API TypeRegistry {
public:
    // ── 单例 ──────────────────────────────────────────────────────────────
    static TypeRegistry& instance() noexcept;

    // ── 注册 ──────────────────────────────────────────────────────────────
    void register_type(const TypeInfo* info) noexcept;

    // ── 查询 ──────────────────────────────────────────────────────────────
    [[nodiscard]] const TypeInfo* find_by_name(
        mine::core::StringView name) const noexcept;

    [[nodiscard]] const TypeInfo* find_by_id(
        mine::core::TypeId id) const noexcept;

    [[nodiscard]] mine::core::Span<const TypeInfo* const>
        all_types() const noexcept;

    [[nodiscard]] size_t type_count() const noexcept;

private:
    TypeRegistry() = default;
    ~TypeRegistry() = default;
    TypeRegistry(const TypeRegistry&) = delete;
    TypeRegistry& operator=(const TypeRegistry&) = delete;

    class Impl;
    Impl* impl_{nullptr};
};

// ── 便捷函数 ──────────────────────────────────────────────────────────────

[[nodiscard]] mine::core::TypeId
    type_id_by_name(mine::core::StringView name) noexcept;

[[nodiscard]] mine::core::StringView
    type_name_by_id(mine::core::TypeId id) noexcept;

} // namespace mine::reflect
```

---

## 静态方法

### `instance()`

**签名**：
```cpp
static TypeRegistry& instance() noexcept;
```

**描述**：获取全局唯一的 TypeRegistry 实例。

**实现**：Meyer's Singleton，利用 C++11 保证的局部静态变量线程安全初始化。

**返回值**：
- 全局 TypeRegistry 的引用（永不为 nullptr）

**时间复杂度**：O(1)

**示例**：
```cpp
TypeRegistry& reg = TypeRegistry::instance();
size_t n = reg.type_count();
```

---

## 注册方法

### `register_type(info)`

**签名**：
```cpp
void register_type(const TypeInfo* info) noexcept;
```

**描述**：注册一个类型的反射信息到全局注册表。

**参数**：
- `info`：指向静态 TypeInfo 实例的指针。指针本身及其引用的数据必须在进程生命周期内有效（通常为 static/global 变量）。

**前置条件**：
- `info != nullptr`
- `info->type_id.valid() == true`
- 同类型（按 TypeId）不可重复注册（重复会导致断言终止）

**实现细节**：
- 延迟初始化内部 `Impl`（首次注册时 `new`，进程退出时泄漏，避免静态析构顺序问题）
- 内部使用 `mine::containers::Vector<const TypeInfo*>` 存储
- 按 TypeId 检测重复注册

**典型调用**：通过 `MINE_REFLECT_IMPL` 宏自动生成，无需手动调用。

```cpp
// 以下由 MINE_REFLECT_IMPL 宏自动生成
namespace {
    struct _AutoReg {
        _AutoReg() noexcept {
            TypeRegistry::instance().register_type(&_s_type_info_MyType);
        }
    };
    static const _AutoReg _auto_inst;
}
```

**示例（手动注册）**：
```cpp
// 通常不需要手动调用，仅用于非 MINE_REFLECT_IMPL 注册的场景
static const TypeInfo custom_info = { /* ... */ };
TypeRegistry::instance().register_type(&custom_info);
```

---

## 查询方法

### `find_by_name(name)`

**签名**：
```cpp
[[nodiscard]] const TypeInfo* find_by_name(
    mine::core::StringView name) const noexcept;
```

**描述**：按类型名查找 TypeInfo。

**参数**：
- `name`：类型名（如 `"Button"`、`"test::Dog"`）。与 `MINE_REFLECT_IMPL` 中 `#TYPE` 字符串化的结果完全一致。

**返回值**：
- 匹配的 TypeInfo 指针
- 未找到或注册表为空则返回 `nullptr`

**时间复杂度**：O(n)，n 为已注册类型总数（线性搜索，适合数百个类型的场景）

**示例**：
```cpp
const TypeInfo* type = TypeRegistry::instance().find_by_name("Dog");
if (type) {
    printf("找到类型: %s\n", type->name.data());
    printf("属性数: %zu\n", type->properties.size());
}
```

### `find_by_id(id)`

**签名**：
```cpp
[[nodiscard]] const TypeInfo* find_by_id(
    mine::core::TypeId id) const noexcept;
```

**描述**：按 TypeId 查找 TypeInfo。

**参数**：
- `id`：类型的 TypeId（需有效，即 `id.valid() == true`）

**返回值**：
- 匹配的 TypeInfo 指针
- `id` 无效或未找到则返回 `nullptr`

**时间复杂度**：O(n)

**示例**：
```cpp
TypeId dog_id = TypeId::of<Dog>();
const TypeInfo* type = TypeRegistry::instance().find_by_id(dog_id);
if (type) {
    CHECK(type->name == "Dog");
}
```

### `all_types()`

**签名**：
```cpp
[[nodiscard]] mine::core::Span<const TypeInfo* const> all_types() const noexcept;
```

**描述**：获取所有已注册类型的视图。

**返回值**：
- `Span<const TypeInfo* const>`：已注册 TypeInfo 指针的只读视图
- 注册表为空时返回空 Span（`data() == nullptr, size() == 0`）

**时间复杂度**：O(1)（返回 Span 视图，无拷贝）

**示例**：
```cpp
for (const TypeInfo* type : TypeRegistry::instance().all_types()) {
    printf("类型: %s (%zu 属性)\n",
           type->name.data(), type->properties.size());
}
```

### `type_count()`

**签名**：
```cpp
[[nodiscard]] size_t type_count() const noexcept;
```

**描述**：获取已注册类型总数。

**返回值**：
- 已注册类型数量（注册表为空时返回 0）

**时间复杂度**：O(1)

---

## 便捷函数

### `type_name_by_id(id)`

**签名**：
```cpp
[[nodiscard]] mine::core::StringView type_name_by_id(
    mine::core::TypeId id) noexcept;
```

**描述**：按 TypeId 查找类型并返回其名称。

**参数**：
- `id`：类型的 TypeId

**返回值**：
- 匹配类型的名称 StringView
- 未找到则返回空 StringView（`size() == 0`）

**等价于**：`TypeRegistry::instance().find_by_id(id)->name`（有 nullptr 保护）

**示例**：
```cpp
printf("类型名: %s\n", type_name_by_id(TypeId::of<Dog>()).data());
// 输出: 类型名: Dog
```

### `type_id_by_name(name)`

**签名**：
```cpp
[[nodiscard]] mine::core::TypeId type_id_by_name(
    mine::core::StringView name) noexcept;
```

**描述**：按名称查找类型并返回其 TypeId。

**参数**：
- `name`：类型名称

**返回值**：
- 匹配类型的 TypeId
- 未找到则返回无效 TypeId（`valid() == false`）

**等价于**：`TypeRegistry::instance().find_by_name(name)->type_id`（有 nullptr 保护）

**示例**：
```cpp
TypeId id = type_id_by_name("Dog");
if (id.valid()) {
    printf("Dog 的 TypeId: %p\n", (void*)id.hash());
}
```

---

## 使用场景

### 1. 反序列化：按类型名创建实例

```cpp
Result<void*> deserialize_and_create(StringView json) {
    JsonReader reader(json);

    // 读取类型标签
    StringView type_name = reader.read_string("_type");
    const TypeInfo* type = TypeRegistry::instance().find_by_name(type_name);

    if (!type) {
        return err_tag, Status::not_found("未知类型");
    }
    if (!type->factory) {
        return err_tag, Status::not_implemented("类型不支持工厂创建");
    }

    // 创建实例
    void* obj = type->factory();
    if (!obj) {
        return err_tag, Status::out_of_memory();
    }

    // 按属性名反序列化
    for (auto& prop : type->properties) {
        if (prop.setter) {
            Variant val = reader.read_value(prop.name);
            prop.setter(obj, val);
        }
    }

    return ok_tag, obj;
}
```

### 2. UI 设计器工具箱面板

```cpp
void populate_toolbox(ToolboxPanel& panel) {
    for (const TypeInfo* type : TypeRegistry::instance().all_types()) {
        // 过滤：只显示继承自 Control 的类型
        const TypeInfo* control_type =
            TypeRegistry::instance().find_by_name("Control");

        if (control_type && type->is_a(control_type)) {
            panel.add_tool(type->name, type->factory);
        }
    }
}
```

### 3. 跨 DLL 类型映射

```cpp
// DLL A 中定义类型
MINE_REFLECT_IMPL(PluginButton, Button,
    MINE_PROP_ENTRY(PluginButton, plugin_id, int, plugin_id, set_plugin_id)
);

// DLL B 中按名称查找（不需要 include PluginButton 头文件）
const TypeInfo* type = TypeRegistry::instance().find_by_name("PluginButton");
if (type) {
    // 即使不同 DLL 中 TypeId::of<PluginButton>() 不同，
    // 按名称查找仍能正确找到
    printf("找到跨 DLL 类型: %s\n", type->name.data());
}
```

### 4. 脚本引擎类型绑定

```cpp
void bind_all_types_to_script(ScriptEngine& engine) {
    for (const TypeInfo* type : TypeRegistry::instance().all_types()) {
        engine.register_type(type->name);

        for (auto& prop : type->properties) {
            engine.register_property(type->name, prop.name,
                prop.getter, prop.setter);
        }
    }
}
```

### 5. 调试：打印所有已注册类型

```cpp
void dump_type_registry() {
    auto& reg = TypeRegistry::instance();
    printf("已注册类型总数: %zu\n", reg.type_count());

    for (const TypeInfo* type : reg.all_types()) {
        printf("  %s", type->name.data());
        if (type->base) {
            printf(" : %s", type->base->name.data());
        }
        printf(" (%zu 属性)\n", type->properties.size());
    }
}
```

---

## 性能特征

| 操作 | 时间复杂度 | 备注 |
|------|------------|------|
| `instance()` | O(1) | 静态局部变量 |
| `register_type()` | O(n) | 含重复检测（线性搜索） |
| `find_by_name()` | O(n) | 线性搜索，n ≤ 数百 |
| `find_by_id()` | O(n) | 线性搜索 |
| `all_types()` | O(1) | 返回 Span 视图 |
| `type_count()` | O(1) | 直接返回计数 |

**为什么使用线性搜索？**

对于 UI 框架中预期的类型数量（数百个），线性搜索远快于哈希表的开销：
- 无哈希计算开销（StringView 比较即 memcmp）
- 无哈希冲突处理
- 内存连续，CPU 缓存友好
- 代码简单，无第三方依赖

**未来优化**：若观察到类型数超过 1000 且有频繁查找场景，可替换为 `HashMap<StringView, const TypeInfo*>` 索引。

---

## 线程安全

**静态初始化安全**：
- `register_type()` 应在静态初始化阶段（`main()` 之前）通过 `MINE_REFLECT_IMPL` 自动调用完成
- 所有注册在单线程阶段完成，到 `main()` 时注册表已完全稳定

**读取安全**：
- `find_by_name()`、`find_by_id()`、`all_types()` 为纯只读操作
- 已注册的 TypeInfo 为不可变数据
- 多线程并发读取安全（无锁）

**运行时注册（不推荐）**：
- 若在 `main()` 之后调用 `register_type()`，与并发读取之间存在竞争
- 当前版本不提供锁保护
- 若确实需要运行时注册，调用方应自行同步

---

## 限制与注意事项

### 注册顺序不确定

不同翻译单元中的 `MINE_REFLECT_IMPL` 静态初始化顺序由链接器决定，不可依赖特定顺序。

**影响**：在静态初始化阶段，一个类型的注册可能早于其基类的注册。但由于 `TypeInfo::base` 通过 `Base::static_type_info()` 直接获取指针（不经过 TypeRegistry），这不会导致问题——基类 TypeInfo 即使尚未注册到 TypeRegistry，其静态变量地址已有效。

### 重复注册断言

```cpp
// ❌ 以下代码触发 MINE_ASSERT 断言
MINE_REFLECT_IMPL(MyType, void);
MINE_REFLECT_IMPL(MyType, void);  // 同一翻译单元中重复
```

不同翻译单元中的重复注册也会触发断言（即使宏使用 `__LINE__` 生成不同符号名，TypeId 相同仍会检测到）。

### 注册表永远增长

已注册的类型不会从注册表中移除。对需要动态加载/卸载插件的场景，当前版本不支持 `unregister_type()`。

**替代方案**：维护每插件独立的注册表实例，或使用类型名前缀过滤。

### 内部 Impl 内存泄漏（有意为之）

TypeRegistry 的内部 `Impl` 在首次 `register_type()` 时 `new` 分配，进程退出时不 `delete`。这是有意设计：
- 避免静态析构顺序问题（Impl 可能在类型 TypeInfo 静态变量析构后才被释放）
- 内存在进程退出时由操作系统回收

---

## 相关类型

- `mine::reflect::TypeInfo`：类型元数据描述符
- `mine::reflect::PropertyInfo`：属性元数据描述符
- `mine::reflect::MethodInfo`：方法元数据描述符
- `mine::core::TypeId`：轻量类型标识符
- `mine::core::StringView`：非拥有字符串视图

---

## 示例：完整用法

```cpp
#include <mine/reflect/Reflect.h>

// ── 类型定义与注册 ──────────────────────────────────────────────

struct GameObject {
    MINE_REFLECT_DECL();
    const char* tag() const { return tag_; }
    void set_tag(const char* t) { tag_ = t; }

    int health() const { return health_; }
    void set_health(int h) { health_ = h; }

private:
    const char* tag_{"Untagged"};
    int health_{100};
};

MINE_REFLECT_IMPL(GameObject, void,
    MINE_PROP_ENTRY(GameObject, tag,    const char*, tag,    set_tag),
    MINE_PROP_ENTRY(GameObject, health, int,         health, set_health)
);

struct Player : GameObject {
    MINE_REFLECT_DECL();
    const char* player_name() const { return name_; }
    void set_player_name(const char* n) { name_ = n; }

private:
    const char* name_{"Player"};
};

MINE_REFLECT_IMPL(Player, GameObject,
    MINE_PROP_ENTRY(Player, player_name, const char*, player_name, set_player_name)
);

// ── 使用 TypeRegistry ───────────────────────────────────────────

void demo_registry() {
    auto& reg = TypeRegistry::instance();

    // 按名称查找
    const TypeInfo* player_type = reg.find_by_name("Player");
    if (player_type) {
        printf("Player 属性:\n");
        for (auto& prop : player_type->properties) {
            printf("  .%s\n", prop.name.data());
        }
        // .player_name

        // 基类属性
        if (player_type->base) {
            for (auto& prop : player_type->base->properties) {
                printf("  .%s (继承)\n", prop.name.data());
            }
            // .tag (继承)
            // .health (继承)
        }
    }

    // 按 TypeId 查找
    TypeId go_id = TypeId::of<GameObject>();
    const TypeInfo* go_type = reg.find_by_id(go_id);
    if (go_type) {
        printf("通过 TypeId 找到: %s\n", go_type->name.data());
    }

    // 枚举所有注册类型
    printf("总注册类型: %zu\n", reg.type_count());
    for (const TypeInfo* type : reg.all_types()) {
        printf("  - %s\n", type->name.data());
    }

    // 便捷函数
    CHECK(type_name_by_id(TypeId::of<Player>()) == "Player");
    CHECK(type_id_by_name("GameObject") == TypeId::of<GameObject>());
}
```
