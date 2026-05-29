/**
 * @file ObservableCollectionBase.h
 * @brief ObservableCollectionBase —— ObservableCollection<T> 的非模板基类。
 *
 * 将订阅者管理逻辑（subscribe/unsubscribe/notify）移入 .cpp 文件，
 * 避免模板实例化时产生大量重复的订阅管理代码，同时确保 ABI 稳定。
 *
 * 使用者不直接使用此类，应使用 ObservableCollection<T>。
 */

#pragma once

#include <mine/core/Pimpl.h>
#include <mine/mvvm/Api.h>
#include <mine/mvvm/INotifyCollectionChanged.h>

namespace mine::mvvm {

/**
 * @brief ObservableCollection<T> 的非模板基类，管理变更通知订阅者列表。
 *
 * 实现 INotifyCollectionChanged 的订阅/取消订阅逻辑，
 * 并提供 notify_subscribers() 供子模板类调用。
 *
 * 不可拷贝、不可移动（订阅者持有此对象指针）。
 */
class MINE_MVVM_API ObservableCollectionBase : public INotifyCollectionChanged {
public:
    ObservableCollectionBase();
    ~ObservableCollectionBase() override;

    ObservableCollectionBase(const ObservableCollectionBase&)            = delete;
    ObservableCollectionBase& operator=(const ObservableCollectionBase&) = delete;
    ObservableCollectionBase(ObservableCollectionBase&&)                 = delete;
    ObservableCollectionBase& operator=(ObservableCollectionBase&&)      = delete;

    // ── INotifyCollectionChanged 实现 ──────────────────────────────────────

    [[nodiscard]] uint32_t subscribe_collection_changed(
        ChangedFn fn, void* user_data) noexcept override;

    void unsubscribe_collection_changed(uint32_t token) noexcept override;

protected:
    /**
     * @brief 向所有已订阅的回调广播集合变更通知。
     *
     * @param args 变更参数
     */
    void notify_subscribers(const CollectionChangedArgs& args) noexcept;

private:
    struct Impl;
    mine::core::Pimpl<Impl> p_;
};

} // namespace mine::mvvm
