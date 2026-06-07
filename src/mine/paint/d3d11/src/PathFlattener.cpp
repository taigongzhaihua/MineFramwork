/**
 * @file PathFlattener.cpp
 * @brief Path 扁平化辅助函数实现。
 */

#include "PathFlattener.h"
#include <algorithm>
#include <cmath>

namespace mine::paint {

float point_line_distance(math::Vec2 p, math::Vec2 a, math::Vec2 b) {
    const math::Vec2 ab = b - a;
    const float len = ab.length();
    if (len <= 1e-6f) {
        return (p - a).length();
    }
    return std::abs((p - a).cross(ab)) / len;
}

void flatten_quad_recursive(
    math::Vec2 p0, math::Vec2 p1, math::Vec2 p2,
    float tolerance,
    int depth,
    containers::Vector<math::Vec2>& out_points)
{
    if (depth >= 10 || point_line_distance(p1, p0, p2) <= tolerance) {
        out_points.push_back(p2);
        return;
    }

    const math::Vec2 p01  = (p0 + p1) * 0.5f;
    const math::Vec2 p12  = (p1 + p2) * 0.5f;
    const math::Vec2 p012 = (p01 + p12) * 0.5f;

    flatten_quad_recursive(p0, p01, p012, tolerance, depth + 1, out_points);
    flatten_quad_recursive(p012, p12, p2, tolerance, depth + 1, out_points);
}

void flatten_cubic_recursive(
    math::Vec2 p0, math::Vec2 p1, math::Vec2 p2, math::Vec2 p3,
    float tolerance,
    int depth,
    containers::Vector<math::Vec2>& out_points)
{
    const float d1 = point_line_distance(p1, p0, p3);
    const float d2 = point_line_distance(p2, p0, p3);
    if (depth >= 10 || std::max(d1, d2) <= tolerance) {
        out_points.push_back(p3);
        return;
    }

    const math::Vec2 p01   = (p0 + p1) * 0.5f;
    const math::Vec2 p12   = (p1 + p2) * 0.5f;
    const math::Vec2 p23   = (p2 + p3) * 0.5f;
    const math::Vec2 p012  = (p01 + p12) * 0.5f;
    const math::Vec2 p123  = (p12 + p23) * 0.5f;
    const math::Vec2 p0123 = (p012 + p123) * 0.5f;

    flatten_cubic_recursive(p0, p01, p012, p0123, tolerance, depth + 1, out_points);
    flatten_cubic_recursive(p0123, p123, p23, p3, tolerance, depth + 1, out_points);
}

void flatten_path_to_subpaths(
    const Path& path,
    containers::Vector<FlattenedSubpath>& out_subpaths,
    float tolerance)
{
    out_subpaths.clear();

    FlattenedSubpath cur;
    bool has_cur = false;
    math::Vec2 cur_pt{};
    math::Vec2 subpath_start{};

    auto flush_cur = [&]() {
        if (!has_cur || cur.points.size() < 2) {
            cur.points.clear();
            cur.closed = false;
            has_cur = false;
            return;
        }

        // 若闭合路径尾点与首点重复，则去重以避免退化边
        if (cur.closed && cur.points.size() >= 2) {
            const auto& first = cur.points.front();
            const auto& last  = cur.points.back();
            if (std::abs(first.x - last.x) < 1e-5f && std::abs(first.y - last.y) < 1e-5f) {
                cur.points.pop_back();
            }
        }

        if (cur.points.size() >= 2) {
            out_subpaths.push_back(std::move(cur));
        }
        cur.points.clear();
        cur.closed = false;
        has_cur = false;
    };

    for (const auto& pc : path.cmds()) {
        if (pc.kind == PathCmdKind::MoveTo) {
            flush_cur();
            has_cur = true;
            subpath_start = pc.pt[0];
            cur_pt = subpath_start;
            cur.closed = false;
            cur.points.push_back(subpath_start);
        }
        else if (pc.kind == PathCmdKind::LineTo) {
            if (!has_cur) {
                has_cur = true;
                subpath_start = pc.pt[0];
                cur_pt = subpath_start;
                cur.closed = false;
                cur.points.push_back(subpath_start);
            }
            else {
                cur.points.push_back(pc.pt[0]);
                cur_pt = pc.pt[0];
            }
        }
        else if (pc.kind == PathCmdKind::QuadTo) {
            if (!has_cur) continue;
            flatten_quad_recursive(cur_pt, pc.pt[0], pc.pt[1], tolerance, 0, cur.points);
            cur_pt = pc.pt[1];
        }
        else if (pc.kind == PathCmdKind::CubicTo) {
            if (!has_cur) continue;
            flatten_cubic_recursive(cur_pt, pc.pt[0], pc.pt[1], pc.pt[2], tolerance, 0, cur.points);
            cur_pt = pc.pt[2];
        }
        else if (pc.kind == PathCmdKind::Close) {
            if (!has_cur) continue;
            cur.closed = true;
            cur_pt = subpath_start;
            flush_cur();
        }
    }

    flush_cur();
}

void reduce_vertices_evenly(
    const containers::Vector<math::Vec2>& input,
    containers::Vector<math::Vec2>& output,
    size_t max_count)
{
    output.clear();
    if (input.empty() || max_count == 0) return;
    if (input.size() <= max_count) {
        output = input;
        return;
    }
    output.reserve(max_count);
    const float step = static_cast<float>(input.size()) / static_cast<float>(max_count);
    for (size_t i = 0; i < max_count; ++i) {
        size_t idx = static_cast<size_t>(static_cast<float>(i) * step);
        if (idx >= input.size()) idx = input.size() - 1;
        output.push_back(input[idx]);
    }
}

} // namespace mine::paint
