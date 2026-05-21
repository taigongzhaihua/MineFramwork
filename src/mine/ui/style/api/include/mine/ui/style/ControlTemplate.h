/**
 * @file ControlTemplate.h
 * @brief ControlTemplate —— 控件视觉树模板对象。
 *
 * ControlTemplate 通过一个 BuildFn 函数指针描述控件的视觉树结构。
 * BuildFn 通常由 mmlc 编译 .mml 文件时自动生成，也可由程序手动构造。
 *
 * 设计原则：
 *   - BuildFn 参数类型为 DependencyObject&（而非 Control&），
 *     以避免 mine.ui.style 对 mine.ui.visual 产生循环依赖。
 *     在 BuildFn 内部，可通过 static_cast<Control&>(target) 还原类型。
 *   - ControlTemplate 为值类型，可被 TemplateRegistry 存储和复制。
 *
 * 优先级链（高 → 低）：
 *   Animation(60) > Local(50) > TemplateBind(40) > StyleTrigger(30)
 *   > StyleSetter(20) > Inherited(10) > Default(0)
 *
 * 生命周期：
 *   模板对象在程序启动时由 TemplateRegistry::register_template() 注册，
 *   build() 在控件初始化时调用，构建视觉树并连接 TemplateBinding。
 */

#pragma once

#include <mine/ui/style/Api.h>
#include <mine/core/StringView.h>
#include <mine/core/TypeId.h>
#include <mine/containers/InlineString.h>

// 前向声明，避免将 DependencyObject.h 拉入公共头链
namespace mine::ui {
class DependencyObject;
}  // namespace mine::ui

namespace mine::ui::style {

/**
 * @brief 控件模板 —— 封装控件视觉树的构建函数和目标类型信息。
 */
class MINE_UI_STYLE_API ControlTemplate {
public:
    /**
     * @brief 模板构建函数类型。
     *
     * 参数为控件的 DependencyObject 基类引用，内部可 static_cast 为具体控件类型。
     * 函数应完成以下工作：
     *   1. 创建视觉树元素（UIElement 子类实例）
     *   2. 调用 static_cast<Control&>(target).set_template_root(root)
     *   3. 通过 static_cast<Control&>(target).bind_template(...) 绑定属性
     */
    using BuildFn = void (*)(ui::DependencyObject& target);

    /**
     * @brief 模板构建函数（公开成员，供 TemplateRegistry 初始化时直接赋值）。
     *
     * 默认为 nullptr，表示空模板（build() 调用时为空操作）。
     */
    BuildFn build_fn_{nullptr};

    // ── 生命周期 ──────────────────────────────────────────────────────────

    ControlTemplate()  = default;
    ~ControlTemplate() = default;

    ControlTemplate(const ControlTemplate&)                = default;
    ControlTemplate& operator=(const ControlTemplate&)     = default;
    ControlTemplate(ControlTemplate&&) noexcept            = default;
    ControlTemplate& operator=(ControlTemplate&&) noexcept = default;

    // ── 属性访问 ──────────────────────────────────────────────────────────

    /**
     * @brief 返回此模板适用的控件类型 ID。
     */
    [[nodiscard]] core::TypeId target_type() const noexcept { return target_type_; }

    /**
     * @brief 返回模板注册名称（如 "DefaultButton"）。
     */
    [[nodiscard]] core::StringView name() const noexcept
    {
        return core::StringView{ name_.data(), name_.size() };
    }

    // ── 操作 ──────────────────────────────────────────────────────────────

    /**
     * @brief 执行构建函数，在 target 上构建模板视觉树。
     *
     * 若 build_fn_ 为 nullptr，则为空操作。
     *
     * @param target 宿主控件（通常为 Control 子类，通过 DependencyObject& 传入）
     */
    void build(ui::DependencyObject& target) const;

    // ── 构建器接口 ────────────────────────────────────────────────────────

    /**
     * @brief 设置模板注册名称，返回自身引用以支持链式调用。
     */
    ControlTemplate& set_name(core::StringView name);

    /**
     * @brief 设置目标控件类型 ID，返回自身引用以支持链式调用。
     */
    ControlTemplate& set_target_type(core::TypeId type_id) noexcept;

private:
    core::TypeId             target_type_{};  ///< 目标控件类型 ID
    containers::InlineString name_;           ///< 模板注册名称（SSO 23 字节）
};

}  // namespace mine::ui::style
