/**
 * @file main.cpp
 * @brief 00-hello-rect 示例：mine.paint Canvas SDF 填充 + 描边 + 裁剪 + 变换命令综合演示。
 *
 * 演示内容（6×6 网格布局，6列 × 6行 = 36格）：
 *   (col0,row0): FillRect              (col1,row0): StrokeRect
 *   (col2,row0): FillRoundedRect        (col3,row0): StrokeRoundedRect
 *   (col4,row0): FillComplexRoundedRect (col5,row0): StrokeComplexRoundedRect
 *   (col0,row1): FillEllipse            (col1,row1): StrokeEllipse
 *   (col2,row1): StrokeBorderedRR四边   (col3,row1): StrokeBorderedRR均匀
 *   (col4,row1): StrokeLine Flat        (col5,row1): StrokeLine Round+Square
 *   (col0,row2): StrokeArc Flat         (col1,row2): StrokeArc Round
 *   (col2,row2): StrokeQuadBezier Flat  (col3,row2): StrokeQuadBezier Round
 *   (col4,row2): StrokeCubicBezier Flat (col5,row2): StrokeCubicBezier Round
 *   (col0,row3): FillPolygon 六边形     (col1,row3): FillPolygon 五角星+Stroke
 *   (col2,row3): LinearGradient FillRect (col3,row3): RadialGradient FillEllipse
 *   (col4,row3): LinearGradient FillRR  (col5,row3): RadialGradient FillRect
 *   (col0,row4): AcrylicBrush RR        (col1,row4): AcrylicBrush Ellipse
 *   (col2,row4): clip_rect              (col3,row4): clip_rounded_rect
 *   (col4,row4): clip_complex_rr        (col5,row4): clip_polygon
 *   (col0,row5): translate+rotate       (col1,row5): translate+scale
 *   (col2,row5): 嵌套变换               (col3,row5): rotation_about
 *   (col4,row5): translate 纯平移       (col5,row5): translate 步进
 *   (col0,row6): FillPath 椭圆路径      (col1,row6): FillPath 心形
 *   (col2,row6): StrokePath 波浪线      (col3,row6): StrokePath 圆形轮廓
 *   (col4,row6): ClipPath 椭圆路径裁剪  (col5,row6): ClipPath 心形路径裁剪
 */

#include <mine/platform/PlatformAbi.h>
#include <mine/gfx/Gfx.h>
#include <mine/paint/Paint.h>
#include <mine/text/FontFace.h>

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
    mine::core::OwnedPtr<mine::text::FontFace>    font_face;  ///< 用于 DrawText 演示的字体面

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

        // 6×6 网格参数（6列 × 6行）
        const float outer_pad = 20.0f;
        const float gap       = 8.0f;
        const float cw        = (W - outer_pad * 2.0f - gap * 5.0f) / 6.0f;
        const float ch        = (H - outer_pad * 2.0f - gap * 6.0f) / 7.0f;

        // 列起始 x 坐标（共 6 列）
        const float col0 = outer_pad;
        const float col1 = outer_pad + (cw + gap) * 1.0f;
        const float col2 = outer_pad + (cw + gap) * 2.0f;
        const float col3 = outer_pad + (cw + gap) * 3.0f;
        const float col4 = outer_pad + (cw + gap) * 4.0f;
        const float col5 = outer_pad + (cw + gap) * 5.0f;

        // 行起始 y 坐标（共 7 行）
        const float row0 = outer_pad;
        const float row1 = outer_pad + (ch + gap) * 1.0f;
        const float row2 = outer_pad + (ch + gap) * 2.0f;
        const float row3 = outer_pad + (ch + gap) * 3.0f;
        const float row4 = outer_pad + (ch + gap) * 4.0f;
        const float row5 = outer_pad + (ch + gap) * 5.0f;
        const float row6 = outer_pad + (ch + gap) * 6.0f;

        const float ip = 10.0f;           // 格子内边距
        const float sw = cw - ip * 2.0f;  // 格子内容宽
        const float sh = ch - ip * 2.0f;  // 格子内容高

        // 颜色定义
        const mine::math::Color col_green  {0.20f, 0.75f, 0.35f, 1.0f};
        const mine::math::Color col_blue   {0.20f, 0.45f, 0.90f, 1.0f};
        const mine::math::Color col_orange {0.95f, 0.55f, 0.10f, 1.0f};
        const mine::math::Color col_purple {0.65f, 0.25f, 0.90f, 1.0f};
        const mine::math::Color col_red    {0.90f, 0.20f, 0.20f, 1.0f};
        const mine::math::Color col_cyan   {0.15f, 0.80f, 0.85f, 1.0f};

        const mine::paint::Pen pen{.width = 2.0f, .miter_limit = 4.0f};

        // ══ 行0：基本形状 ════════════════════════════════════════════════════

        // ── (col0, row0)：FillRect ───────────────────────────────────────────
        canvas.fill_rect(
            mine::math::Rect{col0 + ip, row0 + ip, sw, sh},
            mine::paint::Brush::solid(col_green));

        // ── (col1, row0)：StrokeRect ─────────────────────────────────────────
        canvas.stroke_rect(
            mine::math::Rect{col1 + ip, row0 + ip, sw, sh},
            mine::paint::Brush::solid(col_green), pen);

        // ── (col2, row0)：FillRoundedRect ────────────────────────────────────
        canvas.fill_rounded_rect(
            mine::math::RoundedRect{mine::math::Rect{col2 + ip, row0 + ip, sw, sh}, 16.0f},
            mine::paint::Brush::solid(col_blue));

        // ── (col3, row0)：StrokeRoundedRect ──────────────────────────────────
        canvas.stroke_rounded_rect(
            mine::math::RoundedRect{mine::math::Rect{col3 + ip, row0 + ip, sw, sh}, 16.0f},
            mine::paint::Brush::solid(col_blue), pen);

        // ── (col4, row0)：FillComplexRoundedRect ────────────────────────────
        {
            const mine::math::CornerRadii radii{
                {24.0f, 24.0f},  // 左上角
                {6.0f,  6.0f},   // 右上角
                {24.0f, 24.0f},  // 右下角
                {6.0f,  6.0f}    // 左下角
            };
            canvas.fill_complex_rounded_rect(
                mine::math::ComplexRoundedRect{mine::math::Rect{col4 + ip, row0 + ip, sw, sh}, radii},
                mine::paint::Brush::solid(col_orange));
        }

        // ── (col5, row0)：StrokeComplexRoundedRect ───────────────────────────
        {
            const mine::math::CornerRadii radii{
                {24.0f, 24.0f},  // 左上角
                {6.0f,  6.0f},   // 右上角
                {24.0f, 24.0f},  // 右下角
                {6.0f,  6.0f}    // 左下角
            };
            canvas.stroke_complex_rounded_rect(
                mine::math::ComplexRoundedRect{mine::math::Rect{col5 + ip, row0 + ip, sw, sh}, radii},
                mine::paint::Brush::solid(col_orange), pen);
        }

        // ── (col0, row1)：FillEllipse ─────────────────────────────────────────
        canvas.fill_ellipse(
            mine::math::Vec2{col0 + ip + sw * 0.5f, row1 + ip + sh * 0.5f},
            mine::math::Vec2{sw * 0.5f, sh * 0.5f},
            mine::paint::Brush::solid(col_purple));

        // ── (col1, row1)：StrokeEllipse ───────────────────────────────────────
        canvas.stroke_ellipse(
            mine::math::Vec2{col1 + ip + sw * 0.5f, row1 + ip + sh * 0.5f},
            mine::math::Vec2{sw * 0.5f, sh * 0.5f},
            mine::paint::Brush::solid(col_purple), pen);

        // ── (col2, row1)：StrokeBorderedRoundedRect（四边不等宽 + 四角独立圆角）──
        canvas.stroke_bordered_rounded_rect(
            mine::math::Rect{col2 + ip, row1 + ip, sw, sh},
            mine::paint::Brush::solid(col_red),
            mine::math::Thickness{8.0f, 4.0f, 2.0f, 16.0f},  // left=8, top=4, right=2, bottom=16
            mine::math::CornerRadii{
                {30.0f, 30.0f},  // 左上角
                {10.0f, 10.0f},  // 右上角
                {20.0f, 20.0f},  // 右下角
                {0.0f,  0.0f}    // 左下角（直角）
            });

        // ── (col3, row1)：StrokeBorderedRoundedRect（均匀圆角 + 仅上下边框各12px）──
        canvas.stroke_bordered_rounded_rect(
            mine::math::Rect{col3 + ip, row1 + ip, sw, sh},
            mine::paint::Brush::solid(col_cyan),
            mine::math::Thickness::symmetric(0.0f, 12.0f),  // 仅上下各 12px（symmetric: horizontal=0, vertical=12）
            mine::math::CornerRadii::uniform(16.0f));

        // ── (col4, row1)：StrokeLine（Flat cap，斜线，SDF 抗锯齿）──────────
        {
            const float x0 = col4 + ip;
            const float y0 = row1 + ip;
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
            const float x1 = col5 + ip;
            const float y1 = row1 + ip;
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

        // ══ 行2：圆弧 + 二次 / 三次贝塞尔 ════════════════════════════════════════════

        // ── (col0, row2)：StrokeArc（Flat cap，四分之一圆弧）──────────────────────────
        {
            const float cx7 = col0 + ip + sw * 0.5f;
            const float cy7 = row2 + ip + sh * 0.5f;
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
            const float cy7r = row2 + ip + sh * 0.5f;
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
            const float bx  = col2 + ip;
            const float by  = row2 + ip;
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
            const float bx  = col3 + ip;
            const float by  = row2 + ip;

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
            const float bx = col4 + ip;
            const float by = row2 + ip;

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
            const float bx = col5 + ip;
            const float by = row2 + ip;

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
            const float cy10  = row3 + ip + sh * 0.5f;
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
            const float cy10r = row3 + ip + sh * 0.5f;
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

        // ── 行11 左：线性渐变 FillRect（蓝 → 橙，从左到右）──────────────────
        {
            mine::paint::GradientStop lg_stops[] = {
                {0.0f, mine::math::Color{0.20f, 0.50f, 1.00f, 1.0f}},  // 蓝色
                {1.0f, mine::math::Color{1.00f, 0.45f, 0.10f, 1.0f}},  // 橙色
            };
            auto brush = mine::paint::Brush::linear_gradient(
                mine::math::Vec2{0.0f, 0.5f},   // 起点：左侧中点
                mine::math::Vec2{1.0f, 0.5f},   // 终点：右侧中点
                mine::core::Span<mine::paint::GradientStop>(lg_stops, 2));
            canvas.fill_rect(
                mine::math::Rect{col2 + ip, row3 + ip, sw, sh}, brush);
        }

        // ── 行11 右：径向渐变 FillEllipse（白 → 紫，从中心向外）────────────
        {
            mine::paint::GradientStop rg_stops[] = {
                {0.0f, mine::math::Color{1.00f, 1.00f, 1.00f, 1.0f}},  // 白色中心
                {1.0f, mine::math::Color{0.65f, 0.25f, 0.90f, 0.0f}},  // 紫色透明边缘
            };
            auto brush = mine::paint::Brush::radial_gradient(
                mine::math::Vec2{0.5f, 0.5f},   // 圆心：中心
                1.0f,                            // 外径：1.0（归一化，以较短半边为基准）
                mine::core::Span<mine::paint::GradientStop>(rg_stops, 2));
            // 为径向渐变提供深色背景
            canvas.fill_ellipse(
                mine::math::Vec2{col3 + ip + sw * 0.5f, row3 + ip + sh * 0.5f},
                mine::math::Vec2{sw * 0.5f, sh * 0.5f},
                mine::paint::Brush::solid(mine::math::Color{0.25f, 0.10f, 0.40f, 1.0f}));
            canvas.fill_ellipse(
                mine::math::Vec2{col3 + ip + sw * 0.5f, row3 + ip + sh * 0.5f},
                mine::math::Vec2{sw * 0.5f, sh * 0.5f},
                brush);
        }

        // ── 行12 左：线性渐变 FillRoundedRect（青 → 绿，从上到下）────────────
        {
            mine::paint::GradientStop lg_stops[] = {
                {0.0f, mine::math::Color{0.15f, 0.80f, 0.85f, 1.0f}},  // 青色顶部
                {1.0f, mine::math::Color{0.10f, 0.75f, 0.35f, 1.0f}},  // 绿色底部
            };
            auto brush = mine::paint::Brush::linear_gradient(
                mine::math::Vec2{0.5f, 0.0f},   // 起点：顶部中心
                mine::math::Vec2{0.5f, 1.0f},   // 终点：底部中心
                mine::core::Span<mine::paint::GradientStop>(lg_stops, 2));
            canvas.fill_rounded_rect(
                mine::math::RoundedRect{
                    mine::math::Rect{col4 + ip, row3 + ip, sw, sh},
                    std::min(sw, sh) * 0.25f,  // 圆角半径
                    std::min(sw, sh) * 0.25f},
                brush);
        }

        // ── 行12 右：径向渐变 FillRect（黄 → 红 → 暗红，三色标）───────────────
        {
            mine::paint::GradientStop rg_stops[] = {
                {0.00f, mine::math::Color{1.00f, 0.95f, 0.20f, 1.0f}},  // 黄色中心
                {0.50f, mine::math::Color{0.95f, 0.40f, 0.10f, 1.0f}},  // 橙红中圈
                {1.00f, mine::math::Color{0.50f, 0.05f, 0.05f, 1.0f}},  // 暗红边缘
            };
            auto brush = mine::paint::Brush::radial_gradient(
                mine::math::Vec2{0.5f, 0.5f},   // 圆心：矩形中心
                1.0f,                            // 外径
                mine::core::Span<mine::paint::GradientStop>(rg_stops, 3));
            canvas.fill_rect(
                mine::math::Rect{col5 + ip, row3 + ip, sw, sh}, brush);
        }

        // ══ 行4：亚克力画刷 + 裁剪演示 ══════════════════════════════════════════════

        // ── (col0, row4)：AcrylicBrush FillRoundedRect（蓝色调，30px 模糊）────────
        // 亚克力效果：捕获上一帧内容 → 高斯模糊 → 叠加蓝色色调
        // 注：首帧背景为空（黑色），从第二帧起可见模糊内容
        {
            auto brush = mine::paint::Brush::acrylic(
                mine::math::Color{0.30f, 0.50f, 1.00f, 0.85f},  // 蓝色色调
                0.50f,   // 色调混合比例（0=完全不叠色调，1=完全色调覆盖）
                30.0f,   // 模糊量（像素）
                1.2f);   // 饱和度增益（>1 增强饱和度）
            canvas.fill_rounded_rect(
                mine::math::RoundedRect{
                    mine::math::Rect{col0 + ip, row4 + ip, sw, sh},
                    std::min(sw, sh) * 0.25f,
                    std::min(sw, sh) * 0.25f},
                brush);
        }

        // ── (col1, row4)：AcrylicBrush FillEllipse（橙色调，20px 模糊）────────────
        {
            auto brush = mine::paint::Brush::acrylic(
                mine::math::Color{1.00f, 0.55f, 0.15f, 0.80f},  // 橙色色调
                0.40f,   // 色调混合比例
                20.0f,   // 模糊量（像素，较小模糊用于轻微磨砂效果）
                0.9f);   // 饱和度增益（<1 轻微降饱和）
            canvas.fill_ellipse(
                mine::math::Vec2{col1 + ip + sw * 0.5f, row4 + ip + sh * 0.5f},
                mine::math::Vec2{sw * 0.5f, sh * 0.5f},
                brush);
        }

        // ── 行14 左：clip_rect + FillEllipse（矩形裁剪演示）──────────────────
        // 压入矩形裁剪区（格子上半 60%），填充完整椭圆后弹出裁剪，
        // 展示 clip_rect 将椭圆下部截断的效果。
        {
            const float x = col2, y = row4, w = cw, h = ch;
            const float clip_ip = 4.0f;
            // 裁剪区域：格子上半 60% 高度（截去下 40%）
            mine::math::Rect clip{x + clip_ip, y + clip_ip, w - clip_ip * 2.0f, h * 0.6f - clip_ip};
            canvas.clip_rect(clip);
            // 填充超出裁剪区的大椭圆（下部被矩形截断）
            canvas.fill_ellipse(
                mine::math::Vec2{x + w * 0.5f, y + h * 0.5f},
                mine::math::Vec2{w * 0.45f, h * 0.45f},
                mine::paint::Brush::solid(mine::math::Color{0.2f, 0.8f, 0.4f, 1.0f}));  // 绿色
            canvas.clip_pop();
            // 裁剪区轮廓（半透明白色，标注裁剪边界）
            canvas.stroke_rect(clip,
                mine::paint::Brush::solid(mine::math::Color{1.0f, 1.0f, 1.0f, 0.4f}),
                mine::paint::Pen{1.0f});
        }

        // ── 行14 右：clip_rounded_rect + FillRect（均匀圆角矩形裁剪演示）─────
        // 压入均匀圆角矩形裁剪区，再填充金色矩形（四角被圆角截断），
        // 内部叠加蓝色小圆演示在裁剪区内继续绘制。
        {
            const float x = col3, y = row4, w = cw, h = ch;
            const float clip_ip = 4.0f;
            const float radius = h * 0.3f;  // 30% 高度作为圆角半径
            mine::math::RoundedRect clip_rrect{
                mine::math::Rect{x + clip_ip, y + clip_ip, w - clip_ip * 2.0f, h - clip_ip * 2.0f},
                radius, radius};
            canvas.clip_rounded_rect(clip_rrect);
            canvas.fill_rect(
                mine::math::Rect{x + clip_ip, y + clip_ip, w - clip_ip * 2.0f, h - clip_ip * 2.0f},
                mine::paint::Brush::solid(mine::math::Color{0.95f, 0.75f, 0.2f, 1.0f}));  // 金色
            canvas.fill_ellipse(
                mine::math::Vec2{x + w * 0.5f, y + h * 0.5f},
                mine::math::Vec2{w * 0.25f, h * 0.25f},
                mine::paint::Brush::solid(mine::math::Color{0.2f, 0.2f, 0.9f, 0.9f}));   // 蓝色
            canvas.clip_pop();
            canvas.stroke_rounded_rect(clip_rrect,
                mine::paint::Brush::solid(mine::math::Color{1.0f, 1.0f, 1.0f, 0.4f}),
                mine::paint::Pen{1.0f});
        }

        // ── 行15 左：clip_complex_rounded_rect（四角独立圆角矩形裁剪演示）─────
        // 四角设置不同圆角（左上/右下大，右上/左下小），裁剪青→紫线性渐变矩形，
        // 展示 clip_complex_rounded_rect 的复杂形状裁剪能力。
        {
            const float x = col4, y = row4, w = cw, h = ch;
            const float clip_ip = 4.0f;
            mine::math::ComplexRoundedRect clip_crr{};
            clip_crr.rect = mine::math::Rect{x + clip_ip, y + clip_ip, w - clip_ip * 2.0f, h - clip_ip * 2.0f};
            clip_crr.radii.top_left.x     = clip_crr.radii.top_left.y     = h * 0.4f;
            clip_crr.radii.top_right.x    = clip_crr.radii.top_right.y    = h * 0.1f;
            clip_crr.radii.bottom_left.x  = clip_crr.radii.bottom_left.y  = h * 0.1f;
            clip_crr.radii.bottom_right.x = clip_crr.radii.bottom_right.y = h * 0.4f;
            canvas.clip_complex_rounded_rect(clip_crr);
            mine::paint::GradientStop lgs[] = {
                {0.0f, mine::math::Color{0.1f, 0.9f, 0.9f, 1.0f}},
                {1.0f, mine::math::Color{0.7f, 0.1f, 0.9f, 1.0f}},
            };
            canvas.fill_rect(clip_crr.rect,
                mine::paint::Brush::linear_gradient(
                    mine::math::Vec2{0.0f, 0.5f},
                    mine::math::Vec2{1.0f, 0.5f},
                    mine::core::Span<mine::paint::GradientStop>(lgs, 2)));
            canvas.clip_pop();
            canvas.stroke_complex_rounded_rect(clip_crr,
                mine::paint::Brush::solid(mine::math::Color{1.0f, 1.0f, 1.0f, 0.4f}),
                mine::paint::Pen{});
        }

        // ── (col5, row4)：clip_polygon（等边三角形裁剪演示）─────────────────────
        {
            const float x = col5, y = row4, w = cw, h = ch;
            const float clip_ip = 8.0f;
            const float cx = x + w * 0.5f;
            const float cy = y + h * 0.5f;
            const float r  = std::min(w, h) * 0.5f - ip;
            // 等边三角形三顶点（顶点朝上）
            mine::math::Vec2 tri_verts[3] = {
                {cx,              cy - r},
                {cx + r * 0.866f, cy + r * 0.5f},
                {cx - r * 0.866f, cy + r * 0.5f},
            };
            canvas.clip_polygon(mine::core::Span<const mine::math::Vec2>(tri_verts, 3));
            canvas.fill_rect(
                mine::math::Rect{x + clip_ip, y + clip_ip, w - clip_ip * 2.0f, h - clip_ip * 2.0f},
                mine::paint::Brush::solid(mine::math::Color{0.95f, 0.35f, 0.2f, 1.0f}));  // 橙红色
            canvas.clip_pop();
            // 三角形轮廓（半透明白色边框）
            for (int k = 0; k < 3; ++k) {
                const mine::math::Vec2& va = tri_verts[k];
                const mine::math::Vec2& vb = tri_verts[(k + 1) % 3];
                canvas.stroke_line(va, vb,
                    mine::paint::Brush::solid(mine::math::Color{1.0f, 1.0f, 1.0f, 0.4f}),
                    mine::paint::Pen{1.5f, mine::paint::LineJoin::Miter,
                                 mine::paint::LineCap::Round, mine::paint::LineCap::Round});
            }
        }

        // ── 行16 左：translate + rotate（平移后旋转 30° 的圆角矩形）──────────
        // save() → translate到格子中心 → rotate(30°) → fill_rounded_rect → restore()
        // 同时叠加一个未旋转的轮廓（半透明），对比旋转效果。
        {
            const float x = col0, y = row5, w = cw, h = ch;
            const float cx = x + w * 0.5f, cy = y + h * 0.5f;
            const float rw = sw * 0.72f, rh = sh * 0.52f;
            constexpr float kPi = 3.14159265f;
            const float angle = kPi / 6.0f;  // 30°

            // 未旋转的参考轮廓（半透明白色）
            canvas.stroke_rounded_rect(
                mine::math::RoundedRect{
                    mine::math::Rect{cx - rw * 0.5f, cy - rh * 0.5f, rw, rh},
                    12.0f, 12.0f},
                mine::paint::Brush::solid(mine::math::Color{1.0f, 1.0f, 1.0f, 0.2f}),
                mine::paint::Pen{1.0f});

            // save → translate → rotate → 绘制 → restore
            canvas.save();
            canvas.translate(mine::math::Vec2{cx, cy});
            canvas.rotate(angle);
            // 画布原点已在格子中心，矩形以原点为中心放置
            canvas.fill_rounded_rect(
                mine::math::RoundedRect{
                    mine::math::Rect{-rw * 0.5f, -rh * 0.5f, rw, rh}, 12.0f, 12.0f},
                mine::paint::Brush::solid(col_orange));
            canvas.restore();
        }

        // ── 行16 右：translate + scale（平移后放大 1.5 倍的椭圆）───────────
        // 先绘制正常大小轮廓作参考，再 save → translate → scale → fill_ellipse → restore。
        {
            const float x = col1, y = row5, w = cw, h = ch;
            const float cx = x + w * 0.5f, cy = y + h * 0.5f;
            const float erx = sw * 0.28f, ery = sh * 0.33f;

            // 正常大小轮廓（半透明白色，scale=1 参考）
            canvas.stroke_ellipse(
                mine::math::Vec2{cx, cy},
                mine::math::Vec2{erx, ery},
                mine::paint::Brush::solid(mine::math::Color{1.0f, 1.0f, 1.0f, 0.2f}),
                mine::paint::Pen{1.0f});

            // save → translate → scale(1.5) → fill_ellipse → restore
            canvas.save();
            canvas.translate(mine::math::Vec2{cx, cy});
            canvas.scale(1.5f);
            // 现在原点在格子中心，scale 已生效
            canvas.fill_ellipse(
                mine::math::Vec2{0.0f, 0.0f},
                mine::math::Vec2{erx, ery},
                mine::paint::Brush::solid(mine::math::Color{0.25f, 0.5f, 1.0f, 0.85f}));
            canvas.restore();
        }

        // ── 行17 左：嵌套变换（外层旋转 + 内层旋转+缩放）────────────────────
        // 外层：save → translate → rotate(15°) → fill_rounded_rect（绿色）
        // 内层：再 save → rotate(30°) → scale(0.6) → fill_rounded_rect（橙色）→ restore → restore
        {
            const float x = col2, y = row5, w = cw, h = ch;
            const float cx = x + w * 0.5f, cy = y + h * 0.5f;
            const float rw = sw * 0.60f, rh = sh * 0.58f;
            constexpr float kPi = 3.14159265f;

            // 外层：旋转 15°
            canvas.save();
            canvas.translate(mine::math::Vec2{cx, cy});
            canvas.rotate(kPi / 12.0f);  // 15°

            canvas.fill_rounded_rect(
                mine::math::RoundedRect{
                    mine::math::Rect{-rw * 0.5f, -rh * 0.5f, rw, rh}, 10.0f, 10.0f},
                mine::paint::Brush::solid(mine::math::Color{0.25f, 0.72f, 0.30f, 0.9f}));

            // 内层（叠加在外层变换之上）：再旋转 30° + 缩放 0.6
            canvas.save();
            canvas.rotate(kPi / 6.0f);  // 再旋转 30°（相对于外层画布）
            canvas.scale(0.6f);
            canvas.fill_rounded_rect(
                mine::math::RoundedRect{
                    mine::math::Rect{-rw * 0.5f, -rh * 0.5f, rw, rh}, 10.0f, 10.0f},
                mine::paint::Brush::solid(mine::math::Color{0.92f, 0.42f, 0.12f, 0.9f}));
            canvas.restore();  // 恢复外层变换

            canvas.restore();  // 恢复初始变换
        }

        // ── 行17 右：rotation_about（五个矩形绕格子中心均匀旋转分布）────────
        // 五个彩色圆角矩形绕格子中心以 72° 间隔排布，使用 Transform2D::rotation_about
        // 演示绕任意点旋转的能力。
        {
            const float x = col3, y = row5, w = cw, h = ch;
            const float cx = x + w * 0.5f, cy = y + h * 0.5f;
            const float orbit_r = std::min(sw, sh) * 0.30f;  // 轨道半径
            const float rw = sw * 0.22f, rh = sh * 0.18f;    // 小矩形尺寸
            constexpr float kPi = 3.14159265f;

            // 中心参考点（白色小圆）
            canvas.fill_ellipse(
                mine::math::Vec2{cx, cy},
                mine::math::Vec2{4.0f, 4.0f},
                mine::paint::Brush::solid(mine::math::Color{1.0f, 1.0f, 1.0f, 0.6f}));

            // 五个小矩形，初始位置均在 (cx + orbit_r, cy)，用 rotation_about 旋转到各自位置
            const mine::math::Color orbit_colors[5] = {
                {1.0f, 0.30f, 0.30f, 0.90f},  // 红
                {0.30f, 1.0f, 0.30f, 0.90f},  // 绿
                {0.30f, 0.45f, 1.0f, 0.90f},  // 蓝
                {1.0f, 0.85f, 0.20f, 0.90f},  // 黄
                {0.90f, 0.30f, 0.90f, 0.90f}, // 紫
            };
            for (int k = 0; k < 5; ++k) {
                const float angle = static_cast<float>(k) * (2.0f * kPi / 5.0f);  // 72° 间隔
                // rotation_about(angle, pivot) 将坐标系绕 pivot 旋转 angle 弧度
                canvas.save();
                canvas.transform(mine::math::Transform2D::rotation_about(
                    angle, mine::math::Point{cx, cy}));
                // 矩形初始位于中心右方 orbit_r 处，旋转后被映射到轨道对应位置
                canvas.fill_rounded_rect(
                    mine::math::RoundedRect{
                        mine::math::Rect{cx + orbit_r - rw * 0.5f, cy - rh * 0.5f, rw, rh}, 6.0f, 6.0f},
                    mine::paint::Brush::solid(orbit_colors[k]));
                canvas.restore();
            }
        }

        // ── 行18 左：translate（纯平移演示）──────────────────────────────────
        // 原始位置轮廓（半透明白色）+ save → translate(dx, dy) → 填充圆角矩形 → restore
        // 直观展示 translate 将画布原点偏移后绘图的效果。
        {
            const float x = col4, y = row5;
            const float rw = sw * 0.55f, rh = sh * 0.55f;
            const float dx = sw * 0.20f, dy = sh * 0.18f;  // 平移量
            // 参考矩形定位于格子内容区左上偏内位置
            const float ref_x = x + ip + sw * 0.08f;
            const float ref_y = y + ip + sh * 0.08f;

            // 原始位置参考轮廓（半透明白色虚线感觉）
            canvas.stroke_rounded_rect(
                mine::math::RoundedRect{
                    mine::math::Rect{ref_x, ref_y, rw, rh}, 10.0f, 10.0f},
                mine::paint::Brush::solid(mine::math::Color{1.0f, 1.0f, 1.0f, 0.25f}),
                mine::paint::Pen{1.5f});

            // save → translate → 在相同相对坐标绘制填充矩形 → restore
            canvas.save();
            canvas.translate(mine::math::Vec2{dx, dy});
            canvas.fill_rounded_rect(
                mine::math::RoundedRect{
                    mine::math::Rect{ref_x, ref_y, rw, rh}, 10.0f, 10.0f},
                mine::paint::Brush::solid(mine::math::Color{0.25f, 0.72f, 0.90f, 0.90f}));
            canvas.restore();
        }

        // ── 行18 右：translate 步进演示（对角线递进平移）──────────────────
        // 5 个小圆角矩形，每个在前一个基础上向右下方固定步进平移，颜色从蓝渐变到橙。
        // 展示多次独立 save/translate/draw/restore 的累积效果对比。
        {
            const float x = col5, y = row5;
            constexpr int n = 5;
            const float rw = sw * 0.28f, rh = sh * 0.28f;
            // 步进量：让 n 个矩形均匀分布在格子内容区对角线上
            const float step_x = (sw - rw) / (n - 1);
            const float step_y = (sh - rh) / (n - 1);
            const float start_x = x + ip;
            const float start_y = y + ip;

            for (int k = 0; k < n; ++k) {
                const float t = static_cast<float>(k) / (n - 1);
                // 颜色从蓝（左上）渐变到橙（右下）
                const mine::math::Color c{
                    0.15f + t * 0.80f,
                    0.50f - t * 0.15f,
                    0.92f - t * 0.62f,
                    0.82f};
                // 各矩形从 (0, 0) 开始，通过 translate 定位到对角线上
                canvas.save();
                canvas.translate(mine::math::Vec2{start_x + step_x * k,
                                                  start_y + step_y * k});
                canvas.fill_rounded_rect(
                    mine::math::RoundedRect{
                        mine::math::Rect{0.0f, 0.0f, rw, rh}, 8.0f, 8.0f},
                    mine::paint::Brush::solid(c));
                canvas.restore();
            }
        }

        // ══ 行6：FillPath / StrokePath / ClipPath 演示 ═══════════════════════

        // ── (col0, row6)：FillPath — cubic bezier 近似椭圆路径填充 ─────────────
        // 用 4 段三次贝塞尔曲线近似圆（κ≈0.5523），展示 FillPath 对曲线路径的支持。
        {
            constexpr float k_pi = 3.14159265358979323846f;
            const float cx6 = col0 + ip + sw * 0.5f;
            const float cy6 = row6 + ip + sh * 0.5f;
            const float rx6 = sw * 0.44f;  // 水平半径
            const float ry6 = sh * 0.44f;  // 垂直半径（略扁的椭圆）
            constexpr float kappa = 0.5523f;

            mine::paint::PathBuilder pb6;
            pb6.move_to({cx6, cy6 - ry6});
            pb6.cubic_to({cx6 + kappa * rx6, cy6 - ry6},
                         {cx6 + rx6, cy6 - kappa * ry6},
                         {cx6 + rx6, cy6});
            pb6.cubic_to({cx6 + rx6, cy6 + kappa * ry6},
                         {cx6 + kappa * rx6, cy6 + ry6},
                         {cx6, cy6 + ry6});
            pb6.cubic_to({cx6 - kappa * rx6, cy6 + ry6},
                         {cx6 - rx6, cy6 + kappa * ry6},
                         {cx6 - rx6, cy6});
            pb6.cubic_to({cx6 - rx6, cy6 - kappa * ry6},
                         {cx6 - kappa * rx6, cy6 - ry6},
                         {cx6, cy6 - ry6});
            pb6.close();
            auto path6 = pb6.build();
            canvas.fill_path(path6, mine::paint::Brush::solid(col_green));
            (void)k_pi;
        }

        // ── (col1, row6)：FillPath — 心形路径（cubic bezier 近似）─────────────
        // 4 段三次贝塞尔曲线拼合心形，展示 FillPath 对复杂闭合路径的支持。
        {
            const float cx6 = col1 + ip + sw * 0.5f;
            const float cy6 = row6 + ip + sh * 0.5f;
            const float r6  = std::min(sw, sh) * 0.40f;

            mine::paint::PathBuilder pb6;
            pb6.move_to({cx6, cy6 + r6});                        // 底部尖点
            pb6.cubic_to({cx6 - r6 * 0.45f, cy6 + r6 * 0.70f},
                         {cx6 - r6,          cy6},
                         {cx6 - r6,          cy6 - r6 * 0.25f}); // 左下段
            pb6.cubic_to({cx6 - r6,          cy6 - r6 * 0.85f},
                         {cx6 - r6 * 0.50f,  cy6 - r6},
                         {cx6,               cy6 - r6 * 0.65f}); // 左上段
            pb6.cubic_to({cx6 + r6 * 0.50f,  cy6 - r6},
                         {cx6 + r6,          cy6 - r6 * 0.85f},
                         {cx6 + r6,          cy6 - r6 * 0.25f}); // 右上段
            pb6.cubic_to({cx6 + r6,          cy6},
                         {cx6 + r6 * 0.45f,  cy6 + r6 * 0.70f},
                         {cx6,               cy6 + r6});          // 右下段
            pb6.close();
            auto path6 = pb6.build();
            canvas.fill_path(path6, mine::paint::Brush::solid(col_red));
        }

        // ── (col2, row6)：StrokePath — 波浪路径（多段 QuadTo 描边）─────────────
        // 4 段二次贝塞尔拼成连续波浪，展示 StrokePath 对开放曲线路径的描边支持。
        {
            const float x6    = col2 + ip;
            const float mid_y = row6 + ip + sh * 0.5f;
            const float amp   = sh * 0.36f;
            const float step  = sw / 4.0f;

            mine::paint::PathBuilder pb6;
            pb6.move_to({x6, mid_y});
            for (int k = 0; k < 4; ++k) {
                const float ctrl_y = mid_y + ((k % 2 == 0) ? -amp : amp);
                const float ctrl_x = x6 + (k + 0.5f) * step;
                const float end_x  = x6 + (k + 1.0f) * step;
                pb6.quad_to({ctrl_x, ctrl_y}, {end_x, mid_y});
            }
            auto path6 = pb6.build();
            mine::paint::Pen pen6{.width = 3.0f};
            canvas.stroke_path(path6, mine::paint::Brush::solid(col_orange), pen6);
        }

        // ── (col3, row6)：StrokePath — 六角星形轮廓（QuadTo 波浪环）────────────
        // 用 6 段 QuadTo 拼合一个向外凸出的星形闭合轮廓，展示闭合路径的描边效果。
        {
            constexpr float k_pi = 3.14159265358979323846f;
            const float cx6 = col3 + ip + sw * 0.5f;
            const float cy6 = row6 + ip + sh * 0.5f;
            const float r_out = std::min(sw, sh) * 0.44f;
            const float r_in  = r_out * 0.55f;

            mine::paint::PathBuilder pb6;
            // 从顶部顶点开始，6 段 QuadTo：奇数段向外，偶数段向内
            pb6.move_to({cx6, cy6 - r_out});
            for (int k = 0; k < 6; ++k) {
                const float a_ctrl = -k_pi * 0.5f + (k + 0.5f) * k_pi / 3.0f;
                const float a_end  = -k_pi * 0.5f + (k + 1.0f) * k_pi / 3.0f;
                const float r_ctrl = (k % 2 == 0) ? r_in : r_out * 1.15f;
                pb6.quad_to({cx6 + r_ctrl * std::cos(a_ctrl),
                              cy6 + r_ctrl * std::sin(a_ctrl)},
                             {cx6 + r_out  * std::cos(a_end),
                              cy6 + r_out  * std::sin(a_end)});
            }
            pb6.close();
            auto path6 = pb6.build();
            mine::paint::Pen pen6{.width = 2.5f};
            canvas.stroke_path(path6, mine::paint::Brush::solid(col_cyan), pen6);
            (void)k_pi;
        }

        // ── (col4, row6)：ClipPath — 椭圆路径裁剪（内部线性渐变矩形）────────────
        // cubic bezier 近似椭圆作为裁剪形状，展示 clip_path 对曲线路径的支持。
        {
            const float cx6 = col4 + ip + sw * 0.5f;
            const float cy6 = row6 + ip + sh * 0.5f;
            const float rx6 = sw * 0.44f;
            const float ry6 = sh * 0.44f;
            constexpr float kappa = 0.5523f;

            mine::paint::PathBuilder pb6;
            pb6.move_to({cx6, cy6 - ry6});
            pb6.cubic_to({cx6 + kappa * rx6, cy6 - ry6},
                         {cx6 + rx6, cy6 - kappa * ry6},
                         {cx6 + rx6, cy6});
            pb6.cubic_to({cx6 + rx6, cy6 + kappa * ry6},
                         {cx6 + kappa * rx6, cy6 + ry6},
                         {cx6, cy6 + ry6});
            pb6.cubic_to({cx6 - kappa * rx6, cy6 + ry6},
                         {cx6 - rx6, cy6 + kappa * ry6},
                         {cx6 - rx6, cy6});
            pb6.cubic_to({cx6 - rx6, cy6 - kappa * ry6},
                         {cx6 - kappa * rx6, cy6 - ry6},
                         {cx6, cy6 - ry6});
            pb6.close();

            canvas.save();
            canvas.clip_path(pb6.build());
            // 内部填充对角线线性渐变矩形
            mine::paint::GradientStop stops[] = {
                {0.0f, mine::math::Color{0.15f, 0.55f, 1.00f, 1.0f}},
                {1.0f, mine::math::Color{0.90f, 0.25f, 0.75f, 1.0f}},
            };
            canvas.fill_rect(
                mine::math::Rect{col4 + ip, row6 + ip, sw, sh},
                mine::paint::Brush::linear_gradient(
                    mine::math::Vec2{0.0f, 0.0f},
                    mine::math::Vec2{1.0f, 1.0f},
                    mine::core::Span<const mine::paint::GradientStop>{stops, 2}));
            canvas.restore();
        }

        // ── (col5, row6)：ClipPath — 心形路径裁剪（内部填充橙色椭圆）──────────
        // 心形 cubic bezier 路径作为裁剪区域，内填橙色椭圆，展示复杂路径裁剪效果。
        {
            const float cx6 = col5 + ip + sw * 0.5f;
            const float cy6 = row6 + ip + sh * 0.5f;
            const float r6  = std::min(sw, sh) * 0.40f;

            mine::paint::PathBuilder pb6;
            pb6.move_to({cx6, cy6 + r6});
            pb6.cubic_to({cx6 - r6 * 0.45f, cy6 + r6 * 0.70f},
                         {cx6 - r6,          cy6},
                         {cx6 - r6,          cy6 - r6 * 0.25f});
            pb6.cubic_to({cx6 - r6,          cy6 - r6 * 0.85f},
                         {cx6 - r6 * 0.50f,  cy6 - r6},
                         {cx6,               cy6 - r6 * 0.65f});
            pb6.cubic_to({cx6 + r6 * 0.50f,  cy6 - r6},
                         {cx6 + r6,          cy6 - r6 * 0.85f},
                         {cx6 + r6,          cy6 - r6 * 0.25f});
            pb6.cubic_to({cx6 + r6,          cy6},
                         {cx6 + r6 * 0.45f,  cy6 + r6 * 0.70f},
                         {cx6,               cy6 + r6});
            pb6.close();

            canvas.save();
            canvas.clip_path(pb6.build());
            // 内填橙色椭圆
            canvas.fill_ellipse(
                mine::math::Vec2{cx6, cy6},
                mine::math::Vec2{sw * 0.48f, sh * 0.48f},
                mine::paint::Brush::solid(col_orange));
            canvas.restore();
        }

        // ── 文字渲染演示（顶部边距区域）──────────────────────────────────────
        // 在 15 行网格上方的 20px 留白区域绘制标题文字
        if (font_face) {
            canvas.draw_text(
                mine::core::StringView{"MineFramework - SDF Shapes + FreeType Text Rendering"},
                mine::math::Vec2{outer_pad, 15.0f},   // 基线 y=15，Segoe UI 12px ascender≈9px
                font_face.get(),
                12.0f,
                mine::paint::Brush::solid(mine::math::Color{1.0f, 1.0f, 1.0f, 0.85f}));
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
    win_desc.size          = {960.0f, 1160.0f};
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

    // 7.5 加载系统字体（用于顶部标题 DrawText 演示，失败时跳过，不影响其他功能）
    renderer.font_face = mine::text::FontFace::load_from_file("C:\\Windows\\Fonts\\segoeui.ttf");
    if (!renderer.font_face) {
        // Segoe UI 不可用时尝试 Arial
        renderer.font_face = mine::text::FontFace::load_from_file("C:\\Windows\\Fonts\\arial.ttf");
    }
    if (!renderer.font_face) {
        std::fprintf(stderr, "警告：系统字体加载失败，DrawText 演示将被跳过\n");
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
