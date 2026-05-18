/**
 * @file Path.h
 * @brief 不可变几何路径类型。
 *
 * Path 表示一条由 MoveTo / LineTo / CubicTo / QuadTo / Close 等命令
 * 组成的不可变路径。通常由 PathBuilder 构建，完成后不可修改。
 *
 * 依赖：mine.containers（Vector）, mine.math（Vec2）
 */

#pragma once

#include <cstddef>
#include <cstdint>

#include <mine/containers/Vector.h>
#include <mine/core/Span.h>
#include <mine/math/Vec2.h>

namespace mine::paint {

/**
 * @brief 路径命令类型。
 */
enum class PathCmdKind : uint8_t {
    MoveTo,   ///< 移动到新起点（不绘制），使用 pt[0]
    LineTo,   ///< 直线连接到目标点，使用 pt[0]
    CubicTo,  ///< 三次贝塞尔曲线：控制点1=pt[0]，控制点2=pt[1]，终点=pt[2]
    QuadTo,   ///< 二次贝塞尔曲线：控制点=pt[0]，终点=pt[1]
    Close,    ///< 关闭当前子路径（连接回最近的 MoveTo 点）
};

/**
 * @brief 单条路径命令。
 *
 * 最多携带 3 个二维点（CubicTo 使用全部 3 个，其余命令只用到前面若干个）。
 * 结构体为 trivially copyable，可直接存入 Vector / 传递给 GPU。
 */
struct PathCmd {
    PathCmdKind kind{PathCmdKind::MoveTo};
    math::Vec2  pt[3]{};  ///< 端点/控制点（按 kind 使用不同数量）
};

/**
 * @brief 不可变几何路径。
 *
 * Path 是一个值类型，内部持有 PathCmd 序列。一旦由 PathBuilder::build()
 * 创建完成，路径内容不可修改。
 *
 * 生命周期注意：
 *   - Path 可以拷贝（深拷贝 PathCmd 数组）。
 *   - Path 可以移动（零分配转移）。
 *   - DisplayList 通过索引引用路径，需要 DisplayList 持有 Path 的所有权。
 */
class Path {
public:
    Path() = default;

    /**
     * @brief 从 PathCmd 序列构造（由 PathBuilder 内部调用）。
     * @param cmds  路径命令序列（移动语义）
     */
    explicit Path(containers::Vector<PathCmd> cmds) noexcept
        : cmds_(std::move(cmds)) {}

    // ── 访问器 ──────────────────────────────────────────────────────────

    /// 返回路径命令的只读视图。
    [[nodiscard]] core::Span<const PathCmd> cmds() const noexcept {
        return {cmds_.data(), cmds_.size()};
    }

    /// 路径命令数量。
    [[nodiscard]] size_t cmd_count() const noexcept { return cmds_.size(); }

    /// 路径是否为空（无任何命令）。
    [[nodiscard]] bool empty() const noexcept { return cmds_.empty(); }

private:
    containers::Vector<PathCmd> cmds_{};
};

} // namespace mine::paint
