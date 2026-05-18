/**
 * @file BorderWidths.h
 * @brief 矩形四边独立描边宽度结构体。
 *
 * 用于 Canvas::stroke_bordered_rect()，支持每条边独立设置描边厚度，
 * 类似 CSS 的 border-top / border-right / border-bottom / border-left。
 * 描边方向为内侧（向矩形内部延伸，不超出矩形外边界）。
 */

#pragma once

namespace mine::paint {

/**
 * @brief 矩形四边独立描边宽度（逻辑像素）。
 *
 * 描边方向为内侧：每条边的描边宽度从矩形边界向矩形内部延伸，
 * 不会超出矩形的外轮廓。
 *
 * 使用示例：
 * @code
 *   // 四边等宽
 *   canvas.stroke_bordered_rect(rect, brush, BorderWidths::all(4.0f));
 *
 *   // 仅上下边（横向边框）
 *   canvas.stroke_bordered_rect(rect, brush, BorderWidths{.top=2.f, .bottom=2.f});
 *
 *   // 左粗右细
 *   canvas.stroke_bordered_rect(rect, brush, {8.0f, 2.0f, 8.0f, 2.0f});
 * @endcode
 */
struct BorderWidths {
    float top    = 0.0f;  ///< 上边描边宽度（像素，向下延伸）
    float right  = 0.0f;  ///< 右边描边宽度（像素，向左延伸）
    float bottom = 0.0f;  ///< 下边描边宽度（像素，向上延伸）
    float left   = 0.0f;  ///< 左边描边宽度（像素，向右延伸）

    /// 四边等宽工厂方法。
    [[nodiscard]] static BorderWidths all(float w) noexcept {
        return {w, w, w, w};
    }

    /// 垂直方向（上下）和水平方向（左右）分别指定。
    [[nodiscard]] static BorderWidths axes(float vertical, float horizontal) noexcept {
        return {vertical, horizontal, vertical, horizontal};
    }
};

} // namespace mine::paint
