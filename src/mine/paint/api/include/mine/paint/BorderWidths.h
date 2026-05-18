/**
 * @file BorderWidths.h
 * @brief 已弃用 — 请直接使用 mine::math::Thickness。
 *
 * 保留此文件仅为向后兼容。BorderWidths 与 math::Thickness 语义完全相同
 * （left / top / right / bottom 四边独立厚度），但 math::Thickness 拥有更丰富
 * 的工具函数（horizontal() / vertical() / is_zero() 等），且已与 Rect::deflated()
 * 和 ComplexRoundedRect::inner_rect() 等 API 深度集成。
 */

#pragma once

#include <mine/math/Thickness.h>

namespace mine::paint {

/// @deprecated 请使用 mine::math::Thickness 替代。
using BorderWidths = mine::math::Thickness;

} // namespace mine::paint
