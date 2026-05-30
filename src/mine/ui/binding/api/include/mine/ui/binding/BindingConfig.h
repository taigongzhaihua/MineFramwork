/**
 * @file BindingConfig.h
 * @brief Binding 描述符 —— 等价于 WPF 的 new Binding("PropName") { ... }。
 *
 * 轻量聚合结构体，不拥有任何资源，用于向 FrameworkElement::set_binding() /
 * BindingExpression::bind() 传递完整的绑定配置（属性名、方向、转换器、回退值）。
 *
 * 用法（C++20 指定初始化语法）：
 * @code
 *   // 有值转换器：将字节数格式化为人类可读字符串
 *   count_label_.set_binding(TextBlock::TextProperty, ui::Binding{
 *       .prop_name  = "byte_count",
 *       .converter  = &bytes_to_human_readable,
 *       .conv_param = "MB",
 *   });
 *
 *   // 带回退值：当属性为空时显示占位符
 *   name_label_.set_binding(TextBlock::TextProperty, ui::Binding{
 *       .prop_name = "user_name",
 *       .fallback  = core::Variant{ "未知用户" },
 *   });
 * @endcode
 */

#pragma once

#include <mine/ui/binding/Api.h>
#include <mine/ui/binding/BindingMode.h>
#include <mine/core/StringView.h>
#include <mine/core/Variant.h>

namespace mine::ui {

struct IConverter;

/**
 * @brief 绑定描述符（轻量配置对象，不拥有资源）。
 *
 * 生命周期约定（与 BindingExpression 字段约定一致）：
 *   - `prop_name` / `conv_param` 须为字符串字面量或长期存活的字符串
 *   - `converter` 裸指针，调用方须保证其生命周期覆盖对应控件
 *   - `fallback`  以值形式被 bind() 内部复制，描述符本身析构后仍安全
 *   - 描述符本身在 set_binding() / bind() 返回后即可销毁
 */
struct MINE_UI_BINDING_API Binding {
    /// 源属性名（须与 MINE_OBSERVABLE 宏名称完全一致）—— 必填
    core::StringView prop_name;

    /// 绑定方向，默认 OneWay
    BindingMode mode = BindingMode::OneWay;

    /// 值转换器（可选；调用方负责其生命周期须覆盖绑定存活期）
    IConverter* converter = nullptr;

    /// 传递给 converter.convert() 的参数字符串（须为字符串字面量或长期存活的字符串）
    core::StringView conv_param;

    /// 回退值：当 getter 或 converter 返回空 Variant 时写入目标属性
    core::Variant fallback;
};

} // namespace mine::ui
