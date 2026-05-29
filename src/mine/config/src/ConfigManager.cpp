/**
 * @file ConfigManager.cpp
 * @brief 分层配置管理器实现。
 *
 * 实现了三层优先级的配置系统（Env > File > Default），
 * 支持 JSON/TOML 文件加载和环境变量覆盖。
 *
 * 文件读取：使用 C 标准库 FILE*（不依赖 mine.io）
 * 字符串存储：使用 mine::containers::InlineString + HashMap
 */

#include <mine/config/ConfigManager.h>
#include "JsonParser.h"
#include "TomlParser.h"

#include <mine/core/Pimpl.h>
#include <mine/containers/HashMap.h>
#include <mine/containers/InlineString.h>
#include <mine/containers/Containers.h>   // Hash<InlineString> 特化
#include <mine/containers/Vector.h>

#include <cstdio>
#include <cstring>
#include <cctype>

// 抑制 MSVC fopen 过时警告
#if defined(_MSC_VER)
#   pragma warning(disable: 4996)
#endif

// Windows 环境变量 API：手动前向声明，避免包含 windows.h（WIN32_LEAN_AND_MEAN 兼容问题）
#if defined(_WIN32)
extern "C" {
    __declspec(dllimport) char* __stdcall GetEnvironmentStringsA();
    __declspec(dllimport) int   __stdcall FreeEnvironmentStringsA(char*);
}
#endif

namespace mine::config {

// ─────────────────────────────────────────────────────────────────────────────
// ConfigManager::Impl：内部实现
// ─────────────────────────────────────────────────────────────────────────────

struct ConfigManager::Impl {
    // 三层 HashMap
    mine::containers::HashMap<mine::containers::InlineString, ConfigValue> default_layer_;
    mine::containers::HashMap<mine::containers::InlineString, ConfigValue> file_layer_;
    mine::containers::HashMap<mine::containers::InlineString, ConfigValue> env_layer_;

    // 记录最后加载的文件路径和格式（用于 reload）
    mine::containers::InlineString last_file_path_;
    ConfigFormat                   last_file_format_{ConfigFormat::Auto};
    bool                           has_last_file_{false};

    /**
     * @brief 按优先级查找键（Env > File > Default）。
     *
     * @param key 点分隔键名
     * @return 指向值的指针，未找到返回 nullptr
     */
    const ConfigValue* find(mine::core::StringView key) const noexcept {
        // 用 StringView 查找时，HashMap 会通过 Hash<InlineString> 接受
        // 但 find(K&) 要求 InlineString，需要构造临时对象
        mine::containers::InlineString k{key};

        if (const ConfigValue* v = env_layer_.find(k))     return v;
        if (const ConfigValue* v = file_layer_.find(k))    return v;
        if (const ConfigValue* v = default_layer_.find(k)) return v;
        return nullptr;
    }

    /**
     * @brief 检查某键在哪一层（返回最高优先级层，无则返回 Default）。
     */
    ConfigLayer which_layer(mine::core::StringView key) const noexcept {
        mine::containers::InlineString k{key};
        if (env_layer_.contains(k))     return ConfigLayer::Env;
        if (file_layer_.contains(k))    return ConfigLayer::File;
        if (default_layer_.contains(k)) return ConfigLayer::Default;
        return ConfigLayer::Default;  // 不存在也返回 Default（调用方应先 has() 检查）
    }
};

// ─────────────────────────────────────────────────────────────────────────────
// 内部工具函数（文件级匿名命名空间）
// ─────────────────────────────────────────────────────────────────────────────

namespace {

/**
 * @brief 读取文件全部内容到缓冲区。
 *
 * @param path    文件路径（null 终止字符串）
 * @param content 输出缓冲区（追加内容，调用前应为空）
 * @return Status::Ok 或 Status::IoError
 */
mine::core::Status read_file_content(
    const char*                      path,
    mine::containers::Vector<char>&  content) noexcept
{
    FILE* f = fopen(path, "rb");
    if (!f) return mine::core::Status{mine::core::StatusCode::IoError};

    // 获取文件大小
    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fseek(f, 0, SEEK_SET);

    if (size < 0) {
        fclose(f);
        return mine::core::Status{mine::core::StatusCode::IoError};
    }

    size_t file_size = static_cast<size_t>(size);

    // 分配缓冲区（+1 用于追加 null 字节，方便 strtod 等函数使用）
    content.resize(file_size + 1);

    size_t read = fread(content.data(), 1, file_size, f);
    fclose(f);

    if (read != file_size) {
        content.resize(0);
        return mine::core::Status{mine::core::StatusCode::IoError};
    }

    content[file_size] = '\0';  // 添加 null 终止
    content.resize(file_size);  // 报告有效大小（不含 null）
    return {};
}

/**
 * @brief 根据文件路径的扩展名自动检测配置格式。
 *
 * 支持识别 .json 和 .toml；未识别则默认返回 JSON。
 *
 * @param path 文件路径（StringView）
 * @return ConfigFormat::Json 或 ConfigFormat::Toml
 */
ConfigFormat detect_format(mine::core::StringView path) noexcept {
    // 从末尾反向查找 '.' 作为扩展名分隔符
    for (int i = static_cast<int>(path.size()) - 1; i >= 0; --i) {
        char c = path[static_cast<size_t>(i)];
        if (c == '.' ) {
            mine::core::StringView ext{
                path.data() + static_cast<size_t>(i),
                path.size()  - static_cast<size_t>(i)};

            if (ext.size() == 5 &&
                (ext[1] == 'j' || ext[1] == 'J') &&
                (ext[2] == 's' || ext[2] == 'S') &&
                (ext[3] == 'o' || ext[3] == 'O') &&
                (ext[4] == 'n' || ext[4] == 'N'))
            {
                return ConfigFormat::Json;
            }
            if (ext.size() == 5 &&
                (ext[1] == 't' || ext[1] == 'T') &&
                (ext[2] == 'o' || ext[2] == 'O') &&
                (ext[3] == 'm' || ext[3] == 'M') &&
                (ext[4] == 'l' || ext[4] == 'L'))
            {
                return ConfigFormat::Toml;
            }
            break;  // 识别到扩展名但不支持，停止搜索
        }
        // 遇到路径分隔符则停止
        if (c == '/' || c == '\\') break;
    }
    return ConfigFormat::Json;  // 默认 JSON
}

/**
 * @brief 将环境变量值字符串推断为 ConfigValue。
 *
 * 推断规则：
 *   - "true"/"false"（不区分大小写）→ Bool
 *   - 纯整数字面量 → Integer
 *   - 含小数点/指数的数字 → Float
 *   - 其他 → String
 *
 * @param value_str null 终止的值字符串
 */
ConfigValue infer_env_value(const char* value_str) noexcept {
    if (!value_str || value_str[0] == '\0') {
        return ConfigValue{mine::core::StringView{}};
    }

    // 比较时忽略大小写的工具 lambda（C-style）
    auto icase_eq = [](const char* a, const char* b) noexcept -> bool {
        while (*a && *b) {
            char ca = static_cast<char>((*a >= 'A' && *a <= 'Z') ? *a + 32 : *a);
            char cb = static_cast<char>((*b >= 'A' && *b <= 'Z') ? *b + 32 : *b);
            if (ca != cb) return false;
            ++a; ++b;
        }
        return *a == '\0' && *b == '\0';
    };

    if (icase_eq(value_str, "true"))  return ConfigValue{true};
    if (icase_eq(value_str, "false")) return ConfigValue{false};

    // 尝试数字
    const char* p = value_str;
    bool is_float = false;
    if (*p == '-' || *p == '+') ++p;
    const char* int_start = p;
    while (*p >= '0' && *p <= '9') ++p;
    if (p == int_start) {
        // 非数字开头，视为字符串
        return ConfigValue{mine::core::StringView{value_str}};
    }
    if (*p == '.') {
        is_float = true;
        ++p;
        while (*p >= '0' && *p <= '9') ++p;
    }
    if (*p == 'e' || *p == 'E') {
        is_float = true;
        ++p;
        if (*p == '+' || *p == '-') ++p;
        while (*p >= '0' && *p <= '9') ++p;
    }

    if (*p != '\0') {
        // 有非数字尾缀，视为字符串
        return ConfigValue{mine::core::StringView{value_str}};
    }

    if (is_float) {
        char* ep = nullptr;
        double val = strtod(value_str, &ep);
        return ConfigValue{val};
    } else {
        bool negative = (*value_str == '-');
        const char* sp = value_str;
        if (*sp == '-' || *sp == '+') ++sp;
        int64_t val = 0;
        for (; *sp != '\0'; ++sp) val = val * 10 + static_cast<int64_t>(*sp - '0');
        return ConfigValue{negative ? -val : val};
    }
}

/**
 * @brief 解析回调的用户数据结构。
 *
 * 将 key→value 写入目标 HashMap。
 */
struct InsertContext {
    mine::containers::HashMap<mine::containers::InlineString, ConfigValue>* target;
};

/// 解析回调：将键值对写入 InsertContext 指向的 HashMap
static void insert_callback(void* ud,
                             mine::core::StringView key,
                             ConfigValue value) noexcept
{
    auto* ctx = static_cast<InsertContext*>(ud);
    mine::containers::InlineString k{key};
    ctx->target->insert_or_assign(std::move(k), std::move(value));
}

/**
 * @brief 将环境变量名（去掉前缀后）转换为点分隔键名。
 *
 * 转换规则：
 *   1. 全部转为小写
 *   2. 将 '_' 替换为 '.'
 *
 * 示例：WINDOW_WIDTH → window.width
 *
 * @param src     源字符串（去掉前缀后的部分）
 * @param src_len 源字符串长度
 * @param buf     输出缓冲区
 * @param buf_cap 缓冲区容量
 * @param out_len 输出有效长度
 */
void env_name_to_key(const char* src, size_t src_len,
                     char* buf, size_t buf_cap,
                     size_t& out_len) noexcept
{
    out_len = 0;
    for (size_t i = 0; i < src_len && out_len + 1 < buf_cap; ++i) {
        char c = src[i];
        if (c == '_') {
            buf[out_len++] = '.';
        } else if (c >= 'A' && c <= 'Z') {
            buf[out_len++] = static_cast<char>(c + 32);  // 转小写
        } else {
            buf[out_len++] = c;
        }
    }
    if (out_len < buf_cap) buf[out_len] = '\0';
}

} // namespace（匿名）

// ─────────────────────────────────────────────────────────────────────────────
// ConfigManager 构造/析构
// ─────────────────────────────────────────────────────────────────────────────

ConfigManager::ConfigManager()
    : p_{mine::core::make_pimpl<Impl>()}
{}

ConfigManager::~ConfigManager() = default;

// ─────────────────────────────────────────────────────────────────────────────
// 默认值层
// ─────────────────────────────────────────────────────────────────────────────

void ConfigManager::set_default(mine::core::StringView key, ConfigValue value) noexcept {
    mine::containers::InlineString k{key};
    p_->default_layer_.insert_or_assign(std::move(k), std::move(value));
}

// ─────────────────────────────────────────────────────────────────────────────
// 文件层
// ─────────────────────────────────────────────────────────────────────────────

mine::core::Status ConfigManager::load_file(mine::core::StringView path,
                                             ConfigFormat           fmt) noexcept
{
    // 记录路径以支持 reload
    p_->last_file_path_   = path;
    p_->last_file_format_ = fmt;
    p_->has_last_file_    = true;

    // 读取文件内容
    mine::containers::Vector<char> content;
    {
        mine::containers::InlineString path_cstr{path};  // 保证 null 终止
        mine::core::Status st = read_file_content(path_cstr.data(), content);
        if (!st.ok()) return st;
    }

    mine::core::StringView text{content.data(), content.size()};

    // 确定实际格式
    ConfigFormat actual_fmt = (fmt == ConfigFormat::Auto)
        ? detect_format(path)
        : fmt;

    // 解析
    InsertContext ctx{&p_->file_layer_};

    switch (actual_fmt) {
    case ConfigFormat::Json:
        return detail::parse_json(text, insert_callback, &ctx);
    case ConfigFormat::Toml:
        return detail::parse_toml(text, insert_callback, &ctx);
    default:
        return detail::parse_json(text, insert_callback, &ctx);
    }
}

mine::core::Status ConfigManager::load_string(mine::core::StringView content,
                                               ConfigFormat           fmt) noexcept
{
    InsertContext ctx{&p_->file_layer_};

    switch (fmt) {
    case ConfigFormat::Json:
        return detail::parse_json(content, insert_callback, &ctx);
    case ConfigFormat::Toml:
        return detail::parse_toml(content, insert_callback, &ctx);
    default:
        return detail::parse_json(content, insert_callback, &ctx);
    }
}

mine::core::Status ConfigManager::reload() noexcept {
    if (!p_->has_last_file_) return {};  // 从未加载过文件，空操作

    // 清除现有 File 层数据
    p_->file_layer_.clear();

    return load_file(p_->last_file_path_, p_->last_file_format_);
}

// ─────────────────────────────────────────────────────────────────────────────
// 环境变量层
// ─────────────────────────────────────────────────────────────────────────────

void ConfigManager::load_env(mine::core::StringView prefix) noexcept {
    // 前缀转为临时缓冲区（可能需要大小写比较）
    char prefix_buf[256] = {};
    size_t prefix_len = prefix.size() < sizeof(prefix_buf) - 1
        ? prefix.size() : sizeof(prefix_buf) - 1;
    memcpy(prefix_buf, prefix.data(), prefix_len);
    prefix_buf[prefix_len] = '\0';

#if defined(_WIN32)
    // Windows：使用 Win32 GetEnvironmentStringsA 枚举所有环境变量
    char* env_block = GetEnvironmentStringsA();
    if (!env_block) return;

    const char* p = env_block;
    while (*p != '\0') {
        const char* entry = p;

        // 查找 '=' 分隔符
        const char* eq = entry;
        while (*eq && *eq != '=') ++eq;

        if (*eq == '=') {
            // 比较前缀（区分大小写）
            size_t name_len = static_cast<size_t>(eq - entry);
            bool has_prefix = (name_len >= prefix_len) &&
                              (memcmp(entry, prefix_buf, prefix_len) == 0);

            if (has_prefix) {
                const char* name_rest     = entry + prefix_len;
                size_t      name_rest_len = name_len - prefix_len;

                char key_buf[512] = {};
                size_t key_len = 0;
                env_name_to_key(name_rest, name_rest_len,
                                key_buf, sizeof(key_buf), key_len);

                if (key_len > 0) {
                    const char* value_str = eq + 1;
                    ConfigValue val = infer_env_value(value_str);

                    mine::containers::InlineString k{
                        mine::core::StringView{key_buf, key_len}};
                    p_->env_layer_.insert_or_assign(std::move(k), std::move(val));
                }
            }
        }

        // 越过 null 终止符跳到下一条目
        while (*p) ++p;
        ++p;
    }

    ::FreeEnvironmentStringsA(env_block);

#else
    // POSIX：使用 environ 全局变量
    extern char** environ;
    char** envp = environ;

    if (!envp) return;

    for (char** ep = envp; *ep != nullptr; ++ep) {
        const char* entry = *ep;

        // 查找 '=' 分隔符
        const char* eq = entry;
        while (*eq && *eq != '=') ++eq;
        if (*eq != '=') continue;

        // 比较前缀（区分大小写）
        size_t name_len = static_cast<size_t>(eq - entry);
        bool has_prefix = (name_len >= prefix_len) &&
                          (memcmp(entry, prefix_buf, prefix_len) == 0);

        if (has_prefix) {
            const char* name_rest     = entry + prefix_len;
            size_t      name_rest_len = name_len - prefix_len;

            char key_buf[512] = {};
            size_t key_len = 0;
            env_name_to_key(name_rest, name_rest_len,
                            key_buf, sizeof(key_buf), key_len);

            if (key_len > 0) {
                const char* value_str = eq + 1;
                ConfigValue val = infer_env_value(value_str);

                mine::containers::InlineString k{
                    mine::core::StringView{key_buf, key_len}};
                p_->env_layer_.insert_or_assign(std::move(k), std::move(val));
            }
        }
    }

#endif
}

// ─────────────────────────────────────────────────────────────────────────────
// 层管理
// ─────────────────────────────────────────────────────────────────────────────

void ConfigManager::clear_layer(ConfigLayer layer) noexcept {
    switch (layer) {
    case ConfigLayer::Default:
        p_->default_layer_.clear();
        break;
    case ConfigLayer::File:
        p_->file_layer_.clear();
        break;
    case ConfigLayer::Env:
        p_->env_layer_.clear();
        break;
    }
}

void ConfigManager::clear_all() noexcept {
    p_->default_layer_.clear();
    p_->file_layer_.clear();
    p_->env_layer_.clear();
}

// ─────────────────────────────────────────────────────────────────────────────
// 读取接口
// ─────────────────────────────────────────────────────────────────────────────

bool ConfigManager::has(mine::core::StringView key) const noexcept {
    return p_->find(key) != nullptr;
}

ConfigValue ConfigManager::get(mine::core::StringView key) const noexcept {
    const ConfigValue* v = p_->find(key);
    return v ? *v : ConfigValue{};
}

ConfigLayer ConfigManager::which_layer(mine::core::StringView key) const noexcept {
    return p_->which_layer(key);
}

bool ConfigManager::get_bool(mine::core::StringView key,
                              bool fallback) const noexcept
{
    const ConfigValue* v = p_->find(key);
    return v ? v->to_bool(fallback) : fallback;
}

int64_t ConfigManager::get_integer(mine::core::StringView key,
                                    int64_t fallback) const noexcept
{
    const ConfigValue* v = p_->find(key);
    return v ? v->to_integer(fallback) : fallback;
}

double ConfigManager::get_float(mine::core::StringView key,
                                 double fallback) const noexcept
{
    const ConfigValue* v = p_->find(key);
    return v ? v->to_float(fallback) : fallback;
}

mine::core::StringView ConfigManager::get_string(mine::core::StringView key,
                                                   mine::core::StringView fallback) const noexcept
{
    const ConfigValue* v = p_->find(key);
    if (!v || !v->is_string()) return fallback;
    return v->as_string();
}

} // namespace mine::config
