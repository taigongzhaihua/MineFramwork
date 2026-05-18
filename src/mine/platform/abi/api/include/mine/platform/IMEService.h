/**
 * @file IMEService.h
 * @brief 输入法（IME）服务接口。
 *
 * @note M0 阶段为占位接口，方法体以 MINE_TODO_NOT_IMPLEMENTED 标记。
 *       完整实现在 mine.ui.input 模块（L4）中与输入事件流对接。
 */

#pragma once

#include <mine/math/Rect.h>

namespace mine::platform {

/**
 * @brief 输入法服务接口，控制 IME 候选框位置与状态。
 */
class IMEService {
public:
    virtual ~IMEService() = default;

    /**
     * @brief 启用 IME 输入（接收组合输入事件）。
     * @param composition_rect 候选框的建议位置（逻辑像素，窗口坐标系）
     */
    virtual void enable(math::Rect composition_rect) = 0;

    /// 禁用 IME 输入（如纯英文输入框）
    virtual void disable() = 0;

    /// 当前 IME 是否处于启用状态
    [[nodiscard]] virtual bool is_enabled() const = 0;

    /// 更新候选框位置（光标移动时调用）
    virtual void set_composition_rect(math::Rect composition_rect) = 0;
};

} // namespace mine::platform
