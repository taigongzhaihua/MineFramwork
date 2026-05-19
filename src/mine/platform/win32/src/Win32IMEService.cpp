/**
 * @file Win32IMEService.cpp
 * @brief Win32IMEService 实现——基于 Windows IMM32 API。
 */

#include "Win32IMEService.h"

namespace mine::platform::win32 {

// ── 内部接口 ────────────────────────────────────────────────────────────────

void Win32IMEService::on_focus(HWND hwnd) noexcept {
    active_hwnd_ = hwnd;

    if (enabled_) {
        // 恢复 IME 关联：使用默认 IME 上下文重新绑定
        ImmAssociateContextEx(hwnd, nullptr, IACE_DEFAULT);
        // 恢复候选框位置
        apply_composition_rect(composition_rect_);
    } else {
        // 维持禁用状态：解除新窗口的 IME 关联
        ImmAssociateContext(hwnd, nullptr);
    }
}

void Win32IMEService::on_blur() noexcept {
    active_hwnd_ = nullptr;
}

// ── IMEService 接口 ──────────────────────────────────────────────────────────

void Win32IMEService::enable(math::Rect composition_rect) {
    if (!active_hwnd_) {
        return;
    }
    enabled_         = true;
    composition_rect_ = composition_rect;

    // 重新关联默认 IME 上下文（若之前通过 disable() 解除了关联则恢复）
    ImmAssociateContextEx(active_hwnd_, nullptr, IACE_DEFAULT);

    // 定位候选框
    apply_composition_rect(composition_rect);
}

void Win32IMEService::disable() {
    if (!active_hwnd_) {
        return;
    }
    enabled_ = false;

    // 解除 IME 上下文关联：窗口不再接收组合输入
    ImmAssociateContext(active_hwnd_, nullptr);
}

void Win32IMEService::set_composition_rect(math::Rect composition_rect) {
    composition_rect_ = composition_rect;

    if (!active_hwnd_ || !enabled_) {
        return;
    }
    apply_composition_rect(composition_rect);
}

// ── 私有实现 ────────────────────────────────────────────────────────────────

void Win32IMEService::apply_composition_rect(math::Rect rect) {
    HIMC himc = ImmGetContext(active_hwnd_);
    if (!himc) {
        return;
    }

    // 候选窗口位置：显示在输入区域下方（y + height）
    CANDIDATEFORM cand_form{};
    cand_form.dwIndex        = 0;
    cand_form.dwStyle        = CFS_CANDIDATEPOS;
    cand_form.ptCurrentPos.x = static_cast<LONG>(rect.x);
    cand_form.ptCurrentPos.y = static_cast<LONG>(rect.y + rect.height);
    ImmSetCandidateWindow(himc, &cand_form);

    // 组合字符串起始位置：与输入光标对齐
    COMPOSITIONFORM comp_form{};
    comp_form.dwStyle        = CFS_POINT;
    comp_form.ptCurrentPos.x = static_cast<LONG>(rect.x);
    comp_form.ptCurrentPos.y = static_cast<LONG>(rect.y);
    ImmSetCompositionWindow(himc, &comp_form);

    ImmReleaseContext(active_hwnd_, himc);
}

} // namespace mine::platform::win32
