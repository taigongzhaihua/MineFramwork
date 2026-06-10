# DrawCmd、DisplayList 与 TextRun 详细接口文档

## 概述

`DrawCmd`、`DisplayList` 和 `TextRun` 是 `mine.paint` 模块的**录制系统核心类型**。

**核心特性：**
- **DrawCmd**：单条绘制命令（扁平结构，约 150+ 字节）
- **DisplayList**：不可变绘制命令列表（Vector<DrawCmd> + Vector<Path> + Vector<TextRun>）
- **TextRun**：文字段落（UTF-8 + 字体 + 大小 + 原点 + 字符间距）
- **值类型**：可拷贝、可移动

---

## 文件位置

```
src/mine/paint/api/include/mine/paint/DrawCmd.h
src/mine/paint/api/include/mine/paint/DisplayList.h
```

---

## DrawCmd（绘制命令）

### 枚举类型

#### DrawCmdKind

```cpp
enum class DrawCmdKind : uint8_t {
    // 状态管理
    Save,
    Restore,
    Transform,
    Translate,
    Scale,
    Rotate,
    ClipRect,
    ClipRoundedRect,
    ClipComplexRoundedRect,
    ClipPolygon,
    ClipPath,
    ClipPop,
    
    // 填充命令
    FillRect,
    FillRoundedRect,
    FillComplexRoundedRect,
    FillEllipse,
    FillPath,
    FillPolygon,
    
    // 描边命令
    StrokeRect,
    StrokeRoundedRect,
    StrokeComplexRoundedRect,
    StrokeBorderedRect,
    StrokeBorderedRoundedRect,
    StrokeEllipse,
    StrokeLine,
    StrokeArc,
    StrokeQuadBezier,
    StrokeCubicBezier,
    StrokePath,
    StrokePolygon,
    
    // 文字渲染
    DrawText,
};
```

**描述**：绘制命令类型枚举。

**枚举值**：
- **状态管理**：Save、Restore、Transform、Translate、Scale、Rotate、ClipRect、ClipRoundedRect、ClipComplexRoundedRect、ClipPolygon、ClipPath、ClipPop
- **填充命令**：FillRect、FillRoundedRect、FillComplexRoundedRect、FillEllipse、FillPath、FillPolygon
- **描边命令**：StrokeRect、StrokeRoundedRect、StrokeComplexRoundedRect、StrokeBorderedRect、StrokeBorderedRoundedRect、StrokeEllipse、StrokeLine、StrokeArc、StrokeQuadBezier、StrokeCubicBezier、StrokePath、StrokePolygon
- **文字渲染**：DrawText

---

### DrawCmd 结构体

```cpp
struct DrawCmd {
    DrawCmdKind         kind{DrawCmdKind::Save};
    
    // 几何参数（按命令类型使用不同字段）
    math::Rect          rect{};                    ///< 矩形几何
    math::RoundedRect   rrect{};                   ///< 圆角矩形
    math::ComplexRoundedRect complex_rrect{};      ///< 复杂圆角矩形
    math::Vec2          pt_a{};                    ///< 点 A（中心点/起点/p0）
    math::Vec2          pt_b{};                    ///< 点 B（半径/终点/p1）
    math::Vec2          pt_c{};                    ///< 点 C（p2）
    math::Vec2          pt_d{};                    ///< 点 D（p3）
    uint32_t            path_index{0};             ///< 路径索引（引用 DisplayList::paths_）
    math::Transform2D   transform{};               ///< 变换矩阵
    Brush               brush{};                   ///< 画刷
    Pen                 pen{};                     ///< 描边样式
    math::Thickness     border_widths{};           ///< 四边描边宽度
    math::CornerRadii   border_radii{};            ///< 四角圆角半径
};
```

**描述**：单条绘制命令（扁平结构）。

**特征**：
- 所有命令共享同一结构体，按 kind 使用不同字段
- 约 150+ 字节（取决于对齐）
- Trivially copyable

**字段**：
- `kind`：命令类型
- `rect`：矩形几何
- `rrect`：圆角矩形
- `complex_rrect`：复杂圆角矩形
- `pt_a`：点 A（中心点/起点/p0）
- `pt_b`：点 B（半径/终点/p1）
- `pt_c`：点 C（p2）
- `pt_d`：点 D（p3）
- `path_index`：路径索引（引用 DisplayList::paths_）
- `transform`：变换矩阵
- `brush`：画刷
- `pen`：描边样式
- `border_widths`：四边描边宽度
- `border_radii`：四角圆角半径

---

## TextRun（文字段落）

### TextRun 结构体

```cpp
struct TextRun {
    containers::InlineString<256> utf8{};          ///< UTF-8 文本（内联存储，最长 255 字节）
    void*                         font_face{nullptr}; ///< 字体（text::FontFace*）
    float                         size_px{16.0f};  ///< 字号（像素）
    float                         origin_x{0.0f};  ///< 基线起点 X
    float                         origin_y{0.0f};  ///< 基线起点 Y
    float                         character_spacing{0.0f}; ///< 字符间距（像素）
};
```

**描述**：文字段落（UTF-8 + 字体 + 大小 + 原点 + 字符间距）。

**特征**：
- 内联存储 UTF-8 文本（最长 255 字节）
- font_face 使用 void* 避免头文件循环依赖（实际类型为 text::FontFace*）

**字段**：
- `utf8`：UTF-8 文本（内联存储，最长 255 字节）
- `font_face`：字体（text::FontFace*）
- `size_px`：字号（像素）
- `origin_x`：基线起点 X
- `origin_y`：基线起点 Y
- `character_spacing`：字符间距（像素）

---

## DisplayList（不可变绘制命令列表）

### DisplayList 类

```cpp
class DisplayList {
public:
    DisplayList() = default;
    explicit DisplayList(
        containers::Vector<DrawCmd> cmds,
        containers::Vector<Path>    paths,
        containers::Vector<TextRun> text_runs) noexcept;
    
    // 访问器
    [[nodiscard]] core::Span<const DrawCmd>  cmds() const noexcept;
    [[nodiscard]] core::Span<const Path>     paths() const noexcept;
    [[nodiscard]] core::Span<const TextRun>  text_runs() const noexcept;
    [[nodiscard]] size_t cmd_count() const noexcept;
    [[nodiscard]] bool empty() const noexcept;
};
```

**描述**：不可变绘制命令列表。

**特征**：
- 由 Canvas::end() 创建
- 可以拷贝（深拷贝所有数据）
- 可以移动（零分配转移）
- IRenderer::render() 接受 DisplayList

---

## 成员方法

### DisplayList::构造函数

```cpp
explicit DisplayList(
    containers::Vector<DrawCmd> cmds,
    containers::Vector<Path>    paths,
    containers::Vector<TextRun> text_runs) noexcept;
```

**描述**：从命令列表、路径列表和文字段落列表构造 DisplayList。

**参数**：
- `cmds`：命令列表
- `paths`：路径列表
- `text_runs`：文字段落列表

**特征**：
- 移动语义，零分配转移

---

### DisplayList::cmds()

```cpp
[[nodiscard]] core::Span<const DrawCmd> cmds() const noexcept;
```

**描述**：返回命令列表。

**返回**：命令列表（只读）

---

### DisplayList::paths()

```cpp
[[nodiscard]] core::Span<const Path> paths() const noexcept;
```

**描述**：返回路径列表。

**返回**：路径列表（只读）

---

### DisplayList::text_runs()

```cpp
[[nodiscard]] core::Span<const TextRun> text_runs() const noexcept;
```

**描述**：返回文字段落列表。

**返回**：文字段落列表（只读）

---

### DisplayList::cmd_count()

```cpp
[[nodiscard]] size_t cmd_count() const noexcept;
```

**描述**：返回命令数量。

**返回**：命令数量

---

### DisplayList::empty()

```cpp
[[nodiscard]] bool empty() const noexcept;
```

**描述**：是否为空（无任何命令）。

**返回**：空返回 true

---

## DrawCmd 命令字段映射

以下列出每种 DrawCmdKind 使用哪些字段：

### 状态管理

| Kind | 使用的字段 |
|------|-----------|
| Save | 无 |
| Restore | 无 |
| Transform | transform |
| Translate | pt_a（offset） |
| Scale | pt_a（factor）|
| Rotate | pt_a.x（angle_rad）|
| ClipRect | rect |
| ClipRoundedRect | rrect |
| ClipComplexRoundedRect | complex_rrect |
| ClipPolygon | path_index（引用 paths_，包含顶点） |
| ClipPath | path_index |
| ClipPop | 无 |

---

### 填充命令

| Kind | 使用的字段 |
|------|-----------|
| FillRect | rect, brush |
| FillRoundedRect | rrect, brush |
| FillComplexRoundedRect | complex_rrect, brush |
| FillEllipse | pt_a（center）, pt_b（radii）, brush |
| FillPath | path_index, brush |
| FillPolygon | path_index, brush |

---

### 描边命令

| Kind | 使用的字段 |
|------|-----------|
| StrokeRect | rect, brush, pen |
| StrokeRoundedRect | rrect, brush, pen |
| StrokeComplexRoundedRect | complex_rrect, brush, pen |
| StrokeBorderedRect | rect, brush, border_widths |
| StrokeBorderedRoundedRect | rect, brush, border_widths, border_radii |
| StrokeEllipse | pt_a（center）, pt_b（radii）, brush, pen |
| StrokeLine | pt_a（from）, pt_b（to）, brush, pen |
| StrokeArc | pt_a（center）, pt_b.x（radius）, pt_b.y（start_angle）, pt_c.x（sweep_angle）, brush, pen |
| StrokeQuadBezier | pt_a（p0）, pt_b（p1）, pt_c（p2）, brush, pen |
| StrokeCubicBezier | pt_a（p0）, pt_b（p1）, pt_c（p2）, pt_d（p3）, brush, pen |
| StrokePath | path_index, brush, pen |
| StrokePolygon | path_index, brush, pen |

---

### 文字渲染

| Kind | 使用的字段 |
|------|-----------|
| DrawText | path_index（引用 text_runs_）, brush |

---

## 使用场景

### 1. 遍历 DisplayList

```cpp
DisplayList dl = canvas.end();

for (const DrawCmd& cmd : dl.cmds()) {
    switch (cmd.kind) {
        case DrawCmdKind::FillRect:
            // 处理填充矩形
            break;
        case DrawCmdKind::StrokeLine:
            // 处理描边线段
            break;
        // ... 其他命令
    }
}
```

---

### 2. 访问路径

```cpp
DisplayList dl = canvas.end();

for (const DrawCmd& cmd : dl.cmds()) {
    if (cmd.kind == DrawCmdKind::FillPath) {
        const Path& path = dl.paths()[cmd.path_index];
        // 处理路径
    }
}
```

---

### 3. 访问文字段落

```cpp
DisplayList dl = canvas.end();

for (const DrawCmd& cmd : dl.cmds()) {
    if (cmd.kind == DrawCmdKind::DrawText) {
        const TextRun& text_run = dl.text_runs()[cmd.path_index];
        // 处理文字
    }
}
```

---

### 4. 分析命令统计

```cpp
DisplayList dl = canvas.end();

size_t fill_count = 0;
size_t stroke_count = 0;
size_t text_count = 0;

for (const DrawCmd& cmd : dl.cmds()) {
    if (cmd.kind >= DrawCmdKind::FillRect && cmd.kind <= DrawCmdKind::FillPolygon) {
        ++fill_count;
    } else if (cmd.kind >= DrawCmdKind::StrokeRect && cmd.kind <= DrawCmdKind::StrokePolygon) {
        ++stroke_count;
    } else if (cmd.kind == DrawCmdKind::DrawText) {
        ++text_count;
    }
}

std::cout << "填充命令: " << fill_count << "\n";
std::cout << "描边命令: " << stroke_count << "\n";
std::cout << "文字命令: " << text_count << "\n";
```

---

## 性能分析

### DrawCmd 内存布局

**特征**：
- 扁平结构，所有字段内联
- 约 150+ 字节（取决于对齐）
- Trivially copyable

---

### DisplayList 拷贝

**特征**：
- DisplayList 拷贝会深拷贝所有数据（cmds_、paths_、text_runs_）
- 推荐使用移动语义避免拷贝

---

### 路径和文字内联化

**特征**：
- DrawCmd 通过 path_index 引用 DisplayList::paths_ 和 text_runs_
- 相同路径重复使用会产生多份拷贝

---

## 最佳实践

### 1. 使用移动语义

```cpp
// ✅ 推荐：移动语义
DisplayList dl = canvas.end();  // 移动
renderer->render(dl, target);

// ❌ 不推荐：拷贝
DisplayList dl1 = canvas.end();
DisplayList dl2 = dl1;  // 深拷贝
```

---

### 2. 避免频繁遍历

```cpp
// ✅ 推荐：一次遍历
DisplayList dl = canvas.end();
for (const DrawCmd& cmd : dl.cmds()) {
    // 处理命令
}

// ❌ 不推荐：多次遍历
DisplayList dl = canvas.end();
for (const DrawCmd& cmd : dl.cmds()) {
    if (cmd.kind == DrawCmdKind::FillRect) {
        // 处理填充矩形
    }
}
for (const DrawCmd& cmd : dl.cmds()) {
    if (cmd.kind == DrawCmdKind::StrokeLine) {
        // 处理描边线段
    }
}
```

---

### 3. 按需访问 paths_ 和 text_runs_

```cpp
// ✅ 推荐：按需访问
DisplayList dl = canvas.end();
for (const DrawCmd& cmd : dl.cmds()) {
    if (cmd.kind == DrawCmdKind::FillPath) {
        const Path& path = dl.paths()[cmd.path_index];
        // 处理路径
    }
}

// ❌ 不推荐：预先遍历所有路径
DisplayList dl = canvas.end();
for (const Path& path : dl.paths()) {
    // 处理路径（不知道哪些命令使用了它）
}
```

---

## 常见陷阱

### 1. 访问错误的字段

```cpp
// ❌ 问题：访问错误的字段
DisplayList dl = canvas.end();
for (const DrawCmd& cmd : dl.cmds()) {
    if (cmd.kind == DrawCmdKind::FillRect) {
        // 错误：FillRect 使用 rect，不使用 pt_a/pt_b
        Vec2 center = cmd.pt_a;  // 未定义行为
    }
}

// ✅ 解决：按照字段映射表访问
DisplayList dl = canvas.end();
for (const DrawCmd& cmd : dl.cmds()) {
    if (cmd.kind == DrawCmdKind::FillRect) {
        Rect rect = cmd.rect;  // 正确
    }
}
```

---

### 2. path_index 越界

```cpp
// ❌ 问题：path_index 越界
DisplayList dl = canvas.end();
for (const DrawCmd& cmd : dl.cmds()) {
    if (cmd.kind == DrawCmdKind::FillPath) {
        const Path& path = dl.paths()[cmd.path_index];  // 可能越界
    }
}

// ✅ 解决：检查边界
DisplayList dl = canvas.end();
for (const DrawCmd& cmd : dl.cmds()) {
    if (cmd.kind == DrawCmdKind::FillPath) {
        if (cmd.path_index < dl.paths().size()) {
            const Path& path = dl.paths()[cmd.path_index];
        }
    }
}
```

---

### 3. 拷贝 DisplayList 导致性能问题

```cpp
// ❌ 问题：频繁拷贝
DisplayList dl = canvas.end();
for (int i = 0; i < 100; ++i) {
    DisplayList copy = dl;  // 深拷贝
    renderer->render(copy, target);
}

// ✅ 解决：直接使用
DisplayList dl = canvas.end();
for (int i = 0; i < 100; ++i) {
    renderer->render(dl, target);  // 无需拷贝
}
```

---

## 完整示例

```cpp
#include <mine/paint/Canvas.h>
#include <mine/paint/DisplayList.h>
#include <mine/paint/DrawCmd.h>
#include <mine/paint/Brush.h>
#include <mine/paint/Pen.h>

using namespace mine::paint;
using namespace mine::math;

void analyze_display_list(const DisplayList& dl) {
    size_t fill_count = 0;
    size_t stroke_count = 0;
    size_t text_count = 0;
    size_t transform_count = 0;
    size_t clip_count = 0;
    
    for (const DrawCmd& cmd : dl.cmds()) {
        // 统计命令类型
        if (cmd.kind >= DrawCmdKind::FillRect && cmd.kind <= DrawCmdKind::FillPolygon) {
            ++fill_count;
        } else if (cmd.kind >= DrawCmdKind::StrokeRect && cmd.kind <= DrawCmdKind::StrokePolygon) {
            ++stroke_count;
        } else if (cmd.kind == DrawCmdKind::DrawText) {
            ++text_count;
        } else if (cmd.kind >= DrawCmdKind::Transform && cmd.kind <= DrawCmdKind::Rotate) {
            ++transform_count;
        } else if (cmd.kind >= DrawCmdKind::ClipRect && cmd.kind <= DrawCmdKind::ClipPop) {
            ++clip_count;
        }
        
        // 访问路径
        if (cmd.kind == DrawCmdKind::FillPath || cmd.kind == DrawCmdKind::StrokePath) {
            if (cmd.path_index < dl.paths().size()) {
                const Path& path = dl.paths()[cmd.path_index];
                std::cout << "路径顶点数: " << path.cmd_count() << "\n";
            }
        }
        
        // 访问文字
        if (cmd.kind == DrawCmdKind::DrawText) {
            if (cmd.path_index < dl.text_runs().size()) {
                const TextRun& text_run = dl.text_runs()[cmd.path_index];
                std::cout << "文字: " << text_run.utf8.view() << "\n";
                std::cout << "字号: " << text_run.size_px << "px\n";
            }
        }
    }
    
    std::cout << "═══ 命令统计 ═══\n";
    std::cout << "填充命令: " << fill_count << "\n";
    std::cout << "描边命令: " << stroke_count << "\n";
    std::cout << "文字命令: " << text_count << "\n";
    std::cout << "变换命令: " << transform_count << "\n";
    std::cout << "裁剪命令: " << clip_count << "\n";
    std::cout << "总命令数: " << dl.cmd_count() << "\n";
    std::cout << "路径数量: " << dl.paths().size() << "\n";
    std::cout << "文字段落: " << dl.text_runs().size() << "\n";
}

void custom_renderer_example(const DisplayList& dl) {
    // 自定义渲染器示例
    for (const DrawCmd& cmd : dl.cmds()) {
        switch (cmd.kind) {
            case DrawCmdKind::FillRect: {
                // 填充矩形
                const Rect& rect = cmd.rect;
                const Brush& brush = cmd.brush;
                if (brush.kind() == BrushKind::SolidColor) {
                    Color color = brush.color();
                    std::cout << "填充矩形: (" << rect.x << ", " << rect.y << ", " 
                              << rect.width << ", " << rect.height << ") "
                              << "颜色: RGBA(" << color.r << ", " << color.g << ", " 
                              << color.b << ", " << color.a << ")\n";
                }
                break;
            }
            
            case DrawCmdKind::StrokeLine: {
                // 描边线段
                const Vec2& from = cmd.pt_a;
                const Vec2& to = cmd.pt_b;
                const Pen& pen = cmd.pen;
                std::cout << "描边线段: (" << from.x << ", " << from.y << ") → ("
                          << to.x << ", " << to.y << ") "
                          << "线宽: " << pen.width << "px\n";
                break;
            }
            
            case DrawCmdKind::DrawText: {
                // 绘制文字
                if (cmd.path_index < dl.text_runs().size()) {
                    const TextRun& text_run = dl.text_runs()[cmd.path_index];
                    std::cout << "绘制文字: \"" << text_run.utf8.view() << "\" "
                              << "位置: (" << text_run.origin_x << ", " << text_run.origin_y << ") "
                              << "字号: " << text_run.size_px << "px\n";
                }
                break;
            }
            
            default:
                break;
        }
    }
}

int main() {
    Canvas canvas;
    
    // 绘制一些内容
    canvas.fill_rect({10, 10, 200, 100}, Brush::solid_rgb(0xFF0000));
    canvas.stroke_line({10, 120}, {210, 120}, Brush::solid_rgb(0x0000FF), Pen{2.0f});
    
    Path triangle = PathBuilder{}
        .move_to({100, 140})
        .line_to({150, 240})
        .line_to({50, 240})
        .close()
        .build();
    canvas.fill_path(triangle, Brush::solid_rgb(0x00FF00));
    
    // 完成录制
    DisplayList dl = canvas.end();
    
    // 分析 DisplayList
    analyze_display_list(dl);
    
    // 自定义渲染
    custom_renderer_example(dl);
    
    return 0;
}
```

---

## 总结

`DrawCmd`、`DisplayList` 和 `TextRun` 是 `mine.paint` 模块的录制系统核心类型，具备：

1. **DrawCmd**：单条绘制命令（扁平结构，约 150+ 字节）
2. **DisplayList**：不可变绘制命令列表（Vector<DrawCmd> + Vector<Path> + Vector<TextRun>）
3. **TextRun**：文字段落（UTF-8 + 字体 + 大小 + 原点 + 字符间距）
4. **值类型**：可拷贝、可移动

**使用建议**：
- 使用移动语义避免拷贝
- 避免频繁遍历
- 按需访问 paths_ 和 text_runs_
- 按照字段映射表访问 DrawCmd 字段
- 检查 path_index 边界
