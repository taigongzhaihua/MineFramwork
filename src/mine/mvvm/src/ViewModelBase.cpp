/**
 * @file ViewModelBase.cpp
 * @brief ViewModelBase 实现 —— MVVM ViewModel 完整基类。
 */

#include <mine/mvvm/ViewModelBase.h>

namespace mine::mvvm {

// ── 构造 / 析构 ───────────────────────────────────────────────────────────

ViewModelBase::ViewModelBase()  = default;
ViewModelBase::~ViewModelBase() = default;

// ── 框架调用接口 ─────────────────────────────────────────────────────────

void ViewModelBase::initialize() noexcept
{
    on_initialize();
}

void ViewModelBase::navigated_to(const mine::core::Variant& param) noexcept
{
    on_navigated_to(param);
}

void ViewModelBase::navigated_from() noexcept
{
    on_navigated_from();
}

void ViewModelBase::appear() noexcept
{
    on_appear();
}

void ViewModelBase::disappear() noexcept
{
    on_disappear();
}

void ViewModelBase::dispose() noexcept
{
    on_dispose();
}

// ── 生命周期虚方法默认实现（均为空实现，子类可重写）─────────────────────

void ViewModelBase::on_initialize() noexcept {}

void ViewModelBase::on_navigated_to(const mine::core::Variant& /*param*/) noexcept {}

void ViewModelBase::on_navigated_from() noexcept {}

void ViewModelBase::on_appear() noexcept {}

void ViewModelBase::on_disappear() noexcept {}

void ViewModelBase::on_dispose() noexcept {}

} // namespace mine::mvvm
