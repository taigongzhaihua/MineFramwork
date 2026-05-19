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

    explicit Impl(FT_Face f, bool from_memory)
        : face(f), is_memory_font(from_memory) {}

    ~Impl() {
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

    const FT_Error err = FT_Set_Pixel_Sizes(
        impl_->face,
        static_cast<FT_UInt>(width),
        static_cast<FT_UInt>(height));

    if (err != 0) {
        FT_LOG("FT_Set_Pixel_Sizes 失败");
        return false;
    }
    return true;
}

bool FontFace::rasterize(uint32_t codepoint, GlyphBitmap& out) {
    if (impl_ == nullptr || impl_->face == nullptr) {
        return false;
    }

    FT_Face face = impl_->face;

    // 查找字形索引，0 表示未找到（使用 .notdef 字形，也可视为成功）
    const FT_UInt glyph_index = FT_Get_Char_Index(face, static_cast<FT_ULong>(codepoint));

    // 加载字形（仅加载字形槽，不渲染）
    FT_Error err = FT_Load_Glyph(face, glyph_index, FT_LOAD_DEFAULT);
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

} // namespace mine::text
