# TextLayout 详细接口文档

## 概述

`TextLayout` 是 `mine.text` 模块的**文本行布局工具**，提供换行、分行等文本排版辅助功能。

**核心特性：**
- **自动换行**：按宽度限制自动断行
- **段落分割**：按 `\n` 分割段落
- **行信息查询**：根据字节偏移查找所在行

---

## 文件位置

```
src/mine/text/api/include/mine/text/TextLayout.h
```

---

## 类型定义

### LineInfo

```cpp
struct LineInfo {
    uint32_t start_offset = 0;  ///< 该行在原始文本中的字节偏移
    uint32_t byte_length  = 0;  ///< 该行的字节长度（不含 \n）
    float    disp_width   = 0;  ///< 该行显示宽度（像素）
};
```

**描述**：单行文本信息。

**字段**：
- `start_offset`：该行在原始文本中的字节偏移
- `byte_length`：该行的字节长度（不含 `\n`）
- `disp_width`：该行显示宽度（像素）

**特征**：
- `start_offset + byte_length` 指向行尾（可能是 `\n` 或文本结束）
- `disp_width` 由传入的 `LineMeasureFn` 回调测量

---

### LineMeasureFn

```cpp
using LineMeasureFn = float (*)(void* ctx, const char* data, uint32_t len);
```

**描述**：测量一段文本宽度的回调函数类型。

**参数**：
- `ctx`：用户上下文指针（传递给 `split_lines` 的 `measure_ctx`）
- `data`：UTF-8 文本数据指针
- `len`：文本字节长度

**返回**：文本宽度（像素）

**示例**：
```cpp
float my_measure_fn(void* ctx, const char* data, uint32_t len) {
    auto* face = static_cast<FontFace*>(ctx);
    return face->measure_text(data, len, 24.0f);
}
```

---

## 全局函数

### split_lines()

```cpp
void split_lines(
    const char* text,
    uint32_t text_len,
    float max_width,
    LineMeasureFn measure,
    void* measure_ctx,
    bool enable_width_wrap,
    containers::Vector<LineInfo>& out_lines
);
```

**描述**：将一段 UTF-8 文本分割成多行。

**参数**：
- `text`：UTF-8 文本数据指针
- `text_len`：文本字节长度
- `max_width`：最大宽度（像素）；`enable_width_wrap=true` 时生效
- `measure`：测量文本宽度的回调函数
- `measure_ctx`：传递给 `measure` 的用户上下文指针
- `enable_width_wrap`：是否启用宽度换行
- `out_lines`：输出行信息数组

**行为**：
1. 按 `\n` 分割段落（每个 `\n` 产生一个新行）
2. 若 `enable_width_wrap=true`：
   - 对每个段落，按 `max_width` 进行宽度换行
   - 在空格处断行（若无空格则在字符边界强制断行）
3. 若 `enable_width_wrap=false`：
   - 仅按 `\n` 分割，忽略 `max_width`

**特征**：
- `out_lines` 在调用前会被清空（`out_lines.clear()`）
- 空文本返回空数组
- 行尾 `\n` 不计入 `byte_length`

**示例**：
```cpp
const char* text = "Hello\nWorld";
uint32_t len = 11;

containers::Vector<LineInfo> lines;
split_lines(text, len, 100.0f, my_measure_fn, &face, false, lines);

// 结果:
// lines[0]: start_offset=0, byte_length=5, disp_width=?
// lines[1]: start_offset=6, byte_length=5, disp_width=?
```

---

### find_line_by_offset()

```cpp
bool find_line_by_offset(
    const containers::Vector<LineInfo>& lines,
    uint32_t offset,
    size_t& out_line_idx,
    uint32_t& out_line_offset
);
```

**描述**：根据字节偏移查找所在行。

**参数**：
- `lines`：行信息数组（来自 `split_lines`）
- `offset`：原始文本中的字节偏移
- `out_line_idx`：输出行索引（0-based）
- `out_line_offset`：输出在该行内的字节偏移（相对行首）

**返回**：找到返回 `true`；未找到（offset 越界）返回 `false`

**特征**：
- 二分查找实现，时间复杂度 O(log n)
- 若 `offset` 指向 `\n`，返回该 `\n` 所属行
- 若 `offset` 超出文本末尾，返回 `false`

**示例**：
```cpp
// 假设文本 "Hello\nWorld"，lines 已通过 split_lines 获得
size_t line_idx = 0;
uint32_t line_offset = 0;

// 查找偏移 7（字符 'o'）
if (find_line_by_offset(lines, 7, line_idx, line_offset)) {
    // line_idx = 1（第二行）
    // line_offset = 1（该行第二个字符）
}
```

---

## 使用场景

### 1. 简单段落分割（按 \n）

```cpp
const char* text = "Hello\nWorld\n!";
uint32_t len = 13;

containers::Vector<LineInfo> lines;
split_lines(text, len, 0.0f, my_measure_fn, &face, false, lines);

// 结果:
// lines[0]: "Hello"
// lines[1]: "World"
// lines[2]: "!"
```

---

### 2. 宽度自动换行

```cpp
const char* text = "Hello World this is a long line";
uint32_t len = 31;

containers::Vector<LineInfo> lines;
split_lines(text, len, 100.0f, my_measure_fn, &face, true, lines);

// 结果（假设每行最多 2 个单词）:
// lines[0]: "Hello World"
// lines[1]: "this is a"
// lines[2]: "long line"
```

---

### 3. 查找光标所在行

```cpp
// 假设文本 "Hello\nWorld"
containers::Vector<LineInfo> lines;
split_lines(text, 11, 0.0f, my_measure_fn, &face, false, lines);

// 用户点击坐标 (x, y)，计算出字节偏移 offset=7
size_t line_idx = 0;
uint32_t line_offset = 0;

if (find_line_by_offset(lines, 7, line_idx, line_offset)) {
    log("光标在第 {} 行，偏移 {}", line_idx + 1, line_offset);
}
```

---

### 4. 多行文本绘制

```cpp
void draw_multiline_text(const char* text, uint32_t len, float x, float y, float max_width) {
    containers::Vector<LineInfo> lines;
    split_lines(text, len, max_width, my_measure_fn, &face, true, lines);
    
    float line_height = face->line_height();
    float pen_y = y;
    
    for (const auto& line : lines) {
        // 绘制当前行
        draw_text(text + line.start_offset, line.byte_length, x, pen_y);
        
        // 前进到下一行
        pen_y += line_height;
    }
}
```

---

### 5. TextBox 光标定位

```cpp
class TextBox {
    containers::Vector<LineInfo> lines_;
    
public:
    void on_mouse_click(float x, float y) {
        // 1. 根据 y 坐标计算行号
        float line_height = face_->line_height();
        size_t line_idx = static_cast<size_t>(y / line_height);
        
        if (line_idx >= lines_.size()) {
            line_idx = lines_.size() - 1;
        }
        
        // 2. 根据 x 坐标计算该行内偏移
        const LineInfo& line = lines_[line_idx];
        uint32_t line_offset = find_offset_by_x(text_ + line.start_offset, line.byte_length, x);
        
        // 3. 计算全局偏移
        uint32_t global_offset = line.start_offset + line_offset;
        
        // 4. 设置光标
        set_cursor(global_offset);
    }
};
```

---

### 6. 计算文本高度

```cpp
float calculate_text_height(const char* text, uint32_t len, float max_width) {
    containers::Vector<LineInfo> lines;
    split_lines(text, len, max_width, my_measure_fn, &face, true, lines);
    
    return lines.size() * face->line_height();
}
```

---

## 性能分析

### split_lines 时间复杂度

**特征**：
- 最坏情况：O(n²)，每个字符都需要测量一次
- 实际场景：O(n × m)，n 为字符数，m 为单词平均长度
- 优化建议：缓存测量结果

**示例**：
```cpp
// ❌ 问题：每次重新分割
void on_resize(float new_width) {
    split_lines(text_, len_, new_width, my_measure_fn, &face_, true, lines_);
}

// ✅ 解决：仅在必要时重新分割
void on_resize(float new_width) {
    if (std::abs(new_width - last_width_) > 0.01f) {
        split_lines(text_, len_, new_width, my_measure_fn, &face_, true, lines_);
        last_width_ = new_width;
    }
}
```

---

### find_line_by_offset 时间复杂度

**特征**：
- 时间复杂度：O(log n)，二分查找
- 适用于频繁查询场景（如光标移动）

---

## 最佳实践

### 1. 缓存 split_lines 结果

```cpp
class MultilineTextRenderer {
    containers::Vector<LineInfo> lines_;
    float last_max_width_ = 0.0f;
    
public:
    void update_layout(const char* text, uint32_t len, float max_width) {
        if (std::abs(max_width - last_max_width_) < 0.01f && !lines_.empty()) {
            return;  // 宽度未变化，复用缓存
        }
        
        split_lines(text, len, max_width, my_measure_fn, &face_, true, lines_);
        last_max_width_ = max_width;
    }
};
```

---

### 2. 使用 find_line_by_offset 而非线性查找

```cpp
// ✅ 推荐：使用 find_line_by_offset（O(log n)）
size_t line_idx = 0;
uint32_t line_offset = 0;
if (find_line_by_offset(lines, offset, line_idx, line_offset)) {
    // 找到
}

// ❌ 不推荐：线性查找（O(n)）
for (size_t i = 0; i < lines.size(); ++i) {
    if (offset >= lines[i].start_offset && offset < lines[i].start_offset + lines[i].byte_length) {
        // 找到
    }
}
```

---

### 3. 传递正确的 measure_ctx

```cpp
// ✅ 推荐：传递完整上下文
struct MeasureContext {
    FontFace* face;
    float font_size;
};

float my_measure_fn(void* ctx, const char* data, uint32_t len) {
    auto* mctx = static_cast<MeasureContext*>(ctx);
    return mctx->face->measure_text(data, len, mctx->font_size);
}

MeasureContext ctx{&face, 24.0f};
split_lines(text, len, max_width, my_measure_fn, &ctx, true, lines);

// ❌ 不推荐：传递不完整上下文
float my_measure_fn(void* ctx, const char* data, uint32_t len) {
    auto* face = static_cast<FontFace*>(ctx);
    return face->measure_text(data, len, 24.0f);  // 字号硬编码
}
```

---

## 常见陷阱

### 1. 忘记清空 out_lines

```cpp
// ✅ 注意：split_lines 会自动清空 out_lines
containers::Vector<LineInfo> lines;
split_lines(text1, len1, max_width, measure, ctx, true, lines);
// lines 包含 text1 的行信息

split_lines(text2, len2, max_width, measure, ctx, true, lines);
// lines 已被清空，现在包含 text2 的行信息
```

---

### 2. enable_width_wrap=false 时 max_width 无效

```cpp
// ❌ 问题：期望换行但未生效
split_lines(text, len, 100.0f, measure, ctx, false, lines);
// enable_width_wrap=false，max_width 被忽略

// ✅ 解决：启用宽度换行
split_lines(text, len, 100.0f, measure, ctx, true, lines);
```

---

### 3. 忽略 find_line_by_offset 返回值

```cpp
// ❌ 问题：忽略返回值
size_t line_idx = 0;
uint32_t line_offset = 0;
find_line_by_offset(lines, offset, line_idx, line_offset);
// offset 可能越界，line_idx 未初始化

// ✅ 解决：检查返回值
if (find_line_by_offset(lines, offset, line_idx, line_offset)) {
    // 找到
} else {
    log("偏移越界");
}
```

---

### 4. 修改文本后忘记重新分割

```cpp
// ❌ 问题：修改文本后忘记重新分割
text_ = "New text";
// lines_ 仍为旧文本的行信息

// ✅ 解决：立即重新分割
text_ = "New text";
split_lines(text_.c_str(), text_.size(), max_width_, measure, ctx, true, lines_);
```

---

## 完整示例

```cpp
#include <mine/text/TextLayout.h>
#include <mine/text/FontFace.h>
#include <mine/containers/Vector.h>

using namespace mine::text;

class MultilineTextBox {
    core::OwnedPtr<FontFace> face_;
    containers::Vector<LineInfo> lines_;
    std::string text_;
    float max_width_ = 0.0f;
    
public:
    MultilineTextBox(const char* font_path) {
        face_ = FontFace::load_from_file(font_path);
        if (!face_) {
            log("字体加载失败");
            return;
        }
        face_->set_pixel_size(0, 24);
    }
    
    void set_text(const char* text) {
        text_ = text;
        update_layout();
    }
    
    void set_max_width(float width) {
        if (std::abs(width - max_width_) < 0.01f) {
            return;
        }
        max_width_ = width;
        update_layout();
    }
    
    void draw(float x, float y) {
        float line_height = face_->line_height();
        float pen_y = y;
        
        for (const auto& line : lines_) {
            // 绘制当前行
            draw_line(text_.c_str() + line.start_offset, line.byte_length, x, pen_y);
            
            // 前进到下一行
            pen_y += line_height;
        }
    }
    
    void on_mouse_click(float x, float y) {
        // 1. 根据 y 坐标计算行号
        float line_height = face_->line_height();
        size_t line_idx = static_cast<size_t>(y / line_height);
        
        if (line_idx >= lines_.size()) {
            line_idx = lines_.size() - 1;
        }
        
        // 2. 根据 x 坐标计算该行内偏移
        const LineInfo& line = lines_[line_idx];
        uint32_t line_offset = find_offset_by_x(text_.c_str() + line.start_offset, line.byte_length, x);
        
        // 3. 计算全局偏移
        uint32_t global_offset = line.start_offset + line_offset;
        
        // 4. 设置光标
        set_cursor(global_offset);
    }
    
    float get_height() const {
        return lines_.size() * face_->line_height();
    }
    
private:
    void update_layout() {
        split_lines(
            text_.c_str(),
            static_cast<uint32_t>(text_.size()),
            max_width_,
            [](void* ctx, const char* data, uint32_t len) -> float {
                auto* face = static_cast<FontFace*>(ctx);
                return face->measure_text(data, len, 24.0f);
            },
            face_.get(),
            true,
            lines_
        );
    }
    
    void draw_line(const char* text, uint32_t len, float x, float y) {
        ShapeResult result = face_->shape_text(text, len, 24.0f);
        
        float pen_x = x;
        float baseline = y;
        
        for (const auto& glyph : result.glyphs) {
            GlyphBitmap bitmap{};
            if (!face_->rasterize_glyph(glyph.glyph_index, bitmap)) {
                continue;
            }
            
            // 计算字形位置
            float glyph_x = pen_x + glyph.x_offset + bitmap.metrics.bearing_x;
            float glyph_y = baseline + glyph.y_offset - bitmap.metrics.bearing_y;
            
            // 绘制字形
            draw_glyph_bitmap(glyph_x, glyph_y, bitmap);
            
            // 前进笔触
            pen_x += glyph.x_advance;
        }
    }
    
    uint32_t find_offset_by_x(const char* line_text, uint32_t line_len, float x) {
        // 遍历每个字符，找到最接近 x 的位置
        float pen_x = 0.0f;
        uint32_t offset = 0;
        
        while (offset < line_len) {
            // 测量从行首到当前位置的宽度
            float width = face_->measure_text(line_text, offset + 1, 24.0f);
            
            if (width > x) {
                // 找到最接近的位置
                float prev_width = pen_x;
                if (x - prev_width < width - x) {
                    return offset;  // 更接近前一个字符
                } else {
                    return offset + 1;  // 更接近当前字符
                }
            }
            
            pen_x = width;
            
            // 前进到下一个字符
            offset = utf8_next(line_text, offset, line_len);
        }
        
        return line_len;
    }
    
    void draw_glyph_bitmap(float x, float y, const GlyphBitmap& bitmap) {
        // 拷贝到纹理并绘制
        // ...
    }
    
    void set_cursor(uint32_t offset) {
        log("光标位置: {}", offset);
    }
};

int main() {
    MultilineTextBox textbox("C:/Windows/Fonts/arial.ttf");
    
    // 设置文本
    textbox.set_text("Hello\nWorld\nThis is a long line that will wrap");
    
    // 设置最大宽度
    textbox.set_max_width(200.0f);
    
    // 绘制
    textbox.draw(100.0f, 100.0f);
    
    // 获取高度
    float height = textbox.get_height();
    log("文本高度: {}", height);
    
    // 模拟鼠标点击
    textbox.on_mouse_click(150.0f, 120.0f);
    
    return 0;
}
```

---

## 总结

`TextLayout` 是 `mine.text` 模块的文本行布局工具，具备：

1. **自动换行**：按宽度限制自动断行
2. **段落分割**：按 `\n` 分割段落
3. **行信息查询**：根据字节偏移查找所在行

**使用建议**：
- 缓存 `split_lines` 结果（宽度未变化时复用）
- 使用 `find_line_by_offset` 而非线性查找
- 传递正确的 `measure_ctx`（含字号等参数）
- 修改文本后立即重新分割
- 检查 `find_line_by_offset` 返回值（防止越界）
