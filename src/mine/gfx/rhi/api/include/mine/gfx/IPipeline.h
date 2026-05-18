/**
 * @file IPipeline.h
 * @brief 图形/计算管线状态对象接口（M0 阶段占位接口）。
 *
 * 完整设计将在 M0.3（mine.paint 绘制）阶段填充。
 * 当前仅提供基础接口，供 ICommandList::set_pipeline() 使用。
 */

#pragma once

#include "GfxTypes.h"

namespace mine::gfx {

/// 管线类型（图形或计算）
enum class PipelineType : int {
    Graphics, ///< 含顶点着色器、像素着色器的图形管线
    Compute,  ///< 仅含计算着色器的计算管线
};

/**
 * @brief 预编译管线状态对象接口。
 *
 * 管线对象封装着色器程序、光栅化状态、混合状态、深度/模板状态等，
 * 创建开销较高，应在初始化阶段预创建并复用。
 *
 * @note M0 阶段仅声明接口，具体描述符和创建方法将在 M0.3 中补充。
 */
class IPipeline {
public:
    virtual ~IPipeline() = default;

    /// 返回管线类型
    [[nodiscard]] virtual PipelineType type() const noexcept = 0;
};

} // namespace mine::gfx
