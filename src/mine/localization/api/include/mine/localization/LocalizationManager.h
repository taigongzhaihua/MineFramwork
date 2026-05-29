/**
 * @file LocalizationManager.h
 * @brief 本地化管理器：多语言资源字典、运行时语言切换、tr() 翻译查询。
 *
 * 设计概述：
 *   - 三层回退策略：当前语言 → 回退语言（默认 "en"）→ 返回 key 原文
 *   - 支持两种资源包格式：JSON（{"key":"value"}）和 KeyValue（key=value 每行）
 *   - 参数替换：翻译文本中的 {0}~{3} 占位符，tr_format() 自动替换
 *   - 复数支持：n==1 使用 key，否则查找 key + "_plural"（简单 CLDR 规则）
 *   - 语言切换通知：函数指针 + token 订阅/取消订阅
 *   - 线程安全：本类不保证线程安全，调用方应在单一线程使用或自行加锁
 */
#pragma once

#include <mine/localization/Api.h>
#include <mine/core/Status.h>
#include <mine/core/StringView.h>
#include <mine/core/Pimpl.h>
#include <mine/containers/InlineString.h>
#include <mine/containers/Vector.h>

#pragma warning(push)
#pragma warning(disable: 4251) // Pimpl 成员的 DLL 接口警告

namespace mine::localization {

// ─────────────────────────────────────────────────────────────────────────────
// 枚举与回调类型
// ─────────────────────────────────────────────────────────────────────────────

/// 资源包数据格式
enum class CatalogFormat : uint8_t {
    Json,     ///< JSON 格式（平面对象）：{"翻译键":"翻译文本", ...}
    KeyValue, ///< 键值对格式：翻译键=翻译文本（每行一条，# 或 ; 开头为注释行）
};

/**
 * @brief 语言切换通知回调类型。
 * @param old_lang  切换前的语言代码（未设置时为空视图）
 * @param new_lang  切换后的语言代码
 * @param userdata  注册时传入的用户数据指针
 */
using LanguageChangedCallback = void(*)(mine::core::StringView old_lang,
                                        mine::core::StringView new_lang,
                                        void* userdata);

// ─────────────────────────────────────────────────────────────────────────────
// LocalizationManager
// ─────────────────────────────────────────────────────────────────────────────

/**
 * @brief 本地化管理器。
 *
 * 维护多种语言的翻译字典，支持运行时语言切换和变更通知。
 * 通过 global_manager() 可获取进程级默认单例；也可手动创建独立实例。
 *
 * 典型用法：
 * @code
 *   auto& mgr = mine::localization::global_manager();
 *   mgr.load_catalog("zh-CN", R"({"app.title":"MineFramework 示例"})");
 *   mgr.load_catalog("en",    R"({"app.title":"MineFramework Demo"})");
 *   mgr.set_language("zh-CN");
 *
 *   // 查询翻译
 *   auto title = mine::localization::tr("app.title");
 * @endcode
 */
class MINE_LOCALIZATION_API LocalizationManager {
public:
    LocalizationManager() noexcept;
    ~LocalizationManager() noexcept;

    // 不可复制（内部包含 Pimpl 持有的哈希表，语义上不应复制）
    LocalizationManager(const LocalizationManager&) = delete;
    LocalizationManager& operator=(const LocalizationManager&) = delete;

    // ── 资源包加载 ───────────────────────────────────────────────────────

    /**
     * @brief 从字符串内容加载指定语言的翻译资源包。
     *
     * 多次对同一语言调用时，新条目会覆盖旧条目（合并，而非整体替换）。
     *
     * @param language  语言代码，如 "zh-CN"、"en-US"、"en"（区分大小写）
     * @param data      资源包内容字符串
     * @param fmt       资源包格式，默认 JSON
     * @return          Ok 成功；InvalidArg 参数为空；ParseError 格式错误
     */
    [[nodiscard]] mine::core::Status load_catalog(
        mine::core::StringView language,
        mine::core::StringView data,
        CatalogFormat fmt = CatalogFormat::Json) noexcept;

    /**
     * @brief 手动注册单条翻译（适用于少量硬编码字符串）。
     *
     * @param language  语言代码
     * @param key       翻译键
     * @param value     翻译文本
     */
    void add_translation(mine::core::StringView language,
                         mine::core::StringView key,
                         mine::core::StringView value) noexcept;

    // ── 语言管理 ─────────────────────────────────────────────────────────

    /**
     * @brief 设置当前活跃语言。
     *
     * 若新语言与当前语言相同则无操作；
     * 否则更新当前语言并触发所有已注册的语言切换通知。
     *
     * @param language  语言代码（建议与加载时使用的代码一致）
     */
    void set_language(mine::core::StringView language) noexcept;

    /// 获取当前活跃语言代码（从未设置时返回空视图）
    [[nodiscard]] mine::core::StringView get_language() const noexcept;

    /**
     * @brief 设置回退语言（默认为 "en"）。
     *
     * 当在当前语言中找不到某条翻译时，自动在回退语言中继续查找。
     * 设置为空字符串可禁用回退查找。
     */
    void set_fallback_language(mine::core::StringView language) noexcept;

    /// 获取当前回退语言代码
    [[nodiscard]] mine::core::StringView get_fallback_language() const noexcept;

    /**
     * @brief 将已加载的语言代码列表追加到 out（不清空 out）。
     *
     * 语言代码按首次加载的顺序排列。
     */
    void get_available_languages(
        mine::containers::Vector<mine::containers::InlineString>& out) const noexcept;

    /// 检查指定语言代码是否已通过 load_catalog 或 add_translation 加载
    [[nodiscard]] bool has_language(mine::core::StringView language) const noexcept;

    // ── 翻译查询 ─────────────────────────────────────────────────────────

    /**
     * @brief 查找翻译文本。
     *
     * 查找顺序：当前语言 → 回退语言 → 返回 key 原文。
     * 返回的 StringView 指向内部存储，切换语言后不应继续持有。
     *
     * @param key  翻译键
     * @return     翻译文本的只读视图；若未找到，返回 key 本身的视图
     */
    [[nodiscard]] mine::core::StringView tr(mine::core::StringView key) const noexcept;

    /**
     * @brief 查找翻译并替换位置参数（{0}~{3}）。
     *
     * 示例：翻译文本 "Hello, {0}! You have {1} messages."
     *        调用 tr_format("key", "Alice", "3") → "Hello, Alice! You have 3 messages."
     *
     * 未提供的参数（空 StringView）不会替换对应占位符（占位符保留原样）。
     *
     * @param key   翻译键
     * @param arg0  替换 {0} 的字符串（默认空）
     * @param arg1  替换 {1} 的字符串
     * @param arg2  替换 {2} 的字符串
     * @param arg3  替换 {3} 的字符串
     * @return      替换后的完整字符串（按需堆分配或 SSO）
     */
    [[nodiscard]] mine::containers::InlineString tr_format(
        mine::core::StringView key,
        mine::core::StringView arg0 = {},
        mine::core::StringView arg1 = {},
        mine::core::StringView arg2 = {},
        mine::core::StringView arg3 = {}) const noexcept;

    /**
     * @brief 简单复数形式查找。
     *
     * n == 1 时：查找 key（单数形式）。
     * n != 1 时：优先查找 key + "_plural"（复数形式），若不存在则回退到 key。
     *
     * 未找到时均回退到 key 原文。
     *
     * @param key  翻译键（单数形式对应的键）
     * @param n    数量，决定使用单数还是复数形式
     * @return     翻译文本视图
     */
    [[nodiscard]] mine::core::StringView tr_plural(
        mine::core::StringView key, int64_t n) const noexcept;

    // ── 变更通知 ─────────────────────────────────────────────────────────

    /**
     * @brief 订阅语言切换通知。
     *
     * @param cb        回调函数（不可为 nullptr）
     * @param userdata  传递给回调的用户数据（可为 nullptr）
     * @return          订阅 token（用于 unsubscribe）；0 表示注册失败
     */
    [[nodiscard]] uint32_t subscribe(LanguageChangedCallback cb,
                                     void* userdata = nullptr) noexcept;

    /**
     * @brief 取消订阅语言切换通知。
     *
     * @param token  subscribe() 返回的 token；无效 token 无操作
     */
    void unsubscribe(uint32_t token) noexcept;

private:
    struct Impl;
    mine::core::Pimpl<Impl> p_;
};

// ─────────────────────────────────────────────────────────────────────────────
// 全局管理器与快捷函数
// ─────────────────────────────────────────────────────────────────────────────

/**
 * @brief 获取进程级全局本地化管理器。
 *
 * 返回的引用在程序整个生命周期内有效。
 * 适用于大多数应用场景；若需隔离（如多语言并行单测），请手动创建实例。
 */
MINE_LOCALIZATION_API LocalizationManager& global_manager() noexcept;

/**
 * @brief 在全局管理器中查找翻译（快捷函数）。
 *
 * 等价于 global_manager().tr(key)。
 */
inline mine::core::StringView tr(mine::core::StringView key) noexcept {
    return global_manager().tr(key);
}

/**
 * @brief 在全局管理器中查找翻译并替换参数（快捷函数）。
 *
 * 等价于 global_manager().tr_format(key, arg0, arg1, arg2, arg3)。
 */
inline mine::containers::InlineString tr_format(
    mine::core::StringView key,
    mine::core::StringView arg0 = {},
    mine::core::StringView arg1 = {},
    mine::core::StringView arg2 = {},
    mine::core::StringView arg3 = {}) noexcept
{
    return global_manager().tr_format(key, arg0, arg1, arg2, arg3);
}

} // namespace mine::localization

#pragma warning(pop)
