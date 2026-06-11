# Border 类 API 文档

## 1. 概述

`Border` 是 MineFramework 控件系统中的核心基元控件(Primitive Control),作为**组合式外观架构**的基石,提供背景、边框、圆角、单子元素容器等功能。它是实现复杂控件外观的首选构建块。

**核心职责：**

- **外观真相源**：所有外观参数(背景/边框画刷/粗细/圆角)均为 `DependencyProperty`,支持数据绑定、动画、样式继承
- **单子元素容器**：通过 `child` 属性承载一个子元素,提供标准的测量/布局/渲染管理
- **组合式外观模型**：复杂控件(如 Button)通过 `bind_property()` 将自己的外观属性同步到 Border,实现外观与逻辑分离
- **高性能渲染**：直接使用 Skia Canvas API 绘制,支持抗锯齿、阴影、渐变等高级效果

**继承关系：**

```
DependencyObject
    └── Visual
            └── UIElement
                    └── FrameworkElement
                            └── Control
                                    └── Border  ← 本文档
```

**架构意义：**

1. **合格基元控件**：完全符合"组合式外观架构"设计原则,所有外观参数均为 DP,无内部状态泄露
2. **可重用性极强**：任何需要背景/边框的场景都可使用 Border,避免重复实现渲染逻辑
3. **ControlTemplate 核心**：几乎所有控件模板的根元素都是 Border,提供一致的外观基础
4. **动画友好**：所有 DP 支持动画,可实现背景色过渡、边框宽度动画、圆角变形等效果

**典型用途：**

- **Button ControlTemplate**：Border 提供背景、边框、圆角,内部放置 ContentPresenter
- **卡片容器**：创建带阴影、圆角的卡片布局
- **分隔线**：设置 `border_thickness="0,0,0,1"` 实现底部分隔线
- **遮罩层**：半透明背景覆盖整个内容区域

**设计原则：**

- **纯外观控件**：不包含业务逻辑,仅负责渲染外观
- **依赖属性驱动**：所有外观参数通过 DP 暴露,支持绑定、动画、继承
- **单一职责**：仅处理背景/边框/圆角渲染 + 单子元素布局,不扩展其他功能
- **性能优先**：渲染代码高度优化,支持脏区检测,避免不必要的重绘

**性能特性：**

- **脏区优化**：仅在外观属性变更时重绘,布局不变时复用渲染缓存
- **批量绘制**：背景、边框、内容在同一 `on_render()` 调用中完成,减少 Canvas 状态切换
- **圆角裁剪**：自动生成圆角裁剪路径,子元素内容不会溢出边界
- **硬件加速**：通过 Skia 后端支持 GPU 加速,大量 Border 场景下性能优异

**相关类型：**

- `Brush`：背景和边框画刷,支持纯色、渐变、图案
- `Thickness`：边框粗细,四边可独立设置
- `CornerRadii`：圆角半径,四角可独立设置
- `UIElement`：子元素基类,Border 可承载任意 UIElement

---

## 2. 文件位置

**头文件：**
```
src/mine/ui/controls/Border.h
```

**实现文件：**
```
src/mine/ui/controls/Border.cpp
```

**模块归属：**
```
mine.ui.controls
```

**命名空间：**
```cpp
namespace mine::ui
```

**依赖头文件：**
```cpp
#include <mine/ui/Control.h>
#include <mine/ui/DependencyProperty.h>
#include <mine/ui/UIElement.h>
#include <mine/paint/Brush.h>
#include <mine/paint/Canvas.h>
#include <mine/math/Thickness.h>
#include <mine/math/CornerRadii.h>
```

---

## 3. 类定义

### 3.1 完整类声明

```cpp
namespace mine::ui {

/// 单子元素边框容器控件
/// 
/// 提供背景、边框、圆角渲染功能,是组合式外观架构的核心基元。
/// 所有外观参数均为依赖属性,支持数据绑定、动画、样式继承。
/// 
/// 典型用法:
/// - Button ControlTemplate 的根元素
/// - 卡片容器的外观层
/// - 分隔线 (border_thickness="0,0,0,1")
/// - 遮罩层 (半透明背景)
class MINE_API Border : public Control {
    MINE_RTTI_DECLARE(Border, Control)

public:
    /// Background 依赖属性 (Brush 类型)
    /// 默认值: Brush::transparent()
    /// 继承性: 否
    /// 动画性: 是 (支持 BrushAnimation)
    /// 说明: 背景画刷,在边框内部绘制
    static const DependencyProperty& BackgroundProperty;

    /// BorderBrush 依赖属性 (Brush 类型)
    /// 默认值: Brush::transparent()
    /// 继承性: 否
    /// 动画性: 是 (支持 BrushAnimation)
    /// 说明: 边框画刷,沿边界绘制
    static const DependencyProperty& BorderBrushProperty;

    /// BorderThickness 依赖属性 (Thickness 类型)
    /// 默认值: Thickness{0, 0, 0, 0}
    /// 继承性: 否
    /// 动画性: 是 (支持 ThicknessAnimation)
    /// 说明: 边框粗细,四边可独立设置
    static const DependencyProperty& BorderThicknessProperty;

    /// CornerRadius 依赖属性 (CornerRadii 类型)
    /// 默认值: CornerRadii{0, 0, 0, 0}
    /// 继承性: 否
    /// 动画性: 是 (支持 CornerRadiiAnimation)
    /// 说明: 圆角半径,四角可独立设置
    static const DependencyProperty& CornerRadiusProperty;

    /// 构造函数
    Border();

    /// 析构函数
    ~Border() override;

    // ==================== 子元素管理 ====================

    /// 获取子元素
    /// @return 当前子元素指针,无子元素时返回 nullptr
    [[nodiscard]] UIElement* child() const noexcept;

    /// 设置子元素 (原始指针版本)
    /// @param child 要设置的子元素,可为 nullptr 清空
    /// @note 调用者负责管理 child 的生命周期,Border 不获取所有权
    void set_child(UIElement* child);

    /// 设置子元素 (OwnedPtr 版本)
    /// @param child 要设置的子元素,Border 接管所有权
    void set_child(core::OwnedPtr<UIElement> child);

    // ==================== 外观属性访问器 ====================

    /// 获取背景画刷
    [[nodiscard]] paint::Brush background() const noexcept;

    /// 设置背景画刷
    /// @param brush 背景画刷,可为透明 (Brush::transparent())
    void set_background(paint::Brush brush);

    /// 获取边框画刷
    [[nodiscard]] paint::Brush border_brush() const noexcept;

    /// 设置边框画刷
    /// @param brush 边框画刷,可为透明
    void set_border_brush(paint::Brush brush);

    /// 获取边框粗细
    [[nodiscard]] math::Thickness border_thickness() const noexcept;

    /// 设置边框粗细
    /// @param thickness 边框粗细,所有分量必须 >= 0
    void set_border_thickness(math::Thickness thickness);

    /// 获取圆角半径
    [[nodiscard]] math::CornerRadii corner_radius() const noexcept;

    /// 设置圆角半径
    /// @param radii 圆角半径,所有分量必须 >= 0
    void set_corner_radius(math::CornerRadii radii);

protected:
    // ==================== 布局虚方法重写 ====================

    /// 测量 Border 所需尺寸
    /// @param available_size 父元素提供的可用空间
    /// @note 从可用空间中减去 BorderThickness,传递给子元素测量,
    ///       然后将子元素的 DesiredSize 加上 BorderThickness 作为自身期望尺寸
    void on_measure(math::Size available_size) override;

    /// 排列 Border 到最终矩形
    /// @param final_rect 父元素分配的最终位置和尺寸
    /// @note 从最终矩形中减去 BorderThickness,将剩余空间分配给子元素
    void on_arrange(math::Rect final_rect) override;

    /// 渲染 Border 外观和子元素
    /// @param canvas Skia Canvas 对象
    /// @note 渲染顺序: 背景 → 子元素 → 边框(确保边框在最上层)
    void on_render(paint::Canvas& canvas) override;

private:
    /// 当前子元素指针
    UIElement* child_ = nullptr;

    /// 子元素所有权管理器
    core::OwnedPtr<UIElement> child_owner_;

    /// 绘制背景
    void render_background(paint::Canvas& canvas, const math::Rect& bounds);

    /// 绘制边框
    void render_border(paint::Canvas& canvas, const math::Rect& bounds);

    /// 创建圆角矩形路径
    void create_rounded_rect_path(
        paint::Path& path,
        const math::Rect& rect,
        const math::CornerRadii& radii
    );
};

} // namespace mine::ui
```

### 3.2 依赖属性注册

```cpp
// BackgroundProperty 注册
const DependencyProperty& Border::BackgroundProperty =
    DependencyProperty::Register<Border>(
        "Background",
        PropertyMetadata::create<paint::Brush>()
            .default_value(paint::Brush::transparent())
            .flags(PropertyMetadata::AffectsRender)
    );

// BorderBrushProperty 注册
const DependencyProperty& Border::BorderBrushProperty =
    DependencyProperty::Register<Border>(
        "BorderBrush",
        PropertyMetadata::create<paint::Brush>()
            .default_value(paint::Brush::transparent())
            .flags(PropertyMetadata::AffectsRender)
    );

// BorderThicknessProperty 注册
const DependencyProperty& Border::BorderThicknessProperty =
    DependencyProperty::Register<Border>(
        "BorderThickness",
        PropertyMetadata::create<math::Thickness>()
            .default_value(math::Thickness{0, 0, 0, 0})
            .flags(PropertyMetadata::AffectsMeasure | PropertyMetadata::AffectsRender)
    );

// CornerRadiusProperty 注册
const DependencyProperty& Border::CornerRadiusProperty =
    DependencyProperty::Register<Border>(
        "CornerRadius",
        PropertyMetadata::create<math::CornerRadii>()
            .default_value(math::CornerRadii{0, 0, 0, 0})
            .flags(PropertyMetadata::AffectsRender)
    );
```

---

## 4. 成员方法

### 4.1 静态依赖属性

#### BackgroundProperty

```cpp
static const DependencyProperty& BackgroundProperty;
```

**描述：** 背景画刷依赖属性。

**类型：** `paint::Brush`

**默认值：** `Brush::transparent()`(完全透明)

**影响：** 标记为 `AffectsRender`,变更时触发重绘但不重新测量布局

**绑定示例：**
```cpp
// 绑定到 ViewModel
border->bind_property(
    Border::BackgroundProperty,
    view_model,
    "ButtonBackground"
);

// 模板绑定 (ControlTemplate 中)
border->bind_property(
    Border::BackgroundProperty,
    TemplateBinding{Button::BackgroundProperty}
);
```

---

#### BorderBrushProperty

```cpp
static const DependencyProperty& BorderBrushProperty;
```

**描述：** 边框画刷依赖属性。

**类型：** `paint::Brush`

**默认值：** `Brush::transparent()`

**影响：** 标记为 `AffectsRender`

**示例：**
```cpp
// 纯色边框
border->set_border_brush(Brush::from_color(Colors::Gray));

// 渐变边框
auto gradient = LinearGradientBrush::create(
    Point{0, 0}, Point{100, 0},
    {Colors::Blue, Colors::Purple}
);
border->set_border_brush(gradient);
```

---

#### BorderThicknessProperty

```cpp
static const DependencyProperty& BorderThicknessProperty;
```

**描述：** 边框粗细依赖属性。

**类型：** `math::Thickness`

**默认值：** `Thickness{0, 0, 0, 0}`(无边框)

**影响：** 标记为 `AffectsMeasure | AffectsRender`,变更时触发重新测量和重绘

**Thickness 结构：**
```cpp
struct Thickness {
    float left;    // 左边框粗细
    float top;     // 上边框粗细
    float right;   // 右边框粗细
    float bottom;  // 下边框粗细
    
    // 构造函数
    Thickness(float all);                        // 四边相同
    Thickness(float horizontal, float vertical); // 水平/垂直
    Thickness(float l, float t, float r, float b); // 四边独立
};
```

**示例：**
```cpp
// 四边相同
border->set_border_thickness(Thickness{1});  // 1 像素边框

// 水平/垂直不同
border->set_border_thickness(Thickness{2, 1});  // 左右2,上下1

// 四边独立
border->set_border_thickness(Thickness{1, 2, 3, 4});
```

---

#### CornerRadiusProperty

```cpp
static const DependencyProperty& CornerRadiusProperty;
```

**描述：** 圆角半径依赖属性。

**类型：** `math::CornerRadii`

**默认值：** `CornerRadii{0, 0, 0, 0}`(无圆角)

**影响：** 标记为 `AffectsRender`

**CornerRadii 结构：**
```cpp
struct CornerRadii {
    float top_left;     // 左上角半径
    float top_right;    // 右上角半径
    float bottom_right; // 右下角半径
    float bottom_left;  // 左下角半径
    
    // 构造函数
    CornerRadii(float all);                               // 四角相同
    CornerRadii(float tl, float tr, float br, float bl); // 四角独立
};
```

**示例：**
```cpp
// 四角相同
border->set_corner_radius(CornerRadii{8});  // 8 像素圆角

// 四角独立 (Material Design 风格)
border->set_corner_radius(CornerRadii{16, 16, 0, 0});  // 上圆角,下直角
```

---

### 4.2 构造函数

#### Border()

```cpp
Border();
```

**描述：** 默认构造函数,初始化基类 `Control` 并设置默认属性值。

**后置条件：**
- `child() == nullptr`
- `background()` 返回透明画刷
- `border_brush()` 返回透明画刷
- `border_thickness()` 返回 `Thickness{0, 0, 0, 0}`
- `corner_radius()` 返回 `CornerRadii{0, 0, 0, 0}`

---

### 4.3 析构函数

#### ~Border()

```cpp
~Border() override;
```

**描述：** 虚析构函数,清理子元素。

**行为：**
- 如果 `child_owner_` 持有所有权,自动释放子元素
- 从视觉树移除子元素

---

### 4.4 子元素管理

#### child()

```cpp
[[nodiscard]] UIElement* child() const noexcept;
```

**描述：** 获取当前子元素。

**返回值：** 子元素指针,无子元素时返回 `nullptr`

**示例：**
```cpp
if (auto* child = border->child()) {
    child->set_opacity(0.5f);
}
```

---

#### set_child(UIElement*)

```cpp
void set_child(UIElement* child);
```

**描述：** 设置子元素(原始指针版本)。

**参数：**
- `child`：要设置的子元素,可为 `nullptr` 清空

**行为：**
1. 如果已有旧子元素,从视觉树移除
2. 如果 `child != nullptr`,加入视觉树并设置父元素
3. 触发 `InvalidateMeasure()`,重新测量布局

**所有权：** Border 不获取所有权,调用者负责管理 `child` 的生命周期

**示例：**
```cpp
auto border = core::make_owned<Border>();
auto text = core::make_owned<TextBlock>();
text->set_text("Hello");

// 设置子元素(不转移所有权)
border->set_child(text.get());

// 稍后释放所有权给视觉树
window->set_content(border.release());
text.release();
```

---

#### set_child(OwnedPtr<UIElement>)

```cpp
void set_child(core::OwnedPtr<UIElement> child);
```

**描述：** 设置子元素(OwnedPtr 版本),Border 接管所有权。

**参数：**
- `child`：要设置的子元素,OwnedPtr 自动管理生命周期

**行为：**
1. 移除旧子元素
2. 将 `child` 的所有权转移到 `child_owner_`
3. 加入视觉树
4. 触发 `InvalidateMeasure()`

**推荐用法：** 大多数场景下优先使用此版本,生命周期管理更安全

**示例：**
```cpp
auto border = core::make_owned<Border>();
auto text = core::make_owned<TextBlock>();
text->set_text("Hello");

// Border 接管所有权
border->set_child(std::move(text));
```

---

### 4.5 外观属性访问器

#### background()

```cpp
[[nodiscard]] paint::Brush background() const noexcept;
```

**描述：** 获取背景画刷。

**返回值：** `paint::Brush` 对象

---

#### set_background()

```cpp
void set_background(paint::Brush brush);
```

**描述：** 设置背景画刷。

**参数：**
- `brush`：背景画刷,可为透明

**行为：**
- 调用 `SetValue(BackgroundProperty, brush)`
- 触发 `InvalidateVisual()`,重新绘制

**示例：**
```cpp
// 纯色背景
border->set_background(Brush::from_color(Color{0xFF, 0x00, 0x00}));  // 红色

// 渐变背景
auto gradient = LinearGradientBrush::create(
    Point{0, 0}, Point{0, 100},
    {Colors::LightBlue, Colors::DarkBlue}
);
border->set_background(gradient);

// 透明背景
border->set_background(Brush::transparent());
```

---

#### border_brush()

```cpp
[[nodiscard]] paint::Brush border_brush() const noexcept;
```

**描述：** 获取边框画刷。

**返回值：** `paint::Brush` 对象

---

#### set_border_brush()

```cpp
void set_border_brush(paint::Brush brush);
```

**描述：** 设置边框画刷。

**参数：**
- `brush`：边框画刷

**示例：**
```cpp
border->set_border_brush(Brush::from_color(Colors::Gray));
border->set_border_thickness(Thickness{1});  // 必须设置粗细才可见
```

---

#### border_thickness()

```cpp
[[nodiscard]] math::Thickness border_thickness() const noexcept;
```

**描述：** 获取边框粗细。

**返回值：** `math::Thickness` 结构体

---

#### set_border_thickness()

```cpp
void set_border_thickness(math::Thickness thickness);
```

**描述：** 设置边框粗细。

**参数：**
- `thickness`：边框粗细,所有分量必须 >= 0

**行为：**
- 调用 `SetValue(BorderThicknessProperty, thickness)`
- 触发 `InvalidateMeasure()` 和 `InvalidateVisual()`

**示例：**
```cpp
// 1 像素边框
border->set_border_thickness(Thickness{1});

// 底部分隔线
border->set_border_thickness(Thickness{0, 0, 0, 1});

// 左侧强调线
border->set_border_thickness(Thickness{3, 0, 0, 0});
```

---

#### corner_radius()

```cpp
[[nodiscard]] math::CornerRadii corner_radius() const noexcept;
```

**描述：** 获取圆角半径。

**返回值：** `math::CornerRadii` 结构体

---

#### set_corner_radius()

```cpp
void set_corner_radius(math::CornerRadii radii);
```

**描述：** 设置圆角半径。

**参数：**
- `radii`：圆角半径,所有分量必须 >= 0

**行为：**
- 调用 `SetValue(CornerRadiusProperty, radii)`
- 触发 `InvalidateVisual()`

**示例：**
```cpp
// 8 像素圆角
border->set_corner_radius(CornerRadii{8});

// 胶囊形 (高度的一半)
float height = 40.0f;
border->set_corner_radius(CornerRadii{height / 2});

// Material Design Fab 按钮 (完全圆形)
border->set_corner_radius(CornerRadii{28});  // 直径 56
```

---

### 4.6 布局虚方法重写

#### on_measure()

```cpp
void on_measure(math::Size available_size) override;
```

**描述：** 测量 Border 所需尺寸。

**参数：**
- `available_size`：父元素提供的可用空间

**测量逻辑：**

```cpp
void Border::on_measure(math::Size available_size) {
    auto thickness = border_thickness();
    
    // 从可用空间中减去边框粗细
    auto child_available = Size{
        std::max(0.0f, available_size.width - thickness.left - thickness.right),
        std::max(0.0f, available_size.height - thickness.top - thickness.bottom)
    };
    
    if (child_) {
        // 测量子元素
        child_->Measure(child_available);
        auto child_desired = child_->DesiredSize();
        
        // Border 期望尺寸 = 子元素 + 边框
        set_desired_size(Size{
            child_desired.width + thickness.left + thickness.right,
            child_desired.height + thickness.top + thickness.bottom
        });
    } else {
        // 无子元素,期望尺寸仅为边框粗细
        set_desired_size(Size{
            thickness.left + thickness.right,
            thickness.top + thickness.bottom
        });
    }
}
```

**示例：**
```
可用空间: 100 x 50
边框粗细: 2 (四边)
子元素期望尺寸: 80 x 30

子元素可用空间: 100 - 2*2 = 96, 50 - 2*2 = 46
Border 期望尺寸: 80 + 2*2 = 84, 30 + 2*2 = 34
```

---

#### on_arrange()

```cpp
void on_arrange(math::Rect final_rect) override;
```

**描述：** 排列 Border 到最终矩形。

**参数：**
- `final_rect`：父元素分配的最终位置和尺寸

**排列逻辑：**

```cpp
void Border::on_arrange(math::Rect final_rect) {
    auto thickness = border_thickness();
    
    // 子元素矩形 = 最终矩形 - 边框粗细
    auto child_rect = Rect{
        final_rect.x + thickness.left,
        final_rect.y + thickness.top,
        std::max(0.0f, final_rect.width - thickness.left - thickness.right),
        std::max(0.0f, final_rect.height - thickness.top - thickness.bottom)
    };
    
    if (child_) {
        // 排列子元素
        child_->Arrange(child_rect);
    }
}
```

**示例：**
```
最终矩形: (10, 20, 100, 50)  // x, y, width, height
边框粗细: 2 (四边)

子元素矩形: (10+2, 20+2, 100-4, 50-4) = (12, 22, 96, 46)
```

---

#### on_render()

```cpp
void on_render(paint::Canvas& canvas) override;
```

**描述：** 渲染 Border 外观和子元素。

**参数：**
- `canvas`：Skia Canvas 对象

**渲染顺序：**
1. **绘制背景**：填充整个 Border 区域(包括边框内部)
2. **渲染子元素**：递归调用 `child_->Render(canvas)`
3. **绘制边框**：沿边界绘制,确保边框在最上层

**伪代码：**
```cpp
void Border::on_render(paint::Canvas& canvas) {
    auto bounds = Rect{0, 0, ActualWidth(), ActualHeight()};
    
    // 1. 绘制背景
    auto bg = background();
    if (!bg.is_transparent()) {
        render_background(canvas, bounds);
    }
    
    // 2. 裁剪子元素到圆角区域
    auto radii = corner_radius();
    if (radii.is_rounded()) {
        canvas.save();
        Path clip_path;
        create_rounded_rect_path(clip_path, bounds, radii);
        canvas.clip_path(clip_path);
    }
    
    // 3. 渲染子元素
    if (child_ && child_->Visibility() == Visibility::Visible) {
        child_->Render(canvas);
    }
    
    if (radii.is_rounded()) {
        canvas.restore();
    }
    
    // 4. 绘制边框
    auto border = border_brush();
    auto thickness = border_thickness();
    if (!border.is_transparent() && thickness.has_value()) {
        render_border(canvas, bounds);
    }
}
```

---

## 5. 使用场景

### 5.1 Button ControlTemplate 根元素

最典型的用法是作为 Button 模板的根元素:

**MML 模板：**
```xml
<ControlTemplate target_type="Button">
    <Border name="PART_Background"
            background="{bind Background, mode: TemplateBinding}"
            border_brush="{bind BorderBrush, mode: TemplateBinding}"
            border_thickness="{bind BorderThickness, mode: TemplateBinding}"
            corner_radius="4"
            padding="16,8">
        <ContentPresenter />
    </Border>
</ControlTemplate>
```

**C++ 等效代码：**
```cpp
auto create_button_template() -> ControlTemplate* {
    auto border = core::make_owned<Border>();
    border->set_name("PART_Background");
    
    // 模板绑定
    border->bind_property(
        Border::BackgroundProperty,
        TemplateBinding{Button::BackgroundProperty}
    );
    border->bind_property(
        Border::BorderBrushProperty,
        TemplateBinding{Button::BorderBrushProperty}
    );
    border->bind_property(
        Border::BorderThicknessProperty,
        TemplateBinding{Button::BorderThicknessProperty}
    );
    
    // 固定外观
    border->set_corner_radius(CornerRadii{4});
    border->set_padding(Thickness{16, 8, 16, 8});
    
    // 添加内容占位符
    auto presenter = core::make_owned<ContentPresenter>();
    border->set_child(std::move(presenter));
    
    return ControlTemplate::from_root(border.release());
}
```

---

### 5.2 卡片容器

创建带阴影、圆角的 Material Design 卡片:

```cpp
#include <mine/ui/controls/Border.h>
#include <mine/ui/controls/TextBlock.h>
#include <mine/ui/layout/StackPanel.h>

auto create_card(core::StringView title, core::StringView content) 
    -> core::OwnedPtr<Border> {
    auto card = core::make_owned<Border>();
    
    // 外观设置
    card->set_background(Brush::from_color(Colors::White));
    card->set_border_brush(Brush::from_color(Color{0xE0, 0xE0, 0xE0}));
    card->set_border_thickness(Thickness{1});
    card->set_corner_radius(CornerRadii{8});
    card->set_padding(Thickness{16});
    
    // 阴影效果(通过 Effect 系统)
    card->set_effect(DropShadowEffect::create(
        Color{0x00, 0x00, 0x00, 0x20},  // 半透明黑色
        Vector2{0, 2},                   // 偏移 (x, y)
        4.0f                             // 模糊半径
    ));
    
    // 内容布局
    auto panel = core::make_owned<StackPanel>();
    panel->set_spacing(8.0f);
    
    auto title_text = core::make_owned<TextBlock>();
    title_text->set_text(title);
    title_text->set_font_size(18.0f);
    title_text->set_font_weight(FontWeight::Bold);
    panel->add_child(std::move(title_text));
    
    auto content_text = core::make_owned<TextBlock>();
    content_text->set_text(content);
    content_text->set_font_size(14.0f);
    content_text->set_foreground(Brush::from_color(Color{0x66, 0x66, 0x66}));
    panel->add_child(std::move(content_text));
    
    card->set_child(std::move(panel));
    
    return card;
}

// 使用
auto card = create_card("标题", "这是卡片内容描述文本...");
window->set_content(card.release());
```

---

### 5.3 分隔线

使用 Border 创建各种分隔线:

**水平分隔线：**
```cpp
auto create_divider() -> core::OwnedPtr<Border> {
    auto divider = core::make_owned<Border>();
    divider->set_height(1.0f);  // 1 像素高度
    divider->set_border_brush(Brush::from_color(Color{0xE0, 0xE0, 0xE0}));
    divider->set_border_thickness(Thickness{0, 0, 0, 1});  // 仅底部边框
    return divider;
}
```

**垂直分隔线：**
```cpp
auto create_vertical_divider() -> core::OwnedPtr<Border> {
    auto divider = core::make_owned<Border>();
    divider->set_width(1.0f);
    divider->set_border_brush(Brush::from_color(Color{0xE0, 0xE0, 0xE0}));
    divider->set_border_thickness(Thickness{0, 0, 1, 0});  // 仅右边框
    return divider;
}
```

**带渐变的分隔线：**
```cpp
auto create_gradient_divider() -> core::OwnedPtr<Border> {
    auto divider = core::make_owned<Border>();
    divider->set_height(2.0f);
    
    auto gradient = LinearGradientBrush::create(
        Point{0, 0}, Point{100, 0},
        {
            {Colors::Transparent, 0.0f},
            {Colors::Blue, 0.5f},
            {Colors::Transparent, 1.0f}
        }
    );
    divider->set_background(gradient);
    
    return divider;
}
```

---

### 5.4 遮罩层

半透明遮罩覆盖整个窗口:

```cpp
auto create_modal_overlay() -> core::OwnedPtr<Border> {
    auto overlay = core::make_owned<Border>();
    
    // 半透明黑色背景
    overlay->set_background(Brush::from_color(Color{0x00, 0x00, 0x00, 0x80}));
    
    // 点击遮罩关闭对话框
    overlay->add_handler(UIElement::MouseDownEvent(), [](RoutedEventArgs& e) {
        close_dialog();
        e.set_handled(true);
    });
    
    // 中心放置对话框
    auto dialog = core::make_owned<Border>();
    dialog->set_background(Brush::from_color(Colors::White));
    dialog->set_corner_radius(CornerRadii{8});
    dialog->set_padding(Thickness{24});
    dialog->set_width(400.0f);
    dialog->set_horizontal_alignment(HorizontalAlignment::Center);
    dialog->set_vertical_alignment(VerticalAlignment::Center);
    
    // ... 添加对话框内容 ...
    
    overlay->set_child(std::move(dialog));
    
    return overlay;
}
```

---

### 5.5 渐变背景按钮

使用 LinearGradientBrush 实现渐变背景:

```cpp
auto create_gradient_button(core::StringView text) 
    -> core::OwnedPtr<Button> {
    auto button = core::make_owned<Button>();
    button->set_content(text);
    
    // 创建渐变画刷
    auto gradient = LinearGradientBrush::create(
        Point{0, 0}, Point{0, 40},  // 垂直渐变
        {
            {Color{0x42, 0xA5, 0xF5}, 0.0f},  // 浅蓝
            {Color{0x1E, 0x88, 0xE5}, 1.0f}   // 深蓝
        }
    );
    
    button->set_background(gradient);
    button->set_border_thickness(Thickness{0});  // 无边框
    button->set_corner_radius(CornerRadii{4});
    button->set_foreground(Brush::from_color(Colors::White));
    
    return button;
}
```

---

### 5.6 圆形头像容器

使用圆角实现圆形裁剪:

```cpp
auto create_avatar(core::StringView image_url, float size) 
    -> core::OwnedPtr<Border> {
    auto avatar = core::make_owned<Border>();
    
    // 圆形裁剪 (半径 = 尺寸/2)
    avatar->set_width(size);
    avatar->set_height(size);
    avatar->set_corner_radius(CornerRadii{size / 2});
    
    // 图片内容
    auto image = core::make_owned<Image>();
    image->set_source(image_url);
    image->set_stretch(Stretch::UniformToFill);  // 填充整个容器
    
    avatar->set_child(std::move(image));
    
    // 可选: 添加边框
    avatar->set_border_brush(Brush::from_color(Colors::White));
    avatar->set_border_thickness(Thickness{2});
    
    return avatar;
}

// 使用
auto avatar = create_avatar("https://avatar.example.com/user.jpg", 48.0f);
```

---

### 5.7 状态指示器

使用 Border 创建各种状态指示器:

**在线状态点：**
```cpp
auto create_status_indicator(bool online) -> core::OwnedPtr<Border> {
    auto indicator = core::make_owned<Border>();
    
    indicator->set_width(12.0f);
    indicator->set_height(12.0f);
    indicator->set_corner_radius(CornerRadii{6.0f});  // 圆形
    
    auto color = online ? Colors::Green : Colors::Gray;
    indicator->set_background(Brush::from_color(color));
    
    // 白色边框
    indicator->set_border_brush(Brush::from_color(Colors::White));
    indicator->set_border_thickness(Thickness{2});
    
    return indicator;
}
```

**进度条：**
```cpp
class ProgressBar : public Border {
public:
    ProgressBar() {
        // 背景轨道
        set_height(4.0f);
        set_background(Brush::from_color(Color{0xE0, 0xE0, 0xE0}));
        set_corner_radius(CornerRadii{2});
        
        // 进度填充
        fill_ = core::make_owned<Border>();
        fill_->set_background(Brush::from_color(Colors::Blue));
        fill_->set_corner_radius(CornerRadii{2});
        fill_->set_horizontal_alignment(HorizontalAlignment::Left);
        
        set_child(fill_.get());
    }
    
    void set_value(float progress) {  // 0.0 ~ 1.0
        progress = std::clamp(progress, 0.0f, 1.0f);
        fill_->set_width(ActualWidth() * progress);
    }

private:
    core::OwnedPtr<Border> fill_;
};
```

---

### 5.8 边框动画

使用动画系统实现边框颜色/粗细过渡:

```cpp
void animate_border_on_hover(Border* border) {
    // 鼠标悬停时边框变粗
    auto storyboard = animation::Storyboard::create();
    
    auto thickness_anim = core::make_owned<animation::ThicknessAnimation>();
    thickness_anim->set_from(Thickness{1});
    thickness_anim->set_to(Thickness{3});
    thickness_anim->set_duration(200ms);
    thickness_anim->set_easing_function(animation::EasingFunction::ease_out());
    
    storyboard.add_animation(
        border,
        Border::BorderThicknessProperty,
        thickness_anim.release()
    );
    
    auto color_anim = core::make_owned<animation::BrushAnimation>();
    color_anim->set_from(Brush::from_color(Colors::Gray));
    color_anim->set_to(Brush::from_color(Colors::Blue));
    color_anim->set_duration(200ms);
    
    storyboard.add_animation(
        border,
        Border::BorderBrushProperty,
        color_anim.release()
    );
    
    auto story = storyboard.build();
    story->begin();
}

// 使用
border->add_handler(UIElement::MouseEnterEvent(), [border_ptr = border](auto& e) {
    animate_border_on_hover(border_ptr);
});
```

---

## 6. 最佳实践

### 6.1 优先使用 OwnedPtr 版本的 set_child()

**推荐：**
```cpp
auto border = core::make_owned<Border>();
auto text = core::make_owned<TextBlock>();

border->set_child(std::move(text));  // 所有权转移,安全
```

**不推荐：**
```cpp
auto border = core::make_owned<Border>();
auto text = core::make_owned<TextBlock>();

border->set_child(text.get());  // 需要手动管理 text 生命周期
text.release();
```

**理由：**
- `OwnedPtr` 版本自动管理生命周期,避免悬垂指针
- 代码更简洁,意图更明确
- 减少内存泄漏风险

---

### 6.2 边框粗细影响布局,需谨慎设置

**问题场景：**
```cpp
// 期望按钮宽度为 100
button->set_width(100.0f);
button->set_border_thickness(Thickness{2});  // 边框粗细 2

// 实际内容宽度: 100 - 2*2 = 96
// 如果 ContentPresenter 设置了最小宽度 100,布局会溢出!
```

**正确做法：** 考虑边框粗细调整控件尺寸

```cpp
// 期望内容宽度 100
float content_width = 100.0f;
float border = 2.0f;

button->set_width(content_width + border * 2);  // 104
button->set_border_thickness(Thickness{border});
```

---

### 6.3 使用透明背景而非不设置背景

**推荐：**
```cpp
border->set_background(Brush::transparent());  // 显式透明
```

**不推荐：**
```cpp
// 不设置背景 (依赖默认值)
```

**理由：**
- 显式设置意图更明确
- 避免依赖默认值,提高代码可读性
- 便于后续动画(从透明过渡到不透明)

---

### 6.4 圆角半径不应超过尺寸的一半

**问题代码：**
```cpp
auto border = core::make_owned<Border>();
border->set_width(50.0f);
border->set_height(30.0f);
border->set_corner_radius(CornerRadii{40});  // 超过高度一半!
// 渲染结果: 圆角被裁剪,形状不规则
```

**正确做法：**
```cpp
auto border = core::make_owned<Border>();
border->set_width(50.0f);
border->set_height(30.0f);

// 圆角半径 <= min(width, height) / 2
float max_radius = std::min(50.0f, 30.0f) / 2;  // 15
border->set_corner_radius(CornerRadii{max_radius});
```

---

### 6.5 避免在 on_render() 中修改属性

**问题代码：**
```cpp
class CustomBorder : public Border {
protected:
    void on_render(paint::Canvas& canvas) override {
        // 错误: 在渲染中修改属性
        if (some_condition) {
            set_background(Brush::from_color(Colors::Red));
        }
        
        Border::on_render(canvas);
    }
};
```

**正确做法：** 在事件处理器或属性变更回调中修改

```cpp
class CustomBorder : public Border {
public:
    CustomBorder() {
        add_handler(MouseEnterEvent(), [this](auto& e) {
            // 正确: 在事件处理器中修改
            set_background(Brush::from_color(Colors::Red));
        });
    }
};
```

**理由：**
- `on_render()` 应该是纯渲染逻辑,不应修改状态
- 在渲染中修改属性可能触发新的渲染循环,导致无限递归
- 属性修改应在布局/事件阶段完成

---

## 7. 常见陷阱

### 7.1 忘记设置边框粗细导致边框不可见

**问题代码：**
```cpp
border->set_border_brush(Brush::from_color(Colors::Black));
// 边框粗细仍为默认值 Thickness{0},边框不可见!
```

**正确做法：**
```cpp
border->set_border_brush(Brush::from_color(Colors::Black));
border->set_border_thickness(Thickness{1});  // 必须设置粗细
```

---

### 7.2 边框粗细导致子元素尺寸不符预期

**问题场景：**
```cpp
// 期望子元素尺寸为 100x50
auto border = core::make_owned<Border>();
border->set_width(100.0f);
border->set_height(50.0f);
border->set_border_thickness(Thickness{5});  // 较粗边框

auto text = core::make_owned<TextBlock>();
border->set_child(std::move(text));

// 实际子元素可用空间: (100-10) x (50-10) = 90 x 40
```

---

### 7.3 子元素生命周期管理错误

**问题代码：**
```cpp
void create_ui() {
    auto border = core::make_owned<Border>();
    
    {
        auto text = core::make_owned<TextBlock>();
        border->set_child(text.get());  // 仅存储指针
        // text 在此处被销毁!
    }
    
    window->set_content(border.release());
    // 运行时崩溃: border.child_ 指向已释放内存
}
```

**正确做法：**
```cpp
void create_ui() {
    auto border = core::make_owned<Border>();
    auto text = core::make_owned<TextBlock>();
    
    border->set_child(std::move(text));  // 转移所有权
    window->set_content(border.release());
}
```

---

### 7.4 过度嵌套 Border 影响性能

**问题代码：**
```cpp
// 过度嵌套,每层仅为了设置一个属性
auto outer = core::make_owned<Border>();
outer->set_background(Brush::from_color(Colors::White));

auto middle = core::make_owned<Border>();
middle->set_corner_radius(CornerRadii{8});
outer->set_child(middle.get());

auto inner = core::make_owned<Border>();
inner->set_padding(Thickness{16});
middle->set_child(inner.get());

auto content = create_content();
inner->set_child(std::move(content));
```

**正确做法：** 合并到一个 Border

```cpp
auto border = core::make_owned<Border>();
border->set_background(Brush::from_color(Colors::White));
border->set_corner_radius(CornerRadii{8});
border->set_padding(Thickness{16});

border->set_child(create_content());
```

---

### 7.5 混淆 Padding 和 BorderThickness

**错误认知：** "Padding 和 BorderThickness 效果相同,都是增加间距"

**实际区别：**

| 属性 | 作用区域 | 影响 | 渲染 |
|------|---------|------|------|
| BorderThickness | Border 外层 | 减少子元素可用空间 | 绘制边框线 |
| Padding | Control 基类属性 | 减少子元素可用空间 | 不绘制任何内容 |

**示例：**
```cpp
// BorderThickness: 绘制可见边框
border->set_border_thickness(Thickness{10});
border->set_border_brush(Brush::from_color(Colors::Red));
// 效果: 10 像素红色边框

// Padding: 仅增加间距,无视觉效果
border->set_padding(Thickness{10});
// 效果: 子元素与边界距离 10 像素,无可见内容
```

---

## 8. 完整示例

### 8.1 Material Design 按钮完整实现

实现一个符合 Material Design 规范的 Raised Button:

```cpp
#include <mine/ui/controls/Border.h>
#include <mine/ui/controls/Button.h>
#include <mine/ui/controls/ContentPresenter.h>
#include <mine/ui/animation/Storyboard.h>
#include <mine/ui/animation/ColorAnimation.h>
#include <mine/ui/animation/DoubleAnimation.h>

class MaterialButton : public Button {
    MINE_RTTI_DECLARE(MaterialButton, Button)

public:
    MaterialButton() {
        // 创建自定义模板
        apply_template(create_material_template());
        
        // 设置默认外观
        set_background(Brush::from_color(Color{0x21, 0x96, 0xF3}));  // MD Blue 500
        set_foreground(Brush::from_color(Colors::White));
        set_border_thickness(Thickness{0});
        set_corner_radius(CornerRadii{4});
        set_padding(Thickness{16, 8, 16, 8});
        set_min_width(64.0f);
        set_height(36.0f);
        
        // 绑定事件
        setup_animations();
    }

private:
    ControlTemplate* create_material_template() {
        auto root = core::make_owned<Border>();
        root->set_name("PART_Background");
        
        // 模板绑定
        root->bind_property(
            Border::BackgroundProperty,
            TemplateBinding{Button::BackgroundProperty}
        );
        root->bind_property(
            Border::BorderThicknessProperty,
            TemplateBinding{Button::BorderThicknessProperty}
        );
        root->bind_property(
            Border::CornerRadiusProperty,
            TemplateBinding{Button::CornerRadiusProperty}
        );
        root->bind_property(
            Border::PaddingProperty,
            TemplateBinding{Button::PaddingProperty}
        );
        
        // 阴影效果
        root->set_effect(DropShadowEffect::create(
            Color{0x00, 0x00, 0x00, 0x40},  // 40% 不透明度
            Vector2{0, 2},
            4.0f
        ));
        
        // 内容展示
        auto presenter = core::make_owned<ContentPresenter>();
        presenter->bind_property(
            ContentPresenter::ForegroundProperty,
            TemplateBinding{Button::ForegroundProperty}
        );
        presenter->set_horizontal_alignment(HorizontalAlignment::Center);
        presenter->set_vertical_alignment(VerticalAlignment::Center);
        
        root->set_child(std::move(presenter));
        
        return ControlTemplate::from_root(root.release());
    }
    
    void setup_animations() {
        // 鼠标悬停: 背景变浅
        add_handler(MouseEnterEvent(), [this](auto& e) {
            auto sb = animation::Storyboard::create();
            
            auto color_anim = core::make_owned<animation::ColorAnimation>();
            color_anim->set_from(Color{0x21, 0x96, 0xF3});  // Blue 500
            color_anim->set_to(Color{0x42, 0xA5, 0xF5});    // Blue 400 (浅色)
            color_anim->set_duration(150ms);
            
            sb.add_animation(this, Button::BackgroundProperty, color_anim.release());
            
            hover_story_ = sb.build();
            hover_story_->begin();
        });
        
        // 鼠标离开: 恢复原色
        add_handler(MouseLeaveEvent(), [this](auto& e) {
            if (hover_story_) {
                hover_story_->stop();
            }
            
            auto sb = animation::Storyboard::create();
            
            auto color_anim = core::make_owned<animation::ColorAnimation>();
            color_anim->set_to(Color{0x21, 0x96, 0xF3});
            color_anim->set_duration(150ms);
            
            sb.add_animation(this, Button::BackgroundProperty, color_anim.release());
            sb.build()->begin();
        });
        
        // 按下: 阴影增强
        add_handler(MouseDownEvent(), [this](auto& e) {
            auto border = find_template_child<Border>("PART_Background");
            if (!border) return;
            
            // 增强阴影
            border->set_effect(DropShadowEffect::create(
                Color{0x00, 0x00, 0x00, 0x60},
                Vector2{0, 4},
                8.0f
            ));
        });
        
        // 释放: 恢复阴影
        add_handler(MouseUpEvent(), [this](auto& e) {
            auto border = find_template_child<Border>("PART_Background");
            if (!border) return;
            
            border->set_effect(DropShadowEffect::create(
                Color{0x00, 0x00, 0x00, 0x40},
                Vector2{0, 2},
                4.0f
            ));
        });
    }
    
    core::OwnedPtr<animation::Storyboard> hover_story_;
};

// 使用
void create_material_buttons() {
    auto layout = core::make_owned<StackPanel>();
    layout->set_spacing(16.0f);
    
    // 主按钮
    auto primary = core::make_owned<MaterialButton>();
    primary->set_content("确认");
    layout->add_child(primary.release());
    
    // 次要按钮 (不同颜色)
    auto secondary = core::make_owned<MaterialButton>();
    secondary->set_content("取消");
    secondary->set_background(Brush::from_color(Color{0x9E, 0x9E, 0x9E}));  // Gray
    layout->add_child(secondary.release());
    
    window->set_content(layout.release());
}
```

---

### 8.2 自定义卡片组件

完整的卡片组件,包含头部、内容、操作区:

```cpp
#include <mine/ui/controls/Border.h>
#include <mine/ui/controls/TextBlock.h>
#include <mine/ui/layout/Grid.h>
#include <mine/ui/layout/StackPanel.h>

class Card : public Border {
    MINE_RTTI_DECLARE(Card, Border)

public:
    Card() {
        // Border 外观
        set_background(Brush::from_color(Colors::White));
        set_border_brush(Brush::from_color(Color{0xE0, 0xE0, 0xE0}));
        set_border_thickness(Thickness{1});
        set_corner_radius(CornerRadii{8});
        
        // 阴影
        set_effect(DropShadowEffect::create(
            Color{0x00, 0x00, 0x00, 0x20},
            Vector2{0, 2},
            4.0f
        ));
        
        // 根布局
        root_grid_ = core::make_owned<Grid>();
        root_grid_->add_row_definition(GridLength::auto_size());  // 头部
        root_grid_->add_row_definition(GridLength::star(1.0f));   // 内容
        root_grid_->add_row_definition(GridLength::auto_size());  // 操作区
        
        // 头部容器
        header_border_ = core::make_owned<Border>();
        header_border_->set_padding(Thickness{16, 12, 16, 12});
        header_border_->set_background(Brush::from_color(Color{0xF5, 0xF5, 0xF5}));
        Grid::set_row(header_border_.get(), 0);
        root_grid_->add_child(header_border_.get());
        
        // 内容容器
        content_border_ = core::make_owned<Border>();
        content_border_->set_padding(Thickness{16});
        Grid::set_row(content_border_.get(), 1);
        root_grid_->add_child(content_border_.get());
        
        // 操作区容器
        actions_border_ = core::make_owned<Border>();
        actions_border_->set_padding(Thickness{8});
        actions_border_->set_border_brush(Brush::from_color(Color{0xE0, 0xE0, 0xE0}));
        actions_border_->set_border_thickness(Thickness{0, 1, 0, 0});  // 顶部分隔线
        Grid::set_row(actions_border_.get(), 2);
        root_grid_->add_child(actions_border_.get());
        
        // 默认隐藏操作区
        actions_border_->set_visibility(Visibility::Collapsed);
        
        set_child(root_grid_.get());
        root_grid_.release();
    }
    
    /// 设置标题
    void set_title(core::StringView title) {
        auto text = core::make_owned<TextBlock>();
        text->set_text(title);
        text->set_font_size(16.0f);
        text->set_font_weight(FontWeight::Bold);
        
        header_border_->set_child(text.release());
    }
    
    /// 设置内容
    void set_content_element(core::OwnedPtr<UIElement> content) {
        content_border_->set_child(std::move(content));
    }
    
    /// 添加操作按钮
    void add_action(core::OwnedPtr<Button> button) {
        // 创建操作面板(如果不存在)
        if (!actions_panel_) {
            actions_panel_ = core::make_owned<StackPanel>();
            actions_panel_->set_orientation(Orientation::Horizontal);
            actions_panel_->set_spacing(8.0f);
            actions_panel_->set_horizontal_alignment(HorizontalAlignment::Right);
            actions_border_->set_child(actions_panel_.get());
        }
        
        actions_panel_->add_child(std::move(button));
        actions_border_->set_visibility(Visibility::Visible);
    }

private:
    core::OwnedPtr<Grid> root_grid_;
    core::OwnedPtr<Border> header_border_;
    core::OwnedPtr<Border> content_border_;
    core::OwnedPtr<Border> actions_border_;
    core::OwnedPtr<StackPanel> actions_panel_;
};

// 使用
void create_user_profile_card() {
    auto card = core::make_owned<Card>();
    card->set_width(320.0f);
    card->set_title("用户资料");
    
    // 内容
    auto content_panel = core::make_owned<StackPanel>();
    content_panel->set_spacing(12.0f);
    
    auto name_text = core::make_owned<TextBlock>();
    name_text->set_text("张三");
    name_text->set_font_size(20.0f);
    content_panel->add_child(name_text.release());
    
    auto email_text = core::make_owned<TextBlock>();
    email_text->set_text("zhangsan@example.com");
    email_text->set_foreground(Brush::from_color(Color{0x66, 0x66, 0x66}));
    content_panel->add_child(email_text.release());
    
    card->set_content_element(content_panel.release());
    
    // 操作按钮
    auto edit_btn = core::make_owned<Button>();
    edit_btn->set_content("编辑");
    card->add_action(edit_btn.release());
    
    auto delete_btn = core::make_owned<Button>();
    delete_btn->set_content("删除");
    delete_btn->set_background(Brush::from_color(Colors::Red));
    card->add_action(delete_btn.release());
    
    window->set_content(card.release());
}
```

---

## 9. 总结

### 9.1 核心要点

- **Border** 是组合式外观架构的核心基元控件,提供背景、边框、圆角、单子元素容器功能
- **所有外观参数均为依赖属性**,支持数据绑定、动画、样式继承
- **高性能渲染**,直接使用 Skia API,支持脏区优化和硬件加速
- **ControlTemplate 首选根元素**,几乎所有控件模板都使用 Border 作为根
- **边框粗细影响布局**,从可用空间中减去边框粗细后传递给子元素

### 9.2 关键特性

| 特性 | 说明 |
|------|------|
| 背景画刷 | 支持纯色、渐变、图案,完全透明到不透明 |
| 边框画刷 | 独立于背景,可设置不同颜色/渐变 |
| 边框粗细 | 四边可独立设置,影响子元素可用空间 |
| 圆角半径 | 四角可独立设置,自动裁剪子元素 |
| 单子元素 | 通过 `set_child()` 承载一个子元素 |
| 依赖属性 | 所有外观参数支持绑定/动画/继承 |

### 9.3 性能特性

- **脏区优化**：仅在外观属性变更时重绘
- **批量绘制**：背景/边框/内容在同一渲染调用中完成
- **圆角裁剪**：自动生成裁剪路径,子元素不溢出
- **硬件加速**：通过 Skia 后端支持 GPU 加速

### 9.4 使用建议

1. **优先使用 OwnedPtr 版本的 set_child()**,生命周期管理更安全
2. **考虑边框粗细对布局的影响**,调整控件尺寸以容纳边框
3. **使用透明背景而非不设置**,意图更明确
4. **圆角半径不超过尺寸一半**,避免渲染异常
5. **避免在 on_render() 中修改属性**,保持渲染逻辑纯净

### 9.5 常见问题快速索引

| 问题 | 解决方案 | 参考章节 |
|------|----------|---------|
| 边框不可见 | 设置 border_thickness | 7.1 |
| 子元素尺寸不符预期 | 考虑边框粗细影响 | 7.2 |
| 子元素生命周期错误 | 使用 OwnedPtr 版本 | 7.3 |
| 过度嵌套影响性能 | 合并多个 Border | 7.4 |
| 混淆 Padding 和 BorderThickness | 理解二者区别 | 7.5 |

---

**文档版本:** 1.0  
**最后更新:** 2025-01-10  
**作者:** MineUI 开发团队  
**许可证:** MIT License
