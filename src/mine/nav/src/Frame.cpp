/**
 * @file Frame.cpp
 * @brief Frame 导航容器控件实现（任务 20）。
 *
 * 实现要点：
 *   - Pimpl 持有路由表、历史栈、事件订阅者列表
 *   - navigate_to 流程：查路由 → 询问旧页 → 通知旧页 → 创建新页
 *                       → 切换内容 → 通知新页 → 广播事件
 *   - go_back 流程：询问当前页 → 通知当前页 → 弹出历史栈顶
 *                   → 恢复前一页显示 → 广播事件
 *   - 历史栈中所有 Page 实例的所有权由 Frame 持有（OwnedPtr）
 *   - UserControl::set_content() 负责视觉子树更新（add_child/remove_child）
 *   - Page 有虚析构函数，typed_deleter<Page> 可安全销毁任意派生类
 */

#include <mine/nav/Frame.h>
#include <mine/ui/controls/Page.h>
#include <mine/core/Memory.h>
#include <mine/core/Pimpl.h>
#include <mine/containers/Vector.h>
#include <mine/containers/InlineString.h>

namespace mine::nav {

// ============================================================================
// Pimpl 实现结构体
// ============================================================================

struct Frame::Impl {
    // ── 路由表条目 ───────────────────────────────────────────────────────────
    struct RouteEntry {
        /// 路由名称（区分大小写）
        mine::containers::InlineString  name;
        /// 页面工厂函数指针
        INavigationService::PageFactory factory{nullptr};
        /// 传递给工厂的用户数据
        void*                           user_data{nullptr};
    };

    // ── 历史栈条目 ───────────────────────────────────────────────────────────
    struct HistoryEntry {
        /// 路由名称（用于广播事件和 current_route()）
        mine::containers::InlineString  route;
        /// 页面实例（Frame 持有所有权，OwnedPtr 析构时自动销毁）
        mine::core::OwnedPtr<mine::ui::Page> page;
        /// 导航参数（保存备用，目前未用于 go_back 恢复，仅供扩展）
        mine::core::Variant             param;
    };

    // ── 事件订阅者条目 ───────────────────────────────────────────────────────
    struct Subscriber {
        INavigationService::EventFn fn{nullptr};
        void*                       user_data{nullptr};
        uint32_t                    token{0};
    };

    /// 已注册的路由表（按注册顺序排列，重名时最后注册的覆盖前者）
    mine::containers::Vector<RouteEntry>   routes;

    /// 历史导航栈（栈顶 = 当前显示的页面）
    mine::containers::Vector<HistoryEntry> history;

    /// 事件订阅者列表
    mine::containers::Vector<Subscriber>  subscribers;

    /// 下一个可用的订阅令牌（从 1 开始，0 保留为"无效令牌"）
    uint32_t next_token{1};
};

// ============================================================================
// 辅助函数：删除器（通过虚析构安全销毁任意 Page 派生类）
// ============================================================================

/// Page 实例的删除器：通过虚析构函数正确调用派生类析构，
/// 然后通过默认分配器（malloc/free）释放内存。
/// 安全性：Page 继承自 Visual（含虚析构），MallocAllocator::dealloc 忽略 size 参数。
static void page_deleter(void* ptr) noexcept {
    auto* page = static_cast<mine::ui::Page*>(ptr);
    // 调用析构（虚析构 → 正确销毁派生类）
    page->~Page();
    // 释放内存（与 MINE_NEW/new 使用的 malloc 对称）
    mine::core::default_allocator()->dealloc(ptr, 0, alignof(mine::ui::Page));
}

// ============================================================================
// Frame 构造 / 析构
// ============================================================================

Frame::Frame()
    : p_{mine::core::make_pimpl<Impl>()}
{}

Frame::~Frame() = default;

// ============================================================================
// INavigationService — 路由管理
// ============================================================================

void Frame::add_route(mine::core::StringView  name,
                      PageFactory             factory,
                      void*                   user_data) noexcept
{
    // 查找是否已有同名路由，有则覆盖
    for (auto& entry : p_->routes) {
        if (static_cast<mine::core::StringView>(entry.name) == name) {
            entry.factory   = factory;
            entry.user_data = user_data;
            return;
        }
    }

    // 新增路由
    Impl::RouteEntry entry;
    entry.name      = name;
    entry.factory   = factory;
    entry.user_data = user_data;
    p_->routes.push_back(std::move(entry));
}

// ============================================================================
// INavigationService — 导航操作
// ============================================================================

mine::core::Status Frame::navigate_to(mine::core::StringView route,
                                       mine::core::Variant    param) noexcept
{
    // 步骤 1：查找路由
    Impl::RouteEntry* found = nullptr;
    for (auto& entry : p_->routes) {
        if (static_cast<mine::core::StringView>(entry.name) == route) {
            found = &entry;
            break;
        }
    }

    if (found == nullptr) {
        // 路由未注册 → 广播失败事件并返回
        NavigationEvent ev{};
        ev.type      = NavigationEventType::NavigationFailed;
        ev.route     = route;
        ev.parameter = &param;
        broadcast_event(ev);
        return mine::core::Status{mine::core::StatusCode::NotFound};
    }

    // 步骤 2：广播 Navigating 事件（路由确认，Page 尚未切换）
    {
        NavigationEvent ev{};
        ev.type      = NavigationEventType::Navigating;
        ev.route     = route;
        ev.parameter = &param;
        broadcast_event(ev);
    }

    // 步骤 3：询问当前页面是否允许离开
    if (!p_->history.empty()) {
        mine::ui::Page* current = p_->history.back().page.get();
        if (current != nullptr && !current->on_navigate_away()) {
            // 当前页面拒绝离开 → 广播取消事件并返回
            NavigationEvent ev{};
            ev.type      = NavigationEventType::NavigationCancelled;
            ev.route     = route;
            ev.parameter = &param;
            broadcast_event(ev);
            return mine::core::Status{mine::core::StatusCode::Cancelled};
        }
    }

    // 步骤 4：通知当前页面"即将离开"
    if (!p_->history.empty()) {
        mine::ui::Page* current = p_->history.back().page.get();
        if (current != nullptr) {
            current->on_navigated_from();
        }
    }

    // 步骤 5：通过工厂创建新页面
    mine::ui::Page* raw_page = found->factory(found->user_data);
    if (raw_page == nullptr) {
        // 工厂返回 nullptr，视为失败
        NavigationEvent ev{};
        ev.type      = NavigationEventType::NavigationFailed;
        ev.route     = route;
        ev.parameter = &param;
        broadcast_event(ev);
        return mine::core::Status{mine::core::StatusCode::InvalidState};
    }

    // 步骤 6：将新页面压入历史栈（Frame 取得所有权）
    {
        Impl::HistoryEntry entry;
        entry.route = route;
        entry.page  = mine::core::OwnedPtr<mine::ui::Page>{raw_page, &page_deleter};
        entry.param = param;
        p_->history.push_back(std::move(entry));
    }

    // 步骤 7：切换内容（触发 UserControl::on_content_changed → 视觉子树更新）
    switch_content(p_->history.back().page.get());

    // 步骤 8：通知新页面"已到达"
    p_->history.back().page->on_navigated_to(param);

    // 步骤 9：广播 Navigated 事件（导航完成）
    {
        NavigationEvent ev{};
        ev.type      = NavigationEventType::Navigated;
        ev.route     = route;
        ev.parameter = &param;
        broadcast_event(ev);
    }

    return mine::core::Status{mine::core::StatusCode::Ok};
}

bool Frame::go_back() noexcept
{
    // 历史栈中需要至少 2 个条目（当前页 + 前一页）才能回退
    if (!can_go_back()) {
        return false;
    }

    // 步骤 1：询问当前页面是否允许离开
    mine::ui::Page* current = p_->history.back().page.get();
    if (current != nullptr && !current->on_navigate_away()) {
        return false;
    }

    // 步骤 2：通知当前页面"即将离开"
    if (current != nullptr) {
        current->on_navigated_from();
    }

    // 步骤 3：弹出当前页面（OwnedPtr 析构 → 销毁当前 Page）
    p_->history.pop_back();

    // 步骤 4：恢复显示前一页面
    mine::ui::Page* prev_page = p_->history.back().page.get();
    switch_content(prev_page);

    // 步骤 5：广播 Navigated 事件（路由名为前一页路由）
    {
        const mine::core::Variant& prev_param = p_->history.back().param;
        NavigationEvent ev{};
        ev.type      = NavigationEventType::Navigated;
        ev.route     = static_cast<mine::core::StringView>(p_->history.back().route);
        ev.parameter = &prev_param;
        broadcast_event(ev);
    }

    return true;
}

bool Frame::can_go_back() const noexcept
{
    return p_->history.size() >= 2;
}

mine::core::StringView Frame::current_route() const noexcept
{
    if (p_->history.empty()) {
        return {};
    }
    return static_cast<mine::core::StringView>(p_->history.back().route);
}

// ============================================================================
// INavigationService — 事件订阅
// ============================================================================

uint32_t Frame::subscribe(EventFn fn, void* user_data) noexcept
{
    if (fn == nullptr) {
        return 0; // 无效订阅，返回 0
    }

    Impl::Subscriber sub;
    sub.fn        = fn;
    sub.user_data = user_data;
    sub.token     = p_->next_token++;
    p_->subscribers.push_back(sub);
    return sub.token;
}

void Frame::unsubscribe(uint32_t token) noexcept
{
    if (token == 0) return;

    auto& subs = p_->subscribers;
    for (size_t i = 0; i < subs.size(); ++i) {
        if (subs[i].token == token) {
            // swap-with-back 模式：O(1) 删除，保持其他元素顺序
            subs[i] = subs[subs.size() - 1];
            subs.pop_back();
            return;
        }
    }
    // token 不存在，静默忽略
}

// ============================================================================
// Frame 特有接口
// ============================================================================

mine::ui::Page* Frame::current_page() const noexcept
{
    if (p_->history.empty()) {
        return nullptr;
    }
    return p_->history.back().page.get();
}

size_t Frame::history_depth() const noexcept
{
    return p_->history.size();
}

// ============================================================================
// 私有辅助方法
// ============================================================================

void Frame::broadcast_event(const NavigationEvent& ev) noexcept
{
    // 迭代副本大小，防止回调中调用 unsubscribe 导致迭代器失效
    const size_t count = p_->subscribers.size();
    for (size_t i = 0; i < count; ++i) {
        // 安全检查：回调可能在迭代中调用 unsubscribe 缩小列表
        if (i < p_->subscribers.size() && p_->subscribers[i].fn != nullptr) {
            p_->subscribers[i].fn(this, ev, p_->subscribers[i].user_data);
        }
    }
}

void Frame::switch_content(mine::ui::Page* new_page) noexcept
{
    // 通过 UserControl::set_content() 更新内容
    // UserControl::on_content_changed() 会自动处理：
    //   - 移除旧内容元素（remove_child）
    //   - 添加新内容元素（add_child）
    // 这会触发 Page::on_parent_changed → on_unloaded/on_loaded
    if (new_page != nullptr) {
        set_content(new_page);
    } else {
        // 清空内容（恢复到无 Page 状态）
        set_content(static_cast<mine::ui::UIElement*>(nullptr));
    }
}

} // namespace mine::nav
