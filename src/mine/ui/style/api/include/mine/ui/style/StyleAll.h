#pragma once

/// mine.ui.style 模块伞形头文件。
/// 包含本模块所有公开 API。
/// 注意：ControlTemplate / TemplateRegistry 已于架构重构中移除（2026-06），
/// 控件外观定制请改用 Style + 继承重写 on_render()。

#include <mine/ui/style/Api.h>
#include <mine/ui/style/HandlerId.h>
#include <mine/ui/style/ResourceDictionary.h>
#include <mine/ui/style/StyleSetter.h>
#include <mine/ui/style/Style.h>
#include <mine/ui/style/VisualStateManager.h>
