/**
 * @file FontFace.h
 * @brief 字体面（FontFace）的公开 API，基于 FreeType 实现。
 *
 * 提供从文件或内存加载字体、设置像素尺寸、光栅化单个字形（glyph）的能力。
 * 光栅化结果为 8-bit 灰度 alpha 位图（FT_RENDER_MODE_NORMAL）。
 *
 * 生命周期：
 *   - FontFace 对象可长期持有（通常贯穿应用程序生命周期）
 *   - GlyphBitmap 中的 data 指针指向 FontFace 内部缓冲区，
 *     下一次调用 rasterize() 前有效。不要在外部持有此指针。
 *   - 非线程安全：同一 FontFace 实例不可并发调用 rasterize()
 */

#pragma once

#include <mine/core/Memory.h>
#include <mine/containers/Vector.h>
#include <cstdint>

namespace mine::text {

// ============================================================================
// HarfBuzz 文字塑形结果类型
// ============================================================================

/**
 * @brief HarfBuzz 塑形后的单个字形信息。
 *
 * 包含字形索引、原始文本偏移、位置和前进量，可直接用于光栅化和排版。
 */
struct ShapedGlyph {
    uint32_t codepoint   = 0;  ///< Unicode 码点（用于光栅化）
    uint32_t glyph_index = 0;  ///< 字体内部字形索引
    uint32_t cluster     = 0;  ///< 原始 UTF-8 文本中的字节偏移
    float    x_advance   = 0;  ///< 水平前进量（像素）
    float    y_advance   = 0;  ///< 垂直前进量（像素，横排时通常为 0）
    float    x_offset    = 0;  ///< 水平偏移（像素，用于标记定位等）
    float    y_offset    = 0;  ///< 垂直偏移（像素，横排时通常为 0）
};

/**
 * @brief HarfBuzz 文字塑形完整结果。
 */
struct ShapeResult {
    containers::Vector<ShapedGlyph> glyphs;     ///< 塑形后的字形序列
    float                           advance = 0; ///< 总水平前进量（像素）
};

// ============================================================================
// 字形度量 / 光栅化（FreeType 层，与 HarfBuzz 协同）
// ============================================================================

/**
 * @brief 字形度量信息（单位：像素）。
 *
 * 坐标系约定：
 *   - bearing_x：字形位图左边距与当前笔触点的水平偏移（正值向右）
 *   - bearing_y：字形位图顶部与基线的垂直偏移（正值向上）
 *   - advance_x：水平笔触前进量（到下一个字形的距离）
 */
struct GlyphMetrics {
    int32_t  bearing_x;  ///< 左边距（像素）
    int32_t  bearing_y;  ///< 顶边距（像素，基线上方为正）
    int32_t  advance_x;  ///< 水平前进量（像素）
    uint32_t width;      ///< 字形位图宽度（像素）
    uint32_t height;     ///< 字形位图高度（像素）
};

/**
 * @brief 字形光栅化结果。
 *
 * data 指针指向 FontFace 内部缓冲区（8-bit 灰度，每像素 1 字节）。
 * 下一次 rasterize() 调用后此指针失效。
 */
struct GlyphBitmap {
    GlyphMetrics    metrics;  ///< 字形度量
    uint32_t        pitch;    ///< 每行字节数（>= metrics.width，可能有对齐填充）
    const uint8_t*  data;     ///< 灰度 alpha 数据指针（内部缓冲，只读）
};

/**
 * @brief 一段文字的可见墨迹包围盒（相对基线起点）。
 *
 * 坐标系约定：
 *   - left/top：相对 draw_text() 基线起点的偏移，top 可为负值
 *   - width/height：实际可见字形位图的包围盒尺寸
 *   - advance_width：按字形 advance 累加后的总笔触前进量（含字符间距）
 *
 * 若文字仅包含空格等无可见墨迹字符，则 width/height 为 0，
 * 但 advance_width 仍保留实际笔触前进量。
 */
struct TextInkBounds {
    float left{0.0f};
    float top{0.0f};
    float width{0.0f};
    float height{0.0f};
    float advance_width{0.0f};
};

/**
 * @brief 字体面，封装 FreeType FT_Face 对象。
 *
 * 使用示例：
 * @code
 *   auto face = mine::text::FontFace::load_from_file("C:/Windows/Fonts/arial.ttf");
 *   face->set_pixel_size(0, 24);
 *
 *   mine::text::GlyphBitmap bitmap{};
 *   if (face->rasterize(U'A', bitmap)) {
 *       // bitmap.data 有效，立即拷贝到图集
 *   }
 * @endcode
 */
class FontFace {
public:
    ~FontFace();

    // 禁止拷贝
    FontFace(const FontFace&)            = delete;
    FontFace& operator=(const FontFace&) = delete;

    /**
     * @brief 从磁盘文件加载字体面。
     * @param path  字体文件路径（UTF-8 编码，支持 .ttf/.otf/.ttc 等 FreeType 格式）
     * @return 成功返回 OwnedPtr<FontFace>；失败返回空指针。
     */
    [[nodiscard]] static core::OwnedPtr<FontFace> load_from_file(const char* path);

    /**
     * @brief 从内存缓冲区加载字体面。
     * @param data  字体数据指针（生命周期须长于 FontFace 对象）
     * @param size  数据字节数
     * @return 成功返回 OwnedPtr<FontFace>；失败返回空指针。
     */
    [[nodiscard]] static core::OwnedPtr<FontFace> load_from_memory(
        const uint8_t* data, size_t size);

    /**
     * @brief 设置字形光栅化的像素尺寸。
     *
     * 调用 rasterize() 前必须先调用此方法。
     * @param width   字形宽度（像素）；传 0 则由 height 等比推算
     * @param height  字形高度（像素）；通常即字号像素值（如 24 → 24px）
     * @return 设置成功返回 true；字体不含该尺寸的位图且缩放失败时返回 false。
     */
    bool set_pixel_size(uint32_t width, uint32_t height);

    /**
     * @brief 光栅化指定 Unicode 码点的字形。
     *
     * 将字形渲染为 8-bit 灰度 alpha 位图（FT_RENDER_MODE_NORMAL）。
     * 结果写入 @p out，out.data 指向内部缓冲区，下次调用前有效。
     *
     * @param codepoint  Unicode 码点（如 U'A' = 0x41）
     * @param out        输出字形位图结构
     * @return 成功（含空白字形）返回 true；找不到字形时返回 false。
     */
    bool rasterize(uint32_t codepoint, GlyphBitmap& out);

    /**
     * @brief 光栅化指定字体内部字形索引的 glyph。
     *
     * 与 rasterize(uint32_t, GlyphBitmap&) 的区别：
     *   - 本函数直接使用 glyph_index（如 HarfBuzz 塑形返回的索引），
     *     跳过 FT_Get_Char_Index 查找（每字形节省一次 cmap 查询）。
     *
     * @param glyph_index  字体内部字形索引（来自 hb_glyph_info_t::codepoint）
     * @param out          输出字形位图结构
     * @return 成功返回 true
     */
    bool rasterize_glyph(uint32_t glyph_index, GlyphBitmap& out);

    /**
     * @brief 直接使用 FreeType 测量单个字形的水平 advance（不经过 HarfBuzz）。
     *
     * 与 measure_text() 的区别：本函数跳过塑形流程，直接调用
     * FT_Load_Glyph 读取 advance。适用于逐字形缓存构建等场景，
     * 保证与 rasterize_glyph 使用的 advance 完全一致。
     *
     * @param glyph_index  字体内部字形索引
     * @param font_size_px 字号（逻辑像素）
     * @return 水平前进量（像素），失败返回 0
     */
    [[nodiscard]] float measure_glyph_advance(uint32_t glyph_index,
                                               float    font_size_px) const;

    /**
     * @brief 返回当前字号下的上行距（ascender，基线上方高度，单位：像素）。
     *
     * 正值。
     */
    [[nodiscard]] int32_t ascender() const noexcept;

    /**
     * @brief 返回当前字号下的下行距（descender，基线下方深度，单位：像素）。
     *
     * 通常为负值。
     */
    [[nodiscard]] int32_t descender() const noexcept;

    /**
     * @brief 返回当前字号下推荐的行高（单位：像素）。
     *
     * = ascender - descender + 行间距。
     */
    [[nodiscard]] int32_t line_height() const noexcept;

    /**
     * @brief 使用 HarfBuzz 塑形测量一段 UTF-8 文字的水平宽度。
     *
     * 内部调用 shape_text() 执行完整塑形（含 kerning、连字等
     * OpenType 特性），累加 ShapedGlyph 的 x_advance 得到总宽度。
     *
     * @param utf8          UTF-8 编码文字缓冲区（无需 null 结尾）
     * @param len           缓冲区字节数
     * @param font_size_px  字号（逻辑像素）
     * @return 文字总水平宽度（像素）；失败或空文字返回 0.0f
     */
    [[nodiscard]] float measure_text(const char* utf8,
                                     size_t      len,
                                     float       font_size_px) const;

    /**
     * @brief 测量一段 UTF-8 文字的实际可见墨迹包围盒。
     *
     * 与 measure_text() 的区别：
     *   - measure_text() 返回笔触前进量（advance）累加宽度
     *   - 本函数返回最终渲染时真实字形位图的可见边界
     *
     * 该结果可用于“视觉居中”场景，避免因 bearing/advance 差异导致
     * 文字看起来偏右或偏下。
     *
     * @param utf8               UTF-8 编码文字缓冲区（无需 null 结尾）
     * @param len                缓冲区字节数
     * @param font_size_px       字号（逻辑像素）
     * @param character_spacing  每个字形 advance 后追加的字符间距（像素）
     * @return 文字墨迹包围盒及总 advance 宽度
     */
    [[nodiscard]] TextInkBounds measure_text_ink_bounds(const char* utf8,
                                                        size_t      len,
                                                        float       font_size_px,
                                                        float       character_spacing = 0.0f) const;

    /**
     * @brief 使用 HarfBuzz 对一段 UTF-8 文字进行塑形（shaping）。
     *
     * 塑形过程包括：字符到字形的映射（cmap）、连字（ligature）替换、
     * 标记定位（mark positioning）、kerning 等 OpenType 特性处理。
     *
     * 返回的 ShapedGlyph 序列可直接用于光栅化和排版：
     *   - codepoint / glyph_index → 传给 rasterize() 获取位图
     *   - x_advance / x_offset → 累加得到笔触位置
     *   - cluster → 关联回原始 UTF-8 文本字节偏移
     *
     * @param utf8          UTF-8 编码文字缓冲区
     * @param len           缓冲区字节数
     * @param font_size_px  字号（逻辑像素）
     * @return 塑形结果；len==0 或字体无效时返回空 glyphs
     */
    [[nodiscard]] ShapeResult shape_text(const char* utf8,
                                         size_t      len,
                                         float       font_size_px) const;

    /**
     * @brief 默认构造（仅供内部工厂函数使用，直接构造得到的对象无效）。
     *
     * 请通过 load_from_file() 或 load_from_memory() 工厂方法创建有效的 FontFace 实例。
     */
    FontFace() = default;

private:
    /// 不透明实现指针，隐藏 FreeType 头文件依赖
    struct Impl;
    Impl* impl_{nullptr};
};

} // namespace mine::text
