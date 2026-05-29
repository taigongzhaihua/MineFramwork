/**
 * @file ConfigManager.h
 * @brief 分层配置管理器。
 *
 * ConfigManager 实现了三层优先级的配置系统：
 *
 *   优先级从高到低：
 *     环境变量层（Env）  > 文件层（File）  > 默认值层（Default）
 *
 * 使用场景：
 *   1. 应用启动时注册默认值（set_default）
 *   2. 从配置文件加载用户配置（load_file，支持 JSON/TOML）
 *   3. 允许环境变量覆盖配置（load_env，按前缀过滤）
 *   4. 运行时通过键名读取配置项（get_xxx）
 *
 * 键命名规范：
 *   - 点分隔的层次路径，如 "window.width"、"app.theme"
 *   - JSON 嵌套对象展平为点分隔路径
 *   - TOML 节头作为键前缀
 *   - 环境变量前缀后的部分转换为小写点分隔路径
 *     例如：前缀 "APP_"，变量 APP_WINDOW_WIDTH → "window.width"
 *
 * 典型用法：
 * @code
 *   mine::config::ConfigManager cfg;
 *   cfg.set_default("window.width",  mine::config::ConfigValue{1280});
 *   cfg.set_default("window.height", mine::config::ConfigValue{720});
 *   cfg.set_default("app.theme",     mine::config::ConfigValue{"light"});
 *
 *   cfg.load_file("config.toml");   // 加载用户配置
 *   cfg.load_env("APP_");           // 环境变量覆盖
 *
 *   int width  = static_cast<int>(cfg.get_integer("window.width", 1280));
 *   auto theme = cfg.get_string("app.theme", "light");
 * @endcode
 */

#pragma once

#include <cstdint>
#include <mine/config/Api.h>
#include <mine/config/ConfigValue.h>
#include <mine/core/Status.h>
#include <mine/core/StringView.h>
#include <mine/core/Pimpl.h>

namespace mine::config {

// ─────────────────────────────────────────────────────────────────────────────
// ConfigLayer：配置层标识
// ─────────────────────────────────────────────────────────────────────────────

/**
 * @brief 配置层枚举，表示配置项来自哪一层。
 *
 * 数值越大优先级越高（Env > File > Default）。
 */
enum class ConfigLayer : uint8_t {
    Default = 0,  ///< 默认值层（代码中硬编码的默认值）
    File    = 1,  ///< 文件层（从 JSON/TOML 文件加载）
    Env     = 2,  ///< 环境变量层（最高优先级）
};

// ─────────────────────────────────────────────────────────────────────────────
// ConfigFormat：配置文件格式
// ─────────────────────────────────────────────────────────────────────────────

/**
 * @brief 配置文件格式枚举。
 */
enum class ConfigFormat : uint8_t {
    Json = 0,  ///< JSON 格式（.json）
    Toml = 1,  ///< TOML 格式（.toml）
    Auto = 2,  ///< 根据文件扩展名自动检测（默认）
};

// ─────────────────────────────────────────────────────────────────────────────
// ConfigManager：分层配置管理器
// ─────────────────────────────────────────────────────────────────────────────

/**
 * @brief 分层配置管理器。
 *
 * ABI 安全设计：使用 Pimpl 模式，所有内部存储（HashMap、解析器状态）
 * 均隐藏在实现文件中，不暴露到公共头文件。
 */
#if defined(_MSC_VER)
#   pragma warning(push)
#   pragma warning(disable: 4251)  // Pimpl 成员的 DLL 接口警告（框架已知问题）
#endif
class MINE_CONFIG_API ConfigManager {
public:
    ConfigManager();
    ~ConfigManager();

    // ── 默认值层 ─────────────────────────────────────────────────────────

    /**
     * @brief 设置默认值（Default 层）。
     *
     * 若该键已有默认值，则覆盖。默认值优先级最低，会被文件配置和环境变量覆盖。
     *
     * @param key   点分隔的键名（如 "window.width"）
     * @param value 默认值
     */
    void set_default(mine::core::StringView key, ConfigValue value) noexcept;

    // ── 文件层 ───────────────────────────────────────────────────────────

    /**
     * @brief 从文件加载配置到 File 层。
     *
     * 支持 JSON 和 TOML 格式，格式由 fmt 参数决定（默认根据扩展名自动检测）。
     * 重复调用会将新值合并到 File 层（相同键覆盖旧值）。
     *
     * @param path 配置文件路径
     * @param fmt  文件格式（默认 Auto，根据扩展名判断）
     * @return Status::Ok 表示成功；IoError 表示文件读取失败；ParseError 表示解析失败
     */
    [[nodiscard]] mine::core::Status load_file(
        mine::core::StringView path,
        ConfigFormat           fmt = ConfigFormat::Auto) noexcept;

    /**
     * @brief 从字符串内容加载配置到 File 层。
     *
     * 用于从内嵌配置字符串或网络获取的配置加载。
     *
     * @param content 配置文件内容
     * @param fmt     文件格式（必须显式指定，不支持 Auto）
     * @return Status::Ok 表示成功；ParseError 表示解析失败
     */
    [[nodiscard]] mine::core::Status load_string(
        mine::core::StringView content,
        ConfigFormat           fmt) noexcept;

    /**
     * @brief 重新加载最后一次 load_file 的配置文件。
     *
     * 若从未调用 load_file，则返回 Status::Ok（空操作）。
     *
     * @return 与 load_file 相同的错误码
     */
    [[nodiscard]] mine::core::Status reload() noexcept;

    // ── 环境变量层 ───────────────────────────────────────────────────────

    /**
     * @brief 从环境变量加载配置到 Env 层（最高优先级）。
     *
     * 枚举所有以 prefix 开头的环境变量，去掉前缀后将 '_' 转换为 '.'，
     * 并全部转换为小写，作为配置键名。
     *
     * 转换示例（前缀 "APP_"）：
     *   APP_WINDOW_WIDTH=1280  →  key="window.width", value=1280（解析为整数）
     *   APP_APP_THEME=dark     →  key="app.theme", value="dark"（字符串）
     *
     * 值类型自动推断：
     *   - "true"/"false" → Bool
     *   - 纯整数字面量 → Integer
     *   - 含小数点/指数的数字 → Float
     *   - 其他 → String
     *
     * @param prefix 环境变量前缀（区分大小写，通常全大写，如 "APP_"）
     */
    void load_env(mine::core::StringView prefix) noexcept;

    // ── 层管理 ───────────────────────────────────────────────────────────

    /**
     * @brief 清除指定层的所有配置。
     *
     * @param layer 要清除的层
     */
    void clear_layer(ConfigLayer layer) noexcept;

    /**
     * @brief 清除所有层的所有配置（包括默认值）。
     */
    void clear_all() noexcept;

    // ── 读取接口 ─────────────────────────────────────────────────────────

    /**
     * @brief 检查键是否存在（任意层）。
     *
     * @param key 点分隔的键名
     * @return true 表示键存在于至少一层中
     */
    [[nodiscard]] bool has(mine::core::StringView key) const noexcept;

    /**
     * @brief 获取配置值（考虑所有层的优先级）。
     *
     * 按 Env > File > Default 顺序查找，返回第一个匹配的值。
     * 若所有层均无此键，返回 ConfigValue{}（Null 类型）。
     *
     * @param key 点分隔的键名
     * @return 对应的 ConfigValue（Null 表示键不存在）
     */
    [[nodiscard]] ConfigValue get(mine::core::StringView key) const noexcept;

    /**
     * @brief 查询某键的实际来源层（用于调试）。
     *
     * @param key 点分隔的键名
     * @return 包含该键的最高优先级层；若不存在则返回 ConfigLayer::Default
     *         但 has() 仍会返回 false
     */
    [[nodiscard]] ConfigLayer which_layer(mine::core::StringView key) const noexcept;

    // ── 类型化读取便捷方法 ───────────────────────────────────────────────

    /**
     * @brief 读取布尔值。
     *
     * @param key      键名
     * @param fallback 键不存在或类型不兼容时的回退值
     */
    [[nodiscard]] bool get_bool(
        mine::core::StringView key,
        bool fallback = false) const noexcept;

    /**
     * @brief 读取整数值（64 位有符号）。
     *
     * @param key      键名
     * @param fallback 键不存在或类型不兼容时的回退值
     */
    [[nodiscard]] int64_t get_integer(
        mine::core::StringView key,
        int64_t fallback = 0) const noexcept;

    /**
     * @brief 读取浮点值（64 位双精度）。
     *
     * @param key      键名
     * @param fallback 键不存在或类型不兼容时的回退值
     */
    [[nodiscard]] double get_float(
        mine::core::StringView key,
        double fallback = 0.0) const noexcept;

    /**
     * @brief 读取字符串值。
     *
     * 注意：返回的 StringView 生命周期与 ConfigManager 绑定，不可在
     * clear_layer()/clear_all()/reload()/load_file() 调用后继续使用。
     *
     * @param key      键名
     * @param fallback 键不存在或类型不兼容时的回退视图
     */
    [[nodiscard]] mine::core::StringView get_string(
        mine::core::StringView key,
        mine::core::StringView fallback = {}) const noexcept;

private:
    struct Impl;
    mine::core::Pimpl<Impl> p_;
};
#if defined(_MSC_VER)
#   pragma warning(pop)
#endif

} // namespace mine::config
