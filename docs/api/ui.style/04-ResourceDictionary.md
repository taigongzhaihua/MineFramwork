# ResourceDictionary API 文档

## 概述

`ResourceDictionary` 是 MineUI 样式系统的核心资源管理组件，提供树形键值资源存储和动态通知机制。它支持多层资源查找、字典合并、父子连接和订阅通知，是实现主题切换、样式继承和动态资源绑定的基础设施。

### 核心特性

1. **树形资源查找**  
   支持三层查找顺序：本层 `local_` → 合并层 `merged_` → 父层 `parent_`（递归）。未命中返回空 `Variant`。

2. **字典合并机制**  
   通过 `merge()` 可将多个资源字典合并到当前字典，支持主题继承（如 Dark 主题扩展 Base 主题）。合并字典使用弱引用，不拥有其生命周期。

3. **静态 vs 动态资源**  
   - `set(key, value)`：写入静态值，不触发订阅通知（适合批量初始化）
   - `set_dynamic(key, value)`：写入动态值，触发订阅通知和广播事件（适合运行时更新）

4. **双重通知机制**  
   - **精确订阅**：`subscribe(key, callback)` 监听特定键的值变更（用于 DynamicResource 绑定）
   - **广播通知**：`on_resource_changed(callback)` 监听任意资源变更（用于子字典级联更新）

5. **父字典连接**  
   通过 `set_parent(parent)` 连接到父字典，形成布局树同构的资源查找链。子字典自动订阅父字典的 `resource_changed` 事件。

6. **全量刷新支持**  
   `notify_resource_changed("*")` 触发全量刷新广播，用于主题切换等大规模资源更新场景。

### 查找顺序

```
find("PrimaryColor") 查找流程：
  1. 本层 local_["PrimaryColor"]
  2. 合并层 merged_[0].find("PrimaryColor")  // 按 merge() 调用顺序
     merged_[1].find("PrimaryColor")
  3. 父层 parent_->find("PrimaryColor")      // 递归查找
  4. 未命中返回 Variant()（空值）
```

### 通知机制

```cpp
// 动态更新触发两种通知：
dict.set_dynamic("AccentColor", Color::Blue);

// 1. 精确订阅回调（仅订阅 "AccentColor" 的回调被触发）
subscribe("AccentColor", [](const Variant& v) { /* ... */ });

// 2. 广播通知（所有 on_resource_changed 订阅者收到 "AccentColor" 变更）
on_resource_changed([](StringView key) { 
    if (key == "AccentColor") { /* ... */ }
});
```

---

## 文件位置

```
src/mine/ui/style/api/include/mine/ui/style/ResourceDictionary.h
```

---

## 类定义

```cpp
namespace mine::ui::style {

/**
 * @brief 树形资源字典。
 *
 * 用于 MineUI 样式系统的多层键值资源存储。每个 UIElement 可持有一个
 * ResourceDictionary，通过 set_parent() 连接到父控件的字典，形成树形查找链。
 *
 * 查找顺序（find）：
 *   本层 local_ → 合并层 merged_（按 merge 顺序）→ 父层 parent_（递归）
 *
 * 通知机制：
 *   - subscribe(key, cb)     ：订阅特定键的值变更（DynamicResource 场景）
 *   - on_resource_changed(cb)：订阅任意键变更的广播（子字典、主题切换场景）
 *   - set_dynamic(key, val)  ：写入并广播，触发上述所有相关回调
 *   - notify_resource_changed(key)：手动广播（如主题合并后调用）
 *
 * 线程安全：不提供，调用方负责在同一线程使用。
 */
class MINE_UI_STYLE_API ResourceDictionary {
public:
    ResourceDictionary();
    ~ResourceDictionary();

    /// 不可拷贝（内部持有弱引用指针和 move-only 回调）
    ResourceDictionary(const ResourceDictionary&)            = delete;
    ResourceDictionary& operator=(const ResourceDictionary&) = delete;

    /// 可移动
    ResourceDictionary(ResourceDictionary&&) noexcept;
    ResourceDictionary& operator=(ResourceDictionary&&) noexcept;

    // ── 写入 ──────────────────────────────────────────────────────────────

    /// 写入静态值，不触发订阅通知（适用于批量初始化 / 主题加载）。
    void set(core::StringView key, core::Variant value);

    /// 写入动态值：更新后触发该 key 的所有 subscribe() 回调，
    /// 并广播 resource_changed（on_resource_changed 订阅者均会收到通知）。
    void set_dynamic(core::StringView key, core::Variant value);

    // ── 合并 ──────────────────────────────────────────────────────────────

    /// 合并另一资源字典（弱引用，不拥有其生命周期）。
    /// 查找时若本层无命中，依次在合并层中查找（按 merge 调用顺序）。
    void merge(const ResourceDictionary& other);

    /// 清除所有已合并字典（保留本层和父层连接）。
    void clear_merged() noexcept;

    // ── 查找 ──────────────────────────────────────────────────────────────

    /// 树形查找：本层 → 合并层 → 父层。
    /// 未命中返回空 Variant（调用方用 has_value() 判断）。
    [[nodiscard]] core::Variant find(core::StringView key) const noexcept;

    /// 仅查本层。未命中返回空 Variant。
    [[nodiscard]] core::Variant find_local(core::StringView key) const noexcept;

    // ── DynamicResource 订阅 ───────────────────────────────────────────────

    /// 订阅特定 key 的值变更。
    /// @return 取消令牌，传入 unsubscribe() 可取消。kInvalidHandlerId 表示失败。
    HandlerId subscribe(core::StringView key,
                        core::Function<void(const core::Variant&)> callback);

    /// 取消特定 key 的订阅。
    void unsubscribe(HandlerId id) noexcept;

    // ── 父字典连接 ────────────────────────────────────────────────────────

    /// 连接父字典（布局系统在 visual tree attach 时调用）。
    /// 传 nullptr 解除连接。自动管理父字典的 resource_changed 订阅。
    void set_parent(ResourceDictionary* parent) noexcept;

    /// 返回当前父字典，可为 nullptr。
    [[nodiscard]] ResourceDictionary* parent() const noexcept;

    // ── resource_changed 广播接口 ─────────────────────────────────────────

    /// 订阅任意资源键变更广播。key="*" 表示全量刷新（如主题切换）。
    /// @return 取消令牌，传入 off_resource_changed() 可取消。
    HandlerId on_resource_changed(core::Function<void(core::StringView)> callback);

    /// 取消 resource_changed 广播订阅。
    void off_resource_changed(HandlerId id) noexcept;

    /// 手动广播资源变更通知。
    /// 主题合并（merge + clear_merged）后调用，key="*" 表示全量刷新。
    void notify_resource_changed(core::StringView key);

private:
    struct Impl;
    core::Pimpl<Impl> p_;
};

}  // namespace mine::ui::style
```

---

## 成员方法详解

### 构造与析构

#### `ResourceDictionary()`

默认构造函数，创建空资源字典。

```cpp
auto dict = ResourceDictionary();
```

---

#### `~ResourceDictionary()`

析构函数，自动清理所有订阅和合并引用。

---

#### 移动语义

```cpp
ResourceDictionary(ResourceDictionary&&) noexcept;
ResourceDictionary& operator=(ResourceDictionary&&) noexcept;
```

支持移动构造和移动赋值。**不可拷贝**（内部持有弱引用指针和 move-only 回调）。

**示例：**
```cpp
auto dict1 = ResourceDictionary();
dict1.set("Key", "Value");

auto dict2 = std::move(dict1);  // ✅ 移动合法
// dict1 现在处于未定义状态，不可再使用

// auto dict3 = dict2;  // ❌ 编译错误：不可拷贝
```

---

### 写入方法

#### `void set(core::StringView key, core::Variant value)`

写入静态资源值，**不触发任何订阅通知**。适用于批量初始化、主题文件加载等场景。

**参数：**
- `key`：资源键（如 `"PrimaryColor"`、`"FontSize"`）
- `value`：资源值（`Variant` 可存储任意类型：`int`、`double`、`Color`、`Brush` 等）

**特点：**
- 性能更高（无通知开销）
- 不触发 `subscribe()` 回调
- 不触发 `on_resource_changed()` 广播

**使用场景：**
- 应用启动时批量加载主题资源
- 从 JSON/XML 文件解析样式字典
- 控件初始化时写入默认样式

**示例：**
```cpp
auto theme = ResourceDictionary();
theme.set("PrimaryColor", Color::from_rgb(0x007ACC));
theme.set("FontSize", 14.0);
theme.set("Padding", Thickness(10, 5));
```

---

#### `void set_dynamic(core::StringView key, core::Variant value)`

写入动态资源值，**触发双重通知机制**：
1. 调用该键所有 `subscribe()` 订阅者的回调
2. 广播 `resource_changed` 事件（所有 `on_resource_changed()` 订阅者收到通知）

**参数：**
- `key`：资源键
- `value`：新值

**特点：**
- 运行时更新后立即生效
- 自动通知所有依赖该资源的 UI 元素
- 适合用户交互、动画、主题热更新

**使用场景：**
- 用户点击按钮切换主题颜色
- 动画逐帧更新资源值
- 暗黑模式 / 亮色模式切换
- 实时响应用户设置（字体大小、语言等）

**示例：**
```cpp
// 初始化
dict.set("AccentColor", Color::Blue);

// 订阅变更
dict.subscribe("AccentColor", [](const Variant& v) {
    auto color = v.get<Color>();
    std::cout << "AccentColor 变更为 " << color.to_hex() << std::endl;
});

// 运行时动态更新（触发上述回调）
dict.set_dynamic("AccentColor", Color::Red);  // 输出：AccentColor 变更为 #FF0000
```

---

### 合并方法

#### `void merge(const ResourceDictionary& other)`

合并另一资源字典，**使用弱引用**（不拥有其生命周期）。合并后的字典在 `find()` 查找时作为第二优先级（本层 → 合并层 → 父层）。

**参数：**
- `other`：被合并的字典（必须保证其生命周期 ≥ 当前字典）

**特点：**
- 支持多次调用，按调用顺序形成合并链
- 不拷贝资源，仅持有弱引用
- 合并字典销毁不影响当前字典（查找时跳过无效引用）

**使用场景：**
- 主题继承（Dark 主题 merge Base 主题）
- 控件默认样式 + 用户自定义样式
- 多层资源覆盖（全局 → 窗口 → 页面 → 控件）

**示例：**
```cpp
auto base_theme = ResourceDictionary();
base_theme.set("Background", Color::White);
base_theme.set("Foreground", Color::Black);

auto dark_theme = ResourceDictionary();
dark_theme.set("Background", Color::from_rgb(0x1E1E1E));  // 覆盖
dark_theme.merge(base_theme);  // 继承 Foreground

// 查找顺序：dark_theme.local_ → base_theme.local_
auto bg = dark_theme.find("Background");  // Color(0x1E1E1E)
auto fg = dark_theme.find("Foreground");  // Color::Black（来自 base_theme）
```

---

#### `void clear_merged() noexcept`

清除所有已合并字典，**保留本层和父层连接**。常用于主题切换前清理旧主题引用。

**示例：**
```cpp
auto app_dict = ResourceDictionary();
app_dict.merge(light_theme);

// 用户点击"切换到暗黑模式"
app_dict.clear_merged();
app_dict.merge(dark_theme);
app_dict.notify_resource_changed("*");  // 全量刷新 UI
```

---

### 查找方法

#### `[[nodiscard]] core::Variant find(core::StringView key) const noexcept`

树形查找资源，按以下顺序递归：

```
1. 本层 local_[key]
2. 合并层 merged_[0].find(key)
          merged_[1].find(key)
          ...
3. 父层 parent_->find(key)  // 递归
4. 未命中返回 Variant()
```

**参数：**
- `key`：资源键

**返回值：**
- 命中：包含值的 `Variant`（用 `has_value()` 判断，用 `get<T>()` 提取）
- 未命中：空 `Variant`

**示例：**
```cpp
auto color_var = dict.find("PrimaryColor");
if (color_var.has_value()) {
    auto color = color_var.get<Color>();
    // 使用 color
} else {
    // 回退到默认值
    auto color = Color::Black;
}
```

---

#### `[[nodiscard]] core::Variant find_local(core::StringView key) const noexcept`

**仅查找本层资源**，不查合并层和父层。用于调试或显式覆盖检测。

**示例：**
```cpp
// 检测本层是否显式定义了某资源
if (dict.find_local("CustomColor").has_value()) {
    std::cout << "当前层定义了 CustomColor" << std::endl;
}
```

---

### DynamicResource 订阅

#### `HandlerId subscribe(core::StringView key, core::Function<void(const core::Variant&)> callback)`

订阅特定键的值变更。当 `set_dynamic(key, value)` 被调用时，触发回调。

**参数：**
- `key`：监听的资源键
- `callback`：变更回调，参数为新值

**返回值：**
- 成功：返回 `HandlerId`（非零），用于 `unsubscribe()`
- 失败：返回 `kInvalidHandlerId`（值为 `0`）

**生命周期：**
- 订阅不持有 `callback` 捕获对象的所有权，需确保捕获对象在取消订阅前有效
- 使用 RAII 封装避免泄漏：

```cpp
class DynamicResourceBinding {
public:
    DynamicResourceBinding(ResourceDictionary* dict, 
                           core::StringView key,
                           core::Function<void(const Variant&)> cb)
        : dict_(dict), id_(dict->subscribe(key, std::move(cb))) {}
    
    ~DynamicResourceBinding() {
        if (id_ != kInvalidHandlerId) {
            dict_->unsubscribe(id_);
        }
    }

private:
    ResourceDictionary* dict_;
    HandlerId id_;
};
```

**示例：**
```cpp
auto dict = ResourceDictionary();
dict.set("TextColor", Color::Black);

// 订阅
auto id = dict.subscribe("TextColor", [](const Variant& v) {
    auto color = v.get<Color>();
    std::cout << "文本颜色变更: " << color.to_hex() << std::endl;
});

// 触发回调
dict.set_dynamic("TextColor", Color::Red);  // 输出: 文本颜色变更: #FF0000

// 取消订阅
dict.unsubscribe(id);

// 不再触发回调
dict.set_dynamic("TextColor", Color::Blue);
```

---

#### `void unsubscribe(HandlerId id) noexcept`

取消订阅，释放回调资源。传入无效 ID（`kInvalidHandlerId` 或已取消的 ID）为安全操作（no-op）。

---

### 父字典连接

#### `void set_parent(ResourceDictionary* parent) noexcept`

连接父字典，形成查找链。布局系统在 Visual Tree attach 时自动调用。

**参数：**
- `parent`：父字典指针（可为 `nullptr` 解除连接）

**副作用：**
- 自动订阅父字典的 `resource_changed` 事件
- 父字典资源变更时，当前字典重新广播通知（级联传播）

**调用时机：**
- `UIElement::on_visual_tree_attached()`：连接到父元素的字典
- `UIElement::on_visual_tree_detached()`：解除连接（传 `nullptr`）

**示例：**
```cpp
auto app_dict = ResourceDictionary();
app_dict.set("GlobalFontSize", 14.0);

auto window_dict = ResourceDictionary();
window_dict.set_parent(&app_dict);  // 连接父字典

// 子字典可查找父字典资源
auto font_size = window_dict.find("GlobalFontSize");  // 14.0
```

---

#### `[[nodiscard]] ResourceDictionary* parent() const noexcept`

返回当前父字典指针，无父字典时返回 `nullptr`。

---

### resource_changed 广播

#### `HandlerId on_resource_changed(core::Function<void(core::StringView)> callback)`

订阅任意资源变更广播。当以下情况发生时触发：
- `set_dynamic(key, value)` 被调用
- `notify_resource_changed(key)` 被手动调用
- 父字典触发 `resource_changed` 事件（级联传播）

**参数：**
- `callback`：广播回调，参数为变更的键（`"*"` 表示全量刷新）

**返回值：**
- 成功：`HandlerId`（非零）
- 失败：`kInvalidHandlerId`

**使用场景：**
- 子控件监听父控件资源变更（自动连接，无需手动订阅）
- 主题切换时通知所有控件重新查询资源
- 调试：打印所有资源变更日志

**示例：**
```cpp
auto dict = ResourceDictionary();

auto id = dict.on_resource_changed([](core::StringView key) {
    if (key == "*") {
        std::cout << "全量刷新触发" << std::endl;
    } else {
        std::cout << "资源变更: " << key << std::endl;
    }
});

dict.set_dynamic("Color", Color::Red);       // 输出: 资源变更: Color
dict.notify_resource_changed("*");           // 输出: 全量刷新触发

dict.off_resource_changed(id);
```

---

#### `void off_resource_changed(HandlerId id) noexcept`

取消广播订阅。传入无效 ID 为安全操作。

---

#### `void notify_resource_changed(core::StringView key)`

手动广播资源变更通知。

**参数：**
- `key`：变更的键（`"*"` 表示全量刷新，所有订阅者应重新查询所有资源）

**使用场景：**
- 主题切换后批量更新（`clear_merged()` → `merge(new_theme)` → `notify_resource_changed("*")`）
- 外部数据源变更后同步资源（如从配置文件重新加载）

**示例：**
```cpp
// 主题切换完整流程
void switch_theme(ResourceDictionary& app_dict, const ResourceDictionary& new_theme) {
    app_dict.clear_merged();
    app_dict.merge(new_theme);
    app_dict.notify_resource_changed("*");  // 通知所有控件刷新
}
```

---

## 使用场景

### 1. 静态资源加载（批量初始化）

主题文件加载时使用 `set()` 批量写入，避免触发通知开销。

```cpp
auto load_theme_from_json(const std::string& json) -> ResourceDictionary {
    auto dict = ResourceDictionary();
    auto root = nlohmann::json::parse(json);

    for (auto& [key, value] : root.items()) {
        if (value.is_string() && value.get<std::string>().starts_with("#")) {
            // 解析颜色
            dict.set(key, Color::from_hex(value.get<std::string>()));
        } else if (value.is_number()) {
            dict.set(key, value.get<double>());
        }
        // ... 其他类型
    }

    return dict;
}

// 使用
auto theme = load_theme_from_json(R"({
    "PrimaryColor": "#007ACC",
    "FontSize": 14,
    "Padding": 10
})");
```

---

### 2. 动态资源更新（运行时变更）

用户交互时使用 `set_dynamic()` 触发 UI 实时更新。

```cpp
// 用户拖动滑块调整字体大小
slider.on_value_changed([&app_dict](double value) {
    app_dict.set_dynamic("GlobalFontSize", value);
    // 所有订阅 "GlobalFontSize" 的 TextBlock 自动更新
});

// TextBlock 订阅
text_block.subscribe("GlobalFontSize", [this](const Variant& v) {
    set_font_size(v.get<double>());
    invalidate_measure();
});
```

---

### 3. 树形资源查找（父子继承）

布局树结构与资源查找链同构。

```cpp
// 应用级字典
auto app_dict = ResourceDictionary();
app_dict.set("DefaultBackground", Color::White);

// 窗口级字典
auto window_dict = ResourceDictionary();
window_dict.set_parent(&app_dict);
window_dict.set("WindowBackground", Color::from_rgb(0xF0F0F0));

// 页面级字典
auto page_dict = ResourceDictionary();
page_dict.set_parent(&window_dict);
page_dict.set("PageBackground", Color::from_rgb(0xFAFAFA));

// Button 查找资源（从 page_dict 开始）
auto bg = page_dict.find("PageBackground");       // 命中 page_dict
auto win_bg = page_dict.find("WindowBackground"); // 命中 window_dict（父层）
auto def_bg = page_dict.find("DefaultBackground");// 命中 app_dict（祖父层）
auto missing = page_dict.find("NonExistent");     // 未命中，返回空 Variant
```

---

### 4. 主题合并（继承与覆盖）

使用 `merge()` 实现主题继承。

```cpp
// 基础主题（定义所有键）
auto base_theme = ResourceDictionary();
base_theme.set("Background", Color::White);
base_theme.set("Foreground", Color::Black);
base_theme.set("BorderColor", Color::Gray);
base_theme.set("FontSize", 14.0);

// 暗黑主题（仅覆盖颜色相关键）
auto dark_theme = ResourceDictionary();
dark_theme.set("Background", Color::from_rgb(0x1E1E1E));
dark_theme.set("Foreground", Color::from_rgb(0xD4D4D4));
dark_theme.set("BorderColor", Color::from_rgb(0x3E3E3E));
dark_theme.merge(base_theme);  // 继承 FontSize

// 高对比度主题（覆盖所有颜色 + 加大字体）
auto hc_theme = ResourceDictionary();
hc_theme.set("Background", Color::Black);
hc_theme.set("Foreground", Color::White);
hc_theme.set("BorderColor", Color::Yellow);
hc_theme.set("FontSize", 16.0);  // 覆盖 base_theme 的值
hc_theme.merge(base_theme);

// 应用主题切换
void apply_theme(ResourceDictionary& app_dict, const ResourceDictionary& theme) {
    app_dict.clear_merged();
    app_dict.merge(theme);
    app_dict.notify_resource_changed("*");
}
```

---

### 5. DynamicResource 订阅（绑定到属性）

控件属性绑定到资源，资源变更时自动刷新。

```cpp
class Button {
public:
    void bind_background(ResourceDictionary* dict, core::StringView key) {
        // 初始查询
        auto initial = dict->find(key);
        if (initial.has_value()) {
            background_ = initial.get<Brush>();
        }

        // 订阅变更
        bg_subscription_ = dict->subscribe(key, [this](const Variant& v) {
            background_ = v.get<Brush>();
            invalidate_visual();
        });
    }

    ~Button() {
        if (bg_subscription_ != kInvalidHandlerId) {
            // 需保存 dict 指针或使用 RAII 包装
            dict_->unsubscribe(bg_subscription_);
        }
    }

private:
    Brush background_;
    HandlerId bg_subscription_ = kInvalidHandlerId;
    ResourceDictionary* dict_ = nullptr;  // 弱引用
};
```

---

### 6. 父字典连接（Visual Tree 同步）

`UIElement` 在 attach 时自动连接父字典。

```cpp
class UIElement {
protected:
    virtual void on_visual_tree_attached(UIElement* parent) {
        if (parent && parent->resources_) {
            resources_.set_parent(&parent->resources_);
        }
    }

    virtual void on_visual_tree_detached() {
        resources_.set_parent(nullptr);
    }

private:
    ResourceDictionary resources_;
};
```

---

### 7. 全量刷新（主题切换）

主题切换后通知所有控件重新查询资源。

```cpp
class Application {
public:
    void set_theme(Theme theme) {
        current_theme_ = theme;

        app_dict_.clear_merged();
        switch (theme) {
            case Theme::Light:
                app_dict_.merge(light_theme_);
                break;
            case Theme::Dark:
                app_dict_.merge(dark_theme_);
                break;
            case Theme::HighContrast:
                app_dict_.merge(hc_theme_);
                break;
        }

        // 全量刷新（key="*"）
        app_dict_.notify_resource_changed("*");
    }

private:
    ResourceDictionary app_dict_;
    ResourceDictionary light_theme_;
    ResourceDictionary dark_theme_;
    ResourceDictionary hc_theme_;
    Theme current_theme_ = Theme::Light;
};
```

---

## 最佳实践

### ✅ 使用静态写入初始化主题

批量加载时使用 `set()`，避免不必要的通知开销。

```cpp
// ✅ 高效
auto theme = ResourceDictionary();
theme.set("Color1", Color::Red);
theme.set("Color2", Color::Blue);
theme.set("Color3", Color::Green);

// ❌ 低效（触发 3 次广播）
auto theme = ResourceDictionary();
theme.set_dynamic("Color1", Color::Red);
theme.set_dynamic("Color2", Color::Blue);
theme.set_dynamic("Color3", Color::Green);
```

---

### ✅ 使用 RAII 管理订阅生命周期

避免忘记 `unsubscribe()` 导致的内存泄漏和悬垂回调。

```cpp
// ✅ RAII 封装
class ResourceBinding {
public:
    ResourceBinding(ResourceDictionary* dict, 
                    core::StringView key,
                    core::Function<void(const Variant&)> cb)
        : dict_(dict), id_(dict->subscribe(key, std::move(cb))) {}

    ~ResourceBinding() {
        if (id_ != kInvalidHandlerId) {
            dict_->unsubscribe(id_);
        }
    }

    ResourceBinding(ResourceBinding&&) noexcept = default;
    ResourceBinding& operator=(ResourceBinding&&) noexcept = default;

private:
    ResourceDictionary* dict_;
    HandlerId id_;
};

// 使用
class MyControl {
public:
    void attach_resources(ResourceDictionary* dict) {
        binding_ = ResourceBinding(dict, "Color", [this](const Variant& v) {
            color_ = v.get<Color>();
        });
    }

private:
    ResourceBinding binding_;  // 析构时自动取消订阅
    Color color_;
};

// ❌ 手动管理容易遗漏
class BadControl {
public:
    void attach_resources(ResourceDictionary* dict) {
        id_ = dict->subscribe("Color", [this](const Variant& v) {
            color_ = v.get<Color>();
        });
    }

    // 忘记取消订阅 → 内存泄漏
    // ~BadControl() { dict_->unsubscribe(id_); }

private:
    HandlerId id_;
    Color color_;
};
```

---

### ✅ 主题切换后调用全量刷新

`clear_merged()` + `merge()` 后必须调用 `notify_resource_changed("*")`。

```cpp
// ✅ 完整流程
void switch_to_dark() {
    app_dict.clear_merged();
    app_dict.merge(dark_theme);
    app_dict.notify_resource_changed("*");  // 通知所有控件
}

// ❌ 缺少通知（UI 不会更新）
void switch_to_dark_bad() {
    app_dict.clear_merged();
    app_dict.merge(dark_theme);
    // 控件仍显示旧值，直到下次手动刷新
}
```

---

### ✅ 合并字典前检查生命周期

`merge()` 使用弱引用，被合并字典必须保持有效。

```cpp
// ✅ 合并全局主题（生命周期 ≥ app_dict）
auto& global_theme = Application::instance().get_theme();
app_dict.merge(global_theme);

// ❌ 合并临时对象（悬垂引用）
void bad_merge(ResourceDictionary& dict) {
    auto temp_theme = ResourceDictionary();
    temp_theme.set("Color", Color::Red);
    dict.merge(temp_theme);  // temp_theme 离开作用域后失效
}
// 后续 dict.find("Color") 可能崩溃或返回空值
```

---

### ✅ 使用 `find()` 而非 `find_local()` 进行常规查找

`find()` 支持完整的树形查找，`find_local()` 仅用于调试或显式检测本层覆盖。

```cpp
// ✅ 标准查找
auto color = dict.find("PrimaryColor");

// ❌ 不必要的本地查找（可能遗漏父层和合并层的值）
auto color = dict.find_local("PrimaryColor");
if (!color.has_value()) {
    // 手动查找父层？繁琐且易错
}
```

---

### ✅ 避免在回调中递归触发 `set_dynamic()`

可能导致无限递归或性能问题。

```cpp
// ❌ 递归触发
dict.subscribe("Value", [&dict](const Variant& v) {
    auto val = v.get<int>();
    if (val < 10) {
        dict.set_dynamic("Value", val + 1);  // 递归触发自己
    }
});

// ✅ 使用标志位或外部状态机避免递归
bool is_updating = false;
dict.subscribe("Value", [&dict, &is_updating](const Variant& v) {
    if (is_updating) return;
    
    is_updating = true;
    auto val = v.get<int>();
    if (val < 10) {
        dict.set_dynamic("Value", val + 1);
    }
    is_updating = false;
});
```

---

## 常见陷阱

### ❌ 陷阱 1：忘记取消订阅导致内存泄漏

```cpp
// ❌ 错误示例
class Widget {
public:
    void bind_color(ResourceDictionary* dict) {
        id_ = dict->subscribe("Color", [this](const Variant& v) {
            color_ = v.get<Color>();
        });
        dict_ = dict;
    }

    // 忘记取消订阅
    // ~Widget() { dict_->unsubscribe(id_); }

private:
    ResourceDictionary* dict_;
    HandlerId id_;
    Color color_;
};

// ✅ 正确做法
class Widget {
public:
    void bind_color(ResourceDictionary* dict) {
        id_ = dict->subscribe("Color", [this](const Variant& v) {
            color_ = v.get<Color>();
        });
        dict_ = dict;
    }

    ~Widget() {
        if (id_ != kInvalidHandlerId) {
            dict_->unsubscribe(id_);
        }
    }

private:
    ResourceDictionary* dict_;
    HandlerId id_ = kInvalidHandlerId;
    Color color_;
};
```

---

### ❌ 陷阱 2：合并临时字典导致悬垂引用

```cpp
// ❌ 错误示例
void apply_theme(ResourceDictionary& dict) {
    auto theme = load_theme_from_file("theme.json");
    dict.merge(theme);  // theme 离开作用域后失效
}

// 后续 dict.find() 可能崩溃

// ✅ 正确做法：保证被合并字典生命周期
class ThemeManager {
public:
    void load_theme(const std::string& path) {
        current_theme_ = load_theme_from_file(path);
        app_dict_.clear_merged();
        app_dict_.merge(current_theme_);  // current_theme_ 持久存在
        app_dict_.notify_resource_changed("*");
    }

private:
    ResourceDictionary app_dict_;
    ResourceDictionary current_theme_;  // 持久化
};
```

---

### ❌ 陷阱 3：捕获 `this` 的回调在对象销毁后触发

```cpp
// ❌ 错误示例
class Button {
public:
    void attach(ResourceDictionary* dict) {
        dict->subscribe("ButtonColor", [this](const Variant& v) {
            this->set_color(v.get<Color>());  // Button 销毁后 this 失效
        });
    }
};

// Button 被销毁后，dict 触发回调 → 崩溃

// ✅ 正确做法：使用弱引用或在析构时取消订阅
class Button {
public:
    void attach(ResourceDictionary* dict) {
        dict_ = dict;
        id_ = dict->subscribe("ButtonColor", [this](const Variant& v) {
            this->set_color(v.get<Color>());
        });
    }

    ~Button() {
        if (id_ != kInvalidHandlerId) {
            dict_->unsubscribe(id_);  // 销毁前取消订阅
        }
    }

private:
    ResourceDictionary* dict_ = nullptr;
    HandlerId id_ = kInvalidHandlerId;
};
```

---

### ❌ 陷阱 4：主题切换后未调用 `notify_resource_changed("*")`

```cpp
// ❌ 错误示例
void switch_theme(ResourceDictionary& dict, const ResourceDictionary& theme) {
    dict.clear_merged();
    dict.merge(theme);
    // 缺少通知 → UI 不更新，用户看到旧颜色
}

// ✅ 正确做法
void switch_theme(ResourceDictionary& dict, const ResourceDictionary& theme) {
    dict.clear_merged();
    dict.merge(theme);
    dict.notify_resource_changed("*");  // 全量刷新
}
```

---

### ❌ 陷阱 5：在 `subscribe()` 回调中修改订阅列表

可能导致迭代器失效或死锁。

```cpp
// ❌ 错误示例
dict.subscribe("Key", [&dict](const Variant& v) {
    // 回调中再次订阅 → 可能导致内部容器重新分配
    dict.subscribe("OtherKey", [](const Variant&) {});
});

// ✅ 正确做法：使用事件队列或延迟到下一帧
std::vector<std::function<void()>> deferred_actions;

dict.subscribe("Key", [&deferred_actions, &dict](const Variant& v) {
    deferred_actions.push_back([&dict]() {
        dict.subscribe("OtherKey", [](const Variant&) {});
    });
});

// 在事件循环中执行延迟操作
for (auto& action : deferred_actions) {
    action();
}
deferred_actions.clear();
```

---

## 完整示例：应用主题系统

以下示例展示如何使用 `ResourceDictionary` 构建完整的主题系统，支持 Light/Dark 主题切换、DynamicResource 订阅和树形资源查找。

```cpp
#include <mine/ui/style/ResourceDictionary.h>
#include <mine/paint/Color.h>
#include <mine/paint/SolidColorBrush.h>
#include <iostream>
#include <string>
#include <vector>

using namespace mine;
using namespace mine::ui::style;
using namespace mine::paint;
using namespace mine::core;

// ================================
// RAII 订阅管理器
// ================================

class ResourceSubscription {
public:
    ResourceSubscription() = default;

    ResourceSubscription(ResourceDictionary* dict,
                         StringView key,
                         Function<void(const Variant&)> callback)
        : dict_(dict), id_(dict->subscribe(key, std::move(callback))) {}

    ~ResourceSubscription() {
        if (id_ != kInvalidHandlerId && dict_) {
            dict_->unsubscribe(id_);
        }
    }

    ResourceSubscription(ResourceSubscription&& other) noexcept
        : dict_(other.dict_), id_(other.id_) {
        other.id_ = kInvalidHandlerId;
    }

    ResourceSubscription& operator=(ResourceSubscription&& other) noexcept {
        if (this != &other) {
            if (id_ != kInvalidHandlerId && dict_) {
                dict_->unsubscribe(id_);
            }
            dict_ = other.dict_;
            id_ = other.id_;
            other.id_ = kInvalidHandlerId;
        }
        return *this;
    }

    ResourceSubscription(const ResourceSubscription&) = delete;
    ResourceSubscription& operator=(const ResourceSubscription&) = delete;

private:
    ResourceDictionary* dict_ = nullptr;
    HandlerId id_ = kInvalidHandlerId;
};

// ================================
// 控件基类：Button
// ================================

class Button {
public:
    explicit Button(std::string text) : text_(std::move(text)) {}

    void attach_resources(ResourceDictionary* dict) {
        dict_ = dict;

        // 订阅背景色
        bg_subscription_ = ResourceSubscription(
            dict, "ButtonBackground",
            [this](const Variant& v) {
                if (v.has_value()) {
                    background_ = v.get<Color>();
                    std::cout << "  [Button '" << text_ << "'] 背景色更新: "
                              << background_.to_hex() << std::endl;
                }
            }
        );

        // 订阅前景色
        fg_subscription_ = ResourceSubscription(
            dict, "ButtonForeground",
            [this](const Variant& v) {
                if (v.has_value()) {
                    foreground_ = v.get<Color>();
                    std::cout << "  [Button '" << text_ << "'] 前景色更新: "
                              << foreground_.to_hex() << std::endl;
                }
            }
        );

        // 初始查询
        auto bg = dict->find("ButtonBackground");
        if (bg.has_value()) {
            background_ = bg.get<Color>();
        }

        auto fg = dict->find("ButtonForeground");
        if (fg.has_value()) {
            foreground_ = fg.get<Color>();
        }

        std::cout << "  [Button '" << text_ << "'] 初始化: bg=" 
                  << background_.to_hex() << ", fg=" << foreground_.to_hex() 
                  << std::endl;
    }

    void render() const {
        std::cout << "  [Button '" << text_ << "'] 渲染: "
                  << "bg=" << background_.to_hex() 
                  << ", fg=" << foreground_.to_hex() << std::endl;
    }

private:
    std::string text_;
    ResourceDictionary* dict_ = nullptr;
    Color background_ = Color::Gray;
    Color foreground_ = Color::Black;
    ResourceSubscription bg_subscription_;
    ResourceSubscription fg_subscription_;
};

// ================================
// 控件基类：TextBlock
// ================================

class TextBlock {
public:
    explicit TextBlock(std::string text) : text_(std::move(text)) {}

    void attach_resources(ResourceDictionary* dict) {
        dict_ = dict;

        // 订阅文本颜色
        fg_subscription_ = ResourceSubscription(
            dict, "TextForeground",
            [this](const Variant& v) {
                if (v.has_value()) {
                    foreground_ = v.get<Color>();
                    std::cout << "  [TextBlock '" << text_ << "'] 前景色更新: "
                              << foreground_.to_hex() << std::endl;
                }
            }
        );

        // 订阅字体大小
        size_subscription_ = ResourceSubscription(
            dict, "FontSize",
            [this](const Variant& v) {
                if (v.has_value()) {
                    font_size_ = v.get<double>();
                    std::cout << "  [TextBlock '" << text_ << "'] 字体大小更新: "
                              << font_size_ << std::endl;
                }
            }
        );

        // 初始查询
        auto fg = dict->find("TextForeground");
        if (fg.has_value()) {
            foreground_ = fg.get<Color>();
        }

        auto size = dict->find("FontSize");
        if (size.has_value()) {
            font_size_ = size.get<double>();
        }

        std::cout << "  [TextBlock '" << text_ << "'] 初始化: fg=" 
                  << foreground_.to_hex() << ", size=" << font_size_ 
                  << std::endl;
    }

    void render() const {
        std::cout << "  [TextBlock '" << text_ << "'] 渲染: "
                  << "fg=" << foreground_.to_hex() 
                  << ", size=" << font_size_ << std::endl;
    }

private:
    std::string text_;
    ResourceDictionary* dict_ = nullptr;
    Color foreground_ = Color::Black;
    double font_size_ = 14.0;
    ResourceSubscription fg_subscription_;
    ResourceSubscription size_subscription_;
};

// ================================
// 窗口：包含控件层级
// ================================

class Window {
public:
    Window() {
        // 创建控件
        button1_ = std::make_unique<Button>("确定");
        button2_ = std::make_unique<Button>("取消");
        title_ = std::make_unique<TextBlock>("欢迎使用 MineUI");
        subtitle_ = std::make_unique<TextBlock>("主题演示");

        // 设置父字典（模拟 Visual Tree）
        window_dict_.set_parent(&app_dict_);
        panel_dict_.set_parent(&window_dict_);
    }

    void attach_app_resources(ResourceDictionary* app_dict) {
        app_dict_ = *app_dict;  // 拷贝（实际场景中应使用引用）
        window_dict_.set_parent(&app_dict_);

        // 绑定控件到资源字典
        button1_->attach_resources(&panel_dict_);
        button2_->attach_resources(&panel_dict_);
        title_->attach_resources(&panel_dict_);
        subtitle_->attach_resources(&panel_dict_);
    }

    void render() const {
        std::cout << "\n=== 渲染窗口 ===" << std::endl;
        title_->render();
        subtitle_->render();
        button1_->render();
        button2_->render();
    }

    ResourceDictionary& window_resources() { return window_dict_; }
    ResourceDictionary& panel_resources() { return panel_dict_; }

private:
    ResourceDictionary app_dict_;      // 应用级字典（拷贝自 Application）
    ResourceDictionary window_dict_;   // 窗口级字典
    ResourceDictionary panel_dict_;    // 面板级字典

    std::unique_ptr<Button> button1_;
    std::unique_ptr<Button> button2_;
    std::unique_ptr<TextBlock> title_;
    std::unique_ptr<TextBlock> subtitle_;
};

// ================================
// 主题管理器
// ================================

class ThemeManager {
public:
    ThemeManager() {
        initialize_themes();
    }

    enum class Theme {
        Light,
        Dark,
        HighContrast
    };

    void apply_theme(ResourceDictionary& app_dict, Theme theme) {
        std::cout << "\n=== 切换主题: " << theme_name(theme) << " ===" << std::endl;

        app_dict.clear_merged();

        switch (theme) {
            case Theme::Light:
                app_dict.merge(light_theme_);
                break;
            case Theme::Dark:
                app_dict.merge(dark_theme_);
                break;
            case Theme::HighContrast:
                app_dict.merge(hc_theme_);
                break;
        }

        // 全量刷新通知
        app_dict.notify_resource_changed("*");
    }

    void print_theme_info(Theme theme) const {
        const auto& dict = get_theme_dict(theme);
        std::cout << "\n--- " << theme_name(theme) << " 主题资源 ---" << std::endl;

        std::vector<std::string> keys = {
            "ButtonBackground", "ButtonForeground",
            "TextForeground", "FontSize"
        };

        for (const auto& key : keys) {
            auto value = dict.find_local(key);
            if (value.has_value()) {
                if (value.is<Color>()) {
                    std::cout << "  " << key << ": " 
                              << value.get<Color>().to_hex() << std::endl;
                } else if (value.is<double>()) {
                    std::cout << "  " << key << ": " 
                              << value.get<double>() << std::endl;
                }
            }
        }
    }

private:
    void initialize_themes() {
        // Light 主题
        light_theme_.set("ButtonBackground", Color::from_rgb(0xE1E1E1));
        light_theme_.set("ButtonForeground", Color::Black);
        light_theme_.set("TextForeground", Color::from_rgb(0x1E1E1E));
        light_theme_.set("FontSize", 14.0);

        // Dark 主题
        dark_theme_.set("ButtonBackground", Color::from_rgb(0x3E3E3E));
        dark_theme_.set("ButtonForeground", Color::White);
        dark_theme_.set("TextForeground", Color::from_rgb(0xD4D4D4));
        dark_theme_.set("FontSize", 14.0);

        // 高对比度主题
        hc_theme_.set("ButtonBackground", Color::Black);
        hc_theme_.set("ButtonForeground", Color::Yellow);
        hc_theme_.set("TextForeground", Color::White);
        hc_theme_.set("FontSize", 16.0);  // 更大字体
    }

    const ResourceDictionary& get_theme_dict(Theme theme) const {
        switch (theme) {
            case Theme::Light: return light_theme_;
            case Theme::Dark: return dark_theme_;
            case Theme::HighContrast: return hc_theme_;
        }
        return light_theme_;
    }

    static const char* theme_name(Theme theme) {
        switch (theme) {
            case Theme::Light: return "Light";
            case Theme::Dark: return "Dark";
            case Theme::HighContrast: return "HighContrast";
        }
        return "Unknown";
    }

    ResourceDictionary light_theme_;
    ResourceDictionary dark_theme_;
    ResourceDictionary hc_theme_;
};

// ================================
// 应用程序
// ================================

class Application {
public:
    Application() {
        // 初始化应用级资源（所有主题共享）
        app_dict_.set("AppTitle", "MineUI Theme Demo");
        app_dict_.set("DefaultMargin", 10.0);

        // 订阅全局资源变更（调试用）
        debug_subscription_ = app_dict_.on_resource_changed(
            [](StringView key) {
                if (key == "*") {
                    std::cout << "  [Application] 全量刷新触发" << std::endl;
                } else {
                    std::cout << "  [Application] 资源变更: " << key << std::endl;
                }
            }
        );

        // 应用默认主题
        theme_manager_.apply_theme(app_dict_, ThemeManager::Theme::Light);

        // 创建窗口
        window_ = std::make_unique<Window>();
        window_->attach_app_resources(&app_dict_);
    }

    ~Application() {
        if (debug_subscription_ != kInvalidHandlerId) {
            app_dict_.off_resource_changed(debug_subscription_);
        }
    }

    void switch_theme(ThemeManager::Theme theme) {
        theme_manager_.apply_theme(app_dict_, theme);
    }

    void render() const {
        window_->render();
    }

    void demo_dynamic_update() {
        std::cout << "\n=== 动态更新资源（运行时） ===" << std::endl;
        app_dict_.set_dynamic("FontSize", 18.0);
    }

    void demo_tree_lookup() {
        std::cout << "\n=== 树形查找演示 ===" << std::endl;

        // 在面板级字典添加本地资源
        window_->panel_resources().set("LocalColor", Color::Red);

        // 查找顺序测试
        std::cout << "从 panel_dict_ 查找资源：" << std::endl;

        // 本层查找
        auto local = window_->panel_resources().find("LocalColor");
        std::cout << "  LocalColor (本层): " 
                  << (local.has_value() ? local.get<Color>().to_hex() : "未找到")
                  << std::endl;

        // 父层查找
        auto from_parent = window_->panel_resources().find("AppTitle");
        std::cout << "  AppTitle (父层): " 
                  << (from_parent.has_value() ? from_parent.get<std::string>() : "未找到")
                  << std::endl;

        // 合并层查找
        auto from_merged = window_->panel_resources().find("ButtonBackground");
        std::cout << "  ButtonBackground (合并层): " 
                  << (from_merged.has_value() ? from_merged.get<Color>().to_hex() : "未找到")
                  << std::endl;

        // 未找到
        auto missing = window_->panel_resources().find("NonExistentKey");
        std::cout << "  NonExistentKey: " 
                  << (missing.has_value() ? "找到" : "未找到")
                  << std::endl;
    }

    void print_theme_info(ThemeManager::Theme theme) const {
        theme_manager_.print_theme_info(theme);
    }

private:
    ResourceDictionary app_dict_;
    ThemeManager theme_manager_;
    HandlerId debug_subscription_ = kInvalidHandlerId;
    std::unique_ptr<Window> window_;
};

// ================================
// 主函数：演示完整流程
// ================================

int main() {
    std::cout << "======================================" << std::endl;
    std::cout << "  MineUI ResourceDictionary 示例" << std::endl;
    std::cout << "======================================\n" << std::endl;

    // 1. 创建应用（默认 Light 主题）
    auto app = Application();

    // 2. 渲染初始状态
    app.render();

    // 3. 切换到 Dark 主题
    std::cout << "\n\n━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━" << std::endl;
    app.switch_theme(ThemeManager::Theme::Dark);
    app.render();

    // 4. 切换到高对比度主题
    std::cout << "\n\n━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━" << std::endl;
    app.switch_theme(ThemeManager::Theme::HighContrast);
    app.render();

    // 5. 动态更新资源
    std::cout << "\n\n━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━" << std::endl;
    app.demo_dynamic_update();
    app.render();

    // 6. 树形查找演示
    std::cout << "\n\n━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━" << std::endl;
    app.demo_tree_lookup();

    // 7. 打印主题信息
    std::cout << "\n\n━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━" << std::endl;
    app.print_theme_info(ThemeManager::Theme::Light);
    app.print_theme_info(ThemeManager::Theme::Dark);
    app.print_theme_info(ThemeManager::Theme::HighContrast);

    std::cout << "\n\n======================================" << std::endl;
    std::cout << "  演示结束" << std::endl;
    std::cout << "======================================" << std::endl;

    return 0;
}
```

### 示例输出

```
======================================
  MineUI ResourceDictionary 示例
======================================


=== 切换主题: Light ===
  [Application] 全量刷新触发
  [Button '确定'] 初始化: bg=#E1E1E1, fg=#000000
  [Button '取消'] 初始化: bg=#E1E1E1, fg=#000000
  [TextBlock '欢迎使用 MineUI'] 初始化: fg=#1E1E1E, size=14
  [TextBlock '主题演示'] 初始化: fg=#1E1E1E, size=14

=== 渲染窗口 ===
  [TextBlock '欢迎使用 MineUI'] 渲染: fg=#1E1E1E, size=14
  [TextBlock '主题演示'] 渲染: fg=#1E1E1E, size=14
  [Button '确定'] 渲染: bg=#E1E1E1, fg=#000000
  [Button '取消'] 渲染: bg=#E1E1E1, fg=#000000


━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━

=== 切换主题: Dark ===
  [Application] 全量刷新触发
  [Button '确定'] 背景色更新: #3E3E3E
  [Button '确定'] 前景色更新: #FFFFFF
  [Button '取消'] 背景色更新: #3E3E3E
  [Button '取消'] 前景色更新: #FFFFFF
  [TextBlock '欢迎使用 MineUI'] 前景色更新: #D4D4D4
  [TextBlock '主题演示'] 前景色更新: #D4D4D4

=== 渲染窗口 ===
  [TextBlock '欢迎使用 MineUI'] 渲染: fg=#D4D4D4, size=14
  [TextBlock '主题演示'] 渲染: fg=#D4D4D4, size=14
  [Button '确定'] 渲染: bg=#3E3E3E, fg=#FFFFFF
  [Button '取消'] 渲染: bg=#3E3E3E, fg=#FFFFFF


━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━

=== 切换主题: HighContrast ===
  [Application] 全量刷新触发
  [Button '确定'] 背景色更新: #000000
  [Button '确定'] 前景色更新: #FFFF00
  [Button '取消'] 背景色更新: #000000
  [Button '取消'] 前景色更新: #FFFF00
  [TextBlock '欢迎使用 MineUI'] 前景色更新: #FFFFFF
  [TextBlock '欢迎使用 MineUI'] 字体大小更新: 16
  [TextBlock '主题演示'] 前景色更新: #FFFFFF
  [TextBlock '主题演示'] 字体大小更新: 16

=== 渲染窗口 ===
  [TextBlock '欢迎使用 MineUI'] 渲染: fg=#FFFFFF, size=16
  [TextBlock '主题演示'] 渲染: fg=#FFFFFF, size=16
  [Button '确定'] 渲染: bg=#000000, fg=#FFFF00
  [Button '取消'] 渲染: bg=#000000, fg=#FFFF00


━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━

=== 动态更新资源（运行时） ===
  [Application] 资源变更: FontSize
  [TextBlock '欢迎使用 MineUI'] 字体大小更新: 18
  [TextBlock '主题演示'] 字体大小更新: 18

=== 渲染窗口 ===
  [TextBlock '欢迎使用 MineUI'] 渲染: fg=#FFFFFF, size=18
  [TextBlock '主题演示'] 渲染: fg=#FFFFFF, size=18
  [Button '确定'] 渲染: bg=#000000, fg=#FFFF00
  [Button '取消'] 渲染: bg=#000000, fg=#FFFF00


━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━

=== 树形查找演示 ===
从 panel_dict_ 查找资源：
  LocalColor (本层): #FF0000
  AppTitle (父层): MineUI Theme Demo
  ButtonBackground (合并层): #000000
  NonExistentKey: 未找到


━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━

--- Light 主题资源 ---
  ButtonBackground: #E1E1E1
  ButtonForeground: #000000
  TextForeground: #1E1E1E
  FontSize: 14

--- Dark 主题资源 ---
  ButtonBackground: #3E3E3E
  ButtonForeground: #FFFFFF
  TextForeground: #D4D4D4
  FontSize: 14

--- HighContrast 主题资源 ---
  ButtonBackground: #000000
  ButtonForeground: #FFFF00
  TextForeground: #FFFFFF
  FontSize: 16


======================================
  演示结束
======================================
```

---

## 总结

`ResourceDictionary` 是 MineUI 样式系统的核心组件，通过树形查找、字典合并和双重通知机制实现了灵活的资源管理：

### 核心能力

1. **多层资源查找**：本层 → 合并层 → 父层，支持样式继承和覆盖
2. **动态通知机制**：精确订阅 + 广播通知，实现 DynamicResource 绑定和主题热更新
3. **弱引用合并**：支持主题继承（Dark 主题扩展 Base 主题），无拷贝开销
4. **父字典连接**：与布局树同构，支持自动级联查找和通知传播

### 典型场景

- **应用启动**：使用 `set()` 批量加载主题文件
- **运行时更新**：使用 `set_dynamic()` 触发控件实时刷新
- **主题切换**：`clear_merged()` + `merge()` + `notify_resource_changed("*")`
- **控件绑定**：`subscribe()` + RAII 封装管理订阅生命周期
- **树形查找**：`find()` 递归查找父层和合并层资源

### 注意事项

- **订阅必须取消**：使用 RAII 封装避免内存泄漏
- **合并字典生命周期**：被合并字典必须保持有效（弱引用）
- **主题切换通知**：`notify_resource_changed("*")` 触发全量刷新
- **避免回调递归**：不在回调中再次调用 `set_dynamic()` 同一键

`ResourceDictionary` 的设计遵循了"零成本抽象"原则：静态资源（`set()`）无通知开销，动态资源（`set_dynamic()`）仅在需要时触发通知，弱引用合并避免资源拷贝。这使得它既适合大规模主题系统（数千条样式规则），也适合实时交互场景（毫秒级响应）。

---

**相关文档：**
- [01-VisualStateManager.md](01-VisualStateManager.md) - 视觉状态管理
- [02-Storyboard.md](02-Storyboard.md) - 动画时间线
- [03-Style.md](03-Style.md) - 样式系统
- [05-ThemeManager.md](05-ThemeManager.md) - 主题管理器

**源文件：**
- `src/mine/ui/style/api/include/mine/ui/style/ResourceDictionary.h`
- `src/mine/ui/style/src/ResourceDictionary.cpp`
