/**
 * @file Canvas.h
 * @brief 绘制上下文（录制模式）。
 *
 * Canvas 是面向用户的主要绘制接口。以录制模式运行：
 * 所有绘制调用被记录为 DrawCmd，最终由 end() 汇聚成 DisplayList
 * 交给 IRenderer 执行实际渲染。
 *
 * 典型使用流程：
 * @code
 *   Canvas canvas;
 *   canvas.fill_rect({10, 10, 200, 100}, Brush::solid({0.2f, 0.6f, 1.0f}));
 *   canvas.stroke_rounded_rect({{50, 50, 300, 200}, 8.f}, Brush::solid_rgb(0xFF0000), Pen{2.f});
 *   DisplayList dl = canvas.end();
 *   renderer->render(dl, target_texture);
 * @endcode
 *
 * 注意：
 *   - Canvas 非线程安全，单线程使用。
 *   - end() 后 Canvas 恢复空状态，可继续复用。
 *   - save()/restore() 仅保存/恢复变换与裁剪状态。
 *
 * 依赖：mine.paint（DisplayList、DrawCmd、Brush、Pen、Path）,
 *       mine.math（Rect、RoundedRect、Vec2、Transform2D）
 */

#pragma once

#include <mine/paint/DisplayList.h>
#include <mine/paint/PathBuilder.h>
#include <mine/math/Thickness.h>
#include <mine/math/Transform2D.h>
#include <mine/math/ComplexRoundedRect.h>
#include <mine/math/CornerRadii.h>

namespace mine::paint {

/**
 * @brief 绘制上下文（录制模式）。
 *
 * 所有 fill_* / stroke_* 方法均将命令录制到内部缓冲，
 * 调用 end() 后生成 DisplayList 并重置自身状态。
 */
class Canvas {
public:
    Canvas()  = default;
    ~Canvas() = default;

    // 不可拷贝（持有命令缓冲所有权），允许移动
    Canvas(const Canvas&)            = delete;
    Canvas& operator=(const Canvas&) = delete;
    Canvas(Canvas&&)                 = default;
    Canvas& operator=(Canvas&&)      = default;

    // ── 状态管理 ────────────────────────────────────────────────────────

    /**
     * @brief 保存当前绘制状态（变换 + 裁剪）到栈中。
     *
     * 与 restore() 成对使用。可嵌套调用。
     */
    void save();

    /**
     * @brief 恢复最近一次 save() 保存的状态。
     *
     * 若 save/restore 不匹配（restore 比 save 多），则无操作。
     */
    void restore();

    /**
     * @brief 将当前变换与给定变换级联（在当前变换之后应用）。
     * @param t 要附加的变换矩阵
     */
    void transform(const math::Transform2D& t);

    /// 平移。
    void translate(math::Vec2 offset);

    /// 等比缩放。
    void scale(float factor);

    /// 沿 X/Y 独立缩放。
    void scale(math::Vec2 factor);

    /// 旋转（弧度，顺时针）。
    void rotate(float angle_rad);

    /**
     * @brief 以矩形区域裁剪后续绘制内容。
     *
     * 与当前裁剪区域取交集。可多次嵌套调用；通过 save()/restore() 撤销。
     */
    void clip_rect(math::Rect rect);

    // ── 填充命令 ────────────────────────────────────────────────────────

    /// 填充矩形。
    void fill_rect(math::Rect rect, const Brush& brush);

    /// 填充圆角矩形。
    void fill_rounded_rect(math::RoundedRect rrect, const Brush& brush);

    /// 填充四角各自独立椭圆半径的圆角矩形。
    void fill_complex_rounded_rect(math::ComplexRoundedRect rrect, const Brush& brush);

    /**
     * @brief 填充椭圆。
     * @param center 椭圆中心点
     * @param radii  X 轴半径（radii.x）和 Y 轴半径（radii.y）
     */
    void fill_ellipse(math::Vec2 center, math::Vec2 radii, const Brush& brush);

    /// 填充正圆。
    void fill_circle(math::Vec2 center, float radius, const Brush& brush) {
        fill_ellipse(center, {radius, radius}, brush);
    }

    /// 填充任意路径。
    void fill_path(const Path& path, const Brush& brush);

    // ── 描边命令 ────────────────────────────────────────────────────────

    /// 描边矩形。
    void stroke_rect(math::Rect rect, const Brush& brush, const Pen& pen = {});

    /// 描边圆角矩形。
    void stroke_rounded_rect(math::RoundedRect rrect, const Brush& brush, const Pen& pen = {});

    /// 描边四角各自独立椭圆半径的圆角矩形。
    void stroke_complex_rounded_rect(math::ComplexRoundedRect rrect, const Brush& brush, const Pen& pen = {});

    /**
     * @brief 四边各自独立宽度的矩形内侧描边（类 CSS border）。
     *
     * 描边方向为内侧：每条边的描边宽度从矩形边界向内延伸，
     * 不超出矩形的外轮布。
     * @param rect    矩形几何
     * @param brush   填充画刷
     * @param widths  四边宽度（调用 BorderWidths::all(w) 得到四边等宽）
     */
    void stroke_bordered_rect(math::Rect rect, const Brush& brush, math::Thickness widths);

    /**
     * @brief 四边各自独立宽度 + 四角各自独立圆角的矩形内侧描边。
     *
     * 结合 CSS border-width 和 border-radius 语义：外轮廓随圆角圆滑，
     * 各边描边带独立计算，角部直角相交并被外轮廓圆角自然剪裁。
     * @param rect   矩形几何
     * @param brush  画刷（当前仅支持 SolidColor）
     * @param widths 四边描边宽度（内侧方向）
     * @param radii  四角圆角半径（外轮廓）
     */
    void stroke_bordered_rounded_rect(math::Rect rect, const Brush& brush,
                                      math::Thickness widths, math::CornerRadii radii);

    /**
     * @brief 描边椭圆。
     * @param center 椭圆中心点
     * @param radii  X 轴半径（radii.x）和 Y 轴半径（radii.y）
     */
    void stroke_ellipse(math::Vec2 center, math::Vec2 radii, const Brush& brush, const Pen& pen = {});

    /// 描边正圆。
    void stroke_circle(math::Vec2 center, float radius, const Brush& brush, const Pen& pen = {}) {
        stroke_ellipse(center, {radius, radius}, brush, pen);
    }

    /// 描边线段（from → to）。
    void stroke_line(math::Vec2 from, math::Vec2 to, const Brush& brush, const Pen& pen = {});

    /// 描边任意路径。
    void stroke_path(const Path& path, const Brush& brush, const Pen& pen = {});

    // ── 完成录制 ────────────────────────────────────────────────────────

    /**
     * @brief 完成录制，返回不可变 DisplayList。
     *
     * 调用后 Canvas 恢复为空状态（命令缓冲和路径缓冲均清空）。
     * 变换/裁剪栈也被清空（未匹配的 save() 被丢弃）。
     */
    [[nodiscard]] DisplayList end();

    /// 当前录制的命令数量。
    [[nodiscard]] size_t cmd_count() const noexcept { return cmds_.size(); }

    /// 是否为空（无任何命令）。
    [[nodiscard]] bool empty() const noexcept { return cmds_.empty(); }

    /// 丢弃所有已录制的命令并重置状态（不生成 DisplayList）。
    void discard();

private:
    containers::Vector<DrawCmd> cmds_{};    ///< 正在录制的命令序列
    containers::Vector<Path>   paths_{};   ///< 路径数据（DrawCmd 通过 path_index 引用）

    /// 保存路径到内部数组，返回其索引（供 fill_path/stroke_path 使用）。
    uint32_t intern_path(const Path& path);

    /// 压入一条命令到录制缓冲。
    void push(DrawCmd cmd) { cmds_.push_back(std::move(cmd)); }
};

} // namespace mine::paint
