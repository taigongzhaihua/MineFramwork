/**
 * @file ControlTemplate.cpp
 * @brief ControlTemplate 控件模板实现。
 */

#include <mine/ui/style/ControlTemplate.h>
#include <mine/ui/property/DependencyObject.h>

namespace mine::ui::style {

// ============================================================================
// 操作
// ============================================================================

void ControlTemplate::build(ui::DependencyObject& target) const
{
    // build_fn_ 为 nullptr 时为空操作（模板未配置）
    if (build_fn_) {
        build_fn_(target);
    }
}

// ============================================================================
// 构建器接口
// ============================================================================

ControlTemplate& ControlTemplate::set_name(core::StringView name)
{
    // 使用 StringView 赋值运算符，内部调用 assign() 而非 placement new
    name_ = name;
    return *this;
}

ControlTemplate& ControlTemplate::set_target_type(core::TypeId type_id) noexcept
{
    target_type_ = type_id;
    return *this;
}

}  // namespace mine::ui::style
