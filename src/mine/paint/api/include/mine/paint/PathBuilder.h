/**
 * @file PathBuilder.h
 * @brief 命令式路径构造器。
 *
 * PathBuilder 以流式 API 接收路径命令，最终调用 build() 生成不可变 Path。
 *
 * 使用示例：
 * @code
 *   using namespace mine;
 *   Path p = PathBuilder{}
 *       .move_to({10.f, 10.f})
 *       .line_to({100.f, 10.f})
 *       .line_to({100.f, 100.f})
 *       .close()
 *       .build();
 * @endcode
 *
 * 依赖：mine.paint（Path）, mine.math（Vec2、Rect、RoundedRect）
 */

#pragma once

#include <mine/paint/Path.h>
#include <mine/math/Rect.h>
#include <mine/math/RoundedRect.h>

namespace mine::paint {

/**
 * @brief 几何路径构造器。
 *
 * 内部维护一个可增长的 PathCmd 序列。
 * 调用 build() 后，命令序列被移走，构造器恢复为空状态。
 */
class PathBuilder {
public:
    PathBuilder()  = default;
    ~PathBuilder() = default;

    // 禁止拷贝，允许移动
    PathBuilder(const PathBuilder&)            = delete;
    PathBuilder& operator=(const PathBuilder&) = delete;
    PathBuilder(PathBuilder&&)                 = default;
    PathBuilder& operator=(PathBuilder&&)      = default;

    // ── 基础路径命令 ───────────────────────────────────────────────────

    /// 移动画笔到 pt，不绘制线条。开始新的子路径。
    PathBuilder& move_to(math::Vec2 pt);

    /// 从当前点画直线到 pt。
    PathBuilder& line_to(math::Vec2 pt);

    /**
     * @brief 三次贝塞尔曲线：当前点 → c1 → c2 → end。
     * @param c1   第一控制点
     * @param c2   第二控制点
     * @param end  曲线终点
     */
    PathBuilder& cubic_to(math::Vec2 c1, math::Vec2 c2, math::Vec2 end);

    /**
     * @brief 二次贝塞尔曲线：当前点 → ctrl → end。
     * @param ctrl 控制点
     * @param end  曲线终点
     */
    PathBuilder& quad_to(math::Vec2 ctrl, math::Vec2 end);

    /// 关闭当前子路径（用直线连回最近的 MoveTo 点）。
    PathBuilder& close();

    // ── 便捷几何命令 ───────────────────────────────────────────────────

    /**
     * @brief 添加一个矩形（顺时针，从左上角开始）。
     *
     * 等价于 move_to → line_to × 3 → close。
     */
    PathBuilder& add_rect(math::Rect rect);

    /**
     * @brief 添加一个圆角矩形。
     *
     * M0 阶段将圆角近似为三次贝塞尔曲线（κ ≈ 0.5523）。
     */
    PathBuilder& add_rounded_rect(math::RoundedRect rrect);

    /**
     * @brief 添加一个椭圆。
     * @param center 椭圆中心
     * @param radii  X 轴半径（radii.x）和 Y 轴半径（radii.y）
     *
     * 用 4 段三次贝塞尔曲线近似椭圆（κ ≈ 0.5523）。
     */
    PathBuilder& add_ellipse(math::Vec2 center, math::Vec2 radii);

    // ── 完成构建 ────────────────────────────────────────────────────────

    /**
     * @brief 完成路径构建，返回不可变 Path。
     *
     * 内部命令序列被移走，构造器恢复为空状态，可继续复用。
     */
    [[nodiscard]] Path build();

    /// 当前是否为空（无任何命令）。
    [[nodiscard]] bool empty() const noexcept { return cmds_.empty(); }

    /// 重置构造器，清空所有命令。
    void reset() { cmds_.clear(); }

private:
    containers::Vector<PathCmd> cmds_{};

    /// 记录一条命令（内部辅助）。
    void push(PathCmd cmd) { cmds_.push_back(cmd); }
};

} // namespace mine::paint
