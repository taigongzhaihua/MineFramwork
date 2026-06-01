/**
 * @file Canvas.cpp
 * @brief Canvas 绘制上下文实现（录制模式）。
 */

#include <mine/paint/Canvas.h>

#include <cmath>     // std::cos, std::sin

namespace mine::paint {

// ── 状态管理 ──────────────────────────────────────────────────────────────────

void Canvas::save() {
    // 保存当前裁剪层深度（restore 时自动弹出超出部分）
    clip_depth_stack_.push_back(clip_depth_);
    DrawCmd cmd;
    cmd.kind = DrawCmdKind::TransformPush;
    push(cmd);
}

void Canvas::restore() {
    // 自动弹出自上次 save() 以来推入的所有裁剪层
    if (!clip_depth_stack_.empty()) {
        const int saved_depth = clip_depth_stack_.back();
        clip_depth_stack_.pop_back();
        while (clip_depth_ > saved_depth) {
            DrawCmd pop_cmd;
            pop_cmd.kind = DrawCmdKind::ClipPop;
            push(pop_cmd);
            --clip_depth_;
        }
    }
    DrawCmd cmd;
    cmd.kind = DrawCmdKind::TransformPop;
    push(cmd);
}

void Canvas::transform(const math::Transform2D& t) {
    DrawCmd cmd;
    cmd.kind      = DrawCmdKind::TransformSet;  // 仅级联当前变换，不压栈
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
    ++clip_depth_;
    DrawCmd cmd;
    cmd.kind = DrawCmdKind::ClipPushRect;
    cmd.rect = rect;
    push(cmd);
}

void Canvas::clip_rounded_rect(math::RoundedRect rrect) {
    ++clip_depth_;
    DrawCmd cmd;
    cmd.kind  = DrawCmdKind::ClipPushRoundedRect;
    cmd.rrect = rrect;
    push(cmd);
}

void Canvas::clip_complex_rounded_rect(math::ComplexRoundedRect rrect) {
    ++clip_depth_;
    DrawCmd cmd;
    cmd.kind          = DrawCmdKind::ClipPushComplexRoundedRect;
    cmd.complex_rrect = rrect;
    push(cmd);
}

void Canvas::clip_polygon(core::Span<const math::Vec2> vertices) {
    if (vertices.size() < 3) return;  // 少于 3 个顶点不构成多边形

    // 计算 AABB（供渲染器确定 SDF 包围盒）
    float min_x = vertices[0].x, min_y = vertices[0].y;
    float max_x = vertices[0].x, max_y = vertices[0].y;
    for (size_t i = 1; i < vertices.size(); ++i) {
        if (vertices[i].x < min_x) min_x = vertices[i].x;
        if (vertices[i].y < min_y) min_y = vertices[i].y;
        if (vertices[i].x > max_x) max_x = vertices[i].x;
        if (vertices[i].y > max_y) max_y = vertices[i].y;
    }

    // 将多边形顶点存为 Path（MoveTo + LineTo × N-1 + Close）
    PathBuilder pb;
    pb.move_to(vertices[0]);
    for (size_t i = 1; i < vertices.size(); ++i) {
        pb.line_to(vertices[i]);
    }
    pb.close();

    ++clip_depth_;
    DrawCmd cmd;
    cmd.kind       = DrawCmdKind::ClipPushPolygon;
    cmd.pt_a       = {(min_x + max_x) * 0.5f, (min_y + max_y) * 0.5f};  // AABB 中心
    cmd.pt_b       = {(max_x - min_x) * 0.5f, (max_y - min_y) * 0.5f};  // AABB 半尺寸
    cmd.path_index = intern_path(pb.build());
    push(cmd);
}

void Canvas::clip_path(const Path& path) {
    // 路径由渲染器内部扁平化，Canvas 层只需存储原始路径引用
    ++clip_depth_;
    DrawCmd cmd;
    cmd.kind       = DrawCmdKind::ClipPushPath;
    cmd.path_index = intern_path(path);
    push(cmd);
}

void Canvas::clip_pop() {
    if (clip_depth_ > 0) --clip_depth_;
    DrawCmd cmd;
    cmd.kind = DrawCmdKind::ClipPop;
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

void Canvas::fill_polygon(core::Span<const math::Vec2> vertices, const Brush& brush) {
    if (vertices.size() < 3) return;  // 少于3个顶点不构成多边形

    // 计算顶点 AABB，用于确定 SDF 渲染包围盒中心和半尺寸
    float min_x = vertices[0].x, min_y = vertices[0].y;
    float max_x = vertices[0].x, max_y = vertices[0].y;
    for (size_t i = 1; i < vertices.size(); ++i) {
        if (vertices[i].x < min_x) min_x = vertices[i].x;
        if (vertices[i].y < min_y) min_y = vertices[i].y;
        if (vertices[i].x > max_x) max_x = vertices[i].x;
        if (vertices[i].y > max_y) max_y = vertices[i].y;
    }

    // 将多边形顶点存为 Path（MoveTo + LineTo × N-1 + Close）
    PathBuilder pb;
    pb.move_to(vertices[0]);
    for (size_t i = 1; i < vertices.size(); ++i) {
        pb.line_to(vertices[i]);
    }
    pb.close();

    DrawCmd cmd;
    cmd.kind       = DrawCmdKind::FillPolygon;
    cmd.pt_a       = {(min_x + max_x) * 0.5f, (min_y + max_y) * 0.5f};  // AABB 中心
    cmd.pt_b       = {(max_x - min_x) * 0.5f, (max_y - min_y) * 0.5f};  // AABB 半尺寸
    cmd.path_index = intern_path(pb.build());
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

void Canvas::stroke_arc(math::Vec2 center, float radius,
                        float start_angle, float sweep_angle,
                        const Brush& brush, const Pen& pen) {
    // pt_a = 圆心，pt_b.x = 半径，pt_b.y = 起始角，pt_c.x = 扫掠角
    DrawCmd cmd;
    cmd.kind    = DrawCmdKind::StrokeArc;
    cmd.pt_a    = center;
    cmd.pt_b    = {radius, start_angle};
    cmd.pt_c    = {sweep_angle, 0.0f};
    cmd.brush   = brush;
    cmd.pen     = pen;
    push(cmd);
}

void Canvas::stroke_quad_bezier(math::Vec2 p0, math::Vec2 p1, math::Vec2 p2,
                                const Brush& brush, const Pen& pen) {
    // pt_a = P0（起点），pt_b = P1（控制点），pt_c = P2（终点）
    DrawCmd cmd;
    cmd.kind  = DrawCmdKind::StrokeQuadBezier;
    cmd.pt_a  = p0;
    cmd.pt_b  = p1;
    cmd.pt_c  = p2;
    cmd.brush = brush;
    cmd.pen   = pen;
    push(cmd);
}

void Canvas::stroke_cubic_bezier(math::Vec2 p0, math::Vec2 p1, math::Vec2 p2, math::Vec2 p3,
                                 const Brush& brush, const Pen& pen) {
    // pt_a = P0（起点），pt_b = P1（第一控制点），pt_c = P2（第二控制点），pt_d = P3（终点）
    DrawCmd cmd;
    cmd.kind  = DrawCmdKind::StrokeCubicBezier;
    cmd.pt_a  = p0;
    cmd.pt_b  = p1;
    cmd.pt_c  = p2;
    cmd.pt_d  = p3;
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

void Canvas::stroke_polygon(core::Span<const math::Vec2> vertices, const Brush& brush, const Pen& pen) {
    if (vertices.size() < 3) return;  // 少于3个顶点不构成多边形

    // 计算顶点 AABB，包围盒外扩 stroke_w/2 以容纳描边区域
    const float half_sw = pen.width * 0.5f;
    float min_x = vertices[0].x - half_sw, min_y = vertices[0].y - half_sw;
    float max_x = vertices[0].x + half_sw, max_y = vertices[0].y + half_sw;
    for (size_t i = 1; i < vertices.size(); ++i) {
        const float lx = vertices[i].x - half_sw;
        const float ly = vertices[i].y - half_sw;
        const float rx = vertices[i].x + half_sw;
        const float ry = vertices[i].y + half_sw;
        if (lx < min_x) min_x = lx;
        if (ly < min_y) min_y = ly;
        if (rx > max_x) max_x = rx;
        if (ry > max_y) max_y = ry;
    }

    // 将多边形顶点存为 Path（MoveTo + LineTo × N-1 + Close）
    PathBuilder pb;
    pb.move_to(vertices[0]);
    for (size_t i = 1; i < vertices.size(); ++i) {
        pb.line_to(vertices[i]);
    }
    pb.close();

    DrawCmd cmd;
    cmd.kind       = DrawCmdKind::StrokePolygon;
    cmd.pt_a       = {(min_x + max_x) * 0.5f, (min_y + max_y) * 0.5f};  // AABB 中心
    cmd.pt_b       = {(max_x - min_x) * 0.5f, (max_y - min_y) * 0.5f};  // AABB 半尺寸（含描边外扩）
    cmd.path_index = intern_path(pb.build());
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

uint32_t Canvas::intern_text_run(TextRun run) {
    const uint32_t index = static_cast<uint32_t>(text_runs_.size());
    text_runs_.push_back(std::move(run));
    return index;
}

// ── 文字绘制 ──────────────────────────────────────────────────────────────────

void Canvas::draw_text(
    core::StringView text,
    math::Vec2       origin,
    void*            font_face,
    float            size_px,
    const Brush&     brush,
    float            character_spacing) {

    if (font_face == nullptr || text.empty() || size_px <= 0.0f) {
        return;
    }

    // 构造 TextRun，将文字内容拷贝到 inline 缓冲
    TextRun run;
    const uint32_t raw_len = static_cast<uint32_t>(text.size());
    // 超出缓冲则截断（保留末尾 '\0'）
    const uint32_t copy_len = raw_len < TextRun::kMaxUtf8Bytes
        ? raw_len
        : (TextRun::kMaxUtf8Bytes - 1u);

    for (uint32_t i = 0; i < copy_len; ++i) {
        run.utf8[i] = text[i];
    }
    run.utf8[copy_len] = '\0';
    run.length            = copy_len;
    run.font_face         = font_face;
    run.size_px           = size_px;
    run.origin_x          = origin.x;
    run.origin_y          = origin.y;
    run.character_spacing = character_spacing;

    const uint32_t run_index = intern_text_run(std::move(run));

    // 生成 DrawText 命令，path_index 复用为 text_run 索引
    DrawCmd cmd;
    cmd.kind       = DrawCmdKind::DrawText;
    cmd.path_index = run_index;
    cmd.brush      = brush;
    push(cmd);
}

// ── 完成/重置 ──────────────────────────────────────────────────────────────────

DisplayList Canvas::end() {
    // 将内部缓冲移走构造 DisplayList，自身恢复为空
    DisplayList dl(std::move(cmds_), std::move(paths_), std::move(text_runs_));
    cmds_       = {};
    paths_      = {};
    text_runs_  = {};
    return dl;
}

void Canvas::discard() {
    cmds_       = {};
    paths_      = {};
    text_runs_  = {};
}

} // namespace mine::paint
