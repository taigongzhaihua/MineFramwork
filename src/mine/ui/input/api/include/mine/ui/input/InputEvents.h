/**
 * @file InputEvents.h
 * @brief mine.ui.input 模块标准路由事件声明。
 *
 * 遵循 WPF 风格，每个输入事件提供一对隧道（Preview）和冒泡事件：
 *   - PreviewXxx（隧道）：从根部向目标传播，允许父节点提前拦截
 *   - Xxx（冒泡）：从目标向根部传播
 *
 * 事件在首次调用对应函数时注册（Meyer's 单例），此后返回稳定引用。
 */

#pragma once

#include <mine/ui/event/RoutedEvent.h>
#include <mine/ui/input/Api.h>

namespace mine::ui::input {

// ── 键盘事件 ─────────────────────────────────────────────────────────────────

/// Preview 键按下（隧道策略，根 → 目标）
MINE_UI_INPUT_API const RoutedEvent& PreviewKeyDownEvent();
/// 键按下（冒泡策略，目标 → 根）
MINE_UI_INPUT_API const RoutedEvent& KeyDownEvent();

/// Preview 键释放（隧道策略）
MINE_UI_INPUT_API const RoutedEvent& PreviewKeyUpEvent();
/// 键释放（冒泡策略）
MINE_UI_INPUT_API const RoutedEvent& KeyUpEvent();

// ── 鼠标事件 ─────────────────────────────────────────────────────────────────

/// Preview 鼠标按下（隧道策略）
MINE_UI_INPUT_API const RoutedEvent& PreviewMouseDownEvent();
/// 鼠标按下（冒泡策略）
MINE_UI_INPUT_API const RoutedEvent& MouseDownEvent();

/// Preview 鼠标释放（隧道策略）
MINE_UI_INPUT_API const RoutedEvent& PreviewMouseUpEvent();
/// 鼠标释放（冒泡策略）
MINE_UI_INPUT_API const RoutedEvent& MouseUpEvent();

/// 鼠标移动（冒泡策略；无 Preview 变体，直接派发）
MINE_UI_INPUT_API const RoutedEvent& MouseMoveEvent();

/// 鼠标滚轮（冒泡策略）
MINE_UI_INPUT_API const RoutedEvent& MouseWheelEvent();

} // namespace mine::ui::input
