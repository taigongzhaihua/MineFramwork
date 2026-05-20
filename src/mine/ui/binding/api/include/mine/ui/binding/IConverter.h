/**
 * @file IConverter.h
 * @brief 绑定值转换器接口。
 *
 * 转换器允许在源值写入目标之前对其进行类型转换或格式化，
 * 例如将 bool 转换为 Visibility 枚举，或将字节数格式化为人类可读字符串。
 *
 * M1.1 实现 convert()（OneWay 方向）；
 * M2 实现 convert_back()（TwoWay 回写方向）。
 *
 * MML 用法：
 *   text: bind(vm.Bytes, converter: BytesToHumanReadable, parameter: "MB");
 */

#pragma once

#include <mine/core/TypeId.h>
#include <mine/core/Variant.h>
#include <mine/core/StringView.h>
#include <mine/ui/binding/Api.h>

namespace mine::ui {

/**
 * @brief 绑定值转换器接口。
 *
 * 实现者须确保两个方法均为 noexcept（不抛异常）。
 * 转换失败时应返回空 Variant（即 Variant{}），
 * BindingExpression 将使用 fallback 值代替。
 */
struct MINE_UI_BINDING_API IConverter {
    virtual ~IConverter() = default;

    /**
     * @brief 正向转换：将源值转换为目标类型（OneWay 方向使用）。
     *
     * @param value       来自 getter 的源值
     * @param target_type 目标 DependencyProperty 的值类型（TypeId）
     * @param parameter   MML 中 converter 的 parameter 字符串（可为空）
     * @return 转换后的值；失败时返回空 Variant
     */
    [[nodiscard]] virtual core::Variant convert(
        const core::Variant& value,
        core::TypeId         target_type,
        core::StringView     parameter) const noexcept = 0;

    /**
     * @brief 反向转换：将目标值转换回源类型（TwoWay 回写时使用，M2 实现）。
     *
     * M1.1 中此方法不会被调用，实现者可返回空 Variant 作为占位。
     *
     * @param value       来自目标属性的当前值
     * @param target_type 源对象期望的值类型
     * @param parameter   MML 中 converter 的 parameter 字符串（可为空）
     * @return 转换后的源值；失败时返回空 Variant
     */
    [[nodiscard]] virtual core::Variant convert_back(
        const core::Variant& value,
        core::TypeId         target_type,
        core::StringView     parameter) const noexcept = 0;
};

} // namespace mine::ui
