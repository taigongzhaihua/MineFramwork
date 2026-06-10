# Path 与 PathBuilder 详细接口文档

## 概述

`Path` 和 `PathBuilder` 是 `mine.paint` 模块的**几何路径系统**，用于构建和表示任意几何形状。

**核心特性：**
- **Path**：不可变几何路径（MoveTo/LineTo/CubicTo/QuadTo/Close）
- **PathBuilder**：命令式路径构造器（流式 API）
- **便捷几何**：矩形、圆角矩形、椭圆等一键添加
- **值类型**：可拷贝、可移动

---

## 文件位置

```
src/mine/paint/api/include/mine/paint/Path.h
src/mine/paint/api/include/mine/paint/PathBuilder.h
```

---

## Path（不可变路径）

### 枚举类型

#### PathCmdKind

```cpp
enum class PathCmdKind : uint8_t {
    MoveTo,   ///< 移动到新起点（不绘制），使用 pt[0]
    LineTo,   ///< 直线连接到目标点，使用 pt[0]
    CubicTo,  ///< 三次贝塞尔曲线：控制点1=pt[0]，控制点2=pt[1]，终点=pt[2]
    QuadTo,   ///< 二次贝塞尔曲线：控制点=pt[0]，终点=pt[1]
    Close,    ///< 关闭当前子路径（连接回最近的 MoveTo 点）
};
```

**描述**：路径命令类型。

**枚举值**：
- `MoveTo`：移动到新起点（不绘制），使用 pt[0]
- `LineTo`：直线连接到目标点，使用 pt[0]
- `CubicTo`：三次贝塞尔曲线，控制点1=pt[0]，控制点2=pt[1]，终点=pt[2]
- `QuadTo`：二次贝塞尔曲线，控制点=pt[0]，终点=pt[1]
- `Close`：关闭当前子路径（连接回最近的 MoveTo 点）

---

### PathCmd 结构体

```cpp
struct PathCmd {
    PathCmdKind kind{PathCmdKind::MoveTo};
    math::Vec2  pt[3]{};  ///< 端点/控制点（按 kind 使用不同数量）
};
```

**描述**：单条路径命令。

**特征**：
- 最多携带 3 个二维点（CubicTo 使用全部 3 个，其余命令只用到前面若干个）
- Trivially copyable，可直接存入 Vector / 传递给 GPU

**字段**：
- `kind`：路径命令类型
- `pt`：端点/控制点（按 kind 使用不同数量）

---

### Path 类

```cpp
class Path {
public:
    Path() = default;
    explicit Path(containers::Vector<PathCmd> cmds) noexcept;
    
    // 访问器
    [[nodiscard]] core::Span<const PathCmd> cmds() const noexcept;
    [[nodiscard]] size_t cmd_count() const noexcept;
    [[nodiscard]] bool empty() const noexcept;
};
```

**描述**：不可变几何路径。

**特征**：
- 一旦由 `PathBuilder::build()` 创建完成，路径内容不可修改
- 可以拷贝（深拷贝 PathCmd 数组）
- 可以移动（零分配转移）
- DisplayList 通过索引引用路径，需要 DisplayList 持有 Path 的所有权

---

## PathBuilder（路径构造器）

### 类定义

```cpp
class PathBuilder {
public:
    PathBuilder()  = default;
    ~PathBuilder() = default;
    
    // 禁止拷贝，允许移动
    PathBuilder(const PathBuilder&)            = delete;
    PathBuilder& operator=(const PathBuilder&) = delete;
    PathBuilder(PathBuilder&&)                 = default;
    PathBuilder& operator=(PathBuilder&&)      = default;
    
    // 基础路径命令
    PathBuilder& move_to(math::Vec2 pt);
    PathBuilder& line_to(math::Vec2 pt);
    PathBuilder& cubic_to(math::Vec2 c1, math::Vec2 c2, math::Vec2 end);
    PathBuilder& quad_to(math::Vec2 ctrl, math::Vec2 end);
    PathBuilder& close();
    
    // 便捷几何命令
    PathBuilder& add_rect(math::Rect rect);
    PathBuilder& add_rounded_rect(math::RoundedRect rrect);
    PathBuilder& add_complex_rounded_rect(math::ComplexRoundedRect rrect);
    PathBuilder& add_ellipse(math::Vec2 center, math::Vec2 radii);
    
    // 完成构建
    [[nodiscard]] Path build();
    [[nodiscard]] bool empty() const noexcept;
    void reset();
};
```

**描述**：几何路径构造器。

**特征**：
- 内部维护一个可增长的 PathCmd 序列
- 调用 `build()` 后，命令序列被移走，构造器恢复为空状态
- 流式 API，支持链式调用

---

## 成员方法

### PathBuilder::move_to()

```cpp
PathBuilder& move_to(math::Vec2 pt);
```

**描述**：移动画笔到 pt，不绘制线条。开始新的子路径。

**参数**：
- `pt`：目标位置

**返回**：自身引用（支持链式调用）

---

### PathBuilder::line_to()

```cpp
PathBuilder& line_to(math::Vec2 pt);
```

**描述**：从当前点画直线到 pt。

**参数**：
- `pt`：目标位置

**返回**：自身引用（支持链式调用）

---

### PathBuilder::cubic_to()

```cpp
PathBuilder& cubic_to(math::Vec2 c1, math::Vec2 c2, math::Vec2 end);
```

**描述**：三次贝塞尔曲线：当前点 → c1 → c2 → end。

**参数**：
- `c1`：第一控制点
- `c2`：第二控制点
- `end`：曲线终点

**返回**：自身引用（支持链式调用）

---

### PathBuilder::quad_to()

```cpp
PathBuilder& quad_to(math::Vec2 ctrl, math::Vec2 end);
```

**描述**：二次贝塞尔曲线：当前点 → ctrl → end。

**参数**：
- `ctrl`：控制点
- `end`：曲线终点

**返回**：自身引用（支持链式调用）

---

### PathBuilder::close()

```cpp
PathBuilder& close();
```

**描述**：关闭当前子路径（用直线连回最近的 MoveTo 点）。

**返回**：自身引用（支持链式调用）

---

### PathBuilder::add_rect()

```cpp
PathBuilder& add_rect(math::Rect rect);
```

**描述**：添加一个矩形（顺时针，从左上角开始）。

**参数**：
- `rect`：矩形几何

**特征**：
- 等价于 `move_to → line_to × 3 → close`

**返回**：自身引用（支持链式调用）

---

### PathBuilder::add_rounded_rect()

```cpp
PathBuilder& add_rounded_rect(math::RoundedRect rrect);
```

**描述**：添加一个圆角矩形。

**参数**：
- `rrect`：圆角矩形（rect + radius_x + radius_y）

**特征**：
- M0 阶段将圆角近似为三次贝塞尔曲线（κ ≈ 0.5523）

**返回**：自身引用（支持链式调用）

---

### PathBuilder::add_complex_rounded_rect()

```cpp
PathBuilder& add_complex_rounded_rect(math::ComplexRoundedRect rrect);
```

**描述**：添加一个四角各自独立椭圆半径的圆角矩形。

**参数**：
- `rrect`：复杂圆角矩形（rect + 四组独立 corner radii）

**特征**：
- 每个角使用 CornerRadii 中各自的 (rx, ry)
- 若某角 rx=0 或 ry=0 则该角退化为直角
- M0 阶段将圆角近似为三次贝塞尔曲线（κ ≈ 0.5523）

**返回**：自身引用（支持链式调用）

---

### PathBuilder::add_ellipse()

```cpp
PathBuilder& add_ellipse(math::Vec2 center, math::Vec2 radii);
```

**描述**：添加一个椭圆。

**参数**：
- `center`：椭圆中心
- `radii`：X 轴半径（radii.x）和 Y 轴半径（radii.y）

**特征**：
- 用 4 段三次贝塞尔曲线近似椭圆（κ ≈ 0.5523）

**返回**：自身引用（支持链式调用）

---

### PathBuilder::build()

```cpp
[[nodiscard]] Path build();
```

**描述**：完成路径构建，返回不可变 Path。

**返回**：不可变路径对象

**特征**：
- 内部命令序列被移走，构造器恢复为空状态，可继续复用

---

### PathBuilder::empty()

```cpp
[[nodiscard]] bool empty() const noexcept;
```

**描述**：当前是否为空（无任何命令）。

**返回**：空返回 true

---

### PathBuilder::reset()

```cpp
void reset();
```

**描述**：重置构造器，清空所有命令。

---

## 使用场景

### 1. 简单三角形

```cpp
Path triangle = PathBuilder{}
    .move_to({100.f, 50.f})
    .line_to({150.f, 150.f})
    .line_to({50.f, 150.f})
    .close()
    .build();

canvas.fill_path(triangle, Brush::solid_rgb(0xFF0000));
```

---

### 2. 矩形路径

```cpp
Path rect = PathBuilder{}
    .add_rect({10.f, 10.f, 200.f, 100.f})
    .build();

canvas.stroke_path(rect, Brush::solid_rgb(0x0000FF), Pen{2.f});
```

---

### 3. 圆角矩形路径

```cpp
Path rrect = PathBuilder{}
    .add_rounded_rect({{10.f, 10.f, 200.f, 100.f}, 15.f})
    .build();

canvas.fill_path(rrect, Brush::solid_rgb(0x00FF00));
```

---

### 4. 椭圆路径

```cpp
Path ellipse = PathBuilder{}
    .add_ellipse({100.f, 100.f}, {50.f, 30.f})
    .build();

canvas.fill_path(ellipse, Brush::solid_rgb(0xFFFF00));
```

---

### 5. 贝塞尔曲线

```cpp
Path bezier = PathBuilder{}
    .move_to({10.f, 100.f})
    .cubic_to({40.f, 10.f}, {90.f, 190.f}, {120.f, 100.f})
    .build();

canvas.stroke_path(bezier, Brush::solid_rgb(0xFF00FF), Pen{3.f});
```

---

### 6. 复杂组合路径

```cpp
PathBuilder builder;
builder.add_rect({10.f, 10.f, 100.f, 100.f});
builder.add_ellipse({60.f, 60.f}, {30.f, 30.f});
Path combined = builder.build();

canvas.fill_path(combined, Brush::solid_rgb(0x00FFFF));
```

---

### 7. 构造器复用

```cpp
PathBuilder builder;

// 第一个路径
Path path1 = builder
    .move_to({10.f, 10.f})
    .line_to({100.f, 10.f})
    .line_to({100.f, 100.f})
    .close()
    .build();

// 构造器已恢复空状态，可继续复用
Path path2 = builder
    .move_to({200.f, 200.f})
    .line_to({300.f, 200.f})
    .line_to({300.f, 300.f})
    .close()
    .build();
```

---

## 性能分析

### 路径构建开销

**特征**：
- PathCmd 序列动态增长（Vector）
- 每次 add_* 可能触发 Vector 扩容
- 建议预留容量（PathBuilder 可添加 reserve() 方法）

---

### 贝塞尔曲线近似

**特征**：
- 圆角矩形和椭圆使用三次贝塞尔曲线近似（κ ≈ 0.5523）
- 近似误差较小，肉眼难以察觉
- GPU 渲染时进一步扁平化为直线段

---

### 路径拷贝

**特征**：
- Path 拷贝会深拷贝 PathCmd 数组
- 推荐使用移动语义避免拷贝

---

## 最佳实践

### 1. 使用链式调用

```cpp
// ✅ 推荐：链式调用
Path path = PathBuilder{}
    .move_to({10.f, 10.f})
    .line_to({100.f, 10.f})
    .line_to({100.f, 100.f})
    .close()
    .build();

// ❌ 不推荐：逐行调用
PathBuilder builder;
builder.move_to({10.f, 10.f});
builder.line_to({100.f, 10.f});
builder.line_to({100.f, 100.f});
builder.close();
Path path = builder.build();
```

---

### 2. 优先使用便捷几何方法

```cpp
// ✅ 推荐：使用 add_rect()
Path rect = PathBuilder{}
    .add_rect({10.f, 10.f, 200.f, 100.f})
    .build();

// ❌ 不推荐：手动绘制矩形
Path rect = PathBuilder{}
    .move_to({10.f, 10.f})
    .line_to({210.f, 10.f})
    .line_to({210.f, 110.f})
    .line_to({10.f, 110.f})
    .close()
    .build();
```

---

### 3. 使用移动语义

```cpp
// ✅ 推荐：移动语义
Path path = builder.build();  // 移动

// ❌ 不推荐：拷贝
Path path1 = builder.build();
Path path2 = path1;  // 深拷贝
```

---

### 4. 复用 PathBuilder

```cpp
// ✅ 推荐：复用构造器
PathBuilder builder;
Path path1 = builder.move_to({10.f, 10.f}).line_to({100.f, 10.f}).close().build();
Path path2 = builder.move_to({200.f, 200.f}).line_to({300.f, 200.f}).close().build();

// ❌ 不推荐：每次创建新构造器
Path path1 = PathBuilder{}.move_to({10.f, 10.f}).line_to({100.f, 10.f}).close().build();
Path path2 = PathBuilder{}.move_to({200.f, 200.f}).line_to({300.f, 200.f}).close().build();
```

---

## 常见陷阱

### 1. 忘记调用 close()

```cpp
// ❌ 问题：路径未闭合
Path path = PathBuilder{}
    .move_to({10.f, 10.f})
    .line_to({100.f, 10.f})
    .line_to({100.f, 100.f})
    .build();  // 缺少 close()

// ✅ 解决：显式 close()
Path path = PathBuilder{}
    .move_to({10.f, 10.f})
    .line_to({100.f, 10.f})
    .line_to({100.f, 100.f})
    .close()
    .build();
```

---

### 2. build() 后继续使用

```cpp
// ❌ 问题：build() 后构造器已清空
PathBuilder builder;
builder.move_to({10.f, 10.f});
Path path = builder.build();
builder.line_to({100.f, 10.f});  // 错误：构造器已空

// ✅ 解决：重新开始
builder.move_to({10.f, 10.f});
builder.line_to({100.f, 10.f});
Path path = builder.build();
```

---

### 3. 拷贝 Path 导致性能问题

```cpp
// ❌ 问题：频繁拷贝
Path path = builder.build();
for (int i = 0; i < 100; ++i) {
    Path copy = path;  // 深拷贝
    canvas.fill_path(copy, brush);
}

// ✅ 解决：直接使用
Path path = builder.build();
for (int i = 0; i < 100; ++i) {
    canvas.fill_path(path, brush);  // 无需拷贝
}
```

---

### 4. 空路径渲染

```cpp
// ❌ 问题：空路径
Path path = PathBuilder{}.build();  // 空路径
canvas.fill_path(path, brush);  // 无渲染

// ✅ 解决：检查空路径
if (!path.empty()) {
    canvas.fill_path(path, brush);
}
```

---

## 完整示例

```cpp
#include <mine/paint/Canvas.h>
#include <mine/paint/PathBuilder.h>
#include <mine/paint/Brush.h>

using namespace mine::paint;
using namespace mine::math;

void draw_paths(Canvas& canvas) {
    // 1. 三角形
    Path triangle = PathBuilder{}
        .move_to({100.f, 50.f})
        .line_to({150.f, 150.f})
        .line_to({50.f, 150.f})
        .close()
        .build();
    
    auto red = Brush::solid_rgb(0xFF0000);
    canvas.fill_path(triangle, red);
    
    // 2. 矩形路径
    Path rect = PathBuilder{}
        .add_rect({200.f, 50.f, 100.f, 100.f})
        .build();
    
    auto blue = Brush::solid_rgb(0x0000FF);
    Pen pen{2.f};
    canvas.stroke_path(rect, blue, pen);
    
    // 3. 圆角矩形
    Path rrect = PathBuilder{}
        .add_rounded_rect({{350.f, 50.f, 100.f, 100.f}, 15.f})
        .build();
    
    auto green = Brush::solid_rgb(0x00FF00);
    canvas.fill_path(rrect, green);
    
    // 4. 椭圆
    Path ellipse = PathBuilder{}
        .add_ellipse({550.f, 100.f}, {50.f, 30.f})
        .build();
    
    auto yellow = Brush::solid_rgb(0xFFFF00);
    canvas.fill_path(ellipse, yellow);
    
    // 5. 贝塞尔曲线
    Path bezier = PathBuilder{}
        .move_to({50.f, 200.f})
        .cubic_to({100.f, 150.f}, {200.f, 250.f}, {250.f, 200.f})
        .build();
    
    auto magenta = Brush::solid_rgb(0xFF00FF);
    Pen thick_pen{3.f};
    canvas.stroke_path(bezier, magenta, thick_pen);
    
    // 6. 复杂组合
    PathBuilder builder;
    builder.add_rect({300.f, 200.f, 80.f, 80.f});
    builder.add_ellipse({340.f, 240.f}, {25.f, 25.f});
    Path combined = builder.build();
    
    auto cyan = Brush::solid_rgb(0x00FFFF);
    canvas.fill_path(combined, cyan);
    
    // 7. 构造器复用
    Path star1 = builder
        .move_to({450.f, 200.f})
        .line_to({470.f, 250.f})
        .line_to({520.f, 250.f})
        .line_to({480.f, 280.f})
        .line_to({500.f, 330.f})
        .line_to({450.f, 300.f})
        .line_to({400.f, 330.f})
        .line_to({420.f, 280.f})
        .line_to({380.f, 250.f})
        .line_to({430.f, 250.f})
        .close()
        .build();
    
    auto orange = Brush::solid_rgb(0xFFA500);
    canvas.fill_path(star1, orange);
}
```

---

## 总结

`Path` 和 `PathBuilder` 是 `mine.paint` 模块的几何路径系统，具备：

1. **Path**：不可变几何路径（MoveTo/LineTo/CubicTo/QuadTo/Close）
2. **PathBuilder**：命令式路径构造器（流式 API）
3. **便捷几何**：矩形、圆角矩形、椭圆等一键添加
4. **值类型**：可拷贝、可移动

**使用建议**：
- 使用链式调用
- 优先使用便捷几何方法（add_rect/add_ellipse）
- 使用移动语义避免拷贝
- 复用 PathBuilder
- 显式调用 close() 闭合路径
- 检查空路径
