# TextBlock 类 API 文档

## 1. 概述

`TextBlock` 是 MineFramework 中的多行只读文本展示控件,提供丰富的排版功能,包括自动换行、文字对齐、行高控制、字间距、省略号裁剪等特性。它是所有文本显示场景的首选控件。

**核心职责：**

- **多行文本渲染**：支持多行文本显示,自动换行(字符级/单词级边界)
- **高级排版**：行高、字间距、文字对齐、最大行数限制、省略号裁剪
- **性能优化**：使用 HarfBuzz 进行文本整形,Skia 进行高性能渲染
- **依赖属性集成**：所有文本属性均为 DP,支持数据绑定、动画、样式继承

**继承关系：**

```
DependencyObject
    └── Visual
            └── UIElement
                    └── FrameworkElement
                            └── Control
                                    └── TextBlock  ← 本文档
```

**架构意义：**

1. **只读文本显示**：专注于文本展示,不支持编辑(编辑请使用 TextBox)
2. **布局友好**：精确的文本测量,与布局系统完美集成
3. **国际化支持**：通过 HarfBuzz 支持复杂文字整形(阿拉伯语、印地语等)
4. **MVVM 友好**：文本属性支持数据绑定,实时更新 UI

**典型用途：**

- **标签文本**：控件旁边的说明文字
- **段落内容**：多行文本段落展示
- **标题文本**：设置大字号、粗体的标题
- **ContentPresenter 内联文本**：作为 Button 等控件的文本内容

**设计原则：**

- **只读性**：不支持用户编辑,保持控件职责单一
- **高性能**：文本整形结果缓存,避免重复计算
- **精确测量**：基于字体度量的精确尺寸计算
- **完整性**：支持所有常见文本排版需求

**性能特性：**

- **文本整形缓存**：HarfBuzz 整形结果缓存,文本不变时复用
- **增量渲染**：仅绘制可见区域的文本
- **字体缓存**：字体对象在 FontManager 中全局缓存
- **脏区优化**：文本属性变更时标记脏区,避免全局重绘

---

## 2. 文件位置

**头文件：**
```
src/mine/ui/controls/TextBlock.h
```

**实现文件：**
```
src/mine/ui/controls/TextBlock.cpp
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
#include <mine/paint/Brush.h>
#include <mine/paint/Canvas.h>
#include <mine/core/InlineString.h>
#include <mine/text/TextLayout.h>
```

---

## 3. 类定义

### 3.1 完整类声明

```cpp
namespace mine::ui {

/// 多行只读文本展示控件
/// 
/// 提供丰富的文本排版功能:
/// - 自动换行 (字符级/单词级边界)
/// - 文字对齐 (左/中/右)
/// - 行高、字间距控制
/// - 省略号裁剪 (CharacterEllipsis)
/// - 最大行数限制
/// 
/// 仅用于文本显示,不支持编辑(编辑请使用 TextBox)
class MINE_API TextBlock : public Control {
    MINE_RTTI_DECLARE(TextBlock, Control)

public:
    /// Text 依赖属性 (InlineString 类型)
    static const DependencyProperty& TextProperty;

    /// FontSize 依赖属性 (float 类型)
    static const DependencyProperty& FontSizeProperty;

    /// Foreground 依赖属性 (Brush 类型)
    static const DependencyProperty& ForegroundProperty;

    /// Background 依赖属性 (Brush 类型)
    static const DependencyProperty& BackgroundProperty;

    /// Padding 依赖属性 (Thickness 类型)
    static const DependencyProperty& PaddingProperty;

    /// TextWrapping 依赖属性 (TextWrapping 类型)
    static const DependencyProperty& TextWrappingProperty;

    /// TextAlignment 依赖属性 (TextAlignment 类型)
    static const DependencyProperty& TextAlignmentProperty;

    /// TextTrimming 依赖属性 (TextTrimming 类型)
    static const DependencyProperty& TextTrimmingProperty;

    /// LineHeight 依赖属性 (float 类型)
    static const DependencyProperty& LineHeightProperty;

    /// CharacterSpacing 依赖属性 (float 类型)
    static const DependencyProperty& CharacterSpacingProperty;

    /// MaxLines 依赖属性 (int32_t 类型)
    static const DependencyProperty& MaxLinesProperty;

    /// 构造函数
    TextBlock();

    /// 析构函数
    ~TextBlock() override;

    // ==================== 文本内容 ====================

    [[nodiscard]] core::StringView text() const noexcept;
    void set_text(core::StringView text);

    // ==================== 字体属性 ====================

    [[nodiscard]] float font_size() const noexcept;
    void set_font_size(float size_px);

    // ==================== 颜色属性 ====================

    [[nodiscard]] paint::Brush foreground() const noexcept;
    void set_foreground(paint::Brush brush);

    [[nodiscard]] paint::Brush background() const noexcept;
    void set_background(paint::Brush brush);

    // ==================== 布局属性 ====================

    [[nodiscard]] math::Thickness padding() const noexcept;
    void set_padding(math::Thickness padding);

    // ==================== 排版属性 ====================

    [[nodiscard]] TextWrapping text_wrapping() const noexcept;
    void set_text_wrapping(TextWrapping mode);

    [[nodiscard]] TextAlignment text_alignment() const noexcept;
    void set_text_alignment(TextAlignment align);

    [[nodiscard]] bool use_ink_alignment() const noexcept;
    void set_use_ink_alignment(bool enabled);

    // ==================== 高级排版 ====================

    [[nodiscard]] TextTrimming text_trimming() const noexcept;
    void set_text_trimming(TextTrimming mode);

    [[nodiscard]] float line_height() const noexcept;
    void set_line_height(float height_px);

    [[nodiscard]] float character_spacing() const noexcept;
    void set_character_spacing(float spacing_px);

    [[nodiscard]] int32_t max_lines() const noexcept;
    void set_max_lines(int32_t lines);

protected:
    void on_measure(math::Size available_size) override;
    void on_arrange(math::Rect final_rect) override;
    void on_render(paint::Canvas& canvas) override;

private:
    text::TextLayout text_layout_;
    bool layout_dirty_ = true;
    
    void invalidate_text_layout();
    void update_text_layout(math::Size constraint);
};

/// 文本换行模式
enum class TextWrapping : uint8_t {
    NoWrap = 0,      // 不换行
    Wrap,            // 在字符边界换行
    WrapAtWord       // 在单词边界换行(英文友好)
};

/// 文本对齐方式
enum class TextAlignment : uint8_t {
    Left = 0,        // 左对齐
    Center,          // 居中对齐
    Right            // 右对齐
};

/// 文本裁剪模式
enum class TextTrimming : uint8_t {
    None = 0,                // 不裁剪
    CharacterEllipsis        // 字符级省略号 (...)
};

} // namespace mine::ui
```

### 3.2 依赖属性注册

```cpp
// TextProperty
const DependencyProperty& TextBlock::TextProperty =
    DependencyProperty::Register<TextBlock>(
        "Text",
        PropertyMetadata::create<core::InlineString>()
            .default_value(core::InlineString{})
            .flags(PropertyMetadata::AffectsMeasure | PropertyMetadata::AffectsRender)
    );

// FontSizeProperty (继承属性)
const DependencyProperty& TextBlock::FontSizeProperty =
    DependencyProperty::Register<TextBlock>(
        "FontSize",
        PropertyMetadata::create<float>()
            .default_value(14.0f)
            .flags(PropertyMetadata::Inherits | PropertyMetadata::AffectsMeasure)
    );

// ForegroundProperty (继承属性)
const DependencyProperty& TextBlock::ForegroundProperty =
    DependencyProperty::Register<TextBlock>(
        "Foreground",
        PropertyMetadata::create<paint::Brush>()
            .default_value(paint::Brush::from_color(Colors::Black))
            .flags(PropertyMetadata::Inherits | PropertyMetadata::AffectsRender)
    );

// ... 其他属性类似 ...
```

---

## 4. 成员方法

### 4.1 静态依赖属性

#### TextProperty

```cpp
static const DependencyProperty& TextProperty;
```

**描述：** 文本内容依赖属性。

**类型：** `core::InlineString`

**默认值：** 空字符串

**影响：** `AffectsMeasure | AffectsRender`

**示例：**
```cpp
text_block->set_text("Hello, World!");

// 数据绑定
text_block->bind_property(
    TextBlock::TextProperty,
    view_model,
    "MessageText"
);
```

---

#### FontSizeProperty

```cpp
static const DependencyProperty& FontSizeProperty;
```

**描述：** 字体大小依赖属性(像素单位)。

**类型：** `float`

**默认值：** 14.0f

**继承性：** 是(从父元素继承)

**影响：** `Inherits | AffectsMeasure`

---

#### ForegroundProperty

```cpp
static const DependencyProperty& ForegroundProperty;
```

**描述：** 前景色依赖属性。

**类型：** `paint::Brush`

**默认值：** 黑色

**继承性：** 是

**影响：** `Inherits | AffectsRender`

---

### 4.2 文本内容

#### text()

```cpp
[[nodiscard]] core::StringView text() const noexcept;
```

**描述：** 获取文本内容。

**返回值：** 字符串视图

---

#### set_text()

```cpp
void set_text(core::StringView text);
```

**描述：** 设置文本内容。

**参数：**
- `text`：要显示的文本

**行为：**
- 转换为 `InlineString` 存储
- 触发 `InvalidateMeasure()` 和 `InvalidateVisual()`
- 标记 `layout_dirty_ = true`,下次测量时重新整形

---

### 4.3 字体属性

#### font_size()

```cpp
[[nodiscard]] float font_size() const noexcept;
```

**描述：** 获取字体大小(像素)。

---

#### set_font_size()

```cpp
void set_font_size(float size_px);
```

**描述：** 设置字体大小。

**参数：**
- `size_px`：字体大小(像素),必须 > 0

---

### 4.4 颜色属性

#### foreground()

```cpp
[[nodiscard]] paint::Brush foreground() const noexcept;
```

**描述：** 获取前景色。

---

#### set_foreground()

```cpp
void set_foreground(paint::Brush brush);
```

**描述：** 设置前景色。

**示例：**
```cpp
// 纯色
text_block->set_foreground(Brush::from_color(Colors::Red));

// 渐变色
auto gradient = LinearGradientBrush::create(
    Point{0, 0}, Point{100, 0},
    {Colors::Blue, Colors::Purple}
);
text_block->set_foreground(gradient);
```

---

#### background()

```cpp
[[nodiscard]] paint::Brush background() const noexcept;
```

**描述：** 获取背景色。

---

#### set_background()

```cpp
void set_background(paint::Brush brush);
```

**描述：** 设置背景色。

**示例：**
```cpp
// 高亮背景
text_block->set_background(Brush::from_color(Color{0xFF, 0xFF, 0x00}));  // 黄色
```

---

### 4.5 布局属性

#### padding()

```cpp
[[nodiscard]] math::Thickness padding() const noexcept;
```

**描述：** 获取内边距。

---

#### set_padding()

```cpp
void set_padding(math::Thickness padding);
```

**描述：** 设置内边距。

**示例：**
```cpp
text_block->set_padding(Thickness{8, 4, 8, 4});
```

---

### 4.6 排版属性

#### text_wrapping()

```cpp
[[nodiscard]] TextWrapping text_wrapping() const noexcept;
```

**描述：** 获取换行模式。

---

#### set_text_wrapping()

```cpp
void set_text_wrapping(TextWrapping mode);
```

**描述：** 设置换行模式。

**参数：**
- `mode`：换行模式枚举值
  - `NoWrap`：不换行,文本超出边界时裁剪
  - `Wrap`：在任意字符边界换行
  - `WrapAtWord`：在单词边界换行(对英文友好)

**示例：**
```cpp
// 单词边界换行
text_block->set_text_wrapping(TextWrapping::WrapAtWord);
```

---

#### text_alignment()

```cpp
[[nodiscard]] TextAlignment text_alignment() const noexcept;
```

**描述：** 获取文本对齐方式。

---

#### set_text_alignment()

```cpp
void set_text_alignment(TextAlignment align);
```

**描述：** 设置文本对齐方式。

**参数：**
- `align`：对齐方式枚举值
  - `Left`：左对齐
  - `Center`：居中对齐
  - `Right`：右对齐

---

#### use_ink_alignment()

```cpp
[[nodiscard]] bool use_ink_alignment() const noexcept;
```

**描述：** 获取是否使用墨水对齐。

---

#### set_use_ink_alignment()

```cpp
void set_use_ink_alignment(bool enabled);
```

**描述：** 设置是否使用墨水对齐。

**说明：**
- `true`：基于字形实际边界对齐,消除视觉偏移
- `false`：基于字体度量边界对齐,保持基线一致

---

### 4.7 高级排版

#### text_trimming()

```cpp
[[nodiscard]] TextTrimming text_trimming() const noexcept;
```

**描述：** 获取文本裁剪模式。

---

#### set_text_trimming()

```cpp
void set_text_trimming(TextTrimming mode);
```

**描述：** 设置文本裁剪模式。

**参数：**
- `mode`：裁剪模式
  - `None`：不裁剪
  - `CharacterEllipsis`：字符级省略号 (...)

**示例：**
```cpp
// 单行文本,超出显示省略号
text_block->set_text_wrapping(TextWrapping::NoWrap);
text_block->set_text_trimming(TextTrimming::CharacterEllipsis);
```

---

#### line_height()

```cpp
[[nodiscard]] float line_height() const noexcept;
```

**描述：** 获取行高(像素)。

---

#### set_line_height()

```cpp
void set_line_height(float height_px);
```

**描述：** 设置行高。

**参数：**
- `height_px`：行高(像素),0 表示使用默认行高

**示例：**
```cpp
// 增加行间距
text_block->set_line_height(24.0f);  // 字号 14,行高 24
```

---

#### character_spacing()

```cpp
[[nodiscard]] float character_spacing() const noexcept;
```

**描述：** 获取字间距(像素)。

---

#### set_character_spacing()

```cpp
void set_character_spacing(float spacing_px);
```

**描述：** 设置字间距。

**参数：**
- `spacing_px`：字间距(像素),正值增加间距,负值减少间距

**示例：**
```cpp
// 标题文字,增加字间距
text_block->set_character_spacing(2.0f);
```

---

#### max_lines()

```cpp
[[nodiscard]] int32_t max_lines() const noexcept;
```

**描述：** 获取最大行数限制。

---

#### set_max_lines()

```cpp
void set_max_lines(int32_t lines);
```

**描述：** 设置最大行数限制。

**参数：**
- `lines`：最大行数,0 表示不限制

**示例：**
```cpp
// 限制为3行,超出部分裁剪
text_block->set_max_lines(3);
text_block->set_text_trimming(TextTrimming::CharacterEllipsis);
```

---

### 4.8 布局虚方法

#### on_measure()

```cpp
void on_measure(math::Size available_size) override;
```

**描述：** 测量文本所需尺寸。

**测量逻辑：**
1. 从可用空间减去 Padding
2. 调用 `text_layout_.Measure(constraint)` 进行文本整形
3. 获取文本实际尺寸,加上 Padding 作为期望尺寸

---

#### on_arrange()

```cpp
void on_arrange(math::Rect final_rect) override;
```

**描述：** 排列文本到最终矩形。

**排列逻辑：**
- 根据 `TextAlignment` 调整文本绘制起点

---

#### on_render()

```cpp
void on_render(paint::Canvas& canvas) override;
```

**描述：** 渲染文本和背景。

**渲染顺序：**
1. 绘制背景(如果设置)
2. 绘制文本(使用 `text_layout_.Draw(canvas)`)

---

## 5. 使用场景

### 5.1 基础文本显示

```cpp
auto text = core::make_owned<TextBlock>();
text->set_text("Hello, World!");
text->set_font_size(16.0f);
text->set_foreground(Brush::from_color(Colors::Black));
```

### 5.2 多行文本段落

```cpp
auto paragraph = core::make_owned<TextBlock>();
paragraph->set_text("这是一个很长的段落文本...");
paragraph->set_text_wrapping(TextWrapping::WrapAtWord);
paragraph->set_width(300.0f);
paragraph->set_line_height(24.0f);
```

### 5.3 标题文本

```cpp
auto title = core::make_owned<TextBlock>();
title->set_text("文章标题");
title->set_font_size(24.0f);
title->set_font_weight(FontWeight::Bold);
title->set_character_spacing(2.0f);
```

### 5.4 省略号裁剪

```cpp
auto label = core::make_owned<TextBlock>();
label->set_text("很长的文件名.txt");
label->set_width(150.0f);
label->set_text_wrapping(TextWrapping::NoWrap);
label->set_text_trimming(TextTrimming::CharacterEllipsis);
// 显示: "很长的文件..."
```

### 5.5 数据绑定

```cpp
// ViewModel
class AppViewModel : public mvvm::ViewModelBase {
public:
    MINE_PROPERTY(core::InlineString, status_text, "就绪")
};

// View
auto text = core::make_owned<TextBlock>();
text->bind_property(
    TextBlock::TextProperty,
    view_model,
    "status_text"
);
// ViewModel 更新时,文本自动同步
```

---

## 6. 最佳实践

1. **设置固定宽度**：多行文本应设置宽度,否则可能无限宽
2. **选择合适换行模式**：英文用 `WrapAtWord`,中文用 `Wrap`
3. **使用数据绑定**：MVVM 场景下绑定到 ViewModel
4. **避免频繁修改文本**：文本整形有开销,批量更新

---

## 7. 常见陷阱

### 7.1 忘记设置宽度导致单行显示

**问题：**
```cpp
auto text = core::make_owned<TextBlock>();
text->set_text("很长的文本...");
text->set_text_wrapping(TextWrapping::Wrap);
// 没有设置宽度,文本仍然单行显示!
```

**解决：**
```cpp
text->set_width(300.0f);  // 必须设置宽度
```

### 7.2 中文文本使用 WrapAtWord 导致不换行

**问题：**
```cpp
text->set_text_wrapping(TextWrapping::WrapAtWord);  // 中文没有空格分隔!
```

**解决：**
```cpp
text->set_text_wrapping(TextWrapping::Wrap);  // 字符级换行
```

---

## 8. 完整示例

### 8.1 文章阅读器

```cpp
auto create_article_view(core::StringView title, core::StringView content) 
    -> core::OwnedPtr<StackPanel> {
    auto panel = core::make_owned<StackPanel>();
    panel->set_padding(Thickness{24});
    panel->set_spacing(16.0f);
    
    // 标题
    auto title_text = core::make_owned<TextBlock>();
    title_text->set_text(title);
    title_text->set_font_size(28.0f);
    title_text->set_font_weight(FontWeight::Bold);
    title_text->set_character_spacing(1.5f);
    panel->add_child(title_text.release());
    
    // 分隔线
    auto divider = core::make_owned<Border>();
    divider->set_height(1.0f);
    divider->set_background(Brush::from_color(Color{0xE0, 0xE0, 0xE0}));
    divider->set_margin(Thickness{0, 8, 0, 8});
    panel->add_child(divider.release());
    
    // 正文
    auto content_text = core::make_owned<TextBlock>();
    content_text->set_text(content);
    content_text->set_font_size(16.0f);
    content_text->set_line_height(28.0f);
    content_text->set_text_wrapping(TextWrapping::WrapAtWord);
    content_text->set_foreground(Brush::from_color(Color{0x33, 0x33, 0x33}));
    panel->add_child(content_text.release());
    
    return panel;
}
```

---

## 9. 总结

- **TextBlock** 是只读多行文本展示控件
- 支持自动换行、对齐、行高、字间距等高级排版
- 所有文本属性均为依赖属性,支持数据绑定
- 使用 HarfBuzz + Skia 实现高性能文本渲染
- 适用于标签、段落、标题等所有文本显示场景

---

**文档版本:** 1.0  
**最后更新:** 2025-01-10  
**作者:** MineUI 开发团队  
**许可证:** MIT License
