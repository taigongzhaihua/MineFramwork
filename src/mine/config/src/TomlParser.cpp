/**
 * @file TomlParser.cpp
 * @brief TOML 解析器实现（mine.config 内部使用）。
 *
 * 逐行扫描解析，支持以下 TOML 子集：
 *   - 注释行（# ...）
 *   - 节头（[section] / [section.sub]）
 *   - 键值对（key = value）
 *   - 字符串（"基本字符串" / '字面字符串'）
 *   - 整数、浮点、布尔
 *
 * 遇到不支持的语法（数组、内联表等）时跳过该行，不报错。
 */

#include "TomlParser.h"

#include <cstring>
#include <cstdlib>
#include <cctype>

namespace mine::config::detail {

namespace {

// ─────────────────────────────────────────────────────────────────────────────
// 辅助工具：字符分类与字符串裁剪
// ─────────────────────────────────────────────────────────────────────────────

/// 判断字符是否为空白（空格/制表符，不含换行）
static inline bool is_space(char c) noexcept {
    return c == ' ' || c == '\t';
}

/// 判断字符是否为数字
static inline bool is_digit(char c) noexcept {
    return c >= '0' && c <= '9';
}

/// 跳过行首空白，返回更新后的指针
static const char* skip_space(const char* p, const char* end) noexcept {
    while (p < end && is_space(*p)) ++p;
    return p;
}

/// 去除行尾空白，返回有效末尾指针（不含空白和注释）
static const char* trim_end(const char* begin, const char* end) noexcept {
    // 去掉末尾空白
    while (end > begin && is_space(*(end - 1))) --end;
    return end;
}

// ─────────────────────────────────────────────────────────────────────────────
// TomlParserImpl：TOML 解析器
// ─────────────────────────────────────────────────────────────────────────────

class TomlParserImpl {
public:
    TomlParserImpl(mine::core::StringView toml,
                   ParseCallback cb,
                   void* ud) noexcept
        : input_{toml}
        , cb_{cb}
        , ud_{ud}
        , section_len_{0}
    {
        section_[0] = '\0';
    }

    /// 启动解析
    mine::core::Status run() noexcept {
        const char* p = input_.data();
        const char* end = p + input_.size();

        while (p < end) {
            // 找到行尾
            const char* line_start = p;
            const char* line_end = p;
            while (line_end < end && *line_end != '\n') ++line_end;

            // 解析这一行
            process_line(line_start, line_end);

            // 跳过换行符
            p = line_end;
            if (p < end && *p == '\n') ++p;
        }

        return {};  // TOML 解析器遇到错误时跳过行，不返回 ParseError
    }

private:
    // ── 行解析 ───────────────────────────────────────────────────────────

    /**
     * @brief 解析一行（去掉回车符后处理）。
     */
    void process_line(const char* begin, const char* end) noexcept {
        // 去掉末尾回车符
        if (end > begin && *(end - 1) == '\r') --end;

        // 跳过行首空白
        const char* p = skip_space(begin, end);
        if (p >= end) return;  // 空行

        // 注释行
        if (*p == '#') return;

        // 节头行：[section] 或 [section.sub]
        if (*p == '[') {
            parse_section(p, end);
            return;
        }

        // 键值对行：key = value
        parse_key_value(p, end);
    }

    /**
     * @brief 解析节头 [section]，更新 section_ 前缀。
     */
    void parse_section(const char* p, const char* end) noexcept {
        if (p >= end || *p != '[') return;
        ++p;  // 跳过 '['

        // 不支持 [[数组节]] — 跳过第二个 '['
        if (p < end && *p == '[') return;

        // 找到 ']' 的位置
        const char* bracket = p;
        while (bracket < end && *bracket != ']') ++bracket;
        if (bracket >= end) return;  // 未找到 ']'

        // 去除前后空白
        const char* sec_begin = skip_space(p, bracket);
        const char* sec_end   = trim_end(sec_begin, bracket);

        size_t sec_len = static_cast<size_t>(sec_end - sec_begin);
        if (sec_len == 0 || sec_len >= sizeof(section_)) return;

        memcpy(section_, sec_begin, sec_len);
        section_[sec_len] = '\0';
        section_len_ = sec_len;
    }

    /**
     * @brief 解析键值对 key = value。
     */
    void parse_key_value(const char* p, const char* end) noexcept {
        // 查找 '='
        const char* eq = p;
        while (eq < end && *eq != '=' && *eq != '#') ++eq;
        if (eq >= end || *eq != '=') return;  // 无 '='

        // 提取键名
        const char* key_begin = p;
        const char* key_end   = trim_end(key_begin, eq);
        key_begin = skip_space(key_begin, key_end);

        size_t key_len = static_cast<size_t>(key_end - key_begin);
        if (key_len == 0) return;

        // 组装完整键名：section_ + '.' + key（或仅 key）
        char full_key[512] = {};
        size_t full_key_len = 0;

        if (section_len_ > 0) {
            size_t copy = section_len_ < sizeof(full_key) - 2
                ? section_len_ : sizeof(full_key) - 2;
            memcpy(full_key, section_, copy);
            full_key_len = copy;
            full_key[full_key_len++] = '.';
        }
        {
            size_t avail = sizeof(full_key) - full_key_len - 1;
            size_t copy  = key_len < avail ? key_len : avail;
            memcpy(full_key + full_key_len, key_begin, copy);
            full_key_len += copy;
            full_key[full_key_len] = '\0';
        }

        // 解析值（'=' 之后）
        const char* val_begin = skip_space(eq + 1, end);
        if (val_begin >= end) return;

        // 去掉行尾注释（# 之后的部分），注意字符串内的 '#' 不算注释
        const char* val_end = strip_inline_comment(val_begin, end);
        val_end = trim_end(val_begin, val_end);

        if (val_begin >= val_end) return;

        // 解析值类型
        ConfigValue val;
        if (!parse_value(val_begin, val_end, val)) return;

        cb_(ud_,
            mine::core::StringView{full_key, full_key_len},
            std::move(val));
    }

    /**
     * @brief 去除行内注释（不处理字符串内的 '#'）。
     *
     * @return 指向注释开始之前的末尾指针
     */
    const char* strip_inline_comment(const char* begin, const char* end) noexcept {
        const char* p = begin;
        // 若行以字符串开头，跳过整个字符串
        if (p < end && (*p == '"' || *p == '\'')) {
            char quote = *p++;
            while (p < end && *p != quote) {
                if (*p == '\\' && quote == '"') {
                    if (p + 1 < end) p += 2;
                    else ++p;
                } else {
                    ++p;
                }
            }
            if (p < end) ++p;  // 跳过结束引号
            // 字符串结束后的部分才有可能有注释
        }
        // 从 p 开始查找 '#'
        while (p < end) {
            if (*p == '#') return p;
            ++p;
        }
        return end;
    }

    /**
     * @brief 解析 TOML 值（字符串/整数/浮点/布尔）。
     *
     * @param begin 值文本开始（已去除前导空白）
     * @param end   值文本末尾（已去除尾部空白和行内注释）
     * @param out   解析结果
     * @return true 解析成功
     */
    bool parse_value(const char* begin, const char* end, ConfigValue& out) noexcept {
        if (begin >= end) return false;

        // 字符串：基本字符串 "..." 或字面字符串 '...'
        if (*begin == '"')  return parse_basic_string(begin, end, out);
        if (*begin == '\'') return parse_literal_string(begin, end, out);

        // 数组或内联表：跳过（不支持）
        if (*begin == '[' || *begin == '{') return false;

        // 布尔
        size_t len = static_cast<size_t>(end - begin);
        if (len == 4 && strncmp(begin, "true", 4) == 0) {
            out = ConfigValue{true};
            return true;
        }
        if (len == 5 && strncmp(begin, "false", 5) == 0) {
            out = ConfigValue{false};
            return true;
        }

        // 数字（整数或浮点）
        return parse_number(begin, end, out);
    }

    /**
     * @brief 解析 TOML 基本字符串 "..."（支持转义）。
     */
    bool parse_basic_string(const char* begin, const char* end, ConfigValue& out) noexcept {
        const char* p = begin;
        if (p >= end || *p != '"') return false;
        ++p;

        // 多行基本字符串 """..."""：不支持，直接返回
        if (p + 1 < end && *p == '"' && *(p + 1) == '"') return false;

        const char* str_start = p;
        bool has_escape = false;

        while (p < end && *p != '"') {
            if (*p == '\\') {
                has_escape = true;
                ++p;
                if (p < end) ++p;
            } else {
                ++p;
            }
        }
        if (p >= end) return false;  // 未找到结束引号
        const char* str_end = p;
        // 忽略结束引号之后的内容（应该是行尾）

        if (!has_escape) {
            out = ConfigValue{mine::core::StringView{
                str_start, static_cast<size_t>(str_end - str_start)}};
        } else {
            char buf[1024] = {};
            size_t buf_len = 0;

            for (const char* sp = str_start; sp < str_end; ) {
                if (*sp != '\\') {
                    if (buf_len + 1 < sizeof(buf)) buf[buf_len++] = *sp;
                    ++sp;
                } else {
                    ++sp;
                    if (sp >= str_end) break;
                    if (buf_len + 1 < sizeof(buf)) {
                        switch (*sp) {
                        case '"':  buf[buf_len++] = '"';  break;
                        case '\\': buf[buf_len++] = '\\'; break;
                        case 'n':  buf[buf_len++] = '\n'; break;
                        case 'r':  buf[buf_len++] = '\r'; break;
                        case 't':  buf[buf_len++] = '\t'; break;
                        case 'b':  buf[buf_len++] = '\b'; break;
                        case 'f':  buf[buf_len++] = '\f'; break;
                        default:   buf[buf_len++] = *sp;  break;
                        }
                    }
                    ++sp;
                }
            }

            out = ConfigValue{mine::core::StringView{buf, buf_len}};
        }
        return true;
    }

    /**
     * @brief 解析 TOML 字面字符串 '...'（不处理转义，所有字符原样保留）。
     */
    bool parse_literal_string(const char* begin, const char* end, ConfigValue& out) noexcept {
        const char* p = begin;
        if (p >= end || *p != '\'') return false;
        ++p;

        // 多行字面字符串 '''...'''：不支持
        if (p + 1 < end && *p == '\'' && *(p + 1) == '\'') return false;

        const char* str_start = p;
        while (p < end && *p != '\'') ++p;
        if (p >= end) return false;

        out = ConfigValue{mine::core::StringView{
            str_start, static_cast<size_t>(p - str_start)}};
        return true;
    }

    /**
     * @brief 解析整数或浮点数。
     */
    bool parse_number(const char* begin, const char* end, ConfigValue& out) noexcept {
        if (begin >= end) return false;

        const char* p = begin;
        bool is_float = false;

        // 可选符号
        if (p < end && (*p == '-' || *p == '+')) ++p;

        // 整数部分
        const char* int_start = p;
        while (p < end && is_digit(*p)) ++p;

        if (p == int_start && p < end) return false;  // 无数字

        // 小数部分
        if (p < end && *p == '.') {
            is_float = true;
            ++p;
            while (p < end && is_digit(*p)) ++p;
        }

        // 指数部分
        if (p < end && (*p == 'e' || *p == 'E')) {
            is_float = true;
            ++p;
            if (p < end && (*p == '+' || *p == '-')) ++p;
            while (p < end && is_digit(*p)) ++p;
        }

        // 若还有剩余字符，则不是纯数字
        if (p < end) return false;

        if (is_float) {
            char buf[64] = {};
            size_t len = static_cast<size_t>(end - begin);
            if (len >= sizeof(buf)) return false;
            memcpy(buf, begin, len);
            char* ep = nullptr;
            double val = strtod(buf, &ep);
            out = ConfigValue{val};
        } else {
            bool negative = (*begin == '-');
            const char* sp = begin;
            if (*sp == '-' || *sp == '+') ++sp;
            int64_t val = 0;
            for (; sp < end; ++sp) val = val * 10 + static_cast<int64_t>(*sp - '0');
            out = ConfigValue{negative ? -val : val};
        }
        return true;
    }

    // ── 成员 ─────────────────────────────────────────────────────────────

    mine::core::StringView input_;      ///< 完整输入
    ParseCallback          cb_;         ///< 输出回调
    void*                  ud_;         ///< 用户数据
    char                   section_[256]; ///< 当前节前缀
    size_t                 section_len_; ///< 节前缀有效长度
};

} // namespace（匿名）

// ─────────────────────────────────────────────────────────────────────────────
// 公共入口
// ─────────────────────────────────────────────────────────────────────────────

mine::core::Status parse_toml(mine::core::StringView toml,
                               ParseCallback          callback,
                               void*                  user_data) noexcept
{
    TomlParserImpl impl{toml, callback, user_data};
    return impl.run();
}

} // namespace mine::config::detail
