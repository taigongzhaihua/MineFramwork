/**
 * @file ValuePriority.h
 * @brief 依赖属性值优先级枚举。
 *
 * 优先级从高到低决定 DependencyObject::get_value() 返回哪个有效值。
 * 设计遵循 WPF 的值源优先级，简化为当前需要的层次。
 */

#pragma once

#include <cstdint>

namespace mine::ui {

/**
 * @brief 依赖属性值来源优先级（数值越大优先级越高）。
 *
 * 优先级链（高 → 低）：
 *   Animation > Local > TemplateBind > StyleTrigger > StyleSetter > Inherited > Default
 *
 * 完整说明见 docs/20-style-template.md §20.5。
 */
enum class ValuePriority : uint8_t {
    /// 属性默认值（PropertyMetadata.default_value），最低优先级，不存入槽
    Default      = 0,
    /// 值继承：来自可视化树祖先（inherits = true 属性）
    Inherited    = 10,
    /// 样式 setter（Style::setters 直接赋值）
    StyleSetter  = 20,
    /// 样式触发器 / 视觉状态 setter（当前视觉状态激活的 setter）
    StyleTrigger = 30,
    /// 控件模板绑定（TemplateBinding）
    TemplateBind = 40,
    /// 本地值：代码直接调用 set_value() 或 MML 属性赋值
    Local        = 50,
    /// 动画值：动画系统写入，最高优先级
    Animation    = 60,
};

} // namespace mine::ui
