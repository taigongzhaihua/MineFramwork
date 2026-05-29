/**
 * @file LocalizationManager.cpp
 * @brief 本地化管理器实现。
 *
 * 核心数据结构：
 *   所有翻译条目存储在单一 HashMap 中，键为复合格式：
 *   "语言代码\x01翻译键"（\x01 = SOH，不会出现在合法语言代码或翻译键中）。
 *
 * 资源格式支持：
 *   - JSON：{"翻译键":"翻译文本",...}（仅支持平面对象，嵌套对象与非字符串值被跳过）
 *   - KeyValue：翻译键=翻译文本（每行一条，# 或 ; 开头为注释行）
 */

#include <mine/localization/LocalizationManager.h>

#include <mine/core/Pimpl.h>
#include <mine/containers/HashMap.h>
#include <mine/containers/InlineString.h>
#include <mine/containers/Containers.h>   // Hash<InlineString> 特化
#include <mine/containers/Vector.h>

#include <cstring>
#include <cctype>
#include <cstdint>

#if defined(_MSC_VER)
#   pragma warning(disable: 4996)   // 抑制 MSVC 弃用警告（如 strcpy 等）
#endif

namespace mine::localization {

// ─────────────────────────────────────────────────────────────────────────────
// Impl：内部实现结构
// ─────────────────────────────────────────────────────────────────────────────

struct LocalizationManager::Impl {
    // 翻译字典：复合键 = "语言代码\x01翻译键" → 翻译文本
    mine::containers::HashMap<mine::containers::InlineString,
                               mine::containers::InlineString> translations_;

    // 已加载语言代码列表（插入顺序，去重），用于 get_available_languages
    mine::containers::Vector<mine::containers::InlineString> languages_;

    // 当前活跃语言（空字符串表示未设置）
    mine::containers::InlineString current_language_;

    // 回退语言（默认 "en"；查当前语言未命中时继续查此语言）
    mine::containers::InlineString fallback_language_{
        mine::core::StringView{"en", 2}};

    // 语言切换通知订阅者列表
    struct Subscriber {
        LanguageChangedCallback callback = nullptr;
        void*                   userdata = nullptr;
        uint32_t                token    = 0;
    };
    mine::containers::Vector<Subscriber> subscribers_;
    uint32_t next_token_ = 1;  ///< 下一个订阅 token（从 1 开始，0 表示无效）

    /**
     * @brief 在指定语言的字典中查找翻译键。
     *
     * @return 翻译文本视图（指向内部存储），未找到时返回空视图
     */
    mine::core::StringView lookup_in(mine::core::StringView lang,
                                      mine::core::StringView key) const noexcept;

    /**
     * @brief 按两层回退策略查找翻译：当前语言 → 回退语言 → 空视图。
     */
    mine::core::StringView lookup(mine::core::StringView key) const noexcept;
};

// ─────────────────────────────────────────────────────────────────────────────
// 匿名命名空间：内部工具函数与解析函数
// ─────────────────────────────────────────────────────────────────────────────

// 翻译字典和语言列表的类型别名（减少冗长的类型名）
using TranslationMap = mine::containers::HashMap<mine::containers::InlineString,
                                                   mine::containers::InlineString>;
using LanguageList   = mine::containers::Vector<mine::containers::InlineString>;

namespace {

/**
 * @brief 构造复合键：语言代码 + '\x01' + 翻译键。
 *
 * \x01（SOH）是不可见控制字符，不会出现在合法的语言代码或翻译键中，
 * 因此可安全用作分隔符。
 */
mine::containers::InlineString make_composite_key(
    mine::core::StringView lang,
    mine::core::StringView key) noexcept
{
    char buf[640];

    // 截断过长的部分（防止缓冲区溢出）
    size_t lang_len = lang.size() < 128 ? lang.size() : 128;
    size_t key_len  = key.size()  < 510 ? key.size()  : 510;

    memcpy(buf, lang.data(), lang_len);
    buf[lang_len] = '\x01';
    memcpy(buf + lang_len + 1, key.data(), key_len);
    buf[lang_len + 1 + key_len] = '\0';

    return mine::containers::InlineString{
        mine::core::StringView{buf, lang_len + 1 + key_len}};
}

/**
 * @brief 确保语言代码已在 languages 列表中（去重追加）。
 */
void ensure_language_in(mine::core::StringView lang, LanguageList& languages) noexcept {
    for (const auto& l : languages) {
        if (mine::core::StringView{l.data(), l.size()} == lang) return;
    }
    languages.push_back(mine::containers::InlineString{lang});
}

/// 跳过 ASCII 空白字符（空格、Tab、CR、LF）
const char* skip_whitespace(const char* p, const char* end) noexcept {
    while (p < end &&
           (*p == ' ' || *p == '\t' || *p == '\r' || *p == '\n')) {
        ++p;
    }
    return p;
}

/**
 * @brief 解析 JSON 字符串字面量（p 指向开头 '"' 之后的位置）。
 *
 * 处理常见转义序列（\n、\t、\\、\"、\/、\r、\b、\f）；
 * \uXXXX 仅处理 U+0000~U+007F 范围（直接取低字节），超出范围的跳过。
 * UTF-8 多字节序列原样复制。
 *
 * @param p       当前位置（解析结束后更新为闭合 '"' 之后）
 * @param end     数据末尾
 * @param out     输出缓冲区
 * @param cap     缓冲区容量（含终止符）
 * @param out_len 输出：实际写入字节数（不含终止符）
 * @return        true 解析成功；false 字符串格式错误（未闭合等）
 */
bool parse_json_string(const char*& p, const char* end,
                        char* out, size_t cap,
                        size_t& out_len) noexcept
{
    out_len = 0;
    while (p < end && *p != '"') {
        if (*p == '\\') {
            ++p;
            if (p >= end) return false;
            char c;
            switch (*p) {
                case '"':  c = '"';  break;
                case '\\': c = '\\'; break;
                case '/':  c = '/';  break;
                case 'n':  c = '\n'; break;
                case 'r':  c = '\r'; break;
                case 't':  c = '\t'; break;
                case 'b':  c = '\b'; break;
                case 'f':  c = '\f'; break;
                case 'u': {
                    // 解析 \uXXXX（4 位十六进制）
                    uint32_t cp = 0;
                    for (int i = 0; i < 4 && p + 1 < end; ++i) {
                        ++p;
                        char hex = *p;
                        if      (hex >= '0' && hex <= '9') cp = cp * 16 + (hex - '0');
                        else if (hex >= 'a' && hex <= 'f') cp = cp * 16 + (hex - 'a' + 10);
                        else if (hex >= 'A' && hex <= 'F') cp = cp * 16 + (hex - 'A' + 10);
                    }
                    // 仅处理 ASCII 范围
                    if (cp < 0x80 && out_len < cap - 1) {
                        out[out_len++] = static_cast<char>(cp);
                    }
                    ++p;
                    continue; // 外层循环会再执行 ++p
                }
                default: c = *p; break;
            }
            if (out_len < cap - 1) out[out_len++] = c;
        } else {
            // UTF-8 多字节序列：根据首字节确定后续字节数，整段复制
            unsigned char uc = static_cast<unsigned char>(*p);
            int extra = 0;
            if      ((uc & 0xE0) == 0xC0) extra = 1;
            else if ((uc & 0xF0) == 0xE0) extra = 2;
            else if ((uc & 0xF8) == 0xF0) extra = 3;

            if (out_len < cap - 1) out[out_len++] = *p;
            for (int i = 0; i < extra && p + 1 < end; ++i) {
                ++p;
                if (out_len < cap - 1) out[out_len++] = *p;
            }
        }
        ++p;
    }

    if (p >= end || *p != '"') return false; // 未闭合
    ++p; // 跳过闭合 '"'
    if (out_len < cap) out[out_len] = '\0';
    return true;
}

/**
 * @brief 跳过 JSON 中的非字符串值（数字、bool、null、对象、数组）。
 *
 * 简单实现：按括号层级追踪，遇到顶层的 ',' 或 '}' 时停止。
 */
void skip_json_value(const char*& p, const char* end) noexcept {
    int depth   = 0;
    bool in_str = false;
    while (p < end) {
        char c = *p;
        if (in_str) {
            if (c == '\\' && p + 1 < end) { p += 2; continue; }
            if (c == '"') in_str = false;
        } else {
            if (c == '"') { in_str = true; }
            else if (c == '{' || c == '[') { ++depth; }
            else if (c == '}' || c == ']') {
                if (depth == 0) return; // 不消耗该字符
                --depth;
            }
            else if ((c == ',') && depth == 0) { return; }
        }
        ++p;
    }
}

/**
 * @brief 解析 JSON 平面对象格式，将条目写入翻译字典。
 *
 * 格式要求：顶层为 JSON 对象，所有値必须为字符串类型；
 * 非字符串値（数字、bool、null、嵌套对象/数组）会被跳过。
 *
 * @param data         JSON 数据
 * @param lang         语言代码（用于构造复合键）
 * @param translations 目标翻译字典
 * @param languages    目标语言列表
 * @return             Ok 成功，ParseError 格式错误（找不到顶层 '{'）
 */
mine::core::Status parse_json_catalog(
    mine::core::StringView data,
    mine::core::StringView lang,
    TranslationMap& translations,
    LanguageList& languages) noexcept
{
    const char* p   = data.data();
    const char* end = p + data.size();

    // 跳过可选的 BOM（0xEF 0xBB 0xBF）
    if (end - p >= 3 &&
        (unsigned char)p[0] == 0xEF &&
        (unsigned char)p[1] == 0xBB &&
        (unsigned char)p[2] == 0xBF) {
        p += 3;
    }

    p = skip_whitespace(p, end);
    if (p >= end || *p != '{') {
        return mine::core::Status{mine::core::StatusCode::ParseError};
    }
    ++p; // 跳过 '{'

    while (true) {
        p = skip_whitespace(p, end);
        if (p >= end) break;

        if (*p == '}') { ++p; break; }  // 对象结束
        if (*p == ',') { ++p; continue; } // 跳过逗号

        // 期望键（字符串）
        if (*p != '"') {
            return mine::core::Status{mine::core::StatusCode::ParseError};
        }
        ++p; // 跳过开头 '"'

        char key_buf[512];
        size_t key_len = 0;
        if (!parse_json_string(p, end, key_buf, sizeof(key_buf), key_len)) {
            return mine::core::Status{mine::core::StatusCode::ParseError};
        }

        // 跳过 ':'
        p = skip_whitespace(p, end);
        if (p >= end || *p != ':') {
            return mine::core::Status{mine::core::StatusCode::ParseError};
        }
        ++p;

        // 解析值
        p = skip_whitespace(p, end);
        if (p >= end) {
            return mine::core::Status{mine::core::StatusCode::ParseError};
        }

        if (*p == '"') {
            // 字符串值 → 写入翻译字典
            ++p; // 跳过开头 '"'
            char val_buf[4096];
            size_t val_len = 0;
            if (!parse_json_string(p, end, val_buf, sizeof(val_buf), val_len)) {
                return mine::core::Status{mine::core::StatusCode::ParseError};
            }

            ensure_language_in(lang, languages);
            auto ck = make_composite_key(
                lang, mine::core::StringView{key_buf, key_len});
            translations.insert_or_assign(
                std::move(ck),
                mine::containers::InlineString{
                    mine::core::StringView{val_buf, val_len}});
        } else {
            // 非字符串值：跳过
            skip_json_value(p, end);
        }
    }

    // 解析成功（即使无条目）：确保语言已注册
    ensure_language_in(lang, languages);
    return mine::core::Status{mine::core::StatusCode::Ok};
}

/**
 * @brief 解析 KeyValue 格式，将条目写入翻译字典。
 *
 * 格式规则：
 *   - 每行格式：翻译键=翻译文本
 *   - # 或 ; 开头的行视为注释行，跳过
 *   - 空行跳过
 *   - key 和 value 均去除首尾空白（value 仅去除行首空白，行尾保留）
 *
 * @param data         资源包内容
 * @param lang         语言代码
 * @param translations 目标翻译字典
 * @param languages    目标语言列表
 * @return             始终返回 Ok（KeyValue 格式宽松，不强制解析失败）
 */
mine::core::Status parse_kv_catalog(
    mine::core::StringView data,
    mine::core::StringView lang,
    TranslationMap& translations,
    LanguageList& languages) noexcept
{
    const char* p   = data.data();
    const char* end = p + data.size();

    while (p < end) {
        // 截取当前行
        const char* line_start = p;
        while (p < end && *p != '\n') ++p;
        const char* line_end = p;
        if (p < end) ++p; // 跳过 '\n'

        // 去除行尾 '\r'
        const char* le = line_end;
        while (le > line_start && *(le - 1) == '\r') --le;

        const char* ls = line_start;

        // 跳过行首空白
        while (ls < le && (*ls == ' ' || *ls == '\t')) ++ls;

        // 跳过空行、注释行
        if (ls >= le || *ls == '#' || *ls == ';') continue;

        // 查找第一个 '='
        const char* eq = ls;
        while (eq < le && *eq != '=') ++eq;
        if (eq >= le) continue; // 无 '='，忽略此行

        // key：去除尾部空白
        const char* ks = ls;
        const char* ke = eq;
        while (ke > ks && (*(ke - 1) == ' ' || *(ke - 1) == '\t')) --ke;
        if (ks >= ke) continue; // key 为空，忽略

        // value：跳过 '=' 之后的空白（行尾空白保留，允许翻译文本中含空格）
        const char* vs = eq + 1;
        while (vs < le && (*vs == ' ' || *vs == '\t')) ++vs;
        const char* ve = le;

        mine::core::StringView key_sv{ks, static_cast<size_t>(ke - ks)};
        mine::core::StringView val_sv{vs, static_cast<size_t>(ve - vs)};

        ensure_language_in(lang, languages);
        auto ck = make_composite_key(lang, key_sv);
        translations.insert_or_assign(
            std::move(ck),
            mine::containers::InlineString{val_sv});
    }

    return mine::core::Status{mine::core::StatusCode::Ok};
}

/**
 * @brief 替换翻译文本中的位置参数 {0}~{3}。
 *
 * 扫描模板字符串，遇到 {N}（N 为 0~3 的单个数字）时替换为对应参数。
 * 未提供的参数（空 StringView）保留占位符不变。
 *
 * @param tmpl   翻译文本（模板）
 * @param args   4 个参数（args[0]~args[3]，对应 {0}~{3}）
 * @return       替换后的完整字符串
 */
mine::containers::InlineString do_format(
    mine::core::StringView tmpl,
    mine::core::StringView args[4]) noexcept
{
    char out[4096];
    size_t out_len = 0;

    const char* p   = tmpl.data();
    const char* end = p + tmpl.size();

    while (p < end) {
        // 检测 {N} 格式（N 为 0~3 的单个 ASCII 数字）
        if (*p == '{' && p + 2 < end && *(p + 2) == '}') {
            char idx_char = *(p + 1);
            if (idx_char >= '0' && idx_char <= '3') {
                int idx = idx_char - '0';
                mine::core::StringView arg = args[idx];
                if (!arg.empty()) {
                    // 替换占位符
                    size_t copy_len = arg.size();
                    if (out_len + copy_len >= sizeof(out)) {
                        copy_len = sizeof(out) - out_len - 1;
                    }
                    if (copy_len > 0) {
                        memcpy(out + out_len, arg.data(), copy_len);
                        out_len += copy_len;
                    }
                    p += 3; // 跳过 {N}
                    continue;
                }
                // 参数为空：保留占位符原样
            }
        }
        // 普通字符（含 UTF-8 多字节序列）直接复制
        if (out_len < sizeof(out) - 1) {
            out[out_len++] = *p;
        }
        ++p;
    }

    out[out_len] = '\0';
    return mine::containers::InlineString{
        mine::core::StringView{out, out_len}};
}

} // anonymous namespace

// ─────────────────────────────────────────────────────────────────────────────
// Impl 成员函数实现（需在匿名命名空间之后定义，以使用 make_composite_key）
// ─────────────────────────────────────────────────────────────────────────────

mine::core::StringView LocalizationManager::Impl::lookup_in(
    mine::core::StringView lang,
    mine::core::StringView key) const noexcept
{
    auto ck = make_composite_key(lang, key);
    if (const auto* v = translations_.find(ck)) {
        return mine::core::StringView{v->data(), v->size()};
    }
    return {};
}

mine::core::StringView LocalizationManager::Impl::lookup(
    mine::core::StringView key) const noexcept
{
    // 第一层：当前语言
    if (!current_language_.empty()) {
        auto result = lookup_in(
            mine::core::StringView{current_language_.data(),
                                    current_language_.size()},
            key);
        if (!result.empty()) return result;
    }
    // 第二层：回退语言
    if (!fallback_language_.empty()) {
        auto result = lookup_in(
            mine::core::StringView{fallback_language_.data(),
                                    fallback_language_.size()},
            key);
        if (!result.empty()) return result;
    }
    return {};
}

// ─────────────────────────────────────────────────────────────────────────────
// LocalizationManager 成员函数实现
// ─────────────────────────────────────────────────────────────────────────────

LocalizationManager::LocalizationManager() noexcept
    : p_{mine::core::make_pimpl<Impl>()}
{}

LocalizationManager::~LocalizationManager() noexcept = default;

// ── 资源包加载 ───────────────────────────────────────────────────────────────

mine::core::Status LocalizationManager::load_catalog(
    mine::core::StringView language,
    mine::core::StringView data,
    CatalogFormat fmt) noexcept
{
    if (language.empty()) {
        return mine::core::Status{mine::core::StatusCode::InvalidArg};
    }
    if (data.empty()) {
        // 空数据视为合法（加载了 0 条翻译）
        ensure_language_in(language, p_->languages_);
        return mine::core::Status{mine::core::StatusCode::Ok};
    }

    switch (fmt) {
        case CatalogFormat::Json:
            return parse_json_catalog(data, language, p_->translations_, p_->languages_);
        case CatalogFormat::KeyValue:
            return parse_kv_catalog(data, language, p_->translations_, p_->languages_);
        default:
            return mine::core::Status{mine::core::StatusCode::InvalidArg};
    }
}

void LocalizationManager::add_translation(
    mine::core::StringView language,
    mine::core::StringView key,
    mine::core::StringView value) noexcept
{
    if (language.empty() || key.empty()) return;

    ensure_language_in(language, p_->languages_);
    auto ck = make_composite_key(language, key);
    p_->translations_.insert_or_assign(
        std::move(ck),
        mine::containers::InlineString{value});
}

// ── 语言管理 ─────────────────────────────────────────────────────────────────

void LocalizationManager::set_language(mine::core::StringView language) noexcept {
    // 与当前语言相同时无操作
    mine::core::StringView cur{p_->current_language_.data(),
                                p_->current_language_.size()};
    if (cur == language) return;

    // 保存旧语言（用于通知）
    mine::containers::InlineString old_lang = p_->current_language_;

    // 更新当前语言
    p_->current_language_ = mine::containers::InlineString{language};

    // 触发订阅者通知
    mine::core::StringView old_sv{old_lang.data(), old_lang.size()};
    mine::core::StringView new_sv{p_->current_language_.data(),
                                   p_->current_language_.size()};
    for (auto& sub : p_->subscribers_) {
        if (sub.callback) {
            sub.callback(old_sv, new_sv, sub.userdata);
        }
    }
}

mine::core::StringView LocalizationManager::get_language() const noexcept {
    return mine::core::StringView{p_->current_language_.data(),
                                   p_->current_language_.size()};
}

void LocalizationManager::set_fallback_language(
    mine::core::StringView language) noexcept
{
    p_->fallback_language_ = mine::containers::InlineString{language};
}

mine::core::StringView LocalizationManager::get_fallback_language() const noexcept {
    return mine::core::StringView{p_->fallback_language_.data(),
                                   p_->fallback_language_.size()};
}

void LocalizationManager::get_available_languages(
    mine::containers::Vector<mine::containers::InlineString>& out) const noexcept
{
    for (const auto& lang : p_->languages_) {
        out.push_back(lang);
    }
}

bool LocalizationManager::has_language(mine::core::StringView language) const noexcept {
    for (const auto& lang : p_->languages_) {
        if (mine::core::StringView{lang.data(), lang.size()} == language) return true;
    }
    return false;
}

// ── 翻译查询 ─────────────────────────────────────────────────────────────────

mine::core::StringView LocalizationManager::tr(
    mine::core::StringView key) const noexcept
{
    mine::core::StringView result = p_->lookup(key);
    // 未找到时返回 key 原文（使调用方始终得到可显示的文本）
    return result.empty() ? key : result;
}

mine::containers::InlineString LocalizationManager::tr_format(
    mine::core::StringView key,
    mine::core::StringView arg0,
    mine::core::StringView arg1,
    mine::core::StringView arg2,
    mine::core::StringView arg3) const noexcept
{
    mine::core::StringView tmpl = p_->lookup(key);
    if (tmpl.empty()) tmpl = key; // 未找到翻译，以 key 作为模板

    mine::core::StringView args[4] = {arg0, arg1, arg2, arg3};
    return do_format(tmpl, args);
}

mine::core::StringView LocalizationManager::tr_plural(
    mine::core::StringView key,
    int64_t n) const noexcept
{
    if (n == 1) {
        // 单数形式
        return tr(key);
    }

    // 复数形式：优先查 key + "_plural"
    char plural_buf[512];
    size_t key_len = key.size() < 504 ? key.size() : 504;
    memcpy(plural_buf, key.data(), key_len);
    static const char kPluralSuffix[] = "_plural";
    memcpy(plural_buf + key_len, kPluralSuffix, sizeof(kPluralSuffix));
    // sizeof 包含 '\0'，所以 plural_len = key_len + 7
    size_t plural_len = key_len + (sizeof(kPluralSuffix) - 1);
    plural_buf[plural_len] = '\0';

    mine::core::StringView plural_key{plural_buf, plural_len};
    mine::core::StringView result = p_->lookup(plural_key);

    // 回退到单数形式
    if (result.empty()) return tr(key);
    return result;
}

// ── 变更通知 ─────────────────────────────────────────────────────────────────

uint32_t LocalizationManager::subscribe(
    LanguageChangedCallback cb,
    void* userdata) noexcept
{
    if (!cb) return 0; // 空回调不注册

    uint32_t token = p_->next_token_++;
    Impl::Subscriber sub;
    sub.callback = cb;
    sub.userdata = userdata;
    sub.token    = token;
    p_->subscribers_.push_back(sub);
    return token;
}

void LocalizationManager::unsubscribe(uint32_t token) noexcept {
    auto& subs = p_->subscribers_;
    for (size_t i = 0; i < subs.size(); ++i) {
        if (subs[i].token == token) {
            // swap-with-back O(1) 删除
            if (i + 1 < subs.size()) {
                subs[i] = subs[subs.size() - 1];
            }
            subs.pop_back();
            return;
        }
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// 全局管理器
// ─────────────────────────────────────────────────────────────────────────────

/// 进程级全局本地化管理器实例（程序启动时构造，结束时析构）
static LocalizationManager s_global_manager;

LocalizationManager& global_manager() noexcept {
    return s_global_manager;
}

} // namespace mine::localization
