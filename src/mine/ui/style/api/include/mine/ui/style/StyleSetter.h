#pragma once

#include <mine/ui/style/Api.h>
#include <mine/core/Variant.h>
#include <mine/containers/InlineString.h>
#include <mine/containers/SmallVector.h>

namespace mine::ui {
// 前置声明，避免引入重量级头文件
class DependencyProperty;
}  // namespace mine::ui

namespace mine::ui::style {

/**
 * @brief 单个依赖属性设置器。
 *
 * StyleSetter 描述"将某个依赖属性设置为指定值"的操作。
 *
 * 两种模式（互斥）：
 *   - 静态值（res_key 为空）：直接将 value 写入目标属性
 *   - DynamicResource（res_key 非空）：在运行时从资源字典查找 res_key 对应的值，
 *     并订阅其变更（由 mmlc 生成的 apply_fn_ 处理；程序化路径暂不实现）
 */
struct MINE_UI_STYLE_API StyleSetter {
    /// 目标依赖属性（非空；指向全局唯一注册实例）
    const ui::DependencyProperty* property{nullptr};
    /// 静态值（res_key 为空时生效）
    core::Variant                 value;
    /// DynamicResource 键（非空时优先于 value；典型长度 < 24 字节）
    containers::InlineString      res_key;
};

/**
 * @brief 单个视觉状态下的属性设置器集合。
 *
 * 由 Style 在 apply_state() 时查找并应用到目标元素（StyleTrigger 优先级）。
 */
struct MINE_UI_STYLE_API VisualStateSetters {
    /// 视觉状态名（如 "Normal"、"Hovered"、"Pressed"、"Focused"、"Disabled"）
    containers::InlineString           state_name;
    /// 该状态下需要应用的属性 setter 列表（典型 2-8 个）
    containers::SmallVector<StyleSetter, 8> setters;
};

}  // namespace mine::ui::style
