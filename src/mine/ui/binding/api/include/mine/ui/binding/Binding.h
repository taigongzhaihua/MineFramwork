/**
 * @file Binding.h
 * @brief mine.ui.binding 模块的伞形头文件，包含所有公共类型。
 *
 * 用户只需引入此单一头文件即可使用绑定模块的全部功能：
 *   - BindingMode（绑定方向）
 *   - IConverter（值转换器接口）
 *   - INotifyPropertyChanged（属性变更通知接口）
 *   - PropertyDependency（依赖源描述符）
 *   - BindingExpression（运行时绑定表达式）
 */

#pragma once

#include <mine/ui/binding/Api.h>
#include <mine/ui/binding/BindingExpression.h>
#include <mine/ui/binding/BindingMode.h>
#include <mine/ui/binding/IConverter.h>
#include <mine/ui/binding/INotifyPropertyChanged.h>
#include <mine/ui/binding/ModuleTag.h>
#include <mine/ui/binding/PropertyDependency.h>
