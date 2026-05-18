/**
 * @file DrawCmd.h
 * @brief 单条绘制命令结构体定义。
 *
 * DrawCmd 是一个带 kind 标签的扁平结构体，Canvas 将绘制操作录制为
 * DrawCmd 序列后存入 DisplayList，由 IRenderer 实际执行渲染。
 *
 * 设计原则：
 *   - DrawCmd 是 trivially movable 的值类型（无虚函数、无动态分配）。
 *   - 路径（Path）由 DisplayList 统一持有，DrawCmd 通过 path_index 引用。
 *   - 尽量保持结构体小巧，复杂几何数据用 path_index 间接访问。
 *
 * 依赖：mine.paint（Brush、Pen），mine.math（Rect、RoundedRect、Vec2、Transform2D）
 */

#pragma once

#include <cstdint>

#include <mine/paint/Brush.h>
#include <mine/paint/Pen.h>
#include <mine/paint/BorderWidths.h>
#include <mine/math/Rect.h>
#include <mine/math/RoundedRect.h>
#include <mine/math/ComplexRoundedRect.h>
#include <mine/math/CornerRadii.h>
#include <mine/math/Vec2.h>
#include <mine/math/Transform2D.h>

namespace mine::paint {

/**
 * @brief 绘制命令类型枚举。
 */
enum class DrawCmdKind : uint8_t {
    // ── 填充命令 ────────────────────────────────────────────────────────
    FillRect,                ///< 填充矩形
    FillRoundedRect,         ///< 填充圆角矩形（uniform radius_x/radius_y）
    FillComplexRoundedRect,  ///< 填充四角各自独立的椭圆圆角矩形
    FillEllipse,             ///< 填充椭圆
    FillPath,                ///< 填充任意路径（通过 path_index 引用）

    // ── 描边命令 ────────────────────────────────────────────────────────
    StrokeRect,              ///< 描边矩形（均匀描边，Pen.width 决定线宽）
    StrokeRoundedRect,       ///< 描边圆角矩形
    StrokeComplexRoundedRect,///< 描边四角各自独立的椭圆圆角矩形
    StrokeEllipse,           ///< 描边椭圆
    StrokeBorderedRect,      ///< 四边各自独立宽度的矩形内侧描边（类 CSS border）
    StrokeBorderedRoundedRect, ///< 四边各自独立宽度 + 四角各自独立圆角的内侧描边
    StrokeLine,        ///< 描边线段（from → to）
    StrokePath,        ///< 描边任意路径（通过 path_index 引用）

    // ── 状态命令 ────────────────────────────────────────────────────────
    ClipPushRect,      ///< 压入矩形裁剪区域（与当前裁剪区域取交集）
    ClipPop,           ///< 弹出最近一次裁剪状态
    TransformPush,     ///< 压入变换矩阵（与当前变换级联）
    TransformPop,      ///< 弹出最近一次变换状态
};

/**
 * @brief 单条绘制命令。
 *
 * 扁平结构体，根据 kind 字段激活对应的字段组（如 rect/rrect/pt_a 等）。
 * 未使用的字段保留为零初始化，不影响渲染。
 *
 * 字段分组对照表：
 *
 *  kind               | 几何          | brush | pen | path_index
 *  -------------------|---------------|-------|-----|------------
 *  FillRect           | rect          | ✓     |     |
 *  FillRoundedRect    | rrect         | ✓     |     |
 *  FillEllipse        | pt_a(中心)    | ✓     |     |
 *                     | pt_b(半径)    |       |     |
 *  FillPath           |               | ✓     |     | ✓
 *  StrokeRect         | rect          | ✓     | ✓   |
 *  StrokeRoundedRect  | rrect         | ✓     | ✓   |
 *  StrokeEllipse      | pt_a(中心)    | ✓     | ✓   |
 *                     | pt_b(半径)    |       |     |
 *  StrokeBorderedRect | rect          | ✓     |     |  pt_a={top,right}
 *                     | border_widths |       |     |  pt_b={bottom,left}
 *  StrokeBorderedRoundedRect | rect   | ✓     |     |
 *                     | border_widths |       |     |
 *                     | border_radii  |       |     |
 *  StrokeLine         | pt_a(起点)    | ✓     | ✓   |
 *                     | pt_b(终点)    |       |     |
 *  StrokePath         |               | ✓     | ✓   | ✓
 *  ClipPushRect       | rect          |       |     |
 *  ClipPop            |               |       |     |
 *  TransformPush      | transform     |       |     |
 *  TransformPop       |               |       |     |
 */
struct DrawCmd {
    DrawCmdKind kind{DrawCmdKind::FillRect};

    // ── 几何数据 ────────────────────────────────────────────────────────
    math::Rect               rect{};          ///< 矩形（FillRect / StrokeRect / ClipPushRect）
    math::RoundedRect        rrect{};         ///< 圆角矩形（FillRoundedRect / StrokeRoundedRect）
    math::ComplexRoundedRect complex_rrect{}; ///< 独立四角圆角矩形（FillComplexRoundedRect / StrokeComplexRoundedRect）
    math::Vec2        pt_a{};          ///< 通用向量 A（中心、起点等）
    math::Vec2        pt_b{};          ///< 通用向量 B（半径、终点等）
    uint32_t          path_index{0};   ///< 路径索引（FillPath / StrokePath）
    math::Transform2D transform{};     ///< 变换矩阵（TransformPush）

    // ── 绘制属性 ────────────────────────────────────────────────────────
    Brush        brush{};          ///< 填充/描边画刷
    Pen          pen{};            ///< 描边样式（描边命令使用）
    BorderWidths border_widths{};  ///< 四边独立描边宽度（StrokeBorderedRect / StrokeBorderedRoundedRect）
    math::CornerRadii border_radii{}; ///< 四角独立圆角半径（StrokeBorderedRoundedRect 使用）
};

} // namespace mine::paint
