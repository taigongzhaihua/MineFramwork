/**
 * @file TemplateRegistry.h
 * @brief TemplateRegistry —— 控件模板注册表（全局单例）。
 *
 * 存储所有已注册的 ControlTemplate，提供按名称和目标类型的查找接口。
 *
 * 典型用法（由 mmlc 生成代码调用）：
 * @code
 *   // 在静态初始化阶段注册模板
 *   static const auto& s_btn_tmpl =
 *       TemplateRegistry::instance().register_template(
 *           "DefaultButton",
 *           TypeId::of<Button>(),
 *           &build_default_button);
 *
 *   // 运行时查找
 *   const ControlTemplate* tmpl =
 *       TemplateRegistry::instance().find("DefaultButton");
 *   if (tmpl) tmpl->build(my_button);
 * @endcode
 *
 * 存储策略：
 *   - 内部使用 Vector<OwnedPtr<ControlTemplate>> 存储，保证引用地址稳定。
 *   - 线性查找（预期模板数量较小，< 100）。
 *
 * 线程安全：
 *   所有方法须在 UI 线程调用（静态初始化阶段除外）。
 */

#pragma once

#include <mine/ui/style/Api.h>
#include <mine/ui/style/ControlTemplate.h>
#include <mine/core/Pimpl.h>
#include <mine/core/StringView.h>
#include <mine/core/TypeId.h>

namespace mine::ui::style {

/**
 * @brief 控件模板全局注册表。
 */
class MINE_UI_STYLE_API TemplateRegistry {
public:
    /**
     * @brief 获取全局单例实例。
     *
     * 使用函数内局部静态变量实现，C++11 保证线程安全初始化。
     */
    static TemplateRegistry& instance() noexcept;

    /**
     * @brief 注册一个控件模板，返回对存储对象的常量引用。
     *
     * 通常由 mmlc 生成的静态初始化代码调用。
     * 若同名模板已存在，则新增一条记录（find 返回最后注册的）。
     *
     * @param name        模板注册名称（如 "DefaultButton"）
     * @param target_type 目标控件类型 ID
     * @param build_fn    构建函数指针（nullptr 表示空模板）
     * @return            对已存储 ControlTemplate 的常量引用（引用地址稳定）
     */
    const ControlTemplate& register_template(core::StringView         name,
                                             core::TypeId             target_type,
                                             ControlTemplate::BuildFn build_fn) noexcept;

    /**
     * @brief 按名称查找模板。
     *
     * @param name 模板注册名称
     * @return     指向已注册模板的指针；若未找到则返回 nullptr
     */
    [[nodiscard]] const ControlTemplate* find(core::StringView name) const noexcept;

    /**
     * @brief 按目标类型查找默认模板（用于控件初始化时自动应用）。
     *
     * 返回最后注册的、target_type 匹配的模板。
     *
     * @param type 目标控件类型 ID
     * @return     指向已注册模板的指针；若未找到则返回 nullptr
     */
    [[nodiscard]] const ControlTemplate* find_default(core::TypeId type) const noexcept;

private:
    TemplateRegistry();

    struct Impl;
    core::Pimpl<Impl> p_;
};

}  // namespace mine::ui::style
