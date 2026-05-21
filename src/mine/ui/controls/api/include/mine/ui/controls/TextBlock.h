/**
 * @file TextBlock.h
 * @brief TextBlock —— 基础文本展示控件（M1.1 最小可用实现）。
 */

#pragma once

#include <mine/ui/controls/Api.h>
#include <mine/ui/visual/Control.h>
#include <mine/containers/InlineString.h>
#include <mine/math/Color.h>
#include <mine/math/Thickness.h>

namespace mine::ui {

/**
 * @brief 只读文本控件。
 *
 * 当前阶段提供：
 * 1. 文本内容与基础测量
 * 2. 背景色和前景色
 * 3. 样式/模板槽位与视觉状态挂点（继承自 Control）
 */
class MINE_UI_CONTROLS_API TextBlock : public Control {
public:
    TextBlock();
    ~TextBlock() override;

    TextBlock(const TextBlock&)            = delete;
    TextBlock& operator=(const TextBlock&) = delete;
    TextBlock(TextBlock&&)                 = default;
    TextBlock& operator=(TextBlock&&)      = default;

    [[nodiscard]] core::StringView text() const noexcept;
    void set_text(core::StringView text);

    [[nodiscard]] float font_size() const noexcept;
    void set_font_size(float size_px);

    [[nodiscard]] math::Color foreground() const noexcept;
    void set_foreground(math::Color color);

    [[nodiscard]] math::Color background() const noexcept;
    void set_background(math::Color color);

    [[nodiscard]] math::Thickness padding() const noexcept;
    void set_padding(math::Thickness padding);

    /**
     * @brief 设置绘制文字所用字体对象（由外部持有生命周期）。
     */
    void set_font_face(void* font_face) noexcept;

protected:
    void on_measure(math::Size available_size) override;
    void on_render(paint::Canvas& canvas) override;

private:
    containers::InlineString text_;
    float                    font_size_px_ = 14.0f;
    math::Color              foreground_   = math::Color::Black;
    math::Color              background_   = math::Color::Transparent;
    math::Thickness          padding_      = math::Thickness::symmetric(4.0f, 2.0f);
    void*                    font_face_    = nullptr;
};

} // namespace mine::ui
