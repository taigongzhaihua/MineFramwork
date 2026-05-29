/**
 * @file INotifyCollectionChanged.h
 * @brief 集合变更通知接口（等价于 C# INotifyCollectionChanged）。
 *
 * 可观察集合（ObservableCollection<T>）实现此接口，允许 UI 绑定系统
 * 订阅集合结构变更事件，从而高效地刷新列表控件（仅更新变更部分，
 * 而非重建整个列表）。
 *
 * 回调函数为原始函数指针 + void* user_data 模式，避免依赖 std::function，
 * 符合本项目禁用异常、不依赖 STL 的约束。
 */

#pragma once

#include <cstdint>
#include <mine/mvvm/Api.h>
#include <mine/mvvm/CollectionChangedArgs.h>

namespace mine::mvvm {

/**
 * @brief 集合变更通知接口。
 *
 * UI 绑定层和列表控件通过此接口监听集合变更，
 * 并据此做出最小化更新（增量刷新）。
 */
class MINE_MVVM_API INotifyCollectionChanged {
public:
    virtual ~INotifyCollectionChanged() = default;

    /**
     * @brief 集合变更回调函数类型。
     *
     * @param sender    触发通知的集合实例
     * @param args      变更参数（动作类型、位置、数量）
     * @param user_data 订阅时传入的用户数据
     */
    using ChangedFn = void (*)(INotifyCollectionChanged* sender,
                                const CollectionChangedArgs& args,
                                void*                        user_data);

    /**
     * @brief 订阅集合变更事件。
     *
     * @param fn        变更时调用的回调函数
     * @param user_data 透传给回调的用户数据（可为 nullptr）
     * @return 订阅令牌（用于 unsubscribe_collection_changed 取消订阅）
     */
    [[nodiscard]] virtual uint32_t subscribe_collection_changed(
        ChangedFn fn, void* user_data) noexcept = 0;

    /**
     * @brief 取消集合变更事件订阅。
     *
     * @param token subscribe_collection_changed 返回的令牌；无效令牌为空操作
     */
    virtual void unsubscribe_collection_changed(uint32_t token) noexcept = 0;
};

} // namespace mine::mvvm
