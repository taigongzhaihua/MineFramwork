/**
 * @file main.cpp
 * @brief 00-hello-rect 示例：mine.paint Canvas SDF 填充 + 描边命令综合演示。
 *
 * 演示内容（2×9 网格布局，左列填充/右列描边）：
 *   行1 左：FillRect                      — 纯色填充矩形（绿色）
 *   行1 右：StrokeRect                    — 矩形描边（绿色，线宽 4px）
 *   行2 左：FillRoundedRect               — 均匀圆角矩形填充（蓝色，radius=20px）
 *   行2 右：StrokeRoundedRect             — 均匀圆角矩形描边（蓝色，线宽 4px）
 *   行3 左：FillComplexRoundedRect        — 四角独立圆角矩形填充（橙色）
 *   行3 右：StrokeComplexRoundedRect      — 四角独立圆角矩形描边（橙色，线宽 4px）
 *   行4 左：FillEllipse                   — 満源填充（紫色）
 *   行4 右：StrokeEllipse                 — 満源描边（紫色，线宽 4px）
 *   行5 左：StrokeBorderedRoundedRect     — 四边不等宽 + 四角独立圆角内侧描边（红色）
 *   行5 右：StrokeBorderedRoundedRect     — 均匀圆角 + 仅上下边框（青色，上下员4 12px）
 *   行6 左：StrokeLine (Flat cap)          — 平端线段（双色渐变 SDF）
 *   行6 右：StrokeLine (Round + Square)    — 混合端点样式线段对比
 *   行7 左：StrokeArc (Flat cap)           — 四分之一圆弧，平端截断
 *   行7 右：StrokeArc (Round cap)          — 半圆弧，圆形端点
 *   行8 左：StrokeQuadBezier (Flat cap)    — 二次贝塞尔抛物线，平端截断
 *   行8 右：StrokeQuadBezier (Round cap)   — 二次贝塞尔 S 形曲线，圆形端点
 *   行9 左：StrokeCubicBezier (Flat cap)   — 水平 S/反 S 对照曲线（蓝色4px + 橙色2px，同起同终）
 *   行9 右：StrokeCubicBezier (Round cap)  — 三次贝塞尔大 S 形，圆形端点
 *   行10左：FillPolygon（凸多边形）         — SDF 填充正六边形（蓝色，IQ 绕数法）
 *   行10右：FillPolygon（凹多边形）         — SDF 填充五角星 + StrokePolygon 六边形描边
 */

#include <mine/platform/PlatformAbi.h>
#include <mine/gfx/Gfx.h>
#include <mine/paint/Paint.h>

#include <cstdio>
#include <cmath>

// ── 渲染器 ───────────────────────────────────────────────────────────────────

/**
 * @brief Hello-Rect 渲染器，封装 RHI 设备 + Paint 渲染器 + 交换链。
 */
struct HelloRectRenderer : public mine::platform::IWindowEventSink {
    mine::core::OwnedPtr<mine::gfx::IDevice>     device;
    mine::core::OwnedPtr<mine::gfx::IQueue>       queue;
    mine::core::OwnedPtr<mine::gfx::ISwapchain>   swapchain;
    mine::core::OwnedPtr<mine::gfx::ICommandList> cmd_list;   ///< 用于背景清除
    mine::core::OwnedPtr<mine::paint::IRenderer>  paint_renderer;

    /// 物理像素 / 逻辑像素缩放因子（由 IWindow::dpi() 初始化，DpiChanged 时更新）
    float dpi_scale_{1.0f};

    // IWindowEventSink：响应窗口尺寸变化和 DPI 变化事件
    void on_window_event(mine::platform::WindowEvent& event) override {
        if (event.kind == mine::platform::WindowEventKind::DpiChanged) {
            // 更新 DPI 缩放因子并同步通知渲染器
            const float old_scale = dpi_scale_;
            dpi_scale_ = event.new_dpi / 96.0f;
            paint_renderer->set_dpi_scale(dpi_scale_);

            // 当前交换链是物理大小，换算出逻辑大小后以新 scale 重新计算物理大小
            const float logical_w = static_cast<float>(swapchain->width())  / old_scale;
            const float logical_h = static_cast<float>(swapchain->height()) / old_scale;
            const auto  new_phys_w = static_cast<uint32_t>(std::max(1.0f, logical_w * dpi_scale_));
            const auto  new_phys_h = static_cast<uint32_t>(std::max(1.0f, logical_h * dpi_scale_));

            queue->wait_idle();
            swapchain->resize(new_phys_w, new_phys_h);
            render();
        }
        else if (event.kind == mine::platform::WindowEventKind::Resized) {
            // new_size 是逻辑像素，× dpi_scale_ 得物理像素大小
            const auto w = static_cast<uint32_t>(std::max(1.0f, event.new_size.width  * dpi_scale_));
            const auto h = static_cast<uint32_t>(std::max(1.0f, event.new_size.height * dpi_scale_));

            // 等待 GPU 空闲后再 resize 交换链
            queue->wait_idle();
            swapchain->resize(w, h);

            // resize 后立即重新渲染一帧
            render();
        }
    }

    /**
     * @brief 渲染一帧：清屏 + 2×8 网格展示填充、描边、线段、圆弧、贝塞尔命令 + 呈现。
     */
    void render() {
        if (!device || !swapchain || !paint_renderer) return;

        mine::gfx::ITexture* back_buf = swapchain->current_render_target();
        if (!back_buf) return;

        const float W = static_cast<float>(swapchain->width());
        const float H = static_cast<float>(swapchain->height());

        // ── 1. 构建绘制命令列表 ────────────────────────────────────────────

        mine::paint::Canvas canvas;

        // 背景：深灰色覆盖整个视口
        canvas.fill_rect(
            mine::math::Rect{0.0f, 0.0f, W, H},
            mine::paint::Brush::solid(mine::math::Color{0.12f, 0.12f, 0.12f, 1.0f}));

        // 2×10 网格参数（2列 × 10行）
        const float outer_pad = 20.0f;
        const float gap       = 10.0f;
        const float cw        = (W - outer_pad * 2.0f - gap) * 0.5f;
        const float ch        = (H - outer_pad * 2.0f - gap * 9.0f) / 10.0f;

        const float col0 = outer_pad;
        const float col1 = outer_pad + cw + gap;

        const float row0 = outer_pad;
        const float row1 = outer_pad + ch + gap;
        const float row2 = outer_pad + (ch + gap) * 2.0f;
        const float row3 = outer_pad + (ch + gap) * 3.0f;
        const float row4 = outer_pad + (ch + gap) * 4.0f;
        const float row5 = outer_pad + (ch + gap) * 5.0f;
        const float row6 = outer_pad + (ch + gap) * 6.0f;
        const float row7 = outer_pad + (ch + gap) * 7.0f;
        const float row8 = outer_pad + (ch + gap) * 8.0f;
        const float row9 = outer_pad + (ch + gap) * 9.0f;

        const float ip = 14.0f;
        const float sw = cw - ip * 2.0f;
        const float sh = ch - ip * 2.0f;

        // 颜色定义
        const mine::math::Color col_green  {0.20f, 0.75f, 0.35f, 1.0f};
        const mine::math::Color col_blue   {0.20f, 0.45f, 0.90f, 1.0f};
        const mine::math::Color col_orange {0.95f, 0.55f, 0.10f, 1.0f};
        const mine::math::Color col_purple {0.65f, 0.25f, 0.90f, 1.0f};
        const mine::math::Color col_red    {0.90f, 0.20f, 0.20f, 1.0f};
        const mine::math::Color col_cyan   {0.15f, 0.80f, 0.85f, 1.0f};

        const mine::paint::Pen pen{.width = 2.0f, .miter_limit = 4.0f};

        // ── 行1 左：FillRect ────────────────────────────────────────────
        canvas.fill_rect(
            mine::math::Rect{col0 + ip, row0 + ip, sw, sh},
            mine::paint::Brush::solid(col_green));

        // ── 行1 右：StrokeRect ──────────────────────────────────────────
        canvas.stroke_rect(
            mine::math::Rect{col1 + ip, row0 + ip, sw, sh},
            mine::paint::Brush::solid(col_green), pen);

        // ── 行2 左：FillRoundedRect ─────────────────────────────────────
        canvas.fill_rounded_rect(
            mine::math::RoundedRect{mine::math::Rect{col0 + ip, row1 + ip, sw, sh}, 20.0f},
            mine::paint::Brush::solid(col_blue));

        // ── 行2 右：StrokeRoundedRect ───────────────────────────────────
        canvas.stroke_rounded_rect(
            mine::math::RoundedRect{mine::math::Rect{col1 + ip, row1 + ip, sw, sh}, 20.0f},
            mine::paint::Brush::solid(col_blue), pen);

        // ── 行3 左：FillComplexRoundedRect ─────────────────────────────
        const mine::math::CornerRadii radii{
            {36.0f, 36.0f},  // 左上角
            {8.0f,  8.0f},   // 右上角
            {36.0f, 36.0f},  // 右下角
            {8.0f,  8.0f}    // 左下角
        };
        canvas.fill_complex_rounded_rect(
            mine::math::ComplexRoundedRect{mine::math::Rect{col0 + ip, row2 + ip, sw, sh}, radii},
            mine::paint::Brush::solid(col_orange));

        // ── 行3 右：StrokeComplexRoundedRect ───────────────────────────
        canvas.stroke_complex_rounded_rect(
            mine::math::ComplexRoundedRect{mine::math::Rect{col1 + ip, row2 + ip, sw, sh}, radii},
            mine::paint::Brush::solid(col_orange), pen);

        // ── 行4 左：FillEllipse ─────────────────────────────────────────
        canvas.fill_ellipse(
            mine::math::Vec2{col0 + ip + sw * 0.5f, row3 + ip + sh * 0.5f},
            mine::math::Vec2{sw * 0.5f, sh * 0.5f},
            mine::paint::Brush::solid(col_purple));

        // ── 行4 右：StrokeEllipse ───────────────────────────────────────
        canvas.stroke_ellipse(
            mine::math::Vec2{col1 + ip + sw * 0.5f, row3 + ip + sh * 0.5f},
            mine::math::Vec2{sw * 0.5f, sh * 0.5f},
            mine::paint::Brush::solid(col_purple), pen);

        // ── 行5 左：StrokeBorderedRoundedRect（四边不等宽 + 四角独立圆角）──
        canvas.stroke_bordered_rounded_rect(
            mine::math::Rect{col0 + ip, row4 + ip, sw, sh},
            mine::paint::Brush::solid(col_red),
            mine::math::Thickness{8.0f, 4.0f, 2.0f, 16.0f},  // left=8, top=4, right=2, bottom=16
            mine::math::CornerRadii{
                {30.0f, 30.0f},  // 左上角
                {10.0f, 10.0f},  // 右上角
                {20.0f, 20.0f},  // 右下角
                {0.0f,  0.0f}    // 左下角（直角）
            });

        // ── 行5 右：StrokeBorderedRoundedRect（均匀圆角 + 仅上下边框各12px）──
        canvas.stroke_bordered_rounded_rect(
            mine::math::Rect{col1 + ip, row4 + ip, sw, sh},
            mine::paint::Brush::solid(col_cyan),
            mine::math::Thickness::symmetric(0.0f, 12.0f),  // 仅上下各 12px（symmetric: horizontal=0, vertical=12）
            mine::math::CornerRadii::uniform(16.0f));

        // ── 行6 左：StrokeLine（Flat cap，斜线，SDF 抗锯齿）──────────────
        // 展示 SDF 线段的平端截断，线条边缘无锯齿
        {
            const float x0 = col0 + ip;
            const float y0 = row5 + ip;
            // 第一条：水平线（Flat，粗）
            mine::paint::Pen pen_flat;
            pen_flat.width     = 6.0f;
            pen_flat.start_cap = mine::paint::LineCap::Flat;
            pen_flat.end_cap   = mine::paint::LineCap::Flat;
            canvas.stroke_line(
                mine::math::Vec2{x0, y0 + sh * 0.25f},
                mine::math::Vec2{x0 + sw, y0 + sh * 0.25f},
                mine::paint::Brush::solid(col_green), pen_flat);
            // 第二条：斜线（Flat，细）
            mine::paint::Pen pen_flat_thin;
            pen_flat_thin.width     = 2.0f;
            pen_flat_thin.start_cap = mine::paint::LineCap::Flat;
            pen_flat_thin.end_cap   = mine::paint::LineCap::Flat;
            canvas.stroke_line(
                mine::math::Vec2{x0, y0 + sh * 0.5f},
                mine::math::Vec2{x0 + sw, y0 + sh * 0.75f},
                mine::paint::Brush::solid(col_orange), pen_flat_thin);
        }

        // ── 行6 右：StrokeLine（Round/Square 混合端点对比）──────────────────
        // 上：Round cap（两端圆形）；下：Square cap（两端方形延伸）
        {
            const float x1 = col1 + ip;
            const float y1 = row5 + ip;
            // Round cap
            mine::paint::Pen pen_round;
            pen_round.width     = 8.0f;
            pen_round.start_cap = mine::paint::LineCap::Round;
            pen_round.end_cap   = mine::paint::LineCap::Round;
            canvas.stroke_line(
                mine::math::Vec2{x1, y1 + sh * 0.3f},
                mine::math::Vec2{x1 + sw, y1 + sh * 0.3f},
                mine::paint::Brush::solid(col_blue), pen_round);
            // Square cap
            mine::paint::Pen pen_square;
            pen_square.width     = 8.0f;
            pen_square.start_cap = mine::paint::LineCap::Square;
            pen_square.end_cap   = mine::paint::LineCap::Square;
            canvas.stroke_line(
                mine::math::Vec2{x1, y1 + sh * 0.7f},
                mine::math::Vec2{x1 + sw, y1 + sh * 0.7f},
                mine::paint::Brush::solid(col_purple), pen_square);
        }

        // ── 行7 左：StrokeArc（Flat cap，四分之一圆弧）──────────────────
        // 圆心位于 Quad 区域左下，90° 弧（从 0 到 π/2，顺时针 = 向下方向）
        {
            const float cx7 = col0 + ip + sw * 0.5f;
            const float cy7 = row6 + ip + sh * 0.5f;
            const float r7  = std::min(sw, sh) * 0.35f;  // 圆弧半径

            mine::paint::Pen pen_arc_flat;
            pen_arc_flat.width     = 5.0f;
            pen_arc_flat.start_cap = mine::paint::LineCap::Flat;
            pen_arc_flat.end_cap   = mine::paint::LineCap::Flat;

            constexpr float k_pi = 3.14159265358979323846f;
            // 四分之一圆弧：0° → 90°（顺时针，屏幕坐标 Y 向下）
            canvas.stroke_arc(
                mine::math::Vec2{cx7, cy7},
                r7, 0.0f, k_pi * 0.5f,
                mine::paint::Brush::solid(col_green), pen_arc_flat);

            // 再画一个更大的扇形（180° - 360°，即下半圆，展示 Flat cap 截断）
            mine::paint::Pen pen_arc_flat2;
            pen_arc_flat2.width     = 3.0f;
            pen_arc_flat2.start_cap = mine::paint::LineCap::Flat;
            pen_arc_flat2.end_cap   = mine::paint::LineCap::Flat;
            canvas.stroke_arc(
                mine::math::Vec2{cx7, cy7},
                r7 * 1.5f, k_pi, k_pi,
                mine::paint::Brush::solid(col_orange), pen_arc_flat2);
        }

        // ── 行7 右：StrokeArc（Round cap，半圆弧）─────────────────────────
        // 180° 半圆弧，两端圆形，展示 Round cap 的自然过渡
        {
            const float cx7r = col1 + ip + sw * 0.5f;
            const float cy7r = row6 + ip + sh * 0.5f;
            const float r7r  = std::min(sw, sh) * 0.35f;

            mine::paint::Pen pen_arc_round;
            pen_arc_round.width     = 6.0f;
            pen_arc_round.start_cap = mine::paint::LineCap::Round;
            pen_arc_round.end_cap   = mine::paint::LineCap::Round;

            constexpr float k_pi = 3.14159265358979323846f;
            // 上半圆弧：从 -180° 到 0°（逆时针扫 180°）
            canvas.stroke_arc(
                mine::math::Vec2{cx7r, cy7r},
                r7r, -k_pi, k_pi,
                mine::paint::Brush::solid(col_blue), pen_arc_round);

            // 小圆弧作为点缀（270° 弧，Round cap）
            mine::paint::Pen pen_arc_small;
            pen_arc_small.width     = 2.5f;
            pen_arc_small.start_cap = mine::paint::LineCap::Round;
            pen_arc_small.end_cap   = mine::paint::LineCap::Round;
            canvas.stroke_arc(
                mine::math::Vec2{cx7r, cy7r},
                r7r * 1.6f, k_pi * 0.5f, k_pi * 1.5f,
                mine::paint::Brush::solid(col_cyan), pen_arc_small);
        }

        // ── 行8 左：StrokeQuadBezier（Flat cap，抛物线弧）──────────────────
        // P0→P1（控制点）→P2 形成典型弧线，Flat cap 截断两端
        {
            const float bx  = col0 + ip;
            const float by  = row7 + ip;
            // 单条抛物线贝塞尔（Flat cap，红色）
            mine::paint::Pen pen_bez_flat;
            pen_bez_flat.width     = 4.0f;
            pen_bez_flat.start_cap = mine::paint::LineCap::Flat;
            pen_bez_flat.end_cap   = mine::paint::LineCap::Flat;
            canvas.stroke_quad_bezier(
                mine::math::Vec2{bx, by + sh * 0.8f},          // P0 左下
                mine::math::Vec2{bx + sw * 0.5f, by},          // P1 顶部控制点
                mine::math::Vec2{bx + sw, by + sh * 0.8f},     // P2 右下
                mine::paint::Brush::solid(col_red), pen_bez_flat);

            // 倒置抛物线（细线，Flat cap，橙色，展示方向对称）
            mine::paint::Pen pen_bez_flat2;
            pen_bez_flat2.width     = 2.0f;
            pen_bez_flat2.start_cap = mine::paint::LineCap::Flat;
            pen_bez_flat2.end_cap   = mine::paint::LineCap::Flat;
            canvas.stroke_quad_bezier(
                mine::math::Vec2{bx, by + sh * 0.2f},
                mine::math::Vec2{bx + sw * 0.5f, by + sh},
                mine::math::Vec2{bx + sw, by + sh * 0.2f},
                mine::paint::Brush::solid(col_orange), pen_bez_flat2);
        }

        // ── 行8 右：StrokeQuadBezier（Round cap，S 形曲线组合）─────────────
        // 两段贝塞尔组合成 S 形，展示 Round cap 的自然衔接
        {
            const float bx  = col1 + ip;
            const float by  = row7 + ip;

            mine::paint::Pen pen_bez_round;
            pen_bez_round.width     = 5.0f;
            pen_bez_round.start_cap = mine::paint::LineCap::Round;
            pen_bez_round.end_cap   = mine::paint::LineCap::Round;

            // 上半段：左上 → 中 → 右中（圆形端点）
            canvas.stroke_quad_bezier(
                mine::math::Vec2{bx, by + sh * 0.1f},           // 左上
                mine::math::Vec2{bx + sw * 0.7f, by + sh * 0.1f},  // 控制点（右上）
                mine::math::Vec2{bx + sw * 0.5f, by + sh * 0.5f},  // 中间
                mine::paint::Brush::solid(col_purple), pen_bez_round);

            // 下半段：中 → 左下 → 右下（圆形端点）
            canvas.stroke_quad_bezier(
                mine::math::Vec2{bx + sw * 0.5f, by + sh * 0.5f},  // 中间
                mine::math::Vec2{bx + sw * 0.3f, by + sh * 0.9f},  // 控制点（左下）
                mine::math::Vec2{bx + sw, by + sh * 0.9f},          // 右下
                mine::paint::Brush::solid(col_cyan), pen_bez_round);
        }

        // ── 行9 左：StrokeCubicBezier（Flat cap，S 形曲线）──────────────────
        // 两条水平 S/反 S 对照曲线（同起同终，弯曲方向相反），展示三次贝塞尔双曲率特性
        {
            const float bx = col0 + ip;
            const float by = row8 + ip;

            mine::paint::Pen pen_cub_flat;
            pen_cub_flat.width     = 4.0f;
            pen_cub_flat.start_cap = mine::paint::LineCap::Flat;
            pen_cub_flat.end_cap   = mine::paint::LineCap::Flat;

            // 主 S 形（Flat cap，蓝色）：P0 左中 → P1 中下（下拉） → P2 中上（上拉） → P3 右中
            // 从左中出发，先向下弯后向上弯，形成水平 S 形
            canvas.stroke_cubic_bezier(
                mine::math::Vec2{bx,             by + sh * 0.5f},  // P0 左中
                mine::math::Vec2{bx + sw * 0.5f, by + sh},          // P1 中下（向下拉）
                mine::math::Vec2{bx + sw * 0.5f, by},               // P2 中上（向上拉）
                mine::math::Vec2{bx + sw,        by + sh * 0.5f},  // P3 右中
                mine::paint::Brush::solid(col_blue), pen_cub_flat);

            // 反 S 形（Flat cap，橙色）：与蓝色 S 形对称，先向上弯后向下弯
            mine::paint::Pen pen_cub_flat2;
            pen_cub_flat2.width     = 2.0f;
            pen_cub_flat2.start_cap = mine::paint::LineCap::Flat;
            pen_cub_flat2.end_cap   = mine::paint::LineCap::Flat;
            canvas.stroke_cubic_bezier(
                mine::math::Vec2{bx,             by + sh * 0.5f},  // P0 左中
                mine::math::Vec2{bx + sw * 0.5f, by},               // P1 中上（向上拉）
                mine::math::Vec2{bx + sw * 0.5f, by + sh},          // P2 中下（向下拉）
                mine::math::Vec2{bx + sw,        by + sh * 0.5f},  // P3 右中
                mine::paint::Brush::solid(col_orange), pen_cub_flat2);
        }

        // ── 行9 右：StrokeCubicBezier（Round cap，大 S + 对称 C 形）────────────
        // Round cap 自然圆弧端点，展示三次贝塞尔在复杂路径中的表现
        {
            const float bx = col1 + ip;
            const float by = row8 + ip;

            mine::paint::Pen pen_cub_round;
            pen_cub_round.width     = 5.0f;
            pen_cub_round.start_cap = mine::paint::LineCap::Round;
            pen_cub_round.end_cap   = mine::paint::LineCap::Round;

            // 大 S 形（Round cap，紫色）：控制点拨幅更大形成夸张弯曲
            canvas.stroke_cubic_bezier(
                mine::math::Vec2{bx + sw * 0.2f, by + sh * 0.05f},   // P0
                mine::math::Vec2{bx + sw * 0.9f, by + sh * 0.25f},   // P1（向右上拉）
                mine::math::Vec2{bx + sw * 0.1f, by + sh * 0.75f},   // P2（向左下拉）
                mine::math::Vec2{bx + sw * 0.8f, by + sh * 0.95f},   // P3
                mine::paint::Brush::solid(col_purple), pen_cub_round);

            // C 形弧线对照（Round cap，绿色，细线）
            mine::paint::Pen pen_cub_round2;
            pen_cub_round2.width     = 3.0f;
            pen_cub_round2.start_cap = mine::paint::LineCap::Round;
            pen_cub_round2.end_cap   = mine::paint::LineCap::Round;
            canvas.stroke_cubic_bezier(
                mine::math::Vec2{bx + sw * 0.8f, by + sh * 0.1f},    // P0 右上
                mine::math::Vec2{bx,             by + sh * 0.1f},     // P1（向左拉顶部）
                mine::math::Vec2{bx,             by + sh * 0.9f},     // P2（向左拉底部）
                mine::math::Vec2{bx + sw * 0.8f, by + sh * 0.9f},    // P3 右下
                mine::paint::Brush::solid(col_green), pen_cub_round2);
        }

        // ── 行10 左：FillPolygon（SDF 填充正六边形，凸多边形示例）──────────
        // 以格子中心为圆心，绘制内切六边形（flat-top 朝向，6 个顶点均匀分布）
        {
            const float cx10  = col0 + ip + sw * 0.5f;
            const float cy10  = row9 + ip + sh * 0.5f;
            const float r10   = std::min(sw, sh) * 0.44f;  // 外接圆半径
            constexpr float k_pi = 3.14159265358979323846f;
            mine::math::Vec2 hex[6];
            for (int k = 0; k < 6; ++k) {
                const float ang = k_pi / 6.0f + k * k_pi / 3.0f;  // 从 30° 起，每 60° 一个顶点
                hex[k] = mine::math::Vec2{cx10 + r10 * std::cos(ang),
                                          cy10 + r10 * std::sin(ang)};
            }
            canvas.fill_polygon(
                mine::core::Span<const mine::math::Vec2>{hex, 6},
                mine::paint::Brush::solid(col_blue));
        }

        // ── 行10 右：FillPolygon 五角星（凹多边形）+ StrokePolygon 六边形描边 ──
        // 五角星：10 个顶点，外圈 5 个（outer_r）和内圈 5 个（inner_r）交替排列
        // StrokePolygon：描边正六边形叠加，展示轮廓描边效果
        {
            const float cx10r = col1 + ip + sw * 0.5f;
            const float cy10r = row9 + ip + sh * 0.5f;
            const float outer_r = std::min(sw, sh) * 0.44f;  // 外圈半径
            const float inner_r = outer_r * 0.38f;            // 内圈半径（约 0.382 = 1/φ²，黄金比例）
            constexpr float k_pi = 3.14159265358979323846f;

            // 构建五角星顶点（10 点，从顶部开始，顺时针）
            mine::math::Vec2 star[10];
            for (int k = 0; k < 10; ++k) {
                const float ang = -k_pi * 0.5f + k * k_pi / 5.0f;  // 从 -90° 起，每 36° 一点
                const float r   = (k % 2 == 0) ? outer_r : inner_r;
                star[k] = mine::math::Vec2{cx10r + r * std::cos(ang),
                                           cy10r + r * std::sin(ang)};
            }
            // 填充五角星（凹多边形，IQ 绕数法正确处理内凹区域）
            canvas.fill_polygon(
                mine::core::Span<const mine::math::Vec2>{star, 10},
                mine::paint::Brush::solid(col_red));

            // 叠加描边六边形（flat-top，蓝色轮廓，2px 线宽）
            mine::math::Vec2 hex2[6];
            for (int k = 0; k < 6; ++k) {
                const float ang = k_pi / 6.0f + k * k_pi / 3.0f;
                hex2[k] = mine::math::Vec2{cx10r + outer_r * std::cos(ang),
                                           cy10r + outer_r * std::sin(ang)};
            }
            mine::paint::Pen pen_poly;
            pen_poly.width = 2.0f;
            canvas.stroke_polygon(
                mine::core::Span<const mine::math::Vec2>{hex2, 6},
                mine::paint::Brush::solid(col_cyan),
                pen_poly);
        }

        mine::paint::DisplayList dl = canvas.end();

        // ── 2. 提交渲染 ────────────────────────────────────────────────────

        paint_renderer->begin_frame();
        paint_renderer->render(dl, back_buf);
        paint_renderer->end_frame();

        // ── 3. 呈现后缓冲 ──────────────────────────────────────────────────
        swapchain->present();
    }
};

// ── main ──────────────────────────────────────────────────────────────────────

int main(int /*argc*/, char* /*argv*/[])
{
    // 1. 创建应用宿主（Win32 平台实现，消息循环）
    auto host = mine::platform::create_application_host();

    // 2. 创建 D3D11 图形设备
    auto device = mine::gfx::create_device(mine::gfx::Backend::Auto);
    if (!device) {
        std::fprintf(stderr, "错误：图形设备创建失败（当前系统不支持 D3D11）\n");
        return 1;
    }

    // 3. 创建 2D 渲染器（基于 RHI，不依赖具体图形 API）
    auto paint_renderer = mine::paint::create_renderer(device.get());
    if (!paint_renderer) {
        std::fprintf(stderr, "错误：2D 渲染器创建失败（着色器编译失败？）\n");
        return 1;
    }

    // 4. 创建窗口
    mine::platform::WindowDesc win_desc{};
    win_desc.title         = "MineFramework - Canvas SDF 渲染演示（Fill / Stroke × Rect / RoundedRect / ComplexRoundedRect / Ellipse）";
    win_desc.size          = {960.0f, 810.0f};
    win_desc.auto_position = true;
    win_desc.resizable     = true;
    win_desc.kind          = mine::platform::WindowKind::Normal;

    auto window = host->create_window(win_desc);
    if (!window) {
        std::fprintf(stderr, "错误：窗口创建失败\n");
        return 1;
    }

    // 5. 构建渲染器
    HelloRectRenderer renderer;
    renderer.device         = std::move(device);
    renderer.paint_renderer = std::move(paint_renderer);

    renderer.queue    = renderer.device->create_queue(mine::gfx::QueueType::Graphics);
    renderer.cmd_list = renderer.device->create_command_list(mine::gfx::QueueType::Graphics);
    if (!renderer.queue || !renderer.cmd_list) {
        std::fprintf(stderr, "错误：命令队列或命令列表创建失败\n");
        return 1;
    }

    // 6. 创建交换链
    mine::gfx::SwapchainDesc sc_desc{};
    sc_desc.native_window = window->native_handle().ptr;
    sc_desc.width         = static_cast<uint32_t>(win_desc.size.width);
    sc_desc.height        = static_cast<uint32_t>(win_desc.size.height);
    sc_desc.image_count   = 2;
    sc_desc.format        = mine::gfx::PixelFormat::BGRA8_UNorm;
    sc_desc.vsync         = mine::gfx::Vsync::On;

    renderer.swapchain = renderer.device->create_swapchain(sc_desc);
    if (!renderer.swapchain) {
        std::fprintf(stderr, "错误：交换链创建失败\n");
        return 1;
    }

    // 7. 从窗口获取实际 DPI 缩放因子，以物理像素大小重新创建交换链
    {
        const float dpi_scale = window->dpi() / 96.0f;
        if (dpi_scale > 1.001f) {
            // DPI 大于 100%，重建交换链为物理像素大小（此时交换链尚未 present 过，可直接 resize）
            const auto phys_w = static_cast<uint32_t>(win_desc.size.width  * dpi_scale);
            const auto phys_h = static_cast<uint32_t>(win_desc.size.height * dpi_scale);
            renderer.swapchain->resize(phys_w, phys_h);
        }
        renderer.dpi_scale_ = dpi_scale;
        renderer.paint_renderer->set_dpi_scale(dpi_scale);
    }

    // 8. 订阅窗口事件（Resized + DpiChanged）
    window->events().subscribe(&renderer);

    // 9. 显示窗口
    window->show();

    // 10. 渲染第一帧
    renderer.render();

    // 11. 进入消息循环，直到窗口关闭
    const int exit_code = host->run();

    // 12. 清理
    window->events().unsubscribe(&renderer);

    return exit_code;
}
