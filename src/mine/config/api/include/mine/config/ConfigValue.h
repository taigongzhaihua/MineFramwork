/**
 * @file ConfigValue.h
 * @brief 配置值类型：存储单个配置项的值（null / bool / integer / float / string）。
 *
 * ConfigValue 是 mine.config 的核心值类型，表示配置系统中的一个具体值。
 * 支持五种基本类型：
 *   - Null：缺省/未设置状态
 *   - Bool：布尔值
 *   - Integer：64 位有符号整数
 *   - Float：64 位浮点数
 *   - String：UTF-8 字符串（内部使用 InlineString 存储，支持 SSO）
 *
 * 设计约定：
 *   - 无 MINE_CONFIG_API 修饰：值类型纯头文件可见，无 DLL 接口问题
 *   - 不抛异常；类型不匹配访问时通过 MINE_ASSERT 断言终止
 *   - as_xxx() 要求类型精确匹配；to_xxx() 允许合理的类型提升/转换
 */

#pragma once

#include <cstdint>
#include <cstring>
#include <new>
#include <type_traits>
#include <mine/core/Assert.h>
#include <mine/core/StringView.h>
#include <mine/containers/InlineString.h>

namespace mine::config {

// ─────────────────────────────────────────────────────────────────────────────
// ConfigValueType：值类型标签
// ─────────────────────────────────────────────────────────────────────────────

/**
 * @brief 配置值类型枚举。
 */
enum class ConfigValueType : uint8_t {
    Null    = 0,  ///< 空/未设置
    Bool    = 1,  ///< 布尔值
    Integer = 2,  ///< 64 位有符号整数
    Float   = 3,  ///< 64 位浮点数
    String  = 4,  ///< UTF-8 字符串
};

// ─────────────────────────────────────────────────────────────────────────────
// ConfigValue：类型安全的配置值
// ─────────────────────────────────────────────────────────────────────────────

/**
 * @brief 类型安全的配置项值。
 *
 * 内存布局：
 *   - type_：1 字节类型标签
 *   - 内部 union：最大 32 字节（InlineString 大小），对齐 8 字节
 *
 * 字符串管理：在 union 内部用 placement new 就地构造 InlineString，
 * 析构时手动调用析构函数，保证正确的 SSO / 堆内存释放。
 */
class ConfigValue {
public:
    // ── 构造 ─────────────────────────────────────────────────────────────

    /// 默认构造：Null 类型
    ConfigValue() noexcept : type_{ConfigValueType::Null} {}

    /// 从 bool 构造
    explicit ConfigValue(bool v) noexcept
        : type_{ConfigValueType::Bool}
    {
        bool_ = v;
    }

    /// 从 int64_t 构造
    explicit ConfigValue(int64_t v) noexcept
        : type_{ConfigValueType::Integer}
    {
        integer_ = v;
    }

    /// 从 int 构造（隐式提升为 int64_t）
    explicit ConfigValue(int v) noexcept
        : type_{ConfigValueType::Integer}
    {
        integer_ = static_cast<int64_t>(v);
    }

    /// 从 double 构造
    explicit ConfigValue(double v) noexcept
        : type_{ConfigValueType::Float}
    {
        float_ = v;
    }

    /// 从 StringView 构造（内部拷贝字符串数据）
    ConfigValue(mine::core::StringView v) noexcept  // NOLINT(google-explicit-constructor)
        : type_{ConfigValueType::String}
    {
        new(str_storage_) mine::containers::InlineString(v);
    }

    /// 从 C 字符串字面量构造（拷贝字符串数据）
    ConfigValue(const char* v) noexcept  // NOLINT(google-explicit-constructor)
        : type_{ConfigValueType::String}
    {
        new(str_storage_) mine::containers::InlineString(v ? v : "");
    }

    // ── 拷贝/移动 ────────────────────────────────────────────────────────

    ConfigValue(const ConfigValue& other) noexcept
        : type_{other.type_}
    {
        copy_from(other);
    }

    ConfigValue(ConfigValue&& other) noexcept
        : type_{other.type_}
    {
        move_from(std::move(other));
    }

    ConfigValue& operator=(const ConfigValue& other) noexcept {
        if (this != &other) {
            destroy_string_if_needed();
            type_ = other.type_;
            copy_from(other);
        }
        return *this;
    }

    ConfigValue& operator=(ConfigValue&& other) noexcept {
        if (this != &other) {
            destroy_string_if_needed();
            type_ = other.type_;
            move_from(std::move(other));
        }
        return *this;
    }

    ~ConfigValue() noexcept {
        destroy_string_if_needed();
    }

    // ── 类型查询 ─────────────────────────────────────────────────────────

    /// 获取值类型
    [[nodiscard]] ConfigValueType type() const noexcept { return type_; }

    [[nodiscard]] bool is_null()    const noexcept { return type_ == ConfigValueType::Null; }
    [[nodiscard]] bool is_bool()    const noexcept { return type_ == ConfigValueType::Bool; }
    [[nodiscard]] bool is_integer() const noexcept { return type_ == ConfigValueType::Integer; }
    [[nodiscard]] bool is_float()   const noexcept { return type_ == ConfigValueType::Float; }
    [[nodiscard]] bool is_string()  const noexcept { return type_ == ConfigValueType::String; }

    // ── 精确类型访问（类型不匹配时断言）────────────────────────────────

    /**
     * @brief 获取布尔值（要求 type == Bool）。
     */
    [[nodiscard]] bool as_bool() const noexcept {
        MINE_ASSERT_MSG(is_bool(), "ConfigValue::as_bool(): 类型不是 Bool");
        return bool_;
    }

    /**
     * @brief 获取整数值（要求 type == Integer）。
     */
    [[nodiscard]] int64_t as_integer() const noexcept {
        MINE_ASSERT_MSG(is_integer(), "ConfigValue::as_integer(): 类型不是 Integer");
        return integer_;
    }

    /**
     * @brief 获取浮点值（要求 type == Float）。
     */
    [[nodiscard]] double as_float() const noexcept {
        MINE_ASSERT_MSG(is_float(), "ConfigValue::as_float(): 类型不是 Float");
        return float_;
    }

    /**
     * @brief 获取字符串视图（要求 type == String）。
     *
     * 返回的 StringView 生命周期与此 ConfigValue 绑定，不可在 ConfigValue 析构后使用。
     */
    [[nodiscard]] mine::core::StringView as_string() const noexcept {
        MINE_ASSERT_MSG(is_string(), "ConfigValue::as_string(): 类型不是 String");
        return str_ref();
    }

    // ── 带回退的类型转换访问 ─────────────────────────────────────────────

    /**
     * @brief 将值转换为 bool（带回退）。
     *
     * 转换规则：
     *   - Bool    → 直接返回
     *   - Integer → 非零为 true
     *   - Float   → 非零为 true
     *   - String  → "true"/"1" → true，"false"/"0" → false，其余 fallback
     *   - Null    → fallback
     */
    [[nodiscard]] bool to_bool(bool fallback = false) const noexcept {
        switch (type_) {
        case ConfigValueType::Bool:    return bool_;
        case ConfigValueType::Integer: return integer_ != 0;
        case ConfigValueType::Float:   return float_ != 0.0;
        case ConfigValueType::String: {
            const auto sv = str_ref();
            if (sv == "true"  || sv == "1") return true;
            if (sv == "false" || sv == "0") return false;
            return fallback;
        }
        default: return fallback;
        }
    }

    /**
     * @brief 将值转换为 int64_t（带回退）。
     *
     * 转换规则：
     *   - Integer → 直接返回
     *   - Bool    → 1 或 0
     *   - Float   → 截断为整数
     *   - String  → 解析为整数，失败返回 fallback
     *   - Null    → fallback
     */
    [[nodiscard]] int64_t to_integer(int64_t fallback = 0) const noexcept {
        switch (type_) {
        case ConfigValueType::Integer: return integer_;
        case ConfigValueType::Bool:    return bool_ ? 1 : 0;
        case ConfigValueType::Float:   return static_cast<int64_t>(float_);
        case ConfigValueType::String: {
            const auto sv = str_ref();
            if (sv.empty()) return fallback;
            // 手动解析整数（避免 strtoll 的异常问题）
            int64_t result = 0;
            bool negative = false;
            const char* p = sv.data();
            const char* end = p + sv.size();
            if (p < end && (*p == '-' || *p == '+')) {
                negative = (*p == '-');
                ++p;
            }
            if (p == end) return fallback;
            for (; p < end; ++p) {
                if (*p < '0' || *p > '9') return fallback;
                result = result * 10 + (*p - '0');
            }
            return negative ? -result : result;
        }
        default: return fallback;
        }
    }

    /**
     * @brief 将值转换为 double（带回退）。
     *
     * 转换规则：
     *   - Float   → 直接返回
     *   - Integer → 提升为 double
     *   - Bool    → 1.0 或 0.0
     *   - String  → 解析为浮点，失败返回 fallback
     *   - Null    → fallback
     */
    [[nodiscard]] double to_float(double fallback = 0.0) const noexcept {
        switch (type_) {
        case ConfigValueType::Float:   return float_;
        case ConfigValueType::Integer: return static_cast<double>(integer_);
        case ConfigValueType::Bool:    return bool_ ? 1.0 : 0.0;
        case ConfigValueType::String: {
            const auto sv = str_ref();
            if (sv.empty()) return fallback;
            // 使用 strtod 解析（标准 C 函数，无异常）
            // 需要 null 终止字符串；InlineString 保证 null 终止
            char* endptr = nullptr;
            double result = strtod(sv.data(), &endptr);
            if (endptr == sv.data() || endptr != sv.data() + sv.size()) {
                return fallback;
            }
            return result;
        }
        default: return fallback;
        }
    }

    /**
     * @brief 将值转换为 StringView（带回退）。
     *
     * 仅对 String 类型有效，其他类型返回 fallback。
     * （数值转字符串需要缓冲区，不在此提供）
     */
    [[nodiscard]] mine::core::StringView to_string(
        mine::core::StringView fallback = {}) const noexcept
    {
        if (is_string()) return str_ref();
        return fallback;
    }

    // ── 相等比较 ─────────────────────────────────────────────────────────

    [[nodiscard]] bool operator==(const ConfigValue& other) const noexcept {
        if (type_ != other.type_) return false;
        switch (type_) {
        case ConfigValueType::Null:    return true;
        case ConfigValueType::Bool:    return bool_ == other.bool_;
        case ConfigValueType::Integer: return integer_ == other.integer_;
        case ConfigValueType::Float:   return float_ == other.float_;
        case ConfigValueType::String:  return str_ref() == other.str_ref();
        default: return false;
        }
    }

    [[nodiscard]] bool operator!=(const ConfigValue& other) const noexcept {
        return !(*this == other);
    }

private:
    // ── 内部辅助：字符串存储管理 ─────────────────────────────────────────

    /// 获取字符串存储的 InlineString 引用（仅在 type == String 时有效）
    [[nodiscard]] mine::containers::InlineString& str_ref() noexcept {
        return *std::launder(
            reinterpret_cast<mine::containers::InlineString*>(str_storage_));
    }

    [[nodiscard]] const mine::containers::InlineString& str_ref() const noexcept {
        return *std::launder(
            reinterpret_cast<const mine::containers::InlineString*>(str_storage_));
    }

    /// 若当前类型为 String，析构内部 InlineString（释放堆内存）
    void destroy_string_if_needed() noexcept {
        if (type_ == ConfigValueType::String) {
            str_ref().~InlineString();
        }
    }

    /// 从 other 拷贝数据（调用前必须确保当前对象的字符串已析构）
    void copy_from(const ConfigValue& other) noexcept {
        switch (other.type_) {
        case ConfigValueType::Null:                                       break;
        case ConfigValueType::Bool:    bool_    = other.bool_;            break;
        case ConfigValueType::Integer: integer_ = other.integer_;         break;
        case ConfigValueType::Float:   float_   = other.float_;           break;
        case ConfigValueType::String:
            new(str_storage_) mine::containers::InlineString(other.str_ref());
            break;
        }
    }

    /// 从 other 移动数据（调用前必须确保当前对象的字符串已析构）
    void move_from(ConfigValue&& other) noexcept {
        switch (other.type_) {
        case ConfigValueType::Null:                                           break;
        case ConfigValueType::Bool:    bool_    = other.bool_;                break;
        case ConfigValueType::Integer: integer_ = other.integer_;             break;
        case ConfigValueType::Float:   float_   = other.float_;               break;
        case ConfigValueType::String:
            new(str_storage_) mine::containers::InlineString(
                std::move(other.str_ref()));
            // 将 other 置为 Null（避免二次析构字符串）
            other.str_ref().~InlineString();
            other.type_ = ConfigValueType::Null;
            break;
        }
    }

    // ── 成员字段 ─────────────────────────────────────────────────────────

    /// 当前值的类型
    ConfigValueType type_{ConfigValueType::Null};

    /// 值存储 union：
    ///   - bool_/integer_/float_ 用于对应基本类型
    ///   - str_storage_ 用于就地存储 InlineString（placement new 管理生命周期）
    union {
        bool    bool_;      ///< Bool 类型的值
        int64_t integer_;   ///< Integer 类型的值
        double  float_;     ///< Float 类型的值
        // 字符串存储：与 InlineString 对齐，大小相同
        alignas(mine::containers::InlineString)
        unsigned char str_storage_[sizeof(mine::containers::InlineString)];
    };
};

} // namespace mine::config
