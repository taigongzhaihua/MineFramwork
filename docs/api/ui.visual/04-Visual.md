# Visual 详细接口文档

## 概述

`Visual` 是 `mine.ui.visual` 模块的**视觉树基类**，是所有可见 UI 元素的共同基础。

**核心特性：**
- **视觉树管理**：父子节点关系（非拥有指针）
- **局部仿射变换**：RenderTransform（math::Transform2D）
- **多种裁剪**：矩形/圆角矩形/四角独立圆角
- **依赖属性**：Opacity（不透明度）、Visibility（可见性）
- **脏区追踪**：invalidate_render() 标记渲染失效
- **渲染管线**：render_to_canvas() 递归绘制到 paint::Canvas
- **路由事件**：实现 IRoutedEventTarget 接口

**继承关系：**
```
DependencyObject (mine.ui.property)
    └─ Visual (mine.ui.visual)
        └─ UIElement
            └─ Control
```

---

## 文件位置

```
src/mine/ui/visual/api/include/mine/ui/visual/Visual.h
```

---

## 类定义

```cpp
class MINE_UI_VISUAL_API Visual : public DependencyObject,
                                   public IRoutedEventTarget {
public:
    // 依赖属性（静态描述符）
    static const DependencyProperty& OpacityProperty;
    static const DependencyProperty& VisibilityProperty;

    // 生命周期
    Visual();
    ~Visual() override;
    Visual(const Visual&)            = delete;
    Visual& operator=(const Visual&) = delete;
    Visual(Visual&&)            = default;
    Visual& operator=(Visual&&) = default;

    // 视觉树管理
    [[nodiscard]] Visual* parent() const noexcept;
    [[nodiscard]] uint32_t child_count() const noexcept;
    [[nodiscard]] Visual* child_at(uint32_t index) const noexcept;
    void add_child(Visual* child);
    void remove_child(Visual* child);
    void remove_all_children();

    // 局部变换
    [[nodiscard]] const math::Transform2D& render_transform() const noexcept;
    void set_render_transform(const math::Transform2D& t);

    // 矩形裁剪
    [[nodiscard]] bool has_clip_rect() const noexcept;
    [[nodiscard]] math::Rect clip_rect() const noexcept;
    void set_clip_rect(math::Rect rect);
    void clear_clip_rect();

    // 圆角矩形裁剪
    void set_clip_rounded_rect(math::RoundedRect rrect);
    [[nodiscard]] bool has_clip_rounded_rect() const noexcept;
    [[nodiscard]] math::RoundedRect clip_rounded_rect() const noexcept;
    void clear_clip_rounded_rect();

    // 四角独立圆角裁剪
    void set_clip_complex_rounded_rect(math::ComplexRoundedRect rrect);
    [[nodiscard]] bool has_clip_complex_rounded_rect() const noexcept;
    [[nodiscard]] math::ComplexRoundedRect clip_complex_rounded_rect() const noexcept;
    void clear_clip_complex_rounded_rect();

    // 快捷属性访问器（依赖属性）
    [[nodiscard]] float opacity() const;
    void set_opacity(float v);
    [[nodiscard]] Visibility visibility() const;
    void set_visibility(Visibility v);

    // 脏区管理
    [[nodiscard]] bool is_render_dirty() const noexcept;
    void invalidate_render() override;

    // 渲染管线
    void render_to_canvas(paint::Canvas& canvas);

    // 路由事件处理器管理
    void add_handler(const RoutedEvent& event,
                     RoutedEventHandlerFn fn,
                     void* user_data = nullptr);
    void remove_handler(const RoutedEvent& event,
                        RoutedEventHandlerFn fn,
                        void* user_data = nullptr);

    // IRoutedEventTarget 接口实现
    [[nodiscard]] IRoutedEventTarget* parent_target() const noexcept override;
    void invoke_handlers(const RoutedEvent& event,
                          RoutedEventArgs&   args) noexcept override;

    // 无 RTTI 类型探查
    [[nodiscard]] virtual UIElement* as_element() noexcept;

protected:
    virtual void on_parent_changed(Visual* old_parent, Visual* new_parent) noexcept;
    virtual void on_render(paint::Canvas& canvas);
    void on_property_changed(const DependencyProperty& prop,
                             const core::Variant&      old_value,
                             const core::Variant&      new_value) override;

private:
    struct Impl;
    core::Pimpl<Impl> p_;
};
```

---

## 依赖属性

### OpacityProperty

```cpp
static const DependencyProperty& OpacityProperty;
```

**描述**：不透明度属性，值类型 `float`，范围 `[0.0, 1.0]`，默认 `1.0`。

**特征**：
- 变更后触发 `invalidate_render()`
- 当前 M1.1 阶段影响脏区标记
- 像素级混合由 mine.compose 在后续里程碑实现

**示例**：
```cpp
// 通过依赖属性系统设置
visual.set_value(Visual::OpacityProperty, core::Variant::from_float(0.5f));

// 通过快捷访问器设置（推荐）
visual.set_opacity(0.5f);
```

---

### VisibilityProperty

```cpp
static const DependencyProperty& VisibilityProperty;
```

**描述**：可见性属性，值类型 `Visibility`，默认 `Visibility::Visible`。

**特征**：
- `Collapsed` 时跳过 `render_to_canvas`，也不参与布局
- `Hidden` 时跳过渲染但仍占据布局空间
- 变更后触发 `invalidate_render()`

**示例**：
```cpp
// 通过依赖属性系统设置
visual.set_value(Visual::VisibilityProperty, 
                 core::Variant::from_enum(Visibility::Collapsed));

// 通过快捷访问器设置（推荐）
visual.set_visibility(Visibility::Collapsed);
```

---

## 生命周期

### Visual

```cpp
Visual();
```

**描述**：构造 Visual 对象。

**示例**：
```cpp
Visual visual;
```

---

### ~Visual

```cpp
~Visual() override;
```

**描述**：析构 Visual 对象。

**行为**：
- 自动从父节点移除
- 清空所有子节点的父指针
- 释放内部资源

---

## 视觉树管理

### parent

```cpp
[[nodiscard]] Visual* parent() const noexcept;
```

**描述**：返回父节点指针。

**返回值**：
- 父节点指针（若当前为根节点则返回 `nullptr`）

**示例**：
```cpp
Visual* p = visual.parent();
if (p == nullptr) {
    std::cout << "visual 是根节点" << std::endl;
}
```

---

### child_count

```cpp
[[nodiscard]] uint32_t child_count() const noexcept;
```

**描述**：返回直接子节点数量。

**返回值**：
- 直接子节点数量

**示例**：
```cpp
uint32_t count = visual.child_count();
std::cout << "子节点数量: " << count << std::endl;
```

---

### child_at

```cpp
[[nodiscard]] Visual* child_at(uint32_t index) const noexcept;
```

**描述**：按索引返回直接子节点指针。

**参数**：
- `index`：子节点索引（0-based）

**返回值**：
- 子节点指针（不做越界检查，Debug 模式下断言）

**示例**：
```cpp
for (uint32_t i = 0; i < visual.child_count(); ++i) {
    Visual* child = visual.child_at(i);
    // 处理子节点
}
```

---

### add_child

```cpp
void add_child(Visual* child);
```

**描述**：将 `child` 添加为当前节点的最后一个子节点。

**参数**：
- `child`：要添加的子节点指针

**前置条件**：
- `child != nullptr`
- `child != this`（不能成为自身子节点）
- `child->parent() == nullptr`（child 尚未挂载到其他父节点）

**示例**：
```cpp
Visual parent;
Visual child;

parent.add_child(&child);

// child.parent() == &parent
// parent.child_count() == 1
// parent.child_at(0) == &child
```

---

### remove_child

```cpp
void remove_child(Visual* child);
```

**描述**：从子节点列表中移除 `child`。

**参数**：
- `child`：要移除的子节点指针

**行为**：
- 若 `child` 不在列表中则为空操作
- 移除后 `child->parent()` 置为 `nullptr`

**示例**：
```cpp
parent.remove_child(&child);

// child.parent() == nullptr
// parent.child_count() 减少 1
```

---

### remove_all_children

```cpp
void remove_all_children();
```

**描述**：移除所有子节点，并将其父指针置为 `nullptr`。

**示例**：
```cpp
parent.remove_all_children();

// parent.child_count() == 0
// 所有原子节点的 parent() == nullptr
```

---

## 局部变换

### render_transform

```cpp
[[nodiscard]] const math::Transform2D& render_transform() const noexcept;
```

**描述**：返回当前局部仿射变换。

**返回值**：
- 局部仿射变换（`math::Transform2D`）

**用途**：
- `render_to_canvas()` 中通过 `Canvas::transform()` 应用
- `UIElement::hit_test()` 中通过逆变换映射坐标

**示例**：
```cpp
const math::Transform2D& t = visual.render_transform();
```

---

### set_render_transform

```cpp
void set_render_transform(const math::Transform2D& t);
```

**描述**：设置局部仿射变换并触发渲染失效。

**参数**：
- `t`：仿射变换矩阵

**示例**：
```cpp
// 平移变换
math::Transform2D t = math::Transform2D::make_translate(100.0, 50.0);
visual.set_render_transform(t);

// 旋转变换
math::Transform2D t2 = math::Transform2D::make_rotate(M_PI / 4);  // 45 度
visual.set_render_transform(t2);

// 缩放变换
math::Transform2D t3 = math::Transform2D::make_scale(2.0, 2.0);
visual.set_render_transform(t3);

// 组合变换
math::Transform2D t4 = math::Transform2D::make_translate(100, 50)
                       * math::Transform2D::make_rotate(M_PI / 4);
visual.set_render_transform(t4);
```

---

## 矩形裁剪

### has_clip_rect

```cpp
[[nodiscard]] bool has_clip_rect() const noexcept;
```

**描述**：返回当前是否有矩形裁剪区域。

**返回值**：
- `true`：有矩形裁剪
- `false`：无矩形裁剪

---

### clip_rect

```cpp
[[nodiscard]] math::Rect clip_rect() const noexcept;
```

**描述**：返回当前矩形裁剪区域。

**返回值**：
- 矩形裁剪区域（仅 `has_clip_rect() == true` 时有效）

**注意**：调用前应先检查 `has_clip_rect()`

---

### set_clip_rect

```cpp
void set_clip_rect(math::Rect rect);
```

**描述**：设置矩形裁剪区域并触发渲染失效。

**参数**：
- `rect`：裁剪矩形（局部坐标系）

**行为**：
- 与 `Canvas::save/restore` 结合使用
- 仅对本节点及子树有效
- 与圆角矩形裁剪互斥（调用此方法会替换圆角裁剪）

**示例**：
```cpp
math::Rect clip{0, 0, 100, 100};
visual.set_clip_rect(clip);
```

---

### clear_clip_rect

```cpp
void clear_clip_rect();
```

**描述**：清除矩形裁剪区域并触发渲染失效。

**行为**：
- 仅当当前裁剪类型为矩形时生效
- 不影响圆角矩形裁剪

---

## 圆角矩形裁剪

### set_clip_rounded_rect

```cpp
void set_clip_rounded_rect(math::RoundedRect rrect);
```

**描述**：设置圆角矩形裁剪区域并触发渲染失效。

**参数**：
- `rrect`：圆角矩形区域（局部坐标系）

**行为**：
- 与矩形裁剪互斥（调用此方法会替换矩形裁剪）
- 同时影响本节点自身渲染及所有子节点
- 自动作为 UIElement 命中测试的默认边界形状

**示例**：
```cpp
math::RoundedRect rrect{math::Rect{0, 0, 100, 100}, 10.0f};  // 圆角半径 10
visual.set_clip_rounded_rect(rrect);
```

---

### has_clip_rounded_rect

```cpp
[[nodiscard]] bool has_clip_rounded_rect() const noexcept;
```

**描述**：返回当前是否有圆角矩形裁剪区域。

---

### clip_rounded_rect

```cpp
[[nodiscard]] math::RoundedRect clip_rounded_rect() const noexcept;
```

**描述**：返回当前圆角矩形裁剪区域。

**返回值**：
- 圆角矩形裁剪区域（仅 `has_clip_rounded_rect() == true` 时有效）

---

### clear_clip_rounded_rect

```cpp
void clear_clip_rounded_rect();
```

**描述**：清除圆角矩形裁剪区域并触发渲染失效。

---

## 四角独立圆角裁剪

### set_clip_complex_rounded_rect

```cpp
void set_clip_complex_rounded_rect(math::ComplexRoundedRect rrect);
```

**描述**：设置四角独立圆角裁剪区域并触发渲染失效。

**参数**：
- `rrect`：四角独立圆角矩形区域（局部坐标系）

**行为**：
- 与矩形裁剪和统一圆角裁剪互斥
- 同时作为 UIElement 命中测试的默认边界形状

**示例**：
```cpp
// 四角独立圆角：左上 10，右上 20，右下 15，左下 5
math::ComplexRoundedRect rrect{
    math::Rect{0, 0, 100, 100},
    {10.0f, 20.0f, 15.0f, 5.0f}  // 左上、右上、右下、左下
};
visual.set_clip_complex_rounded_rect(rrect);
```

---

### has_clip_complex_rounded_rect

```cpp
[[nodiscard]] bool has_clip_complex_rounded_rect() const noexcept;
```

**描述**：返回当前是否有四角独立圆角裁剪区域。

---

### clip_complex_rounded_rect

```cpp
[[nodiscard]] math::ComplexRoundedRect clip_complex_rounded_rect() const noexcept;
```

**描述**：返回四角独立圆角裁剪区域。

**返回值**：
- 四角独立圆角矩形区域（仅 `has_clip_complex_rounded_rect() == true` 时有效）

---

### clear_clip_complex_rounded_rect

```cpp
void clear_clip_complex_rounded_rect();
```

**描述**：清除四角独立圆角裁剪区域并触发渲染失效。

---

## 快捷属性访问器

### opacity

```cpp
[[nodiscard]] float opacity() const;
```

**描述**：返回当前不透明度值。

**返回值**：
- 不透明度（`[0.0, 1.0]`）

---

### set_opacity

```cpp
void set_opacity(float v);
```

**描述**：设置不透明度。

**参数**：
- `v`：不透明度（会被钳制到 `[0.0, 1.0]`）

**示例**：
```cpp
visual.set_opacity(0.5f);  // 50% 不透明度
```

---

### visibility

```cpp
[[nodiscard]] Visibility visibility() const;
```

**描述**：返回当前可见性状态。

**返回值**：
- 可见性状态（`Visibility` 枚举）

---

### set_visibility

```cpp
void set_visibility(Visibility v);
```

**描述**：设置可见性状态。

**参数**：
- `v`：可见性状态

**示例**：
```cpp
visual.set_visibility(Visibility::Collapsed);  // 折叠元素
```

---

## 脏区管理

### is_render_dirty

```cpp
[[nodiscard]] bool is_render_dirty() const noexcept;
```

**描述**：返回当前渲染脏标志。

**返回值**：
- `true`：自上次渲染以来节点或子树有变化，需要重绘
- `false`：无需重绘

---

### invalidate_render

```cpp
void invalidate_render() override;
```

**描述**：将渲染脏标志置为 `true`，并向上传播到根节点。

**触发时机**：
- 属性系统在 `affects_render = true` 的属性变更时自动调用
- 子类在自定义绘制数据变更时手动调用

**示例**：
```cpp
// 手动触发渲染失效
visual.invalidate_render();
```

---

## 渲染管线

### render_to_canvas

```cpp
void render_to_canvas(paint::Canvas& canvas);
```

**描述**：将本节点及子树递归渲染到 Canvas。

**参数**：
- `canvas`：目标 Canvas（录制模式，由窗口系统构造后传入）

**渲染顺序**：
1. 若 `Visibility == Collapsed`，直接返回
2. `canvas.save()`
3. 若有变换，`canvas.transform(render_transform_)`
4. 若有裁剪，`canvas.clip_rect(clip_rect_)`
5. 若 `Visibility == Visible`，调用 `on_render(canvas)`
6. 递归渲染所有子节点
7. `canvas.restore()`
8. 清除渲染脏标志

**示例**：
```cpp
paint::Canvas canvas;
visual.render_to_canvas(canvas);
```

---

## 路由事件处理器管理

### add_handler

```cpp
void add_handler(const RoutedEvent& event,
                 RoutedEventHandlerFn fn,
                 void* user_data = nullptr);
```

**描述**：为指定路由事件注册处理器。

**参数**：
- `event`：目标路由事件描述符
- `fn`：处理器函数指针（不可为 `nullptr`）
- `user_data`：用户自定义数据，原样传回处理器

**行为**：
- 同一 `(event, handler, user_data)` 组合可多次注册，每次均会触发

**示例**：
```cpp
visual.add_handler(
    Button::ClickEvent,
    [](IRoutedEventTarget* sender, RoutedEventArgs& args, void* user_data) {
        std::cout << "Clicked!" << std::endl;
    },
    nullptr
);
```

---

### remove_handler

```cpp
void remove_handler(const RoutedEvent& event,
                    RoutedEventHandlerFn fn,
                    void* user_data = nullptr);
```

**描述**：移除第一个匹配的路由事件处理器。

**参数**：
- `event`：目标路由事件描述符
- `fn`：处理器函数指针
- `user_data`：用户自定义数据

**行为**：
- 按 `(event, handler, user_data)` 三元组精确匹配，仅移除第一个

---

## IRoutedEventTarget 接口实现

### parent_target

```cpp
[[nodiscard]] IRoutedEventTarget* parent_target() const noexcept override;
```

**描述**：返回父节点作为路由事件目标。

**返回值**：
- 父节点指针（`nullptr` 表示根节点）

---

### invoke_handlers

```cpp
void invoke_handlers(const RoutedEvent& event,
                      RoutedEventArgs&   args) noexcept override;
```

**描述**：触发本节点上注册到指定事件的所有处理器。

**参数**：
- `event`：路由事件描述符
- `args`：路由事件参数

**行为**：
- 按注册顺序调用
- 每次回调后检查 `args.handled()`，为 `true` 时停止本层处理

---

## 无 RTTI 类型探查

### as_element

```cpp
[[nodiscard]] virtual UIElement* as_element() noexcept;
```

**描述**：若此 Visual 实际上是 UIElement，返回 `this` 的 `UIElement*` 指针；否则返回 `nullptr`。

**返回值**：
- `UIElement*` 指针（若不是 UIElement 则返回 `nullptr`）

**用途**：
- 项目禁用 RTTI（`/GR-`），禁止使用 `dynamic_cast`
- UIElement 覆盖此方法返回 `this`，使命中测试得以在子树中递归

**示例**：
```cpp
Visual* visual = ...;
UIElement* element = visual->as_element();
if (element != nullptr) {
    // visual 实际上是 UIElement
}
```

---

## 受保护方法

### on_parent_changed

```cpp
virtual void on_parent_changed(Visual* old_parent, Visual* new_parent) noexcept;
```

**描述**：父节点变更钩子，子类可覆盖以响应"加入/离开视觉树"事件。

**参数**：
- `old_parent`：变更前的父节点（`nullptr` 表示原来是根节点）
- `new_parent`：变更后的父节点（`nullptr` 表示成为根节点）

**触发时机**：
- `add_child` / `remove_child` / 析构时调用

**典型用途**：
- UserControl 覆盖此方法，当 `old_parent == nullptr` 且 `new_parent != nullptr` 时调用 `on_loaded()`
- 反之调用 `on_unloaded()`

**默认实现**：空操作

---

### on_render

```cpp
virtual void on_render(paint::Canvas& canvas);
```

**描述**：自身绘制虚方法，子类覆盖以实现自定义绘制逻辑。

**参数**：
- `canvas`：当前绘制上下文

**调用时机**：
- 此方法在 `canvas` 已完成 `save/transform/clip` 之后调用
- 坐标系处于本节点的局部坐标系中

**默认实现**：空操作（Visual 本身不绘制任何内容）

**示例**：
```cpp
class MyVisual : public Visual {
protected:
    void on_render(paint::Canvas& canvas) override {
        // 绘制自定义内容
        paint::Paint paint;
        paint.set_color(paint::Color::red());
        canvas.draw_rect(math::Rect{0, 0, 100, 100}, paint);
    }
};
```

---

### on_property_changed

```cpp
void on_property_changed(const DependencyProperty& prop,
                         const core::Variant&      old_value,
                         const core::Variant&      new_value) override;
```

**描述**：覆盖 `DependencyObject::on_property_changed`，对 `VisibilityProperty`/`OpacityProperty` 的变更刷新内部缓存。

**参数**：
- `prop`：变更的依赖属性
- `old_value`：旧值
- `new_value`：新值

---

## 使用场景

### 1. 创建视觉树

```cpp
#include <mine/ui/visual/Visual.h>

using namespace mine::ui;

Visual root;
Visual child1;
Visual child2;

root.add_child(&child1);
root.add_child(&child2);

// 视觉树：
// root
//   ├─ child1
//   └─ child2
```

---

### 2. 遍历子节点

```cpp
for (uint32_t i = 0; i < root.child_count(); ++i) {
    Visual* child = root.child_at(i);
    std::cout << "Child " << i << std::endl;
}
```

---

### 3. 应用仿射变换（平移/旋转/缩放）

```cpp
// 平移 100, 50
math::Transform2D t = math::Transform2D::make_translate(100.0, 50.0);
visual.set_render_transform(t);

// 旋转 45 度
math::Transform2D t2 = math::Transform2D::make_rotate(M_PI / 4);
visual.set_render_transform(t2);

// 缩放 2 倍
math::Transform2D t3 = math::Transform2D::make_scale(2.0, 2.0);
visual.set_render_transform(t3);

// 组合变换：先平移后旋转
math::Transform2D t4 = math::Transform2D::make_translate(100, 50)
                       * math::Transform2D::make_rotate(M_PI / 4);
visual.set_render_transform(t4);
```

---

### 4. 设置矩形裁剪

```cpp
math::Rect clip{0, 0, 100, 100};
visual.set_clip_rect(clip);

// 渲染时超出 (0,0)-(100,100) 的部分被裁剪
```

---

### 5. 设置圆角矩形裁剪

```cpp
math::RoundedRect rrect{math::Rect{0, 0, 100, 100}, 10.0f};
visual.set_clip_rounded_rect(rrect);

// 渲染时应用圆角裁剪（圆角半径 10）
```

---

### 6. 设置四角独立圆角裁剪

```cpp
math::ComplexRoundedRect rrect{
    math::Rect{0, 0, 100, 100},
    {10.0f, 20.0f, 15.0f, 5.0f}  // 左上、右上、右下、左下
};
visual.set_clip_complex_rounded_rect(rrect);

// 四角圆角半径独立设置
```

---

### 7. 控制可见性和不透明度

```cpp
// 设置 50% 不透明度
visual.set_opacity(0.5f);

// 折叠元素（不渲染也不占据布局空间）
visual.set_visibility(Visibility::Collapsed);

// 恢复可见
visual.set_visibility(Visibility::Visible);
```

---

### 8. 注册路由事件处理器

```cpp
visual.add_handler(
    Button::ClickEvent,
    [](IRoutedEventTarget* sender, RoutedEventArgs& args, void* user_data) {
        std::cout << "Clicked!" << std::endl;
        args.set_handled(true);
    },
    nullptr
);
```

---

### 9. 手动触发渲染失效

```cpp
// 自定义绘制数据变更时手动触发
visual.invalidate_render();

// 框架会在下一帧重新调用 render_to_canvas
```

---

### 10. 自定义渲染逻辑

```cpp
class MyVisual : public Visual {
protected:
    void on_render(paint::Canvas& canvas) override {
        // 绘制红色矩形
        paint::Paint paint;
        paint.set_color(paint::Color::red());
        canvas.draw_rect(math::Rect{0, 0, 100, 100}, paint);
        
        // 绘制文本
        canvas.draw_text("Hello", 10, 50, paint);
    }
};
```

---

## 最佳实践

### 1. 使用快捷访问器而非依赖属性 API

```cpp
// ✅ 推荐：使用快捷访问器
visual.set_opacity(0.5f);
visual.set_visibility(Visibility::Collapsed);

// ❌ 不推荐：使用依赖属性 API（冗长）
visual.set_value(Visual::OpacityProperty, core::Variant::from_float(0.5f));
visual.set_value(Visual::VisibilityProperty, 
                 core::Variant::from_enum(Visibility::Collapsed));
```

---

### 2. 添加子节点前检查父节点

```cpp
// ✅ 推荐：添加子节点前检查
if (child.parent() == nullptr) {
    parent.add_child(&child);
}

// ❌ 不推荐：不检查（可能违反前置条件）
parent.add_child(&child);  // ❌ 若 child 已有父节点，违反前置条件
```

---

### 3. 移除子节点前检查父节点

```cpp
// ✅ 推荐：移除子节点前检查
if (child.parent() == &parent) {
    parent.remove_child(&child);
}

// ✅ 推荐：使用 remove_all_children（无需检查）
parent.remove_all_children();
```

---

### 4. 使用组合变换而非多次设置

```cpp
// ✅ 推荐：使用组合变换（一次设置）
math::Transform2D t = math::Transform2D::make_translate(100, 50)
                      * math::Transform2D::make_rotate(M_PI / 4);
visual.set_render_transform(t);

// ❌ 不推荐：多次设置（后者覆盖前者）
visual.set_render_transform(math::Transform2D::make_translate(100, 50));
visual.set_render_transform(math::Transform2D::make_rotate(M_PI / 4));  // 覆盖平移
```

---

### 5. 裁剪类型互斥，选择合适的裁剪

```cpp
// ✅ 推荐：根据需求选择合适的裁剪类型
visual.set_clip_rect(math::Rect{0, 0, 100, 100});  // 矩形裁剪
visual.set_clip_rounded_rect(math::RoundedRect{...});  // 圆角裁剪
visual.set_clip_complex_rounded_rect(math::ComplexRoundedRect{...});  // 四角独立圆角

// ⚠️ 注意：后设置的裁剪会替换前面的裁剪
```

---

### 6. 覆盖 on_render 实现自定义绘制

```cpp
// ✅ 推荐：覆盖 on_render 实现自定义绘制
class MyVisual : public Visual {
protected:
    void on_render(paint::Canvas& canvas) override {
        // 自定义绘制逻辑
    }
};

// ❌ 不推荐：在外部直接调用 render_to_canvas（破坏封装）
paint::Canvas canvas;
visual.render_to_canvas(canvas);  // ❌ 通常由窗口系统调用
```

---

## 常见陷阱

### 1. 尝试添加已有父节点的子节点

```cpp
// ❌ 问题：child 已有父节点
Visual parent1;
Visual parent2;
Visual child;

parent1.add_child(&child);
parent2.add_child(&child);  // ❌ 违反前置条件（child->parent() != nullptr）

// ✅ 解决：先从旧父节点移除
parent1.remove_child(&child);
parent2.add_child(&child);
```

---

### 2. 访问越界子节点索引

```cpp
// ❌ 问题：访问越界索引
Visual* child = visual.child_at(100);  // ❌ 若 child_count() < 100，未定义行为

// ✅ 解决：先检查 child_count()
if (index < visual.child_count()) {
    Visual* child = visual.child_at(index);
}
```

---

### 3. 裁剪类型互斥导致意外覆盖

```cpp
// ❌ 问题：设置圆角裁剪覆盖了矩形裁剪
visual.set_clip_rect(math::Rect{0, 0, 100, 100});
visual.set_clip_rounded_rect(math::RoundedRect{...});  // ❌ 矩形裁剪被替换

// ✅ 解决：明确裁剪类型（仅使用一种）
visual.set_clip_rounded_rect(math::RoundedRect{...});  // 仅使用圆角裁剪
```

---

### 4. 忘记触发 invalidate_render

```cpp
// ❌ 问题：自定义绘制数据变更但未触发 invalidate_render
class MyVisual : public Visual {
public:
    void set_color(const paint::Color& c) {
        color_ = c;
        // ❌ 忘记触发渲染失效
    }

protected:
    void on_render(paint::Canvas& canvas) override {
        paint::Paint paint;
        paint.set_color(color_);
        canvas.draw_rect(math::Rect{0, 0, 100, 100}, paint);
    }

private:
    paint::Color color_;
};

// ✅ 解决：变更数据后调用 invalidate_render
void set_color(const paint::Color& c) {
    if (color_ != c) {
        color_ = c;
        invalidate_render();  // ✅ 触发渲染失效
    }
}
```

---

### 5. 直接调用 render_to_canvas

```cpp
// ❌ 问题：在外部直接调用 render_to_canvas（破坏封装）
paint::Canvas canvas;
visual.render_to_canvas(canvas);  // ❌ 通常由窗口系统调用

// ✅ 解决：由窗口系统或根 Visual 调用
// 应用程序代码通常不直接调用 render_to_canvas
```

---

## 完整示例

```cpp
#include <mine/ui/visual/Visual.h>
#include <mine/paint/Canvas.h>
#include <mine/paint/Paint.h>
#include <mine/paint/Color.h>
#include <mine/math/Transform2D.h>
#include <mine/math/Rect.h>
#include <iostream>

using namespace mine::ui;
using namespace mine::paint;
using namespace mine::math;

// ────────────────────────────────────────────────────────────────────────────
// 自定义 Visual 类
// ────────────────────────────────────────────────────────────────────────────

class ColoredRect : public Visual {
public:
    explicit ColoredRect(const Color& c) : color_(c) {}

    void set_color(const Color& c) {
        if (color_ != c) {
            color_ = c;
            invalidate_render();
        }
    }

protected:
    void on_render(Canvas& canvas) override {
        Paint paint;
        paint.set_color(color_);
        canvas.draw_rect(Rect{0, 0, 100, 100}, paint);
    }

private:
    Color color_;
};

// ────────────────────────────────────────────────────────────────────────────
// 使用示例
// ────────────────────────────────────────────────────────────────────────────

int main() {
    // ════════════════════════════════════════════════════════════════════════
    // 1. 创建视觉树
    // ════════════════════════════════════════════════════════════════════════
    Visual root;
    ColoredRect child1(Color::red());
    ColoredRect child2(Color::blue());
    
    root.add_child(&child1);
    root.add_child(&child2);
    
    std::cout << "根节点子节点数量: " << root.child_count() << std::endl;
    // 输出：根节点子节点数量: 2
    
    // ════════════════════════════════════════════════════════════════════════
    // 2. 应用变换
    // ════════════════════════════════════════════════════════════════════════
    
    // child1 平移 (100, 0)
    Transform2D t1 = Transform2D::make_translate(100, 0);
    child1.set_render_transform(t1);
    
    // child2 平移 (200, 0) 并旋转 45 度
    Transform2D t2 = Transform2D::make_translate(200, 0)
                     * Transform2D::make_rotate(M_PI / 4);
    child2.set_render_transform(t2);
    
    // ════════════════════════════════════════════════════════════════════════
    // 3. 设置裁剪
    // ════════════════════════════════════════════════════════════════════════
    
    // child1 矩形裁剪
    child1.set_clip_rect(Rect{0, 0, 50, 50});
    
    // child2 圆角矩形裁剪
    child2.set_clip_rounded_rect(RoundedRect{Rect{0, 0, 100, 100}, 10.0f});
    
    // ════════════════════════════════════════════════════════════════════════
    // 4. 控制可见性和不透明度
    // ════════════════════════════════════════════════════════════════════════
    
    child1.set_opacity(0.8f);  // 80% 不透明度
    child2.set_opacity(0.6f);  // 60% 不透明度
    
    // ════════════════════════════════════════════════════════════════════════
    // 5. 渲染到 Canvas
    // ════════════════════════════════════════════════════════════════════════
    
    Canvas canvas;  // 由窗口系统创建
    root.render_to_canvas(canvas);
    
    std::cout << "渲染完成" << std::endl;
    
    // ════════════════════════════════════════════════════════════════════════
    // 6. 遍历子节点
    // ════════════════════════════════════════════════════════════════════════
    
    for (uint32_t i = 0; i < root.child_count(); ++i) {
        Visual* child = root.child_at(i);
        std::cout << "子节点 " << i << " 不透明度: " << child->opacity() << std::endl;
    }
    // 输出：
    // 子节点 0 不透明度: 0.8
    // 子节点 1 不透明度: 0.6
    
    // ════════════════════════════════════════════════════════════════════════
    // 7. 移除子节点
    // ════════════════════════════════════════════════════════════════════════
    
    root.remove_child(&child1);
    std::cout << "移除后子节点数量: " << root.child_count() << std::endl;
    // 输出：移除后子节点数量: 1
    
    return 0;
}
```

---

## 总结

`Visual` 是 `mine.ui.visual` 模块的视觉树基类，具备：

1. **视觉树管理**：父子节点关系（非拥有指针）
2. **局部仿射变换**：RenderTransform（math::Transform2D）
3. **多种裁剪**：矩形/圆角矩形/四角独立圆角
4. **依赖属性**：Opacity（不透明度）、Visibility（可见性）
5. **脏区追踪**：invalidate_render() 标记渲染失效
6. **渲染管线**：render_to_canvas() 递归绘制到 paint::Canvas
7. **路由事件**：实现 IRoutedEventTarget 接口

**使用建议**：
- 使用快捷访问器而非依赖属性 API
- 添加/移除子节点前检查父节点
- 使用组合变换而非多次设置
- 裁剪类型互斥，选择合适的裁剪
- 覆盖 on_render 实现自定义绘制
- 自定义数据变更后调用 invalidate_render
- 避免在外部直接调用 render_to_canvas
