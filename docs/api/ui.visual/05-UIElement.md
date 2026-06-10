# UIElement 详细接口文档

## 概述

`UIElement` 是 `mine.ui.visual` 模块的**具有布局接口与命中测试能力的视觉元素基类**。

**核心特性：**
- **布局接口**：Measure/Arrange 两阶段布局系统
- **布局边界**：bounds_rect（由布局系统设置的最终排列矩形）
- **期望尺寸**：desired_size（由 Measure 阶段计算）
- **命中测试**：hit_test（屏幕坐标→局部坐标→判断命中）
- **命中穿透**：is_hit_transparent（跳过内部实现元素）
- **键盘焦点**：is_focusable（支持焦点管理）
- **布局失效**：invalidate_measure/invalidate_arrange（脏标志管理）

**继承关系：**
```
Visual (mine.ui.visual)
    └─ UIElement (mine.ui.visual)
        └─ Control
```

---

## 文件位置

```
src/mine/ui/visual/api/include/mine/ui/visual/UIElement.h
```

---

## 类定义

```cpp
class MINE_UI_VISUAL_API UIElement : public Visual {
public:
    // 生命周期
    UIElement();
    ~UIElement() override;
    UIElement(const UIElement&)            = delete;
    UIElement& operator=(const UIElement&) = delete;
    UIElement(UIElement&&)                 = default;
    UIElement& operator=(UIElement&&)      = default;

    // 类型探查
    [[nodiscard]] UIElement* as_element() noexcept override;

    // 布局边界
    [[nodiscard]] math::Rect bounds_rect() const noexcept;
    void set_bounds_rect(math::Rect rect);
    [[nodiscard]] math::Size desired_size() const noexcept;

    // 布局接口
    void measure(math::Size available_size);
    virtual void arrange(math::Rect slot);

    // 命中测试
    virtual UIElement* hit_test(math::Point p);
    void set_hit_transparent(bool transparent) noexcept;
    [[nodiscard]] bool is_hit_transparent() const noexcept;

    // 键盘焦点
    void set_focusable(bool focusable) noexcept;
    [[nodiscard]] bool is_focusable() const noexcept;

    // 布局失效
    void invalidate_measure() override;
    void invalidate_arrange() override;

protected:
    // 布局虚方法
    virtual void on_measure(math::Size available_size);
    virtual void on_arrange(math::Rect final_rect);
    virtual bool hit_test_local(math::Point p) const;

    // 辅助方法
    void set_desired_size(math::Size size) noexcept;

private:
    struct Impl;
    core::Pimpl<Impl> p_;
};
```

---

## 生命周期

### UIElement

```cpp
UIElement();
```

**描述**：构造 UIElement 对象。

**初始状态**：
- `bounds_rect` = `{0, 0, 0, 0}`（零矩形）
- `desired_size` = `{0, 0}`（零尺寸）
- `is_hit_transparent` = `false`（可命中）
- `is_focusable` = `false`（不可聚焦）

**示例**：
```cpp
UIElement element;
```

---

### ~UIElement

```cpp
~UIElement() override;
```

**描述**：析构 UIElement 对象。

---

## 类型探查

### as_element

```cpp
[[nodiscard]] UIElement* as_element() noexcept override;
```

**描述**：返回 `this`，供 `Visual::as_element()` 虚方法链使用（无 RTTI）。

**返回值**：
- `this`（UIElement 指针）

**用途**：
- 项目禁用 RTTI（`/GR-`），禁止使用 `dynamic_cast`
- UIElement 覆盖此方法返回 `this`，使命中测试得以在子树中递归

**示例**：
```cpp
Visual* visual = &element;
UIElement* elem = visual->as_element();  // 返回 &element
```

---

## 布局边界

### bounds_rect

```cpp
[[nodiscard]] math::Rect bounds_rect() const noexcept;
```

**描述**：返回由布局系统设置的最终排列矩形（相对于父节点坐标系）。

**返回值**：
- 最终排列矩形（`math::Rect`）

**特征**：
- 在布局系统（mine.ui.layout）Arrange 阶段完成后由 `set_bounds_rect()` 写入
- 未经布局时默认为零矩形 `{0, 0, 0, 0}`

**示例**：
```cpp
math::Rect bounds = element.bounds_rect();
std::cout << "x=" << bounds.x << ", y=" << bounds.y 
          << ", width=" << bounds.width << ", height=" << bounds.height << std::endl;
```

---

### set_bounds_rect

```cpp
void set_bounds_rect(math::Rect rect);
```

**描述**：由布局系统调用，设置元素的最终排列矩形。

**参数**：
- `rect`：最终排列矩形（父节点坐标系）

**行为**：
- 同时调用 `on_arrange(rect)` 让子类更新内部状态

**示例**：
```cpp
// 由布局系统调用
element.set_bounds_rect(math::Rect{100, 50, 200, 100});

// element.bounds_rect() == {100, 50, 200, 100}
// on_arrange({100, 50, 200, 100}) 被调用
```

---

### desired_size

```cpp
[[nodiscard]] math::Size desired_size() const noexcept;
```

**描述**：返回期望尺寸（由 Measure 阶段写入）。

**返回值**：
- 期望尺寸（`math::Size`）

**特征**：
- 默认为 `{0, 0}`
- 由布局系统完成 Measure 后更新

**示例**：
```cpp
math::Size desired = element.desired_size();
std::cout << "desired_width=" << desired.width 
          << ", desired_height=" << desired.height << std::endl;
```

---

## 布局接口

### measure

```cpp
void measure(math::Size available_size);
```

**描述**：公共 Measure 入口，驱动元素完成测量并更新 `desired_size`。

**参数**：
- `available_size`：父节点提供的可用空间

**行为**：
- 调用受保护的 `on_measure(available_size)`
- mine.ui.layout 中的 FrameworkElement 覆盖 `on_measure` 以实现完整的约束与边距处理

**示例**：
```cpp
// 父节点调用 measure 测量子节点
element.measure(math::Size{400, 300});

// element.desired_size() 更新为子节点期望尺寸
```

---

### arrange

```cpp
virtual void arrange(math::Rect slot);
```

**描述**：公共 Arrange 入口，将元素排列到指定矩形区域内。

**参数**：
- `slot`：父节点为本元素分配的矩形区域（父节点坐标系）

**行为**：
- 默认实现直接调用 `set_bounds_rect(slot)`
- mine.ui.layout 中的 FrameworkElement 覆盖此方法，在调用 `set_bounds_rect` 之前处理 Margin 和 HorizontalAlignment/VerticalAlignment 对齐

**示例**：
```cpp
// 父节点调用 arrange 排列子节点
element.arrange(math::Rect{100, 50, 200, 100});

// element.bounds_rect() == {100, 50, 200, 100}
// on_arrange({100, 50, 200, 100}) 被调用
```

---

## 命中测试

### hit_test

```cpp
virtual UIElement* hit_test(math::Point p);
```

**描述**：在指定屏幕坐标处进行命中测试，返回最顶层命中的 UIElement。

**参数**：
- `p`：屏幕坐标系中的测试点（相对于父节点坐标系）

**返回值**：
- 命中的最内层 UIElement 指针；若无命中则返回 `nullptr`

**默认实现流程**：
1. 若 `Visibility == Collapsed`，返回 `nullptr`
2. 将输入点逆变换到本节点局部坐标系
3. 若有裁剪区域且点在裁剪外，返回 `nullptr`
4. 逆序遍历子节点，跳过命中穿透（`is_hit_transparent`）的子节点，首个命中子节点立即返回
5. 子节点均未命中时，调用 `hit_test_local()` 判断本节点自身

**使用建议**：
- 控件通常不需要覆盖此方法
- 通过 `set_hit_transparent()` 标记内部实现子元素
- 在 `on_arrange()` 中调用 `set_clip_rounded_rect()` 定义控件形状
- 基类实现即可正确完成命中测试

**示例**：
```cpp
// 测试鼠标点击位置
math::Point mouse_pos{150, 75};
UIElement* hit = element.hit_test(mouse_pos);

if (hit != nullptr) {
    std::cout << "命中元素" << std::endl;
} else {
    std::cout << "未命中" << std::endl;
}
```

---

### set_hit_transparent

```cpp
void set_hit_transparent(bool transparent) noexcept;
```

**描述**：设置命中穿透标志（等价 Qt `WA_TransparentForMouseEvents`）。

**参数**：
- `transparent`：`true` 表示命中穿透

**特征**：
- 设为 `true` 后，此元素在 `hit_test` 遍历子树时被跳过，不会作为命中目标
- 但仍参与正常渲染
- 适用于 Control 内部的 ContentPresenter 等实现细节元素
- 确保鼠标事件始终派发给控件本身而非内部元素

**自动设置**：
- `Control::set_inner_element()` 会自动为内部元素设置此标志
- 通常不需要手动调用

**示例**：
```cpp
// 将内部元素标记为命中穿透
inner_element.set_hit_transparent(true);

// hit_test() 遍历时跳过 inner_element
```

---

### is_hit_transparent

```cpp
[[nodiscard]] bool is_hit_transparent() const noexcept;
```

**描述**：返回命中穿透标志。

**返回值**：
- `true`：此元素对命中测试不可见
- `false`：此元素可命中（默认）

---

## 键盘焦点

### set_focusable

```cpp
void set_focusable(bool focusable) noexcept;
```

**描述**：设置可聚焦标志。

**参数**：
- `focusable`：`true` 表示可聚焦

**特征**：
- 为 `true` 时，InputRouter 在 MouseDown 命中此元素后会自动将其设为键盘焦点
- 后续键盘输入（KeyDown/KeyUp/TextInput）将发往此元素
- 默认为 `false`（不可聚焦，不参与焦点切换）

**示例**：
```cpp
// 设置文本框可聚焦
text_box.set_focusable(true);

// 用户点击文本框后，键盘输入将发往 text_box
```

---

### is_focusable

```cpp
[[nodiscard]] bool is_focusable() const noexcept;
```

**描述**：返回可聚焦标志。

**返回值**：
- `true`：可聚焦
- `false`：不可聚焦（默认）

---

## 布局失效

### invalidate_measure

```cpp
void invalidate_measure() override;
```

**描述**：布局测量失效，设置内部脏标志。

**行为**：
- mine.ui.layout 接管后转为队列通知
- 框架会在下一帧重新调用 `measure()`

**示例**：
```cpp
// 内容变更时触发测量失效
element.invalidate_measure();

// 框架会在下一帧重新测量
```

---

### invalidate_arrange

```cpp
void invalidate_arrange() override;
```

**描述**：布局排列失效，设置内部脏标志。

**行为**：
- 框架会在下一帧重新调用 `arrange()`

**示例**：
```cpp
// 对齐方式变更时触发排列失效
element.invalidate_arrange();

// 框架会在下一帧重新排列
```

---

## 受保护方法

### on_measure

```cpp
virtual void on_measure(math::Size available_size);
```

**描述**：Measure 阶段虚方法，计算元素的期望尺寸。

**参数**：
- `available_size`：父节点提供的可用空间

**行为**：
- 子类覆盖此方法实现自定义尺寸计算逻辑
- 默认实现将 `desired_size` 置为 `{0, 0}`

**子类实现建议**：
- 调用 `set_desired_size()` 设置期望尺寸
- 递归测量子节点（调用子节点的 `measure()`）

**示例**：
```cpp
class MyElement : public UIElement {
protected:
    void on_measure(math::Size available_size) override {
        // 计算期望尺寸
        math::Size desired{100, 50};
        set_desired_size(desired);
    }
};
```

---

### on_arrange

```cpp
virtual void on_arrange(math::Rect final_rect);
```

**描述**：Arrange 阶段虚方法，响应布局系统的最终排列。

**参数**：
- `final_rect`：布局系统分配的最终矩形

**行为**：
- 子类覆盖此方法在布局确定后更新内部状态（如子元素位置）
- 默认实现为空操作

**子类实现建议**：
- 更新子元素位置（调用子元素的 `arrange()`）
- 设置裁剪区域（如圆角裁剪）

**示例**：
```cpp
class MyPanel : public UIElement {
protected:
    void on_arrange(math::Rect final_rect) override {
        // 排列子元素
        for (uint32_t i = 0; i < child_count(); ++i) {
            UIElement* child = child_at(i)->as_element();
            if (child != nullptr) {
                math::Rect child_rect{0, 0, final_rect.width, final_rect.height};
                child->arrange(child_rect);
            }
        }
    }
};
```

---

### hit_test_local

```cpp
virtual bool hit_test_local(math::Point p) const;
```

**描述**：局部坐标系下的命中测试判断。

**参数**：
- `p`：已变换到本节点局部坐标系的测试点

**返回值**：
- `true`：命中本节点
- `false`：未命中

**默认实现优先级**：
1. 若设置了 `clip_rounded_rect`：测试点是否在圆角矩形内（外角不命中）
2. 否则：测试点是否在 `bounds_rect()` 范围内

**使用建议**：
- 控件通常在 `on_arrange()` 中通过 `set_clip_rounded_rect()` 设置圆角裁剪
- 使命中测试与视觉边界自动保持一致
- 无需覆盖此方法

**示例**：
```cpp
class CircleElement : public UIElement {
protected:
    bool hit_test_local(math::Point p) const override {
        // 自定义圆形命中测试
        math::Rect bounds = bounds_rect();
        double cx = bounds.width / 2.0;
        double cy = bounds.height / 2.0;
        double radius = std::min(cx, cy);
        
        double dx = p.x - cx;
        double dy = p.y - cy;
        double distance = std::sqrt(dx * dx + dy * dy);
        
        return distance <= radius;
    }
};
```

---

### set_desired_size

```cpp
void set_desired_size(math::Size size) noexcept;
```

**描述**：设置期望尺寸（供 FrameworkElement 等子类在 `on_measure` 中调用）。

**参数**：
- `size`：计算出的期望尺寸

**行为**：
- 同时清除 `measure_dirty_` 标志

**示例**：
```cpp
class MyElement : public UIElement {
protected:
    void on_measure(math::Size available_size) override {
        math::Size desired{100, 50};
        set_desired_size(desired);
    }
};
```

---

## 使用场景

### 1. 自定义布局逻辑

```cpp
#include <mine/ui/visual/UIElement.h>

using namespace mine::ui;

class MyPanel : public UIElement {
protected:
    void on_measure(math::Size available_size) override {
        // 测量所有子节点
        for (uint32_t i = 0; i < child_count(); ++i) {
            UIElement* child = child_at(i)->as_element();
            if (child != nullptr) {
                child->measure(available_size);
            }
        }
        
        // 计算期望尺寸（所有子节点高度之和）
        double total_height = 0.0;
        double max_width = 0.0;
        
        for (uint32_t i = 0; i < child_count(); ++i) {
            UIElement* child = child_at(i)->as_element();
            if (child != nullptr) {
                math::Size child_desired = child->desired_size();
                total_height += child_desired.height;
                max_width = std::max(max_width, child_desired.width);
            }
        }
        
        set_desired_size(math::Size{max_width, total_height});
    }
    
    void on_arrange(math::Rect final_rect) override {
        // 垂直排列所有子节点
        double y_offset = 0.0;
        
        for (uint32_t i = 0; i < child_count(); ++i) {
            UIElement* child = child_at(i)->as_element();
            if (child != nullptr) {
                math::Size child_desired = child->desired_size();
                math::Rect child_rect{
                    0, y_offset,
                    final_rect.width, child_desired.height
                };
                child->arrange(child_rect);
                y_offset += child_desired.height;
            }
        }
    }
};
```

---

### 2. 命中测试与鼠标交互

```cpp
// 命中测试
math::Point mouse_pos{150, 75};
UIElement* hit = root_element.hit_test(mouse_pos);

if (hit != nullptr) {
    std::cout << "命中元素" << std::endl;
    
    // 派发鼠标事件
    RoutedEventArgs args{MouseDownEvent};
    args.set_source(hit);
    EventManager::raise(*hit, args);
}
```

---

### 3. 内部元素命中穿透

```cpp
class MyControl : public UIElement {
public:
    MyControl() {
        // 创建内部元素
        content_presenter_ = std::make_unique<UIElement>();
        
        // 标记为命中穿透（鼠标事件不派发给内部元素）
        content_presenter_->set_hit_transparent(true);
        
        // 添加为子节点
        add_child(content_presenter_.get());
    }

private:
    std::unique_ptr<UIElement> content_presenter_;
};
```

---

### 4. 可聚焦文本框

```cpp
class TextBox : public UIElement {
public:
    TextBox() {
        // 设置可聚焦
        set_focusable(true);
    }
    
    // 处理键盘输入
    void on_key_down(KeyEventArgs& args) {
        // 处理按键
    }
};
```

---

### 5. 自定义圆形命中测试

```cpp
class CircleButton : public UIElement {
protected:
    bool hit_test_local(math::Point p) const override {
        // 圆形命中测试
        math::Rect bounds = bounds_rect();
        double cx = bounds.width / 2.0;
        double cy = bounds.height / 2.0;
        double radius = std::min(cx, cy);
        
        double dx = p.x - cx;
        double dy = p.y - cy;
        double distance = std::sqrt(dx * dx + dy * dy);
        
        return distance <= radius;
    }
    
    void on_render(paint::Canvas& canvas) override {
        // 绘制圆形按钮
        math::Rect bounds = bounds_rect();
        double cx = bounds.width / 2.0;
        double cy = bounds.height / 2.0;
        double radius = std::min(cx, cy);
        
        paint::Paint paint;
        paint.set_color(paint::Color::blue());
        canvas.draw_circle(math::Point{cx, cy}, radius, paint);
    }
};
```

---

### 6. 触发布局失效

```cpp
class MyElement : public UIElement {
public:
    void set_content(const std::string& content) {
        if (content_ != content) {
            content_ = content;
            
            // 内容变更，触发测量失效
            invalidate_measure();
            
            // 触发渲染失效
            invalidate_render();
        }
    }

private:
    std::string content_;
};
```

---

### 7. 圆角按钮（自动命中测试）

```cpp
class RoundedButton : public UIElement {
protected:
    void on_arrange(math::Rect final_rect) override {
        // 设置圆角裁剪（自动用于命中测试和渲染）
        math::RoundedRect rrect{final_rect, 10.0f};  // 圆角半径 10
        set_clip_rounded_rect(rrect);
    }
    
    void on_render(paint::Canvas& canvas) override {
        // 绘制按钮背景
        paint::Paint paint;
        paint.set_color(paint::Color::blue());
        canvas.draw_rounded_rect(
            math::RoundedRect{bounds_rect(), 10.0f},
            paint
        );
    }
};

// 命中测试自动考虑圆角（外角不命中）
```

---

## 最佳实践

### 1. 覆盖 on_measure 而非 measure

```cpp
// ✅ 推荐：覆盖 on_measure
class MyElement : public UIElement {
protected:
    void on_measure(math::Size available_size) override {
        // 自定义测量逻辑
    }
};

// ❌ 不推荐：覆盖 measure（破坏基类逻辑）
class MyElement : public UIElement {
public:
    void measure(math::Size available_size) override {
        // ❌ 破坏基类逻辑
    }
};
```

---

### 2. 在 on_measure 中调用 set_desired_size

```cpp
// ✅ 推荐：在 on_measure 中调用 set_desired_size
void on_measure(math::Size available_size) override {
    math::Size desired{100, 50};
    set_desired_size(desired);
}

// ❌ 不推荐：直接修改 desired_size（无法访问）
void on_measure(math::Size available_size) override {
    // ❌ desired_size 是私有成员，无法直接修改
}
```

---

### 3. 使用 set_clip_rounded_rect 自动命中测试

```cpp
// ✅ 推荐：使用 set_clip_rounded_rect 自动命中测试
void on_arrange(math::Rect final_rect) override {
    set_clip_rounded_rect(math::RoundedRect{final_rect, 10.0f});
    // hit_test_local 自动考虑圆角
}

// ❌ 不推荐：手动覆盖 hit_test_local（重复逻辑）
bool hit_test_local(math::Point p) const override {
    // ❌ 手动实现圆角命中测试（重复逻辑）
}
```

---

### 4. 内部元素使用 set_hit_transparent

```cpp
// ✅ 推荐：内部元素使用 set_hit_transparent
content_presenter_->set_hit_transparent(true);

// ❌ 不推荐：覆盖 hit_test 跳过内部元素（复杂）
UIElement* hit_test(math::Point p) override {
    // ❌ 手动跳过内部元素（复杂）
}
```

---

### 5. 可聚焦控件调用 set_focusable

```cpp
// ✅ 推荐：可聚焦控件调用 set_focusable
text_box.set_focusable(true);

// ❌ 不推荐：手动管理焦点（复杂）
// ❌ 手动处理 MouseDown 设置焦点（框架自动处理）
```

---

## 常见陷阱

### 1. 忘记调用 set_desired_size

```cpp
// ❌ 问题：忘记调用 set_desired_size
void on_measure(math::Size available_size) override {
    // ❌ 忘记调用 set_desired_size
    // desired_size 保持为 {0, 0}
}

// 布局系统无法获取正确的期望尺寸

// ✅ 解决：调用 set_desired_size
void on_measure(math::Size available_size) override {
    math::Size desired{100, 50};
    set_desired_size(desired);
}
```

---

### 2. 覆盖 measure 而非 on_measure

```cpp
// ❌ 问题：覆盖 measure 破坏基类逻辑
class MyElement : public UIElement {
public:
    void measure(math::Size available_size) override {
        // ❌ 破坏基类逻辑（不调用 on_measure）
        set_desired_size(math::Size{100, 50});
    }
};

// ✅ 解决：覆盖 on_measure
class MyElement : public UIElement {
protected:
    void on_measure(math::Size available_size) override {
        set_desired_size(math::Size{100, 50});
    }
};
```

---

### 3. 在 on_arrange 中未排列子元素

```cpp
// ❌ 问题：在 on_arrange 中未排列子元素
void on_arrange(math::Rect final_rect) override {
    // ❌ 忘记排列子元素
}

// 子元素 bounds_rect 未更新，显示不正确

// ✅ 解决：排列所有子元素
void on_arrange(math::Rect final_rect) override {
    for (uint32_t i = 0; i < child_count(); ++i) {
        UIElement* child = child_at(i)->as_element();
        if (child != nullptr) {
            child->arrange(math::Rect{0, 0, final_rect.width, final_rect.height});
        }
    }
}
```

---

### 4. 命中测试未考虑裁剪区域

```cpp
// ❌ 问题：覆盖 hit_test_local 但未考虑裁剪
bool hit_test_local(math::Point p) const override {
    // ❌ 未考虑 clip_rounded_rect
    return bounds_rect().contains(p);
}

// 圆角外的点仍然命中（不符合视觉）

// ✅ 解决：使用 set_clip_rounded_rect（基类自动处理）
void on_arrange(math::Rect final_rect) override {
    set_clip_rounded_rect(math::RoundedRect{final_rect, 10.0f});
    // hit_test_local 自动考虑圆角
}
```

---

### 5. 忘记触发布局失效

```cpp
// ❌ 问题：内容变更但未触发布局失效
void set_content(const std::string& content) {
    content_ = content;
    // ❌ 忘记触发 invalidate_measure
}

// 布局系统不知道需要重新测量

// ✅ 解决：触发 invalidate_measure
void set_content(const std::string& content) {
    if (content_ != content) {
        content_ = content;
        invalidate_measure();
        invalidate_render();
    }
}
```

---

## 完整示例

```cpp
#include <mine/ui/visual/UIElement.h>
#include <mine/paint/Canvas.h>
#include <mine/paint/Paint.h>
#include <mine/paint/Color.h>
#include <mine/math/Rect.h>
#include <mine/math/Point.h>
#include <mine/math/Size.h>
#include <iostream>
#include <cmath>

using namespace mine::ui;
using namespace mine::paint;
using namespace mine::math;

// ────────────────────────────────────────────────────────────────────────────
// 自定义圆形按钮
// ────────────────────────────────────────────────────────────────────────────

class CircleButton : public UIElement {
public:
    CircleButton() {
        set_focusable(true);  // 可聚焦
    }

protected:
    void on_measure(Size available_size) override {
        // 固定尺寸 100x100
        set_desired_size(Size{100, 100});
    }
    
    void on_arrange(Rect final_rect) override {
        // 无子元素，无需排列
    }
    
    bool hit_test_local(Point p) const override {
        // 圆形命中测试
        Rect bounds = bounds_rect();
        double cx = bounds.width / 2.0;
        double cy = bounds.height / 2.0;
        double radius = std::min(cx, cy);
        
        double dx = p.x - cx;
        double dy = p.y - cy;
        double distance = std::sqrt(dx * dx + dy * dy);
        
        return distance <= radius;
    }
    
    void on_render(Canvas& canvas) override {
        // 绘制圆形按钮
        Rect bounds = bounds_rect();
        double cx = bounds.width / 2.0;
        double cy = bounds.height / 2.0;
        double radius = std::min(cx, cy);
        
        Paint paint;
        paint.set_color(Color::blue());
        canvas.draw_circle(Point{cx, cy}, radius, paint);
    }
};

// ────────────────────────────────────────────────────────────────────────────
// 自定义垂直面板
// ────────────────────────────────────────────────────────────────────────────

class VerticalPanel : public UIElement {
protected:
    void on_measure(Size available_size) override {
        // 测量所有子节点
        double total_height = 0.0;
        double max_width = 0.0;
        
        for (uint32_t i = 0; i < child_count(); ++i) {
            UIElement* child = child_at(i)->as_element();
            if (child != nullptr) {
                child->measure(available_size);
                Size child_desired = child->desired_size();
                total_height += child_desired.height;
                max_width = std::max(max_width, child_desired.width);
            }
        }
        
        set_desired_size(Size{max_width, total_height});
    }
    
    void on_arrange(Rect final_rect) override {
        // 垂直排列所有子节点
        double y_offset = 0.0;
        
        for (uint32_t i = 0; i < child_count(); ++i) {
            UIElement* child = child_at(i)->as_element();
            if (child != nullptr) {
                Size child_desired = child->desired_size();
                Rect child_rect{
                    0, y_offset,
                    final_rect.width, child_desired.height
                };
                child->arrange(child_rect);
                y_offset += child_desired.height;
            }
        }
    }
};

// ────────────────────────────────────────────────────────────────────────────
// 使用示例
// ────────────────────────────────────────────────────────────────────────────

int main() {
    // ════════════════════════════════════════════════════════════════════════
    // 1. 创建元素树
    // ════════════════════════════════════════════════════════════════════════
    VerticalPanel panel;
    CircleButton button1;
    CircleButton button2;
    
    panel.add_child(&button1);
    panel.add_child(&button2);
    
    // ════════════════════════════════════════════════════════════════════════
    // 2. 布局：Measure 阶段
    // ════════════════════════════════════════════════════════════════════════
    Size available_size{400, 300};
    panel.measure(available_size);
    
    Size panel_desired = panel.desired_size();
    std::cout << "面板期望尺寸: " << panel_desired.width << "x" << panel_desired.height << std::endl;
    // 输出：面板期望尺寸: 100x200 (两个按钮高度之和)
    
    // ════════════════════════════════════════════════════════════════════════
    // 3. 布局：Arrange 阶段
    // ════════════════════════════════════════════════════════════════════════
    Rect panel_rect{0, 0, 400, 300};
    panel.arrange(panel_rect);
    
    // 检查子节点布局
    Rect button1_bounds = button1.bounds_rect();
    Rect button2_bounds = button2.bounds_rect();
    
    std::cout << "按钮1 位置: x=" << button1_bounds.x << ", y=" << button1_bounds.y << std::endl;
    std::cout << "按钮2 位置: x=" << button2_bounds.x << ", y=" << button2_bounds.y << std::endl;
    // 输出：
    // 按钮1 位置: x=0, y=0
    // 按钮2 位置: x=0, y=100
    
    // ════════════════════════════════════════════════════════════════════════
    // 4. 命中测试
    // ════════════════════════════════════════════════════════════════════════
    
    // 测试点击按钮1中心
    Point click1{50, 50};
    UIElement* hit1 = panel.hit_test(click1);
    std::cout << "点击 (50, 50): " << (hit1 == &button1 ? "命中按钮1" : "未命中") << std::endl;
    // 输出：点击 (50, 50): 命中按钮1
    
    // 测试点击按钮2中心
    Point click2{50, 150};
    UIElement* hit2 = panel.hit_test(click2);
    std::cout << "点击 (50, 150): " << (hit2 == &button2 ? "命中按钮2" : "未命中") << std::endl;
    // 输出：点击 (50, 150): 命中按钮2
    
    // 测试点击按钮1外角（圆形命中测试）
    Point click3{2, 2};
    UIElement* hit3 = panel.hit_test(click3);
    std::cout << "点击 (2, 2): " << (hit3 == nullptr ? "未命中" : "命中") << std::endl;
    // 输出：点击 (2, 2): 未命中（圆形外角）
    
    // ════════════════════════════════════════════════════════════════════════
    // 5. 焦点管理
    // ════════════════════════════════════════════════════════════════════════
    std::cout << "按钮1 可聚焦: " << (button1.is_focusable() ? "是" : "否") << std::endl;
    // 输出：按钮1 可聚焦: 是
    
    // ════════════════════════════════════════════════════════════════════════
    // 6. 渲染
    // ════════════════════════════════════════════════════════════════════════
    Canvas canvas;
    panel.render_to_canvas(canvas);
    
    std::cout << "渲染完成" << std::endl;
    
    return 0;
}
```

---

## 总结

`UIElement` 是 `mine.ui.visual` 模块的具有布局接口与命中测试能力的视觉元素基类，具备：

1. **布局接口**：Measure/Arrange 两阶段布局系统
2. **布局边界**：bounds_rect（由布局系统设置的最终排列矩形）
3. **期望尺寸**：desired_size（由 Measure 阶段计算）
4. **命中测试**：hit_test（屏幕坐标→局部坐标→判断命中）
5. **命中穿透**：is_hit_transparent（跳过内部实现元素）
6. **键盘焦点**：is_focusable（支持焦点管理）
7. **布局失效**：invalidate_measure/invalidate_arrange（脏标志管理）

**使用建议**：
- 覆盖 **on_measure** 而非 measure
- 在 **on_measure** 中调用 **set_desired_size**
- 使用 **set_clip_rounded_rect** 自动命中测试
- 内部元素使用 **set_hit_transparent**
- 可聚焦控件调用 **set_focusable**
- 在 **on_arrange** 中排列所有子元素
- 内容变更时触发 **invalidate_measure**
- 对齐方式变更时触发 **invalidate_arrange**
