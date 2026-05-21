/**
 * @file Control.cpp
 * @brief Control 控件基类实现。
 *
 * M1.1 阶段 Control 仅作为继承链节点，不添加额外逻辑。
 * 后续里程碑（mine.ui.style）将在此添加样式属性与控件模板支持。
 */

#include <mine/ui/visual/Control.h>

namespace mine::ui {

Control::Control()  = default;
Control::~Control() = default;

void Control::set_style_slot(core::StringView slot)
{
	style_slot_ = slot;
}

core::StringView Control::style_slot() const noexcept
{
	return core::StringView{ style_slot_.data(), style_slot_.size() };
}

void Control::set_template_slot(core::StringView slot)
{
	template_slot_ = slot;
}

core::StringView Control::template_slot() const noexcept
{
	return core::StringView{ template_slot_.data(), template_slot_.size() };
}

ControlVisualState Control::visual_state() const noexcept
{
	return visual_state_;
}

void Control::update_visual_state()
{
	const ControlVisualState next = compute_visual_state();
	if (next == visual_state_) {
		return;
	}
	const ControlVisualState old = visual_state_;
	visual_state_ = next;
	on_visual_state_changed(old, next);
}

ControlVisualState Control::compute_visual_state() const
{
	return ControlVisualState::Normal;
}

void Control::on_visual_state_changed(ControlVisualState /*old_state*/,
									  ControlVisualState /*new_state*/)
{
	// 状态变化至少触发一次重绘，具体样式映射在 mine.ui.style 阶段接入。
	invalidate_render();
}

} // namespace mine::ui
