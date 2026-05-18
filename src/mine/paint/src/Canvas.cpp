/**
 * @file Canvas.cpp
 * @brief Canvas 绘制上下文实现（录制模式）。
 */

#include <mine/paint/Canvas.h>

#include <cmath>     // std::cos, std::sin

namespace mine::paint {

// ── 状态管理 ──────────────────────────────────────────────────────────────────

void Canvas::save() {
    DrawCmd cmd;
    cmd.kind = DrawCmdKind::TransformPush;
    push(cmd);
}

void Canvas::restore() {
    DrawCmd cmd;
    cmd.kind = DrawCmdKind::TransformPop;
    push(cmd);
}

void Canvas::transform(const math::Transform2D& t) {
    DrawCmd cmd;
    cmd.kind      = DrawCmdKind::TransformPush;
    cmd.transform = t;
    push(cmd);
}

void Canvas::translate(math::Vec2 offset) {
    transform(math::Transform2D::translation(offset));
}

void Canvas::scale(float factor) {
    transform(math::Transform2D::scale(factor));
}

void Canvas::scale(math::Vec2 factor) {
    transform(math::Transform2D::scale(factor.x, factor.y));
}

void Canvas::rotate(float angle_rad) {
    transform(math::Transform2D::rotation(angle_rad));
}

void Canvas::clip_rect(math::Rect rect) {
    DrawCmd cmd;
    cmd.kind = DrawCmdKind::ClipPushRect;
    cmd.rect = rect;
    push(cmd);
}

// ── 填充命令 ──────────────────────────────────────────────────────────────────

void Canvas::fill_rect(math::Rect rect, const Brush& brush) {
    DrawCmd cmd;
    cmd.kind  = DrawCmdKind::FillRect;
    cmd.rect  = rect;
    cmd.brush = brush;
    push(cmd);
}

void Canvas::fill_rounded_rect(math::RoundedRect rrect, const Brush& brush) {
    DrawCmd cmd;
    cmd.kind  = DrawCmdKind::FillRoundedRect;
    cmd.rrect = rrect;
    cmd.brush = brush;
    push(cmd);
}

void Canvas::fill_complex_rounded_rect(math::ComplexRoundedRect rrect, const Brush& brush) {
    DrawCmd cmd;
    cmd.kind         = DrawCmdKind::FillComplexRoundedRect;
    cmd.complex_rrect = rrect;
    cmd.brush        = brush;
    push(cmd);
}

void Canvas::fill_ellipse(math::Vec2 center, math::Vec2 radii, const Brush& brush) {
    // 以 pt_a 存中心，pt_b 存半径
    DrawCmd cmd;
    cmd.kind  = DrawCmdKind::FillEllipse;
    cmd.pt_a  = center;
    cmd.pt_b  = radii;
    cmd.brush = brush;
    push(cmd);
}

void Canvas::fill_path(const Path& path, const Brush& brush) {
    DrawCmd cmd;
    cmd.kind       = DrawCmdKind::FillPath;
    cmd.path_index = intern_path(path);
    cmd.brush      = brush;
    push(cmd);
}

// ── 描边命令 ──────────────────────────────────────────────────────────────────

void Canvas::stroke_rect(math::Rect rect, const Brush& brush, const Pen& pen) {
    DrawCmd cmd;
    cmd.kind  = DrawCmdKind::StrokeRect;
    cmd.rect  = rect;
    cmd.brush = brush;
    cmd.pen   = pen;
    push(cmd);
}

void Canvas::stroke_rounded_rect(math::RoundedRect rrect, const Brush& brush, const Pen& pen) {
    DrawCmd cmd;
    cmd.kind  = DrawCmdKind::StrokeRoundedRect;
    cmd.rrect = rrect;
    cmd.brush = brush;
    cmd.pen   = pen;
    push(cmd);
}

void Canvas::stroke_complex_rounded_rect(math::ComplexRoundedRect rrect, const Brush& brush, const Pen& pen) {
    DrawCmd cmd;
    cmd.kind          = DrawCmdKind::StrokeComplexRoundedRect;
    cmd.complex_rrect = rrect;
    cmd.brush         = brush;
    cmd.pen           = pen;
    push(cmd);
}

void Canvas::stroke_bordered_rect(math::Rect rect, const Brush& brush, math::Thickness widths) {
    DrawCmd cmd;
    cmd.kind          = DrawCmdKind::StrokeBorderedRect;
    cmd.rect          = rect;
    cmd.brush         = brush;
    cmd.border_widths = widths;
    push(cmd);
}

void Canvas::stroke_bordered_rounded_rect(math::Rect rect, const Brush& brush,
                                          math::Thickness widths, math::CornerRadii radii) {
    DrawCmd cmd;
    cmd.kind          = DrawCmdKind::StrokeBorderedRoundedRect;
    cmd.rect          = rect;
    cmd.brush         = brush;
    cmd.border_widths = widths;
    cmd.border_radii  = radii;
    push(cmd);
}

void Canvas::stroke_ellipse(math::Vec2 center, math::Vec2 radii, const Brush& brush, const Pen& pen) {
    DrawCmd cmd;
    cmd.kind  = DrawCmdKind::StrokeEllipse;
    cmd.pt_a  = center;
    cmd.pt_b  = radii;
    cmd.brush = brush;
    cmd.pen   = pen;
    push(cmd);
}

void Canvas::stroke_line(math::Vec2 from, math::Vec2 to, const Brush& brush, const Pen& pen) {
    DrawCmd cmd;
    cmd.kind  = DrawCmdKind::StrokeLine;
    cmd.pt_a  = from;
    cmd.pt_b  = to;
    cmd.brush = brush;
    cmd.pen   = pen;
    push(cmd);
}

void Canvas::stroke_path(const Path& path, const Brush& brush, const Pen& pen) {
    DrawCmd cmd;
    cmd.kind       = DrawCmdKind::StrokePath;
    cmd.path_index = intern_path(path);
    cmd.brush      = brush;
    cmd.pen        = pen;
    push(cmd);
}

// ── 内部辅助 ──────────────────────────────────────────────────────────────────

uint32_t Canvas::intern_path(const Path& path) {
    const uint32_t index = static_cast<uint32_t>(paths_.size());
    paths_.push_back(path);   // 拷贝路径数据（Path 持有 Vector<PathCmd>）
    return index;
}

// ── 完成/重置 ──────────────────────────────────────────────────────────────────

DisplayList Canvas::end() {
    // 将内部缓冲移走构造 DisplayList，自身恢复为空
    DisplayList dl(std::move(cmds_), std::move(paths_));
    cmds_  = {};
    paths_ = {};
    return dl;
}

void Canvas::discard() {
    cmds_  = {};
    paths_ = {};
}

} // namespace mine::paint
