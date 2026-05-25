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
#include <cstdint>

namespace mine::text {

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
     * @brief 测量一段 UTF-8 文字的水平宽度（不执行光栅化）。
     *
     * 内部逐字符解码 UTF-8 → Unicode 码点，对每个码点调用
     * FT_Load_Glyph（仅加载度量，不渲染位图），累加 advance.x 得到总宽度。
     * 性能远优于逐字调用 rasterize()。
     *
     * 函数内部会临时调用 FT_Set_Pixel_Sizes 切换字号；调用完成后
     * ascender() / descender() / line_height() 的返回值与 font_size_px 对应。
     *
     * @warning 与 rasterize() / set_pixel_size() 同样不可并发调用。
     *
     * @param utf8          UTF-8 编码文字缓冲区（无需 null 结尾）
     * @param len           缓冲区字节数
     * @param font_size_px  字号（逻辑像素，四舍五入为整像素后传给 FreeType）
     * @return 文字总水平宽度（逻辑像素，浮点）；失败或空文字返回 0.0f
     */
    [[nodiscard]] float measure_text(const char* utf8,
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
