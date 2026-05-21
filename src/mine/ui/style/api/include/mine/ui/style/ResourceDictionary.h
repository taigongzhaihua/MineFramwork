#pragma once

#include <mine/ui/style/Api.h>
#include <mine/ui/style/HandlerId.h>

#include <mine/core/Function.h>
#include <mine/core/Pimpl.h>
#include <mine/core/StringView.h>
#include <mine/core/Variant.h>

namespace mine::ui::style {

/**
 * @brief 树形资源字典。
 *
 * 用于 MineUI 样式系统的多层键值资源存储。每个 UIElement 可持有一个
 * ResourceDictionary，通过 set_parent() 连接到父控件的字典，形成树形查找链。
 *
 * 查找顺序（find）：
 *   本层 local_ → 合并层 merged_（按 merge 顺序）→ 父层 parent_（递归）
 *
 * 通知机制：
 *   - subscribe(key, cb)     ：订阅特定键的值变更（DynamicResource 场景）
 *   - on_resource_changed(cb)：订阅任意键变更的广播（子字典、主题切换场景）
 *   - set_dynamic(key, val)  ：写入并广播，触发上述所有相关回调
 *   - notify_resource_changed(key)：手动广播（如主题合并后调用）
 *
 * 线程安全：不提供，调用方负责在同一线程使用。
 */
class MINE_UI_STYLE_API ResourceDictionary {
public:
    ResourceDictionary();
    ~ResourceDictionary();

    /// 不可拷贝（内部持有弱引用指针和 move-only 回调）
    ResourceDictionary(const ResourceDictionary&)            = delete;
    ResourceDictionary& operator=(const ResourceDictionary&) = delete;

    /// 可移动
    ResourceDictionary(ResourceDictionary&&) noexcept;
    ResourceDictionary& operator=(ResourceDictionary&&) noexcept;

    // ── 写入 ──────────────────────────────────────────────────────────────

    /// 写入静态值，不触发订阅通知（适用于批量初始化 / 主题加载）。
    void set(core::StringView key, core::Variant value);

    /// 写入动态值：更新后触发该 key 的所有 subscribe() 回调，
    /// 并广播 resource_changed（on_resource_changed 订阅者均会收到通知）。
    void set_dynamic(core::StringView key, core::Variant value);

    // ── 合并 ──────────────────────────────────────────────────────────────

    /// 合并另一资源字典（弱引用，不拥有其生命周期）。
    /// 查找时若本层无命中，依次在合并层中查找（按 merge 调用顺序）。
    void merge(const ResourceDictionary& other);

    /// 清除所有已合并字典（保留本层和父层连接）。
    void clear_merged() noexcept;

    // ── 查找 ──────────────────────────────────────────────────────────────

    /// 树形查找：本层 → 合并层 → 父层。
    /// 未命中返回空 Variant（调用方用 has_value() 判断）。
    [[nodiscard]] core::Variant find(core::StringView key) const noexcept;

    /// 仅查本层。未命中返回空 Variant。
    [[nodiscard]] core::Variant find_local(core::StringView key) const noexcept;

    // ── DynamicResource 订阅 ───────────────────────────────────────────────

    /// 订阅特定 key 的值变更。
    /// @return 取消令牌，传入 unsubscribe() 可取消。kInvalidHandlerId 表示失败。
    HandlerId subscribe(core::StringView key,
                        core::Function<void(const core::Variant&)> callback);

    /// 取消特定 key 的订阅。
    void unsubscribe(HandlerId id) noexcept;

    // ── 父字典连接 ────────────────────────────────────────────────────────

    /// 连接父字典（布局系统在 visual tree attach 时调用）。
    /// 传 nullptr 解除连接。自动管理父字典的 resource_changed 订阅。
    void set_parent(ResourceDictionary* parent) noexcept;

    /// 返回当前父字典，可为 nullptr。
    [[nodiscard]] ResourceDictionary* parent() const noexcept;

    // ── resource_changed 广播接口 ─────────────────────────────────────────

    /// 订阅任意资源键变更广播。key="*" 表示全量刷新（如主题切换）。
    /// @return 取消令牌，传入 off_resource_changed() 可取消。
    HandlerId on_resource_changed(core::Function<void(core::StringView)> callback);

    /// 取消 resource_changed 广播订阅。
    void off_resource_changed(HandlerId id) noexcept;

    /// 手动广播资源变更通知。
    /// 主题合并（merge + clear_merged）后调用，key="*" 表示全量刷新。
    void notify_resource_changed(core::StringView key);

private:
    struct Impl;
    core::Pimpl<Impl> p_;
};

}  // namespace mine::ui::style
