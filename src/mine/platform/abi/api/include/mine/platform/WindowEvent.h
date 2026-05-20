/**
 * @file WindowEvent.h
 * @brief 窗口事件类型定义。
 *
 * WindowEvent 采用带标签的扁平结构体：所有字段共存，
 * 调用方根据 kind 字段判断哪些有效，无需 union 或 std::variant。
 */

#pragma once

#include <mine/math/Size.h>
#include <mine/math/Point.h>
#include <mine/math/Rect.h>

namespace mine::platform {

/**
 * @brief 窗口事件类型。
 */
enum class WindowEventKind : int {
    Resized,                ///< 窗口客户区尺寸发生改变
    Moved,                  ///< 窗口位置发生改变
    Closing,                ///< 用户请求关闭（可通过 cancel = true 阻止）
    Closed,                 ///< 窗口已销毁（此后不得再调用 IWindow 方法）
    FocusGained,            ///< 窗口获得键盘焦点
    FocusLost,              ///< 窗口失去键盘焦点
    DpiChanged,             ///< 每英寸像素数改变（per-monitor DPI v2）
    Activated,              ///< 窗口切换为前台激活状态
    Deactivated,            ///< 窗口切换为后台非激活状态

    // ── IME 输入法事件 ────────────────────────────────────────────────────
    ImeCompositionStarted,  ///< IME 组合输入开始（WM_IME_STARTCOMPOSITION）
    ImeCompositionChanged,  ///< IME 组合字符串更新，ime_text_utf8 含当前预编辑文字
    ImeCompositionCommitted,///< IME 确认提交，ime_text_utf8 含最终提交的文字
    ImeCompositionEnded,    ///< IME 组合输入结束（WM_IME_ENDCOMPOSITION）

    // ── 键盘输入事件 ──────────────────────────────────────────────────────
    KeyDown,                ///< 键盘按键按下（含重复）
    KeyUp,                  ///< 键盘按键释放
    Char,                   ///< 字符输入（已处理修饰键的可打印字符）

    // ── 鼠标输入事件 ──────────────────────────────────────────────────────
    MouseMove,              ///< 鼠标在客户区移动
    MouseDown,              ///< 鼠标按键按下
    MouseUp,                ///< 鼠标按键释放
    MouseWheel,             ///< 鼠标滚轮滚动
};

/**
 * @brief 窗口事件数据，扁平结构。
 *
 * 根据 kind 字段读取对应数据字段：
 *   Resized                → new_size
 *   Moved                  → new_position
 *   DpiChanged             → new_dpi, suggested_rect（系统建议的新窗口矩形）
 *   Closing                → 可置 cancel = true 阻止关闭
 *   ImeCompositionChanged  → ime_text_utf8, ime_text_length（当前预编辑文字）
 *   ImeCompositionCommitted→ ime_text_utf8, ime_text_length（最终提交文字）
 *   其余字段均为零值，可忽略。
 */
struct WindowEvent {
    WindowEventKind kind{WindowEventKind::Closed};

    // Resized 时有效：客户区新尺寸（逻辑像素）
    math::Size new_size{};

    // Moved 时有效：屏幕坐标新位置（逻辑像素）
    math::Point new_position{};

    // DpiChanged 时有效
    float      new_dpi{96.0f};              ///< 新 DPI 值
    math::Rect suggested_rect{};            ///< 系统建议的新窗口矩形（屏幕坐标）

    // Closing 时可设置为 true 以取消关闭操作
    mutable bool cancel{false};

    // ImeCompositionChanged / ImeCompositionCommitted 时有效
    char     ime_text_utf8[256]{};          ///< 组合文字或提交文字（UTF-8 编码，以 \0 结尾）
    uint32_t ime_text_length{0};            ///< ime_text_utf8 中的有效字节数（不含末尾 \0）

    // ── 键盘事件字段（KeyDown / KeyUp / Char 时有效）─────────────────────────

    /// 平台虚拟按键码（Windows: VIRTUAL-KEY code；其他平台类似）
    uint32_t key_vk_code{0};

    /// 硬件扫描码（与平台相关，Mine.ui.input 转换为平台无关 Key 枚举）
    uint32_t key_scan_code{0};

    /// Char 事件的 UTF-32 字符码点（仅 kind==Char 时有效）
    uint32_t char_utf32{0};

    /// 键盘事件是否为自动重复（按住按键产生的重复按下消息）
    bool key_is_repeat{false};

    // ── 鼠标事件字段（MouseMove / MouseDown / MouseUp / MouseWheel 时有效）───

    /// 鼠标位于窗口客户区的 X 坐标（逻辑像素）
    float mouse_x{0.0f};

    /// 鼠标位于窗口客户区的 Y 坐标（逻辑像素）
    float mouse_y{0.0f};

    /// 触发事件的鼠标按键（0=左键, 1=右键, 2=中键, 3=X1, 4=X2）
    /// MouseMove 时为 0，MouseWheel 时为 0
    uint8_t mouse_button{0};

    /// 滚轮滚动量（正值向上，负值向下；一格通常为 120.0f）
    /// 仅 kind==MouseWheel 时有效
    float mouse_wheel_delta{0.0f};

    // ── 修饰键状态（所有输入事件时有效）─────────────────────────────────────

    bool mod_ctrl{false};   ///< Ctrl 键当前是否按下
    bool mod_shift{false};  ///< Shift 键当前是否按下
    bool mod_alt{false};    ///< Alt 键当前是否按下
};

} // namespace mine::platform
