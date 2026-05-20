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

} // namespace mine::ui
