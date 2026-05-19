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
 *   - 拥有文字段落：TextRun 序列由 DisplayList 持有，DrawText 命令通过 path_index 引用。
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
 * @brief 文字渲染段落（Text Run）。
 *
 * 存储一段连续文字的 UTF-8 内容及其渲染参数。
 * 字符串以 inline 缓冲区存储（最大 512 字节），不进行动态分配。
 *
 * 坐标约定：
 *   origin 为基线起始点，Y 轴向下（与屏幕坐标系一致）。
 *
 * 注意：font_face 指针指向外部持有的 mine::text::FontFace 对象，
 * DisplayList 不拥有字体面，调用方须保证字体面生命周期长于 DisplayList。
 */
struct TextRun {
    /// UTF-8 字符串的最大存储字节数（含终止符）
    static constexpr uint32_t kMaxUtf8Bytes = 512;

    char     utf8[kMaxUtf8Bytes]{};  ///< UTF-8 编码字符串（inline 缓冲，以 '\0' 结尾）
    uint32_t length{0};              ///< 实际字节数（不含终止符）
    void*    font_face{nullptr};     ///< FontFace* 不透明指针（调用方持有，不可为 nullptr）
    float    size_px{0.0f};          ///< 字号（像素）
    float    origin_x{0.0f};         ///< 基线起始点 X（屏幕坐标，像素）
    float    origin_y{0.0f};         ///< 基线起始点 Y（屏幕坐标，像素，向下为正）
};

/**
 * @brief 不可变绘制命令序列。
 *
 * 由 Canvas::end() 创建，之后只读。
 * 包含三个数组：
 *   1. DrawCmd 序列：描述绘制操作
 *   2. Path 序列：路径数据（被 DrawCmd 通过 path_index 引用）
 *   3. TextRun 序列：文字段落（被 DrawText 命令通过 path_index 引用）
 */
class DisplayList {
public:
    DisplayList() = default;

    /**
     * @brief 由命令、路径和文字段落数组构造（供 Canvas::end() 内部调用）。
     * @param cmds       绘制命令序列（移动）
     * @param paths      路径数据序列（移动）
     * @param text_runs  文字段落序列（移动）
     */
    DisplayList(
        containers::Vector<DrawCmd>  cmds,
        containers::Vector<Path>     paths,
        containers::Vector<TextRun>  text_runs) noexcept
        : cmds_      (std::move(cmds))
        , paths_     (std::move(paths))
        , text_runs_ (std::move(text_runs))
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

    /// 返回文字段落的只读视图。
    [[nodiscard]] core::Span<const TextRun> text_runs() const noexcept {
        return {text_runs_.data(), text_runs_.size()};
    }

    /// 命令数量。
    [[nodiscard]] size_t cmd_count() const noexcept { return cmds_.size(); }

    /// 路径数量。
    [[nodiscard]] size_t path_count() const noexcept { return paths_.size(); }

    /// 文字段落数量。
    [[nodiscard]] size_t text_run_count() const noexcept { return text_runs_.size(); }

    /// DisplayList 是否为空（无任何命令）。
    [[nodiscard]] bool empty() const noexcept { return cmds_.empty(); }

private:
    containers::Vector<DrawCmd>  cmds_{};        ///< 绘制命令序列
    containers::Vector<Path>     paths_{};       ///< 路径数据（通过 path_index 引用）
    containers::Vector<TextRun>  text_runs_{};   ///< 文字段落（DrawText 命令通过 path_index 引用）
};

} // namespace mine::paint
