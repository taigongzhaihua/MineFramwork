/**
 * @file GlyphAtlas.h
 * @brief 字形 GPU 图集管理器声明。
 *
 * 从 RhiRenderer.cpp 中提取，负责：
 *   - 维护 1024×1024 R8_UNorm CPU 位图
 *   - Shelf Packer 算法分配图集空间
 *   - 字形缓存（线性查找，最多 2048 条目）
 *   - GPU 纹理懒创建与脏数据上传
 */

#pragma once

#include <cstdint>
#include <cstddef>
#include <mine/core/Memory.h>
#include <mine/gfx/IDevice.h>
#include <mine/gfx/ITexture.h>
#include <mine/text/FontFace.h>

namespace mine::paint {

// ── 辅助结构体 ────────────────────────────────────────────────────────────────

/// Shelf Packer：从图集左上角开始按行分配空间
struct ShelfPacker {
    uint32_t atlas_w{0};   ///< 图集宽度（像素）
    uint32_t atlas_h{0};   ///< 图集高度（像素）
    uint32_t cur_x{0};     ///< 当前行已用宽度
    uint32_t cur_y{0};     ///< 当前行顶部 Y 坐标
    uint32_t shelf_h{0};   ///< 当前行已分配的最大高度

    /// 尝试在图集中分配一块 w×h 的区域
    bool pack(uint32_t w, uint32_t h, uint32_t& out_x, uint32_t& out_y);
};

/// 字形图集缓存键
struct GlyphKey {
    void*    face;       ///< FontFace* 指针（用于区分不同字体）
    uint32_t codepoint;  ///< Unicode 码点
    uint32_t size_px;    ///< 字号（整像素）
};

/// 字形图集条目（缓存键 + 图集位置 + 度量）
struct AtlasEntry {
    GlyphKey key;         ///< 缓存键
    uint16_t atlas_x;     ///< 字形在图集中的 X 坐标（像素）
    uint16_t atlas_y;     ///< 字形在图集中的 Y 坐标（像素）
    uint16_t atlas_w;     ///< 字形宽度（像素，0 表示空白字形）
    uint16_t atlas_h;     ///< 字形高度（像素）
    int16_t  bearing_x;   ///< 左边距（像素，相对基线笔触点）
    int16_t  bearing_y;   ///< 顶边距（像素，基线上方为正）
    int16_t  advance_x;   ///< 水平步进（像素）
};

// ── GlyphAtlas 类 ─────────────────────────────────────────────────────────────

/**
 * @brief 字形 GPU 图集管理器。
 *
 * 非线程安全，在单一渲染线程中使用。
 */
class GlyphAtlas {
public:
    static constexpr uint32_t kAtlasSize = 2048;   ///< 图集边长（像素，升级以支持长文本）
    static constexpr uint32_t kMaxGlyphs = 8192;   ///< 最大字形缓存条目数（升级以支持长文本）
    static constexpr uint32_t kGlyphPad  = 1;      ///< 字形间距（防采样越界）

    GlyphAtlas();
    ~GlyphAtlas() = default;

    GlyphAtlas(const GlyphAtlas&)            = delete;
    GlyphAtlas& operator=(const GlyphAtlas&) = delete;

    /**
     * @brief 查找或插入一个字形到图集。
     * @return 字形条目指针；若图集已满或光栅化失败则返回 nullptr。
     */
    /// 按码点查询/插入（内部 FT_Get_Char_Index → rasterize）
    const AtlasEntry* get_or_insert(text::FontFace* face, uint32_t codepoint, uint32_t size_px);

    /// 按字形索引查询/插入（跳过 FT_Get_Char_Index，配合 HarfBuzz 塑形结果）
    const AtlasEntry* get_or_insert_by_index(text::FontFace* face, uint32_t glyph_index, uint32_t size_px);

    /**
     * @brief 将图集数据上传到 GPU 纹理（若 dirty）。
     * 首次调用时创建 GPU 纹理（全量上传 1024×1024 R8 数据）。
     */
    void flush(gfx::IDevice* device);

    /// 获取 GPU 纹理（flush() 之后有效）
    [[nodiscard]] gfx::ITexture* texture() const noexcept { return gpu_texture_.get(); }

private:
    uint8_t     cpu_data_[kAtlasSize * kAtlasSize]{};  ///< R8 灰度图集（CPU 端）
    ShelfPacker packer_{};                              ///< Shelf 打包器
    bool        dirty_{false};                         ///< 是否有新字形未上传 GPU

    core::OwnedPtr<gfx::ITexture> gpu_texture_;        ///< GPU 纹理（R8_UNorm）

    AtlasEntry  entries_[kMaxGlyphs]{};                ///< 字形缓存（线性数组）
    uint32_t    entry_count_{0};                       ///< 已缓存字形数量

    /// 在已缓存条目中查找（线性扫描）
    const AtlasEntry* find(void* face, uint32_t key, uint32_t size_px) const noexcept;

    /// 将已光栅化的 bitmap 写入图集（get_or_insert / get_or_insert_by_index 共用）
    const AtlasEntry* commit_entry(text::FontFace* face, uint32_t cache_key,
                                   uint32_t size_px, const text::GlyphBitmap& bitmap);
};

} // namespace mine::paint
