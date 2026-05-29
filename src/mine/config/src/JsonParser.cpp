/**
 * @file JsonParser.cpp
 * @brief JSON 解析器实现（mine.config 内部使用）。
 *
 * 使用递归下降解析器，将 JSON 对象展平为点分隔键值对序列。
 * 不依赖任何第三方库，纯手写实现。
 */

#include "JsonParser.h"

#include <cstring>
#include <cstdlib>

namespace mine::config::detail {

namespace {

// ─────────────────────────────────────────────────────────────────────────────
// JsonParserImpl：JSON 递归下降解析器
// ─────────────────────────────────────────────────────────────────────────────

/**
 * @brief JSON 解析器内部实现类。
 *
 * 状态管理：
 *   - p_ / end_  ：当前解析位置和输入边界
 *   - prefix_    ：当前展平路径前缀（如 "window"）
 *   - prefix_len_：前缀有效字节数
 */
class JsonParserImpl {
public:
    JsonParserImpl(mine::core::StringView input,
                   ParseCallback cb,
                   void* ud) noexcept
        : p_{input.data()}
        , end_{input.data() + input.size()}
        , cb_{cb}
        , ud_{ud}
        , prefix_len_{0}
    {
        prefix_[0] = '\0';
    }

    /// 启动解析，返回 Status::Ok 或 Status::ParseError
    mine::core::Status run() noexcept {
        skip_ws();
        if (p_ >= end_) return {};  // 空输入，视为成功

        // 顶层必须是 JSON 对象
        if (*p_ != '{') {
            return mine::core::Status{mine::core::StatusCode::ParseError};
        }

        if (!parse_object()) {
            return mine::core::Status{mine::core::StatusCode::ParseError};
        }
        return {};
    }

private:
    // ── 解析函数 ─────────────────────────────────────────────────────────

    /**
     * @brief 解析 { key: value, ... } 对象（含外层花括号）。
     *
     * 遇到嵌套对象时，更新 prefix_ 并递归调用自身。
     * 遇到数组时，调用 skip_array() 跳过。
     * 遇到标量值时，调用 parse_scalar() 并通过回调输出。
     */
    bool parse_object() noexcept {
        if (p_ >= end_ || *p_ != '{') return false;
        ++p_;  // 跳过 '{'

        skip_ws();
        // 空对象
        if (p_ < end_ && *p_ == '}') { ++p_; return true; }

        while (p_ < end_) {
            skip_ws();

            // 对象结束
            if (p_ < end_ && *p_ == '}') { ++p_; return true; }

            // 解析键名（JSON 字符串）
            char key_seg[256] = {};
            size_t key_seg_len = 0;
            if (!parse_key(key_seg, sizeof(key_seg), key_seg_len)) return false;

            // 跳过 ':'
            skip_ws();
            if (p_ >= end_ || *p_ != ':') return false;
            ++p_;
            skip_ws();

            if (p_ >= end_) return false;

            // 组装完整键名：prefix_ + '.' + key_seg（或仅 key_seg）
            char full_key[512] = {};
            size_t full_key_len = 0;
            if (prefix_len_ > 0) {
                size_t copy_len = prefix_len_ < (sizeof(full_key) - 2)
                    ? prefix_len_ : (sizeof(full_key) - 2);
                memcpy(full_key, prefix_, copy_len);
                full_key_len = copy_len;
                full_key[full_key_len++] = '.';
            }
            {
                size_t avail = sizeof(full_key) - full_key_len - 1;
                size_t seg_copy = key_seg_len < avail ? key_seg_len : avail;
                memcpy(full_key + full_key_len, key_seg, seg_copy);
                full_key_len += seg_copy;
                full_key[full_key_len] = '\0';
            }

            if (*p_ == '{') {
                // 嵌套对象：临时将 full_key 设为新前缀
                size_t saved_prefix_len = prefix_len_;
                size_t copy_len = full_key_len < sizeof(prefix_) - 1
                    ? full_key_len : sizeof(prefix_) - 1;
                memcpy(prefix_, full_key, copy_len);
                prefix_len_ = copy_len;
                prefix_[prefix_len_] = '\0';

                bool ok = parse_object();

                // 恢复前缀
                prefix_len_ = saved_prefix_len;
                prefix_[prefix_len_] = '\0';

                if (!ok) return false;

            } else if (*p_ == '[') {
                // 数组：跳过（不支持数组类型配置项）
                if (!skip_array()) return false;
            } else {
                // 标量值：解析后通过回调输出
                ConfigValue val;
                if (!parse_scalar(val)) return false;
                cb_(ud_,
                    mine::core::StringView{full_key, full_key_len},
                    std::move(val));
            }

            // 跳过可选的 ','
            skip_ws();
            if (p_ < end_ && *p_ == ',') ++p_;
        }

        return false;  // 循环结束但未遇到 '}'：格式错误
    }

    /**
     * @brief 解析 JSON 字符串作为键名，结果写入 buf（含简单转义处理）。
     *
     * @param buf     输出缓冲区（已 null 终止）
     * @param buf_cap 缓冲区容量（字节）
     * @param out_len 实际写入的字节数（不含 null）
     */
    bool parse_key(char* buf, size_t buf_cap, size_t& out_len) noexcept {
        skip_ws();
        if (p_ >= end_ || *p_ != '"') return false;
        ++p_;

        out_len = 0;
        while (p_ < end_ && *p_ != '"') {
            if (*p_ == '\\') {
                ++p_;
                if (p_ >= end_) return false;
                char ch = *p_++;
                char decoded = ch;
                switch (ch) {
                case '"':  decoded = '"';  break;
                case '\\': decoded = '\\'; break;
                case '/':  decoded = '/';  break;
                case 'n':  decoded = '\n'; break;
                case 'r':  decoded = '\r'; break;
                case 't':  decoded = '\t'; break;
                case 'b':  decoded = '\b'; break;
                case 'f':  decoded = '\f'; break;
                default:   decoded = ch;   break;
                }
                if (out_len + 1 < buf_cap) buf[out_len++] = decoded;
            } else {
                if (out_len + 1 < buf_cap) buf[out_len++] = *p_;
                ++p_;
            }
        }

        if (p_ >= end_) return false;  // 未找到闭合 '"'
        ++p_;  // 跳过 '"'

        if (out_len < buf_cap) buf[out_len] = '\0';
        return true;
    }

    /**
     * @brief 解析标量 JSON 值（null/bool/number/string）。
     */
    bool parse_scalar(ConfigValue& out) noexcept {
        skip_ws();
        if (p_ >= end_) return false;

        switch (*p_) {
        case 'n':  // null
            if ((end_ - p_) >= 4 && strncmp(p_, "null", 4) == 0) {
                p_ += 4;
                out = ConfigValue{};
                return true;
            }
            return false;

        case 't':  // true
            if ((end_ - p_) >= 4 && strncmp(p_, "true", 4) == 0) {
                p_ += 4;
                out = ConfigValue{true};
                return true;
            }
            return false;

        case 'f':  // false
            if ((end_ - p_) >= 5 && strncmp(p_, "false", 5) == 0) {
                p_ += 5;
                out = ConfigValue{false};
                return true;
            }
            return false;

        case '"':
            return parse_string_scalar(out);

        default:
            if (*p_ == '-' || (*p_ >= '0' && *p_ <= '9')) {
                return parse_number_scalar(out);
            }
            return false;
        }
    }

    /**
     * @brief 解析 JSON 字符串值（处理转义序列）。
     */
    bool parse_string_scalar(ConfigValue& out) noexcept {
        if (p_ >= end_ || *p_ != '"') return false;
        ++p_;

        // 先扫描一遍，检查是否含转义，同时找到结束引号
        const char* start = p_;
        bool has_escape = false;
        while (p_ < end_ && *p_ != '"') {
            if (*p_ == '\\') {
                has_escape = true;
                ++p_;
                if (p_ < end_) ++p_;
            } else {
                ++p_;
            }
        }
        if (p_ >= end_) return false;  // 未闭合
        const char* stop = p_;
        ++p_;  // 跳过 '"'

        if (!has_escape) {
            // 无转义：直接引用原始输入切片
            out = ConfigValue{mine::core::StringView{
                start, static_cast<size_t>(stop - start)}};
        } else {
            // 有转义：解码到栈上缓冲区
            char buf[1024] = {};
            size_t buf_len = 0;

            for (const char* sp = start; sp < stop; ) {
                if (*sp != '\\') {
                    if (buf_len + 1 < sizeof(buf)) buf[buf_len++] = *sp;
                    ++sp;
                } else {
                    ++sp;
                    if (sp >= stop) break;
                    if (buf_len + 1 < sizeof(buf)) {
                        switch (*sp) {
                        case '"':  buf[buf_len++] = '"';  break;
                        case '\\': buf[buf_len++] = '\\'; break;
                        case '/':  buf[buf_len++] = '/';  break;
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
     * @brief 解析数字（整数或浮点）。
     *
     * 有小数点或指数符号时解析为浮点，否则解析为整数。
     */
    bool parse_number_scalar(ConfigValue& out) noexcept {
        const char* start = p_;
        bool is_float = false;

        // 可选负号
        if (p_ < end_ && *p_ == '-') ++p_;

        // 整数部分
        while (p_ < end_ && *p_ >= '0' && *p_ <= '9') ++p_;

        // 小数部分
        if (p_ < end_ && *p_ == '.') {
            is_float = true;
            ++p_;
            while (p_ < end_ && *p_ >= '0' && *p_ <= '9') ++p_;
        }

        // 指数部分
        if (p_ < end_ && (*p_ == 'e' || *p_ == 'E')) {
            is_float = true;
            ++p_;
            if (p_ < end_ && (*p_ == '+' || *p_ == '-')) ++p_;
            while (p_ < end_ && *p_ >= '0' && *p_ <= '9') ++p_;
        }

        if (p_ == start) return false;  // 未读到任何数字

        if (is_float) {
            char buf[64] = {};
            size_t len = static_cast<size_t>(p_ - start);
            if (len >= sizeof(buf)) return false;
            memcpy(buf, start, len);
            char* ep = nullptr;
            double val = strtod(buf, &ep);
            out = ConfigValue{val};
        } else {
            // 手动解析整数（避免 strtoll 的潜在 errno 副作用）
            bool negative = (*start == '-');
            const char* sp = start;
            if (negative) ++sp;
            int64_t val = 0;
            for (; sp < p_; ++sp) val = val * 10 + static_cast<int64_t>(*sp - '0');
            out = ConfigValue{negative ? -val : val};
        }
        return true;
    }

    /**
     * @brief 跳过 [...] 数组（正确处理嵌套括号和字符串）。
     */
    bool skip_array() noexcept {
        if (p_ >= end_ || *p_ != '[') return false;
        ++p_;

        int depth = 1;
        while (p_ < end_ && depth > 0) {
            if (*p_ == '"') {
                // 跳过字符串，避免其中的 '[' ']' 影响括号计数
                ++p_;
                while (p_ < end_ && *p_ != '"') {
                    if (*p_ == '\\' && p_ + 1 < end_) p_ += 2;
                    else ++p_;
                }
                if (p_ < end_) ++p_;
            } else if (*p_ == '[' || *p_ == '{') {
                ++depth;
                ++p_;
            } else if (*p_ == ']' || *p_ == '}') {
                --depth;
                ++p_;
            } else {
                ++p_;
            }
        }
        return depth == 0;
    }

    // ── 辅助工具 ─────────────────────────────────────────────────────────

    /// 跳过空白字符（空格/制表符/换行）
    void skip_ws() noexcept {
        while (p_ < end_ &&
               (*p_ == ' ' || *p_ == '\t' || *p_ == '\r' || *p_ == '\n'))
        {
            ++p_;
        }
    }

    // ── 成员 ─────────────────────────────────────────────────────────────

    const char*   p_;             ///< 当前解析位置
    const char*   end_;           ///< 输入末尾（不含）
    ParseCallback cb_;            ///< 输出回调
    void*         ud_;            ///< 传递给回调的用户数据
    char          prefix_[512];   ///< 当前展平路径前缀（点分隔，不含末尾点）
    size_t        prefix_len_;    ///< 前缀有效字节数
};

} // namespace（匿名）

// ─────────────────────────────────────────────────────────────────────────────
// 公共入口
// ─────────────────────────────────────────────────────────────────────────────

mine::core::Status parse_json(mine::core::StringView json,
                               ParseCallback          callback,
                               void*                  user_data) noexcept
{
    JsonParserImpl impl{json, callback, user_data};
    return impl.run();
}

} // namespace mine::config::detail
