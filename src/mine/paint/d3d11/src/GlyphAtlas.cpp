/**
 * @file GlyphAtlas.cpp
 * @brief 字形 GPU 图集管理器实现。
 */

#include "GlyphAtlas.h"
#include <mine/text/FontFace.h>
#include <mine/gfx/GfxTypes.h>

namespace mine::paint {

// ── ShelfPacker ────────────────────────────────────────────────────────────────

bool ShelfPacker::pack(uint32_t w, uint32_t h, uint32_t& out_x, uint32_t& out_y) {
    // 当前行容纳不下时，换到新行
    if (cur_x + w > atlas_w) {
        cur_x  = 0;
        cur_y += shelf_h;
        shelf_h = 0;
    }
    if (cur_y + h > atlas_h) {
        return false;  // 图集已满
    }
    out_x    = cur_x;
    out_y    = cur_y;
    cur_x   += w;
    if (h > shelf_h) shelf_h = h;
    return true;
}

// ── GlyphAtlas ─────────────────────────────────────────────────────────────────

GlyphAtlas::GlyphAtlas() {
    packer_.atlas_w = kAtlasSize;
    packer_.atlas_h = kAtlasSize;
    // cpu_data_ 已在构造时零初始化（全黑透明图集）
}

const AtlasEntry* GlyphAtlas::find(void* face, uint32_t codepoint, uint32_t size_px) const noexcept {
    for (uint32_t i = 0; i < entry_count_; ++i) {
        const AtlasEntry& e = entries_[i];
        if (e.key.face == face &&
            e.key.codepoint == codepoint &&
            e.key.size_px == size_px) {
            return &e;
        }
    }
    return nullptr;
}

const AtlasEntry* GlyphAtlas::get_or_insert(
    text::FontFace* face, uint32_t codepoint, uint32_t size_px) {

    if (face == nullptr) return nullptr;

    if (!face->set_pixel_size(0, size_px)) return nullptr;

    text::GlyphBitmap bitmap{};
    if (!face->rasterize(codepoint, bitmap)) return nullptr;

    return commit_entry(face, codepoint, size_px, bitmap);
}

const AtlasEntry* GlyphAtlas::get_or_insert_by_index(
    text::FontFace* face, uint32_t glyph_index, uint32_t size_px) {

    if (face == nullptr) return nullptr;

    if (!face->set_pixel_size(0, size_px)) return nullptr;

    text::GlyphBitmap bitmap{};
    if (!face->rasterize_glyph(glyph_index, bitmap)) return nullptr;

    // 使用 glyph_index 作为缓存键（codepoint 字段复用）
    return commit_entry(face, glyph_index, size_px, bitmap);
}

// ── 内部辅助：将已光栅化的 bitmap 提交到图集 ──────────────────────────────

const AtlasEntry* GlyphAtlas::commit_entry(
    text::FontFace* face, uint32_t cache_key, uint32_t size_px,
    const text::GlyphBitmap& bitmap) {

    if (face == nullptr) return nullptr;

    // 1. 缓存命中直接返回
    const AtlasEntry* cached = find(face, cache_key, size_px);
    if (cached != nullptr) return cached;

    // 2. 缓存已满
    if (entry_count_ >= kMaxGlyphs) return nullptr;

    // 3. 构造新条目
    AtlasEntry& entry = entries_[entry_count_];
    entry.key.face      = face;
    entry.key.codepoint = cache_key;
    entry.key.size_px   = size_px;
    entry.bearing_x     = static_cast<int16_t>(bitmap.metrics.bearing_x);
    entry.bearing_y     = static_cast<int16_t>(bitmap.metrics.bearing_y);
    entry.advance_x     = static_cast<int16_t>(bitmap.metrics.advance_x);
    entry.atlas_w       = static_cast<uint16_t>(bitmap.metrics.width);
    entry.atlas_h       = static_cast<uint16_t>(bitmap.metrics.height);

    // 4. 空白字形（空格等）：仅记录步进，不占图集空间
    if (bitmap.metrics.width == 0 || bitmap.metrics.height == 0) {
        entry.atlas_x = 0;
        entry.atlas_y = 0;
        entry.atlas_w = 0;
        entry.atlas_h = 0;
        ++entry_count_;
        return &entries_[entry_count_ - 1];
    }

    // 5. 在图集中分配区域（含 1px 边距防采样越界）
    uint32_t ax = 0, ay = 0;
    const uint32_t alloc_w = entry.atlas_w + kGlyphPad;
    const uint32_t alloc_h = entry.atlas_h + kGlyphPad;
    if (!packer_.pack(alloc_w, alloc_h, ax, ay)) {
        return nullptr;
    }
    entry.atlas_x = static_cast<uint16_t>(ax);
    entry.atlas_y = static_cast<uint16_t>(ay);

    // 6. 将字形位图逐行写入 CPU 图集
    if (bitmap.data != nullptr) {
        for (uint32_t row = 0; row < entry.atlas_h; ++row) {
            const uint8_t* src_row = bitmap.data + row * bitmap.pitch;
            uint8_t*       dst_row = cpu_data_ + (ay + row) * kAtlasSize + ax;
            for (uint32_t col = 0; col < entry.atlas_w; ++col) {
                dst_row[col] = src_row[col];
            }
        }
    }

    dirty_ = true;
    ++entry_count_;
    return &entries_[entry_count_ - 1];
}

void GlyphAtlas::flush(gfx::IDevice* device) {
    if (device == nullptr) return;

    // 首次 flush：创建 GPU 纹理（R8_UNorm，ShaderResource 绑定）
    if (!gpu_texture_) {
        gfx::TextureDesc tex_desc{};
        tex_desc.width      = kAtlasSize;
        tex_desc.height     = kAtlasSize;
        tex_desc.format     = gfx::PixelFormat::R8_UNorm;
        tex_desc.bind_flags = gfx::TextureBindFlags::ShaderResource;
        tex_desc.mip_levels = 1;
        tex_desc.array_size = 1;

        gpu_texture_ = device->create_texture(tex_desc);
        if (!gpu_texture_) return;

        dirty_ = true;
    }

    // 有新字形加入：全量上传 CPU 图集到 GPU（M0 简化策略）
    if (dirty_) {
        device->update_texture_region(
            gpu_texture_.get(),
            0, 0,
            kAtlasSize,
            kAtlasSize,
            cpu_data_,
            kAtlasSize);
        dirty_ = false;
    }
}

} // namespace mine::paint
