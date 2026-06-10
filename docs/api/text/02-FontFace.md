# FontFace 详细接口文档

## 概述

`FontFace` 是 `mine.text` 模块的**字体面接口**，基于 FreeType 和 HarfBuzz 实现，提供字体加载、光栅化、文字塑形能力。

**核心特性：**
- **字体加载**：从文件或内存加载字体（TTF/OTF/TTC）
- **字形光栅化**：渲染为 8-bit 灰度 alpha 位图
- **文字塑形**：HarfBuzz 塑形（kerning、连字、标记定位）
- **文字测量**：测量宽度、墨迹包围盒
- **字形度量**：bearing、advance、行高

---

## 文件位置

```
src/mine/text/api/include/mine/text/FontFace.h
```

---

## 枚举类型

### HbDirection

```cpp
enum class HbDirection : unsigned int {
    LTR  = 4,   // HB_DIRECTION_LTR
    RTL  = 5,   // HB_DIRECTION_RTL
    TTB  = 6,   // HB_DIRECTION_TTB
    BTT  = 7,   // HB_DIRECTION_BTT
};
```

**描述**：HarfBuzz 文字书写方向。

**枚举值**：
- `LTR`：从左到右（Left-to-Right，如英文）
- `RTL`：从右到左（Right-to-Left，如阿拉伯文）
- `TTB`：从上到下（Top-to-Bottom，如蒙古文）
- `BTT`：从下到上（Bottom-to-Top）

---

### HbScript

```cpp
enum class HbScript : unsigned int {
    COMMON = 0,  // HB_SCRIPT_COMMON — 自动检测
};
```

**描述**：HarfBuzz 文字脚本系统。

**枚举值**：
- `COMMON`：自动检测脚本系统

---

## 结构体

### ShapedGlyph

```cpp
struct ShapedGlyph {
    uint32_t codepoint   = 0;  ///< Unicode 码点（用于光栅化）
    uint32_t glyph_index = 0;  ///< 字体内部字形索引
    uint32_t cluster     = 0;  ///< 原始 UTF-8 文本中的字节偏移
    float    x_advance   = 0;  ///< 水平前进量（像素）
    float    y_advance   = 0;  ///< 垂直前进量（像素，横排时通常为 0）
    float    x_offset    = 0;  ///< 水平偏移（像素，用于标记定位等）
    float    y_offset    = 0;  ///< 垂直偏移（像素，横排时通常为 0）
};
```

**描述**：HarfBuzz 塑形后的单个字形信息。

**字段**：
- `codepoint`：Unicode 码点（用于光栅化）
- `glyph_index`：字体内部字形索引
- `cluster`：原始 UTF-8 文本中的字节偏移
- `x_advance`：水平前进量（像素）
- `y_advance`：垂直前进量（像素，横排时通常为 0）
- `x_offset`：水平偏移（像素，用于标记定位等）
- `y_offset`：垂直偏移（像素，横排时通常为 0）

---

### ShapeResult

```cpp
struct ShapeResult {
    containers::Vector<ShapedGlyph> glyphs;     ///< 塑形后的字形序列
    float                           advance = 0; ///< 总水平前进量（像素）
};
```

**描述**：HarfBuzz 文字塑形完整结果。

**字段**：
- `glyphs`：塑形后的字形序列
- `advance`：总水平前进量（像素）

---

### GlyphMetrics

```cpp
struct GlyphMetrics {
    int32_t  bearing_x;  ///< 左边距（像素）
    int32_t  bearing_y;  ///< 顶边距（像素，基线上方为正）
    int32_t  advance_x;  ///< 水平前进量（像素）
    uint32_t width;      ///< 字形位图宽度（像素）
    uint32_t height;     ///< 字形位图高度（像素）
};
```

**描述**：字形度量信息（单位：像素）。

**坐标系约定**：
- `bearing_x`：字形位图左边距与当前笔触点的水平偏移（正值向右）
- `bearing_y`：字形位图顶部与基线的垂直偏移（正值向上）
- `advance_x`：水平笔触前进量（到下一个字形的距离）

**字段**：
- `bearing_x`：左边距（像素）
- `bearing_y`：顶边距（像素，基线上方为正）
- `advance_x`：水平前进量（像素）
- `width`：字形位图宽度（像素）
- `height`：字形位图高度（像素）

---

### GlyphBitmap

```cpp
struct GlyphBitmap {
    GlyphMetrics    metrics;  ///< 字形度量
    uint32_t        pitch;    ///< 每行字节数（>= metrics.width，可能有对齐填充）
    const uint8_t*  data;     ///< 灰度 alpha 数据指针（内部缓冲，只读）
};
```

**描述**：字形光栅化结果。

**特征**：
- `data` 指针指向 FontFace 内部缓冲区（8-bit 灰度，每像素 1 字节）
- 下一次 `rasterize()` 调用后此指针失效

**字段**：
- `metrics`：字形度量
- `pitch`：每行字节数（>= metrics.width，可能有对齐填充）
- `data`：灰度 alpha 数据指针（内部缓冲，只读）

---

### TextInkBounds

```cpp
struct TextInkBounds {
    float left{0.0f};
    float top{0.0f};
    float width{0.0f};
    float height{0.0f};
    float advance_width{0.0f};
};
```

**描述**：一段文字的可见墨迹包围盒（相对基线起点）。

**坐标系约定**：
- `left`/`top`：相对 draw_text() 基线起点的偏移，top 可为负值
- `width`/`height`：实际可见字形位图的包围盒尺寸
- `advance_width`：按字形 advance 累加后的总笔触前进量（含字符间距）

**特征**：
- 若文字仅包含空格等无可见墨迹字符，则 width/height 为 0
- 但 advance_width 仍保留实际笔触前进量

**字段**：
- `left`：左边距（像素）
- `top`：顶边距（像素，可为负）
- `width`：宽度（像素）
- `height`：高度（像素）
- `advance_width`：总前进量（像素）

---

## FontFace 类

```cpp
class FontFace {
public:
    ~FontFace();
    
    // 禁止拷贝
    FontFace(const FontFace&)            = delete;
    FontFace& operator=(const FontFace&) = delete;
    
    // 工厂方法
    [[nodiscard]] static core::OwnedPtr<FontFace> load_from_file(const char* path);
    [[nodiscard]] static core::OwnedPtr<FontFace> load_from_memory(const uint8_t* data, size_t size);
    
    // 设置
    bool set_pixel_size(uint32_t width, uint32_t height);
    
    // 光栅化
    bool rasterize(uint32_t codepoint, GlyphBitmap& out);
    bool rasterize_glyph(uint32_t glyph_index, GlyphBitmap& out);
    
    // 度量
    [[nodiscard]] float measure_glyph_advance(uint32_t glyph_index, float font_size_px) const;
    [[nodiscard]] int32_t ascender() const noexcept;
    [[nodiscard]] int32_t descender() const noexcept;
    [[nodiscard]] int32_t line_height() const noexcept;
    
    // 测量
    [[nodiscard]] float measure_text(const char* utf8, size_t len, float font_size_px) const;
    [[nodiscard]] TextInkBounds measure_text_ink_bounds(const char* utf8, size_t len, float font_size_px, float character_spacing = 0.0f) const;
    
    // 塑形
    [[nodiscard]] ShapeResult shape_text(const char* utf8, size_t len, float font_size_px) const;
    [[nodiscard]] ShapeResult shape_text_ex(const char* utf8, size_t len, float font_size_px, HbDirection direction, HbScript script, unsigned int language_tag = 0) const;
    
    FontFace() = default;
};
```

---

## 成员方法

### load_from_file()

```cpp
[[nodiscard]] static core::OwnedPtr<FontFace> load_from_file(const char* path);
```

**描述**：从磁盘文件加载字体面。

**参数**：
- `path`：字体文件路径（UTF-8 编码，支持 .ttf/.otf/.ttc 等 FreeType 格式）

**返回**：成功返回 `OwnedPtr<FontFace>`；失败返回空指针

**示例**：
```cpp
auto face = FontFace::load_from_file("C:/Windows/Fonts/arial.ttf");
if (!face) {
    log("字体加载失败");
    return;
}
```

---

### load_from_memory()

```cpp
[[nodiscard]] static core::OwnedPtr<FontFace> load_from_memory(const uint8_t* data, size_t size);
```

**描述**：从内存缓冲区加载字体面。

**参数**：
- `data`：字体数据指针（生命周期须长于 FontFace 对象）
- `size`：数据字节数

**返回**：成功返回 `OwnedPtr<FontFace>`；失败返回空指针

**示例**：
```cpp
// 假设字体数据已嵌入
extern const uint8_t embedded_font[];
extern const size_t embedded_font_size;

auto face = FontFace::load_from_memory(embedded_font, embedded_font_size);
```

---

### set_pixel_size()

```cpp
bool set_pixel_size(uint32_t width, uint32_t height);
```

**描述**：设置字形光栅化的像素尺寸。

**参数**：
- `width`：字形宽度（像素）；传 0 则由 height 等比推算
- `height`：字形高度（像素）；通常即字号像素值（如 24 → 24px）

**返回**：设置成功返回 true；字体不含该尺寸的位图且缩放失败时返回 false

**特征**：
- 调用 `rasterize()` 前必须先调用此方法
- width 传 0 时由 height 等比推算

**示例**：
```cpp
face->set_pixel_size(0, 24);  // 24px 字号
```

---

### rasterize()

```cpp
bool rasterize(uint32_t codepoint, GlyphBitmap& out);
```

**描述**：光栅化指定 Unicode 码点的字形。

**参数**：
- `codepoint`：Unicode 码点（如 U'A' = 0x41）
- `out`：输出字形位图结构

**返回**：成功（含空白字形）返回 true；找不到字形时返回 false

**特征**：
- 将字形渲染为 8-bit 灰度 alpha 位图（FT_RENDER_MODE_NORMAL）
- 结果写入 `out`，`out.data` 指向内部缓冲区，下次调用前有效

**示例**：
```cpp
GlyphBitmap bitmap{};
if (face->rasterize(U'A', bitmap)) {
    // bitmap.data 有效，立即拷贝到图集
    copy_to_atlas(bitmap.data, bitmap.metrics.width, bitmap.metrics.height, bitmap.pitch);
}
```

---

### rasterize_glyph()

```cpp
bool rasterize_glyph(uint32_t glyph_index, GlyphBitmap& out);
```

**描述**：光栅化指定字体内部字形索引的 glyph。

**参数**：
- `glyph_index`：字体内部字形索引（来自 hb_glyph_info_t::codepoint）
- `out`：输出字形位图结构

**返回**：成功返回 true

**特征**：
- 与 `rasterize(uint32_t, GlyphBitmap&)` 的区别：
  - 本函数直接使用 glyph_index（如 HarfBuzz 塑形返回的索引）
  - 跳过 FT_Get_Char_Index 查找（每字形节省一次 cmap 查询）

**示例**：
```cpp
ShapeResult result = face->shape_text("Hello", 5, 24.0f);
for (const auto& glyph : result.glyphs) {
    GlyphBitmap bitmap{};
    face->rasterize_glyph(glyph.glyph_index, bitmap);
    // 使用 bitmap
}
```

---

### measure_glyph_advance()

```cpp
[[nodiscard]] float measure_glyph_advance(uint32_t glyph_index, float font_size_px) const;
```

**描述**：直接使用 FreeType 测量单个字形的水平 advance（不经过 HarfBuzz）。

**参数**：
- `glyph_index`：字体内部字形索引
- `font_size_px`：字号（逻辑像素）

**返回**：水平前进量（像素），失败返回 0

**特征**：
- 与 `measure_text()` 的区别：本函数跳过塑形流程
- 直接调用 FT_Load_Glyph 读取 advance
- 适用于逐字形缓存构建等场景
- 保证与 rasterize_glyph 使用的 advance 完全一致

---

### ascender()

```cpp
[[nodiscard]] int32_t ascender() const noexcept;
```

**描述**：返回当前字号下的上行距（ascender，基线上方高度，单位：像素）。

**返回**：正值

---

### descender()

```cpp
[[nodiscard]] int32_t descender() const noexcept;
```

**描述**：返回当前字号下的下行距（descender，基线下方深度，单位：像素）。

**返回**：通常为负值

---

### line_height()

```cpp
[[nodiscard]] int32_t line_height() const noexcept;
```

**描述**：返回当前字号下推荐的行高（单位：像素）。

**返回**：`ascender - descender + 行间距`

---

### measure_text()

```cpp
[[nodiscard]] float measure_text(const char* utf8, size_t len, float font_size_px) const;
```

**描述**：使用 HarfBuzz 塑形测量一段 UTF-8 文字的水平宽度。

**参数**：
- `utf8`：UTF-8 编码文字缓冲区（无需 null 结尾）
- `len`：缓冲区字节数
- `font_size_px`：字号（逻辑像素）

**返回**：文字总水平宽度（像素）；失败或空文字返回 0.0f

**特征**：
- 内部调用 `shape_text()` 执行完整塑形
- 含 kerning、连字等 OpenType 特性
- 累加 ShapedGlyph 的 x_advance 得到总宽度

**示例**：
```cpp
float width = face->measure_text("Hello", 5, 24.0f);
log("宽度: {}", width);
```

---

### measure_text_ink_bounds()

```cpp
[[nodiscard]] TextInkBounds measure_text_ink_bounds(const char* utf8, size_t len, float font_size_px, float character_spacing = 0.0f) const;
```

**描述**：测量一段 UTF-8 文字的实际可见墨迹包围盒。

**参数**：
- `utf8`：UTF-8 编码文字缓冲区（无需 null 结尾）
- `len`：缓冲区字节数
- `font_size_px`：字号（逻辑像素）
- `character_spacing`：每个字形 advance 后追加的字符间距（像素）

**返回**：文字墨迹包围盒及总 advance 宽度

**特征**：
- 与 `measure_text()` 的区别：
  - `measure_text()` 返回笔触前进量（advance）累加宽度
  - 本函数返回最终渲染时真实字形位图的可见边界
- 该结果可用于"视觉居中"场景
- 避免因 bearing/advance 差异导致文字看起来偏右或偏下

**示例**：
```cpp
TextInkBounds bounds = face->measure_text_ink_bounds("Hello", 5, 24.0f);
log("墨迹: left={}, top={}, width={}, height={}, advance={}",
    bounds.left, bounds.top, bounds.width, bounds.height, bounds.advance_width);
```

---

### shape_text()

```cpp
[[nodiscard]] ShapeResult shape_text(const char* utf8, size_t len, float font_size_px) const;
```

**描述**：使用 HarfBuzz 对一段 UTF-8 文字进行塑形（shaping）。

**参数**：
- `utf8`：UTF-8 编码文字缓冲区
- `len`：缓冲区字节数
- `font_size_px`：字号（逻辑像素）

**返回**：塑形结果；len==0 或字体无效时返回空 glyphs

**特征**：
- 塑形过程包括：
  - 字符到字形的映射（cmap）
  - 连字（ligature）替换
  - 标记定位（mark positioning）
  - kerning 等 OpenType 特性处理
- 返回的 ShapedGlyph 序列可直接用于光栅化和排版：
  - codepoint / glyph_index → 传给 rasterize() 获取位图
  - x_advance / x_offset → 累加得到笔触位置
  - cluster → 关联回原始 UTF-8 文本字节偏移

**示例**：
```cpp
ShapeResult result = face->shape_text("Hello", 5, 24.0f);
for (const auto& glyph : result.glyphs) {
    log("glyph_index={}, x_advance={}", glyph.glyph_index, glyph.x_advance);
}
```

---

### shape_text_ex()

```cpp
[[nodiscard]] ShapeResult shape_text_ex(const char* utf8, size_t len, float font_size_px, HbDirection direction, HbScript script, unsigned int language_tag = 0) const;
```

**描述**：塑形并显式指定书写方向。

**参数**：
- `utf8`：UTF-8 编码文字缓冲区
- `len`：缓冲区字节数
- `font_size_px`：字号（逻辑像素）
- `direction`：HB_DIRECTION_LTR / RTL / TTB / BTT
- `script`：HB_SCRIPT_* 常量（HB_SCRIPT_COMMON 为自动检测）
- `language_tag`：hb_language_t（hb_language_get_default() 为系统默认）

**返回**：塑形结果

**特征**：
- 与 `shape_text` 的区别：
  - 跳过 hb_buffer_guess_segment_properties
  - 使用调用方指定的 direction / script / language

**示例**：
```cpp
ShapeResult result = face->shape_text_ex(
    "مرحبا", 10, 24.0f,
    HbDirection::RTL,
    HbScript::COMMON
);
```

---

## 使用场景

### 1. 加载字体

```cpp
// 从文件加载
auto face = FontFace::load_from_file("C:/Windows/Fonts/arial.ttf");
if (!face) {
    log("字体加载失败");
    return;
}

// 设置字号
face->set_pixel_size(0, 24);
```

---

### 2. 光栅化单个字形

```cpp
face->set_pixel_size(0, 24);

GlyphBitmap bitmap{};
if (face->rasterize(U'A', bitmap)) {
    // 拷贝到图集
    copy_to_atlas(bitmap.data, bitmap.metrics.width, bitmap.metrics.height, bitmap.pitch);
}
```

---

### 3. 测量文字宽度

```cpp
float width = face->measure_text("Hello", 5, 24.0f);
log("宽度: {}", width);
```

---

### 4. 测量墨迹包围盒

```cpp
TextInkBounds bounds = face->measure_text_ink_bounds("Hello", 5, 24.0f);
log("墨迹: left={}, top={}, width={}, height={}, advance={}",
    bounds.left, bounds.top, bounds.width, bounds.height, bounds.advance_width);
```

---

### 5. 文字塑形

```cpp
ShapeResult result = face->shape_text("Hello", 5, 24.0f);

float x = 0.0f;
for (const auto& glyph : result.glyphs) {
    GlyphBitmap bitmap{};
    face->rasterize_glyph(glyph.glyph_index, bitmap);
    
    // 绘制字形
    draw_glyph(x + glyph.x_offset, baseline + glyph.y_offset, bitmap);
    
    // 前进笔触
    x += glyph.x_advance;
}
```

---

### 6. 获取行高

```cpp
face->set_pixel_size(0, 24);

int32_t ascender = face->ascender();
int32_t descender = face->descender();
int32_t line_height = face->line_height();

log("ascender={}, descender={}, line_height={}", ascender, descender, line_height);
```

---

## 线程安全

**特征**：
- FontFace 非线程安全
- 同一 FontFace 实例不可并发调用 `rasterize()`
- 不同 FontFace 实例可并发使用

**示例**：
```cpp
// ❌ 问题：多线程并发调用
std::thread t1([&face]() {
    face->rasterize(U'A', bitmap1);
});
std::thread t2([&face]() {
    face->rasterize(U'B', bitmap2);  // 崩溃
});

// ✅ 解决：每线程单独 FontFace
auto face1 = FontFace::load_from_file("arial.ttf");
auto face2 = FontFace::load_from_file("arial.ttf");
std::thread t1([&face1]() {
    face1->rasterize(U'A', bitmap1);
});
std::thread t2([&face2]() {
    face2->rasterize(U'B', bitmap2);
});
```

---

## 性能分析

### 字形缓存

**特征**：
- 光栅化开销大，应缓存字形位图
- 典型场景：字形图集（Glyph Atlas）

**示例**：
```cpp
class GlyphAtlas {
    struct CachedGlyph {
        math::Rect uv;
        GlyphMetrics metrics;
    };
    
    std::unordered_map<uint32_t, CachedGlyph> cache_;
    
public:
    const CachedGlyph* get_or_rasterize(FontFace* face, uint32_t codepoint) {
        auto it = cache_.find(codepoint);
        if (it != cache_.end()) {
            return &it->second;  // 缓存命中
        }
        
        // 缓存未命中，光栅化
        GlyphBitmap bitmap{};
        if (!face->rasterize(codepoint, bitmap)) {
            return nullptr;
        }
        
        // 拷贝到图集纹理
        math::Rect uv = upload_to_texture(bitmap.data, bitmap.metrics.width, bitmap.metrics.height, bitmap.pitch);
        
        // 缓存
        CachedGlyph cached{uv, bitmap.metrics};
        cache_[codepoint] = cached;
        return &cache_[codepoint];
    }
};
```

---

### HarfBuzz 塑形开销

**特征**：
- HarfBuzz 塑形有一定开销
- 应缓存塑形结果（如静态文本）

**示例**：
```cpp
// ❌ 问题：每帧重新塑形
void on_render() {
    ShapeResult result = face->shape_text("Hello", 5, 24.0f);
    // 绘制...
}

// ✅ 解决：缓存塑形结果
ShapeResult cached_result_;

void set_text(const char* text, size_t len) {
    cached_result_ = face->shape_text(text, len, 24.0f);
}

void on_render() {
    // 直接使用缓存
    for (const auto& glyph : cached_result_.glyphs) {
        // 绘制...
    }
}
```

---

## 最佳实践

### 1. 缓存字形位图

```cpp
// ✅ 推荐：缓存字形位图到图集
GlyphAtlas atlas;
const CachedGlyph* cached = atlas.get_or_rasterize(face.get(), U'A');

// ❌ 不推荐：每次重新光栅化
GlyphBitmap bitmap{};
face->rasterize(U'A', bitmap);
```

---

### 2. 使用 shape_text 而非 measure_text

```cpp
// ✅ 推荐：shape_text 一次性获取全部信息
ShapeResult result = face->shape_text("Hello", 5, 24.0f);
float width = result.advance;
// 同时可用于绘制

// ❌ 不推荐：measure_text 只能测量宽度
float width = face->measure_text("Hello", 5, 24.0f);
// 绘制时还需再次 shape
```

---

### 3. 立即拷贝 GlyphBitmap.data

```cpp
// ✅ 推荐：立即拷贝
GlyphBitmap bitmap{};
face->rasterize(U'A', bitmap);
std::vector<uint8_t> copy(bitmap.data, bitmap.data + bitmap.metrics.height * bitmap.pitch);

// ❌ 不推荐：持有指针
GlyphBitmap bitmap{};
face->rasterize(U'A', bitmap);
const uint8_t* ptr = bitmap.data;  // 下次 rasterize 后失效
```

---

### 4. 使用 rasterize_glyph 而非 rasterize

```cpp
// ✅ 推荐：使用 rasterize_glyph（跳过 cmap 查找）
ShapeResult result = face->shape_text("Hello", 5, 24.0f);
for (const auto& glyph : result.glyphs) {
    face->rasterize_glyph(glyph.glyph_index, bitmap);
}

// ❌ 不推荐：使用 rasterize（每字形查询 cmap）
for (const auto& glyph : result.glyphs) {
    face->rasterize(glyph.codepoint, bitmap);
}
```

---

## 常见陷阱

### 1. 忘记调用 set_pixel_size

```cpp
// ❌ 问题：忘记设置字号
auto face = FontFace::load_from_file("arial.ttf");
GlyphBitmap bitmap{};
face->rasterize(U'A', bitmap);  // 失败

// ✅ 解决：先设置字号
face->set_pixel_size(0, 24);
face->rasterize(U'A', bitmap);
```

---

### 2. 持有 GlyphBitmap.data 指针

```cpp
// ❌ 问题：持有指针
GlyphBitmap bitmap1{};
face->rasterize(U'A', bitmap1);
const uint8_t* ptr = bitmap1.data;

GlyphBitmap bitmap2{};
face->rasterize(U'B', bitmap2);
// ptr 已失效

// ✅ 解决：立即拷贝
std::vector<uint8_t> copy(bitmap1.data, bitmap1.data + bitmap1.metrics.height * bitmap1.pitch);
```

---

### 3. 多线程并发使用同一 FontFace

```cpp
// ❌ 问题：多线程并发
std::thread t1([&face]() {
    face->rasterize(U'A', bitmap1);
});
std::thread t2([&face]() {
    face->rasterize(U'B', bitmap2);  // 崩溃
});

// ✅ 解决：每线程单独 FontFace
auto face1 = FontFace::load_from_file("arial.ttf");
auto face2 = FontFace::load_from_file("arial.ttf");
```

---

### 4. 忽略 load_from_file 失败

```cpp
// ❌ 问题：忽略失败
auto face = FontFace::load_from_file("nonexistent.ttf");
face->set_pixel_size(0, 24);  // 崩溃

// ✅ 解决：检查失败
auto face = FontFace::load_from_file("nonexistent.ttf");
if (!face) {
    log("字体加载失败");
    return;
}
```

---

## 完整示例

```cpp
#include <mine/text/FontFace.h>
#include <mine/containers/Vector.h>

using namespace mine::text;

class TextRenderer {
    core::OwnedPtr<FontFace> face_;
    
public:
    TextRenderer(const char* font_path) {
        face_ = FontFace::load_from_file(font_path);
        if (!face_) {
            log("字体加载失败");
            return;
        }
        face_->set_pixel_size(0, 24);
    }
    
    void draw_text(const char* text, size_t len, float x, float y) {
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
    
    float measure_width(const char* text, size_t len) {
        return face_->measure_text(text, len, 24.0f);
    }
    
    TextInkBounds measure_ink_bounds(const char* text, size_t len) {
        return face_->measure_text_ink_bounds(text, len, 24.0f);
    }
    
private:
    void draw_glyph_bitmap(float x, float y, const GlyphBitmap& bitmap) {
        // 拷贝到纹理并绘制
        // ...
    }
};

int main() {
    TextRenderer renderer("C:/Windows/Fonts/arial.ttf");
    
    // 绘制文本
    renderer.draw_text("Hello世界", 11, 100.0f, 200.0f);
    
    // 测量宽度
    float width = renderer.measure_width("Hello世界", 11);
    log("宽度: {}", width);
    
    // 测量墨迹包围盒
    TextInkBounds bounds = renderer.measure_ink_bounds("Hello世界", 11);
    log("墨迹: left={}, top={}, width={}, height={}",
        bounds.left, bounds.top, bounds.width, bounds.height);
    
    return 0;
}
```

---

## 总结

`FontFace` 是 `mine.text` 模块的字体面接口，具备：

1. **字体加载**：从文件或内存加载字体（TTF/OTF/TTC）
2. **字形光栅化**：渲染为 8-bit 灰度 alpha 位图
3. **文字塑形**：HarfBuzz 塑形（kerning、连字、标记定位）
4. **文字测量**：测量宽度、墨迹包围盒
5. **字形度量**：bearing、advance、行高

**使用建议**：
- 缓存字形位图到图集
- 使用 `shape_text` 而非 `measure_text`
- 立即拷贝 `GlyphBitmap.data`
- 使用 `rasterize_glyph` 而非 `rasterize`
- 检查 `load_from_file` 失败
- 避免多线程并发使用同一 FontFace
