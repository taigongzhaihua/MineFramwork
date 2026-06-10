# Canvas 详细接口文档

## 概述

`Canvas` 是 `mine.paint` 模块的**主要绘制接口**，以**录制模式**运行。

**核心特性：**
- **录制模式**：所有绘制调用被记录为 DrawCmd，调用 end() 后汇聚成 DisplayList
- **状态管理**：save/restore（变换 + 裁剪）
- **变换操作**：平移、缩放、旋转、任意变换
- **裁剪操作**：矩形、圆角矩形、多边形、路径
- **填充命令**：fill_rect、fill_rounded_rect、fill_ellipse、fill_path、fill_polygon 等
- **描边命令**：stroke_rect、stroke_rounded_rect、stroke_ellipse、stroke_line、stroke_arc、stroke_path、stroke_polygon 等
- **文字渲染**：draw_text（UTF-8，支持字符间距）

---

## 文件位置

```
src/mine/paint/api/include/mine/paint/Canvas.h
```

---

## Canvas 类

### 类定义

```cpp
class Canvas {
public:
    Canvas()  = default;
    ~Canvas() = default;
    
    // 不可拷贝，允许移动
    Canvas(const Canvas&)            = delete;
    Canvas& operator=(const Canvas&) = delete;
    Canvas(Canvas&&)                 = default;
    Canvas& operator=(Canvas&&)      = default;
    
    // 状态管理
    void save();
    void restore();
    void transform(const math::Transform2D& t);
    void translate(math::Vec2 offset);
    void scale(float factor);
    void scale(math::Vec2 factor);
    void rotate(float angle_rad);
    
    // 裁剪操作
    void clip_rect(math::Rect rect);
    void clip_rounded_rect(math::RoundedRect rrect);
    void clip_complex_rounded_rect(math::ComplexRoundedRect rrect);
    void clip_polygon(core::Span<const math::Vec2> vertices);
    void clip_path(const Path& path);
    void clip_pop();
    
    // 填充命令
    void fill_rect(math::Rect rect, const Brush& brush);
    void fill_rounded_rect(math::RoundedRect rrect, const Brush& brush);
    void fill_complex_rounded_rect(math::ComplexRoundedRect rrect, const Brush& brush);
    void fill_ellipse(math::Vec2 center, math::Vec2 radii, const Brush& brush);
    void fill_circle(math::Vec2 center, float radius, const Brush& brush);
    void fill_path(const Path& path, const Brush& brush);
    void fill_polygon(core::Span<const math::Vec2> vertices, const Brush& brush);
    
    // 描边命令
    void stroke_rect(math::Rect rect, const Brush& brush, const Pen& pen = {});
    void stroke_rounded_rect(math::RoundedRect rrect, const Brush& brush, const Pen& pen = {});
    void stroke_complex_rounded_rect(math::ComplexRoundedRect rrect, const Brush& brush, const Pen& pen = {});
    void stroke_bordered_rect(math::Rect rect, const Brush& brush, math::Thickness widths);
    void stroke_bordered_rounded_rect(math::Rect rect, const Brush& brush, math::Thickness widths, math::CornerRadii radii);
    void stroke_ellipse(math::Vec2 center, math::Vec2 radii, const Brush& brush, const Pen& pen = {});
    void stroke_circle(math::Vec2 center, float radius, const Brush& brush, const Pen& pen = {});
    void stroke_line(math::Vec2 from, math::Vec2 to, const Brush& brush, const Pen& pen = {});
    void stroke_arc(math::Vec2 center, float radius, float start_angle, float sweep_angle, const Brush& brush, const Pen& pen = {});
    void stroke_quad_bezier(math::Vec2 p0, math::Vec2 p1, math::Vec2 p2, const Brush& brush, const Pen& pen = {});
    void stroke_cubic_bezier(math::Vec2 p0, math::Vec2 p1, math::Vec2 p2, math::Vec2 p3, const Brush& brush, const Pen& pen = {});
    void stroke_path(const Path& path, const Brush& brush, const Pen& pen = {});
    void stroke_polygon(core::Span<const math::Vec2> vertices, const Brush& brush, const Pen& pen = {});
    
    // 文字渲染
    void draw_text(core::StringView text, math::Vec2 origin, void* font_face, float size_px, const Brush& brush, float character_spacing = 0.0f);
    
    // 完成录制
    [[nodiscard]] DisplayList end();
    [[nodiscard]] size_t cmd_count() const noexcept;
    [[nodiscard]] bool empty() const noexcept;
    void discard();
};
```

**描述**：绘制上下文（录制模式）。

**特征**：
- 所有 fill_*/stroke_* 方法均将命令录制到内部缓冲
- 调用 end() 后生成 DisplayList 并重置自身状态
- 非线程安全，单线程使用
- save()/restore() 仅保存/恢复变换与裁剪状态

---

## 成员方法

### 状态管理

#### Canvas::save()

```cpp
void save();
```

**描述**：保存当前绘制状态（变换 + 裁剪）到栈中。

**特征**：
- 与 restore() 成对使用
- 可嵌套调用

---

#### Canvas::restore()

```cpp
void restore();
```

**描述**：恢复最近一次 save() 保存的状态。

**特征**：
- 若 save/restore 不匹配（restore 比 save 多），则无操作

---

#### Canvas::transform()

```cpp
void transform(const math::Transform2D& t);
```

**描述**：将当前变换与给定变换级联（在当前变换之后应用）。

**参数**：
- `t`：要附加的变换矩阵

---

#### Canvas::translate()

```cpp
void translate(math::Vec2 offset);
```

**描述**：平移。

**参数**：
- `offset`：偏移量

---

#### Canvas::scale()

```cpp
void scale(float factor);
void scale(math::Vec2 factor);
```

**描述**：缩放。

**参数**：
- `factor`：等比缩放因子，或 X/Y 独立缩放因子

---

#### Canvas::rotate()

```cpp
void rotate(float angle_rad);
```

**描述**：旋转（弧度，顺时针）。

**参数**：
- `angle_rad`：旋转角度（弧度）

---

### 裁剪操作

#### Canvas::clip_rect()

```cpp
void clip_rect(math::Rect rect);
```

**描述**：以矩形区域裁剪后续绘制内容。

**参数**：
- `rect`：裁剪矩形

**特征**：
- 与当前裁剪区域取交集
- 可多次嵌套调用
- 通过 save()/restore() 撤销

---

#### Canvas::clip_rounded_rect()

```cpp
void clip_rounded_rect(math::RoundedRect rrect);
```

**描述**：以均匀圆角矩形裁剪后续绘制内容。

**参数**：
- `rrect`：圆角矩形（rect + radius_x + radius_y）

**特征**：
- 支持 X/Y 独立的均匀圆角半径，四角相同
- 可与 clip_rect / clip_polygon 嵌套使用
- 通过 save()/restore() 撤销

---

#### Canvas::clip_complex_rounded_rect()

```cpp
void clip_complex_rounded_rect(math::ComplexRoundedRect rrect);
```

**描述**：以四角各自独立椭圆圆角矩形裁剪后续绘制内容。

**参数**：
- `rrect`：复杂圆角矩形（rect + 四组独立 corner radii）

**特征**：
- 每个角可独立设置 X/Y 圆角半径
- 可与其他 clip_* 嵌套使用
- 通过 save()/restore() 撤销

---

#### Canvas::clip_polygon()

```cpp
void clip_polygon(core::Span<const math::Vec2> vertices);
```

**描述**：以多边形裁剪后续绘制内容。

**参数**：
- `vertices`：多边形顶点列表（按顺序，不必须闭合）

**特征**：
- 至少需要 3 个顶点
- 内部转换为 SDF 多边形并以模板缓冲实现精确像素级裁剪
- 可与其他 clip_* 嵌套使用
- 通过 save()/restore() 撤销

---

#### Canvas::clip_path()

```cpp
void clip_path(const Path& path);
```

**描述**：以任意路径形状裁剪后续绘制内容。

**参数**：
- `path`：待裁剪路径（必须含有至少 3 个顶点的子路径）

**特征**：
- 路径中的曲线段（QuadTo / CubicTo）由渲染器内部自动扁平化为折线多边形
- 优先取第一个闭合子路径；若无闭合子路径，则取顶点最多的子路径
- 可与其他 clip_* 嵌套使用
- 通过 save()/restore() 撤销

---

#### Canvas::clip_pop()

```cpp
void clip_pop();
```

**描述**：弹出最近一次压入的裁剪区域。

**特征**：
- 与 clip_rect / clip_rounded_rect / clip_complex_rounded_rect / clip_polygon / clip_path 配套调用
- 也可通过 save()/restore() 自动撤销

---

### 填充命令

#### Canvas::fill_rect()

```cpp
void fill_rect(math::Rect rect, const Brush& brush);
```

**描述**：填充矩形。

**参数**：
- `rect`：矩形
- `brush`：画刷

---

#### Canvas::fill_rounded_rect()

```cpp
void fill_rounded_rect(math::RoundedRect rrect, const Brush& brush);
```

**描述**：填充圆角矩形。

**参数**：
- `rrect`：圆角矩形（rect + radius_x + radius_y）
- `brush`：画刷

---

#### Canvas::fill_complex_rounded_rect()

```cpp
void fill_complex_rounded_rect(math::ComplexRoundedRect rrect, const Brush& brush);
```

**描述**：填充四角各自独立椭圆半径的圆角矩形。

**参数**：
- `rrect`：复杂圆角矩形（rect + 四组独立 corner radii）
- `brush`：画刷

---

#### Canvas::fill_ellipse()

```cpp
void fill_ellipse(math::Vec2 center, math::Vec2 radii, const Brush& brush);
```

**描述**：填充椭圆。

**参数**：
- `center`：椭圆中心点
- `radii`：X 轴半径（radii.x）和 Y 轴半径（radii.y）
- `brush`：画刷

---

#### Canvas::fill_circle()

```cpp
void fill_circle(math::Vec2 center, float radius, const Brush& brush);
```

**描述**：填充正圆。

**参数**：
- `center`：圆心
- `radius`：半径
- `brush`：画刷

**特征**：
- 等价于 `fill_ellipse(center, {radius, radius}, brush)`

---

#### Canvas::fill_path()

```cpp
void fill_path(const Path& path, const Brush& brush);
```

**描述**：填充任意路径。

**参数**：
- `path`：路径
- `brush`：画刷

---

#### Canvas::fill_polygon()

```cpp
void fill_polygon(core::Span<const math::Vec2> vertices, const Brush& brush);
```

**描述**：填充多边形（SDF，支持凸多边形和凹多边形）。

**参数**：
- `vertices`：多边形顶点序列（至少 3 个，最多 64 个）
- `brush`：画刷（当前仅支持 SolidColor）

**特征**：
- 使用 IQ 绕数法多边形 SDF 渲染，亚像素抗锯齿
- 顶点顺序可以是顺时针或逆时针，均正确处理
- 支持任意简单多边形（无自交），最多 64 个顶点

---

### 描边命令

#### Canvas::stroke_rect()

```cpp
void stroke_rect(math::Rect rect, const Brush& brush, const Pen& pen = {});
```

**描述**：描边矩形。

**参数**：
- `rect`：矩形
- `brush`：画刷
- `pen`：描边样式（默认 Pen{1.0f}）

---

#### Canvas::stroke_rounded_rect()

```cpp
void stroke_rounded_rect(math::RoundedRect rrect, const Brush& brush, const Pen& pen = {});
```

**描述**：描边圆角矩形。

**参数**：
- `rrect`：圆角矩形（rect + radius_x + radius_y）
- `brush`：画刷
- `pen`：描边样式（默认 Pen{1.0f}）

---

#### Canvas::stroke_complex_rounded_rect()

```cpp
void stroke_complex_rounded_rect(math::ComplexRoundedRect rrect, const Brush& brush, const Pen& pen = {});
```

**描述**：描边四角各自独立椭圆半径的圆角矩形。

**参数**：
- `rrect`：复杂圆角矩形（rect + 四组独立 corner radii）
- `brush`：画刷
- `pen`：描边样式（默认 Pen{1.0f}）

---

#### Canvas::stroke_bordered_rect()

```cpp
void stroke_bordered_rect(math::Rect rect, const Brush& brush, math::Thickness widths);
```

**描述**：四边各自独立宽度的矩形内侧描边（类 CSS border）。

**参数**：
- `rect`：矩形几何
- `brush`：填充画刷
- `widths`：四边宽度（Thickness{left, top, right, bottom}）

**特征**：
- 描边方向为内侧：每条边的描边宽度从矩形边界向内延伸，不超出矩形的外轮廓

---

#### Canvas::stroke_bordered_rounded_rect()

```cpp
void stroke_bordered_rounded_rect(math::Rect rect, const Brush& brush, math::Thickness widths, math::CornerRadii radii);
```

**描述**：四边各自独立宽度 + 四角各自独立圆角的矩形内侧描边。

**参数**：
- `rect`：矩形几何
- `brush`：画刷（当前仅支持 SolidColor）
- `widths`：四边描边宽度（内侧方向）
- `radii`：四角圆角半径（外轮廓）

**特征**：
- 结合 CSS border-width 和 border-radius 语义：外轮廓随圆角圆滑，各边描边带独立计算，角部直角相交并被外轮廓圆角自然剪裁

---

#### Canvas::stroke_ellipse()

```cpp
void stroke_ellipse(math::Vec2 center, math::Vec2 radii, const Brush& brush, const Pen& pen = {});
```

**描述**：描边椭圆。

**参数**：
- `center`：椭圆中心点
- `radii`：X 轴半径（radii.x）和 Y 轴半径（radii.y）
- `brush`：画刷
- `pen`：描边样式（默认 Pen{1.0f}）

---

#### Canvas::stroke_circle()

```cpp
void stroke_circle(math::Vec2 center, float radius, const Brush& brush, const Pen& pen = {});
```

**描述**：描边正圆。

**参数**：
- `center`：圆心
- `radius`：半径
- `brush`：画刷
- `pen`：描边样式（默认 Pen{1.0f}）

**特征**：
- 等价于 `stroke_ellipse(center, {radius, radius}, brush, pen)`

---

#### Canvas::stroke_line()

```cpp
void stroke_line(math::Vec2 from, math::Vec2 to, const Brush& brush, const Pen& pen = {});
```

**描述**：描边线段（from → to）。

**参数**：
- `from`：起点
- `to`：终点
- `brush`：画刷
- `pen`：描边样式（默认 Pen{1.0f}）

---

#### Canvas::stroke_arc()

```cpp
void stroke_arc(math::Vec2 center, float radius, float start_angle, float sweep_angle, const Brush& brush, const Pen& pen = {});
```

**描述**：描边圆弧。

**参数**：
- `center`：圆心（屏幕像素坐标）
- `radius`：圆弧半径（像素）
- `start_angle`：起始角（弧度；0=右，正值=顺时针）
- `sweep_angle`：扫掠角（弧度；正值=顺时针扫掠，负值=逆时针扫掠）
- `brush`：画刷（当前仅支持 SolidColor）
- `pen`：描边样式（width、start_cap、end_cap；支持 Flat/Round）

**特征**：
- 使用 SDF 渲染，天然亚像素抗锯齿
- 角度约定：0=右（+X），正值=顺时针（屏幕坐标，Y 向下）

---

#### Canvas::stroke_quad_bezier()

```cpp
void stroke_quad_bezier(math::Vec2 p0, math::Vec2 p1, math::Vec2 p2, const Brush& brush, const Pen& pen = {});
```

**描述**：描边二次贝塞尔曲线。

**参数**：
- `p0`：起点（对应曲线参数 t=0）
- `p1`：控制点（决定曲线弯曲方向和程度）
- `p2`：终点（对应曲线参数 t=1）
- `brush`：画刷（当前仅支持 SolidColor）
- `pen`：描边样式（width、start_cap、end_cap）

**特征**：
- 使用 SDF 渲染，闭合解析解（IQ 算法），天然亚像素抗锯齿
- 当 start_cap/end_cap = Round 时，端点自然延伸为圆形（IQ clamp 天然支持）
- 当 start_cap/end_cap = Flat 时，端点处以切线方向截断

---

#### Canvas::stroke_cubic_bezier()

```cpp
void stroke_cubic_bezier(math::Vec2 p0, math::Vec2 p1, math::Vec2 p2, math::Vec2 p3, const Brush& brush, const Pen& pen = {});
```

**描述**：描边三次贝塞尔曲线（P0 → P1 → P2 → P3，Flat/Round cap）。

**参数**：
- `p0`：起点（t=0）
- `p1`：第一控制点
- `p2`：第二控制点
- `p3`：终点（t=1）
- `brush`：画刷（当前仅支持 SolidColor）
- `pen`：描边样式（width、start_cap、end_cap）

---

#### Canvas::stroke_path()

```cpp
void stroke_path(const Path& path, const Brush& brush, const Pen& pen = {});
```

**描述**：描边任意路径。

**参数**：
- `path`：路径
- `brush`：画刷
- `pen`：描边样式（默认 Pen{1.0f}）

---

#### Canvas::stroke_polygon()

```cpp
void stroke_polygon(core::Span<const math::Vec2> vertices, const Brush& brush, const Pen& pen = {});
```

**描述**：描边多边形轮廓（SDF，支持凸多边形和凹多边形）。

**参数**：
- `vertices`：多边形顶点序列（至少 3 个，最多 64 个）
- `brush`：画刷（当前仅支持 SolidColor）
- `pen`：描边样式（width 决定线宽）

**特征**：
- 使用 IQ 绕数法多边形 SDF 渲染，亚像素抗锯齿
- 描边沿轮廓向内外各扩展 pen.width/2，端点角部自然连接
- 支持任意简单多边形（无自交），最多 64 个顶点

---

### 文字渲染

#### Canvas::draw_text()

```cpp
void draw_text(
    core::StringView text,
    math::Vec2       origin,
    void*            font_face,
    float            size_px,
    const Brush&     brush,
    float            character_spacing = 0.0f);
```

**描述**：绘制 UTF-8 文字。

**参数**：
- `text`：UTF-8 文本
- `origin`：文字基线起点
- `font_face`：字体（text::FontFace*，void* 为了避免头文件循环依赖）
- `size_px`：字号（像素）
- `brush`：文字颜色画刷（当前仅支持 SolidColor）
- `character_spacing`：字符间额外间距（像素；默认 0，即不加间距）

---

### 完成录制

#### Canvas::end()

```cpp
[[nodiscard]] DisplayList end();
```

**描述**：完成录制，返回不可变 DisplayList。

**返回**：不可变 DisplayList

**特征**：
- 调用后 Canvas 恢复为空状态（命令缓冲和路径缓冲均清空）
- 变换/裁剪栈也被清空（未匹配的 save() 被丢弃）

---

#### Canvas::cmd_count()

```cpp
[[nodiscard]] size_t cmd_count() const noexcept;
```

**描述**：当前录制的命令数量。

**返回**：命令数量

---

#### Canvas::empty()

```cpp
[[nodiscard]] bool empty() const noexcept;
```

**描述**：是否为空（无任何命令）。

**返回**：空返回 true

---

#### Canvas::discard()

```cpp
void discard();
```

**描述**：丢弃所有已录制的命令并重置状态（不生成 DisplayList）。

---

## 使用场景

### 1. 简单矩形

```cpp
Canvas canvas;
canvas.fill_rect({10, 10, 200, 100}, Brush::solid_rgb(0xFF0000));
DisplayList dl = canvas.end();
renderer->render(dl, target_texture);
```

---

### 2. 圆角矩形

```cpp
Canvas canvas;
canvas.fill_rounded_rect({{10, 10, 200, 100}, 15.f}, Brush::solid_rgb(0x0000FF));
DisplayList dl = canvas.end();
renderer->render(dl, target_texture);
```

---

### 3. 变换和裁剪

```cpp
Canvas canvas;

// 保存状态
canvas.save();

// 变换
canvas.translate({100, 100});
canvas.rotate(0.785f);  // 45 度

// 裁剪
canvas.clip_rect({0, 0, 200, 200});

// 绘制
canvas.fill_rect({0, 0, 100, 100}, Brush::solid_rgb(0xFF0000));

// 恢复状态
canvas.restore();

DisplayList dl = canvas.end();
renderer->render(dl, target_texture);
```

---

### 4. 路径填充

```cpp
Canvas canvas;

Path triangle = PathBuilder{}
    .move_to({100.f, 50.f})
    .line_to({150.f, 150.f})
    .line_to({50.f, 150.f})
    .close()
    .build();

canvas.fill_path(triangle, Brush::solid_rgb(0xFF0000));

DisplayList dl = canvas.end();
renderer->render(dl, target_texture);
```

---

### 5. 线性渐变

```cpp
Canvas canvas;

GradientStop stops[] = {
    {0.0f, {0.2f, 0.5f, 1.0f, 1.0f}},  // 蓝色
    {1.0f, {1.0f, 0.5f, 0.1f, 1.0f}}   // 橙色
};

auto brush = Brush::linear_gradient({0, 0.5f}, {1, 0.5f}, stops);
canvas.fill_rect({10, 10, 200, 100}, brush);

DisplayList dl = canvas.end();
renderer->render(dl, target_texture);
```

---

### 6. 描边和填充

```cpp
Canvas canvas;

// 填充
canvas.fill_rect({10, 10, 200, 100}, Brush::solid_rgb(0xFF0000));

// 描边
Pen pen{2.0f};
pen.line_join = LineJoin::Round;
canvas.stroke_rect({10, 10, 200, 100}, Brush::solid_rgb(0x0000FF), pen);

DisplayList dl = canvas.end();
renderer->render(dl, target_texture);
```

---

### 7. 文字渲染

```cpp
Canvas canvas;

text::FontFace font;
font.load_from_file("path/to/font.ttf");

canvas.draw_text(
    "Hello, World!",
    {10, 100},
    &font,
    24.0f,
    Brush::solid_rgb(0x000000),
    1.0f  // 字符间距
);

DisplayList dl = canvas.end();
renderer->render(dl, target_texture);
```

---

### 8. 多边形

```cpp
Canvas canvas;

Vec2 vertices[] = {
    {100, 50},
    {150, 150},
    {50, 150}
};

canvas.fill_polygon(vertices, Brush::solid_rgb(0x00FF00));

DisplayList dl = canvas.end();
renderer->render(dl, target_texture);
```

---

### 9. 贝塞尔曲线

```cpp
Canvas canvas;

Pen pen{3.0f};
pen.start_cap = LineCap::Round;
pen.end_cap = LineCap::Round;

canvas.stroke_quad_bezier(
    {10, 100},   // 起点
    {100, 10},   // 控制点
    {190, 100},  // 终点
    Brush::solid_rgb(0xFF00FF),
    pen
);

DisplayList dl = canvas.end();
renderer->render(dl, target_texture);
```

---

### 10. 亚克力效果

```cpp
Canvas canvas;

auto brush = Brush::acrylic({0.5f, 0.7f, 1.0f, 1.0f}, 0.6f, 30.0f);
canvas.fill_rect({10, 10, 200, 100}, brush);

DisplayList dl = canvas.end();
renderer->render(dl, target_texture);
```

---

## 性能分析

### 录制模式开销

**特征**：
- 所有绘制命令被记录到 Vector<DrawCmd>
- Vector 动态增长，可能触发扩容
- 每条命令约 150+ 字节（DrawCmd 结构体大小）

---

### 路径内联化

**特征**：
- fill_path/stroke_path 将 Path 保存到 Canvas 内部 Vector<Path>
- DrawCmd 通过 path_index 引用路径
- 相同路径重复使用会产生多份拷贝

---

### 变换和裁剪

**特征**：
- save/restore 使用栈记录状态
- clip_* 操作累积裁剪层深度

---

## 最佳实践

### 1. 复用 Canvas

```cpp
// ✅ 推荐：复用 Canvas
Canvas canvas;

// 第一帧
canvas.fill_rect({10, 10, 200, 100}, Brush::solid_rgb(0xFF0000));
DisplayList dl1 = canvas.end();
renderer->render(dl1, target_texture);

// 第二帧（Canvas 已恢复空状态）
canvas.fill_rect({20, 20, 200, 100}, Brush::solid_rgb(0x0000FF));
DisplayList dl2 = canvas.end();
renderer->render(dl2, target_texture);

// ❌ 不推荐：每帧创建新 Canvas
Canvas canvas1;
canvas1.fill_rect({10, 10, 200, 100}, Brush::solid_rgb(0xFF0000));
DisplayList dl1 = canvas1.end();

Canvas canvas2;
canvas2.fill_rect({20, 20, 200, 100}, Brush::solid_rgb(0x0000FF));
DisplayList dl2 = canvas2.end();
```

---

### 2. 使用 save/restore

```cpp
// ✅ 推荐：使用 save/restore
canvas.save();
canvas.translate({100, 100});
canvas.fill_rect({0, 0, 50, 50}, Brush::solid_rgb(0xFF0000));
canvas.restore();

// ❌ 不推荐：手动反向变换
canvas.translate({100, 100});
canvas.fill_rect({0, 0, 50, 50}, Brush::solid_rgb(0xFF0000));
canvas.translate({-100, -100});  // 容易出错
```

---

### 3. 优先使用便捷几何方法

```cpp
// ✅ 推荐：使用 fill_rect
canvas.fill_rect({10, 10, 200, 100}, brush);

// ❌ 不推荐：手动构建路径
Path rect = PathBuilder{}
    .move_to({10, 10})
    .line_to({210, 10})
    .line_to({210, 110})
    .line_to({10, 110})
    .close()
    .build();
canvas.fill_path(rect, brush);
```

---

### 4. 合理使用裁剪

```cpp
// ✅ 推荐：使用 clip_pop
canvas.clip_rect({10, 10, 200, 100});
canvas.fill_rect({0, 0, 300, 200}, brush);
canvas.clip_pop();

// ❌ 不推荐：忘记 clip_pop
canvas.clip_rect({10, 10, 200, 100});
canvas.fill_rect({0, 0, 300, 200}, brush);
// 裁剪区域未弹出，后续绘制受影响
```

---

### 5. 避免过多 save/restore

```cpp
// ✅ 推荐：批量绘制同类元素
canvas.save();
canvas.translate({100, 100});
for (int i = 0; i < 10; ++i) {
    canvas.fill_rect({i * 10.f, i * 10.f, 50, 50}, brush);
}
canvas.restore();

// ❌ 不推荐：每个元素都 save/restore
for (int i = 0; i < 10; ++i) {
    canvas.save();
    canvas.translate({100 + i * 10.f, 100 + i * 10.f});
    canvas.fill_rect({0, 0, 50, 50}, brush);
    canvas.restore();
}
```

---

## 常见陷阱

### 1. 忘记调用 end()

```cpp
// ❌ 问题：忘记调用 end()
Canvas canvas;
canvas.fill_rect({10, 10, 200, 100}, brush);
// 缺少 DisplayList dl = canvas.end();

// ✅ 解决：显式调用 end()
Canvas canvas;
canvas.fill_rect({10, 10, 200, 100}, brush);
DisplayList dl = canvas.end();
renderer->render(dl, target_texture);
```

---

### 2. save/restore 不匹配

```cpp
// ❌ 问题：save/restore 不匹配
canvas.save();
canvas.translate({100, 100});
canvas.fill_rect({0, 0, 50, 50}, brush);
// 缺少 canvas.restore();

// ✅ 解决：确保 save/restore 匹配
canvas.save();
canvas.translate({100, 100});
canvas.fill_rect({0, 0, 50, 50}, brush);
canvas.restore();
```

---

### 3. clip_* 和 clip_pop 不匹配

```cpp
// ❌ 问题：clip_* 和 clip_pop 不匹配
canvas.clip_rect({10, 10, 200, 100});
canvas.fill_rect({0, 0, 300, 200}, brush);
// 缺少 canvas.clip_pop();

// ✅ 解决：确保 clip_* 和 clip_pop 匹配
canvas.clip_rect({10, 10, 200, 100});
canvas.fill_rect({0, 0, 300, 200}, brush);
canvas.clip_pop();
```

---

### 4. 多次调用 end()

```cpp
// ❌ 问题：多次调用 end()
Canvas canvas;
canvas.fill_rect({10, 10, 200, 100}, brush);
DisplayList dl1 = canvas.end();
DisplayList dl2 = canvas.end();  // 错误：Canvas 已空

// ✅ 解决：重新绘制
Canvas canvas;
canvas.fill_rect({10, 10, 200, 100}, brush);
DisplayList dl1 = canvas.end();

// 重新绘制
canvas.fill_rect({20, 20, 200, 100}, brush);
DisplayList dl2 = canvas.end();
```

---

## 完整示例

```cpp
#include <mine/paint/Canvas.h>
#include <mine/paint/Brush.h>
#include <mine/paint/Pen.h>
#include <mine/paint/PathBuilder.h>
#include <mine/text/FontFace.h>

using namespace mine::paint;
using namespace mine::math;
using namespace mine::text;

void draw_scene(Canvas& canvas, IRenderer* renderer, ITexture* target) {
    // 1. 纯色矩形
    canvas.fill_rect({10, 10, 200, 100}, Brush::solid_rgb(0xFF0000));
    
    // 2. 圆角矩形
    canvas.fill_rounded_rect({{220, 10, 200, 100}, 15.f}, Brush::solid_rgb(0x0000FF));
    
    // 3. 线性渐变
    GradientStop stops[] = {
        {0.0f, {0.2f, 0.5f, 1.0f, 1.0f}},
        {1.0f, {1.0f, 0.5f, 0.1f, 1.0f}}
    };
    auto linear_brush = Brush::linear_gradient({0, 0.5f}, {1, 0.5f}, stops);
    canvas.fill_rect({430, 10, 200, 100}, linear_brush);
    
    // 4. 描边矩形
    Pen pen{2.0f};
    pen.line_join = LineJoin::Round;
    canvas.stroke_rect({10, 120, 200, 100}, Brush::solid_rgb(0x00FF00), pen);
    
    // 5. 变换和裁剪
    canvas.save();
    canvas.translate({320, 170});
    canvas.rotate(0.785f);  // 45 度
    canvas.clip_rect({-50, -50, 100, 100});
    canvas.fill_rect({-50, -50, 100, 100}, Brush::solid_rgb(0xFFFF00));
    canvas.restore();
    
    // 6. 路径
    Path triangle = PathBuilder{}
        .move_to({480, 120})
        .line_to({530, 220})
        .line_to({430, 220})
        .close()
        .build();
    canvas.fill_path(triangle, Brush::solid_rgb(0xFF00FF));
    
    // 7. 贝塞尔曲线
    Pen thick_pen{3.0f};
    thick_pen.start_cap = LineCap::Round;
    thick_pen.end_cap = LineCap::Round;
    canvas.stroke_quad_bezier(
        {10, 240},
        {100, 190},
        {190, 240},
        Brush::solid_rgb(0x00FFFF),
        thick_pen
    );
    
    // 8. 多边形
    Vec2 vertices[] = {
        {250, 240},
        {300, 190},
        {350, 240}
    };
    canvas.fill_polygon(vertices, Brush::solid_rgb(0xFFA500));
    
    // 9. 文字
    FontFace font;
    font.load_from_file("path/to/font.ttf");
    canvas.draw_text(
        "Hello, MineUI!",
        {400, 260},
        &font,
        24.0f,
        Brush::solid_rgb(0x000000),
        1.0f
    );
    
    // 10. 完成录制并渲染
    DisplayList dl = canvas.end();
    renderer->render(dl, target);
}
```

---

## 总结

`Canvas` 是 `mine.paint` 模块的主要绘制接口，具备：

1. **录制模式**：所有绘制调用被记录为 DrawCmd，调用 end() 后汇聚成 DisplayList
2. **状态管理**：save/restore（变换 + 裁剪）
3. **变换操作**：平移、缩放、旋转、任意变换
4. **裁剪操作**：矩形、圆角矩形、多边形、路径
5. **填充命令**：fill_rect、fill_rounded_rect、fill_ellipse、fill_path、fill_polygon 等
6. **描边命令**：stroke_rect、stroke_rounded_rect、stroke_ellipse、stroke_line、stroke_arc、stroke_path、stroke_polygon 等
7. **文字渲染**：draw_text（UTF-8，支持字符间距）

**使用建议**：
- 复用 Canvas
- 使用 save/restore
- 优先使用便捷几何方法
- 合理使用裁剪
- 避免过多 save/restore
- 确保 save/restore 和 clip_*/clip_pop 匹配
- 显式调用 end()
