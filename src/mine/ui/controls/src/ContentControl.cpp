/**
 * @file ContentControl.cpp
 * @brief ContentControl 实现 —— 单内容控件基类（任务 17.1）。
 */

#include <mine/ui/controls/ContentControl.h>
#include <mine/ui/property/PropertyMetadata.h>
#include <mine/containers/InlineString.h>
#include <mine/core/Variant.h>

namespace mine::ui {

// ============================================================================
// 依赖属性注册
// ============================================================================

// ContentControl::ContentProperty
// 值类型：InlineString（文字内容）或 UIElement*（元素内容）；默认空值。
// affects_measure / affects_render：内容变更自动触发 invalidate_measure + invalidate_render。
// changed：转发到虚函数 on_content_changed，子类（如 Button）可重写同步内部缓存。
const DependencyProperty& ContentControl::ContentProperty =
    register_property<ContentControl>(
        "Content",
        core::Variant{},
        PropertyMetadata{
            .affects_measure = true,
            .affects_render  = true,
            .changed         = &ContentControl::s_on_content_changed,
        });

// ============================================================================
// DependencyProperty 变更回调（静态 → 虚方法转发）
// ============================================================================

void ContentControl::s_on_content_changed(DependencyObject*         sender,
                                           const DependencyProperty& /*prop*/,
                                           const core::Variant&      old_v,
                                           const core::Variant&      new_v) noexcept
{
    // 通过多态分派，允许子类（Button 等）重写 on_content_changed 以同步私有缓存
    static_cast<ContentControl*>(sender)->on_content_changed(old_v, new_v);
}

// ============================================================================
// 生命周期
// ============================================================================

ContentControl::ContentControl()  = default;
ContentControl::~ContentControl() = default;

// ============================================================================
// 标准内容接口
// ============================================================================

void ContentControl::set_content(UIElement* element)
{
    // 写入 ContentProperty Local 槽；affects_measure/affects_render 自动触发失效
    if (element != nullptr) {
        set_value(ContentProperty, core::Variant{ element });
    } else {
        // nullptr：清空内容（写入空 Variant）
        set_value(ContentProperty, core::Variant{});
    }
}

void ContentControl::set_content(core::StringView text)
{
    // 将字符串转换为 InlineString 写入 ContentProperty
    set_value(ContentProperty, core::Variant{ containers::InlineString{ text } });
}

const core::Variant& ContentControl::content() const noexcept
{
    // 返回当前生效的最高优先级值
    return get_value(ContentProperty);
}

UIElement* ContentControl::content_element() const noexcept
{
    const core::Variant& v = content();
    return v.has<UIElement*>() ? v.get<UIElement*>() : nullptr;
}

core::StringView ContentControl::content_text() const noexcept
{
    const core::Variant& v = content();
    if (v.has<containers::InlineString>()) {
        const auto& s = v.get<containers::InlineString>();
        return core::StringView{ s.data(), s.size() };
    }
    return {};
}

// ============================================================================
// 内容变更钩子（默认空实现）
// ============================================================================

void ContentControl::on_content_changed(const core::Variant& /*old_v*/,
                                         const core::Variant& /*new_v*/) noexcept
{
    // 基类无需额外处理；PropertyMetadata 已自动触发 invalidate_measure + invalidate_render。
    // 子类（如 Button）重写此方法以同步私有字段缓存。
}

} // namespace mine::ui
