/**
 * @file DisplayList.h
 * @brief 不可变绘制命令序列。
 *
 * DisplayList 是合成器与渲染器之间的稳定边界：
 *   - 合成器（Canvas/Compositor）生成 DisplayList
 *   - 渲染器（IRenderer）消费 DisplayList
 *
 * 设计约定：
 *   - 不可变：一旦由 Canvas::end() 创建，内容不可修改。
 *   - 可移动：零分配转移（Move 语义）。
 *   - 拥有路径数据：Path 序列由 DisplayList 持有，DrawCmd 通过 path_index 引用。
 *
 * 依赖：mine.paint（DrawCmd、Path），mine.containers（Vector），mine.core（Span）
 */

#pragma once

#include <mine/paint/DrawCmd.h>
#include <mine/paint/Path.h>
#include <mine/containers/Vector.h>
#include <mine/core/Span.h>

namespace mine::paint {

/**
 * @brief 不可变绘制命令序列。
 *
 * 由 Canvas::end() 创建，之后只读。
 * 包含两个数组：
 *   1. DrawCmd 序列：描述绘制操作
 *   2. Path 序列：路径数据（被 DrawCmd 通过 path_index 引用）
 */
class DisplayList {
public:
    DisplayList() = default;

    /**
     * @brief 由命令和路径数组构造（供 Canvas::end() 内部调用）。
     * @param cmds  绘制命令序列（移动）
     * @param paths 路径数据序列（移动）
     */
    DisplayList(
        containers::Vector<DrawCmd> cmds,
        containers::Vector<Path>   paths) noexcept
        : cmds_ (std::move(cmds))
        , paths_(std::move(paths))
    {}

    // 不可拷贝，允许移动（保持不可变语义）
    DisplayList(const DisplayList&)            = delete;
    DisplayList& operator=(const DisplayList&) = delete;
    DisplayList(DisplayList&&)                 = default;
    DisplayList& operator=(DisplayList&&)      = default;

    // ── 访问器 ──────────────────────────────────────────────────────────

    /// 返回绘制命令的只读视图。
    [[nodiscard]] core::Span<const DrawCmd> cmds() const noexcept {
        return {cmds_.data(), cmds_.size()};
    }

    /// 返回路径数据的只读视图。
    [[nodiscard]] core::Span<const Path> paths() const noexcept {
        return {paths_.data(), paths_.size()};
    }

    /// 命令数量。
    [[nodiscard]] size_t cmd_count() const noexcept { return cmds_.size(); }

    /// 路径数量。
    [[nodiscard]] size_t path_count() const noexcept { return paths_.size(); }

    /// DisplayList 是否为空（无任何命令）。
    [[nodiscard]] bool empty() const noexcept { return cmds_.empty(); }

private:
    containers::Vector<DrawCmd> cmds_{};   ///< 绘制命令序列
    containers::Vector<Path>   paths_{};  ///< 路径数据（通过 path_index 引用）
};

} // namespace mine::paint
