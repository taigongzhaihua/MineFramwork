/**
 * @file ServiceCollection.cpp
 * @brief ServiceCollection 的实现。
 */

#include <mine/di/ServiceCollection.h>

#include <mine/containers/Vector.h>
#include <mine/core/Assert.h>
#include <mine/core/Pimpl.h>

namespace mine::di {

// ─────────────────────────────────────────────────────────────────────────────
// ServiceCollection::Impl
// ─────────────────────────────────────────────────────────────────────────────

struct ServiceCollection::Impl {
    /// 按注册顺序存储的服务描述符列表
    mine::containers::Vector<detail::ServiceDescriptor> descriptors;
};

// ─────────────────────────────────────────────────────────────────────────────
// ServiceCollection 构造/析构
// ─────────────────────────────────────────────────────────────────────────────

ServiceCollection::ServiceCollection()
    : p_{ core::make_pimpl<Impl>() }
{}

ServiceCollection::~ServiceCollection() = default;

ServiceCollection::ServiceCollection(ServiceCollection&&) noexcept = default;
ServiceCollection& ServiceCollection::operator=(ServiceCollection&&) noexcept = default;

// ─────────────────────────────────────────────────────────────────────────────
// ServiceCollection::register_descriptor
// ─────────────────────────────────────────────────────────────────────────────

void ServiceCollection::register_descriptor(detail::ServiceDescriptor desc) {
    p_->descriptors.push_back(desc);
}

// ─────────────────────────────────────────────────────────────────────────────
// ServiceCollection::build
// ─────────────────────────────────────────────────────────────────────────────

ServiceProvider ServiceCollection::build() {
    return ServiceProvider{ std::move(p_->descriptors) };
}

} // namespace mine::di
