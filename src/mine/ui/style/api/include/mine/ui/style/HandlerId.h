#pragma once

#include <cstdint>

namespace mine::ui::style {

/// 资源字典订阅取消令牌。
/// 由 subscribe() / on_resource_changed() 返回，传入对应的 unsubscribe() /
/// off_resource_changed() 即可取消订阅。
/// 0 为无效值（kInvalidHandlerId）。
using HandlerId = uint32_t;

/// 无效订阅令牌常量。
inline constexpr HandlerId kInvalidHandlerId = 0;

}  // namespace mine::ui::style
