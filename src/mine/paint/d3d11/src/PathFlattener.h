/**
 * @file PathFlattener.h
 * @brief Path 扁平化辅助函数声明。
 *
 * 将 PaintPath 中的贝塞尔曲线段递归细分为线段，供 FillPath/StrokePath 使用。
 * 从 RhiRenderer.cpp 中提取。
 */

#pragma once

#include <mine/containers/Vector.h>
#include <mine/math/Vec2.h>
#include <mine/paint/Path.h>

namespace mine::paint {

/// 扁平化后的单个子路径
struct FlattenedSubpath {
    containers::Vector<math::Vec2> points;  ///< 子路径顶点（按绘制顺序）
    bool closed{false};                     ///< 是否为闭合子路径（遇到 Close）
};

/// 计算点到直线（ab）的距离（用于贝塞尔平坦度判定）
float point_line_distance(math::Vec2 p, math::Vec2 a, math::Vec2 b);

/// 二次贝塞尔扁平化（递归细分）
void flatten_quad_recursive(
    math::Vec2 p0, math::Vec2 p1, math::Vec2 p2,
    float tolerance,
    int depth,
    containers::Vector<math::Vec2>& out_points);

/// 三次贝塞尔扁平化（递归细分）
void flatten_cubic_recursive(
    math::Vec2 p0, math::Vec2 p1, math::Vec2 p2, math::Vec2 p3,
    float tolerance,
    int depth,
    containers::Vector<math::Vec2>& out_points);

/// 将 Path 扁平化为多个子路径（支持 LineTo / QuadTo / CubicTo / Close）
void flatten_path_to_subpaths(
    const Path& path,
    containers::Vector<FlattenedSubpath>& out_subpaths,
    float tolerance = 0.35f);

/// 将顶点数量压缩到 max_count（等距抽样，保留首顶点顺序）
void reduce_vertices_evenly(
    const containers::Vector<math::Vec2>& input,
    containers::Vector<math::Vec2>& output,
    size_t max_count);

} // namespace mine::paint
