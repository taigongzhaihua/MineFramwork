/**
 * @file PathBuilder.cpp
 * @brief PathBuilder 路径构造器实现。
 */

#include <mine/paint/PathBuilder.h>

#include <cmath>  // std::sin, std::cos, std::sqrt

namespace mine::paint {

// ── 贝塞尔圆近似常数 ────────────────────────────────────────────────────────
//
// 用 4 段三次贝塞尔曲线近似圆/椭圆时，控制点偏移系数 κ ≈ 0.5523：
//   κ = (4/3) * tan(π/8) ≈ 0.55228
// 误差 < 0.04%，适用于 UI 绘制。
//
static constexpr float kKappa = 0.55228f;

// ── 基础路径命令 ──────────────────────────────────────────────────────────────

PathBuilder& PathBuilder::move_to(math::Vec2 pt) {
    PathCmd cmd;
    cmd.kind  = PathCmdKind::MoveTo;
    cmd.pt[0] = pt;
    push(cmd);
    return *this;
}

PathBuilder& PathBuilder::line_to(math::Vec2 pt) {
    PathCmd cmd;
    cmd.kind  = PathCmdKind::LineTo;
    cmd.pt[0] = pt;
    push(cmd);
    return *this;
}

PathBuilder& PathBuilder::cubic_to(math::Vec2 c1, math::Vec2 c2, math::Vec2 end) {
    PathCmd cmd;
    cmd.kind  = PathCmdKind::CubicTo;
    cmd.pt[0] = c1;
    cmd.pt[1] = c2;
    cmd.pt[2] = end;
    push(cmd);
    return *this;
}

PathBuilder& PathBuilder::quad_to(math::Vec2 ctrl, math::Vec2 end) {
    PathCmd cmd;
    cmd.kind  = PathCmdKind::QuadTo;
    cmd.pt[0] = ctrl;
    cmd.pt[1] = end;
    push(cmd);
    return *this;
}

PathBuilder& PathBuilder::close() {
    PathCmd cmd;
    cmd.kind = PathCmdKind::Close;
    push(cmd);
    return *this;
}

// ── 便捷几何命令 ──────────────────────────────────────────────────────────────

PathBuilder& PathBuilder::add_rect(math::Rect rect) {
    // 顺时针：左上 → 右上 → 右下 → 左下 → 关闭
    move_to({rect.x,              rect.y});
    line_to({rect.x + rect.width, rect.y});
    line_to({rect.x + rect.width, rect.y + rect.height});
    line_to({rect.x,              rect.y + rect.height});
    close();
    return *this;
}

PathBuilder& PathBuilder::add_rounded_rect(math::RoundedRect rrect) {
    const float x  = rrect.rect.x;
    const float y  = rrect.rect.y;
    const float w  = rrect.rect.width;
    const float h  = rrect.rect.height;

    // RoundedRect 中的 radius_x/radius_y 已在构造时被限制在合法范围内
    const float rx = rrect.radius_x;
    const float ry = rrect.radius_y;
    const float kx = rx * kKappa;
    const float ky = ry * kKappa;

    // 顺时针绘制，从顶边左侧出发：
    //   顶边左端 → 顶右圆角 → 右边 → 底右圆角 → 底边 → 底左圆角 → 左边 → 顶左圆角
    move_to({x + rx, y});

    // 顶边 + 右上圆角
    line_to ({x + w - rx, y});
    cubic_to({x + w - rx + kx, y},
             {x + w,           y + ry - ky},
             {x + w,           y + ry});

    // 右边 + 右下圆角
    line_to ({x + w, y + h - ry});
    cubic_to({x + w,           y + h - ry + ky},
             {x + w - rx + kx, y + h},
             {x + w - rx,      y + h});

    // 底边 + 左下圆角
    line_to ({x + rx, y + h});
    cubic_to({x + rx - kx, y + h},
             {x,           y + h - ry + ky},
             {x,           y + h - ry});

    // 左边 + 左上圆角
    line_to ({x, y + ry});
    cubic_to({x,           y + ry - ky},
             {x + rx - kx, y},
             {x + rx,      y});

    close();
    return *this;
}

PathBuilder& PathBuilder::add_ellipse(math::Vec2 center, math::Vec2 radii) {
    const float cx = center.x;
    const float cy = center.y;
    const float rx = radii.x;
    const float ry = radii.y;
    const float kx = rx * kKappa;
    const float ky = ry * kKappa;

    // 从顶部（cx, cy - ry）顺时针，4 段三次贝塞尔曲线近似椭圆：
    move_to({cx, cy - ry});

    // 右上象限
    cubic_to({cx + kx, cy - ry},
             {cx + rx, cy - ky},
             {cx + rx, cy});

    // 右下象限
    cubic_to({cx + rx, cy + ky},
             {cx + kx, cy + ry},
             {cx,      cy + ry});

    // 左下象限
    cubic_to({cx - kx, cy + ry},
             {cx - rx, cy + ky},
             {cx - rx, cy});

    // 左上象限
    cubic_to({cx - rx, cy - ky},
             {cx - kx, cy - ry},
             {cx,      cy - ry});

    close();
    return *this;
}

PathBuilder& PathBuilder::add_complex_rounded_rect(math::ComplexRoundedRect rrect) {
    const float x = rrect.rect.x;
    const float y = rrect.rect.y;
    const float w = rrect.rect.width;
    const float h = rrect.rect.height;

    // 各角各自的椭圆半轴（已由 ComplexRoundedRect 构造器按 CSS 比例钳制）
    const float tl_rx = rrect.radii.top_left.x;
    const float tl_ry = rrect.radii.top_left.y;
    const float tr_rx = rrect.radii.top_right.x;
    const float tr_ry = rrect.radii.top_right.y;
    const float br_rx = rrect.radii.bottom_right.x;
    const float br_ry = rrect.radii.bottom_right.y;
    const float bl_rx = rrect.radii.bottom_left.x;
    const float bl_ry = rrect.radii.bottom_left.y;

    // 每角的贝塞尔控制点偏移量：κ * 半轴长
    const float tl_kx = tl_rx * kKappa;
    const float tl_ky = tl_ry * kKappa;
    const float tr_kx = tr_rx * kKappa;
    const float tr_ky = tr_ry * kKappa;
    const float br_kx = br_rx * kKappa;
    const float br_ky = br_ry * kKappa;
    const float bl_kx = bl_rx * kKappa;
    const float bl_ky = bl_ry * kKappa;

    // 顺时针绘制，从顶边左上角弧结束点出发：
    //   顶左弧末 → 顶边 → 右上弧 → 右边 → 右下弧 → 底边 → 左下弧 → 左边 → 左上弧 → 闭合
    move_to({x + tl_rx, y});

    // 顶边 + 右上圆角
    line_to ({x + w - tr_rx, y});
    cubic_to({x + w - tr_rx + tr_kx, y},
             {x + w,                  y + tr_ry - tr_ky},
             {x + w,                  y + tr_ry});

    // 右边 + 右下圆角
    line_to ({x + w, y + h - br_ry});
    cubic_to({x + w,                  y + h - br_ry + br_ky},
             {x + w - br_rx + br_kx,  y + h},
             {x + w - br_rx,          y + h});

    // 底边 + 左下圆角
    line_to ({x + bl_rx, y + h});
    cubic_to({x + bl_rx - bl_kx, y + h},
             {x,                  y + h - bl_ry + bl_ky},
             {x,                  y + h - bl_ry});

    // 左边 + 左上圆角
    line_to ({x, y + tl_ry});
    cubic_to({x,                  y + tl_ry - tl_ky},
             {x + tl_rx - tl_kx, y},
             {x + tl_rx,         y});

    close();
    return *this;
}

// ── 完成构建 ──────────────────────────────────────────────────────────────────

Path PathBuilder::build() {
    // 将命令序列移走，返回不可变路径；构造器恢复为空
    return Path(std::move(cmds_));
}

} // namespace mine::paint
