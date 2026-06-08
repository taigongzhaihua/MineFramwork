/**
 * @file FontFace.cpp
 * @brief FontFace 的 FreeType 实现。
 *
 * FT_Library 以函数内静态变量初始化（首次使用时创建，进程退出时销毁）。
 * FT_Library 在 FreeType 层面是线程安全的（多个 FT_Face 可并发渲染，
 * 但同一 FT_Face 不可并发调用）。
 */

#include <mine/text/FontFace.h>
#include <mine/core/Memory.h>

// FreeType 公开头文件
#include <ft2build.h>
#include FT_FREETYPE_H

// HarfBuzz 文字塑形
#include <hb.h>
#include <hb-ft.h>

#include <algorithm>
#include <cstring>

// Windows 调试输出
#if defined(_WIN32)
#   define WIN32_LEAN_AND_MEAN
#   include <windows.h>
#   define FT_LOG(msg) OutputDebugStringA("[mine.text] " msg "\n")
#else
#   include <cstdio>
#   define FT_LOG(msg) (void)std::fprintf(stderr, "[mine.text] " msg "\n")
#endif

namespace mine::text {

// ── FT_Library 全局单例 ───────────────────────────────────────────────────

/**
 * @brief 获取全局 FT_Library 实例（惰性初始化，进程生命周期内唯一）。
 *
 * 使用局部静态变量保证线程安全初始化（C++11 起标准保证）。
 * 进程退出时由析构函数调用 FT_Done_FreeType。
 */
static FT_Library get_ft_library() {
    // 包装结构：确保析构时调用 FT_Done_FreeType
    struct FtLibraryHolder {
        FT_Library lib{nullptr};

        FtLibraryHolder() {
            const FT_Error err = FT_Init_FreeType(&lib);
            if (err != 0) {
                FT_LOG("FT_Init_FreeType 失败");
                lib = nullptr;
            }
        }

        ~FtLibraryHolder() {
            if (lib != nullptr) {
                FT_Done_FreeType(lib);
                lib = nullptr;
            }
        }
    };

    // C++11 标准保证局部静态初始化是线程安全的
    static FtLibraryHolder holder;
    return holder.lib;
}

// ── 内部实现结构 ─────────────────────────────────────────────────────────

struct FontFace::Impl {
    FT_Face  face{nullptr};  ///< FreeType 字体面
    bool     is_memory_font; ///< 是否为内存加载（影响析构路径）
    FT_UInt  cached_pixel_h{0}; ///< 上次通过 FT_Set_Pixel_Sizes 设置的字号（0=未设置）

    // ── HarfBuzz 塑形上下文（惰性创建）─────────────────────────────────
    hb_font_t* hb_font{nullptr};  ///< HarfBuzz 字体对象（绑定到此 FT_Face）

    explicit Impl(FT_Face f, bool from_memory)
        : face(f), is_memory_font(from_memory) {}

    ~Impl() {
        if (hb_font != nullptr) {
            hb_font_destroy(hb_font);
            hb_font = nullptr;
        }
        if (face != nullptr) {
            FT_Done_Face(face);
            face = nullptr;
        }
    }

    // 禁止拷贝
    Impl(const Impl&)            = delete;
    Impl& operator=(const Impl&) = delete;
};

// ── FontFace 析构 ─────────────────────────────────────────────────────────

FontFace::~FontFace() {
    if (impl_ != nullptr) {
        MINE_DELETE(impl_);
        impl_ = nullptr;
    }
}

// ── 工厂方法 ─────────────────────────────────────────────────────────────

core::OwnedPtr<FontFace> FontFace::load_from_file(const char* path) {
    FT_Library lib = get_ft_library();
    if (lib == nullptr || path == nullptr) {
        return nullptr;
    }

    FT_Face ft_face = nullptr;
    const FT_Error err = FT_New_Face(lib, path, 0, &ft_face);
    if (err != 0 || ft_face == nullptr) {
        FT_LOG("FT_New_Face 失败，无法加载字体文件");
        return nullptr;
    }

    auto* font = MINE_NEW(FontFace);
    font->impl_ = MINE_NEW(Impl, ft_face, false);

    return core::OwnedPtr<FontFace>(
        font,
        &core::detail::typed_deleter<FontFace>);
}

core::OwnedPtr<FontFace> FontFace::load_from_memory(
    const uint8_t* data, size_t size) {

    FT_Library lib = get_ft_library();
    if (lib == nullptr || data == nullptr || size == 0) {
        return nullptr;
    }

    FT_Face ft_face = nullptr;
    // FT_New_Memory_Face 不拷贝数据，data 必须在 FT_Face 生命周期内保持有效
    const FT_Error err = FT_New_Memory_Face(
        lib,
        reinterpret_cast<const FT_Byte*>(data),
        static_cast<FT_Long>(size),
        0,
        &ft_face);

    if (err != 0 || ft_face == nullptr) {
        FT_LOG("FT_New_Memory_Face 失败");
        return nullptr;
    }

    auto* font = MINE_NEW(FontFace);
    font->impl_ = MINE_NEW(Impl, ft_face, true);

    return core::OwnedPtr<FontFace>(
        font,
        &core::detail::typed_deleter<FontFace>);
}

// ── 公开方法 ──────────────────────────────────────────────────────────────

bool FontFace::set_pixel_size(uint32_t width, uint32_t height) {
    if (impl_ == nullptr || impl_->face == nullptr) {
        return false;
    }

    const FT_UInt w = static_cast<FT_UInt>(width);
    const FT_UInt h = static_cast<FT_UInt>(height);

    // 若字号未变更则跳过 FT_Set_Pixel_Sizes（与 measure_text 缓存一致）
    if (h == impl_->cached_pixel_h && (w == 0 || w == impl_->cached_pixel_h)) {
        return true;
    }

    const FT_Error err = FT_Set_Pixel_Sizes(impl_->face, w, h);

    if (err != 0) {
        FT_LOG("FT_Set_Pixel_Sizes 失败");
        return false;
    }
    impl_->cached_pixel_h = h;  // 同步缓存
    return true;
}

bool FontFace::rasterize(uint32_t codepoint, GlyphBitmap& out) {
    if (impl_ == nullptr || impl_->face == nullptr) {
        return false;
    }

    FT_Face face = impl_->face;
    const FT_UInt glyph_index = FT_Get_Char_Index(face, static_cast<FT_ULong>(codepoint));
    return rasterize_glyph(glyph_index, out);
}

bool FontFace::rasterize_glyph(uint32_t glyph_index, GlyphBitmap& out) {
    if (impl_ == nullptr || impl_->face == nullptr) {
        return false;
    }

    FT_Face face = impl_->face;
    // FT_LOAD_FORCE_AUTOHINT：强制使用 FreeType 内置自动 hinting，
    // 对小字号比字体内嵌 TrueType Bytecode Interpreter 效果更稳定
    FT_Error err = FT_Load_Glyph(face, glyph_index, FT_LOAD_FORCE_AUTOHINT);
    if (err != 0) {
        FT_LOG("FT_Load_Glyph 失败");
        return false;
    }

    // 仅对轮廓（outline）类型字形渲染为位图
    // 位图字体（bitmap font）直接使用 face->glyph->bitmap
    if (face->glyph->format != FT_GLYPH_FORMAT_BITMAP) {
        err = FT_Render_Glyph(face->glyph, FT_RENDER_MODE_NORMAL);
        if (err != 0) {
            FT_LOG("FT_Render_Glyph 失败");
            return false;
        }
    }

    const FT_GlyphSlot slot   = face->glyph;
    const FT_Bitmap&   bitmap = slot->bitmap;

    // FreeType 度量单位为 1/64 像素，右移 6 位转换为整像素
    out.metrics.bearing_x = slot->bitmap_left;
    out.metrics.bearing_y = slot->bitmap_top;
    out.metrics.advance_x = static_cast<int32_t>(slot->advance.x >> 6);
    out.metrics.width     = bitmap.width;
    out.metrics.height    = bitmap.rows;

    out.pitch = static_cast<uint32_t>(bitmap.pitch < 0 ? -bitmap.pitch : bitmap.pitch);
    out.data  = bitmap.buffer;

    return true;
}

int32_t FontFace::ascender() const noexcept {
    if (impl_ == nullptr || impl_->face == nullptr) {
        return 0;
    }
    // FT_Size_Metrics::ascender 单位为 1/64 像素，右移 6 位转换为整像素
    return static_cast<int32_t>(impl_->face->size->metrics.ascender >> 6);
}

int32_t FontFace::descender() const noexcept {
    if (impl_ == nullptr || impl_->face == nullptr) {
        return 0;
    }
    return static_cast<int32_t>(impl_->face->size->metrics.descender >> 6);
}

int32_t FontFace::line_height() const noexcept {
    if (impl_ == nullptr || impl_->face == nullptr) {
        return 0;
    }
    // height 包含行间距（ascender + descender + line_gap）
    return static_cast<int32_t>(impl_->face->size->metrics.height >> 6);
}

// ── 文件内部：UTF-8 单字符解码辅助 ───────────────────────────────────────────

/**
 * @brief 从 UTF-8 缓冲区解码下一个 Unicode 码点，并将 p 向前推进。
 *
 * 支持 1~4 字节序列，涵盖完整 Unicode 范围（包含 CJK 统一表意字符）。
 * 遇到非法字节序列时返回替换字符 U+FFFD 并尽力向前推进，
 * 调用方可选择跳过或累计替换字符的宽度。
 *
 * @param p    当前读指针（按引用更新，每次调用后指向下一字符起始位置）
 * @param end  缓冲区末端（不可读）
 * @return 解码所得 Unicode 码点；非法序列返回 0xFFFD
 */
static uint32_t utf8_decode_one(const char*& p, const char* end) noexcept
{
    const auto c0 = static_cast<uint8_t>(*p++);

    // 1 字节 ASCII（0xxxxxxx）
    if (c0 < 0x80u) {
        return static_cast<uint32_t>(c0);
    }

    // 延续字节出现在起始位置 → 非法
    if (c0 < 0xC0u) {
        return 0xFFFDu;
    }

    // 2 字节序列（110xxxxx 10xxxxxx）
    if (c0 < 0xE0u) {
        if (p >= end) { return 0xFFFDu; }
        const auto c1 = static_cast<uint8_t>(*p++);
        if ((c1 & 0xC0u) != 0x80u) { return 0xFFFDu; }
        return ((c0 & 0x1Fu) << 6) | (c1 & 0x3Fu);
    }

    // 3 字节序列（1110xxxx 10xxxxxx 10xxxxxx），涵盖 BMP（含 CJK）
    if (c0 < 0xF0u) {
        if (p + 1 >= end) { return 0xFFFDu; }   // 需再读 2 字节
        const auto c1 = static_cast<uint8_t>(*p++);
        const auto c2 = static_cast<uint8_t>(*p++);
        if ((c1 & 0xC0u) != 0x80u || (c2 & 0xC0u) != 0x80u) { return 0xFFFDu; }
        return ((c0 & 0x0Fu) << 12) | ((c1 & 0x3Fu) << 6) | (c2 & 0x3Fu);
    }

    // 4 字节序列（11110xxx 10xxxxxx 10xxxxxx 10xxxxxx），补充平面
    if (p + 2 >= end) { return 0xFFFDu; }        // 需再读 3 字节
    const auto c1 = static_cast<uint8_t>(*p++);
    const auto c2 = static_cast<uint8_t>(*p++);
    const auto c3 = static_cast<uint8_t>(*p++);
    if ((c1 & 0xC0u) != 0x80u || (c2 & 0xC0u) != 0x80u || (c3 & 0xC0u) != 0x80u) {
        return 0xFFFDu;
    }
    return ((c0 & 0x07u) << 18) | ((c1 & 0x3Fu) << 12) | ((c2 & 0x3Fu) << 6) | (c3 & 0x3Fu);
}

// ── measure_text ──────────────────────────────────────────────────────────────

float FontFace::measure_text(const char* utf8,
                              size_t      len,
                              float       font_size_px) const
{
    if (impl_ == nullptr || impl_->face == nullptr || utf8 == nullptr || len == 0) {
        return 0.0f;
    }

    // ── 优先使用 HarfBuzz 塑形 ──────────────────────────────────────────
    const ShapeResult shaped = shape_text(utf8, len, font_size_px);
    if (!shaped.glyphs.empty()) {
        return shaped.advance;
    }

    // ── 回退：FreeType 逐字测量 ──────────────────────────────────────────
    FT_Face face = impl_->face;

    // 仅在字号变更时才调用 FT_Set_Pixel_Sizes（缓存上次设置的字号）
    const FT_UInt pixel_h = static_cast<FT_UInt>(font_size_px + 0.5f);
    if (pixel_h != impl_->cached_pixel_h) {
        if (FT_Set_Pixel_Sizes(face, 0, pixel_h) != 0) {
            FT_LOG("measure_text：FT_Set_Pixel_Sizes 失败");
            return 0.0f;
        }
        impl_->cached_pixel_h = pixel_h;
    }

    float total_width = 0.0f;
    const char* p   = utf8;
    const char* end = utf8 + len;

    while (p < end) {
        const uint32_t cp = utf8_decode_one(p, end);
        if (cp == 0xFFFDu) {
            continue;
        }

        const FT_UInt glyph_idx = FT_Get_Char_Index(face, static_cast<FT_ULong>(cp));

        if (FT_Load_Glyph(face, glyph_idx, FT_LOAD_FORCE_AUTOHINT) != 0) {
            continue;
        }

        total_width += static_cast<float>(face->glyph->advance.x >> 6);
    }

    return total_width;
}

TextInkBounds FontFace::measure_text_ink_bounds(const char* utf8,
                                                size_t      len,
                                                float       font_size_px,
                                                float       character_spacing) const
{
    TextInkBounds bounds{};
    if (impl_ == nullptr || impl_->face == nullptr || utf8 == nullptr || len == 0) {
        return bounds;
    }

    // ── HarfBuzz 塑形（获取含 kerning 的字形序列 + 笔触前进量）─────────
    const ShapeResult shaped = shape_text(utf8, len, font_size_px);
    if (shaped.glyphs.empty()) return bounds;

    float pen_x = 0.0f;
    bool  has_ink = false;
    float min_x = 0.0f, min_y = 0.0f, max_x = 0.0f, max_y = 0.0f;

    for (const auto& g : shaped.glyphs) {
        // 光栅化单个字形获取位图墨迹包围盒（内联 FreeType，绕过 rasterize 的 const 限制）
        FT_Face face = impl_->face;
        const FT_UInt gidx = FT_Get_Char_Index(face, static_cast<FT_ULong>(g.codepoint));
        int32_t bearing_x = 0, bearing_y = 0;
        uint32_t bmp_w = 0, bmp_h = 0;

        if (FT_Load_Glyph(face, gidx, FT_LOAD_FORCE_AUTOHINT) == 0) {
            if (face->glyph->format != FT_GLYPH_FORMAT_BITMAP) {
                FT_Render_Glyph(face->glyph, FT_RENDER_MODE_NORMAL);
            }
            const FT_GlyphSlot slot = face->glyph;
            const FT_Bitmap&   bmp  = slot->bitmap;
            bearing_x = slot->bitmap_left;
            bearing_y = slot->bitmap_top;
            bmp_w     = bmp.width;
            bmp_h     = bmp.rows;
        }

        if (bmp_w > 0 && bmp_h > 0) {
            const float glyph_left   = pen_x + g.x_offset + static_cast<float>(bearing_x);
            const float glyph_top    = g.y_offset - static_cast<float>(bearing_y);
            const float glyph_right  = glyph_left + static_cast<float>(bmp_w);
            const float glyph_bottom = glyph_top  + static_cast<float>(bmp_h);

            if (!has_ink) {
                min_x = glyph_left; min_y = glyph_top;
                max_x = glyph_right; max_y = glyph_bottom;
                has_ink = true;
            } else {
                min_x = std::min(min_x, glyph_left);
                min_y = std::min(min_y, glyph_top);
                max_x = std::max(max_x, glyph_right);
                max_y = std::max(max_y, glyph_bottom);
            }
        }

        // HarfBuzz 塑形前进量（含 kerning）+ 字符间距
        pen_x += g.x_advance + character_spacing;
    }

    bounds.advance_width = pen_x;
    if (has_ink) {
        bounds.left   = min_x;
        bounds.top    = min_y;
        bounds.width  = max_x - min_x;
        bounds.height = max_y - min_y;
    }
    return bounds;
}

// ============================================================================
// HarfBuzz 文字塑形
// ============================================================================

/** 惰性创建 HarfBuzz 字体对象（绑定到 FT_Face） */
static hb_font_t* ensure_hb_font(FT_Face ft_face) {
    // hb-ft 桥接：从 FT_Face 创建 hb_font_t
    hb_font_t* hb_font = hb_ft_font_create_referenced(ft_face);
    if (hb_font == nullptr) return nullptr;

    // 设置与 rasterize/measure_text 一致的 FreeType load flags，
    // 确保 HarfBuzz 塑形得到的 advance 与 FreeType 光栅化结果对齐
    hb_ft_font_set_load_flags(hb_font, FT_LOAD_FORCE_AUTOHINT);

    return hb_font;
}

float FontFace::measure_glyph_advance(uint32_t glyph_index,
                                       float    font_size_px) const
{
    if (impl_ == nullptr || impl_->face == nullptr || glyph_index == 0) {
        return 0.0f;
    }

    FT_Face face = impl_->face;

    // 设置字号
    const FT_UInt pixel_h = static_cast<FT_UInt>(font_size_px + 0.5f);
    if (pixel_h != impl_->cached_pixel_h) {
        if (FT_Set_Pixel_Sizes(face, 0, pixel_h) != 0) {
            return 0.0f;
        }
        impl_->cached_pixel_h = pixel_h;
    }

    if (FT_Load_Glyph(face, static_cast<FT_UInt>(glyph_index),
                       FT_LOAD_FORCE_AUTOHINT) != 0) {
        return 0.0f;
    }

    return static_cast<float>(face->glyph->advance.x >> 6);
}

// ── shape_text 公开 API ────────────────────────────────────────────────────

ShapeResult FontFace::shape_text(const char* utf8,
                                  size_t      len,
                                  float       font_size_px) const
{
    // Vector 为 explicit 默认构造，需显式初始化
    ShapeResult result;
    result.glyphs = containers::Vector<ShapedGlyph>(core::default_allocator());
    result.advance = 0.0f;

    if (impl_ == nullptr || impl_->face == nullptr || utf8 == nullptr || len == 0) {
        return result;
    }

    FT_Face ft_face = impl_->face;

    // 1. 设置字号（复用 cached_pixel_h 优化）
    const FT_UInt pixel_h = static_cast<FT_UInt>(font_size_px + 0.5f);
    if (pixel_h != impl_->cached_pixel_h) {
        if (FT_Set_Pixel_Sizes(ft_face, 0, pixel_h) != 0) {
            return result;
        }
        impl_->cached_pixel_h = pixel_h;
        // 字号变更 → 销毁旧 hb_font 并在下方重建，确保 HarfBuzz 拿到正确的 scale
        if (impl_->hb_font != nullptr) {
            hb_font_destroy(impl_->hb_font);
            impl_->hb_font = nullptr;
        }
    }

    // 2. 惰性创建 HarfBuzz 字体对象
    if (impl_->hb_font == nullptr) {
        impl_->hb_font = ensure_hb_font(ft_face);
        if (impl_->hb_font == nullptr) return result;
    }

    // 3. 创建塑形缓冲，添加文本
    hb_buffer_t* buf = hb_buffer_create();
    hb_buffer_add_utf8(buf, utf8, static_cast<int>(len), 0, static_cast<int>(len));
    hb_buffer_set_direction(buf, HB_DIRECTION_LTR);
    hb_buffer_set_script(buf, HB_SCRIPT_COMMON);     // 自动检测
    hb_buffer_set_language(buf, hb_language_get_default());

    // 5. 执行塑形
    hb_shape(impl_->hb_font, buf, nullptr, 0);

    // 6. 提取结果
    unsigned int glyph_count = 0;
    hb_glyph_info_t*     glyph_infos = hb_buffer_get_glyph_infos(buf, &glyph_count);
    hb_glyph_position_t* glyph_pos   = hb_buffer_get_glyph_positions(buf, &glyph_count);

    result.glyphs.reserve(glyph_count);

    float total_advance = 0.0f;
    const char* text_base = utf8;

    for (unsigned int i = 0; i < glyph_count; ++i) {
        ShapedGlyph sg;
        sg.glyph_index = glyph_infos[i].codepoint;  // HarfBuzz 用 "codepoint" 字段存 glyph index
        sg.cluster     = glyph_infos[i].cluster;

        // 从原始 UTF-8 文本的 cluster 位置解码 Unicode 码点
        if (sg.cluster < len) {
            const char* p = text_base + sg.cluster;
            sg.codepoint = utf8_decode_one(p, text_base + len);
        }

        // HarfBuzz position 单位为 1/64 像素，转为浮点像素
        sg.x_advance = static_cast<float>(glyph_pos[i].x_advance) / 64.0f;
        sg.y_advance = static_cast<float>(glyph_pos[i].y_advance) / 64.0f;
        sg.x_offset  = static_cast<float>(glyph_pos[i].x_offset)  / 64.0f;
        sg.y_offset  = static_cast<float>(glyph_pos[i].y_offset)  / 64.0f;

        total_advance += sg.x_advance;

        result.glyphs.push_back(sg);
    }

    result.advance = total_advance;

    hb_buffer_destroy(buf);
    return result;
}

} // namespace mine::text
