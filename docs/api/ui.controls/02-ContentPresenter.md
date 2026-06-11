# ContentPresenter 类 API 文档

## 1. 概述

`ContentPresenter` 是 MineFramework 控件模板系统的核心基础设施,作为 `ControlTemplate` 视觉树中的"内容槽"(Content Slot),负责将 `ContentControl` 的内容渲染到模板指定位置。它是实现控件外观与内容分离的关键组件。

**核心职责：**

- **内容代理渲染**：自动获取 `TemplatedParent` (模板化父控件)的 `ContentProperty`,并根据内容类型选择合适的渲染策略
- **类型智能切换**：
  - 内容为 `InlineString`：自动创建内联 `TextBlock` 渲染文本,复用 Presenter 的字体/前景色属性
  - 内容为 `UIElement*`：将该元素加入视觉树,委托其测量/布局/渲染
- **模板绑定桥梁**：在 ControlTemplate 应用时,自动建立 `presenter.Content ← templatedParent.Content` 的单向模板绑定,无需手动配置
- **外观属性继承**：提供 `Foreground`、`FontSize`、`Padding` 等依赖属性,供模板设计者控制文本内容的显示效果

**继承关系：**

```
DependencyObject
    └── Visual
            └── UIElement
                    └── FrameworkElement
                            └── Control
                                    └── ContentPresenter  ← 本文档
```

**架构意义：**

1. **模板化核心**：没有 ContentPresenter,ControlTemplate 无法渲染 ContentControl 的内容。它是模板系统与内容系统的连接器
2. **自动化魔法**：开发者在 MML 中只需写 `<ContentPresenter />`,框架自动完成以下工作：
   - 查找 `TemplatedParent.ContentProperty`
   - 建立模板绑定
   - 根据内容类型创建渲染策略
   - 同步字体/前景色等外观属性
3. **性能优化**：文本内容使用内联 TextBlock 渲染,避免创建完整的 Control 实例
4. **扩展点**：派生类可重写 `on_measure()` 自定义测量逻辑,实现特殊布局需求(如 ScrollViewer 中的 ScrollContentPresenter)

**典型用途：**

- **Button ControlTemplate**：在模板中放置 ContentPresenter,展示按钮的 Icon 或文本
- **ListBoxItem ControlTemplate**：渲染列表项的数据内容(可能是字符串、对象或复杂 UI)
- **ScrollViewer ControlTemplate**：ScrollContentPresenter 派生类处理滚动内容的裁剪与偏移
- **自定义控件模板**：任何需要展示动态内容的控件,都通过 ContentPresenter 实现

**设计原则：**

- **约定优于配置**：默认绑定到 `TemplatedParent.ContentProperty`,大多数场景无需显式配置
- **类型透明**：用户无需关心内容是文本还是元素,Presenter 自动处理
- **属性转发**：将自身的 `Foreground`、`FontSize` 等属性传递给内联 TextBlock,保持一致性
- **测量委托**：对于 UIElement 内容,完全委托其测量/布局,不干预子元素逻辑

**性能特性：**

- **延迟创建**：内联 TextBlock 仅在内容为文本时创建,UIElement 内容直接复用
- **单次绑定**：模板绑定在 ControlTemplate 应用时建立,后续无额外开销
- **直通测量**：UIElement 内容的测量/布局请求直接转发,无中间层损耗

**相关类型：**

- `ContentControl`：提供 ContentProperty,ContentPresenter 的渲染源
- `ControlTemplate`：控件模板,包含 ContentPresenter 的视觉树
- `TemplateBinding`：模板绑定表达式,ContentPresenter 自动创建此绑定
- `ScrollContentPresenter`：派生类,处理滚动内容的特殊测量逻辑

---

## 2. 文件位置

**头文件：**
```
src/mine/ui/controls/ContentPresenter.h
```

**实现文件：**
```
src/mine/ui/controls/ContentPresenter.cpp
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
#include <mine/core/Variant.h>
```

---

## 3. 类定义

### 3.1 完整类声明

```cpp
namespace mine::ui {

/// 控件模板内容占位元素
/// 
/// 在 ControlTemplate 视觉树中作为"内容槽",负责渲染 TemplatedParent 的 Content。
/// 根据内容类型自动选择渲染策略:
/// - InlineString: 创建内联 TextBlock 渲染文本
/// - UIElement*: 将元素加入视觉树并委托测量/布局
/// 
/// 模板绑定在 ControlTemplate.Apply() 时自动建立,无需手动配置。
class MINE_API ContentPresenter : public Control {
    MINE_RTTI_DECLARE(ContentPresenter, Control)

public:
    /// Content 依赖属性 (Variant 类型)
    /// 默认值: 空 Variant
    /// 继承性: 否
    /// 动画性: 否
    /// 说明: 由 ControlTemplate 自动绑定到 TemplatedParent.ContentProperty
    static const DependencyProperty& ContentProperty;

    /// Padding 依赖属性 (Thickness 类型)
    /// 默认值: Thickness{0, 0, 0, 0}
    /// 继承性: 否
    /// 动画性: 是 (支持 ThicknessAnimation)
    /// 说明: 内容与 Presenter 边界的间距,影响测量结果
    static const DependencyProperty& PaddingProperty;

    /// Foreground 依赖属性 (Brush 类型)
    /// 默认值: Brush::from_color(Colors::Black)
    /// 继承性: 是 (从父元素继承)
    /// 动画性: 是 (支持 BrushAnimation)
    /// 说明: 文本内容的前景色,传递给内联 TextBlock
    static const DependencyProperty& ForegroundProperty;

    /// FontSize 依赖属性 (float 类型)
    /// 默认值: 14.0f
    /// 继承性: 是 (从父元素继承)
    /// 动画性: 是 (支持 DoubleAnimation)
    /// 说明: 文本内容的字体大小(像素),传递给内联 TextBlock
    static const DependencyProperty& FontSizeProperty;

    /// 构造函数
    ContentPresenter();

    /// 析构函数
    ~ContentPresenter() override;

    // ==================== 内容访问接口 ====================

    /// 获取原始内容 Variant
    /// @return 存储 InlineString 或 UIElement* 的 Variant 引用
    [[nodiscard]] const core::Variant& content() const noexcept;

    /// 设置内容 (通常由模板绑定自动设置,很少手动调用)
    /// @param content 要展示的内容 (InlineString 或 UIElement*)
    void set_content(const core::Variant& content);

    // ==================== 外观属性访问器 ====================

    /// 获取内边距
    [[nodiscard]] math::Thickness padding() const noexcept;

    /// 设置内边距
    /// @param padding 内容与边界的间距
    void set_padding(math::Thickness padding);

    /// 获取前景色
    [[nodiscard]] paint::Brush foreground() const noexcept;

    /// 设置前景色
    /// @param brush 文本内容的前景色
    void set_foreground(paint::Brush brush);

    /// 获取字体大小
    /// @return 字体大小(像素)
    [[nodiscard]] float font_size() const noexcept;

    /// 设置字体大小
    /// @param size_px 字体大小(像素)
    void set_font_size(float size_px);

    // ==================== 字体管理 ====================

    /// 设置字体对象 (高级接口)
    /// @param font_face HarfBuzz 字体对象指针 (hb_font_t*)
    /// @note 通常由 FontManager 自动管理,用户很少直接调用
    void set_font_face(void* font_face) noexcept;

    /// 设置文本对齐方式
    /// @param align 水平对齐方式 (Left/Center/Right)
    void set_text_alignment(TextAlignment align) noexcept;

    /// 设置是否使用墨水对齐
    /// @param enabled true 表示使用字形边界对齐, false 表示使用度量边界对齐
    /// @note 墨水对齐可消除视觉偏移,但可能导致跨字体不对齐
    void set_use_ink_alignment(bool enabled) noexcept;

protected:
    // ==================== 布局虚方法重写 ====================

    /// 测量内容所需尺寸
    /// @param available_size 父元素提供的可用空间
    /// @note 根据内容类型调用不同的测量逻辑:
    ///       - 文本: 调用 TextBlock 的测量逻辑
    ///       - 元素: 委托子元素的 Measure() 方法
    void on_measure(math::Size available_size) override;

    /// 排列内容到最终矩形
    /// @param final_rect 父元素分配的最终位置和尺寸
    /// @note 根据内容类型调用不同的排列逻辑:
    ///       - 文本: 计算文本块的渲染位置
    ///       - 元素: 委托子元素的 Arrange() 方法
    void on_arrange(math::Rect final_rect) override;

private:
    // 内部实现细节(隐藏在 .cpp 中)
    
    /// 内联 TextBlock,用于渲染字符串内容
    /// 仅在内容为 InlineString 时创建,UIElement 内容时为 nullptr
    TextBlock* inline_text_block_ = nullptr;

    /// 当前渲染的内容元素
    /// - 内容为 InlineString 时: 指向 inline_text_block_
    /// - 内容为 UIElement* 时: 指向该元素
    /// - 内容为空时: nullptr
    UIElement* rendered_content_ = nullptr;

    /// 字体对象缓存 (hb_font_t*)
    void* font_face_ = nullptr;

    /// 文本对齐方式
    TextAlignment text_alignment_ = TextAlignment::Left;

    /// 是否使用墨水对齐
    bool use_ink_alignment_ = false;

    /// 内容变更回调
    void on_content_changed(const core::Variant& old_v, const core::Variant& new_v);

    /// 创建或更新内联 TextBlock
    void ensure_inline_text_block();

    /// 清理旧内容
    void clear_old_content();

    /// 同步外观属性到内联 TextBlock
    void sync_text_properties();
};

} // namespace mine::ui
```

### 3.2 RTTI 宏说明

```cpp
MINE_RTTI_DECLARE(ContentPresenter, Control)
```

展开后生成:
- `TypeInfo()` 静态方法: 返回 `ContentPresenter` 类型元数据
- `GetType()` 虚方法: 返回当前实例的运行时类型
- 支持 `dynamic_cast` 等效的 `CastTo<T>()` 方法

### 3.3 依赖属性注册

```cpp
// ContentProperty 注册
DependencyProperty::Register<ContentPresenter>(
    "Content",
    PropertyMetadata::create<core::Variant>()
        .default_value(core::Variant{})
        .flags(PropertyMetadata::AffectsMeasure | PropertyMetadata::AffectsRender)
        .property_changed_callback([](DependencyObject* obj, const auto& e) {
            static_cast<ContentPresenter*>(obj)->on_content_changed(e.old_value, e.new_value);
        })
);

// PaddingProperty 注册
DependencyProperty::Register<ContentPresenter>(
    "Padding",
    PropertyMetadata::create<math::Thickness>()
        .default_value(math::Thickness{0, 0, 0, 0})
        .flags(PropertyMetadata::AffectsMeasure)
);

// ForegroundProperty 注册(继承属性)
DependencyProperty::Register<ContentPresenter>(
    "Foreground",
    PropertyMetadata::create<paint::Brush>()
        .default_value(paint::Brush::from_color(Colors::Black))
        .flags(PropertyMetadata::Inherits | PropertyMetadata::AffectsRender)
);

// FontSizeProperty 注册(继承属性)
DependencyProperty::Register<ContentPresenter>(
    "FontSize",
    PropertyMetadata::create<float>()
        .default_value(14.0f)
        .flags(PropertyMetadata::Inherits | PropertyMetadata::AffectsMeasure)
);
```

---

## 4. 成员方法

### 4.1 静态依赖属性

#### ContentProperty

```cpp
static const DependencyProperty& ContentProperty;
```

**描述：** 存储要展示的内容,类型为 `core::Variant`。

**模板绑定：** 在 ControlTemplate 应用时,ContentPresenter 自动查找 `TemplatedParent` 的 ContentProperty,并建立单向绑定:

```cpp
// 框架自动执行(用户无需手动调用)
presenter->SetBinding(
    ContentPresenter::ContentProperty,
    TemplateBinding{
        ContentControl::ContentProperty,  // 源属性
        templated_parent                   // 源对象
    }
);
```

**支持的类型：**
- `core::InlineString`：纯文本内容
- `UIElement*`：任意可视化元素

**变更回调：** 内容变化时触发 `on_content_changed()`,根据新类型创建/更新渲染元素。

---

#### PaddingProperty

```cpp
static const DependencyProperty& PaddingProperty;
```

**描述：** 内容与 Presenter 边界的内边距。

**影响：**
- `on_measure()` 从可用空间中减去 Padding,传递给内容元素
- `on_arrange()` 根据 Padding 调整内容的最终位置

**示例：**
```cpp
presenter->set_padding(Thickness{16, 8, 16, 8});  // 左16 上8 右16 下8
```

---

#### ForegroundProperty

```cpp
static const DependencyProperty& ForegroundProperty;
```

**描述：** 文本内容的前景色,仅对 `InlineString` 类型的内容有效。

**继承性：** 此属性标记为 `Inherits`,未显式设置时从父元素继承。

**传递机制：** 设置后自动同步到内联 TextBlock:

```cpp
void ContentPresenter::set_foreground(paint::Brush brush) {
    SetValue(ForegroundProperty, brush);
    
    // 如果内容是文本,同步到 TextBlock
    if (inline_text_block_) {
        inline_text_block_->set_foreground(brush);
    }
}
```

---

#### FontSizeProperty

```cpp
static const DependencyProperty& FontSizeProperty;
```

**描述：** 文本内容的字体大小(像素单位)。

**继承性：** 标记为 `Inherits`,未设置时从父元素继承。

**默认值：** 14.0f

**传递机制：** 与 `ForegroundProperty` 相同,自动同步到内联 TextBlock。

---

### 4.2 构造函数

#### ContentPresenter()

```cpp
ContentPresenter();
```

**描述：** 默认构造函数,初始化基类 `Control` 并设置默认属性值。

**初始化逻辑：**
```cpp
ContentPresenter::ContentPresenter()
    : Control() {
    // 属性默认值由依赖属性系统管理
    // 无需手动初始化
}
```

**后置条件：**
- `content()` 返回空 Variant
- `inline_text_block_` 和 `rendered_content_` 为 `nullptr`
- `padding()` 返回 `Thickness{0, 0, 0, 0}`
- `foreground()` 返回黑色画刷
- `font_size()` 返回 14.0f

---

### 4.3 析构函数

#### ~ContentPresenter()

```cpp
~ContentPresenter() override;
```

**描述：** 虚析构函数,清理内联 TextBlock 和内容元素。

**清理逻辑：**
1. 如果 `inline_text_block_ != nullptr`,从视觉树移除并释放
2. 如果 `rendered_content_` 是外部 UIElement,仅从视觉树移除(不释放,由所有者管理)
3. 释放字体对象引用(如果持有)

**异常安全性：** `noexcept`,保证析构不抛出异常。

---

### 4.4 内容访问方法

#### content()

```cpp
[[nodiscard]] const core::Variant& content() const noexcept;
```

**描述：** 获取当前内容的原始 Variant。

**返回值：** 对 `ContentProperty` 当前值的常量引用。

**使用场景：**
- 检查内容类型: `content().is<InlineString>()`
- 获取原始内容值: `content().as<UIElement*>()`
- 调试输出内容信息

**示例：**
```cpp
const auto& var = presenter->content();
if (var.is<core::InlineString>()) {
    std::cout << "文本: " << var.as<core::InlineString>() << std::endl;
} else if (var.is<UIElement*>()) {
    std::cout << "元素: " << var.as<UIElement*>()->GetType().name() << std::endl;
}
```

---

#### set_content()

```cpp
void set_content(const core::Variant& content);
```

**描述：** 手动设置内容(通常由模板绑定自动设置,很少需要手动调用)。

**参数：**
- `content`：要展示的内容,必须是 `InlineString` 或 `UIElement*`

**行为：**
1. 调用 `SetValue(ContentProperty, content)`
2. 触发 `on_content_changed()` 回调
3. 根据新内容类型创建/更新渲染元素
4. 标记 `InvalidateMeasure()` 和 `InvalidateVisual()`

**示例：**
```cpp
// 设置文本内容
presenter->set_content(core::Variant{core::InlineString{"Hello"}});

// 设置元素内容
auto image = core::make_owned<Image>();
presenter->set_content(core::Variant{image.get()});
image.release();
```

---

### 4.5 外观属性访问器

#### padding()

```cpp
[[nodiscard]] math::Thickness padding() const noexcept;
```

**描述：** 获取内边距。

**返回值：** `Thickness` 结构体,包含 `left`、`top`、`right`、`bottom` 四个 float 值。

---

#### set_padding()

```cpp
void set_padding(math::Thickness padding);
```

**描述：** 设置内边距,影响内容的测量和布局。

**参数：**
- `padding`：内边距值,所有分量必须 >= 0

**行为：**
- 调用 `SetValue(PaddingProperty, padding)`
- 自动触发 `InvalidateMeasure()`,重新测量布局

**示例：**
```cpp
// 均匀内边距
presenter->set_padding(Thickness{8, 8, 8, 8});

// 水平/垂直不同
presenter->set_padding(Thickness{16, 8, 16, 8});  // 左右16,上下8

// 四边不同
presenter->set_padding(Thickness{10, 5, 12, 8});
```

---

#### foreground()

```cpp
[[nodiscard]] paint::Brush foreground() const noexcept;
```

**描述：** 获取文本前景色。

**返回值：** `paint::Brush` 对象,可能是 SolidColorBrush、LinearGradientBrush 等。

---

#### set_foreground()

```cpp
void set_foreground(paint::Brush brush);
```

**描述：** 设置文本前景色,仅对字符串内容有效。

**参数：**
- `brush`：前景色画刷

**行为：**
1. 调用 `SetValue(ForegroundProperty, brush)`
2. 如果内容是文本,自动同步到内联 TextBlock
3. 触发 `InvalidateVisual()`,重新绘制

**示例：**
```cpp
// 纯色
presenter->set_foreground(Brush::from_color(Colors::Red));

// 渐变色
auto gradient = LinearGradientBrush::create(
    Point{0, 0}, Point{100, 0},
    {Colors::Blue, Colors::Purple}
);
presenter->set_foreground(gradient);
```

---

#### font_size()

```cpp
[[nodiscard]] float font_size() const noexcept;
```

**描述：** 获取字体大小(像素)。

**返回值：** 浮点数,表示字体高度(像素单位)。

---

#### set_font_size()

```cpp
void set_font_size(float size_px);
```

**描述：** 设置字体大小,仅对字符串内容有效。

**参数：**
- `size_px`：字体大小(像素), 必须 > 0

**行为：**
1. 调用 `SetValue(FontSizeProperty, size_px)`
2. 如果内容是文本,同步到内联 TextBlock
3. 触发 `InvalidateMeasure()`,重新测量文本尺寸

**示例：**
```cpp
presenter->set_font_size(18.0f);  // 18 像素字体
```

---

### 4.6 高级字体管理

#### set_font_face()

```cpp
void set_font_face(void* font_face) noexcept;
```

**描述：** 设置 HarfBuzz 字体对象(高级接口)。

**参数：**
- `font_face`：HarfBuzz `hb_font_t*` 指针

**使用场景：**
- 自定义字体加载逻辑
- 使用非系统字体
- 精确控制字体渲染参数

**注意事项：**
- 通常由 `FontManager` 自动管理,用户很少直接调用
- 调用者负责管理字体对象的生命周期
- 必须在内容为文本时调用,否则无效

**示例：**
```cpp
// 通常通过 FontManager 加载
auto* font = FontManager::instance()->load_font("Segoe UI", 14.0f);
presenter->set_font_face(font);
```

---

#### set_text_alignment()

```cpp
void set_text_alignment(TextAlignment align) noexcept;
```

**描述：** 设置文本对齐方式,仅对字符串内容有效。

**参数：**
- `align`：对齐方式枚举值 (`TextAlignment::Left`/`Center`/`Right`)

**行为：**
- 更新内部 `text_alignment_` 字段
- 如果内容是文本,同步到内联 TextBlock
- 触发 `InvalidateArrange()`,重新排列文本位置

**示例：**
```cpp
presenter->set_text_alignment(TextAlignment::Center);  // 居中对齐
```

---

#### set_use_ink_alignment()

```cpp
void set_use_ink_alignment(bool enabled) noexcept;
```

**描述：** 设置是否使用墨水对齐(Ink Alignment)。

**参数：**
- `enabled`：`true` 使用字形边界对齐, `false` 使用度量边界对齐

**墨水对齐 vs 度量对齐：**
- **度量对齐**：基于字体设计的度量边界(包括字距、行距等虚拟空间),跨字体一致但可能有视觉偏移
- **墨水对齐**：基于实际绘制的字形边界,消除视觉偏移但可能导致跨字体不对齐

**使用场景：**
- 标题文本使用墨水对齐,消除边缘留白
- 段落文本使用度量对齐,保持基线一致

**示例：**
```cpp
// 标题使用墨水对齐
title_presenter->set_use_ink_alignment(true);

// 正文使用度量对齐
body_presenter->set_use_ink_alignment(false);
```

---

### 4.7 布局虚方法重写

#### on_measure()

```cpp
void on_measure(math::Size available_size) override;
```

**描述：** 测量内容所需的尺寸,根据内容类型选择不同策略。

**参数：**
- `available_size`：父元素提供的可用空间

**测量逻辑：**

**1. 文本内容 (InlineString)：**
```cpp
void ContentPresenter::on_measure(math::Size available_size) {
    auto padding = this->padding();
    
    // 减去 Padding
    auto content_size = Size{
        std::max(0.0f, available_size.width - padding.left - padding.right),
        std::max(0.0f, available_size.height - padding.top - padding.bottom)
    };
    
    if (inline_text_block_) {
        // 测量内联 TextBlock
        inline_text_block_->Measure(content_size);
        auto text_size = inline_text_block_->DesiredSize();
        
        // 加上 Padding
        set_desired_size(Size{
            text_size.width + padding.left + padding.right,
            text_size.height + padding.top + padding.bottom
        });
    }
}
```

**2. 元素内容 (UIElement*)：**
```cpp
void ContentPresenter::on_measure(math::Size available_size) {
    auto padding = this->padding();
    
    auto content_size = Size{
        std::max(0.0f, available_size.width - padding.left - padding.right),
        std::max(0.0f, available_size.height - padding.top - padding.bottom)
    };
    
    if (rendered_content_) {
        // 委托子元素测量
        rendered_content_->Measure(content_size);
        auto elem_size = rendered_content_->DesiredSize();
        
        set_desired_size(Size{
            elem_size.width + padding.left + padding.right,
            elem_size.height + padding.top + padding.bottom
        });
    }
}
```

**3. 空内容：**
```cpp
// 返回仅包含 Padding 的尺寸
set_desired_size(Size{
    padding.left + padding.right,
    padding.top + padding.bottom
});
```

---

#### on_arrange()

```cpp
void on_arrange(math::Rect final_rect) override;
```

**描述：** 将内容排列到最终矩形,根据内容类型委托布局。

**参数：**
- `final_rect`：父元素分配的最终位置和尺寸

**排列逻辑：**

```cpp
void ContentPresenter::on_arrange(math::Rect final_rect) {
    auto padding = this->padding();
    
    // 计算内容区域
    auto content_rect = Rect{
        final_rect.x + padding.left,
        final_rect.y + padding.top,
        std::max(0.0f, final_rect.width - padding.left - padding.right),
        std::max(0.0f, final_rect.height - padding.top - padding.bottom)
    };
    
    if (rendered_content_) {
        // 委托子元素排列
        rendered_content_->Arrange(content_rect);
    }
}
```

**注意事项：**
- 文本内容和元素内容使用相同的排列逻辑(都委托给 `rendered_content_`)
- Padding 会减少内容的可用空间
- 如果内容的 DesiredSize 小于分配空间,会根据对齐方式调整位置(由子元素处理)

---

## 5. 使用场景

### 5.1 Button ControlTemplate 基础用法

最常见的用法是在 Button 模板中放置 ContentPresenter:

**MML 模板定义：**
```xml
<ControlTemplate target_type="Button">
    <Border name="PART_Border" 
            background="{bind Background, mode: TemplateBinding}"
            border_brush="{bind BorderBrush, mode: TemplateBinding}"
            border_thickness="{bind BorderThickness, mode: TemplateBinding}"
            corner_radius="4"
            padding="16,8">
        <ContentPresenter horizontal_alignment="Center"
                          vertical_alignment="Center" />
    </Border>
</ControlTemplate>
```

**工作原理：**
1. Button 设置 `content="确认"`
2. ControlTemplate 应用时,ContentPresenter 自动绑定到 `Button.ContentProperty`
3. ContentPresenter 检测到内容是字符串,创建内联 TextBlock 渲染 "确认"
4. 继承 Button 的 `Foreground` 和 `FontSize` 属性

**等效 C++ 代码：**
```cpp
auto create_button_template() -> ControlTemplate* {
    auto border = core::make_owned<Border>();
    border->set_name("PART_Border");
    border->bind_property(
        Border::BackgroundProperty,
        TemplateBinding{Button::BackgroundProperty}
    );
    border->set_corner_radius(CornerRadii{4, 4, 4, 4});
    border->set_padding(Thickness{16, 8, 16, 8});
    
    auto presenter = core::make_owned<ContentPresenter>();
    presenter->set_horizontal_alignment(HorizontalAlignment::Center);
    presenter->set_vertical_alignment(VerticalAlignment::Center);
    
    border->set_child(presenter.release());
    
    return ControlTemplate::from_root(border.release());
}
```

---

### 5.2 图标按钮模板

Button 内容为 Image 元素时,ContentPresenter 自动渲染图标:

**MML 模板：**
```xml
<ControlTemplate target_type="Button">
    <Border background="#00000000" 
            corner_radius="24"
            padding="12">
        <ContentPresenter />
        <!-- 自动展示 Button.Content (Image 元素) -->
    </Border>
</ControlTemplate>

<!-- 使用 -->
<Button>
    <Image source="assets/icons/save.svg" width="24" height="24" />
</Button>
```

**工作原理：**
1. Button 内容设置为 Image 元素
2. ContentPresenter 检测到内容是 UIElement*
3. 将 Image 加入视觉树,委托其测量/渲染
4. Image 自己负责加载 SVG 并绘制图标

---

### 5.3 自定义 Foreground 和 FontSize

在模板中显式设置 Presenter 的外观属性:

**MML 模板：**
```xml
<ControlTemplate target_type="Button">
    <Border background="{bind Background, mode: TemplateBinding}">
        <ContentPresenter foreground="#FFFFFF"
                          font_size="16"
                          padding="12,6"
                          horizontal_alignment="Center" />
    </Border>
</ControlTemplate>
```

**效果：**
- 按钮文本始终显示为白色,无论 Button.Foreground 设置为何值
- 字体大小固定为 16 像素
- 内容与边界有 12,6 的内边距

**适用场景：**
- 特殊按钮样式(如主题色按钮,文本必须为白色)
- 固定尺寸按钮(字体大小不随系统设置变化)

---

### 5.4 模板绑定 Presenter 属性

将 Presenter 的属性绑定到 Button 的自定义属性:

**自定义 Button 类：**
```cpp
class CustomButton : public Button {
public:
    static const DependencyProperty& ContentForegroundProperty;
    static const DependencyProperty& ContentPaddingProperty;
    
    void set_content_foreground(paint::Brush brush);
    void set_content_padding(math::Thickness padding);
};
```

**MML 模板：**
```xml
<ControlTemplate target_type="CustomButton">
    <Border background="{bind Background, mode: TemplateBinding}">
        <ContentPresenter foreground="{bind ContentForeground, mode: TemplateBinding}"
                          padding="{bind ContentPadding, mode: TemplateBinding}" />
    </Border>
</ControlTemplate>

<!-- 使用 -->
<CustomButton content="按钮"
              content_foreground="#FF0000"
              content_padding="20,10" />
```

**优势：**
- 用户可以独立控制 Presenter 的外观
- 模板与控件属性解耦,易于扩展

---

### 5.5 复杂内容模板(ListBox Item)

ListBox 的 ItemTemplate 中使用 ContentPresenter 渲染数据:

**MML 定义：**
```xml
<ListBox items_source="{bind vm.Users}">
    <!-- ItemContainerStyle: 定义每个 ListBoxItem 的样式 -->
    <ListBox.ItemContainerStyle>
        <Style target_type="ListBoxItem">
            <Setter property="Template">
                <ControlTemplate target_type="ListBoxItem">
                    <Border name="PART_Border" background="transparent" padding="8">
                        <ContentPresenter />
                        <!-- 展示 ListBoxItem.Content (User 对象) -->
                    </Border>
                </ControlTemplate>
            </Setter>
        </Style>
    </ListBox.ItemContainerStyle>
    
    <!-- ItemTemplate: 定义 User 对象的可视化表示 -->
    <ListBox.ItemTemplate>
        <DataTemplate>
            <StackPanel orientation="Horizontal" spacing="8">
                <Image source="{bind Avatar}" width="32" height="32" />
                <TextBlock text="{bind Name}" vertical_alignment="Center" />
            </StackPanel>
        </DataTemplate>
    </ListBox.ItemTemplate>
</ListBox>
```

**工作流程：**
1. ListBox 将数据源中的每个 `User` 对象包装到 `ListBoxItem.Content`
2. ListBoxItem 的 ControlTemplate 中,ContentPresenter 展示 `User` 对象
3. ContentPresenter 检测到内容不是字符串也不是 UIElement,查找 `ItemTemplate`
4. 应用 DataTemplate,创建 StackPanel + Image + TextBlock 的可视化树
5. DataTemplate 中的绑定表达式绑定到 `User` 对象的属性

---

### 5.6 嵌套 ContentPresenter(不推荐)

理论上可以嵌套多个 ContentPresenter,但通常没有实际意义:

```xml
<ControlTemplate target_type="Button">
    <Border>
        <ContentPresenter>
            <!-- 内容为 UIElement 时,可能包含另一个 ContentPresenter -->
            <!-- 但这通常是设计错误,应简化视觉树 -->
        </ContentPresenter>
    </Border>
</ControlTemplate>
```

**问题：**
- 增加视觉树深度,影响性能
- 模板绑定逻辑复杂化
- 难以理解和维护

**正确做法：** 使用单层 ContentPresenter,通过 DataTemplate 控制内容的可视化表示。

---

### 5.7 ScrollViewer 模板中的 ScrollContentPresenter

ScrollContentPresenter 是 ContentPresenter 的派生类,处理滚动内容的特殊逻辑:

**MML 模板：**
```xml
<ControlTemplate target_type="ScrollViewer">
    <Grid>
        <ScrollContentPresenter name="PART_ContentPresenter" />
        <!-- 自动展示 ScrollViewer.Content,并应用滚动偏移 -->
        
        <ScrollBar name="PART_VerticalScrollBar" orientation="Vertical" />
        <ScrollBar name="PART_HorizontalScrollBar" orientation="Horizontal" />
    </Grid>
</ControlTemplate>
```

**ScrollContentPresenter 特殊功能：**
- 重写 `on_arrange()`,根据滚动偏移调整内容位置
- 实现内容裁剪,仅渲染可见区域
- 处理滚动手势和鼠标滚轮事件

**C++ 实现概要：**
```cpp
class ScrollContentPresenter : public ContentPresenter {
protected:
    void on_arrange(math::Rect final_rect) override {
        auto offset = get_scroll_offset();  // 从 ScrollViewer 获取
        
        // 应用滚动偏移
        auto content_rect = Rect{
            final_rect.x - offset.x,
            final_rect.y - offset.y,
            content_desired_width_,   // 内容的实际宽度
            content_desired_height_   // 内容的实际高度
        };
        
        if (rendered_content_) {
            rendered_content_->Arrange(content_rect);
        }
        
        // 设置裁剪区域
        set_clip(Rect{0, 0, final_rect.width, final_rect.height});
    }
};
```

---

### 5.8 动态切换内容类型

在运行时切换按钮内容在文本和图标之间:

```cpp
class DynamicButton : public Button {
public:
    void set_mode(Mode mode) {
        if (mode == Mode::Text) {
            // 切换为文本内容
            set_content("保存");
        } else {
            // 切换为图标内容
            auto icon = core::make_owned<Image>();
            icon->set_source("assets/icons/save.svg");
            icon->set_width(20.0f);
            icon->set_height(20.0f);
            set_content(icon.get());
            icon.release();
        }
    }
};

// 使用
auto button = core::make_owned<DynamicButton>();
button->set_mode(Mode::Icon);  // 显示图标

// 1 秒后切换为文本
timer->schedule(1000ms, [=] {
    button->set_mode(Mode::Text);
});
```

**ContentPresenter 的行为：**
- 文本模式：创建内联 TextBlock,渲染 "保存"
- 图标模式：移除 TextBlock,将 Image 加入视觉树
- 切换时自动触发 `InvalidateMeasure()`,重新计算尺寸

---

## 6. 最佳实践

### 6.1 让模板绑定自动工作,避免手动设置 Content

**推荐：**
```xml
<ControlTemplate target_type="Button">
    <Border>
        <!-- ContentPresenter 自动绑定到 Button.Content -->
        <ContentPresenter />
    </Border>
</ControlTemplate>
```

**不推荐：**
```xml
<ControlTemplate target_type="Button">
    <Border>
        <!-- 手动绑定是多余的,框架已自动处理 -->
        <ContentPresenter content="{bind Content, mode: TemplateBinding}" />
    </Border>
</ControlTemplate>
```

**理由：**
- 框架在 ControlTemplate.Apply() 时自动查找 TemplatedParent.ContentProperty
- 手动绑定增加维护成本,且容易出错(如绑定错误的属性)
- 遵循"约定优于配置"原则,代码更简洁

---

### 6.2 使用 Padding 控制内容间距,避免嵌套容器

**推荐：**
```xml
<ControlTemplate target_type="Button">
    <Border background="{bind Background, mode: TemplateBinding}">
        <ContentPresenter padding="16,8" />
        <!-- 直接在 Presenter 上设置 Padding -->
    </Border>
</ControlTemplate>
```

**不推荐：**
```xml
<ControlTemplate target_type="Button">
    <Border background="{bind Background, mode: TemplateBinding}">
        <Border padding="16,8">
            <!-- 额外的 Border 仅为了 Padding,增加视觉树深度 -->
            <ContentPresenter />
        </Border>
    </Border>
</ControlTemplate>
```

**理由：**
- ContentPresenter 本身支持 Padding,无需额外容器
- 减少视觉树深度,提升测量/布局/渲染性能
- 简化模板结构,降低维护成本

---

### 6.3 文本属性优先使用继承,避免硬编码

**推荐：**
```xml
<ControlTemplate target_type="Button">
    <Border>
        <ContentPresenter />
        <!-- Foreground 和 FontSize 从 Button 继承 -->
    </Border>
</ControlTemplate>

<!-- 使用时设置 Button 属性 -->
<Button content="按钮" foreground="#FF0000" font_size="18" />
```

**不推荐：**
```xml
<ControlTemplate target_type="Button">
    <Border>
        <ContentPresenter foreground="#FF0000" font_size="18" />
        <!-- 硬编码,用户无法自定义 -->
    </Border>
</ControlTemplate>
```

**理由：**
- `Foreground` 和 `FontSize` 标记为继承属性,自动从父元素传递
- 用户可以通过设置 Button 属性控制文本外观
- 硬编码降低灵活性,不符合可重用模板设计原则

**例外情况：** 特殊样式按钮(如主题色按钮)确实需要固定文本颜色时,可以硬编码。

---

### 6.4 避免在 Presenter 上设置对齐方式,让内容自己决定

**推荐：**
```xml
<ControlTemplate target_type="Button">
    <Border>
        <ContentPresenter />
        <!-- 内容元素自己设置对齐方式 -->
    </Border>
</ControlTemplate>

<!-- 使用时控制内容对齐 -->
<Button>
    <StackPanel horizontal_alignment="Left">
        <Image source="icon.svg" />
        <TextBlock text="按钮" />
    </StackPanel>
</Button>
```

**不推荐：**
```xml
<ControlTemplate target_type="Button">
    <Border>
        <ContentPresenter horizontal_alignment="Center" />
        <!-- 强制所有内容居中,缺乏灵活性 -->
    </Border>
</ControlTemplate>
```

**理由：**
- 内容元素可能有特定的对齐需求(如左对齐的导航按钮)
- Presenter 强制对齐会覆盖内容的对齐设置
- 让内容元素控制对齐,模板更灵活

**例外情况：** Button 这类标准控件的默认模板可以设置 `Center` 对齐,因为大多数按钮确实需要居中。

---

### 6.5 派生 ContentPresenter 时始终调用基类方法

**推荐：**
```cpp
class CustomPresenter : public ContentPresenter {
protected:
    void on_measure(math::Size available_size) override {
        // 先调用基类测量逻辑
        ContentPresenter::on_measure(available_size);
        
        // 再执行自定义逻辑
        auto desired = DesiredSize();
        // ... 修改 desired ...
        set_desired_size(desired);
    }
};
```

**不推荐：**
```cpp
class CustomPresenter : public ContentPresenter {
protected:
    void on_measure(math::Size available_size) override {
        // 忘记调用基类,内容不会被测量!
        
        // 自定义逻辑
        set_desired_size(Size{100, 100});
    }
};
```

**理由：**
- 基类的 `on_measure()` 负责测量内容元素或内联 TextBlock
- 跳过基类调用会导致内容不显示或尺寸错误
- 保持良好的继承规范,避免微妙的 bug

---

## 7. 常见陷阱

### 7.1 误以为 ContentPresenter 拥有内容元素的所有权

**问题代码：**
```cpp
void create_template() {
    auto presenter = core::make_owned<ContentPresenter>();
    
    {
        auto image = core::make_owned<Image>();
        presenter->set_content(core::Variant{image.get()});
        // image 在此处被销毁!
    }
    
    // presenter 现在持有悬垂指针
}
```

**正确做法：**
```cpp
void create_template() {
    auto presenter = core::make_owned<ContentPresenter>();
    
    auto image = core::make_owned<Image>();
    presenter->set_content(core::Variant{image.get()});
    image.release();  // 将所有权转移给视觉树
}
```

**解释：**
- ContentPresenter 存储原始指针,不管理所有权
- 内容元素的生命周期由视觉树父子关系管理
- 必须确保元素在 Presenter 使用期间保持有效

---

### 7.2 在非模板场景手动创建 ContentPresenter

**问题代码：**
```cpp
// 错误: 试图用 ContentPresenter 作为普通容器
auto panel = core::make_owned<StackPanel>();
auto presenter = core::make_owned<ContentPresenter>();
presenter->set_content(core::Variant{core::InlineString{"文本"}});
panel->add_child(presenter.release());
```

**正确做法：** 直接使用目标控件

```cpp
// 正确: 使用 TextBlock 显示文本
auto panel = core::make_owned<StackPanel>();
auto text = core::make_owned<TextBlock>();
text->set_text("文本");
panel->add_child(text.release());
```

**解释：**
- ContentPresenter 设计用于 ControlTemplate,依赖 TemplatedParent
- 在普通视觉树中使用会失去模板绑定功能
- 直接使用 TextBlock、Image 等控件更高效

---

### 7.3 忘记 Presenter 的 Padding 影响内容尺寸

**问题代码：**
```cpp
// Button 模板
auto border = core::make_owned<Border>();
border->set_padding(Thickness{16, 8, 16, 8});  // Border 有 Padding

auto presenter = core::make_owned<ContentPresenter>();
presenter->set_padding(Thickness{8, 4, 8, 4});  // Presenter 也有 Padding
border->set_child(presenter.release());

// 实际效果: 内容与边框距离为 24(16+8), 12(8+4)
// 用户可能只期望 16, 8 的间距
```

**正确做法：** 只在一个层级设置 Padding

```cpp
// 方案 1: Padding 设置在 Border
auto border = core::make_owned<Border>();
border->set_padding(Thickness{16, 8, 16, 8});

auto presenter = core::make_owned<ContentPresenter>();
// 不设置 Padding
border->set_child(presenter.release());

// 方案 2: Padding 设置在 Presenter
auto border = core::make_owned<Border>();
// 不设置 Padding

auto presenter = core::make_owned<ContentPresenter>();
presenter->set_padding(Thickness{16, 8, 16, 8});
border->set_child(presenter.release());
```

**解释：**
- Padding 在每个层级都会累加
- 多层 Padding 导致实际间距与预期不符
- 选择最合适的层级设置 Padding,保持模板简洁

---

### 7.4 过度依赖 Foreground/FontSize 属性

**问题场景：**
```xml
<ControlTemplate target_type="Button">
    <Border>
        <ContentPresenter foreground="#FF0000" font_size="18" />
    </Border>
</ControlTemplate>

<!-- 用户期望自定义颜色,但模板覆盖了 -->
<Button content="按钮" foreground="#0000FF" />
<!-- 实际显示: 红色,因为 Presenter 硬编码为 #FF0000 -->
```

**正确做法：** 使用模板绑定

```xml
<ControlTemplate target_type="Button">
    <Border>
        <ContentPresenter foreground="{bind Foreground, mode: TemplateBinding}"
                          font_size="{bind FontSize, mode: TemplateBinding}" />
    </Border>
</ControlTemplate>
```

**解释：**
- 直接设置 Presenter 属性会覆盖继承值
- 使用模板绑定保持用户设置的优先级
- 如果需要默认值,应在 Button 类中定义,而非模板中硬编码

---

### 7.5 混淆 ContentPresenter 和 ContentControl

**错误认知：** "ContentPresenter 是 ContentControl 的简化版"

**实际关系：**
- **ContentControl**：拥有 ContentProperty 的控件基类,管理内容生命周期
- **ContentPresenter**：ControlTemplate 中的渲染代理,将 ContentControl 的内容显示到模板中

**关键区别：**

| 特性 | ContentControl | ContentPresenter |
|------|----------------|------------------|
| 用途 | 控件基类,持有内容 | 模板元素,渲染内容 |
| 所有权 | 管理 Content 生命周期 | 不拥有内容,仅渲染 |
| 模板化 | 支持 ControlTemplate | 不支持模板(本身就是模板的一部分) |
| 绑定 | ContentProperty 绑定到 ViewModel | 自动绑定到 TemplatedParent.Content |
| 典型派生类 | Button, CheckBox, ListBoxItem | ScrollContentPresenter |

**使用场景区分：**
- 创建新控件类型 → 继承 ContentControl
- 设计控件模板 → 使用 ContentPresenter

---

### 7.6 在模板外使用 TemplateBinding

**问题代码：**
```cpp
// 错误: 在普通视觉树中使用 TemplateBinding
auto panel = core::make_owned<StackPanel>();
auto presenter = core::make_owned<ContentPresenter>();

presenter->bind_property(
    ContentPresenter::ForegroundProperty,
    TemplateBinding{Button::ForegroundProperty, some_button}
);
```

**解释：**
- `TemplateBinding` 是特殊的绑定类型,仅在 ControlTemplate 上下文中有效
- 在普通视觉树中应使用 `Binding` 或 `MultiBinding`

**正确做法：**
```cpp
// 使用普通绑定
auto panel = core::make_owned<StackPanel>();
auto presenter = core::make_owned<ContentPresenter>();

presenter->bind_property(
    ContentPresenter::ForegroundProperty,
    some_button,
    Button::ForegroundProperty,
    BindingMode::OneWay
);
```

---

## 8. 完整示例

### 8.1 带图标和徽章的通知按钮模板

实现一个完整的通知按钮,包含图标、文本、未读数徽章:

**自定义 Button 类：**
```cpp
#include <mine/ui/controls/Button.h>
#include <mine/ui/DependencyProperty.h>

class NotificationButton : public Button {
    MINE_RTTI_DECLARE(NotificationButton, Button)

public:
    /// 未读消息数依赖属性
    static const DependencyProperty& UnreadCountProperty;
    
    NotificationButton();
    
    [[nodiscard]] int unread_count() const noexcept;
    void set_unread_count(int count);
};

// 属性注册
const DependencyProperty& NotificationButton::UnreadCountProperty =
    DependencyProperty::Register<NotificationButton>(
        "UnreadCount",
        PropertyMetadata::create<int>()
            .default_value(0)
            .flags(PropertyMetadata::AffectsRender)
    );

NotificationButton::NotificationButton() : Button() {}

int NotificationButton::unread_count() const noexcept {
    return GetValue(UnreadCountProperty).as<int>();
}

void NotificationButton::set_unread_count(int count) {
    SetValue(UnreadCountProperty, count);
}
```

**ControlTemplate (MML)：**
```xml
<ControlTemplate target_type="NotificationButton">
    <Grid>
        <Border name="PART_Background"
                background="{bind Background, mode: TemplateBinding}"
                border_brush="{bind BorderBrush, mode: TemplateBinding}"
                border_thickness="{bind BorderThickness, mode: TemplateBinding}"
                corner_radius="4"
                padding="12,6">
            <StackPanel orientation="Horizontal" spacing="8">
                <!-- 图标 -->
                <Image source="assets/icons/bell.svg" width="20" height="20" />
                
                <!-- 文本 (通过 ContentPresenter 渲染) -->
                <ContentPresenter vertical_alignment="Center" />
            </StackPanel>
        </Border>
        
        <!-- 未读徽章 -->
        <Border name="PART_Badge"
                width="20" height="20"
                background="#DC3545"
                corner_radius="10"
                horizontal_alignment="Right"
                vertical_alignment="Top"
                margin="0,-8,-8,0"
                visibility="{bind UnreadCount, mode: TemplateBinding, converter: CountToVisibility}">
            <TextBlock text="{bind UnreadCount, mode: TemplateBinding}"
                       foreground="#FFFFFF"
                       font_size="10"
                       horizontal_alignment="Center"
                       vertical_alignment="Center" />
        </Border>
    </Grid>
</ControlTemplate>
```

**值转换器：**
```cpp
#include <mine/ui/binding/ValueConverter.h>

class CountToVisibilityConverter : public mvvm::IValueConverter {
public:
    core::Variant convert(
        const core::Variant& value,
        const TypeInfo* target_type,
        const core::Variant& parameter
    ) override {
        if (value.is<int>()) {
            int count = value.as<int>();
            return count > 0 ? Visibility::Visible : Visibility::Collapsed;
        }
        return Visibility::Collapsed;
    }

    core::Variant convert_back(
        const core::Variant& value,
        const TypeInfo* target_type,
        const core::Variant& parameter
    ) override {
        throw std::runtime_error("CountToVisibilityConverter: convert_back not supported");
    }
};

// 注册
mvvm::ValueConverterRegistry::register_converter(
    "CountToVisibility",
    core::make_owned<CountToVisibilityConverter>()
);
```

**使用示例：**
```cpp
void create_notification_button() {
    auto button = core::make_owned<NotificationButton>();
    button->set_content("通知");
    button->set_unread_count(5);
    
    button->add_handler(Button::ClickEvent(), [button_ptr = button.get()](auto& e) {
        // 打开通知面板
        show_notification_panel();
        
        // 清除徽章
        button_ptr->set_unread_count(0);
    });
    
    toolbar->add_child(button.release());
}
```

**MML 使用：**
```xml
<NotificationButton content="通知" 
                    unread_count="{bind vm.UnreadNotificationCount}" 
                    on_click="on_notification_clicked" />
```

---

### 8.2 自定义 ScrollContentPresenter

实现一个支持滚动的 ContentPresenter 派生类:

```cpp
#include <mine/ui/controls/ContentPresenter.h>
#include <mine/math/Vector2.h>

class ScrollContentPresenter : public ContentPresenter {
    MINE_RTTI_DECLARE(ScrollContentPresenter, ContentPresenter)

public:
    /// ScrollOffset 依赖属性 (Vector2 类型)
    static const DependencyProperty& ScrollOffsetProperty;
    
    /// CanHorizontallyScroll 依赖属性 (bool 类型)
    static const DependencyProperty& CanHorizontallyScrollProperty;
    
    /// CanVerticallyScroll 依赖属性 (bool 类型)
    static const DependencyProperty& CanVerticallyScrollProperty;
    
    ScrollContentPresenter();
    
    [[nodiscard]] math::Vector2 scroll_offset() const noexcept;
    void set_scroll_offset(math::Vector2 offset);
    
    [[nodiscard]] bool can_horizontally_scroll() const noexcept;
    void set_can_horizontally_scroll(bool enabled);
    
    [[nodiscard]] bool can_vertically_scroll() const noexcept;
    void set_can_vertically_scroll(bool enabled);

protected:
    void on_measure(math::Size available_size) override;
    void on_arrange(math::Rect final_rect) override;

private:
    math::Size content_extent_;  // 内容的实际尺寸
};

// 属性注册
const DependencyProperty& ScrollContentPresenter::ScrollOffsetProperty =
    DependencyProperty::Register<ScrollContentPresenter>(
        "ScrollOffset",
        PropertyMetadata::create<math::Vector2>()
            .default_value(math::Vector2{0, 0})
            .flags(PropertyMetadata::AffectsArrange)
    );

// ... 其他属性注册 ...

ScrollContentPresenter::ScrollContentPresenter() : ContentPresenter() {}

math::Vector2 ScrollContentPresenter::scroll_offset() const noexcept {
    return GetValue(ScrollOffsetProperty).as<math::Vector2>();
}

void ScrollContentPresenter::set_scroll_offset(math::Vector2 offset) {
    SetValue(ScrollOffsetProperty, offset);
}

// 测量逻辑: 允许内容使用无限空间
void ScrollContentPresenter::on_measure(math::Size available_size) {
    auto padding = this->padding();
    
    // 计算内容可用空间
    auto content_width = can_horizontally_scroll() 
        ? std::numeric_limits<float>::infinity()
        : std::max(0.0f, available_size.width - padding.left - padding.right);
        
    auto content_height = can_vertically_scroll()
        ? std::numeric_limits<float>::infinity()
        : std::max(0.0f, available_size.height - padding.top - padding.bottom);
    
    if (auto* content = content_element()) {
        content->Measure(Size{content_width, content_height});
        content_extent_ = content->DesiredSize();
        
        // Presenter 的期望尺寸为可滚动方向的实际内容尺寸,
        // 不可滚动方向为可用空间
        set_desired_size(Size{
            can_horizontally_scroll() 
                ? content_extent_.width + padding.left + padding.right
                : available_size.width,
            can_vertically_scroll()
                ? content_extent_.height + padding.top + padding.bottom
                : available_size.height
        });
    } else {
        ContentPresenter::on_measure(available_size);
    }
}

// 排列逻辑: 应用滚动偏移
void ScrollContentPresenter::on_arrange(math::Rect final_rect) {
    auto padding = this->padding();
    auto offset = scroll_offset();
    
    // 应用滚动偏移
    auto content_rect = Rect{
        final_rect.x + padding.left - offset.x,
        final_rect.y + padding.top - offset.y,
        std::max(content_extent_.width, 
                 final_rect.width - padding.left - padding.right),
        std::max(content_extent_.height,
                 final_rect.height - padding.top - padding.bottom)
    };
    
    if (auto* content = content_element()) {
        content->Arrange(content_rect);
    }
    
    // 设置裁剪区域
    set_clip(Rect{
        0, 0,
        final_rect.width,
        final_rect.height
    });
}
```

**ScrollViewer ControlTemplate：**
```xml
<ControlTemplate target_type="ScrollViewer">
    <Grid>
        <Grid.ColumnDefinitions>
            <ColumnDefinition width="*" />
            <ColumnDefinition width="Auto" />
        </Grid.ColumnDefinitions>
        <Grid.RowDefinitions>
            <RowDefinition height="*" />
            <RowDefinition height="Auto" />
        </Grid.RowDefinitions>
        
        <!-- 内容区域 -->
        <ScrollContentPresenter name="PART_ContentPresenter"
                                grid_row="0" grid_column="0"
                                scroll_offset="{bind ScrollOffset, mode: TemplateBinding}"
                                can_horizontally_scroll="{bind CanHorizontallyScroll, mode: TemplateBinding}"
                                can_vertically_scroll="{bind CanVerticallyScroll, mode: TemplateBinding}" />
        
        <!-- 垂直滚动条 -->
        <ScrollBar name="PART_VerticalScrollBar"
                   grid_row="0" grid_column="1"
                   orientation="Vertical" />
        
        <!-- 水平滚动条 -->
        <ScrollBar name="PART_HorizontalScrollBar"
                   grid_row="1" grid_column="0"
                   orientation="Horizontal" />
    </Grid>
</ControlTemplate>
```

---

### 8.3 MVVM 完整示例: 用户资料卡片

**ViewModel:**
```cpp
#include <mine/mvvm/ViewModelBase.h>

class UserProfileCardViewModel : public mvvm::ViewModelBase {
    MINE_RTTI_DECLARE(UserProfileCardViewModel, mvvm::ViewModelBase)

public:
    MINE_PROPERTY(core::InlineString, user_name, "张三")
    MINE_PROPERTY(core::InlineString, user_title, "高级工程师")
    MINE_PROPERTY(core::InlineString, avatar_url, "")
    MINE_PROPERTY(int, follower_count, 0)
    MINE_PROPERTY(int, following_count, 0)
    
    UserProfileCardViewModel() {
        load_profile();
    }

private:
    void load_profile() {
        // 模拟异步加载
        async::post([this] {
            std::this_thread::sleep_for(500ms);
            
            dispatch_to_ui_thread([this] {
                set_user_name("李四");
                set_user_title("资深架构师");
                set_avatar_url("https://avatar.example.com/user123.jpg");
                set_follower_count(1234);
                set_following_count(567);
            });
        });
    }
};
```

**自定义 ContentControl (MML):**
```xml
<ContentControl name="UserProfileCard">
    <Border background="#FFFFFF"
            border_brush="#E0E0E0"
            border_thickness="1"
            corner_radius="8"
            padding="16">
        <StackPanel orientation="Vertical" spacing="12">
            <!-- 顶部: 头像 + 名字 -->
            <StackPanel orientation="Horizontal" spacing="12">
                <!-- 头像 -->
                <Border width="64" height="64" corner_radius="32">
                    <Image source="{bind vm.avatar_url}" />
                </Border>
                
                <!-- 名字 + 职位 -->
                <StackPanel orientation="Vertical" spacing="4" vertical_alignment="Center">
                    <TextBlock text="{bind vm.user_name}" 
                               font_size="18" 
                               font_weight="Bold" />
                    <TextBlock text="{bind vm.user_title}" 
                               font_size="14" 
                               foreground="#666666" />
                </StackPanel>
            </StackPanel>
            
            <!-- 中间: 关注信息 -->
            <Grid>
                <Grid.ColumnDefinitions>
                    <ColumnDefinition width="*" />
                    <ColumnDefinition width="*" />
                </Grid.ColumnDefinitions>
                
                <!-- 粉丝 -->
                <Button grid_column="0" on_click="on_followers_clicked">
                    <!-- ContentPresenter 渲染此内容 -->
                    <StackPanel orientation="Vertical" spacing="4">
                        <TextBlock text="{bind vm.follower_count}" 
                                   font_size="20" 
                                   font_weight="Bold"
                                   horizontal_alignment="Center" />
                        <TextBlock text="粉丝" 
                                   font_size="12" 
                                   foreground="#666666"
                                   horizontal_alignment="Center" />
                    </StackPanel>
                </Button>
                
                <!-- 关注 -->
                <Button grid_column="1" on_click="on_following_clicked">
                    <StackPanel orientation="Vertical" spacing="4">
                        <TextBlock text="{bind vm.following_count}" 
                                   font_size="20" 
                                   font_weight="Bold"
                                   horizontal_alignment="Center" />
                        <TextBlock text="关注" 
                                   font_size="12" 
                                   foreground="#666666"
                                   horizontal_alignment="Center" />
                    </StackPanel>
                </Button>
            </Grid>
            
            <!-- 底部: 操作按钮 -->
            <StackPanel orientation="Horizontal" spacing="8">
                <Button content="发消息" horizontal_alignment="Stretch" />
                <Button content="关注" background="#0078D4" foreground="#FFFFFF" />
            </StackPanel>
        </StackPanel>
    </Border>
</ContentControl>
```

**Button ControlTemplate (使用 ContentPresenter):**
```xml
<ControlTemplate target_type="Button">
    <Border name="PART_Background"
            background="{bind Background, mode: TemplateBinding}"
            border_brush="{bind BorderBrush, mode: TemplateBinding}"
            border_thickness="{bind BorderThickness, mode: TemplateBinding}"
            corner_radius="{bind CornerRadius, mode: TemplateBinding}"
            padding="{bind Padding, mode: TemplateBinding}">
        <ContentPresenter foreground="{bind Foreground, mode: TemplateBinding}"
                          font_size="{bind FontSize, mode: TemplateBinding}"
                          horizontal_alignment="Center"
                          vertical_alignment="Center" />
    </Border>
    
    <!-- Visual State Manager: 处理 Hovered/Pressed 状态 -->
    <VisualStateManager.VisualStateGroups>
        <VisualStateGroup name="CommonStates">
            <VisualState name="Normal" />
            <VisualState name="Hovered">
                <Storyboard>
                    <ColorAnimation target="PART_Background"
                                    property="Background.Color"
                                    to="#F0F0F0"
                                    duration="100ms" />
                </Storyboard>
            </VisualState>
            <VisualState name="Pressed">
                <Storyboard>
                    <ColorAnimation target="PART_Background"
                                    property="Background.Color"
                                    to="#E0E0E0"
                                    duration="100ms" />
                </Storyboard>
            </VisualState>
        </VisualStateGroup>
    </VisualStateManager.VisualStateGroups>
</ControlTemplate>
```

**工作流程：**
1. 两个 Button ("粉丝"/"关注") 的 Content 设置为 StackPanel 子树
2. Button 的 ControlTemplate 应用时,ContentPresenter 自动绑定到 Button.Content
3. ContentPresenter 检测到内容是 UIElement (StackPanel),将其加入视觉树
4. StackPanel 中的 TextBlock 通过数据绑定显示 ViewModel 属性
5. 用户点击按钮,触发 `on_followers_clicked` 事件处理器

---

## 9. 总结

### 9.1 核心要点

- **ContentPresenter** 是 ControlTemplate 的核心基础设施,负责渲染 ContentControl 的内容到模板中
- **模板绑定自动化**：框架自动将 Presenter.Content 绑定到 TemplatedParent.Content,无需手动配置
- **类型智能切换**：根据内容类型自动选择渲染策略(InlineString → 内联 TextBlock, UIElement → 委托渲染)
- **外观属性传递**：`Foreground`、`FontSize`、`Padding` 等属性传递给文本内容,保持一致性
- **测量/布局委托**：对于 UIElement 内容,完全委托其测量/布局,无中间层损耗

### 9.2 与 WPF 的对比

| 特性 | MineFramework ContentPresenter | WPF ContentPresenter |
|------|-------------------------------|----------------------|
| 内容类型 | InlineString / UIElement* | object (任意对象) |
| 模板绑定 | 自动,查找 ContentProperty | 自动,查找 ContentProperty |
| 文本渲染 | 内联 TextBlock (无额外控件) | ContentPresenter 本身渲染 |
| DataTemplate | 支持 (通过 ItemTemplate) | 相同 |
| 继承属性 | Foreground, FontSize | 相同 |
| 性能 | 内联文本零开销 | 稍有开销 |

### 9.3 性能特性

- **内联文本优化**：字符串内容使用内联 TextBlock,避免创建完整 Control 实例
- **延迟创建**：仅在内容类型匹配时创建渲染元素
- **单次绑定**：模板绑定在 ControlTemplate.Apply() 时建立,后续无额外开销
- **直通测量**：UIElement 内容的测量/布局请求直接转发,无中间层损耗

### 9.4 使用建议

1. **仅在 ControlTemplate 中使用 ContentPresenter**,普通视觉树应使用具体控件
2. **让模板绑定自动工作**,不要手动设置 `content` 属性
3. **使用 Padding 控制间距**,避免嵌套额外容器
4. **文本属性优先使用继承**,除非特殊样式需求
5. **派生 ContentPresenter 时始终调用基类方法**,保持继承规范

### 9.5 后续学习

- **ControlTemplate**：控件模板系统,ContentPresenter 的应用场景
- **DataTemplate**：数据模板,与 ContentPresenter 协同渲染复杂对象
- **ItemsPresenter**：多内容控件的模板占位符,ContentPresenter 的兄弟类
- **ScrollContentPresenter**：ContentPresenter 派生类,处理滚动内容的特殊逻辑

### 9.6 常见问题快速索引

| 问题 | 解决方案 | 参考章节 |
|------|----------|---------|
| 内容元素被提前销毁 | 先设置 content 再 release() | 7.1 |
| 在非模板场景使用 Presenter | 使用具体控件(TextBlock/Image) | 7.2 |
| Padding 累加导致间距错误 | 只在一个层级设置 Padding | 7.3 |
| 用户颜色设置被覆盖 | 使用模板绑定,不硬编码 | 7.4 |
| 混淆 Presenter 和 ContentControl | 理解二者职责差异 | 7.5 |
| 模板外使用 TemplateBinding | 使用普通 Binding | 7.6 |

---

**文档版本:** 1.0  
**最后更新:** 2025-01-10  
**作者:** MineUI 开发团队  
**许可证:** MIT License
