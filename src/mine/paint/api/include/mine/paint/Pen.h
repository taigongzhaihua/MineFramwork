/**
 * @file Pen.h
 * @brief 描边样式（线宽、线连接、线端点）。
 *
 * Pen 是一个轻量 POD 结构体，可直接嵌入 DrawCmd。
 * M0 阶段支持：线宽、斜接连接/斜角连接/圆角连接、平端/圆端/方端。
 */

#pragma once

#include <cstdint>

namespace mine::paint {

/**
 * @brief 线段连接类型。
 *
 * 两条线段交汇处的连接形状。
 */
enum class LineJoin : uint8_t {
    Miter,  ///< 斜接（尖角，受 miter_limit 限制）
    Bevel,  ///< 斜角（截去尖角，平直连接）
    Round,  ///< 圆角（弧形连接）
};

/**
 * @brief 线段端点类型。
 *
 * 开放路径两端（起点和终点）的形状。
 */
enum class LineCap : uint8_t {
    Flat,    ///< 平端（路径终点处截止，无延伸）
    Round,   ///< 圆端（以线宽为直径的半圆延伸）
    Square,  ///< 方端（延伸线宽/2 的距离，形成方形）
};

/**
 * @brief 描边样式。
 *
 * 描述路径或几何图形描边时的视觉参数。
 * 作为值类型使用，可直接拷贝到 DrawCmd。
 *
 * 默认值：1px 宽、斜接连接、平端。
 *
 * @code
 *   Pen pen;
 *   pen.width     = 2.0f;
 *   pen.line_join = LineJoin::Round;
 *   pen.start_cap = LineCap::Round;
 *   pen.end_cap   = LineCap::Round;
 * @endcode
 */
struct Pen {
    float    width       = 1.0f;          ///< 描边宽度（逻辑像素）
    LineJoin line_join   = LineJoin::Miter; ///< 折线连接处形状
    LineCap  start_cap   = LineCap::Flat;  ///< 路径起点端点形状
    LineCap  end_cap     = LineCap::Flat;  ///< 路径终点端点形状
    float    miter_limit = 10.0f;          ///< 斜接限制（超出后退化为 Bevel）
};

} // namespace mine::paint
