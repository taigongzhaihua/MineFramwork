/**
 * @file Win32IMEService.h
 * @brief IMEService 的 Win32 实现——基于 Windows IMM32 API。
 *
 * 通过 ImmGetContext / ImmSetCandidateWindow / ImmSetCompositionWindow 等
 * 函数向系统 IME 传递候选框位置；通过 ImmAssociateContext 启用或禁用 IME。
 *
 * 依赖：imm32.lib（已在 xmake.lua 中通过 add_syslinks 添加）。
 */

#pragma once

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <imm.h>

#include <mine/platform/IMEService.h>

namespace mine::platform::win32 {

/**
 * @brief Win32 输入法服务实现。
 *
 * 每个进程只有一个实例（由 Win32ApplicationHostImpl 持有）。
 * 由 Win32Window 在 WM_SETFOCUS / WM_KILLFOCUS 时通过
 * on_focus() / on_blur() 更新当前活跃 HWND，从而使 enable()、
 * disable()、set_composition_rect() 能够定位到正确的窗口上下文。
 */
class Win32IMEService final : public IMEService {
public:
    Win32IMEService()  = default;
    ~Win32IMEService() override = default;

    // 禁止拷贝/移动（单实例，由 ApplicationHostImpl 持有）
    Win32IMEService(const Win32IMEService&)            = delete;
    Win32IMEService& operator=(const Win32IMEService&) = delete;

    // ── 内部接口（仅 Win32Window 调用）──────────────────────────────────────

    /**
     * @brief 通知某窗口获得焦点，更新活跃 HWND。
     *
     * 如果 IME 此前已通过 enable() 启用，则在新窗口上恢复关联；
     * 如果已通过 disable() 禁用，则在新窗口上也保持禁用。
     */
    void on_focus(HWND hwnd) noexcept;

    /**
     * @brief 通知某窗口失去焦点，清除活跃 HWND。
     */
    void on_blur() noexcept;

    // ── IMEService 接口 ────────────────────────────────────────────────────

    /**
     * @brief 启用 IME，并将候选框定位到指定区域（物理像素坐标）。
     *
     * 若当前无焦点窗口（active_hwnd_ == nullptr），则为空操作。
     */
    void enable(math::Rect composition_rect) override;

    /**
     * @brief 禁用 IME（解除当前窗口与 IME 上下文的关联）。
     *
     * 输入法图标仍会显示，但击键不再产生组合输入。
     */
    void disable() override;

    /**
     * @brief 返回当前 IME 是否处于启用状态。
     */
    [[nodiscard]] bool is_enabled() const override { return enabled_; }

    /**
     * @brief 更新 IME 候选框（和组合字符串窗口）的显示位置。
     *
     * composition_rect 使用物理像素坐标，原点为窗口客户区左上角。
     * 候选列表显示在矩形底部（y + height）。
     */
    void set_composition_rect(math::Rect composition_rect) override;

private:
    /**
     * @brief 将候选框和组合字符串窗口位置应用到 IME 上下文（内部实现）。
     *
     * 需要在 active_hwnd_ 非空时调用。
     */
    void apply_composition_rect(math::Rect rect);

private:
    HWND       active_hwnd_{nullptr};   ///< 当前获得焦点的窗口句柄
    bool       enabled_{true};           ///< 是否已通过 enable() 启用（默认开启，支持 IME 中文输入）
    math::Rect composition_rect_{};      ///< 当前候选框区域（物理像素）
};

} // namespace mine::platform::win32
